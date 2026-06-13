// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include <deque>
#include <array>
#include <boost/thread.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "netbase.h"
#include "mruset.h"
#include "protocol.h"
#include "streams.h"
#include "addrman.h"
#include "util/sock.h"

#ifndef WIN32
#include <arpa/inet.h>
#endif

class CNode;
class CBlockIndex;
class CBlockLocator;
extern CCriticalSection cs_main;
// Duplicate extern of the main.h:82 declaration; both must carry the
// GUARDED_BY annotation so cross-TU readers via net.h see the contract.
extern int nBestHeight GUARDED_BY(cs_main);


/** Time between pings automatically sent out for latency probing and keepalive (in seconds). */
static const int PING_INTERVAL = 2 * 60;
/** Time after which to disconnect, after waiting for a ping response (or inactivity). */
static const int TIMEOUT_INTERVAL = 20 * 60;
extern int MAX_OUTBOUND_CONNECTIONS;
extern int PEER_TIMEOUT;

typedef int64_t NodeId;

inline unsigned int ReceiveFloodSize() { return 1000*gArgs.GetArg("-maxreceivebuffer", 5*1000); }
inline unsigned int SendBufferSize() { return 1000*gArgs.GetArg("-maxsendbuffer", 1*1000); }

void AddOneShot(std::string strDest);
bool GetMyExternalIP(CNetAddr& ipRet);
void AddressCurrentlyConnected(const CService& addr);
CNode* FindNode(const CNetAddr& ip);
CNode* FindNode(const CService& ip);
CNode* ConnectNode(CAddress addrConnect, const char* strDest = nullptr);
void MapPort();
unsigned short GetListenPort();
bool BindListenPort(const CService &bindAddr, std::string& strError=REF(std::string()));
void StartNode(void* parg);
bool StopNode();
// Declared below CNode (the EXCLUSIVE_LOCKS_REQUIRED annotation references
// pnode->cs_vSend and needs the complete CNode type).
void SocketSendData(CNode *pnode);
extern CCriticalSection cs_vNodes;
extern std::vector<CNode*> vNodes GUARDED_BY(cs_vNodes);

struct LocalServiceInfo {
    int nScore;
    int nPort;
};

extern CCriticalSection cs_mapLocalHost;
extern std::map<CNetAddr, LocalServiceInfo> mapLocalHost GUARDED_BY(cs_mapLocalHost);

enum
{
    LOCAL_NONE,   // unknown
    LOCAL_IF,     // address a local interface listens on
    LOCAL_BIND,   // address explicit bound to
    LOCAL_UPNP,   // address reported by UPnP
    LOCAL_HTTP,   // address reported by whatismyip.com and similar
    LOCAL_MANUAL, // address explicitly specified (-externalip=)

    LOCAL_MAX
};

bool IsLimited(enum Network net);
bool IsLimited(const CNetAddr& addr);
bool AddLocal(const CService& addr, int nScore = LOCAL_NONE);
bool AddLocal(const CNetAddr& addr, int nScore = LOCAL_NONE);
bool SeenLocal(const CService& addr);
bool IsLocal(const CService& addr);
bool GetLocal(CService& addr, const CNetAddr* paddrPeer = nullptr);
bool IsReachable(const CNetAddr &addr);
void SetReachable(enum Network net, bool fFlag = false);
CAddress GetLocalAddress(const CNetAddr* paddrPeer = nullptr);
void AdvertiseLocal(CNode *pnode = nullptr);

enum
{
    MSG_TX = 1,
    MSG_BLOCK,
    MSG_PART,
    MSG_SCRAPERINDEX,
};


extern bool fDiscover;
extern bool fUseUPnP;
extern ServiceFlags nLocalServices;
extern AddrMan addrman;
//! \brief Guards \ref mapAlreadyAskedFor. Written and read from
//! ProcessMessage handlers (under cs_main) for the TX / BLOCK paths,
//! from ProcessBlock, from SendMessages' getdata loop, and from
//! CSplitBlob::RecvPart on the scraper PART path which does NOT hold
//! cs_main. A dedicated leaf-level mutex avoids hoisting cs_main into
//! the PART path and keeps the lock as narrow as possible.
extern CCriticalSection cs_mapAlreadyAskedFor;
extern std::map<CInv, int64_t> mapAlreadyAskedFor GUARDED_BY(cs_mapAlreadyAskedFor);
extern ThreadHandler* netThreads;


