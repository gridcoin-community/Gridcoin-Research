// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "net_processing.h"

#include "amount.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "consensus/tx_verify.h"
#include "gridcoin/voting/registry.h"
#include "util.h"
#include "net.h"
#include "streams.h"
#include "alert.h"
#include "banman.h"
#include "checkpoints.h"
#include "txdb.h"
#include "init.h"
#include "node/ui_interface.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/gridcoin.h"
#include "gridcoin/mrc.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/scraper/scraper_net.h"
#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/exceptions.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/reward.h"
#include "gridcoin/staking/spam.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/tally.h"
#include "gridcoin/tx_message.h"
#include "node/blockstorage.h"
#include "node/coherence.h"
#include "node/orphan_blocks.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "random.h"
#include "validation.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <ctime>
#include <math.h>
#include "wallet/wallet.h"

using namespace std;

// mapAlerts / cs_mapAlerts are defined in alert.cpp and reached via the
// same extern idiom rpc/net.cpp uses.
extern CCriticalSection cs_mapAlerts;
extern std::map<uint256, CAlert> mapAlerts GUARDED_BY(cs_mapAlerts);

// cPeerBlockCounts stays in main.cpp (read by GetNumBlocksOfPeers); the moved
// VERSION handler feeds it peer-claimed heights, so declare it here.
extern CMedianFilter<int> cPeerBlockCounts GUARDED_BY(cs_main);

// Relay-message cache. Both accessors -- RelayTransaction (below) and the
// getdata loop in SendMessages -- now live in this TU, so the map is file-local.
static CCriticalSection cs_mapRelay;
static map<CInv, CDataStream> mapRelay GUARDED_BY(cs_mapRelay);
static deque<pair<int64_t, CInv> > vRelayExpiration GUARDED_BY(cs_mapRelay);

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

//////////////////////////////////////////////////////////////////////////////
//
// Peer misbehavior tracking (moved from CNode, issue #2558 PR 2c)
//

// Per-address misbehavior scores. State and the address-keyed accessors live
// here; CNode keeps thin Misbehaving()/GetMisbehavior() wrappers that forward
// to these (the instance Misbehaving() additionally disconnects the node).
static CCriticalSection cs_mapMisbehavior;
static std::map<CAddress, std::pair<int, int64_t>> mapMisbehavior GUARDED_BY(cs_mapMisbehavior);

int GetMisbehaviorAddr(const CAddress& addr)
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

bool MisbehavingAddr(const CAddress& addr, int howmuch)
{
    if (addr.IsLocal())
    {
        LogPrintf("Warning: Local address %s misbehaving (delta: %d)!", addr.ToString(), howmuch);
        return false;
    }

    LOCK(cs_mapMisbehavior);

    int nMisbehavior = GetMisbehaviorAddr(addr) + howmuch;

    mapMisbehavior[addr] = std::make_pair(nMisbehavior, GetAdjustedTime());

    if (nMisbehavior >= gArgs.GetArg("-banscore", 100))
    {
        LogPrint(BCLog::LogFlags::NET, "MisbehavingAddr: %s (%d -> %d) BANNING", addr.ToString(), nMisbehavior - howmuch, nMisbehavior);

        g_banman->Ban(addr, BanReasonNodeMisbehaving);
        return true;
    }

    LogPrint(BCLog::LogFlags::NET, "MisbehavingAddr: %s (%d -> %d)", addr.ToString(), nMisbehavior - howmuch, nMisbehavior);
    return false;
}

// Clear all misbehavior entries whose address matches sub_net. Registered with
// BanMan as its misbehavior-clear callback so a lifted ban also resets scores,
// without BanMan reaching into this map directly. Returns the count cleared.
unsigned int ClearMisbehaviorForSubnet(const CSubNet& sub_net)
{
    unsigned int nZeroed = 0;

    LOCK(cs_mapMisbehavior);

    for (auto iMisbehavior = mapMisbehavior.begin(); iMisbehavior != mapMisbehavior.end();)
    {
        if (sub_net.Match(iMisbehavior->first))
        {
            iMisbehavior = mapMisbehavior.erase(iMisbehavior);
            ++nZeroed;
        }
        else
        {
            ++iMisbehavior;
        }
    }

    return nZeroed;
}

// Orphan transaction storage. All accesses occur under cs_main from
// ProcessMessage / AddOrphanTx / EraseOrphanTx / LimitOrphanTxSize.
map<uint256, CTransaction> mapOrphanTransactions GUARDED_BY(cs_main);
map<uint256, set<uint256> > mapOrphanTransactionsByPrev GUARDED_BY(cs_main);

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:

    size_t nSize = GetSerializeSize(tx, SER_NETWORK, CTransaction::CURRENT_VERSION);

    if (nSize > 5000)
    {
        LogPrint(BCLog::LogFlags::MEMPOOL, "ignoring large orphan tx (size: %" PRIszu ", hash: %s)", nSize, hash.ToString().substr(0,10));
        return false;
    }

    mapOrphanTransactions[hash] = tx;
    for (auto const& txin : tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    LogPrint(BCLog::LogFlags::MEMPOOL, "stored orphan tx %s (mapsz %" PRIszu ")", hash.ToString().substr(0,10), mapOrphanTransactions.size());
    return true;
}

