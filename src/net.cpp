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
#include "init.h"
#include "node/ui_interface.h"
#include "random.h"
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
void StakeMiner(CWallet *pwallet);

//
// Global state variables
//
bool fDiscover = true;
bool fUseUPnP = false;
ServiceFlags nLocalServices = NODE_NETWORK;
CCriticalSection cs_mapLocalHost;
std::map<CNetAddr, LocalServiceInfo> mapLocalHost;
static bool vfLimited[NET_MAX] GUARDED_BY(cs_mapLocalHost) = {};
static CNode* pnodeLocalHost = nullptr;
CAddress addrSeenByPeer(LookupNumeric("0.0.0.0", 0), nLocalServices);
uint64_t nLocalHostNonce = 0;


std::atomic<uint64_t> CNode::nTotalBytesRecv{ 0 };
std::atomic<uint64_t> CNode::nTotalBytesSent{ 0 };

ThreadHandler* netThreads = new ThreadHandler;
static std::vector<SOCKET> vhListenSocket;
CAddrMan addrman;

// Initialization of static class variable.
std::atomic<NodeId> CNode::nLastNodeId {-1};

vector<CNode*> vNodes;
CCriticalSection cs_vNodes;
vector<std::string> vAddedNodes;
CCriticalSection cs_vAddedNodes;

map<CInv, CDataStream> mapRelay;
deque<pair<int64_t, CInv> > vRelayExpiration;
CCriticalSection cs_mapRelay;
map<CInv, int64_t> mapAlreadyAskedFor;

static deque<string> vOneShots;
CCriticalSection cs_vOneShots;

set<CNetAddr> setservAddNodeAddresses;
CCriticalSection cs_setservAddNodeAddresses;

std::map<CAddress, std::pair<int, int64_t>> CNode::mapMisbehavior;
CCriticalSection CNode::cs_mapMisbehavior;

static CSemaphore* semOutbound = nullptr;

// This caches the block locators used to ask for a range of blocks. Due to a
// sub-optimal workaround in our old net messaging code, a node will ask each
// peer that advertises a block for the next range. The node generates a sub-
// set of hashes from the current block chain used as a locator for the block
// in the chain of the peer. Creating locators is extremely expensive--a node
// needs to scan the entire chain--so we cache the locators and reuse them if
// the node sends the same request. For nodes with many connections, this can
// dramatically improve the performance of the messaging system when it needs
// to respond to new blocks.
//
// This optimization will become unnecessary when we backport newer chain and
// net messaging code from Bitcoin. For now, this cache can greatly improve a
// node's ability to serve a higher number of connections.
//
namespace {
    const CBlockIndex* g_getblocks_pindex_begin = nullptr;
    CBlockLocator g_getblocks_locator;
}

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

    if (pindexBegin != g_getblocks_pindex_begin) {
        g_getblocks_pindex_begin = pindexBegin;
        g_getblocks_locator = CBlockLocator(pindexBegin);
    }

    PushMessage(NetMsgType::GETBLOCKS, g_getblocks_locator, hashEnd);
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
    CService addrLocal = pnode->addrLocal;
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
            addrLocal.SetIP(pnode->addrLocal);
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
//    LOCK(cs_hSocket);
    if (hSocket != INVALID_SOCKET)
    {
        LogPrint(BCLog::LogFlags::NET, "disconnecting node %s", addrName);
        closesocket(hSocket);
        hSocket = INVALID_SOCKET;

        // in case this fails, we'll empty the recv buffer when the CNode is deleted
        TRY_LOCK(cs_vRecvMsg, lockRecv);
        if (lockRecv)
            vRecvMsg.clear();
    }
}


bool CNode::DisconnectNode(const std::string& strNode)
{
    LOCK(cs_vNodes);
    if (CNode* pnode = FindNode(strNode)) {
        pnode->fDisconnect = true;
        return true;
    }
    return false;
}

bool CNode::DisconnectNode(const CSubNet& subnet)
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

bool CNode::DisconnectNode(const CNetAddr& addr)
{
    return CNode::DisconnectNode(CSubNet(addr));
}