extern CCriticalSection cs_vAddedNodes;
extern std::vector<std::string> vAddedNodes GUARDED_BY(cs_vAddedNodes);


class CNodeStats
{
public:
    NodeId id;
    uint64_t nServices;
    CAddress addr;
    int64_t nLastSend;
    int64_t nLastRecv;
    uint64_t nSendBytes;
    uint64_t nRecvBytes;
    int64_t nTimeConnected;
    int64_t nTimeOffset;
    std::string addrName;
    int nVersion;
    std::string strSubVer;
    bool fInbound;
    int nStartingHeight;
    int nMisbehavior;
    double dPingTime;
    double dMinPing;
    double dPingWait;
	std::string addrLocal;
	int nTrust;
};




class CNetMessage {
public:
    bool in_data;                   // parsing header (false) or data (true)

    CDataStream hdrbuf;             // partially received header
    CMessageHeader hdr;             // complete header
    unsigned int nHdrPos;

    CDataStream vRecv;              // received message data
    unsigned int nDataPos;

    int64_t nTime;                  // time (in microseconds) of message receipt.

    CNetMessage(int nTypeIn, int nVersionIn) : hdrbuf(nTypeIn, nVersionIn), vRecv(nTypeIn, nVersionIn) {
        hdrbuf.resize(24);
        in_data = false;
        nHdrPos = 0;
        nDataPos = 0;
        nTime = 0;
    }

    bool complete() const
    {
        if (!in_data)
            return false;
        return (hdr.nMessageSize == nDataPos);
    }

    void SetVersion(int nVersionIn)
    {
        hdrbuf.SetVersion(nVersionIn);
        vRecv.SetVersion(nVersionIn);
    }

    int readHeader(const char *pch, unsigned int nBytes);
    int readData(const char *pch, unsigned int nBytes);
};





/** Information about a peer */
class CNode
{
public:
    // socket
    uint64_t nServices;
    // RAII socket wrapper (issue #2558 PR 5a), replacing the raw SOCKET
    // hSocket. The socket-handler thread reads it (to poll/recv/send) while
    // CloseSocketDisconnect may reset it from another thread, so it is guarded;
    // callers take a shared_ptr copy under the lock via GetSock() and operate
    // on that local copy, keeping the fd alive for the duration of a recv/send.
    mutable Mutex m_sock_mutex;
    std::shared_ptr<Sock> m_sock GUARDED_BY(m_sock_mutex);
    CCriticalSection cs_vSend;
    CDataStream ssSend GUARDED_BY(cs_vSend);
    size_t nSendSize GUARDED_BY(cs_vSend); // total size of all vSendMsg entries
    size_t nSendOffset GUARDED_BY(cs_vSend); // offset inside the first vSendMsg already sent
    std::atomic<uint64_t> nSendBytes {0};
    std::deque<SerializeData> vSendMsg GUARDED_BY(cs_vSend);

    CCriticalSection cs_vRecvMsg;
    std::deque<CNetMessage> vRecvMsg GUARDED_BY(cs_vRecvMsg);
    std::atomic<uint64_t> nRecvBytes {0};
    int nRecvVersion GUARDED_BY(cs_vRecvMsg);

    // These three were plain int64_t historically (Bitcoin-Core-inherited).
    // ThreadSocketHandler2's inactivity-check loop reads them racily against
    // the message-handler / connection-accept paths that write them; TSan
    // surfaced races at net.cpp:751,1144,1158,1170,1179,1188 and elsewhere
    // tracking the same addresses. Atomicising matches the pattern already
    // used for nSendBytes / nRecvBytes / nTimeOffset directly above.
    std::atomic<int64_t> nLastSend{0};
    std::atomic<int64_t> nLastRecv{0};
    std::atomic<int64_t> nTimeConnected{0};
    int64_t nNextRebroadcastTime;
    std::atomic<int64_t> nTimeOffset{0};
    CAddress addr;
    std::string addrName;
    // addrLocal is the local address as seen by this peer (sent by them in
    // their VERSION message). It is written once on the message-handler
    // thread that processes that VERSION, and read concurrently by the GUI
    // peers-table refresh (under cs_vNodes) and by net.cpp helpers
    // (GetLocalAddress / IsPeerAddrLocalGood). The pre-existing pattern
    // covered the readers (which hold cs_vNodes) but not the writer (which
    // holds cs_vRecvMsg, a different mutex), surfaced as TSan G4/G5 races
    // in CNetAddr::IsValid via CNode::copyStats.
    //
    // Use the GetAddrLocal()/SetAddrLocal() accessors below rather than
    // touching the field directly.
    mutable CCriticalSection cs_addrLocal;
    CService addrLocal GUARDED_BY(cs_addrLocal);
    int nVersion;
    std::string strSubVer;
	int nTrust;
	////////////////////////


