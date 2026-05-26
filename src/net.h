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

#include "netbase.h"
#include "mruset.h"
#include "protocol.h"
#include "streams.h"
#include "addrman.h"

#ifndef WIN32
#include <arpa/inet.h>
#endif

class CNode;
class CBlockIndex;
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
bool RecvLine(SOCKET hSocket, std::string& strLine);
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
void Discover(boost::thread_group& threadGroup);
extern bool fUseUPnP;
extern ServiceFlags nLocalServices;
// Local-host version nonce, randomised on every outgoing VERSION push and
// compared against incoming VERSIONs to detect self-connection. It is a
// global -- written on the socket-handler thread (PushVersion) and read on
// the message-handler thread (ProcessMessage's "connected to ourself"
// check), so it must be atomic; TSan G11 reports the race via a memcpy
// into its raw storage from GetRandBytes. Note: the broader design is
// imperfect (the same global is clobbered for each outbound connection,
// so the per-connection nonce identity is lost), but that is a separate
// follow-up; atomicising here just closes the data race.
extern std::atomic<uint64_t> nLocalHostNonce;
extern CAddress addrSeenByPeer;
extern CAddrMan addrman;
extern CCriticalSection cs_mapRelay;
extern std::map<CInv, CDataStream> mapRelay GUARDED_BY(cs_mapRelay);
extern std::deque<std::pair<int64_t, CInv> > vRelayExpiration GUARDED_BY(cs_mapRelay);
extern std::map<CInv, int64_t> mapAlreadyAskedFor;
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
    SOCKET hSocket;
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

    // Denial-of-service detection/prevention
    // ---------- address:port -- misbehavior - time
    static CCriticalSection cs_mapMisbehavior;
    static std::map<CAddress, std::pair<int, int64_t>> mapMisbehavior GUARDED_BY(cs_mapMisbehavior);
    // See protected GetMisbehavior() below.
    // int nMisbehavior;

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

    CNode(SOCKET hSocketIn, CAddress addrIn, std::string addrNameIn = "", bool fInboundIn=false) : ssSend(SER_NETWORK, INIT_PROTO_VERSION), setAddrKnown(5000)
    {

        nServices = 0;
        hSocket = hSocketIn;
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
        if (hSocket != INVALID_SOCKET && !fInbound)
            PushVersion();
    }

    ~CNode()
    {
        if (hSocket != INVALID_SOCKET)
        {
            closesocket(hSocket);
            hSocket = INVALID_SOCKET;
        }
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

        int64_t& nRequestTime = mapAlreadyAskedFor[inv];
        LogPrint(BCLog::LogFlags::NET, "askfor %s   %" PRId64 " (%s)", inv.ToString(), nRequestTime, DateTimeStrFormat("%H:%M:%S", nRequestTime/1000000));

        // Make sure not to reuse time indexes to keep things in the same order
        int64_t nNow = (GetAdjustedTime() - 1) * 1000000;
        static int64_t nLastTime;
        ++nLastTime;
        nNow = std::max(nNow, nLastTime);
        nLastTime = nNow;

        // Each retry is 2 minutes after the last
        nRequestTime = std::max(nRequestTime + 2 * 60 * 1000000, nNow);
        mapAskFor.insert(std::make_pair(nRequestTime, inv));
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

    static bool DisconnectNode(const std::string& strNode);
    static bool DisconnectNode(const CSubNet& subnet);
    static bool DisconnectNode(const CNetAddr& addr);
    static bool DisconnectNode(NodeId id);

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

    //!
    //! \brief Score misbehavior against an address without requiring a CNode
    //! instance. Operates on the same static mapMisbehavior used by the
    //! instance method, so scores are shared — misbehavior accumulated here
    //! is visible to any CNode with the same address.
    //!
    //! \param addr    The address to score against.
    //! \param howmuch Misbehavior points to add.
    //!
    //! \return \c true if the accumulated score triggered a ban.
    //!
    static bool MisbehavingAddr(const CAddress& addr, int howmuch);

    int GetMisbehavior() const;

    //!
    //! \brief Get the current misbehavior score for an address without
    //! requiring a CNode instance. Applies the same time-based decay as
    //! the instance method.
    //!
    //! \param addr The address to query.
    //!
    //! \return The decayed misbehavior score.
    //!
    static int GetMisbehaviorAddr(const CAddress& addr);

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

    friend class BanMan;

};

// Re-declared here (was forward-declared above CNode) so the
// EXCLUSIVE_LOCKS_REQUIRED lock-expression can reference pnode->cs_vSend.
void SocketSendData(CNode *pnode) EXCLUSIVE_LOCKS_REQUIRED(pnode->cs_vSend);

inline void RelayInventory(const CInv& inv)
{
    // Put on lists to offer to the other nodes
    {
        LOCK(cs_vNodes);
        for (auto const& pnode : vNodes)
            pnode->PushInventory(inv);
    }
}

class CTransaction;
void RelayTransaction(const CTransaction& tx, const uint256& hash);
void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss);


#endif