bool CNode::DisconnectNode(NodeId id)
{
    LOCK(cs_vNodes);
    for(CNode* pnode : vNodes) {
        if (id == pnode->GetId()) {
            pnode->fDisconnect = true;
            return true;
        }
    }
    return false;
}

void CNode::PushVersion()
{
    int64_t nTime = GetAdjustedTime();
    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(LookupNumeric("0.0.0.0", 0)));
    CAddress addrMe = CAddress(CService(), nLocalServices);
    GetRandBytes({(unsigned char*)&nLocalHostNonce, sizeof(nLocalHostNonce)});
    LogPrint(BCLog::LogFlags::NET, "send version message: version %d, blocks=%d, us=%s, them=%s, peer=%s",
        PROTOCOL_VERSION, nBestHeight, addrMe.ToString(), addrYou.ToString(), addr.ToString());

    //TODO: change `PushMessage()` to use ServiceFlags so we don't need to cast nLocalServices
    PushMessage(
        NetMsgType::ARIES,
        PROTOCOL_VERSION,
        (uint64_t)nLocalServices,
        nTime,
        addrYou,
        addrMe,
        nLocalHostNonce,
        FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>()),
        nBestHeight);
}

bool CNode::Misbehaving(int howmuch)
{
    if (addr.IsLocal())
    {
        LogPrintf("Warning: Local node %s misbehaving (delta: %d)!", addrName, howmuch);
        return false;
    }

    {
        int nMisbehavior = 0;

        LOCK(cs_mapMisbehavior);

        nMisbehavior = GetMisbehavior() + howmuch;

        mapMisbehavior[addr] = std::make_pair(nMisbehavior, GetAdjustedTime());

        if (nMisbehavior >= gArgs.GetArg("-banscore", 100))
        {
            LogPrint(BCLog::LogFlags::NET, "Misbehaving: %s (%d -> %d) DISCONNECTING", addr.ToString(), nMisbehavior-howmuch, nMisbehavior);

            g_banman->Ban(addr, BanReasonNodeMisbehaving);
            CloseSocketDisconnect();
            return true;
        } else
            LogPrint(BCLog::LogFlags::NET, "Misbehaving: %s (%d -> %d)", addr.ToString(), nMisbehavior-howmuch, nMisbehavior);
        return false;
    }
}