	//Block Flood attack Halford
	int64_t nLastOrphan;
	int nOrphanCount;
	int nOrphanCountViolations;


    bool fOneShot;
    bool fClient;
    bool fInbound;
    // Atomic: written by ThreadOpenConnections2 (OpenNetworkConnection sets true
    // after a successful outbound connection) and read by both
    // ThreadSocketHandler2 (close-side bookkeeping) and the message-handler
    // thread (first-messages "remember this address" path). No common lock --
    // sibling of the same CNode-scalar pattern atomicised for G6-G10.
    std::atomic<bool> fNetworkNode;
    bool fSuccessfullyConnected;
    std::atomic_bool fDisconnect;
    CSemaphoreGrant grantOutbound;
    int nRefCount;
protected:

    // Denial-of-service detection/prevention. The misbehavior score map and its
    // address-keyed accessors moved to net_processing (issue #2558 PR 2c); the
    // thin Misbehaving()/GetMisbehavior() wrappers below forward to them.

public:
    uint256 hashContinue;
    CBlockIndex* pindexLastGetBlocksBegin;
    uint256 hashLastGetBlocksEnd;
    int nStartingHeight;

    // flood relay
    std::vector<CAddress> vAddrToSend;
    mruset<CAddress> setAddrKnown;
    bool fGetAddr;
    std::set<uint256> setKnown;
    uint256 hashCheckpointKnown; // ppcoin: known sent sync-checkpoint

    // inventory based relay
    CCriticalSection cs_inventory;
    mruset<CInv> setInventoryKnown GUARDED_BY(cs_inventory);
    std::vector<CInv> vInventoryToSend GUARDED_BY(cs_inventory);
    std::multimap<int64_t, CInv> mapAskFor;

    // Ping time measurement:
    // The pong reply we're expecting, or 0 if no pong expected. Set on the
    // ping-send path in SendMessages and cleared on the PONG receive path in
    // ProcessMessage; read raw from ThreadSocketHandler2's timeout check. The
    // companion nPingUsecTime / nMinPingUsecTime below were already atomic;
    // atomicising these two closes the matching races (TSan main.cpp:2992 and
    // net.cpp:1186).
    std::atomic<uint64_t> nPingNonceSent{0};
    // Time (in usec) the last ping was sent, or 0 if no ping was ever sent.
    std::atomic<int64_t> nPingUsecStart{0};
    // Last measured round-trip time.
    std::atomic<int64_t> nPingUsecTime{0};
    // Best measured round-trip time.
    std::atomic<int64_t> nMinPingUsecTime{std::numeric_limits<int64_t>::max()};

    // Whether a ping is requested.
    bool fPingQueued;

