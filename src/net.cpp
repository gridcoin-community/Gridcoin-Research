// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#endif

#include "wallet/db.h"
#include "banman.h"
#include "net.h"
#include "net_processing.h"
#include "init.h"
#include "node/ui_interface.h"
#include "random.h"
#include "arith_uint256.h"
#include "hash.h"
#include "util.h"
#include "util/threadnames.h"

#include <boost/thread.hpp>
#include <inttypes.h>

#if !defined(HAVE_MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

#ifdef WIN32
  #include <string.h>
#endif

#ifdef USE_UPNP
 #include <miniupnpc/miniwget.h>
 #include <miniupnpc/miniupnpc.h>
 #include <miniupnpc/upnpcommands.h>
 #include <miniupnpc/upnperrors.h>
// The minimum supported miniUPnPc API version is set to 10. This keeps compatibility
// with Ubuntu 16.04 LTS and Debian 8 libminiupnpc-dev packages.
static_assert(MINIUPNPC_API_VERSION >= 10, "miniUPnPc API version >= 10 assumed");
#endif

using namespace std;

extern int nMaxConnections;
int MAX_OUTBOUND_CONNECTIONS = 8;
int PEER_TIMEOUT = 45;

void ThreadMessageHandler2(void* parg);
void ThreadSocketHandler2(void* parg);
void ThreadOpenConnections2(void* parg);
void ThreadOpenAddedConnections2(void* parg);
#ifdef USE_UPNP
void ThreadMapPort2(void* parg);
#endif
void ThreadDNSAddressSeed2(void* parg);
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant* grantOutbound = nullptr, const char* strDest = nullptr, bool fOneShot = false);

//
// Global state variables
//
bool fDiscover = true;
ServiceFlags nLocalServices = NODE_NETWORK;
CCriticalSection cs_mapLocalHost;
std::map<CNetAddr, LocalServiceInfo> mapLocalHost GUARDED_BY(cs_mapLocalHost);
static bool vfLimited[NET_MAX] GUARDED_BY(cs_mapLocalHost) = {};
static CNode* pnodeLocalHost = nullptr;


std::atomic<uint64_t> CNode::nTotalBytesRecv{ 0 };
std::atomic<uint64_t> CNode::nTotalBytesSent{ 0 };

ThreadHandler* netThreads = new ThreadHandler;
std::unique_ptr<CConnman> g_connman;
// Listen sockets are RAII Sock wrappers (issue #2558 PR 5b) so they can join
// the Sock::WaitMany() set uniformly with the per-node sockets.
static std::vector<std::shared_ptr<Sock>> vhListenSocket;
AddrMan addrman;

// Initialization of static class variable.
std::atomic<NodeId> CNode::nLastNodeId {-1};

CCriticalSection cs_vNodes;
vector<CNode*> vNodes GUARDED_BY(cs_vNodes);
CCriticalSection cs_vAddedNodes;
vector<std::string> vAddedNodes GUARDED_BY(cs_vAddedNodes);

CCriticalSection cs_mapAlreadyAskedFor;
map<CInv, int64_t> mapAlreadyAskedFor GUARDED_BY(cs_mapAlreadyAskedFor);

CCriticalSection cs_vOneShots;
static deque<string> vOneShots GUARDED_BY(cs_vOneShots);

CCriticalSection cs_setservAddNodeAddresses;
set<CNetAddr> setservAddNodeAddresses GUARDED_BY(cs_setservAddNodeAddresses);

// CNode misbehavior state (mapMisbehavior / cs_mapMisbehavior) moved to
// net_processing.cpp (issue #2558 PR 2c).

static CSemaphore* semOutbound = nullptr;