void static EraseOrphanTx(uint256 hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CTransaction& tx = mapOrphanTransactions[hash];
    for (auto const& txin : tx.vin)
    {
        mapOrphanTransactionsByPrev[txin.prevout.hash].erase(hash);
        if (mapOrphanTransactionsByPrev[txin.prevout.hash].empty())
            mapOrphanTransactionsByPrev.erase(txin.prevout.hash);
    }
    mapOrphanTransactions.erase(hash);
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans)
    {
        // Evict a random orphan:
        uint256 randomhash = GetRandHash();
        map<uint256, CTransaction>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
    EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered)
{
    for (auto const& pwallet : setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx,wtx))
            return true;
    return false;
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
    EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->Inventory(hash);
}

bool static AlreadyHave(CTxDB& txdb, const CInv& inv) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    switch (inv.type)
    {
    case MSG_TX:
        {
        bool txInMap = false;
        txInMap = mempool.exists(inv.hash);
        return txInMap ||
               mapOrphanTransactions.count(inv.hash) ||
               txdb.ContainsTx(inv.hash);
        }

    case MSG_BLOCK:
        return mapBlockIndex.count(inv.hash) ||
               g_orphan_blocks.Contains(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}


bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv, int64_t nTimeReceived)
{
    LogPrint(BCLog::LogFlags::NOISY, "received: %s from %s (%" PRIszu " bytes)", strCommand, pfrom->addrName, vRecv.size());

    if (strCommand == NetMsgType::ARIES || strCommand == NetMsgType::VERSION)
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->Misbehaving(10);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;

        vRecv >> pfrom->nVersion;

        // In the version following 180324 (mandatory v5.0.0 - Fern), we can finally
        // drop the garbage legacy fields added to the version message:
        //
        if (pfrom->nVersion <= 180324) {
            std::string legacy_dummy;
            vRecv >> legacy_dummy      // pfrom->boinchashnonce
                  >> legacy_dummy      // pfrom->boinchashpw
                  >> legacy_dummy      // pfrom->cpid
                  >> legacy_dummy      // pfrom->enccpid
                  >> legacy_dummy;     // acid
        }

        vRecv >> pfrom->nServices >> nTime >> addrMe;

        LogPrint(BCLog::LogFlags::NOISY, "received aries version %i ...", pfrom->nVersion);

        int64_t timedrift = std::abs(GetAdjustedTime() - nTime);

        if (timedrift > (8*60))
        {
            LogPrint(BCLog::LogFlags::NOISY, "Disconnecting unauthorized peer with Network Time so far off by %" PRId64 " seconds!", timedrift);
            pfrom->Misbehaving(100);
            pfrom->fDisconnect = true;
            return false;
        }

        // Disconnect peers on old protocol versions after a grace period past the
        // BlockV14Height activation height. Skip entirely if that fork is not yet
        // activated (height == INT_MAX) to avoid signed integer overflow UB in the addition.
        const int nLocalBestHeight = WITH_LOCK(cs_main, return pindexBest ? pindexBest->nHeight : 0);
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION
            || (DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD
                && pfrom->nVersion < PROTOCOL_VERSION
                && Params().GetConsensus().BlockV14Height != std::numeric_limits<int>::max()
                && nLocalBestHeight > Params().GetConsensus().BlockV14Height
                                             + Params().GetConsensus().ProtocolVersionGracePeriod
                )
            ) {
            // disconnect from peers older than this proto version
            LogPrint(BCLog::LogFlags::NOISY, "partner %s using obsolete version %i; disconnecting",
                     pfrom->addr.ToString(), pfrom->nVersion);

            pfrom->fDisconnect = true;
            return false;
        }

        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;

        if (!vRecv.empty()) {
            vRecv >> pfrom->strSubVer;

            if (pfrom->strSubVer.size() > 256) {
                pfrom->strSubVer.resize(256);
            }

            // This handles the special disconnect for clients between the mandatory 5.4.0.0 and the 5.4.5.0 since
            // 5.4.6.0 effectively became a mandatory due to the contract version error in TxMessage. The protocol version
            // was not incremented since 5.4.6.0 was originally a leisure and so this is the only reasonable way to distinguish
            // in this situation.
            if (pfrom->strSubVer.find("5.4.5") != std::string::npos
                || pfrom->strSubVer.find("5.4.4") != std::string::npos
                || pfrom->strSubVer.find("5.4.3") != std::string::npos
                || pfrom->strSubVer.find("5.4.2") != std::string::npos
                || pfrom->strSubVer.find("5.4.1") != std::string::npos
                || pfrom->strSubVer.find("5.4.0") != std::string::npos
                ) {

                pfrom->fDisconnect = true;
                return false;
            }
        }

        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;

        // 12-5-2015 - Append Trust fields
        pfrom->nTrust = 0;

        // Allow newbies to connect easily with 0 blocks
        if (gArgs.GetArg("-autoban", "true") == "true")
        {

                // Note: Hacking attempts start in this area

                if (pfrom->nStartingHeight < 1 && pfrom->nServices == 0 )
                {
                    pfrom->Misbehaving(100);
                    LogPrint(BCLog::LogFlags::NET, "Disconnecting possible hacker node with no services.  Banned for 24 hours.");
                    pfrom->fDisconnect=true;
                    return false;
                }
        }



        pfrom->SetAddrLocal(addrMe);
        if (pfrom->fInbound && addrMe.IsRoutable())
        {
            SeenLocal(addrMe);
        }

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {
            LogPrint(BCLog::LogFlags::NET, "connected to self at %s, disconnecting", pfrom->addr.ToString());
            pfrom->fDisconnect = true;
            return true;
        }

        // record my external IP reported by peer
        if (addrMe.IsRoutable()) {
            LOCK(cs_addrSeenByPeer);
            addrSeenByPeer = addrMe;
        }

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        // Moved the below from AddTimeData to here to follow bitcoin's approach.
        int64_t nOffsetSample = nTime - GetTime();
        pfrom->nTimeOffset = nOffsetSample;
        if (!pfrom->fInbound && gArgs.GetBoolArg("-synctime", true))
            AddTimeData(pfrom->addr, nOffsetSample);

        // Change version
        pfrom->PushMessage(NetMsgType::VERACK);
        {
            LOCK(pfrom->cs_vSend);
            pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
        }


        if (!pfrom->fInbound)
        {
            // Advertise our address
            if (!fNoListen && !IsInitialBlockDownload())
            {
                AdvertiseLocal(pfrom);
            }

            // Get recent addresses
            pfrom->PushMessage(NetMsgType::GETADDR);
            pfrom->fGetAddr = true;
            addrman.Good(pfrom->addr);
        }


        // Ask the first connected node for block updates
        static int nAskedForBlocks = 0;
        size_t numNodes;
        {
            LOCK(cs_vNodes);
            numNodes = vNodes.size();
        }
        {
            LOCK(cs_main);
            if (!pfrom->fClient && !pfrom->fOneShot &&
                (pfrom->nStartingHeight > (nBestHeight - 144)) &&
                 (nAskedForBlocks < 1 || (numNodes <= 1 && nAskedForBlocks < 1)))
            {
                nAskedForBlocks++;
                pfrom->PushGetBlocks(pindexBest, uint256());
                LogPrint(BCLog::LogFlags::NET, "Asked For blocks.");
            }
            // cPeerBlockCounts is read by GetNumBlocksOfPeers / IsInitialBlockDownload
            // under cs_main, so the input also belongs under cs_main.
            cPeerBlockCounts.input(pfrom->nStartingHeight);
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            for (auto const& item : mapAlerts)
                item.second.RelayTo(pfrom);
        }

        /* Notify the peer about statsscraper blobs we have */
        LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

        CScraperManifest::PushInvTo(pfrom);

        pfrom->fSuccessfullyConnected = true;

        LogPrint(BCLog::LogFlags::NOISY, "receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s", pfrom->nVersion,
            pfrom->nStartingHeight, addrMe.ToString(), addrFrom.ToString(), pfrom->addr.ToString());
    }
    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else 1-10-2015 Halford
        LogPrintf("Hack attempt from %s - %s (banned) ", pfrom->addrName, pfrom->addr.ToString());
        pfrom->Misbehaving(100);
        pfrom->fDisconnect=true;
        return false;
    }
    else if (strCommand == NetMsgType::VERACK)
    {
        LOCK(pfrom->cs_vRecvMsg);
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
    }
    else if (strCommand == NetMsgType::GRIDADDR || strCommand == NetMsgType::ADDR)
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        if (vAddr.size() > 1000)
        {
            pfrom->Misbehaving(10);
            return error("message addr size() = %" PRIszu "", vAddr.size());
        }

        // Don't store the node address unless they have block height > 50%
        if (pfrom->nStartingHeight < (WITH_LOCK(cs_main, return nBestHeight) * .5)) return true;

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        for (auto &addr : vAddr)
        {
            if (fShutdown)
                return true;
            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);

            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
            {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static arith_uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = UintToArith256(GetRandHash());
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = ArithToUint256(hashSalt ^ (hashAddr<<32) ^ (( GetAdjustedTime() +hashAddr)/(24*60*60)));
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
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }

    else if (strCommand == NetMsgType::INV)
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(50);
            return error("message inv size() = %" PRIszu "", vInv.size());
        }

        // find last block in inv vector
        unsigned int nLastBlock = (unsigned int)(-1);
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK) {
                nLastBlock = vInv.size() - 1 - nInv;
                break;
            }
        }

        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            if (fShutdown) return true;

            // cs_main lock here must be tightly scoped and not be concatenated outside the cs_mapManifest lock, because
            // that will lead to a deadlock. In the original position above the for loop, cs_main is taken first here, then
            // cs_mapManifest below, while in the scraper thread, ScraperCullAndBinCScraperManifests() first locks
            // cs_mapManifest, then calls ScraperDeleteUnauthorizedCScraperManifests(), which calls IsManifestAuthorized(),
            // which locks cs_main to read the AppCacheSection for authorized scrapers.
            bool fAlreadyHave;
            {
                LOCK(cs_main);
                CTxDB txdb("r");

                pfrom->AddInventoryKnown(inv);
                fAlreadyHave = AlreadyHave(txdb, inv);
            }

            // Check also the scraper data propagation system to see if it needs
            // this inventory object:
            if (fAlreadyHave)
            {
                LOCK(CScraperManifest::cs_mapManifest);
                fAlreadyHave = CScraperManifest::AlreadyHave(pfrom, inv);
            }

            LogPrint(BCLog::LogFlags::NOISY, " got inventory: %s  %s", inv.ToString(), fAlreadyHave ? "have" : "new");

            // Relock cs_main after getting done with the CScraperManifest::AlreadyHave.
            {
                LOCK(cs_main);

                if (!fAlreadyHave)
                    pfrom->AskFor(inv);
                else if (inv.type == MSG_BLOCK && g_orphan_blocks.Contains(inv.hash)) {
                    const CBlock* pblock_root = g_orphan_blocks.GetRootBlock(inv.hash);
                    if (pblock_root) {
                        pfrom->PushGetBlocks(pindexBest, pblock_root->GetHash(true));
                    }
                } else if (nInv == nLastBlock) {
                    // In case we are on a very long side-chain, it is possible that we already have
                    // the last block in an inv bundle sent in response to getblocks. Try to detect
                    // this situation and push another getblocks to continue.
                    pfrom->PushGetBlocks(mapBlockIndex[inv.hash], uint256());
                    LogPrint(BCLog::LogFlags::NOISY, "force getblock request: %s", inv.ToString());
                }

                // Track requests for our stuff
                {
                    LOCK(cs_setpwalletRegistered);
                    Inventory(inv.hash);
                }

            }
        }
    }


    else if (strCommand == NetMsgType::GETDATA)
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(10);
            return error("message getdata size() = %" PRIszu "", vInv.size());
        }

        if (vInv.size() != 1)
        {
            LogPrint(BCLog::LogFlags::NET, "received getdata (%" PRIszu " invsz)", vInv.size());
        }

        LOCK(cs_main);
        for (auto const& inv : vInv)
        {
            if (fShutdown)
                return true;
            if (vInv.size() == 1)
            {
              LogPrint(BCLog::LogFlags::NET, "received getdata for: %s", inv.ToString());
            }

            if (inv.type == MSG_BLOCK)
            {
                // Send block from disk
                BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    CBlock block;
                    ReadBlockFromDisk(block, mi->second, Params().GetConsensus());

                    pfrom->PushMessage(NetMsgType::ENCRYPT, block);

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue)
                    {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, hashBestChain));
                        pfrom->PushMessage(NetMsgType::INV, vInv);
                        pfrom->hashContinue.SetNull();
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), mi->second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                    CTransaction tx;
                    if (mempool.lookup(inv.hash, tx)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage(NetMsgType::TX, ss);
                    }
                }
                else if(!pushed && inv.type == MSG_PART) {
                    LOCK(CSplitBlob::cs_mapParts);

                    CSplitBlob::SendPartTo(pfrom, inv.hash);
                }
                else if(!pushed &&  inv.type == MSG_SCRAPERINDEX)
                {
                    LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

                    // Do not send manifests while out of sync.
                    if (!OutOfSyncByAge())
                    {
                        // Do not send unauthorized manifests. This check needs to be done here, because in the
                        // case of a scraper deauthorization, a request from another node to forward the manifest
                        // may come before the housekeeping loop has a chance to do the periodic culling. This could
                        // result in unnecessary node banscore. This will suppress "this" node from sending any
                        // unauthorized manifests.

                        auto iter = CScraperManifest::mapManifest.find(inv.hash);
                        if (iter != CScraperManifest::mapManifest.end())
                        {
                            CScraperManifest_shared_ptr manifest = iter->second;

                            // We are not going to do anything with the banscore here, because this is the sending node,
                            // but it is an out parameter of IsManifestAuthorized.
                            unsigned int banscore_out = 0;

                            // We have to copy out the nTime and pubkey from the selected manifest, because the
                            // IsManifestAuthorized call chain traverses the map and locks the cs_manifests in turn,
                            // which creates a deadlock potential if the cs_manifest lock is already held on one of
                            // the manifests.
                            int64_t nTime = 0;
                            CPubKey pubkey;
                            {
                                LOCK(manifest->cs_manifest);

                                nTime = manifest->nTime;
                                pubkey = manifest->pubkey;
                            }

                            // Also don't send a manifest that is not current.
                            if (CScraperManifest::IsManifestAuthorized(nTime, pubkey, banscore_out)
                                    && WITH_LOCK(manifest->cs_manifest, return manifest->IsManifestCurrent()))
                            {
                                // SendManifestTo takes its own lock on the manifest. Note that the original form of
                                // SendManifestTo took the inv.hash and did another lookup to find the actual
                                // manifest in the mapManifest. This is unnecessary since we already have the manifest
                                // identified above. The new form, which takes a smart shared pointer to the manifest
                                // as an argument, sends the manifest directly using PushMessage, and avoids another
                                // map find.
                                CScraperManifest::SendManifestTo(pfrom, manifest);
                            }
                        }
                    }
                }
            }

            // Track requests for our stuff
            {
                LOCK(cs_setpwalletRegistered);
                Inventory(inv.hash);
            }
        }
    }

    else if (strCommand == NetMsgType::GETBLOCKS)
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = locator.GetBlockIndex();

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        int nLimit = 500;

        LogPrint(BCLog::LogFlags::NET, "getblocks %d to %s limit %d", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20), nLimit);
        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                LogPrint(BCLog::LogFlags::NET, "getblocks stopping at %d %s", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20));
                // ppcoin: tell downloading node about the latest block if it's
                // without risk being rejected due to stake connection check
                if (hashStop != hashBestChain && pindex->GetBlockTime() + nStakeMinAge > pindexBest->GetBlockTime())
                    pfrom->PushInventory(CInv(MSG_BLOCK, hashBestChain));
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0)
            {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                LogPrint(BCLog::LogFlags::NET, "getblocks stopping at limit %d %s", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20));
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }
    else if (strCommand == NetMsgType::GETHEADERS)
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        CBlockIndex* pindex = nullptr;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            BlockMap::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = mi->second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = locator.GetBlockIndex();
            if (pindex)
                pindex = pindex->pnext;
        }

        vector<CBlockHeader> vHeaders;
        int nLimit = 1000;
        LogPrintf("getheaders %d to %s", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20));
        for (; pindex; pindex = pindex->pnext)
        {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        pfrom->PushMessage(NetMsgType::HEADERS, vHeaders);
    }
    else if (strCommand == NetMsgType::TX)
    {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        LOCK(cs_main);

        CValidationState state;
        bool fMissingInputs = false;
        if (AcceptToMemoryPool(mempool, tx, state, &fMissingInputs))
        {
            RelayTransaction(tx, inv.hash);
            {
                LOCK(cs_mapAlreadyAskedFor);
                mapAlreadyAskedFor.erase(inv);
            }
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (set<uint256>::iterator mi = mapOrphanTransactionsByPrev[hashPrev].begin();
                     mi != mapOrphanTransactionsByPrev[hashPrev].end();
                     ++mi)
                {
                    const uint256& orphanTxHash = *mi;
                    CTransaction& orphanTx = mapOrphanTransactions[orphanTxHash];
                    CValidationState orphan_state;
                    bool fMissingInputs2 = false;

                    if (AcceptToMemoryPool(mempool, orphanTx, orphan_state, &fMissingInputs2))
                    {
                        LogPrintf("   accepted orphan tx %s", orphanTxHash.ToString().substr(0,10));
                        RelayTransaction(orphanTx, orphanTxHash);
                        {
                            LOCK(cs_mapAlreadyAskedFor);
                            mapAlreadyAskedFor.erase(CInv(MSG_TX, orphanTxHash));
                        }
                        vWorkQueue.push_back(orphanTxHash);
                        vEraseQueue.push_back(orphanTxHash);
                        pfrom->nTrust++;
                    }
                    else if (!fMissingInputs2)
                    {
                        // invalid orphan
                        vEraseQueue.push_back(orphanTxHash);
                        LogPrintf("   removed invalid orphan tx %s", orphanTxHash.ToString().substr(0,10));
                    }
                }
            }

            for (auto const& hash : vEraseQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            AddOrphanTx(tx);

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded (see CVE-2012-3789)
            unsigned int nEvicted = LimitOrphanTxSize(MAX_ORPHAN_TRANSACTIONS);
            if (nEvicted > 0)
                LogPrintf("mapOrphan overflow, removed %u tx", nEvicted);
        }
        int nDoS = 0;
        if (state.IsInvalid(nDoS) && nDoS > 0)
            pfrom->Misbehaving(nDoS);
    }


    else if (strCommand == NetMsgType::ENCRYPT || strCommand == NetMsgType::BLOCK)
    {
        //Response from getblocks, message = block

        CBlock block;
        vRecv >> block;

        uint256 hashBlock = block.GetHash(true);

        LogPrintf(" Received block %s; ", hashBlock.ToString());
        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY)) block.print();

        CInv inv(MSG_BLOCK, hashBlock);
        pfrom->AddInventoryKnown(inv);

        LOCK(cs_main);

        CValidationState state;
        if (ProcessBlock(pfrom, &block, false, state))
        {
            {
                LOCK(cs_mapAlreadyAskedFor);
                mapAlreadyAskedFor.erase(inv);
            }
            pfrom->nTrust++;
        }
        int nDoS = 0;
        if (state.IsInvalid(nDoS) && nDoS > 0)
        {
                pfrom->Misbehaving(nDoS);
                pfrom->nTrust--;
        }

    }


    else if (strCommand == NetMsgType::GETADDR)
    {
        // Don't return addresses older than nCutOff timestamp
        int64_t nCutOff =  GetAdjustedTime() - (nNodeLifespan * 24 * 60 * 60);
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        for (auto const&addr : vAddr)
            if(addr.nTime > nCutOff)
                pfrom->PushAddress(addr);
    }


    else if (strCommand == NetMsgType::MEMPOOL)
    {
        LOCK(cs_main);

        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        for (unsigned int i = 0; i < vtxid.size(); i++) {
            CInv inv(MSG_TX, vtxid[i]);
            vInv.push_back(inv);
            if (i == (MAX_INV_SZ - 1))
                    break;
        }
        if (vInv.size() > 0)
            pfrom->PushMessage(NetMsgType::INV, vInv);
    }
    else if (strCommand == NetMsgType::PING)
    {
        uint64_t nonce = 0;
        vRecv >> nonce;

        // Echo the message back with the nonce. This allows for two useful features:
        //
        // 1) A remote node can quickly check if the connection is operational
        // 2) Remote nodes can measure the latency of the network thread. If this node
        //    is overloaded it won't respond to pings quickly and the remote node can
        //    avoid sending us more work, like chain download requests.
        //
        // The nonce stops the remote getting confused between different pings: without
        // it, if the remote node sends a ping once per second and this node takes 5
        // seconds to respond to each, the 5th ping the remote sends would appear to
        // return very quickly.
        pfrom->PushMessage(NetMsgType::PONG, nonce);
    }
    else if (strCommand == NetMsgType::PONG)
    {
        int64_t pingUsecEnd = GetTimeMicros();
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0)
            {
                if (nonce == pfrom->nPingNonceSent)
                {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
                        // Successful ping time measurement, replace previous
                        pfrom->nPingUsecTime = pingUsecTime;
                        pfrom->nMinPingUsecTime = std::min(pfrom->nMinPingUsecTime.load(), pingUsecTime);
                    } else {
                        // This should never happen
                        sProblem = "Timing mishap";
                    }
                } else {
                    // Nonce mismatches are normal when pings are overlapping
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
                        // This is most likely a bug in another implementation somewhere, cancel this ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
            // This is most likely a bug in another implementation somewhere, cancel this ping
            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            LogPrintf("pong %s %s: %s, %" PRIx64 " expected, %" PRIx64 " received, %" PRIu64 " bytes"
                , pfrom->addr.ToString()
                , pfrom->strSubVer
                , sProblem, pfrom->nPingNonceSent, nonce, nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }
    else if (strCommand == NetMsgType::ALERT)
    {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0)
        {
            if (alert.ProcessAlert())
            {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    for (auto const& pnode : vNodes)
                        alert.RelayTo(pnode);
                }
            }
            else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                pfrom->Misbehaving(10);
            }
        }
    }

    else if (strCommand == NetMsgType::SCRAPERINDEX)
    {
        CScraperManifest::RecvManifest(pfrom, vRecv);
    }
    else if (strCommand == NetMsgType::PART)
    {
        CSplitBlob::RecvPart(pfrom, vRecv);
    }


    else
    {
        // Ignore unknown commands for extensibility
        // Let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever. Currently only SPV clients actually care
        // about this message: it's needed when they are recursively walking the
        // dependencies of relevant unconfirmed transactions. SPV clients want to
        // do that because they want to know about (and store and rebroadcast and
        // risk analyze) the dependencies of transactions relevant to them, without
        // having to download the entire memory pool.


    }

    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == NetMsgType::ARIES || strCommand == NetMsgType::GRIDADDR || strCommand == NetMsgType::INV || strCommand == NetMsgType::GETDATA || strCommand == NetMsgType::PING || strCommand == NetMsgType::VERSION || strCommand == NetMsgType::ADDR)
            AddressCurrentlyConnected(pfrom->addr);

    return true;
}