    CNode(SOCKET hSocketIn, CAddress addrIn, std::string addrNameIn = "", bool fInboundIn=false) : m_sock(std::make_shared<Sock>(hSocketIn)), ssSend(SER_NETWORK, INIT_PROTO_VERSION), setAddrKnown(5000)
    {

        nServices = 0;
        nRecvVersion = INIT_PROTO_VERSION;
        nLastSend = 0;
        nLastRecv = 0;
        nTimeConnected = GetAdjustedTime();
        nNextRebroadcastTime = GetAdjustedTime();
        addr = addrIn;
        addrName = addrNameIn == "" ? addr.ToStringIPPort() : addrNameIn;
        nVersion = 0;
        strSubVer = "";
        fOneShot = false;
        fClient = false; // set by version message
        fInbound = fInboundIn;
        fNetworkNode = false;
        fSuccessfullyConnected = false;
        fDisconnect = false;
        nRefCount = 0;
        nSendSize = 0;
        nSendOffset = 0;
        hashContinue.SetNull();
        pindexLastGetBlocksBegin = 0;
        hashLastGetBlocksEnd.SetNull();
        nStartingHeight = -1;
        fGetAddr = false;
		//Orphan Attack
		nLastOrphan=0;
		nOrphanCount=0;
		nOrphanCountViolations=0;
		nTrust = 0;
        hashCheckpointKnown.SetNull();
        setInventoryKnown.max_size(SendBufferSize() / 1000);
        nPingNonceSent = 0;
        nPingUsecStart = 0;
        fPingQueued = false;

        // Be shy and don't send version until we hear
        if (hSocketIn != INVALID_SOCKET && !fInbound)
            PushVersion();
    }

    ~CNode()
    {
        // m_sock's destructor closes the underlying socket (issue #2558 PR 5a).
    }

private:

    CNode(const CNode&);
    void operator=(const CNode&);

    NodeId GetNewNodeId();

    const NodeId id = GetNewNodeId();
    // In newer Bitcoin, this is in the connection manager class.
    // For us, we will put it here for the time being.
    static std::atomic<NodeId> nLastNodeId;

    // Network usage totals
    static std::atomic<uint64_t> nTotalBytesRecv;
    static std::atomic<uint64_t> nTotalBytesSent;


public:

    NodeId GetId() const {
        return id;
    }

    int GetRefCount()
    {
        assert(nRefCount >= 0);
        return nRefCount;
    }

    unsigned int GetTotalRecvSize() EXCLUSIVE_LOCKS_REQUIRED(cs_vRecvMsg)
    {
        unsigned int total = 0;
        for (auto const& msg : vRecvMsg)
            total += msg.vRecv.size() + 24;
        return total;
    }

    bool ReceiveMsgBytes(const char *pch, unsigned int nBytes) EXCLUSIVE_LOCKS_REQUIRED(cs_vRecvMsg);

    void SetRecvVersion(int nVersionIn) EXCLUSIVE_LOCKS_REQUIRED(cs_vRecvMsg)
    {
        nRecvVersion = nVersionIn;
        for (auto &msg : vRecvMsg)
            msg.SetVersion(nVersionIn);
    }

    CNode* AddRef()
    {
        nRefCount++;
        return this;
    }

    void Release()
    {
        nRefCount--;
    }



    void AddAddressKnown(const CAddress& addr)
    {
        setAddrKnown.insert(addr);
    }

    void PushAddress(const CAddress& addr)
    {
        // Known checking here is only to save space from duplicates.
        // SendMessages will filter it again for knowns that were added
        // after addresses were pushed.
        if (addr.IsValid() && !setAddrKnown.count(addr) && vAddrToSend.size() < 10000)
            vAddrToSend.push_back(addr);
    }