int CNode::GetMisbehavior() const
{
    int nMisbehavior = 0;

    LOCK(cs_mapMisbehavior);

    const auto& iMisbehavior = mapMisbehavior.find(addr);

    if (iMisbehavior != mapMisbehavior.end())
    {
        // This expression results in the misbehavior decaying linearly over a 24 hour period at a rate equal to the default banscore.
        // The default banscore is normally 100, but can be changed by specifying -banscore on the command line. At the default setting,
        // This results in a decay of roughly 100/24 = 4 points per hour.
        int time_based_decay_correction = std::round(
                    (double) gArgs.GetArg("-banscore", 100)
                    * (double) std::max((int64_t) 0, GetAdjustedTime() - iMisbehavior->second.second)
                    / (double) gArgs.GetArg("-bantime", DEFAULT_MISBEHAVING_BANTIME)
                    );

        // Make sure nMisbehavior doesn't go below zero.
        nMisbehavior = std::max(0, iMisbehavior->second.first - time_based_decay_correction);

        // Delete entry if nMisbehavior is zero.
        if (!nMisbehavior) mapMisbehavior.erase(iMisbehavior);
    }

    return nMisbehavior;
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
    stats.addrLocal = addrLocal.IsValid() ? addrLocal.ToString() : "";

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



// requires LOCK(cs_vSend)
void SocketSendData(CNode *pnode)
{
    std::deque<SerializeData>::iterator it = pnode->vSendMsg.begin();

    while (it != pnode->vSendMsg.end())
    {
        const SerializeData &data = *it;
        assert(data.size() > pnode->nSendOffset);
        int nBytes = send(pnode->hSocket, (const char*)&data[pnode->nSendOffset], data.size() - pnode->nSendOffset, MSG_NOSIGNAL | MSG_DONTWAIT);
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
                if (pnode->fDisconnect ||
                    (pnode->GetRefCount() <= 0 && pnode->vRecvMsg.empty() && pnode->nSendSize == 0 && pnode->ssSend.empty()))
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
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vSend

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;
        bool have_fds = false;

        for (auto const& hListenSocket : vhListenSocket) {
            FD_SET(hListenSocket, &fdsetRecv);
            hSocketMax = max(hSocketMax, hListenSocket);
            have_fds = true;
        }
        {
            LOCK(cs_vNodes);
            for (auto const& pnode : vNodes)
            {
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend) {
                        // do not read, if draining write queue
                        if (!pnode->vSendMsg.empty())
                            FD_SET(pnode->hSocket, &fdsetSend);
                        else
                            FD_SET(pnode->hSocket, &fdsetRecv);
                        FD_SET(pnode->hSocket, &fdsetError);
                        hSocketMax = max(hSocketMax, pnode->hSocket);
                        have_fds = true;
                    }
                }
            }
        }

        int nSelect = select(have_fds ? hSocketMax + 1 : 0,
                             &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        if (fShutdown)
            return;
        if (nSelect == SOCKET_ERROR)
        {
            if (have_fds)
            {
                int nErr = WSAGetLastError();
                LogPrint(BCLog::LogFlags::NET, "socket select error %d", nErr);
                for (unsigned int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            if (!MilliSleep(timeout.tv_usec/1000)) return;
        }


        //
        // Accept new connections
        //
        for (auto const& hListenSocket : vhListenSocket)
        if (hListenSocket != INVALID_SOCKET && FD_ISSET(hListenSocket, &fdsetRecv))
        {
            struct sockaddr_storage sockaddr;
            socklen_t len = sizeof(sockaddr);
            SOCKET hSocket = accept(hListenSocket, (struct sockaddr*)&sockaddr, &len);
            CAddress addr;
            int nInbound = 0;

            int max_connections = std::min<int>(gArgs.GetArg("-maxconnections", 125), 950);

            if (hSocket != INVALID_SOCKET)
                if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
                    LogPrintf("Warning: Unknown socket family");

            {
                LOCK(cs_vNodes);
                for (auto const& pnode : vNodes)
                    if (pnode->fInbound)
                        nInbound++;
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
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetRecv) || FD_ISSET(pnode->hSocket, &fdsetError))
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
                        int nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
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
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetSend))
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SocketSendData(pnode);
            }

            //
            // Inactivity checking
            //
            // Consider this for future removal as this really is not beneficial nor harmful.
            if ((GetAdjustedTime() - pnode->nTimeConnected) > (60*60*2) && (vNodes.size() > (MAX_OUTBOUND_CONNECTIONS*.75)))
            {
                    LogPrint(BCLog::LogFlags::NET, "Node %s connected longer than 2 hours with connection count of %zd, disconnecting. ",
                             pnode->addr.ToString(), vNodes.size());

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

    int error = 0;
#if MINIUPNPC_API_VERSION < 14
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#else
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, 2, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        if (fDiscover) {
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
            if (fShutdown || !fUseUPnP)
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
            if (fShutdown || !fUseUPnP) return;
            if (!MilliSleep(2000)) return;
        }
    }
}

