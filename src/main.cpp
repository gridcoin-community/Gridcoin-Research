// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "gridcoin/voting/registry.h"
#include "util.h"
#include "net.h"
#include "streams.h"
#include "alert.h"
#include "checkpoints.h"
#include "txdb.h"
#include "init.h"
#include "node/ui_interface.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/mrc.h"
#include "gridcoin/contract/contract.h"
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
#include "policy/fees.h"
#include "policy/policy.h"
#include "random.h"
#include "validation.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <ctime>
#include <math.h>

extern bool AskForOutstandingBlocks(uint256 hashStart);
extern bool GridcoinServices();

unsigned int nNodeLifespan;

using namespace std;
using namespace boost;

//
// Global state
//

CCriticalSection cs_setpwalletRegistered;
set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;

CTxMemPool mempool;

///////////////////////MINOR VERSION////////////////////////////////

extern int64_t GetCoinYearReward(int64_t nTime);

namespace GRC {
BlockIndexPool::Pool<CBlockIndex> BlockIndexPool::m_block_index_pool;
BlockIndexPool::Pool<ResearcherContext> BlockIndexPool::m_researcher_context_pool;
}

BlockMap mapBlockIndex;

//Gridcoin Minimum Stake Age (16 Hours)
unsigned int nStakeMinAge = 16 * 60 * 60; // 16 hours
unsigned int nStakeMaxAge = -1; // unlimited

// Gridcoin:
int nCoinbaseMaturity = 100;
CBlockIndex* pindexGenesisBlock = nullptr;
int nBestHeight = -1;

uint256 hashBestChain;
CBlockIndex* pindexBest = nullptr;
std::atomic<int64_t> g_previous_block_time;
std::atomic<int64_t> g_nTimeBestReceived;
std::atomic<bool> g_reorg_in_progress = false;
CMedianFilter<int> cPeerBlockCounts(5, 0); // Amount of blocks that other nodes claim to have




map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*> mapOrphanBlocksByPrev;

map<uint256, CTransaction> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;
const string strMessageMagic = "Gridcoin Signed Message:\n";

// Settings
// This is changed to MIN_TX_FEE * 10 for block version 11 (CTransaction::CURRENT_VERSION 2).
// Note that this is an early init value and will result in overpayment of the fee per kbyte
// if this code is run on a wallet prior to the v11 mandatory switchover unless a manual value
// of -paytxfee is specified as an argument.
int64_t nTransactionFee = MIN_TX_FEE * 10;
int64_t nReserveBalance = 0;
int64_t nMinimumInputValue = 0;

// Gridcoin - Rob Halford

bool fQtActive = false;
bool bGridcoinCoreInitComplete = false;

// Mining status variables
std::string    msMiningErrors;

//When syncing, we grandfather block rejection rules up to this block, as rules became stricter over time and fields changed
int nGrandfather = 1034700;

bool fEnforceCanonical = true;
bool fUseFastIndex = false;

// Temporary block version 11 transition helpers:
int64_t g_v11_timestamp = 0;

// End of Gridcoin Global vars

GRC::SeenStakes g_seen_stakes;
GRC::ChainTrustCache g_chain_trust;

//!
//! \brief Re-exports chain trust values for reporting.
//!
arith_uint256 GetChainTrust(const CBlockIndex* pindex)
{
    return g_chain_trust.GetTrust(pindex);
}

void RegisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.insert(pwalletIn);
    }
}

void UnregisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.erase(pwalletIn);
    }
}

// check whether the passed transaction is from us
bool static IsFromMe(CTransaction& tx)
{
    for (auto const& pwallet : setpwalletRegistered)
        if (pwallet->IsFromMe(tx))
            return true;
    return false;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    for (auto const& pwallet : setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx,wtx))
            return true;
    return false;
}

// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->EraseFromWallet(hash);
}

// make sure all wallets know about the given transaction, in the given block
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fConnect)
{
    if (!fConnect)
    {
        // ppcoin: wallets need to refund inputs when disconnecting coinstake
        if (tx.IsCoinStake())
        {
            for (auto const& pwallet : setpwalletRegistered)
            {
                if (pwallet->IsFromMe(tx))
                {
                    pwallet->DisableTransaction(tx);
                    g_miner_status.ClearLastStake();
                }
            }
        }
        return;
    }

    for (auto const& pwallet : setpwalletRegistered)
        pwallet->AddToWalletIfInvolvingMe(tx, pblock, fUpdate);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

// notify wallets about an updated transaction
void UpdatedTransaction(const uint256& hashTx)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->Inventory(hash);
}

// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->ResendWalletTransactions(fForce);
}


double CoinToDouble(double surrogate)
{
    //Converts satoshis to a human double amount
    double coin = (double)surrogate/(double)COIN;
    return coin;
}

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx)
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

void static EraseOrphanTx(uint256 hash)
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

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
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

//////////////////////////////////////////////////////////////////////////////
//
// CTransaction and CTxIndex
//


int CMerkleTx::SetMerkleBranch(const CBlock* pblock) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    CBlock blockTmp;
    if (pblock == nullptr) {
        // Load the block this tx is in
        CTxIndex txindex;
        if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
            return 0;
        if (!ReadBlockFromDisk(blockTmp, txindex.pos.nFile, txindex.pos.nBlockPos, Params().GetConsensus()))
            return 0;
        pblock = &blockTmp;
    }

    // Update the tx's hashBlock
    hashBlock = pblock->GetHash(true);

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)pblock->vtx.size(); nIndex++)
        if (pblock->vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)pblock->vtx.size())
    {
        nIndex = -1;
        LogPrintf("ERROR: SetMerkleBranch() : couldn't find tx in block");
        return 0;
    }

    // Is the tx in a block that's in the main chain
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = mi->second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    return pindexBest->nHeight - pindex->nHeight + 1;
}


bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx, bool* pfMissingInputs) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    // Mandatory switch to binary contracts (tx version 2):
    if (tx.nVersion < 2) {
        return tx.DoS(100, error("AcceptToMemoryPool : legacy transaction"));
    }

    if (!CheckTransaction(tx))
        return error("AcceptToMemoryPool : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return tx.DoS(100, error("AcceptToMemoryPool : coinbase as individual tx"));

    // ppcoin: coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return tx.DoS(100, error("AcceptToMemoryPool : coinstake as individual tx"));

    // Rather not work on nonstandard transactions
    if (!IsStandardTx(tx))
        return error("AcceptToMemoryPool : nonstandard transaction type");

    // Perform contextual validation for any contracts:

    int DoS = 0;
    if (!tx.GetContracts().empty() && !GRC::ValidateContracts(tx, DoS)) {
        return tx.DoS(DoS, error("%s: invalid contract in tx %s, assigning DoS misbehavior of %i",
                                 __func__,
                                 tx.GetHash().ToString(),
                                 DoS));
    }

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // is there already a transaction in the mempool that has a MRC contract with the same CPID?
    //
    // Note: I really hate this check because it has to iterate over the entire mempool to look for an offender,
    // but I am sufficiently concerned about MRC DoS that it is necessary to stop duplicate MRC request transactions
    // from the same CPID in the accept to memory pool stage.
    //
    // We have implemented a bloom filter to help with the overhead.
    bool tx_contains_valid_mrc = false;

    for (const auto& contract : tx.GetContracts()) {
        if (contract.m_type == GRC::ContractType::MRC) {
            GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

            GRC::Cpid cpid = *(mrc.m_mining_id.TryCpid());
            // A small bloom filter based on the last and first byte of CPID.
            uint64_t k = 1 << (cpid.Raw().front() & 0x3F);
            k |= 1 << (cpid.Raw().back() & 0x3F);

            if ((mempool.m_mrc_bloom & k) != k) {
                // The cpid definitely does not exist in the mempool.
                mempool.m_mrc_bloom |= k;
                tx_contains_valid_mrc = true;

                continue;
            }
            // The cpid might exist in the mempool.

            if (mempool.m_mrc_bloom_dirty) {
                mempool.m_mrc_bloom = 0;
            }

            bool found{false};
            for (const auto& [_, pool_tx] : mempool.mapTx) {
                for (const auto& pool_tx_contract : pool_tx.GetContracts()) {
                    if (pool_tx_contract.m_type == GRC::ContractType::MRC) {
                        GRC::MRC pool_tx_mrc = pool_tx_contract.CopyPayloadAs<GRC::MRC>();

                        GRC::Cpid other_cpid = *(pool_tx_mrc.m_mining_id.TryCpid());
                        mempool.m_mrc_bloom |= 1 << (other_cpid.Raw().front() & 0x3F);
                        mempool.m_mrc_bloom |= 1 << (other_cpid.Raw().back() & 0x3F);

                        // A transaction already in the mempool already has the same CPID as the incoming transaction.
                        // Reject and put a stiff DoS...
                        if (!found && cpid == other_cpid) {
                            found = true;
                            tx.DoS(25, error("%s: MRC contract in tx %s has the same CPID as an existing transaction "
                                             "in the memory pool, %s.",
                                             __func__,
                                             tx.GetHash().ToString(),
                                             pool_tx.GetHash().ToString()));

                            if (!mempool.m_mrc_bloom_dirty) return false;
                        }
                    }
                }
            }

            mempool.m_mrc_bloom_dirty = false;
            if (found) return false;

            tx_contains_valid_mrc = true;
        }
    }

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = nullptr;
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint))
            {
                // Disable replacement feature for now
                return false;

                // Allow replacing with a newer version of the same transaction
                if (i != 0)
                    return false;
                ptxOld = pool.mapNextTx[outpoint].ptx;
                if (IsFinalTx(*ptxOld))
                    return false;
                if (!tx.IsNewerThan(*ptxOld))
                    return false;
                for (unsigned int i = 0; i < tx.vin.size(); i++)
                {
                    COutPoint outpoint = tx.vin[i].prevout;
                    if (!pool.mapNextTx.count(outpoint) || pool.mapNextTx[outpoint].ptx != ptxOld)
                        return false;
                }
                break;
            }
        }
    }

    {
        CTxDB txdb("r");

        // do we already have it?
        if (txdb.ContainsTx(hash))
            return false;

        MapPrevTx mapInputs;
        map<uint256, CTxIndex> mapUnused;
        bool fInvalid = false;
        if (!FetchInputs(tx, txdb, mapUnused, false, false, mapInputs, fInvalid))
        {
            if (fInvalid)
                return error("AcceptToMemoryPool : FetchInputs found invalid tx %s", hash.ToString().substr(0,10).c_str());
            if (pfMissingInputs)
                *pfMissingInputs = true;
            return false;
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (!AreInputsStandard(tx, mapInputs) && !fTestNet)
            return error("AcceptToMemoryPool : nonstandard transaction input");

        // Note: if you modify this code to accept non-standard transactions, then
        // you should add code here to check that the transaction does a
        // reasonable number of ECDSA signature verifications.

        CAmount nFees = GetValueIn(tx, mapInputs) - tx.GetValueOut();
        unsigned int nSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

        // Don't accept it if it can't get into a block
        CAmount txMinFee = GetMinFee(tx, 1000, GMF_RELAY, nSize);
        if (nFees < txMinFee)
            return error("AcceptToMemoryPool : not enough fees %s, %" PRId64 " < %" PRId64 ", nSize %" PRId64,
                         hash.ToString().c_str(),
                         nFees, txMinFee, nSize);

        // Validate any contracts published in the transaction:
        if (!tx.GetContracts().empty() && !CheckContracts(tx, mapInputs, pindexBest->nHeight)) {
            return false;
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!ConnectInputs(tx, txdb, mapInputs, mapUnused, CDiskTxPos(1,1,1), pindexBest, false, false))
        {
            LogPrint(BCLog::LogFlags::MEMPOOL, "WARNING: %s: Unable to Connect Inputs %s.",
                     __func__,
                     hash.ToString().c_str());

            return false;
        }

        // If we accepted a transaction with a valid mrc contract, then signal MRC changed.
        if (tx_contains_valid_mrc) {
            uiInterface.MRCChanged();
        }
    }

    // Store transaction in memory
    {
        LOCK(pool.cs);
        if (ptxOld)
        {
            LogPrint(BCLog::LogFlags::MEMPOOL, "AcceptToMemoryPool : replacing tx %s with new version", ptxOld->GetHash().ToString());
            pool.remove(*ptxOld);
        }
        pool.addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());

    LogPrint(BCLog::LogFlags::MEMPOOL, "AcceptToMemoryPool : accepted %s (poolsz %" PRIszu ")", hash.ToString(), pool.mapTx.size());

    return true;
}

bool CTxMemPool::addUnchecked(const uint256& hash, CTransaction &tx)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptToMemoryPool to properly check the transaction first.
    {
        mapTx[hash] = tx;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
            mapNextTx[tx.vin[i].prevout] = CInPoint(&mapTx[hash], i);
    }
    return true;
}


bool CTxMemPool::remove(const CTransaction &tx, bool fRecursive)
{
    m_mrc_bloom_dirty = true;
    // Remove transaction from memory pool
    {
        LOCK(cs);
        uint256 hash = tx.GetHash();
        if (mapTx.count(hash))
        {
            if (fRecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                        remove(*it->second.ptx, true);
                }
            }
            for (auto const& txin : tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction &tx)
{
    m_mrc_bloom_dirty = true;
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    for (auto const &txin : tx.vin)
    {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction &txConflict = *it->second.ptx;
            if (txConflict != tx)
                remove(txConflict, true);
        }
    }
    return true;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (map<uint256, CTransaction>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back(mi->first);
}

int CMerkleTx::GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (hashBlock.IsNull() || nIndex == -1)
        return 0;
    AssertLockHeld(cs_main);

    // Find the block it claims to be in
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = mi->second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    pindexRet = pindex;
    return pindexBest->nHeight - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(CBlockIndex* &pindexRet) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    int nResult = GetDepthInMainChainINTERNAL(pindexRet);
    if (nResult == 0 && !mempool.exists(GetHash()))
        return -1; // Not in chain, not in mempool

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    return max(0, (nCoinbaseMaturity+10) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return ::AcceptToMemoryPool(mempool, *this, nullptr);
}



bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{

    {
        // Add previous supporting transactions first
        for (auto tx : vtxPrev)
        {
            if (!(tx.IsCoinBase() || tx.IsCoinStake()))
            {
                uint256 hash = tx.GetHash();
                if (!mempool.exists(hash) && !txdb.ContainsTx(hash))
                    tx.AcceptToMemoryPool();
            }
        }
        return AcceptToMemoryPool();
    }
    return false;
}

bool CWalletTx::AcceptWalletTransaction() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CTxDB txdb("r");
    return AcceptWalletTransaction(txdb);
}

// Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock
bool GetTransaction(const uint256 &hash, CTransaction &tx, uint256 &hashBlock)
{
    {
        LOCK(cs_main);
        {
            if (mempool.lookup(hash, tx))
            {
                return true;
            }
        }
        CTxDB txdb("r");
        CTxIndex txindex;
        if (ReadTxFromDisk(tx, txdb, COutPoint(hash, 0), txindex))
        {
            CBlock block;
            if (ReadBlockFromDisk(block, txindex.pos.nFile, txindex.pos.nBlockPos, Params().GetConsensus(), false))
                hashBlock = block.GetHash(true);
            return true;
        }
    }
    return false;
}






//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//
static const CBlock* GetOrphanRoot(const CBlock* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock;
}


// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    LOCK(cs_main);
    return std::max(cPeerBlockCounts.median(), Params().Checkpoints().GetHeight());
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    if ((pindexBest == nullptr || nBestHeight < GetNumBlocksOfPeers()) && nBestHeight < 1185000)
        return true;
    static int64_t nLastUpdate;
    static CBlockIndex* pindexLastBest;
    if (pindexBest != pindexLastBest)
    {
        pindexLastBest = pindexBest;
        nLastUpdate =  GetAdjustedTime();
    }
    return ( GetAdjustedTime() - nLastUpdate < 15 &&
            pindexBest->GetBlockTime() <  GetAdjustedTime() - 8 * 60 * 60);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    arith_uint256 nBestInvalidBlockTrust = pindexNew->GetBlockTrust();
    arith_uint256 nBestBlockTrust = pindexBest->GetBlockTrust();

    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s",
      pindexNew->GetBlockHash().ToString().substr(0,20),
      pindexNew->nHeight,
      g_chain_trust.GetTrust(pindexNew).ToString(),
      nBestInvalidBlockTrust.GetLow64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexNew->GetBlockTime()));
    LogPrintf("InvalidChainFound:  current best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s",
      hashBestChain.ToString().substr(0,20),
      nBestHeight,
      g_chain_trust.Best().ToString(),
      nBestBlockTrust.GetLow64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()));
}