void AddOneShot(string strDest)
{
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort()
{
    return (unsigned short)(gArgs.GetArg("-port", GetDefaultPort()));
}

void CNode::PushGetBlocks(CBlockIndex* pindexBegin, uint256 hashEnd)
{
    if (pindexBegin == pindexLastGetBlocksBegin && hashEnd == hashLastGetBlocksEnd) return;  // Filter out duplicate requests

    pindexLastGetBlocksBegin = pindexBegin;
    hashLastGetBlocksEnd = hashEnd;

    // The shared locator cache lives on CConnman now (issue #2558 PR 9d2); fall
    // back to building it inline in the unreachable no-connman case.
    if (g_connman) {
        PushMessage(NetMsgType::GETBLOCKS, g_connman->GetBlockLocator(pindexBegin), hashEnd);
    } else {
        PushMessage(NetMsgType::GETBLOCKS, CBlockLocator(pindexBegin), hashEnd);
    }
}

// find 'best' local address for a particular peer
bool GetLocal(CService& addr, const CNetAddr *paddrPeer)
{
    if (fNoListen)
        return false;

    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (const auto& entry : mapLocalHost)
        {
            int nScore = entry.second.nScore;
            int nReachability = entry.first.GetReachabilityFrom(paddrPeer);
            if (nReachability > nBestReachability || (nReachability == nBestReachability && nScore > nBestScore))
            {
                addr = CService(entry.first, entry.second.nPort);
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }

    return nBestScore >= 0;
}

// get best local address for a particular peer as a CAddress
// Otherwise, return the unroutable 0.0.0.0 but filled in with
// the normal parameters, since the IP may be changed to a useful
// one by discovery.
CAddress GetLocalAddress(const CNetAddr *paddrPeer)
{
    ServiceFlags nLocalServices = NODE_NETWORK;
    CAddress ret(CService(CNetAddr(),GetListenPort()), nLocalServices);
    CService addr;
    if (GetLocal(addr, paddrPeer))
    {
        ret = CAddress(addr, nLocalServices);
    }
    ret.nTime = GetAdjustedTime();
    return ret;
}

static int GetnScore(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    if (mapLocalHost.count(addr) == 0) return 0;
    return mapLocalHost[addr].nScore;
}

// Is our peer's addrLocal potentially useful as an external IP source?
bool IsPeerAddrLocalGood(CNode *pnode)
{
    CService addrLocal = pnode->GetAddrLocal();
    return fDiscover && pnode->addr.IsRoutable() && addrLocal.IsRoutable() &&
           IsReachable(addrLocal);
}

// pushes our own address to a peer
void AdvertiseLocal(CNode *pnode)
{
    if (!fNoListen && pnode->fSuccessfullyConnected)
    {
        CAddress addrLocal = GetLocalAddress(&pnode->addr);

        // If discovery is enabled, sometimes give our peer the address it
        // tells us that it sees us as in case it has a better idea of our
        // address than we do.
        const int randomNumber = GetRand<int>((GetnScore(addrLocal) > LOCAL_MANUAL) ? 3+1 : 1+1);
        if (IsPeerAddrLocalGood(pnode) && (!addrLocal.IsRoutable() ||
             randomNumber == 0))
        {
            addrLocal.SetIP(pnode->GetAddrLocal());
        }
        if (addrLocal.IsRoutable())
        {
            LogPrint(BCLog::LogFlags::NET, "AdvertiseLocal: advertising address %s", addrLocal.ToString());
            pnode->PushAddress(addrLocal);
        }
    }
}



// learn a new local address
bool AddLocal(const CService& addr, int nScore)
{
    if (!addr.IsRoutable())
        return false;

    if (!fDiscover && nScore < LOCAL_MANUAL)
        return false;

    if (IsLimited(addr))
        return false;

    LogPrint(BCLog::LogFlags::NET, "AddLocal(%s,%i)", addr.ToString(), nScore);
    try
    {

    {
        LOCK(cs_mapLocalHost);
        bool fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo &info = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort = addr.GetPort();
        }
    }

    }
    catch(...)
    {

    }
    return true;
}

bool AddLocal(const CNetAddr &addr, int nScore)
{
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

void RemoveLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    LogPrintf("RemoveLocal(%s)\n", addr.ToString());
    mapLocalHost.erase(addr);
}

void SetReachable(enum Network net, bool fFlag)
{
    if (net == NET_UNROUTABLE || net == NET_INTERNAL)
        return;
    LOCK(cs_mapLocalHost);
    vfLimited[net] = !fFlag;
}

/** check whether a given address is in a network we can probably connect to */
bool IsReachable(const CNetAddr& addr)
{
    LOCK(cs_mapLocalHost);
    enum Network net = addr.GetNetwork();
    return !vfLimited[net];
}

bool IsLimited(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfLimited[net];
}

bool IsLimited(const CNetAddr &addr)
{
    return IsLimited(addr.GetNetwork());
}

/** vote for a local address */
bool SeenLocal(const CService& addr)
{
    {
        LOCK(cs_mapLocalHost);
        if (mapLocalHost.count(addr) == 0)
            return false;
        mapLocalHost[addr].nScore++;
    }
    return true;
}

/** check whether a given address is potentially local */
bool IsLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

CNode* FindNode(const CNetAddr& ip)
{
    LOCK(cs_vNodes);
    for (auto const& pnode : vNodes) {
        if (static_cast<CNetAddr>(pnode->addr) == ip) {
            return pnode;
        }
    }
    return nullptr;
}

CNode* FindNode(const std::string& addrName)
{
    LOCK(cs_vNodes);
    for (auto const& pnode : vNodes) {
        if (pnode->addrName == addrName) {
            return pnode;
        }
    }
    return nullptr;
}

CNode* FindNode(const CService& addr)
{
    LOCK(cs_vNodes);
    for (auto const& pnode : vNodes) {
        if (static_cast<CService>(pnode->addr) == addr) {
            return pnode;
        }
    }
    return nullptr;
}





void AddressCurrentlyConnected(const CService& addr)
{
    addrman.Connected(addr);
}


CNode* ConnectNode(CAddress addrConnect, const char *pszDest)
{
    if (pszDest == nullptr) {
        if (IsLocal(addrConnect))
            return nullptr;

        // Look for an existing connection
        CNode* pnode = FindNode(static_cast<CService>(addrConnect));
        if (pnode)
        {
            LogPrintf("Failed to open new connection, already connected\n");
            return nullptr;
        }
    }


    // Connect
    SOCKET hSocket;
    if (pszDest ? ConnectSocketByName(addrConnect, hSocket, pszDest, GetDefaultPort()) : ConnectSocket(addrConnect, hSocket))
    {
        addrman.Attempt(addrConnect);
        /// debug print
        LogPrint(BCLog::LogFlags::NET, "connected %s", pszDest ? pszDest : addrConnect.ToString());
        // Set to non-blocking
#ifdef WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR)
            LogPrintf("ConnectSocket() : ioctlsocket non-blocking setting error %d", WSAGetLastError());
#else
        if (fcntl(hSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
            LogPrintf("ConnectSocket() : fcntl non-blocking setting error %d", errno);
#endif

        // Add node
        CNode* pnode = new CNode(hSocket, addrConnect, pszDest ? pszDest : "", false);
        pnode->AddRef();

        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }

        pnode->nTimeConnected = GetAdjustedTime();

        // We're making a new connection, harvest entropy from the time (and our peer count)
        RandAddEvent((uint32_t)pnode->GetId());

        return pnode;
    }
    else
    {
        return nullptr;
    }
}

NodeId CNode::GetNewNodeId()
{
    return nLastNodeId.fetch_add(1, std::memory_order_relaxed);
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;

    // Reset the socket under m_sock_mutex (the Sock destructor closes the fd),
    // then release it before taking cs_vRecvMsg to keep m_sock_mutex an inner
    // leaf lock (issue #2558 PR 5a).
    bool was_open;
    {
        LOCK(m_sock_mutex);
        was_open = (bool)m_sock;
        if (m_sock)
        {
            LogPrint(BCLog::LogFlags::NET, "disconnecting node %s", addrName);
            m_sock.reset();
        }
    }

    if (was_open)
    {
        // in case this fails, we'll empty the recv buffer when the CNode is deleted
        TRY_LOCK(cs_vRecvMsg, lockRecv);
        if (lockRecv)
            vRecvMsg.clear();
    }
}


void CNode::PushVersion()
{
    int64_t nTime = GetAdjustedTime();
    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(LookupNumeric("0.0.0.0", 0)));
    CAddress addrMe = CAddress(CService(), nLocalServices);
    // GetRandBytes() writes raw bytes into the provided buffer; that's
    // incompatible with memcpy'ing into a std::atomic's storage directly,
    // so go through a local and store atomically.
    //
    // The local `nonce` is what we send on the wire below -- the atomic
    // store is only to publish it to ProcessMessage's self-connection
    // sentinel check. Reading the atomic back here for the PushMessage
    // payload would lose the value to a concurrent PushVersion() on
    // another outbound connection that ran between our store() and
    // load(). (The single-global-nonce design is still racy across
    // multiple simultaneous outbound connections versus their respective
    // self-connection echoes -- a separate per-connection-nonce follow-up
    // is the proper fix; see PR #2957 commit message for context.)
    uint64_t nonce;
    GetRandBytes({(unsigned char*)&nonce, sizeof(nonce)});
    if (g_connman) g_connman->SetLocalHostNonce(nonce);

    // Snapshot the chain height under cs_main so this method can be called
    // from CNode construction (socket handler thread, no outer locks held)
    // and from ProcessMessage without imposing cs_main on the call site.
    const int nLocalBestHeight = WITH_LOCK(cs_main, return nBestHeight);

    LogPrint(BCLog::LogFlags::NET, "send version message: version %d, blocks=%d, us=%s, them=%s, peer=%s",
        PROTOCOL_VERSION, nLocalBestHeight, addrMe.ToString(), addrYou.ToString(), addr.ToString());

    //TODO: change `PushMessage()` to use ServiceFlags so we don't need to cast nLocalServices
    PushMessage(
        NetMsgType::ARIES,
        PROTOCOL_VERSION,
        (uint64_t)nLocalServices,
        nTime,
        addrYou,
        addrMe,
        nonce,
        FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>()),
        nLocalBestHeight);
}

bool CNode::Misbehaving(int howmuch)
{
    if (addr.IsLocal())
    {
        LogPrintf("Warning: Local node %s misbehaving (delta: %d)!", addrName, howmuch);
        return false;
    }

    // Scoring, decay, and the ban decision live in PeerManagerImpl
    // (g_peerman->Misbehaving, issue #2558 PR 8b); the instance method
    // additionally disconnects this node when its score crosses the ban
    // threshold. No g_peerman (early init / tests without the fixture) means no
    // misbehavior tracking, so treat it as "not banned".
    if (g_peerman && g_peerman->Misbehaving(addr, howmuch))
    {
        CloseSocketDisconnect();
        return true;
    }

    return false;
}


int CNode::GetMisbehavior() const
{
    return g_peerman ? g_peerman->GetMisbehaviorScore(addr) : 0;
}

CService CNode::GetAddrLocal() const
{
    LOCK(cs_addrLocal);
    return addrLocal;
}

void CNode::SetAddrLocal(const CService& addrLocalIn)
{
    LOCK(cs_addrLocal);
    if (addrLocal.IsValid()) {
        LogPrintf("WARN: %s: addrLocal already set for node %d: refusing to change from %s to %s",
                  __func__, id, addrLocal.ToString(), addrLocalIn.ToString());
    } else {
        addrLocal = addrLocalIn;
    }
}

void CNode::copyStats(CNodeStats &stats)
{
    stats.id = id;
    stats.nServices = nServices;
    stats.addr = addr;
    stats.nLastSend = nLastSend;
    stats.nLastRecv = nLastRecv;
    stats.nTimeConnected = nTimeConnected;
    stats.nTimeOffset = nTimeOffset;
    stats.addrName = addrName;
    stats.nVersion = nVersion;
    stats.strSubVer = strSubVer;
    stats.fInbound = fInbound;
    stats.nStartingHeight = nStartingHeight;
    stats.nTrust = nTrust;
    stats.nMisbehavior = GetMisbehavior();

    // No lock for these two... using atomics.
    stats.nSendBytes = nSendBytes;
    stats.nRecvBytes = nRecvBytes;

    // It is common for nodes with good ping times to suddenly become lagged,
    // due to a new block arriving or other large transfer.
    // Merely reporting pingtime might fool the caller into thinking the node was still responsive,
    // since pingtime does not update until the ping is complete, which might take a while.
    // So, if a ping is taking an unusually long time in flight,
    // the caller can immediately detect that this is happening.
    int64_t nPingUsecWait = 0;
    if ((0 != nPingNonceSent) && (0 != nPingUsecStart)) {
        nPingUsecWait = GetTimeMicros() - nPingUsecStart;
    }

    // Raw ping time is in microseconds, but show it to user as whole seconds (Bitcoin users should be well used to small numbers with many decimal places by now :)
    stats.dPingTime = (((double)nPingUsecTime) / 1e6);
    stats.dMinPing  = (((double)nMinPingUsecTime) / 1e6);
    stats.dPingWait = (((double)nPingUsecWait) / 1e6);
    const CService al = GetAddrLocal();
    stats.addrLocal = al.IsValid() ? al.ToString() : "";

}