void MapPort()
{
    if (fUseUPnP && !netThreads->threadExists("ThreadMapPort"))
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

void static ThreadStakeMiner(void* parg)
{
    RenameThread("grc-stakeminer");
    util::ThreadSetInternalName("grc-stakeminer");

    LogPrint(BCLog::LogFlags::NET, "ThreadStakeMiner started");
    CWallet* pwallet = (CWallet*)parg;
    try
    {
        StakeMiner(pwallet);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "ThreadStakeMiner()");
    }
    catch(boost::thread_interrupted&)
    {
        LogPrintf("ThreadStakeMiner exited (interrupt)");
        return;
    }
    catch (...)
    {
        PrintException(nullptr, "ThreadStakeMiner()");
    }
    LogPrintf("ThreadStakeMiner exited");
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
                    if (!ProcessMessages(pnode))
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
                        SendMessages(pnode, pnode == pnodeTrickle);
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

    vhListenSocket.push_back(hListenSocket);

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

void StartNode(void* parg)
{
    // Make this thread recognisable as the startup thread
    RenameThread("grc-nodestart");
    util::ThreadSetInternalName("grc-nodestart");

    fShutdown = false;
    MAX_OUTBOUND_CONNECTIONS = (int) gArgs.GetArg("-maxoutboundconnections", 8);
    int max_connections = std::min<int>(gArgs.GetArg("-maxconnections", 125), 950);
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

    if (!gArgs.GetBoolArg("-dnsseed", true)) {
        LogPrintf("DNS seeding disabled");
    } else if (!netThreads->createThread(ThreadDNSAddressSeed, nullptr, "ThreadDNSAddressSeed")) {
        LogPrintf("Error: createThread(ThreadDNSAddressSeed) failed");
    }
    // Map ports with UPnP
    if (fUseUPnP) {
        MapPort();
    }

    // Send and receive from sockets, accept connections
    if (!netThreads->createThread(ThreadSocketHandler, nullptr, "ThreadSocketHandler")) {
        LogPrintf("Error: createThread(ThreadSocketHandler) failed");
    }

    // Initiate outbound connections from -addnode
    if (!netThreads->createThread(ThreadOpenAddedConnections, nullptr, "ThreadOpenAddedConnections")) {
        LogPrintf("Error: createThread(ThreadOpenAddedConnections) failed");
    }

    // Initiate outbound connections
    if (!netThreads->createThread(ThreadOpenConnections, nullptr, "ThreadOpenConnections")) {
        LogPrintf("Error: createThread(ThreadOpenConnections) failed");
    }

    // Process messages
    if (!netThreads->createThread(ThreadMessageHandler, nullptr, "ThreadMessageHandler")) {
        LogPrintf("Error: createThread(ThreadMessageHandler) failed");
    }

    // Dump network addresses
    if (!netThreads->createThread(ThreadDumpAddress, nullptr, "ThreadDumpAddress")) {
        LogPrintf("Error: createThread(ThreadDumpAddress) failed");
    }

    if (!netThreads->createThread(ThreadStakeMiner, pwalletMain, "ThreadStakeMiner")) {
        LogPrintf("Error: createThread(ThreadStakeMiner) failed");
    }

}

bool StopNode()
{
    LogPrintf("StopNode()");
    fShutdown = true;
    if (semOutbound)
        for (int i=0; i<MAX_OUTBOUND_CONNECTIONS; i++)
            semOutbound->post();

    netThreads->interruptAll();
    netThreads->removeAll();
    UninterruptibleSleep(std::chrono::milliseconds{50});
    DumpAddresses();
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
        // Close sockets
        for (auto const& pnode : vNodes)
            if (pnode->hSocket != INVALID_SOCKET)
                closesocket(pnode->hSocket);
        for (auto &hListenSocket : vhListenSocket)
            if (hListenSocket != INVALID_SOCKET)
                if (closesocket(hListenSocket) == SOCKET_ERROR)
                    LogPrintf("closesocket(hListenSocket) died with error %d", WSAGetLastError());

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
}
instance_of_cnetcleanup;

void RelayTransaction(const CTransaction& tx, const uint256& hash)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, hash, ss);
}

void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss)
{
    CInv inv(MSG_TX, hash);
    {
        LOCK(cs_mapRelay);
        // Expire old relay messages
        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < GetAdjustedTime())
        {
            mapRelay.erase(vRelayExpiration.front().second);
            vRelayExpiration.pop_front();
        }

        // Save original serialized message so newer versions are preserved
        mapRelay.insert(std::make_pair(inv, ss));
        vRelayExpiration.push_back(std::make_pair(GetAdjustedTime() + 15 * 60, inv));
    }

    RelayInventory(inv);
}