static void UpdateSyncTime(const CBlockIndex* const pindexBest)
{
    if (pindexBest && pindexBest->pprev) {
        g_previous_block_time.store(pindexBest->pprev->GetBlockTime());
    } else {
        g_previous_block_time.store(0);
    }

    if (!OutOfSyncByAge()) {
        g_nTimeBestReceived.store(GetAdjustedTime());
    }
}

bool OutOfSyncByAge()
{
    // Assume we are out of sync if the current block age is 10
    // times older than the target spacing. This is the same
    // rules that Bitcoin uses.
    constexpr int64_t maxAge = 90 * 10;

    return GetAdjustedTime() - g_previous_block_time >= maxAge;
}

const GRC::Claim& CBlock::GetClaim() const
{
    if (nVersion >= 11 || !vtx[0].vContracts.empty()) {
        return *vtx[0].vContracts[0].SharePayloadAs<GRC::Claim>();
    }

    // Before block version 11, the Gridcoin reward claim context is stored
    // in the hashBoinc field of the first transaction. We cache the parsed
    // representation here to speed up subsequent access:
    //
    REF(vtx[0]).vContracts.emplace_back(GRC::MakeContract<GRC::Claim>(
        GRC::ContractAction::ADD,
        GRC::Claim::Parse(vtx[0].hashBoinc, nVersion)));

    return *vtx[0].vContracts[0].SharePayloadAs<GRC::Claim>();
}

GRC::Claim CBlock::PullClaim()
{
    if (nVersion >= 11 || !vtx[0].vContracts.empty()) {
        return vtx[0].vContracts[0].PullPayloadAs<GRC::Claim>();
    }

    // Before block version 11, the Gridcoin reward claim context is stored
    // in the hashBoinc field of the first transaction.
    //
    return GRC::Claim::Parse(vtx[0].hashBoinc, nVersion);
}

GRC::SuperblockPtr CBlock::GetSuperblock() const
{
    return GetClaim().m_superblock;
}

GRC::SuperblockPtr CBlock::GetSuperblock(const CBlockIndex* const pindex) const
{
    GRC::SuperblockPtr superblock = GetSuperblock();
    superblock.Rebind(pindex);

    return superblock;
}

bool ReorganizeChain(CTxDB& txdb, unsigned &cnt_dis, unsigned &cnt_con, CBlock &blockNew, CBlockIndex* pindexNew);
bool ForceReorganizeToHash(uint256 NewHash)
{
    LOCK(cs_main);
    CTxDB txdb;

    auto mapItem = mapBlockIndex.find(NewHash);
    if (mapItem == mapBlockIndex.end()) {
        return error("%s: failed to find requested block in block index", __func__);
    }

    CBlockIndex* pindexCur = pindexBest;
    CBlockIndex* pindexNew = mapItem->second;
    LogPrintf("INFO: %s: current best height %i hash %s "
              " Target height %i hash %s",
              __func__,
              pindexCur->nHeight,pindexCur->GetBlockHash().GetHex(),
              pindexNew->nHeight,pindexNew->GetBlockHash().GetHex());

    CBlock blockNew;
    if (!ReadBlockFromDisk(blockNew, pindexNew, Params().GetConsensus())) {
        return error("%s: Fatal Error while reading new best block.", __func__);
    }

    const arith_uint256 previous_chain_trust = g_chain_trust.Best();
    unsigned cnt_dis = 0;
    unsigned cnt_con = 0;
    bool success = false;

    // g_reorg_in_progress is set in ReorganizeChain, because ReorganizeChain determines whether this is a trivial
    // reorg, where we are just adding a new block to the head of the current chain, in which case we do not
    // want the flag to be set.
    success = ReorganizeChain(txdb, cnt_dis, cnt_con, blockNew, pindexNew);

    if (g_chain_trust.Best() < previous_chain_trust) {
        LogPrintf("WARN: %s: Chain trust is now less than before!", __func__);
    }

    // g_reorg_in_progress is set in ReorganizeChain, but cleared by the caller. It is debatable whether this should
    // be cleared here if g_chain_trust.Best() < previous_chain_trust.
    g_reorg_in_progress = false;

    if (!success) {
        return error("%s: Fatal Error while setting best chain.", __func__);
    }

    AskForOutstandingBlocks(uint256());
    LogPrintf("INFO %s: success! height %d hash %s",
              __func__,
              pindexBest->nHeight,
              pindexBest->GetBlockHash().GetHex());

    return true;
}