// File-static since PR 8a: the only callers are PeerManagerImpl (below) and,
// through it, ThreadMessageHandler via g_peerman. Was a net_processing.h export.
static bool ProcessMessages(CNode* pfrom) EXCLUSIVE_LOCKS_REQUIRED(pfrom->cs_vRecvMsg)
{
    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (WITH_LOCK(pfrom->cs_vSend, return pfrom->nSendSize) >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        LogPrint(BCLog::LogFlags::NOISY, "ProcessMessages(message %u msgsz, %zu bytes, complete:%s)",
                 msg.hdr.nMessageSize, msg.vRecv.size(),
                 msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE) != 0) {
            LogPrint(BCLog::LogFlags::NOISY, "PROCESSMESSAGE: INVALID MESSAGESTART");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid())
        {
            LogPrintf("PROCESSMESSAGE: ERRORS IN HEADER %s", hdr.GetCommand());
            continue;
        }
        string strCommand = hdr.GetCommand();


        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        // The previous form `&vRecv.begin()[0]` dereferenced begin() to take
        // its address, which UBSan reported as a null-pointer-of-type bind on
        // a zero-length message. `vRecv.data()` is well-defined for an empty
        // container, but the standard permits it to return nullptr when size()
        // is 0 -- and passing that down to CSHA256::Write would then exhibit
        // `nullptr + 0` UB inside the hash core. That root is closed (see
        // CSHA256::Write's len == 0 guard in crypto/sha256.cpp), so
        // empty-payload messages (verack et al.) hash cleanly here.
        uint256 hash = Hash(Span<std::byte>{vRecv.data(), nMessageSize});

        // We just received a message off the wire, harvest entropy from the time (and the message checksum)
        RandAddEvent(ReadLE32(hash.begin()));

        // TODO: hardcoded checksum size;
        //  will no longer be used once we adopt CNetMessage from Bitcoin
        uint8_t nChecksum[CMessageHeader::CHECKSUM_SIZE];
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (!std::equal(std::begin(nChecksum), std::end(nChecksum), std::begin(hdr.pchChecksum)))
        {
            LogPrintf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x",
               strCommand, nMessageSize, nChecksum, hdr.pchChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try
        {
            fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime);
            if (fShutdown)
                break;
        }
        catch (std::ios_base::failure& e)
        {
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length", strCommand, nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught", strCommand, nMessageSize, e.what());
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(nullptr, "ProcessMessages()");
        }

        if (!fRet)
        {
           LogPrint(BCLog::LogFlags::NOISY, "ProcessMessage(%s, %u bytes) FAILED", strCommand, nMessageSize);
        }
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}