void CNode::CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    for (auto const& pnode : vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}


// requires LOCK(cs_vRecvMsg)
bool CNode::ReceiveMsgBytes(const char *pch, unsigned int nBytes)
{
    nRecvBytes += nBytes;

    while (nBytes > 0) {

        // get current incomplete message, or create a new one
        if (vRecvMsg.empty() ||
            vRecvMsg.back().complete())
            vRecvMsg.push_back(CNetMessage(SER_NETWORK, nRecvVersion));

        CNetMessage& msg = vRecvMsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
            handled = msg.readHeader(pch, nBytes);
        else
            handled = msg.readData(pch, nBytes);

        if (handled < 0)
                return false;

        pch += handled;
        nBytes -= handled;

        if (msg.complete())
            msg.nTime = GetTimeMicros();
    }

    return true;
}

int CNetMessage::readHeader(const char *pch, unsigned int nBytes)
{
    // copy data to temporary parsing buffer
    unsigned int nRemaining = 24 - nHdrPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    memcpy(&hdrbuf[nHdrPos], pch, nCopy);
    nHdrPos += nCopy;

    // if header incomplete, exit
    if (nHdrPos < 24)
        return nCopy;

    // deserialize to CMessageHeader
    try {
        hdrbuf >> hdr;
    }
    catch (std::exception &e) {
        return -1;
    }

    // reject messages larger than MAX_SIZE
    if (hdr.nMessageSize > MAX_SIZE)
            return -1;

    // switch state to reading message data
    in_data = true;

    return nCopy;
}

int CNetMessage::readData(const char *pch, unsigned int nBytes)
{
    unsigned int nRemaining = hdr.nMessageSize - nDataPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    if (vRecv.size() < nDataPos + nCopy) {
        // Allocate up to 256 KiB ahead, but never more than the total message size.
        vRecv.resize(std::min(hdr.nMessageSize, nDataPos + nCopy + 256 * 1024));
    }

    memcpy(&vRecv[nDataPos], pch, nCopy);
    nDataPos += nCopy;

    return nCopy;
}