bool DisconnectBlocksBatch(CTxDB& txdb, list<CTransaction>& vResurrect, unsigned& cnt_dis, CBlockIndex* pcommon) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    set<string> vRereadCPIDs;
    GRC::BeaconRegistry& beacons = GRC::GetBeaconRegistry();
    GRC::PollRegistry& polls = GRC::GetPollRegistry();

    while(pindexBest != pcommon)
    {
        if(!pindexBest->pprev)
            return error("DisconnectBlocksBatch: attempt to reorganize beyond genesis"); /*fatal*/

        LogPrint(BCLog::LogFlags::VERBOSE, "DisconnectBlocksBatch: %s",pindexBest->GetBlockHash().GetHex());

        CBlock block;
        if (!ReadBlockFromDisk(block, pindexBest, Params().GetConsensus()))
            return error("DisconnectBlocksBatch: ReadFromDisk for disconnect failed"); /*fatal*/
        if (!DisconnectBlock(block, txdb, pindexBest))
            return error("DisconnectBlocksBatch: DisconnectBlock %s failed", pindexBest->GetBlockHash().ToString().c_str()); /*fatal*/

        // disconnect from memory
        assert(!pindexBest->pnext);
        if (pindexBest->pprev)
            pindexBest->pprev->pnext = nullptr;

        // Queue memory transactions to resurrect.
        // We only do this for blocks after the last checkpoint (reorganisation before that
        // point should only happen with -reindex/-loadblock, or a misbehaving peer).
        for (auto const& tx : boost::adaptors::reverse(block.vtx))
            if (!(tx.IsCoinBase() || tx.IsCoinStake()) && pindexBest->nHeight > Params().Checkpoints().GetHeight())
                vResurrect.push_front(tx);

        // TODO: Implement flag in CBlockIndex for mrcs?
        if (pindexBest->IsUserCPID() || !pindexBest->m_mrc_researchers.empty()) {
            // The user has no longer staked this block.
            GRC::Tally::ForgetRewardBlock(pindexBest);
        }

        if (pindexBest->IsSuperblock()) {
            // Revert beacon activations which were done by the superblock to revert and resurrect pending records
            // for the reverted activations. This is safe to do before the transactional level reverts with beacon
            // contracts, because any beacon that is activated CANNOT have been a new advertisement in the superblock
            // itself. It would not be verified. AND if the beacon is a renewal, it would never be in the activation list
            // for a superblock.
            beacons.Deactivate(pindexBest->GetBlockHash());

            GRC::Quorum::PopSuperblock(pindexBest);
            GRC::Quorum::LoadSuperblockIndex(pindexBest->pprev);

            if (pindexBest->nVersion >= 11 && !GRC::Tally::RevertSuperblock()) {
                return false;
            }
        }

        if (pindexBest->nHeight > nGrandfather && pindexBest->nVersion <= 10) {
            GRC::Quorum::ForgetVote(pindexBest);
        }

        // Delete beacons, polls and votes from contracts in disconnected blocks.
        if (pindexBest->IsContract())
        {
            // Skip coinbase and coinstake transactions:
            for (auto tx = std::next(block.vtx.begin(), 2), end = block.vtx.end();
                tx != end;
                ++tx)
            {
                for (const auto& contract : tx->GetContracts())
                {
                    if (contract.m_type == GRC::ContractType::BEACON)
                    {
                       const GRC::ContractContext contract_context(contract, *tx, pindexBest);

                       beacons.Revert(contract_context);
                    }

                    if (contract.m_type == GRC::ContractType::POLL || contract.m_type == GRC::ContractType::VOTE) {
                        const GRC::ContractContext contract_context(contract, *tx, pindexBest);

                        polls.Revert(contract_context);
                    }
                }
            }
        }

        // New best block
        cnt_dis++;
        pindexBest = pindexBest->pprev;
        hashBestChain = pindexBest->GetBlockHash();
        nBestHeight = pindexBest->nHeight;
        g_chain_trust.SetBest(pindexBest);

        UpdateSyncTime(pindexBest);

        if (!txdb.WriteHashBestChain(pindexBest->GetBlockHash()))
            return error("DisconnectBlocksBatch: WriteHashBestChain failed"); /*fatal*/

    }

    /* fix up after disconnecting, prepare for new blocks */
    if (cnt_dis > 0)
    {
        // Resurrect memory transactions that were in the disconnected branch
        for( CTransaction& tx : vResurrect)
            AcceptToMemoryPool(mempool, tx, nullptr);

        if (!txdb.TxnCommit())
            return error("DisconnectBlocksBatch: TxnCommit failed"); /*fatal*/

        // Record new best height (the common block) in the beacon registry after the series of reverts.
        GRC::BeaconRegistry& beacons = GRC::GetBeaconRegistry();
        beacons.SetDBHeight(pindexBest->nHeight);

        GRC::ReplayContracts(pindexBest);

        // Tally research averages.
        if(IsV9Enabled_Tally(nBestHeight) && !IsV11Enabled(nBestHeight)) {
            assert(GRC::Tally::IsLegacyTrigger(nBestHeight));
            GRC::Tally::LegacyRecount(pindexBest);
        }
    }

    return true;
}