// Note: this function requires a lock on cs_main before calling. (See below comments.)
// File-static since PR 8a (see ProcessMessages above).
static bool SendMessages(CNode* pto, bool fSendTrickle)
{
    // Some comments and TODOs in order...
    // 1. This function never returns anything but true... (try to find a return other than true).
    // 2. The try lock inside this function causes a potential deadlock due to a lock order reversal in main.
    // 3. The reason for the interior lock is vacated by 1. So the below is commented out, and moved to
    //    the ThreadMessageHandler2 in net.cpp.
    // 4. We need to research why we never return false at all, and subordinately, why we never consume
    //    the value of this function.

    /*
    // Treat lock failures as send successes in case the caller disconnects
    // the node based on the return value.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return true;
    */

    // Don't send anything until we get their version message
    if (pto->nVersion == 0)
        return true;

    //
    // Message: ping
    //
    bool pingSend = false;
    if (pto->fPingQueued)
    {
        // RPC ping request by user
        pingSend = true;
    }
    if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros())
    {
        // Ping automatically sent as a latency probe & keepalive.
        pingSend = true;
    }
    if (pingSend)
    {
        uint64_t nonce = 0;
        while (nonce == 0) {
            GetRandBytes({(unsigned char*)&nonce, sizeof(nonce)});
        }
        pto->fPingQueued = false;
        pto->nPingUsecStart = GetTimeMicros();
        pto->nPingNonceSent = nonce;

        pto->PushMessage(NetMsgType::PING, nonce);
    }

    // Resend wallet transactions that haven't gotten in a block yet.
    // No outer locks held here in SendMessages; acquire in canonical order
    // cs_main -> cs_setpwalletRegistered -> cs_wallet. cs_main is required
    // for the wallet method's mapBlockIndex / pindexBest reads.
    {
        LOCK2(cs_main, cs_setpwalletRegistered);
        ResendWalletTransactions();
    }

    // Address refresh broadcast
    if (!IsInitialBlockDownload())
    {
        if (GetAdjustedTime() > pto->nNextRebroadcastTime)
        {
            // Periodically clear setAddrKnown to allow refresh broadcasts
            //pnode->setAddrKnown.clear();
            // Rebroadcast our address
            if (!fNoListen)
            {
                AdvertiseLocal(pto);
                pto->nNextRebroadcastTime = GetAdjustedTime() + 12*60*60 + GetRand(12*60*60);
            }
        }
    }

    //
    // Message: addr
    //
    if (fSendTrickle)
    {
        vector<CAddress> vAddr;
        vAddr.reserve(pto->vAddrToSend.size());
        for (auto const& addr : pto->vAddrToSend)
        {
            // returns true if wasn't already contained in the set
            if (pto->setAddrKnown.insert(addr).second)
            {
                vAddr.push_back(addr);
                // receiver rejects addr messages larger than 1000
                if (vAddr.size() >= 1000)
                {
                    pto->PushMessage(NetMsgType::GRIDADDR, vAddr);
                    vAddr.clear();
                }
            }
        }
        pto->vAddrToSend.clear();
        if (!vAddr.empty())
            pto->PushMessage(NetMsgType::GRIDADDR, vAddr);
    }


    //
    // Message: inventory
    //
    vector<CInv> vInv;
    vector<CInv> vInvWait;
    {
        LOCK(pto->cs_inventory);
        vInv.reserve(pto->vInventoryToSend.size());
        vInvWait.reserve(pto->vInventoryToSend.size());
        for (auto const& inv : pto->vInventoryToSend)
        {
            if (pto->setInventoryKnown.count(inv))
                continue;

            // trickle out tx inv to protect privacy
            if (inv.type == MSG_TX && !fSendTrickle)
            {
                // 1/4 of tx invs blast to all immediately
                static arith_uint256 hashSalt;
                if (hashSalt == 0)
                    hashSalt = UintToArith256(GetRandHash());
                uint256 hashRand = ArithToUint256(UintToArith256(inv.hash) ^ hashSalt);
                hashRand = Hash(hashRand);
                bool fTrickleWait = ((UintToArith256(hashRand) & 3) != 0);

                // always trickle our own transactions
                if (!fTrickleWait)
                {
                    CWalletTx wtx;
                    LOCK(cs_setpwalletRegistered);
                    if (GetTransaction(inv.hash, wtx))
                        if (wtx.fFromMe)
                            fTrickleWait = true;
                }

                if (fTrickleWait)
                {
                    vInvWait.push_back(inv);
                    continue;
                }
            }

            // returns true if wasn't already contained in the set
            if (pto->setInventoryKnown.insert(inv).second)
            {
                vInv.push_back(inv);
                if (vInv.size() >= 1000)
                {
                    pto->PushMessage(NetMsgType::INV, vInv);
                    vInv.clear();
                }
            }
        }
        pto->vInventoryToSend = vInvWait;
    }
    if (!vInv.empty())
        pto->PushMessage(NetMsgType::INV, vInv);


    //
    // Message: getdata
    //
    vector<CInv> vGetData;
    int64_t nNow =  GetAdjustedTime() * 1000000;
    CTxDB txdb("r");
    while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
    {
        const CInv& inv = (*pto->mapAskFor.begin()).second;

        // mapAlreadyAskedFor gains an entry when the node enqueues a request
        // for the object from a peer, and the node removes the entry when it
        // receives the object. If the request does not exist in this map, we
        // don't need to ask for the object again:
        //
        bool already_asked_present;
        {
            LOCK(cs_mapAlreadyAskedFor);
            already_asked_present = mapAlreadyAskedFor.find(inv) != mapAlreadyAskedFor.end();
        }
        if (!already_asked_present)
        {
            pto->mapAskFor.erase(pto->mapAskFor.begin());
            continue;
        }

        // cs_main is required for the AlreadyHave call (mapBlockIndex
        // lookup) and must be released before the cs_mapManifest scope
        // below to preserve the canonical cs_main -> subsystem order
        // documented at the inv-handling site near main.cpp:2533.
        bool fAlreadyHave;
        {
            LOCK(cs_main);
            fAlreadyHave = AlreadyHave(txdb, inv);
        }

        // Check also the scraper data propagation system to see if it needs
        // this inventory object:
        if (fAlreadyHave)
        {
            LOCK(CScraperManifest::cs_mapManifest);
            fAlreadyHave = CScraperManifest::AlreadyHave(nullptr, inv);
        }

        if (!fAlreadyHave)
        {
            LogPrint(BCLog::LogFlags::NET, "sending getdata: %s", inv.ToString());
            vGetData.push_back(inv);
            if (vGetData.size() >= 1000)
            {
                pto->PushMessage(NetMsgType::GETDATA, vGetData);
                vGetData.clear();
            }

            // Re-check presence under the lock before refreshing the
            // timestamp. Another thread may have removed the entry
            // between the initial presence check above and here
            // (e.g. the inventory arrived and was processed via the
            // TX / BLOCK handlers in ProcessMessage, which erase under
            // cs_mapAlreadyAskedFor). If the entry is gone, the
            // request is satisfied and we should not reinsert it.
            {
                LOCK(cs_mapAlreadyAskedFor);
                auto it = mapAlreadyAskedFor.find(inv);
                if (it != mapAlreadyAskedFor.end()) {
                    it->second = nNow;
                }
            }
        }
        pto->mapAskFor.erase(pto->mapAskFor.begin());
    }
    if (!vGetData.empty())
        pto->PushMessage(NetMsgType::GETDATA, vGetData);

    return true;
}

// ---------------------------------------------------------------------------
// PeerManager (issue #2558 PR 8a): the message-processing manager. For now a
// thin shell over the file-static ProcessMessages/SendMessages above; the
// peer-level API (Misbehaving, ...) and the scheduled tasks land in PR 8b.
// ---------------------------------------------------------------------------

namespace {
class PeerManagerImpl final : public PeerManager
{
public:
    bool ProcessMessages(CNode* pfrom) override EXCLUSIVE_LOCKS_REQUIRED(pfrom->cs_vRecvMsg)
    {
        return ::ProcessMessages(pfrom);
    }

    bool SendMessages(CNode* pto, bool fSendTrickle) override
    {
        return ::SendMessages(pto, fSendTrickle);
    }

    void StartScheduledTasks(CScheduler& /*scheduler*/) override
    {
        // No recurring tasks yet (issue #2558 PR 8a shell).
    }
};
} // namespace

std::unique_ptr<PeerManager> g_peerman;

std::unique_ptr<PeerManager> PeerManager::make(CConnman& /*connman*/)
{
    return std::make_unique<PeerManagerImpl>();
}