void SocketSendData(CNode *pnode) EXCLUSIVE_LOCKS_REQUIRED(pnode->cs_vSend)
{
    // Hold a shared_ptr copy of the socket for the duration of the send so it
    // cannot be closed underneath us (issue #2558 PR 5a).
    const std::shared_ptr<Sock> sock = pnode->GetSock();
    if (!sock) return;

    std::deque<SerializeData>::iterator it = pnode->vSendMsg.begin();

    while (it != pnode->vSendMsg.end())
    {
        const SerializeData &data = *it;
        assert(data.size() > pnode->nSendOffset);
        int nBytes = sock->Send(&data[pnode->nSendOffset], data.size() - pnode->nSendOffset, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (nBytes > 0) {
            pnode->nLastSend = GetAdjustedTime();
            pnode->nSendBytes += nBytes;
            pnode->nSendOffset += nBytes;
            pnode->RecordBytesSent(nBytes);
            if (pnode->nSendOffset == data.size()) {
                pnode->nSendOffset = 0;
                pnode->nSendSize -= data.size();
                it++;
            } else {
                // could not send full message; stop sending more
                break;
            }
        }
        else
        {
            if (nBytes < 0) {
                // error
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                {
                    LogPrint(BCLog::LogFlags::NET, "socket send error %d", nErr);
                    pnode->CloseSocketDisconnect();
                }
            }
            // couldn't send anything at all
            break;
        }
    }

    if (it == pnode->vSendMsg.end()) {
        assert(pnode->nSendOffset == 0);
        assert(pnode->nSendSize == 0);
    }
    pnode->vSendMsg.erase(pnode->vSendMsg.begin(), it);
}

void ThreadSocketHandler(void* parg)
{
    // Make this thread recognisable as the networking thread
    RenameThread("grc-net");
    util::ThreadSetInternalName("grc-net");

    try
    {
        ThreadSocketHandler2(parg);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "ThreadSocketHandler()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrintf("ThreadSocketHandler exited (interrupt)");
        return;
    }
    catch (...)
    {
        throw; // support pthread_cancel()
    }
    LogPrintf("ThreadSocketHandler exited");
}

void ThreadSocketHandler2(void* parg)
{
    LogPrint(BCLog::LogFlags::NET, "ThreadSocketHandler started");
    list<CNode*> vNodesDisconnected;
    unsigned int nPrevNodeCount = 0;

    while (true)
    {
        //
        // Disconnect nodes
        //
        {
            LOCK(cs_vNodes);
            // Disconnect unused nodes
            vector<CNode*> vNodesCopy = vNodes;
            for (auto const& pnode : vNodesCopy)
            {
                // vRecvMsg / nSendSize / ssSend are guarded by per-node locks.
                // Take them in canonical order (cs_vRecvMsg first, then cs_vSend)
                // for the disconnect-eligibility check. Lazy: only if refcount
                // has dropped to zero, since the test short-circuits otherwise.
                bool empty_buffers = false;
                if (pnode->GetRefCount() <= 0) {
                    LOCK2(pnode->cs_vRecvMsg, pnode->cs_vSend);
                    empty_buffers = pnode->vRecvMsg.empty()
                                    && pnode->nSendSize == 0
                                    && pnode->ssSend.empty();
                }
                if (pnode->fDisconnect || empty_buffers)
                {
                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // release outbound grant (if any)
                    pnode->grantOutbound.Release();

                    // close socket and cleanup
                    pnode->CloseSocketDisconnect();

                    // hold in disconnected pool until all refs are released
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }

            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            for (auto const& pnode : vNodesDisconnectedCopy)
            {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0)
                {
                    bool fDelete = false;
                    {
                        TRY_LOCK(pnode->cs_vSend, lockSend);
                        if (lockSend)
                        {
                            TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                            if (lockRecv)
                            {
                                TRY_LOCK(pnode->cs_inventory, lockInv);
                                if (lockInv)
                                    fDelete = true;
                            }
                        }
                    }
                    if (fDelete)
                    {
                        vNodesDisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
        }

        size_t vNodesSize;
        {
            LOCK(cs_vNodes);
            vNodesSize = vNodes.size();
        }

        if(vNodesSize != nPrevNodeCount)
        {
            nPrevNodeCount = vNodesSize;
            uiInterface.NotifyNumConnectionsChanged(nPrevNodeCount);
        }


        //
        // Find which sockets have data to receive
        //
        // poll(2) on POSIX / select(2) on Windows, via Sock::WaitMany
        // (issue #2558 PR 5b). 50ms keeps the old pnode->vSend poll cadence.
        constexpr auto timeout = std::chrono::milliseconds{50};

        Sock::EventsPerSock events_per_sock;

        for (auto const& hListenSocket : vhListenSocket) {
            events_per_sock.emplace(hListenSocket, Sock::Events{Sock::RECV});
        }
        {
            LOCK(cs_vNodes);
            for (auto const& pnode : vNodes)
            {
                const std::shared_ptr<Sock> sock = pnode->GetSock();
                if (!sock || sock->Get() == INVALID_SOCKET)
                    continue;
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend) {
                    // do not read, if draining write queue
                    const Sock::Event requested =
                        (pnode->vSendMsg.empty() ? Sock::RECV : Sock::SEND) | Sock::ERR;
                    events_per_sock.emplace(sock, Sock::Events{requested});
                }
                // A node whose cs_vSend is contended this round is simply left
                // out of the wait set and serviced next iteration (as before).
            }
        }

        if (events_per_sock.empty())
        {
            // Nothing to wait on this round; keep the poll cadence.
            if (!MilliSleep(timeout.count())) return;
        }
        else if (!Sock::WaitMany(timeout, events_per_sock))
        {
            if (fShutdown)
                return;
            LogPrint(BCLog::LogFlags::NET, "socket wait error %d", WSAGetLastError());
            if (!MilliSleep(timeout.count())) return;
            continue; // rebuild the wait set next iteration
        }
        if (fShutdown)
            return;


        //
        // Accept new connections
        //
        for (auto const& hListenSocket : vhListenSocket)
        if (events_per_sock.at(hListenSocket).occurred & Sock::RECV)
        {
            struct sockaddr_storage sockaddr;
            socklen_t len = sizeof(sockaddr);
            SOCKET hSocket = accept(hListenSocket->Get(), (struct sockaddr*)&sockaddr, &len);
            CAddress addr;
            int nInbound = 0;

            int max_connections = std::min<int>(gArgs.GetArg("-maxconnections", 125), 950);

            if (hSocket != INVALID_SOCKET)
                if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
                    LogPrintf("Warning: Unknown socket family");

            std::set<CNetAddr> setActiveInbound;
            {
                LOCK(cs_vNodes);
                for (auto const& pnode : vNodes)
                {
                    if (pnode->fInbound)
                    {
                        nInbound++;
                        setActiveInbound.insert(pnode->addr);
                    }
                }
            }

            if (hSocket == INVALID_SOCKET)
            {
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK)
                    LogPrintf("socket error accept INVALID_SOCKET: %d", nErr);
            }
            else if (nInbound >= max_connections - MAX_OUTBOUND_CONNECTIONS)
            {
                LogPrint(BCLog::LogFlags::NET,
                         "Surpassed max inbound connections of %i",
                         std::max<int>(max_connections - MAX_OUTBOUND_CONNECTIONS, 0));

                closesocket(hSocket);
            }
            else if (g_banman->IsBanned(addr))
            {
                LogPrint(BCLog::LogFlags::NET, "connection from %s dropped (banned)", addr.ToString());
                closesocket(hSocket);
            }
            else
            {
                // Rate-limit inbound connections: at most one connection per
                // 5 seconds from the same IP. Rapid reconnection scores
                // misbehavior points (10 per violation) which accumulate
                // toward a ban at the default banscore threshold.
                {
                    static std::map<CNetAddr, int64_t> mapInboundLastConnect;
                    int64_t nNow = GetAdjustedTime();
                    auto it = mapInboundLastConnect.find(addr);

                    if (it != mapInboundLastConnect.end() && nNow - it->second < 5)
                    {
                        if (g_peerman) g_peerman->Misbehaving(addr, 10);
                        closesocket(hSocket);
                        mapInboundLastConnect[addr] = nNow;
                        continue;
                    }

                    mapInboundLastConnect[addr] = nNow;

                    // Prune entries for IPs that have no active inbound
                    // CNode and whose last connection attempt is older
                    // than the rate-limit window.
                    for (auto mit = mapInboundLastConnect.begin(); mit != mapInboundLastConnect.end(); )
                    {
                        if (nNow - mit->second >= 5 && !setActiveInbound.count(mit->first))
                            mit = mapInboundLastConnect.erase(mit);
                        else
                            ++mit;
                    }
                }

                LogPrint(BCLog::LogFlags::NET, "accepted connection %s", addr.ToString());
                CNode* pnode = new CNode(hSocket, addr, "", true);
                pnode->AddRef();
                {
                    LOCK(cs_vNodes);
                    vNodes.push_back(pnode);
                }

                // We received a new connection, harvest entropy from the time (and our peer count)
                RandAddEvent((uint32_t)pnode->GetId());
            }
        }


        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (auto const& pnode : vNodesCopy)
                pnode->AddRef();
        }
        for (auto const& pnode : vNodesCopy)
        {
            if (fShutdown)
                return;

            //
            // Receive
            //
            // Hold a shared_ptr copy of the socket across the recv so the fd
            // cannot be closed underneath us (issue #2558 PR 5a).
            std::shared_ptr<Sock> sock = pnode->GetSock();
            if (!sock || sock->Get() == INVALID_SOCKET)
                continue;
            Sock::Event occurred = 0;
            {
                const auto it = events_per_sock.find(sock);
                if (it != events_per_sock.end())
                    occurred = it->second.occurred;
            }
            if (occurred & (Sock::RECV | Sock::ERR))
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    if (pnode->GetTotalRecvSize() > ReceiveFloodSize()) {
                        if (!pnode->fDisconnect)
                            LogPrintf("socket recv flood control disconnect (%u bytes)", pnode->GetTotalRecvSize());
                        pnode->CloseSocketDisconnect();
                    }
                    else {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int nBytes = sock->Recv(pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            if (!pnode->ReceiveMsgBytes(pchBuf, nBytes))
                                pnode->CloseSocketDisconnect();
                            pnode->nLastRecv = GetAdjustedTime();
                            pnode->RecordBytesRecv(nBytes);
                        }
                        else if (nBytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                            {
                              LogPrint(BCLog::LogFlags::NET, "socket closed");
                            }
                            pnode->CloseSocketDisconnect();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                if (!pnode->fDisconnect)
                                {
                                   LogPrint(BCLog::LogFlags::NET, "socket recv error %d", nErr);
                                }
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            // Re-fetch: the recv path above may have closed the socket.
            sock = pnode->GetSock();
            if (!sock || sock->Get() == INVALID_SOCKET)
                continue;
            occurred = 0;
            {
                const auto it = events_per_sock.find(sock);
                if (it != events_per_sock.end())
                    occurred = it->second.occurred;
            }
            if (occurred & Sock::SEND)
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SocketSendData(pnode);
            }

            //
            // Inactivity checking
            //
            // Consider this for future removal as this really is not beneficial nor harmful.
            // Read vNodesCopy.size() (the snapshot taken under cs_vNodes above
            // at line 1100) rather than vNodes.size() — vNodes is GUARDED_BY
            // (cs_vNodes) and the size at iteration time is what this check
            // wants anyway.
            if ((GetAdjustedTime() - pnode->nTimeConnected) > (60*60*2) && (vNodesCopy.size() > (MAX_OUTBOUND_CONNECTIONS*.75)))
            {
                    LogPrint(BCLog::LogFlags::NET, "Node %s connected longer than 2 hours with connection count of %zd, disconnecting. ",
                             pnode->addr.ToString(), vNodesCopy.size());

                    pnode->fDisconnect = true;

                    continue;
            }

            int64_t nTime = GetAdjustedTime();

            if (nTime - pnode->nTimeConnected > PEER_TIMEOUT)
            {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0)
                {
                    LogPrint(BCLog::LogFlags::NET, "socket no message in first %d seconds, %d %d",
                             PEER_TIMEOUT, pnode->nLastRecv != 0, pnode->nLastSend != 0);

                    pnode->fDisconnect = true;

                    continue;
                }

                else if (nTime - pnode->nLastSend > TIMEOUT_INTERVAL)
                {
                    LogPrintf("socket sending timeout: %" PRId64 "s", nTime - pnode->nLastSend);

                    pnode->fDisconnect = true;

                    continue;
                }

                else if (nTime - pnode->nLastRecv > TIMEOUT_INTERVAL)
                {
                    LogPrintf("socket receive timeout: %" PRId64 "s", nTime - pnode->nLastRecv);

                    pnode->fDisconnect = true;

                    continue;
                }

                else if (pnode->nPingNonceSent && pnode->nPingUsecStart + TIMEOUT_INTERVAL * 1000000 < GetTimeMicros())
                {
                    LogPrintf("ping timeout: %fs", 0.000001 * (GetTimeMicros() - pnode->nPingUsecStart));

                    pnode->fDisconnect = true;

                    continue;
                }
            }
        }
        {
            LOCK(cs_vNodes);
            for (auto const& pnode : vNodesCopy)
                pnode->Release();
        }

        UninterruptibleSleep(std::chrono::milliseconds{10});
    }
}

#ifdef USE_UPNP
void ThreadMapPort(void* parg)
{
    // Make this thread recognisable as the UPnP thread
    RenameThread("grc-UPnP");
    util::ThreadSetInternalName("grc-UPnP");

    try
    {
        ThreadMapPort2(parg);
    }
    catch (std::exception& e) {
        PrintException(&e, "ThreadMapPort()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrintf("ThreadMapPort exited (interrupt)");
        return;
    }
    catch (...)
    {
        PrintException(nullptr, "ThreadMapPort()");
    }
    LogPrintf("ThreadMapPort exited");
}

void ThreadMapPort2(void* parg)
{
    LogPrint(BCLog::LogFlags::NET, "ThreadMapPort started");

    std::string port = strprintf("%u", GetListenPort());
    const char* multicastif = nullptr;
    const char* minissdpdpath = nullptr;
    struct UPNPDev* devlist = nullptr;
    char lanaddr[64];
#if MINIUPNPC_API_VERSION >= 18
    char wanaddr[64];
#endif

    int error = 0;
#if MINIUPNPC_API_VERSION < 14
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#else
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, 2, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

#if MINIUPNPC_API_VERSION < 18
    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
#else
    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr), wanaddr, sizeof(wanaddr));
#endif
    if (r == 1)
    {
        if (fDiscover) {
            // Note that the below is technically duplicative for API version > 18, since the wanaddr is filled out
            // by UPNP_GetValidIGD in the internal call to UPNP_GetExternalIPAddress for API version > 18. However,
            // it is not harmful to leave the additional separate call here.
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if(r != UPNPCOMMAND_SUCCESS)
                LogPrintf("UPnP: GetExternalIPAddress() returned %d", r);
            else
            {
                if(externalIPAddress[0])
                {
                    CNetAddr resolved;
                    if(LookupHost(externalIPAddress, resolved, false)) {
                        LogPrintf("UPnP: ExternalIPAddress = %s\n", resolved.ToString().c_str());
                        AddLocal(resolved, LOCAL_UPNP);
                    }
                }
                else
                    LogPrintf("UPnP: GetExternalIPAddress not successful.");
            }
        }

        string strDesc = "Gridcoin " + FormatFullVersion();

        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", nullptr, "0");

        if(r!=UPNPCOMMAND_SUCCESS)
            LogPrintf("AddPortMapping(%s, %s, %s) unsuccessful with code %d (%s)",
                port, port, lanaddr, r, strupnperror(r));
        else
            LogPrintf("UPnP Port Mapping successful.");
        int i = 1;
        while (true)
        {
            if (fShutdown || !g_connman || !g_connman->GetUseUPnP())
            {
                r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", nullptr);
                LogPrintf("UPNP_DeletePortMapping() returned : %d", r);
                freeUPNPDevlist(devlist);
                devlist = nullptr;
                FreeUPNPUrls(&urls);
                return;
            }
            if (i % 600 == 0) // Refresh every 20 minutes
            {
#ifndef UPNPDISCOVER_SUCCESS
                /* miniupnpc 1.5 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
                /* miniupnpc 1.6 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                        port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", nullptr, "0");
#endif

                if(r!=UPNPCOMMAND_SUCCESS)
                    LogPrintf("AddPortMapping(%s, %s, %s) was not successful - code %d (%s)",
                        port, port, lanaddr, r, strupnperror(r));
                else
                    LogPrintf("UPnP Port Mapping successful.");;
            }
            if (!MilliSleep(2000)) return;
            i++;
        }
    } else {
        LogPrint(BCLog::LogFlags::NET, "No valid UPnP IGDs found");
        freeUPNPDevlist(devlist);
        devlist = nullptr;
        if (r != 0)
            FreeUPNPUrls(&urls);
        while (true)
        {
            if (fShutdown || !g_connman || !g_connman->GetUseUPnP()) return;
            if (!MilliSleep(2000)) return;
        }
    }
}

void MapPort()
{
    if (g_connman && g_connman->GetUseUPnP() && !netThreads->threadExists("ThreadMapPort"))
    {
        if (!netThreads->createThread(ThreadMapPort, nullptr, "ThreadMapPort"))
            LogPrintf("Error: createThread(ThreadMapPort) failed");
    }
}
#else
void MapPort()
{
    // Intentionally left blank.
}
#endif

// DNS seeds
// Each pair gives a source name and a seed name.
// The first name is used as information source for addrman.
// The second name should resolve to a list of seed addresses.
static const char *strDNSSeed[][2] = {
    {"addnode-us-central.cycy.me", "addnode-us-central.cycy.me"},
    {"ec2-3-81-39-58.compute-1.amazonaws.com", "ec2-3-81-39-58.compute-1.amazonaws.com"},
    {"node.grcpool.com", "node.grcpool.com"},
    {"seeds.gridcoin.ifoggz-network.xyz", "seeds.gridcoin.ifoggz-network.xyz"},
    {"node.gridcoin.network", "node.gridcoin.network"},
    {"", ""},
};

void ThreadDNSAddressSeed(void* parg)
{
    // Make this thread recognisable as the DNS seeding thread
    RenameThread("grc-dnsseed");
    util::ThreadSetInternalName("grc-dnsseed");

    try
    {
        ThreadDNSAddressSeed2(parg);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "ThreadDNSAddressSeed()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrint(BCLog::LogFlags::NET, "ThreadDNSAddressSeed exited (interrupt)");
        return;
    }
    catch (...)
    {
        throw; // support pthread_cancel()
    }
    LogPrint(BCLog::LogFlags::NET, "ThreadDNSAddressSeed exited");
}

void ThreadDNSAddressSeed2(void* parg)
{
    LogPrint(BCLog::LogFlags::NET, "ThreadDNSAddressSeed started");
    int found = 0;

    if (!fTestNet)
    {
        LogPrint(BCLog::LogFlags::NET, "Loading addresses from DNS seeds (could take a while)");

        for (const auto& seed : strDNSSeed) {
            if (HaveNameProxy()) {
                AddOneShot(seed[1]);
            } else {
                vector<CNetAddr> vIPs;
                vector<CAddress> vAdd;
                if (LookupHost(seed[1], vIPs, 0, true))
                {
                    for (auto const& ip : vIPs)
                    {
                        int nOneDay = 24*3600;
                        CAddress addr = CAddress(CService(ip, GetDefaultPort()));
                        addr.nTime = GetAdjustedTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
                        vAdd.push_back(addr);
                        found++;
                    }
                }
                // TODO: The seed name resolve may fail, yielding an IP of [::], which results in
                // addrman assigning the same source to results from different seeds.
                // This should switch to a hard-coded stable dummy IP for each seed name, so that the
                // resolve is not required at all.
                if (!vIPs.empty()) {
                    CService seedSource;
                    Lookup(seed[0], seedSource, 0, true);
                    addrman.Add(vAdd, seedSource);
                }
            }
        }
    }

    LogPrint(BCLog::LogFlags::NET, "%d addresses found from DNS seeds", found);
}

unsigned int pnSeed[] =
{
    0xdf4bd379, 0x7934d29b, 0x26bc02ad, 0x7ab743ad, 0x0ab3a7bc,
    0x375ab5bc, 0xc90b1617, 0x5352fd17, 0x5efc6c18, 0xccdc7d18,
    0x443d9118, 0x84031b18, 0x347c1e18, 0x86512418, 0xfcfe9031,
    0xdb5eb936, 0xef8d2e3a, 0xcf51f23c, 0x18ab663e, 0x36e0df40,
    0xde48b641, 0xad3e4e41, 0xd0f32b44, 0x09733b44, 0x6a51f545,
    0xe593ef48, 0xc5f5ef48, 0x96f4f148, 0xd354d34a, 0x36206f4c,
    0xceefe953, 0x50468c55, 0x89d38d55, 0x65e61a5a, 0x16b1b95d,
    0x702b135e, 0x0f57245e, 0xdaab5f5f, 0xba15ef63,
};

void DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

    CAddrDB adb;
    adb.Write(addrman);

    LogPrint(BCLog::LogFlags::NET, "Flushed %d addresses to peers.dat  %" PRId64 "ms",
             addrman.size(), GetTimeMillis() - nStart);
}

void ThreadDumpAddress2(void* parg)
{
    while (!fShutdown)
    {
        DumpAddresses();
        if (!MilliSleep(600000)) return;
    }
}

void ThreadDumpAddress(void* parg)
{
    // Make this thread recognisable as the address dumping thread
    RenameThread("grc-adrdump");
    util::ThreadSetInternalName("grc-adrdump");

    try
    {
        ThreadDumpAddress2(parg);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "ThreadDumpAddress()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrintf("ThreadDumpAddress exited (interrupt)");
        return;
    }
    catch (...)
    {
        PrintException(nullptr, "ThreadDumpAddress");
    }
    LogPrintf("ThreadDumpAddress exited");
}

void ThreadOpenConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("grc-opencon");
    util::ThreadSetInternalName("grc-opencon");

    try
    {
        ThreadOpenConnections2(parg);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "ThreadOpenConnections()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrintf("ThreadOpenConnections exited (interrupt)");
        return;
    }
    catch (...)
    {
        PrintException(nullptr, "ThreadOpenConnections()");
    }
    LogPrintf("ThreadOpenConnections exited");
}

void static ProcessOneShot()
{
    string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty())
            return;
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        if (!OpenNetworkConnection(addr, &grant, strDest.c_str(), true))
            AddOneShot(strDest);
    }
}

void CNode::RecordBytesRecv(uint64_t bytes)
{
    nTotalBytesRecv += bytes;
}

void CNode::RecordBytesSent(uint64_t bytes)
{
    nTotalBytesSent += bytes;
}

uint64_t CNode::GetTotalBytesRecv()
{
    return nTotalBytesRecv;
}

uint64_t CNode::GetTotalBytesSent()
{
    return nTotalBytesSent;
}

void ThreadOpenConnections2(void* parg)
{
    LogPrint(BCLog::LogFlags::NET, "ThreadOpenConnections started");

    // Connect to specific addresses
    if (gArgs.GetArgs("-connect").size())
    {
        for (int64_t nLoop = 0;; nLoop++)
        {
            ProcessOneShot();
            for (auto const& strAddr : gArgs.GetArgs("-connect"))
            {
                CAddress addr;
                OpenNetworkConnection(addr, nullptr, strAddr.c_str());
                for (int i = 0; i < 10 && i < nLoop; i++)
                {
                    UninterruptibleSleep(std::chrono::milliseconds{500});
                    if (fShutdown)
                        return;
                }
            }
            UninterruptibleSleep(std::chrono::milliseconds{500});
        }
    }

    // Initiate network connections
    int64_t nStart = GetAdjustedTime();
    while (true)
    {
        ProcessOneShot();
        UninterruptibleSleep(std::chrono::milliseconds{500});

        if (fShutdown)
            return;

        CSemaphoreGrant grant(*semOutbound);
        if (fShutdown)
            return;

        // Add seed nodes
        if (addrman.size() == 0 && (GetAdjustedTime() - nStart > 60) && !fTestNet)
        {
            std::vector<CAddress> vAdd;
            for (const auto& seed : pnSeed)
            {
                // It'll only connect to one or two seed nodes because once it connects,
                // it'll get a pile of addresses with newer timestamps.
                // Seed nodes are given a random 'last seen time' of between one and two
                // weeks ago.
                const int64_t nOneWeek = 7*24*60*60;
                struct in_addr ip;
                memcpy(&ip, &seed, sizeof(ip));
                CAddress addr(CService(ip, GetDefaultPort()));
                addr.nTime = GetAdjustedTime() - GetRand(nOneWeek) - nOneWeek;
                vAdd.push_back(addr);
            }
            CNetAddr local;
            LookupHost("127.0.0.1", local, false);
            addrman.Add(vAdd, local);
        }

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        // Only connect out to one peer per network group (/16 for IPv4).
        // Do this here so we don't have to critsect vNodes inside mapAddresses critsect.
        set<vector<unsigned char> > setConnected;
        {
            LOCK(cs_vNodes);
            for (auto const& pnode : vNodes) {
                if (!pnode->fInbound) {
                    setConnected.insert(pnode->addr.GetGroup());
                }
            }
        }

        int64_t nANow = GetAdjustedTime();

        int nTries = 0;
        while (true)
        {
            CAddress addr = addrman.Select();

            // if we selected an invalid address, restart
            if (!addr.IsValid() || setConnected.count(addr.GetGroup()) || IsLocal(addr))
                break;

            // If we didn't find an appropriate destination after trying 100 addresses fetched from addrman,
            // stop this loop, and let the outer loop run again (which sleeps, adds seed nodes, recalculates
            // already-connected network ranges, ...) before trying new addrman addresses.
            nTries++;
            if (nTries > 100)
                break;

            if (IsLimited(addr))
                continue;

            // only consider very recently tried nodes after 30 failed attempts
            if (nANow - addr.nLastTry < 600 && nTries < 30)
                continue;

            // do not allow non-default ports, unless after 50 invalid addresses selected already
            if (addr.GetPort() != GetDefaultPort() && nTries < 50)
                continue;

            addrConnect = addr;
            break;
        }

        if (addrConnect.IsValid())
            OpenNetworkConnection(addrConnect, &grant);
    }
}

void ThreadOpenAddedConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("grc-openaddedcon");
    util::ThreadSetInternalName("grc-openaddedcon");

    try
    {
        ThreadOpenAddedConnections2(parg);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "ThreadOpenAddedConnections()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrintf("ThreadOpenAddedConnections exited (interrupt)");
        return;
    }
    catch (...)
    {
        PrintException(nullptr, "ThreadOpenAddedConnections()");
    }
    LogPrintf("ThreadOpenAddedConnections exited");
}

void ThreadOpenAddedConnections2(void* parg)
{
    LogPrint(BCLog::LogFlags::NET, "ThreadOpenAddedConnections started");

    if (gArgs.GetArgs("-addnode").empty())
        return;

    if (HaveNameProxy()) {
        while(!fShutdown) {
            for (auto const& strAddNode : gArgs.GetArgs("-addnode")) {
                CAddress addr;
                CSemaphoreGrant grant(*semOutbound);
                OpenNetworkConnection(addr, &grant, strAddNode.c_str());
                UninterruptibleSleep(std::chrono::milliseconds{500});
            }
            if (!MilliSleep(120000)) return; // Retry every 2 minutes
        }
        return;
    }

    vector<vector<CService> > vservAddressesToAdd(0);
    for (auto const& strAddNode : gArgs.GetArgs("-addnode"))
    {
        LogPrint(BCLog::LogFlags::NET, "INFO: %s: addnode %s.", __func__, strAddNode);

        vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, GetDefaultPort(), fNameLookup, 0))
        {
            vservAddressesToAdd.push_back(vservNode);
            {
                LOCK(cs_setservAddNodeAddresses);
                for (auto const& serv : vservNode)
                    setservAddNodeAddresses.insert(serv);
            }
        }
    }
    while (true)
    {
        vector<vector<CService> > vservConnectAddresses = vservAddressesToAdd;
        // Attempt to connect to each IP for each addnode entry until at least one is successful per addnode entry
        // (keeping in mind that addnode entries can have many IPs if fNameLookup)
        {
            LOCK(cs_vNodes);
            for (auto const& pnode : vNodes)
                for (vector<vector<CService> >::iterator it = vservConnectAddresses.begin(); it != vservConnectAddresses.end(); it++)
                    for (auto const& addrNode : *(it))
                        if (pnode->addr == addrNode)
                        {
                            it = vservConnectAddresses.erase(it);
                            it--;
                            break;
                        }
        }
        for (auto const& vserv : vservConnectAddresses)
        {
            CSemaphoreGrant grant(*semOutbound);
            OpenNetworkConnection(CAddress(*(vserv.begin())), &grant);
            UninterruptibleSleep(std::chrono::milliseconds{500});
            if (fShutdown) return;
        }
        if (fShutdown) return;
        if (!MilliSleep(120000)) return; // Retry every 2 minutes
        if (fShutdown) return;
    }
}

// if successful, this moves the passed grant to the constructed node
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant *grantOutbound, const char *strDest, bool fOneShot)
{
    //
    // Initiate outbound network connection
    //
    if (fShutdown)
        return false;
    if (!strDest)
        if (IsLocal(addrConnect) ||
            FindNode((CNetAddr)addrConnect) || g_banman->IsBanned(addrConnect) ||
            FindNode(addrConnect.ToStringIPPort().c_str()))
            return false;
    if (strDest && FindNode(strDest))
        return false;

    CNode* pnode = ConnectNode(addrConnect, strDest);
    if (fShutdown)
        return false;
    if (!pnode)
        return false;
    if (grantOutbound)
        grantOutbound->MoveTo(pnode->grantOutbound);
    pnode->fNetworkNode = true;
    if (fOneShot)
        pnode->fOneShot = true;

    return true;
}

void ThreadMessageHandler(void* parg)
{
    // Make this thread recognisable as the message handling thread
    RenameThread("grc-msghand");
    util::ThreadSetInternalName("grc-msghand");

    try
    {
        ThreadMessageHandler2(parg);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "ThreadMessageHandler()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrintf("ThreadMessageHandler exited (interrupt)");
        return;
    }
    catch (...)
    {
        PrintException(nullptr, "ThreadMessageHandler()");
    }
    LogPrintf("ThreadMessageHandler exited");
}

void ThreadMessageHandler2(void* parg)
{
    LogPrint(BCLog::LogFlags::NET, "ThreadMessageHandler started");
    while (!fShutdown)
    {
        // Drive message processing through the connection manager's configured
        // NetEventsInterface (issue #2558 PR 8c) rather than naming g_peerman
        // here. Null if none is configured (no pumping then).
        NetEventsInterface* msgproc = g_connman ? g_connman->GetMessageProcessor() : nullptr;

        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (auto const& pnode : vNodesCopy)
                pnode->AddRef();
        }

        // Poll the connected nodes for messages
        CNode* pnodeTrickle = nullptr;
        if (!vNodesCopy.empty())
            pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];
        for (auto const& pnode : vNodesCopy)
        {
            if (pnode->fDisconnect)
                continue;

            //11-25-2015
            // Receive messages
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                    if (msgproc && !msgproc->ProcessMessages(pnode))
                        pnode->CloseSocketDisconnect();
            }

            if (fShutdown)
                return;

            // Send messages
            {
                // Having the outer cs_main TRY_LOCK here with reversed logic
                // has the same effect as the original TRY_LOCK in Sendmessages,
                // which was if !lockMain return true immediately. (Note
                // that the return value of SendMessages was never consumed.
                // and this is the only place in the code where SendMessages is
                // called. This is to eliminate a potential deadlock condition
                // due to logic order reversal.

                TRY_LOCK(cs_main, lockMain);
                if(lockMain)
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend)
                    {
                        if (msgproc) msgproc->SendMessages(pnode, pnode == pnodeTrickle);
                    }
                }
            }

            if (fShutdown)
                return;
        }

        {
            LOCK(cs_vNodes);
            for (auto const& pnode : vNodesCopy)
                pnode->Release();
        }

        // Wait and allow messages to bunch up.
        // we're sleeping, but we must always check fShutdown after doing this.
        UninterruptibleSleep(std::chrono::milliseconds{100});
        if (fRequestShutdown)
            StartShutdown();
        if (fShutdown)
            return;
    }
}




bool BindListenPort(const CService &addrBind, string& strError)
{
    strError = "";
    int nOne = 1;

#ifdef WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR)
    {
        strError = strprintf("Error: TCP/IP socket library refused to start (WSAStartup returned error %d)", ret);
        LogPrintf("%s", strError);
        return false;
    }
#endif

    // Create socket for listening for incoming connections
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len))
    {
        strError = strprintf("Error: bind address family for %s not supported", addrBind.ToString());
        LogPrintf("%s", strError);
        return false;
    }

    SOCKET hListenSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        strError = strprintf("Error: Couldn't open socket for incoming connections (socket returned error %d)", WSAGetLastError());
        LogPrintf("%s", strError);
        return false;
    }

#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif

#ifndef WIN32
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.  Not an issue on Windows.
    if (setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int)) < 0)
        LogPrint(BCLog::LogFlags::NET, "setsockopt(SO_REUSEADDR) failed");
#ifdef SO_REUSEPORT
    // Not all systems have SO_REUSEPORT. Required by OSX, available in some
    // Linux flavors.
    if (setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEPORT, (void*)&nOne, sizeof(int)) < 0)
        LogPrint(BCLog::LogFlags::NET, "setsockopt(SO_SO_REUSEPORT) failed");
#endif
#endif


#ifdef WIN32
    // Set to non-blocking, incoming connections will also inherit this
    if (ioctlsocket(hListenSocket, FIONBIO, (u_long*)&nOne) == SOCKET_ERROR)
#else
    if (fcntl(hListenSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
#endif
    {
        strError = strprintf("Error: Couldn't set properties on socket for incoming connections (error %d)", WSAGetLastError());
        LogPrintf("%s", strError);
        return false;
    }

    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
#ifdef WIN32
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&nOne, sizeof(int));
#else
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&nOne, sizeof(int));
#endif
#endif
#ifdef WIN32
        int nProtLevel = 10 /* PROTECTION_LEVEL_UNRESTRICTED */;
        int nParameterId = 23 /* IPV6_PROTECTION_LEVEl */;
        // this call is allowed to fail
        setsockopt(hListenSocket, IPPROTO_IPV6, nParameterId, (const char*)&nProtLevel, sizeof(int));
#endif
    }

    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to %s on this computer. Gridcoin is probably already running."), addrBind.ToString());
        else
            strError = strprintf(_("Unable to bind to %s on this computer (bind returned error %d, %s)"), addrBind.ToString(), nErr, strerror(nErr));
        LogPrintf("%s", strError);
        return false;
    }
    LogPrint(BCLog::LogFlags::NET, "Bound to %s", addrBind.ToString());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf("Error: Listening for incoming connections died with %d", WSAGetLastError());
        LogPrintf("%s", strError);
        return false;
    }

    vhListenSocket.push_back(std::make_shared<Sock>(hListenSocket));

    if (addrBind.IsRoutable() && fDiscover)
        AddLocal(addrBind, LOCAL_BIND);

    return true;
}