bool ReorganizeChain(CTxDB& txdb, unsigned &cnt_dis, unsigned &cnt_con, CBlock &blockNew, CBlockIndex* pindexNew)
EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    assert(pindexNew);
    assert(pindexNew->GetBlockHash()==blockNew.GetHash(true));
    /* note: it was already determined that this chain is better than current best */
    /* assert(pindexNew->nChainTrust > nBestChainTrust); but may be overridden by command */
    assert(!pindexGenesisBlock == !pindexBest);

    list<CTransaction> vResurrect;
    list<CBlockIndex*> vConnect;
    set<string> vRereadCPIDs;

    /* find fork point */
    CBlockIndex* pcommon = nullptr;

    if (pindexGenesisBlock) {
        pcommon = pindexNew;
        // The trivial reorg that most often happens here is where pcommon IS pindexBest, which
        // means effectively we are simply adding a new block to the end of the current chain.
        // Notice the logic of the while statement allows one traversal in that case... When
        // pcommon = pindexNew then pcommon->pnext will be nullptr. It will also satisfy
        // pcommon != pindexBest. Then the pointer moves back one block, which in the trivial
        // case will be pindexBest and the while loop ends with pcommon = pindexBest.
        while (pcommon->pnext == nullptr && pcommon != pindexBest) {
            pcommon = pcommon->pprev;

            if (!pcommon) {
                return error("%s: unable to find fork root", __func__);
            }
        }

        // Blocks version 11+ do not use the legacy tally system triggered by
        // block height intervals:
        //
        if (pcommon->nVersion <= 10 && pcommon != pindexBest) {
            pcommon = GRC::Tally::FindLegacyTrigger(pcommon);

            if (!pcommon) {
                return error("%s: unable to find fork root with tally point", __func__);
            }
        }

        if (pcommon != pindexBest || pindexNew->pprev != pcommon) {
            // This is set for the benefit of other threads, such as the GUI or rpc for the poll registry,
            // that do work while not locking cs_main.
            g_reorg_in_progress = true;

            LogPrintf("INFO: %s: from {%s %d}, common {%s %d}, to {%s %d}, disconnecting %d, connecting %d blocks",
                      __func__,
                      pindexBest->GetBlockHash().GetHex().c_str(), pindexBest->nHeight,
                      pcommon->GetBlockHash().GetHex().c_str(), pcommon->nHeight,
                      pindexNew->GetBlockHash().GetHex().c_str(), pindexNew->nHeight,
                      pindexBest->nHeight - pcommon->nHeight,
                      pindexNew->nHeight - pcommon->nHeight);
        }
    }

    /* disconnect blocks (in non-trivial condition) */
    if (pcommon != pindexBest) {
        if (!txdb.TxnBegin()) {
            return error("%s: TxnBegin failed", __func__);
        }
        if (!DisconnectBlocksBatch(txdb, vResurrect, cnt_dis, pcommon)) {
            error("%s: DisconnectBlocksBatch() failed. This is a fatal error. The chain index may be corrupt. Aborting. "
                  "Please reindex the chain and restart.", __func__);
            exit(1); //todo
        }

        int nMismatchSpent;
        int64_t nBalanceInQuestion;
        pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion);
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && cnt_dis > 0) {

        LogPrintf("INFO: %s: disconnected %d blocks", __func__, cnt_dis);
    }

    for (CBlockIndex *p = pindexNew; p != pcommon; p=p->pprev) {
        vConnect.push_front(p);
    }

    /* Connect blocks */
    for (auto const pindex : vConnect) {
        CBlock block_load;
        CBlock &block = (pindex==pindexNew)? blockNew : block_load;

        if (pindex != pindexNew) {
            if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
                return error("%s: ReadFromDisk for connect failed", __func__);
            }

            assert(pindex->GetBlockHash()==block.GetHash(true));
        } else {
            assert(pindex == pindexNew);
            assert(pindexNew->GetBlockHash() == block.GetHash(true));
            assert(pindexNew->GetBlockHash() == blockNew.GetHash(true));
        }

        uint256 hash = block.GetHash(true);

        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: connect %s", __func__, hash.ToString());

        if (!txdb.TxnBegin()) {
            return error("%s: TxnBegin failed", __func__);
        }

        if (pindexGenesisBlock == nullptr) {
            if (hash != (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet)) {
                txdb.TxnAbort();
                return error("%s: genesis block hash does not match", __func__);
            }

            pindexGenesisBlock = pindex;
        } else {
            assert(pindex->GetBlockHash()==block.GetHash(true));
            assert(pindex->pprev == pindexBest);

            if (!ConnectBlock(block, txdb, pindex, false)) {
                txdb.TxnAbort();
                error("%s: ConnectBlock %s failed, Previous block %s",
                      __func__,
                      hash.ToString().c_str(),
                      pindex->pprev->GetBlockHash().ToString());
                InvalidChainFound(pindex);
                return false;
            }
        }

        // Delete redundant memory transactions
        for (auto const& tx : block.vtx) {
            mempool.remove(tx);
            mempool.removeConflicts(tx);
        }

        // Remove stale MRCs in the mempool that are not in this new block. Remember the MRCs were initially validated in
        // AcceptToMemoryPool. Here we just need to do a staleness check.
        std::vector<CTransaction> to_be_erased;

        for (const auto& [_, pool_tx] : mempool.mapTx) {
            for (const auto& pool_tx_contract : pool_tx.GetContracts()) {
                if (pool_tx_contract.m_type == GRC::ContractType::MRC) {
                    GRC::MRC pool_tx_mrc = pool_tx_contract.CopyPayloadAs<GRC::MRC>();

                    if (pool_tx_mrc.m_last_block_hash != hashBestChain) {
                        to_be_erased.push_back(pool_tx);
                    }
                }
            }
        }

        // TODO: Additional mempool removals for generic transactions based on txns...
        // that satisfy lock time requirements,
        // that are at least 30m old,
        // that have been broadcast at least once min 5m ago,
        // that had at least 45s to go in to the last block,
        // and are still not in the txdb? (for the wallet itself, not mempool.)

        for (const auto& tx : to_be_erased) {
            LogPrintf("%s: Erasing stale transaction %s from mempool and wallet.", __func__, tx.GetHash().ToString());
            mempool.remove(tx);
            // If this transaction was in this wallet (i.e. erasure successful), then send signal for GUI.
            if (pwalletMain->EraseFromWallet(tx.GetHash())) {
                pwalletMain->NotifyTransactionChanged(pwalletMain, tx.GetHash(), CT_DELETED);
            }
        }

        // Clean up spent outputs in wallet that are now not spent if mempool transactions erased above. This
        // is ugly and heavyweight and should be replaced when the upstream wallet code is ported. Unlike the
        // repairwallet rpc, this is silent.
        if (!to_be_erased.empty()) {
            int nMisMatchFound = 0;
            CAmount nBalanceInQuestion = 0;

            pwalletMain->FixSpentCoins(nMisMatchFound, nBalanceInQuestion);
        }

        if (!txdb.WriteHashBestChain(pindex->GetBlockHash())) {
            txdb.TxnAbort();
            return error("%s: WriteHashBestChain failed", __func__);
        }

        // Make sure it's successfully written to disk before changing memory structure
        if (!txdb.TxnCommit()) {
            return error("%s: TxnCommit failed", __func__);
        }

        // Add to current best branch
        if (pindex->pprev) {
            assert(!pindex->pprev->pnext);
            pindex->pprev->pnext = pindex;
        }

        // update best block
        hashBestChain = hash;
        pindexBest = pindex;
        nBestHeight = pindexBest->nHeight;
        g_chain_trust.SetBest(pindexBest);
        cnt_con++;

        UpdateSyncTime(pindexBest);

        if (IsV9Enabled_Tally(nBestHeight)
            && !IsV11Enabled(nBestHeight)
            && GRC::Tally::IsLegacyTrigger(nBestHeight)) {

            GRC::Tally::LegacyRecount(pindexBest);
        }
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && (cnt_dis > 0 || cnt_con > 1)) {
        LogPrintf("INFO %s: Disconnected %d and connected %d blocks.",__func__, cnt_dis, cnt_con);
    }

    return true;
}

bool SetBestChain(CTxDB& txdb, CBlock &blockNew, CBlockIndex* pindexNew) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    unsigned cnt_dis = 0;
    unsigned cnt_con = 0;
    bool success = false;
    const auto origBestIndex = pindexBest;
    const arith_uint256 previous_chain_trust = g_chain_trust.Best();

    // g_reorg_in_progress is set in ReorganizeChain, because ReorganizeChain determines whether this is a trivial
    // reorg, where we are just adding a new block to the head of the current chain, in which case we do not
    // want the flag to be set.
    success = ReorganizeChain(txdb, cnt_dis, cnt_con, blockNew, pindexNew);

    if (previous_chain_trust > g_chain_trust.Best()) {
        LogPrintf("INFO: %s: Reorganize caused lower chain trust than before. Reorganizing back.", __func__);

        CBlock origBlock;

        if (!ReadBlockFromDisk(origBlock, origBestIndex, Params().GetConsensus())) {
            return error("%s: Fatal Error while reading original best block", __func__);
        }

        success = ReorganizeChain(txdb, cnt_dis, cnt_con, origBlock, origBestIndex);
    }

    if (!success) {
        return false;
    }

    // g_reorg_in_progress is set in ReorganizeChain, but cleared by the caller, because as above it can
    // be called more than once as part of what is really a single reorg.
    g_reorg_in_progress = false;

    /* Fix up after block connecting */

    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();

    if (!fIsInitialDownload) {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE)) {
        LogPrintf("INFO: %s: new best block {%s %d}, trust=%s, date=%s",
                  __func__,
                  hashBestChain.ToString(), nBestHeight,
                  g_chain_trust.Best().ToString(),
                  DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()));
    } else {
        LogPrintf("INFO: %s: new best block {%s %d}",
                  __func__,
                  hashBestChain.ToString(),
                  nBestHeight);
    }

    #if HAVE_SYSTEM
    std::string strCmd = gArgs.GetArg("-blocknotify", "");
    if (!fIsInitialDownload && !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }
    #endif

    uiInterface.NotifyBlocksChanged(
        fIsInitialDownload,
        pindexNew->nHeight,
        pindexNew->GetBlockTime(),
        blockNew.nBits);

    return GridcoinServices();
}