    void AddInventoryKnown(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            setInventoryKnown.insert(inv);
        }
    }

    void PushInventory(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            if (!setInventoryKnown.count(inv) && vInventoryToSend.size() < 10000)
                vInventoryToSend.push_back(inv);
        }
    }

    void AskFor(const CInv& inv)
    {
        // We're using mapAskFor as a priority queue,
        // the key is the earliest time the request can be sent
        if (mapAskFor.size() > 50000) return;

        // Snapshot the current request time under the mutex. The
        // logging / time-formatting / static-counter arithmetic below
        // does not touch mapAlreadyAskedFor and should not hold the
        // global mutex.
        int64_t nRequestTime;
        {
            LOCK(cs_mapAlreadyAskedFor);
            nRequestTime = mapAlreadyAskedFor[inv];
        }

        LogPrint(BCLog::LogFlags::NET, "askfor %s   %" PRId64 " (%s)", inv.ToString(), nRequestTime, DateTimeStrFormat("%H:%M:%S", nRequestTime/1000000));

        // Make sure not to reuse time indexes to keep things in the same order
        int64_t nNow = (GetAdjustedTime() - 1) * 1000000;
        static int64_t nLastTime;
        ++nLastTime;
        nNow = std::max(nNow, nLastTime);
        nLastTime = nNow;

        // Each retry is 2 minutes after the last
        const int64_t nNewRequestTime = std::max(nRequestTime + 2 * 60 * 1000000, nNow);
        {
            LOCK(cs_mapAlreadyAskedFor);
            mapAlreadyAskedFor[inv] = nNewRequestTime;
        }
        mapAskFor.insert(std::make_pair(nNewRequestTime, inv));
    }

    void BeginMessage(const char* pszCommand) EXCLUSIVE_LOCKS_REQUIRED(cs_vSend)
    {
        assert(ssSend.size() == 0);
        ssSend << CMessageHeader(pszCommand, 0);
    }

    void AbortMessage() EXCLUSIVE_LOCKS_REQUIRED(cs_vSend)
    {
        ssSend.clear();

        LogPrint(BCLog::LogFlags::NOISY, "(aborted)");
    }

    void EndMessage() EXCLUSIVE_LOCKS_REQUIRED(cs_vSend)
    {
        if (ssSend.size() == 0)
            return;

        // Set the size
        unsigned int nSize = ssSend.size() - CMessageHeader::HEADER_SIZE;
        memcpy((char*)&ssSend[CMessageHeader::MESSAGE_SIZE_OFFSET], &nSize, sizeof(nSize));

        // Set the checksum
        uint256 hash = Hash(Span{ssSend}.subspan(CMessageHeader::HEADER_SIZE));
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        assert(ssSend.size () >= CMessageHeader::CHECKSUM_OFFSET + sizeof(nChecksum));
        memcpy((char*)&ssSend[CMessageHeader::CHECKSUM_OFFSET], &nChecksum, sizeof(nChecksum));

        LogPrint(BCLog::LogFlags::NOISY, "(%d bytes)", nSize);

        std::deque<SerializeData>::iterator it = vSendMsg.insert(vSendMsg.end(), SerializeData());
        it->insert(it->end(), ssSend.begin(), ssSend.end());
        ssSend.clear();
        nSendSize += it->size();

        // If write queue empty, attempt "optimistic write"
        if (it == vSendMsg.begin())
            SocketSendData(this);
    }

    void PushVersion();

    template<typename T>
    void PushFields(T field) EXCLUSIVE_LOCKS_REQUIRED(cs_vSend)
    {
        ssSend << field;
    }

    template<typename T, typename... Tfields>
    void PushFields(T field, Tfields... fields) EXCLUSIVE_LOCKS_REQUIRED(cs_vSend)
    {
        ssSend << field;
        PushFields(fields...);
    }

    void PushMessage(const char* pszCommand)
    {
        LOCK(cs_vSend);

        try
        {
            BeginMessage(pszCommand);
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    template<typename... Args>
    void PushMessage(const char* pszCommand, Args... args)
    {
        LOCK(cs_vSend);

        try
        {
            BeginMessage(pszCommand);
            PushFields(args...);
            EndMessage();
        }
        catch (...)
        {
            AbortMessage();
            throw;
        }
    }

    void PushGetBlocks(CBlockIndex* pindexBegin, uint256 hashEnd);
    void CloseSocketDisconnect();

    //! Thread-safe accessor for the socket (issue #2558 PR 5a). Returns a
    //! shared_ptr copy (possibly null, after disconnect). Hold the returned
    //! copy across a send/recv so the fd cannot be closed underneath you.
    std::shared_ptr<Sock> GetSock() const LOCKS_EXCLUDED(m_sock_mutex)
    {
        LOCK(m_sock_mutex);
        return m_sock;
    }

    //! Close the underlying socket immediately. Used by the shutdown-time
    //! CNetCleanup sweep; the socket-handler path uses CloseSocketDisconnect.
    void CloseSocket() LOCKS_EXCLUDED(m_sock_mutex)
    {
        LOCK(m_sock_mutex);
        m_sock.reset();
    }

    // Denial-of-service detection/prevention
    // The idea is to detect peers that are behaving
    // badly and disconnect/ban them, but do it in a
    // one-coding-mistake-won't-shatter-the-entire-network
    // way.
    // IMPORTANT:  There should be nothing I can give a
    // node that it will forward on that will make that
    // node's peers drop it. If there is, an attacker
    // can isolate a node and/or try to split the network.
    // Dropping a node for sending stuff that is invalid
    // now but might be valid in a later version is also
    // dangerous, because it can cause a network split
    // between nodes running old code and nodes running
    // new code.
    // static void ClearBanned(); // needed for unit testing
    // static bool IsBanned(CNetAddr ip);
    bool Misbehaving(int howmuch); // 1 == a little, 100 == a lot

    int GetMisbehavior() const;

    // Thread-safe accessors for addrLocal. See the comment on the field
    // above for the locking rationale.
    CService GetAddrLocal() const LOCKS_EXCLUDED(cs_addrLocal);
    void SetAddrLocal(const CService& addrLocalIn) LOCKS_EXCLUDED(cs_addrLocal);

    void copyStats(CNodeStats &stats);

    static void CopyNodeStats(std::vector<CNodeStats>& vstats);

	// Network stats
    static void RecordBytesRecv(uint64_t bytes);
    static void RecordBytesSent(uint64_t bytes);

    static uint64_t GetTotalBytesRecv();
    static uint64_t GetTotalBytesSent();

};

// Re-declared here (was forward-declared above CNode) so the
// EXCLUSIVE_LOCKS_REQUIRED lock-expression can reference pnode->cs_vSend.
void SocketSendData(CNode *pnode) EXCLUSIVE_LOCKS_REQUIRED(pnode->cs_vSend);

//! Interface for message-processing callbacks driven by the connection manager
//! (issue #2558 PR 8a). PeerManagerImpl implements it; CConnman drives it via
//! Options::m_msgproc in PR 8c. Kept minimal -- just the per-node message pump
//! that ThreadMessageHandler needs.
class NetEventsInterface
{
public:
    //! Process the next message from pfrom's receive queue. Returns false if the
    //! node should be disconnected.
    virtual bool ProcessMessages(CNode* pfrom) EXCLUSIVE_LOCKS_REQUIRED(pfrom->cs_vRecvMsg) = 0;

    //! Send queued messages / generate periodic ones for pto.
    virtual bool SendMessages(CNode* pto, bool fSendTrickle) = 0;

protected:
    //! Instances are owned and deleted through the concrete type (PeerManager),
    //! never through this interface.
    ~NetEventsInterface() = default;
};

//! Connection manager. PR 3 (issue #2558) introduces the lifecycle skeleton:
//! it takes over StartNode/StopNode -- now thin thread-entry forwarders -- via
//! Start()/Interrupt()/Stop(). For now it wraps the still-global connection
//! state (vNodes, addrman, netThreads, ...); the node-access API
//! (GetNodeCount/GetNodeStats in PR 9a; ForEachNode/DisconnectNode in PR 9b)
//! reads/operates over it, storage ownership moves in a later PR, and Options
//! gains m_msgproc with PeerManager in PR 8.
class CConnman
{
public:
    struct Options
    {
        int nMaxConnections = 0;
        int nMaxOutbound = 0;
        //! Message processor the connection manager drives (issue #2558 PR 8c).
        //! Set to g_peerman in AppInit2; null in contexts that never pump
        //! messages (e.g. the test fixture).
        NetEventsInterface* m_msgproc = nullptr;
    };

    CConnman(uint64_t seed0, uint64_t seed1, AddrMan& addrman, bool network_active = true);
    ~CConnman();

    void Init(const Options& opts) { m_options = opts; }
    bool Start();

    //! The message processor (NetEventsInterface) the net threads drive, or
    //! null if none is configured (issue #2558 PR 8c).
    NetEventsInterface* GetMessageProcessor() const { return m_options.m_msgproc; }

    //! Node-access API (issue #2558 PR 9a). Read-only views over the connection
    //! set so external callers stop touching the vNodes/cs_vNodes globals
    //! directly. Each method takes cs_vNodes internally. Backed by the still-
    //! global vNodes for now; storage ownership moves into CConnman in a later PR.
    enum NumConnections {
        CONNECTIONS_NONE = 0,
        CONNECTIONS_IN   = (1U << 0),
        CONNECTIONS_OUT  = (1U << 1),
        CONNECTIONS_ALL  = (CONNECTIONS_IN | CONNECTIONS_OUT),
    };
    size_t GetNodeCount(NumConnections flags) const;
    void GetNodeStats(std::vector<CNodeStats>& vstats) const;

    //! Invoke func for every current node under cs_vNodes (issue #2558 PR 9b).
    //! Iterates all connected nodes (no fDisconnect filter), matching the
    //! direct vNodes loops it replaces.
    void ForEachNode(const std::function<void(CNode*)>& func);

    //! Flag the matching node(s) for disconnection (issue #2558 PR 9b; moved
    //! off CNode's static helpers). Each takes cs_vNodes internally.
    bool DisconnectNode(const std::string& strNode);
    bool DisconnectNode(const CSubNet& subnet);
    bool DisconnectNode(const CNetAddr& addr);
    bool DisconnectNode(NodeId id);

    //! Persistent added-node ("addnode add/remove/getaddednodeinfo") list
    //! (issue #2558 PR 9b2). Backed by the still-global vAddedNodes. AddNode
    //! returns false if already present; RemoveAddedNode false if not present.
    bool AddNode(const std::string& strNode);
    bool RemoveAddedNode(const std::string& strNode);
    std::vector<std::string> GetAddedNodes() const;

    //! Like ForEachNode, but for callers that already hold cs_main and whose
    //! callback reads cs_main-guarded state (issue #2558 PR 9c). Takes cs_vNodes
    //! internally, preserving the canonical cs_main -> cs_vNodes order.
    void ForEachNodeUnderLock(const std::function<void(CNode*)>& func) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Relay an inventory item to every node (issue #2558 PR 9c; replaces the
    //! free RelayInventory shim).
    void RelayInventory(const CInv& inv);

    //! Relay an address to a deterministic, limited subset of nodes (issue #2558
    //! PR 9c; moved from net_processing's ADDR handler).
    void RelayAddress(const CAddress& addr, bool fReachable);

    //! Local-host version nonce for self-connection detection (issue #2558
    //! PR 9d; moved off the net global). Set on each outgoing VERSION push,
    //! compared against incoming VERSIONs.
    uint64_t GetLocalHostNonce() const { return m_local_host_nonce; }
    void SetLocalHostNonce(uint64_t nonce) { m_local_host_nonce = nonce; }

    //! The address a peer last reported seeing us at (issue #2558 PR 9d; moved
    //! off the net global). Surfaced by the getinfo RPCs.
    CAddress GetAddrSeenByPeer() const;
    void SetAddrSeenByPeer(const CAddress& addr);

    //! Shared cache of the GETBLOCKS locator (issue #2558 PR 9d2; was a net.cpp
    //! global). Building a locator scans the chain, so the last one is reused
    //! when the same begin index is requested again. Returns a reference into
    //! the cache (callers serialize it immediately on the message thread).
    const CBlockLocator& GetBlockLocator(const CBlockIndex* pindexBegin);

    void Interrupt();
    void Stop();

private:
    AddrMan& m_addrman;
    const uint64_t nSeed0, nSeed1;
    std::atomic<bool> fNetworkActive;
    Options m_options;
    std::vector<std::thread> m_net_threads;

    //! Local-host version nonce (issue #2558 PR 9d). Atomic: written on the
    //! socket-handler thread (PushVersion), read on the message-handler thread
    //! (self-connection check).
    std::atomic<uint64_t> m_local_host_nonce{0};
    //! The address a peer reports seeing us at (issue #2558 PR 9d). CAddress
    //! copy is not atomic, hence the mutex.
    mutable CCriticalSection m_addr_seen_by_peer_cs;
    CAddress m_addr_seen_by_peer GUARDED_BY(m_addr_seen_by_peer_cs) = CAddress(LookupNumeric("0.0.0.0", 0), nLocalServices);

    //! GETBLOCKS locator cache (issue #2558 PR 9d2). unique_ptr so net.h needs
    //! only a forward declaration of CBlockLocator (its full definition lives in
    //! main.h, which net.h must not include).
    const CBlockIndex* m_getblocks_pindex_begin = nullptr;
    std::unique_ptr<CBlockLocator> m_getblocks_locator;
};

extern std::unique_ptr<CConnman> g_connman;

class CTransaction;


#endif