void static Discover()
{
    if (!fDiscover)
        return;

#ifdef WIN32
    // Get local host IP
    char pszHostName[1000] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR)
    {
        vector<CNetAddr> vaddr;
        if (LookupHost(pszHostName, vaddr, 0, true))
        {
            for (auto const& addr : vaddr)
            {
                AddLocal(addr, LOCAL_IF);
            }
        }
    }
#else
    // Get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs* ifa = myaddrs; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            if ((ifa->ifa_flags & IFF_UP) == 0) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0) continue;
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                CNetAddr addr(s4->sin_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("IPv4 %s: %s", ifa->ifa_name, addr.ToString());
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                CNetAddr addr(s6->sin6_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("IPv6 %s: %s", ifa->ifa_name, addr.ToString());
            }
        }
        freeifaddrs(myaddrs);
    }
#endif
}

CConnman::CConnman(uint64_t seed0, uint64_t seed1, AddrMan& addrmanIn, bool network_active)
    : m_addrman(addrmanIn)
    , nSeed0(seed0)
    , nSeed1(seed1)
    , fNetworkActive(network_active)
{
}

CConnman::~CConnman() = default;

// Node-access API (issue #2558 PR 9a). Read-only views over the connection set,
// backed by the still-global vNodes/cs_vNodes. Each method takes cs_vNodes
// internally so callers no longer touch the globals directly.
size_t CConnman::GetNodeCount(NumConnections flags) const
{
    LOCK(cs_vNodes);
    if (flags == CONNECTIONS_ALL) return vNodes.size();
    size_t nNum = 0;
    for (const auto& pnode : vNodes) {
        if (flags & (pnode->fInbound ? CONNECTIONS_IN : CONNECTIONS_OUT)) ++nNum;
    }
    return nNum;
}