arith_uint256 CBlockIndex::GetBlockTrust() const
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for an arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (bnTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

bool GridcoinServices()
{
    // Block version 9 tally transition:
    //
    // This block controls the switch to a new tallying system introduced with
    // block version 9. Mainnet and testnet activate at different heights seen
    // below.
    //
    if (!IsV9Enabled_Tally(nBestHeight)
        && IsV9Enabled(nBestHeight + (fTestNet ? 200 : 40))
        && nBestHeight % 20 == 0)
    {
        LogPrint(BCLog::LogFlags::TALLY,
            "GridcoinServices: Priming tally system for v9 threshold.");

        GRC::Tally::LegacyRecount(pindexBest);
    }

    // Block version 11 tally transition:
    //
    // Before the first version 11 block arrives, activate the snapshot accrual
    // system by creating a baseline of the research rewards owed in historical
    // superblocks so that we can validate the reward for the next block.
    //
    if (nBestHeight + 1 == Params().GetConsensus().BlockV11Height) {
        LogPrint(BCLog::LogFlags::TALLY,
            "GridcoinServices: Priming tally system for v11 threshold.");

        if (!GRC::Tally::ActivateSnapshotAccrual(pindexBest)) {
            return error("GridcoinServices: Failed to prepare tally for v11.");
        }

        // Set the timestamp for the block version 11 threshold. This
        // is temporary. Remove this variable in a release that comes
        // after the hard fork.
        //
        g_v11_timestamp = pindexBest->nTime;
    }

    // Fix ability for new CPIDs to accrue research rewards earlier than one
    // superblock.
    //
    // A bug in the snapshot accrual system for block version 11+ requires a
    // consensus change to fix. This activates the solution at the following
    // height:
    //

    // This is actually broken. Commented out.
    /*
    if (nBestHeight + 1 == GetNewbieSnapshotFixHeight()) {
        if (!GRC::Tally::FixNewbieSnapshotAccrual()) {
            return error("%s: Failed to fix newbie snapshot accrual", __func__);
        }
    }
    */

    return true;
}

bool AskForOutstandingBlocks(uint256 hashStart)
{
    int iAsked = 0;
    LOCK(cs_vNodes);
    for (auto const& pNode : vNodes)
    {
                if (!pNode->fClient && !pNode->fOneShot && (pNode->nStartingHeight > (nBestHeight - 144)))
                {
                        if (hashStart==uint256())
                        {
                            pNode->PushGetBlocks(pindexBest, uint256());
                        }
                        else
                        {
                            CBlockIndex* pblockindex = mapBlockIndex[hashStart];
                            if (pblockindex)
                            {
                                pNode->PushGetBlocks(pblockindex, uint256());
                            }
                            else
                            {
                                return error("Unable to find block index %s",hashStart.ToString().c_str());
                            }
                        }
                        LogPrintf("Asked for blocks");
                        iAsked++;
                        if (iAsked > 10) break;
                }
    }
    return true;
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock, bool generated_by_me) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    // Check for duplicate
    uint256 hash = pblock->GetHash(true);
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.ToString().c_str());

    if (pblock->hashPrevBlock != hashBestChain)
    {
        // Extra checks to prevent "fill up memory by spamming with bogus blocks"
        const CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
        if (pcheckpoint != nullptr) {
            int64_t deltaTime = pblock->GetBlockTime() - pcheckpoint->nTime;
            if (deltaTime < 0)
            {
                if (pfrom)
                    pfrom->Misbehaving(1);
                return error("ProcessBlock() : block with timestamp before last checkpoint");
            }
        }
    }

    // Preliminary checks
    if (!CheckBlock(*pblock, pindexBest->nHeight + 1))
        return error("ProcessBlock() : CheckBlock FAILED");

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!pblock->hashPrevBlock.IsNull() && !mapBlockIndex.count(pblock->hashPrevBlock))
    {
        LogPrintf("ProcessBlock: ORPHAN BLOCK, prev=%s", pblock->hashPrevBlock.ToString());

        // If we can't ask the node for the parent blocks, no need to keep it.
        // This happens while loading a bootstrap file (-loadblock):
        if (!pfrom) {
            return true;
        }

        if (pblock->IsProofOfStake()) {
            if (g_seen_stakes.ContainsOrphan(pblock->vtx[1])
                && !mapOrphanBlocksByPrev.count(hash))
            {
                return error(
                    "%s: ignored duplicate proof-of-stake for orphan %s",
                    __func__,
                    hash.ToString());
            } else {
                g_seen_stakes.RememberOrphan(pblock->vtx[1]);
            }
        }

        CBlock* pblock2 = new CBlock(*pblock);
        mapOrphanBlocks.insert(make_pair(hash, pblock2));
        mapOrphanBlocksByPrev.insert(make_pair(pblock->hashPrevBlock, pblock2));

        // Ask this guy to fill in what we're missing
        const CBlock* const pblock_root = GetOrphanRoot(pblock2);
        pfrom->PushGetBlocks(pindexBest, pblock_root->GetHash(true));
        // ppcoin: getblocks may not obtain the ancestor block rejected
        // earlier by duplicate-stake check so we ask for it again directly
        if (!IsInitialBlockDownload())
        {
            const CInv ancestor_request(MSG_BLOCK, pblock_root->hashPrevBlock);

            // Ensure that this request is not deferred. CNode::AskFor() bumps
            // the earliest time for a message by two minutes for each call. A
            // node with many connections can miss a parent block because this
            // method can delay the queued request so far into the future that
            // it never sends the request to download that block. We reset the
            // request time first to guarantee that the node does not postpone
            // the message:
            //
            mapAlreadyAskedFor[ancestor_request] = 0;
            pfrom->AskFor(ancestor_request);
        }

        return true;
    }

    // Store to disk
    if (!AcceptBlock(*pblock, generated_by_me))
        return error("ProcessBlock() : AcceptBlock FAILED");

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            CBlock* pblockOrphan = mi->second;
            if (AcceptBlock(*pblockOrphan, generated_by_me))
                vWorkQueue.push_back(pblockOrphan->GetHash(true));
            mapOrphanBlocks.erase(pblockOrphan->GetHash(true));
            g_seen_stakes.ForgetOrphan(pblockOrphan->vtx[1]);
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    return true;
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = fs::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
    {
        fShutdown = true;
        string strMessage = _("Warning: Disk space is low!");
        strMiscWarning = strMessage;
        LogPrintf("*** %s", strMessage);
        uiInterface.ThreadSafeMessageBox(strMessage, "Gridcoin", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return false;
    }
    return true;
}

static fs::path BlockFilePath(unsigned int nFile)
{
    string strBlockFn = strprintf("blk%04u.dat", nFile);
    return GetDataDir() / strBlockFn;
}

FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if ((nFile < 1) || (nFile == (unsigned int) -1))
        return nullptr;
    FILE* file = fsbridge::fopen(BlockFilePath(nFile), pszMode);
    if (!file)
        return nullptr;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return nullptr;
        }
    }
    return file;
}

static unsigned int nCurrentBlockFile = 1;

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    while (true)
    {
        FILE* file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
        if (!file)
            return nullptr;
        if (fseek(file, 0, SEEK_END) != 0)
            return nullptr;
        // FAT32 file size max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < (long)(0x7F000000 - MAX_SIZE))
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        fclose(file);
        nCurrentBlockFile++;
    }
}