void CConnman::GetNodeStats(std::vector<CNodeStats>& vstats) const
{
    // Delegate to the existing snapshot helper (same cs_vNodes lock + per-node
    // copyStats); it folds into this method when storage moves into CConnman.
    CNode::CopyNodeStats(vstats);
}

void CConnman::ForEachNode(const std::function<void(CNode*)>& func)
{
    LOCK(cs_vNodes);
    for (const auto& pnode : vNodes) {
        func(pnode);
    }
}

bool CConnman::DisconnectNode(const std::string& strNode)
{
    LOCK(cs_vNodes);
    if (CNode* pnode = FindNode(strNode)) {
        pnode->fDisconnect = true;
        return true;
    }
    return false;
}

bool CConnman::DisconnectNode(const CSubNet& subnet)
{
    bool disconnected = false;
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (subnet.Match(pnode->addr)) {
            pnode->fDisconnect = true;
            disconnected = true;
        }
    }
    return disconnected;
}

bool CConnman::DisconnectNode(const CNetAddr& addr)
{
    return DisconnectNode(CSubNet(addr));
}

bool CConnman::DisconnectNode(NodeId id)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (id == pnode->GetId()) {
            pnode->fDisconnect = true;
            return true;
        }
    }
    return false;
}

bool CConnman::AddNode(const std::string& strNode)
{
    LOCK(cs_vAddedNodes);
    for (const auto& it : vAddedNodes) {
        if (strNode == it) return false;
    }
    vAddedNodes.push_back(strNode);
    return true;
}

bool CConnman::RemoveAddedNode(const std::string& strNode)
{
    LOCK(cs_vAddedNodes);
    for (auto it = vAddedNodes.begin(); it != vAddedNodes.end(); ++it) {
        if (strNode == *it) {
            vAddedNodes.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<std::string> CConnman::GetAddedNodes() const
{
    LOCK(cs_vAddedNodes);
    return vAddedNodes;
}

const CBlockLocator& CConnman::GetBlockLocator(const CBlockIndex* pindexBegin)
{
    // Building a locator scans the chain, so cache the last one and reuse it
    // when the same begin index is requested again (issue #2558 PR 9d2; shared
    // across nodes, as the former net.cpp global was).
    if (pindexBegin != m_getblocks_pindex_begin || !m_getblocks_locator) {
        m_getblocks_pindex_begin = pindexBegin;
        m_getblocks_locator = std::make_unique<CBlockLocator>(pindexBegin);
    }
    return *m_getblocks_locator;
}

CAddress CConnman::GetAddrSeenByPeer() const
{
    LOCK(m_addr_seen_by_peer_cs);
    return m_addr_seen_by_peer;
}

void CConnman::SetAddrSeenByPeer(const CAddress& addr)
{
    LOCK(m_addr_seen_by_peer_cs);
    m_addr_seen_by_peer = addr;
}

void CConnman::ForEachNodeUnderLock(const std::function<void(CNode*)>& func) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    LOCK(cs_vNodes);
    for (const auto& pnode : vNodes) {
        func(pnode);
    }
}

void CConnman::RelayInventory(const CInv& inv)
{
    ForEachNode([&inv](CNode* pnode) {
        pnode->PushInventory(inv);
    });
}

void CConnman::RelayAddress(const CAddress& addr, bool fReachable)
{
    LOCK(cs_vNodes);
    // Use deterministic randomness to send to the same nodes for 24 hours
    // at a time so the setAddrKnowns of the chosen nodes prevent repeats.
    static arith_uint256 hashSalt;
    if (hashSalt == 0)
        hashSalt = UintToArith256(GetRandHash());
    uint64_t hashAddr = addr.GetHash();
    uint256 hashRand = ArithToUint256(hashSalt ^ (hashAddr << 32) ^ ((GetAdjustedTime() + hashAddr) / (24 * 60 * 60)));
    hashRand = Hash(hashRand);
    multimap<uint256, CNode*> mapMix;
    for (auto const& pnode : vNodes)
    {
        unsigned int nPointer;
        memcpy(&nPointer, &pnode, sizeof(nPointer));
        uint256 hashKey = ArithToUint256(UintToArith256(hashRand) ^ nPointer);
        hashKey = Hash(hashKey);
        mapMix.insert(make_pair(hashKey, pnode));
    }
    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
        (mi->second)->PushAddress(addr);
}

bool CConnman::Start()
{
    fShutdown = false;
    MAX_OUTBOUND_CONNECTIONS = m_options.nMaxOutbound;
    int max_connections = m_options.nMaxConnections;
    int nMaxOutbound = 0;
    if (semOutbound == nullptr) {
        // initialize semaphore
        nMaxOutbound = std::min<int>(MAX_OUTBOUND_CONNECTIONS, max_connections);
        semOutbound = new CSemaphore(nMaxOutbound);
    }

    LogPrintf("Using %i OutboundConnections with a MaxConnections of %" PRId64,
              nMaxOutbound, max_connections);

    if (pnodeLocalHost == nullptr)
        pnodeLocalHost = new CNode(INVALID_SOCKET, CAddress(LookupNumeric("127.0.0.1", 0), nLocalServices));

    Discover();

    //
    // Start threads
    //

    // Net threads now run as std::thread members owned by CConnman (issue
    // #2558 PR 4). Each loop exits on fShutdown; its interruptible MilliSleep
    // is woken by the global g_thread_interrupt fired in Shutdown(). The
    // optional ThreadMapPort still launches on netThreads (on demand, including
    // the Qt UPnP toggle) and is joined via netThreads->removeAll() in Stop().
    if (!gArgs.GetBoolArg("-dnsseed", true)) {
        LogPrintf("DNS seeding disabled");
    } else {
        m_net_threads.emplace_back(ThreadDNSAddressSeed, nullptr);
    }
    // Map ports with UPnP
    if (m_use_upnp) {
        MapPort();
    }

    // Send and receive from sockets, accept connections
    m_net_threads.emplace_back(ThreadSocketHandler, nullptr);

    // Initiate outbound connections from -addnode
    m_net_threads.emplace_back(ThreadOpenAddedConnections, nullptr);

    // Initiate outbound connections
    m_net_threads.emplace_back(ThreadOpenConnections, nullptr);

    // Process messages
    m_net_threads.emplace_back(ThreadMessageHandler, nullptr);

    // Dump network addresses
    m_net_threads.emplace_back(ThreadDumpAddress, nullptr);

    return true;
}

void CConnman::Interrupt()
{
    fShutdown = true;
    if (semOutbound)
        for (int i=0; i<MAX_OUTBOUND_CONNECTIONS; i++)
            semOutbound->post();
}

void CConnman::Stop()
{
    Interrupt();

    // Join the std::thread net threads. They wake via fShutdown plus the global
    // g_thread_interrupt (already fired in Shutdown before StopNode), so the
    // interruptible MilliSleep loops return promptly.
    for (auto& thread : m_net_threads) {
        if (thread.joinable()) thread.join();
    }
    m_net_threads.clear();

    // ThreadMapPort (if running) still lives on netThreads; join it here.
    netThreads->removeAll();
    UninterruptibleSleep(std::chrono::milliseconds{50});
    DumpAddresses();
}

// Thread entry point launched from AppInit2 Step 12. Thin forwarder to
// CConnman::Start() (issue #2558 PR 3).
void StartNode(void* parg)
{
    // Make this thread recognisable as the startup thread
    RenameThread("grc-nodestart");
    util::ThreadSetInternalName("grc-nodestart");

    if (g_connman) g_connman->Start();
}

bool StopNode()
{
    LogPrintf("StopNode()");
    // Guarded: Shutdown() can run after an early AppInit2 failure, before
    // g_connman is constructed.
    if (g_connman) g_connman->Stop();
    return true;
}

class CNetCleanup
{
public:
    CNetCleanup()
    {
    }
    ~CNetCleanup()
    {
        // Close sockets (the Sock destructor closes the fd; issue #2558 PR 5a).
        for (auto const& pnode : vNodes)
            pnode->CloseSocket();
        // Listen sockets are shared_ptr<Sock> now; the Sock destructors close
        // the fds (issue #2558 PR 5b).
        vhListenSocket.clear();

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
}
instance_of_cnetcleanup;