bool LoadBlockIndex(bool fAllowNew)
{
    LOCK(cs_main);

    if (fTestNet)
    {
        // GLOBAL TESTNET SETTINGS - R HALFORD
        nStakeMinAge = 1 * 60 * 60; // test net min age is 1 hour
        nCoinbaseMaturity = 10; // test maturity is 10 blocks
        nGrandfather = 196550;
        //1-24-2016
        MAX_OUTBOUND_CONNECTIONS = (int)gArgs.GetArg("-maxoutboundconnections", 8);
    }

    LogPrintf("Mode=%s", fTestNet ? "TestNet" : "Prod");

    //
    // Load block index
    //
    CTxDB txdb("cr+");
    if (!txdb.LoadBlockIndex())
        return false;

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty())
    {
        if (!fAllowNew)
            return false;

        // Genesis block - Genesis2
        // MainNet - Official New Genesis Block:
        ////////////////////////////////////////
        /*
     21:58:24 block.nTime = 1413149999
    10/12/14 21:58:24 block.nNonce = 1572771
    10/12/14 21:58:24 block.GetHash = 00000f762f698b5962aa81e38926c3a3f1f03e0b384850caed34cd9164b7f990
    10/12/14 21:58:24 CBlock(hash=00000f762f698b5962aa81e38926c3a3f1f03e0b384850caed34cd9164b7f990, ver=1,
    hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000,
    hashMerkleRoot=0bd65ac9501e8079a38b5c6f558a99aea0c1bcff478b8b3023d09451948fe841, nTime=1413149999, nBits=1e0fffff, nNonce=1572771, vtx=1, vchBlockSig=)
    10/12/14 21:58:24   Coinbase(hash=0bd65ac950, nTime=1413149999, ver=1, vin.size=1, vout.size=1, nLockTime=0)
    CTxIn(COutPoint(0000000000, 4294967295), coinbase 00012a4531302f31312f313420416e6472656120526f73736920496e647573747269616c20486561742076696e646963617465642077697468204c454e522076616c69646174696f6e)
    CTxOut(empty)
    vMerkleTree: 0bd65ac950

        */

        const char* pszTimestamp = "10/11/14 Andrea Rossi Industrial Heat vindicated with LENR validation";

        CTransaction txNew;
        //GENESIS TIME
        txNew.nVersion = 1;
        txNew.nTime = 1413033777;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 0 << CBigNum(42) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock.SetNull();
        block.hashMerkleRoot = BlockMerkleRoot(block);
        block.nVersion = 1;
        //R&D - Testers Wanted Thread:
        block.nTime    = !fTestNet ? 1413033777 : 1406674534;
        //Official Launch time:
        block.nBits    = UintToArith256(Params().GetConsensus().powLimit).GetCompact();
        block.nNonce = !fTestNet ? 130208 : 22436;
        LogPrintf("starting Genesis Check...");
        // If genesis block hash does not match, then generate new genesis hash.
        if (block.GetHash(true) != (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
        {
            LogPrintf("Searching for genesis block...");
            // This will figure out a valid hash and Nonce if you're
            // creating a different genesis block: 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000xFFF
            arith_uint256 hashTarget = arith_uint256().SetCompact(block.nBits);
            arith_uint256 thash;
            while (true)
            {
                thash = UintToArith256(block.GetHash(true));
                if (thash <= hashTarget)
                    break;
                if ((block.nNonce & 0xFFF) == 0)
                {
                    LogPrintf("nonce %08X: hash = %s (target = %s)", block.nNonce, thash.ToString(), hashTarget.ToString());
                }
                ++block.nNonce;
                if (block.nNonce == 0)
                {
                    LogPrintf("NONCE WRAPPED, incrementing time");
                    ++block.nTime;
                }
            }
            LogPrintf("block.nTime = %u ", block.nTime);
            LogPrintf("block.nNonce = %u ", block.nNonce);
            LogPrintf("block.GetHash = %s", block.GetHash(true).ToString());
        }


        block.print();

        //// debug print

        //GENESIS3: Official Merkle Root
        uint256 merkle_root = uint256S("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f");
        assert(block.hashMerkleRoot == merkle_root);
        assert(block.GetHash(true) == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
        assert(CheckBlock(block, 1));

        // Start new block file
        unsigned int nFile;
        unsigned int nBlockPos;
        if (!WriteBlockToDisk(block, nFile, nBlockPos, Params().MessageStart()))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        if (!AddToBlockIndex(block, nFile, nBlockPos, hashGenesisBlock))
            return error("LoadBlockIndex() : genesis block not accepted");
    }

    if (fRequestShutdown) {
        return true;
    }

    UpdateSyncTime(pindexBest);

    g_chain_trust.Initialize(pindexGenesisBlock, pindexBest);
    g_seen_stakes.Refill(pindexBest);

    return true;
}

void PrintBlockTree() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (BlockMap::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = mi->second;
        mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        std::stringstream output;

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++) {
                output << "| \n";
            }

            output << "|\\\n";
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++) {
                output << "| \n";
            }

            output << "|\n";
        }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++) {
            output << "| \n";
        }

        // print item (and also prepend above formatting)
        CBlock block;
        ReadBlockFromDisk(block, pindex, Params().GetConsensus());
        LogPrintf("%s%d (%u,%u) %s  %08x  %s  tx %" PRIszu "",
                  output.str(),
                  pindex->nHeight,
                  pindex->nFile,
                  pindex->nBlockPos,
                  block.GetHash(true).ToString().c_str(),
                  block.nBits,
                  DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str(),
                  block.vtx.size());

        PrintWallets(block);

        // put the main time-chain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (unsigned int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
    }
}

bool LoadExternalBlockFile(FILE* fileIn, size_t file_size, unsigned int percent_start, unsigned int percent_end)
{
    int64_t nStart = GetTimeMillis();
    int nLoaded = 0;

    bool display_progress = (file_size > 0 && (percent_end - percent_start) > 0) ? true : false;
    unsigned int cached_percent_progress = 0;

    if (display_progress) {
        uiInterface.InitMessage(_("Block file load progress ") + ToString(percent_start) + "%");
    }

    {
        LOCK(cs_main);
        try {
            CAutoFile blkdat(fileIn, SER_DISK, CLIENT_VERSION);
            unsigned int nPos = 0;
            while (nPos != (unsigned int)-1 && !fRequestShutdown)
            {
                unsigned char pchData[65536];
                do {
                    fseek(blkdat.Get(), nPos, SEEK_SET);
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat.Get());
                    if (nRead <= 8)
                    {
                        nPos = (unsigned int)-1;
                        break;
                    }
                    void* nFind = memchr(pchData, Params().MessageStart()[0], nRead + 1 - CMessageHeader::MESSAGE_START_SIZE);
                    if (nFind)
                    {
                        if (memcmp(nFind, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE) == 0)
                        {
                            nPos += ((unsigned char*)nFind - pchData) + CMessageHeader::MESSAGE_START_SIZE;
                            break;
                        }
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                    }
                    else
                        nPos += sizeof(pchData) - CMessageHeader::MESSAGE_START_SIZE + 1;
                } while(!fRequestShutdown);

                if (nPos == (unsigned int)-1) {
                    if (display_progress) {
                        uiInterface.InitMessage(_("Block file load progress ") + ToString(percent_end) + "%");
                    }

                    break;
                }

                fseek(blkdat.Get(), nPos, SEEK_SET);
                unsigned int nSize;
                blkdat >> nSize;
                if (nSize > 0 && nSize <= MAX_BLOCK_SIZE)
                {
                    CBlock block;
                    blkdat >> block;
                    if (ProcessBlock(nullptr, &block, false)) {
                        ++nLoaded;

                        if (display_progress) {
                            unsigned int percent_progress = percent_start + (uint64_t) nPos
                                    * (uint64_t) (percent_end - percent_start) / file_size;

                            if (percent_progress != cached_percent_progress) {
                                uiInterface.InitMessage(_("Block file load progress ") + ToString(percent_progress) + "%");
                                LogPrintf("INFO: %s: blocks/s: %f, progress: %u%%", __func__,
                                          nLoaded / ((GetTimeMillis() - nStart) / 1000.0), percent_progress);

                                cached_percent_progress = percent_progress;
                            }
                        } else if (nLoaded % 10000 == 0) {
                            LogPrintf("Blocks/s: %f", nLoaded / ((GetTimeMillis() - nStart) / 1000.0));
                        }

                        nPos += 4 + nSize;
                    }
                }
            }
        }
        catch (std::exception &e) {
            LogPrintf("%s() : Deserialize or I/O error caught during load",
                   __PRETTY_FUNCTION__);
        }
    }
    LogPrintf("Loaded %i blocks from external file in %" PRId64 "ms", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

extern map<uint256, CAlert> mapAlerts;
extern CCriticalSection cs_mapAlerts;

string GetWarnings(string strFor)
{
    int nPriority = 0;
    string strStatusBar;

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        for (auto const& item : mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority)
            {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
            }
        }
    }

    if (strFor == "statusbar")
    {
        return strStatusBar;
    }

    assert(!"GetWarnings() : invalid parameter");
    return "error";
}

//////////////////////////////////////////////////////////////////////////////
//
// Messages
//

bool static AlreadyHave(CTxDB& txdb, const CInv& inv)
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
               mapOrphanBlocks.count(inv.hash);
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

        // Note the std::max is there to deal with the rollover of BlockV12Height + DISCONNECT_GRACE_PERIOD if
        // BlockV12Height is set to std::numeric_limits<int>::max() which is the case during testing.
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION
                || (DISCONNECT_OLD_VERSION_AFTER_GRACE_PERIOD
                    && pfrom->nVersion < PROTOCOL_VERSION
                    && pindexBest->nHeight > std::max(Params().GetConsensus().BlockV12Height,
                                                      Params().GetConsensus().BlockV12Height + DISCONNECT_GRACE_PERIOD)))
        {
            // disconnect from peers older than this proto version
            LogPrint(BCLog::LogFlags::NOISY, "partner %s using obsolete version %i; disconnecting",
                     pfrom->addr.ToString(), pfrom->nVersion);

            pfrom->fDisconnect = true;
            return false;
        }

        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty())
            vRecv >> pfrom->strSubVer;
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



        pfrom->addrLocal = addrMe;
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
        if (addrMe.IsRoutable())
            addrSeenByPeer = addrMe;

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
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));


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
        if (!pfrom->fClient && !pfrom->fOneShot &&
            (pfrom->nStartingHeight > (nBestHeight - 144)) &&
             (nAskedForBlocks < 1 || (vNodes.size() <= 1 && nAskedForBlocks < 1)))
        {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(pindexBest, uint256());
            LogPrint(BCLog::LogFlags::NET, "Asked For blocks.");
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

        cPeerBlockCounts.input(pfrom->nStartingHeight);
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
        if (pfrom->nStartingHeight < (nBestHeight*.5)) return true;

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
                else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                    pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash])->GetHash(true));
                } else if (nInv == nLastBlock) {
                    // In case we are on a very long side-chain, it is possible that we already have
                    // the last block in an inv bundle sent in response to getblocks. Try to detect
                    // this situation and push another getblocks to continue.
                    pfrom->PushGetBlocks(mapBlockIndex[inv.hash], uint256());
                    LogPrint(BCLog::LogFlags::NOISY, "force getblock request: %s", inv.ToString());
                }

                // Track requests for our stuff
                Inventory(inv.hash);

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
            Inventory(inv.hash);
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

        bool fMissingInputs = false;
        if (AcceptToMemoryPool(mempool, tx, &fMissingInputs))
        {
            RelayTransaction(tx, inv.hash);
            mapAlreadyAskedFor.erase(inv);
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
                    bool fMissingInputs2 = false;

                    if (AcceptToMemoryPool(mempool, orphanTx, &fMissingInputs2))
                    {
                        LogPrintf("   accepted orphan tx %s", orphanTxHash.ToString().substr(0,10));
                        RelayTransaction(orphanTx, orphanTxHash);
                        mapAlreadyAskedFor.erase(CInv(MSG_TX, orphanTxHash));
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
        if (tx.nDoS) pfrom->Misbehaving(tx.nDoS);
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

        if (ProcessBlock(pfrom, &block, false))
        {
            mapAlreadyAskedFor.erase(inv);
            pfrom->nTrust++;
        }
        if (block.nDoS)
        {
                pfrom->Misbehaving(block.nDoS);
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

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
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
        if (pfrom->nSendSize >= SendBufferSize())
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
        uint256 hash = Hash(Span<std::byte>{(std::byte*)&vRecv.begin()[0], nMessageSize});

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
bool SendMessages(CNode* pto, bool fSendTrickle)
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
            GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
        }
        pto->fPingQueued = false;
        pto->nPingUsecStart = GetTimeMicros();
        pto->nPingNonceSent = nonce;

        pto->PushMessage(NetMsgType::PING, nonce);
    }

    // Resend wallet transactions that haven't gotten in a block yet
    ResendWalletTransactions();

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
        if (mapAlreadyAskedFor.find(inv) == mapAlreadyAskedFor.end())
        {
            pto->mapAskFor.erase(pto->mapAskFor.begin());
            continue;
        }

        bool fAlreadyHave = AlreadyHave(txdb, inv);

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

            mapAlreadyAskedFor[inv] = nNow;
        }
        pto->mapAskFor.erase(pto->mapAskFor.begin());
    }
    if (!vGetData.empty())
        pto->PushMessage(NetMsgType::GETDATA, vGetData);

    return true;
}

GRC::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex)
{
    CBlock block;

    if (!pblockindex || !pblockindex->IsInMainChain()
        || !ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
    {
        return std::nullopt;
    }

    return block.PullClaim();
}

GRC::MintSummary CBlock::GetMint() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    CTxDB txdb("r");
    GRC::MintSummary mint;

    for (const auto& tx : vtx) {
        const CAmount tx_amount_out = tx.GetValueOut();

        if (tx.IsCoinBase()) {
            mint.m_total += tx_amount_out;
            continue;
        }

        CAmount tx_amount_in = 0;

        for (const auto& input : tx.vin) {
            CTransaction input_tx;

            if (txdb.ReadDiskTx(input.prevout.hash, input_tx)) {
                tx_amount_in += input_tx.vout[input.prevout.n].nValue;
            }
        }

        if (tx.IsCoinStake()) {
            mint.m_total += tx_amount_out - tx_amount_in;
        } else {
            mint.m_fees += tx_amount_in - tx_amount_out;
        }
    }

    return mint;
}

GRC::MRCFees CBlock::GetMRCFees() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    GRC::MRCFees mrc_fees;
    unsigned int mrc_output_limit = GetMRCOutputLimit(nVersion, false);

    // Return zeroes for mrc fees if MRC not allowed. (This could have also been done
    // by block version check, but this is more correct.)
    if (!mrc_output_limit) {
        return mrc_fees;
    }

    Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

    const GRC::Claim claim = GetClaim();

    CAmount mrc_total_fees = 0;

    // This is similar to the code in CheckMRCRewards in the Validator class, but with the validation removed because
    // the block has already been validated. We also only need the MRC fee calculation portion.
    for (const auto& tx: vtx) {
        for (const auto& mrc : claim.m_mrc_tx_map) {
            if (mrc.second == tx.GetHash() && !tx.GetContracts().empty()) {
                // An MRC contract must be the first and only contract on a transaction by protocol.
                GRC::Contract contract = tx.GetContracts()[0];

                if (contract.m_type != GRC::ContractType::MRC) continue;

                GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                mrc_fees.m_mrc_minimum_calc_fees += mrc.ComputeMRCFee();

                mrc_total_fees += mrc.m_fee;
                mrc_fees.m_mrc_foundation_fees += mrc.m_fee * foundation_fee_fraction.GetNumerator()
                                                            / foundation_fee_fraction.GetDenominator();
            }
        }
    }

    mrc_fees.m_mrc_staker_fees = mrc_total_fees - mrc_fees.m_mrc_foundation_fees;

    return mrc_fees;
}
