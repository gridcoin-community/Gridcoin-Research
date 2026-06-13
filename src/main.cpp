// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "consensus/tx_verify.h"
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
CCriticalSection cs_tx_val_commit_to_disk;

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
CBlockIndex* pindexGenesisBlock GUARDED_BY(cs_main) = nullptr;
int nBestHeight GUARDED_BY(cs_main) = -1;

uint256 hashBestChain GUARDED_BY(cs_main);
CBlockIndex* pindexBest GUARDED_BY(cs_main) = nullptr;
std::atomic<int64_t> g_previous_block_time;
std::atomic<int64_t> g_nTimeBestReceived;
std::atomic<bool> g_reorg_in_progress = false;
CMedianFilter<int> cPeerBlockCounts GUARDED_BY(cs_main) {5, 0}; // Amount of blocks that other nodes claim to have




// Orphan block storage managed by g_orphan_blocks (node/orphan_blocks.h)


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
std::atomic<bool> bGridcoinCoreInitComplete{false};

// Mining status variables
CCriticalSection cs_msMiningErrors;
std::string msMiningErrors GUARDED_BY(cs_msMiningErrors);

//When syncing, we grandfather block rejection rules up to this block, as rules became stricter over time and fields changed
int nGrandfather = 1034700;

bool fEnforceCanonical = true;
bool fUseFastIndex = false;

// Temporary block version 11 transition helpers:
int64_t g_v11_timestamp = 0;

// End of Gridcoin Global vars

GRC::SeenStakes g_seen_stakes GUARDED_BY(cs_main);
GRC::ChainTrustCache g_chain_trust GUARDED_BY(cs_main);

//!
//! \brief Re-exports chain trust values for reporting.
//!
arith_uint256 GetChainTrust(const CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
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

// Canonical lock order: cs_main -> cs_setpwalletRegistered -> cs_wallet.
// Each wrapper iterates setpwalletRegistered (GUARDED_BY cs_setpwalletRegistered)
// and dispatches into pwallet methods that take pwallet->cs_wallet. Callers
// MUST hold cs_setpwalletRegistered before invoking these wrappers; this is
// enforced by EXCLUSIVE_LOCKS_REQUIRED. Holding the lock at the call site
// (rather than taking it inside the wrapper) lets callers that also hold
// cs_wallet establish the canonical order at acquisition time and avoids
// the cs_setpwalletRegistered <-> cs_wallet inversion the inside-lock
// pattern would otherwise create.


// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
    EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->EraseFromWallet(hash);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
    EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

// notify wallets about an updated transaction
void UpdatedTransaction(const uint256& hashTx)
    EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
    EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->PrintWallet(block);
}


// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_setpwalletRegistered)
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


bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx, CValidationState& state, bool* pfMissingInputs) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    // Mandatory switch to binary contracts (tx version 2):
    if (tx.nVersion < 2) {
        return state.DoS(100, error("AcceptToMemoryPool : legacy transaction"));
    }

    if (!CheckTransaction(tx, state))
        return error("AcceptToMemoryPool : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, error("AcceptToMemoryPool : coinbase as individual tx"));

    // ppcoin: coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return state.DoS(100, error("AcceptToMemoryPool : coinstake as individual tx"));

    // Rather not work on nonstandard transactions
    if (!IsStandardTx(tx))
        return error("AcceptToMemoryPool : nonstandard transaction type");

    // Perform contextual validation for any contracts:

    int DoS = 0;
    if (!tx.GetContracts().empty() && !GRC::ValidateContracts(tx, DoS)) {
        return state.DoS(DoS, error("%s: invalid contract in tx %s, assigning DoS misbehavior of %i",
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
                            state.DoS(25, error("%s: MRC contract in tx %s has the same CPID as an existing transaction "
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
        if (!FetchInputs(tx, state, txdb, mapUnused, false, false, mapInputs, fInvalid))
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
        if (!tx.GetContracts().empty() && !CheckContracts(tx, state, mapInputs, pindexBest->nHeight)) {
            return false;
        }

        // BIP68: Check relative lock-time (sequence locks) when v14 is active.
        if (IsV14Enabled(nBestHeight + 1)) {
            if (!CheckSequenceLocks(tx, 0, pindexBest)) {
                return error("AcceptToMemoryPool : sequence locks not satisfied for tx %s",
                             hash.ToString().substr(0, 10).c_str());
            }
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!ConnectInputs(tx, state, txdb, mapInputs, mapUnused, CDiskTxPos(1,1,1), pindexBest, false, false))
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

    // Notify registered wallets that transaction was added to mempool
    // This enables incoming transactions to appear immediately with 0 confirmations
    {
        LOCK(cs_setpwalletRegistered);
        CTransactionRef ptx = MakeTransactionRef(tx);
        for (auto const& pwallet : setpwalletRegistered)
        {
            pwallet->transactionAddedToMempool(ptx);
        }
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
    {
        LOCK(cs_setpwalletRegistered);
        EraseFromWallets(ptxOld->GetHash());
    }

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
    AssertLockHeld(cs_main);

    if (hashBlock.IsNull() || nIndex == -1)
        return 0;

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
    CValidationState state;
    return ::AcceptToMemoryPool(mempool, *this, state, nullptr);
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

void static InvalidChainFound(CBlockIndex* pindexNew) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
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
    // representation in the block to speed up subsequent access:
    //
    if (m_claim_contract_cache.m_type == GRC::ContractType::UNKNOWN) {
        m_claim_contract_cache = GRC::MakeContract<GRC::Claim>(
            GRC::ContractAction::ADD,
            GRC::Claim::Parse(vtx[0].hashBoinc, nVersion));
    }

    return *m_claim_contract_cache.SharePayloadAs<GRC::Claim>();
}

GRC::Claim CBlock::PullClaim()
{
    if (nVersion >= 11 || !vtx[0].vContracts.empty()) {
        // PullPayloadAs operates on the shared_ptr within the Contract,
        // not on the vector element itself, so const vContracts is fine.
        return vtx[0].vContracts[0].CopyPayloadAs<GRC::Claim>();
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

    GRC::RegistryBookmarks registries;

    // Count superblocks crossed during this disconnect. BeaconRegistry::Deactivate
    // is called per-SB below and is fidelity-correct for the SINGLE most recent SB
    // (it uses m_expired_pending which only carries the latest SB's expired set --
    // see beacon.cpp:1265-1273). When the disconnect crosses 2+ SBs, expired-pending
    // beacons from the prior SBs are lost; the recovery is an in-line beacon
    // registry rebuild after the disconnect loop completes (call site below at the
    // sb_cross_count >= 2 check). The full rationale for in-line vs deferred
    // rebuild and why single-SB reorgs are NOT rebuilt lives at that call site.
    int sb_cross_count = 0;

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
            // for a superblock. We call GetBeaconRegistry directly here, because the IHandler class does not have
            // a virtual method that corresponds to this call, as it is only relevant to beacons.
            GRC::GetBeaconRegistry().Deactivate(pindexBest->GetBlockHash());

            // Count it so we can decide after the loop whether to flag a beacon-registry rebuild
            // for the next startup (the Deactivate-via-m_expired_pending path is only correct
            // for one SB at a time; see the comment above the loop).
            ++sb_cross_count;

            GRC::Quorum::PopSuperblock(pindexBest);
            GRC::Quorum::LoadSuperblockIndex(pindexBest->pprev);

            if (pindexBest->nVersion >= 11 && !GRC::Tally::RevertSuperblock()) {
                return false;
            }
        }

        if (pindexBest->nHeight > nGrandfather && pindexBest->nVersion <= 10) {
            GRC::Quorum::ForgetVote(pindexBest);
        }

        // Delete beacons, scraper entries, protocol entries, projects, polls and votes from contracts
        // in disconnected blocks.
        if (pindexBest->IsContract())
        {
            // Skip coinbase and coinstake transactions:
            for (auto tx = std::next(block.vtx.begin(), 2), end = block.vtx.end();
                tx != end;
                ++tx)
            {
                // This reverts contracts for those contract types which have handlers that properly handle
                // contract level reversions.
                for (const auto& contract : tx->GetContracts())
                {
                    if (GRC::RegistryBookmarks::IsRegistryRevertCapable(contract.m_type.Value())) {
                        const GRC::ContractContext contract_context(contract, *tx, pindexBest);

                        GRC::RegistryBookmarks::GetRegistryWithRevert(contract.m_type.Value()).Revert(contract_context);
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
        for( CTransaction& tx : vResurrect) {
            CValidationState resurrect_state;
            AcceptToMemoryPool(mempool, tx, resurrect_state, nullptr);
        }

        if (!txdb.TxnCommit())
            return error("DisconnectBlocksBatch: TxnCommit failed"); /*fatal*/

        // Record new best height (the common block) in the registries that have a backing DB. This is important
        // to ensure that if the wallet is shutdown, on the next start, the contract replay (if any) is done from
        // the correct height.
        registries.UpdateRegistryBlockHeights(pindexBest->nHeight);

        // Replaying contracts after a block disconnection is no longer needed, as all contract types that have handlers
        // that operate at the tx/contract level have fully implemented reversion.
        //GRC::ReplayContracts(pindexBest);

        // Tally research averages.
        if(IsV9Enabled_Tally(nBestHeight) && !IsV11Enabled(nBestHeight)) {
            assert(GRC::Tally::IsLegacyTrigger(nBestHeight));
            GRC::Tally::LegacyRecount(pindexBest);
        }

        // If the reorg crossed two or more superblock boundaries, the per-SB Deactivate calls
        // above could not fully resurrect expired-pending beacons from the prior SBs
        // (m_expired_pending only carries the most recent SB's set -- the limitation
        // acknowledged at beacon.cpp:1265-1273). Rebuild the beacon registry in-line
        // BEFORE returning to the reconnect side of ReorganizeChain: the rebuild walks
        // from V11_height to the current (common-ancestor) tip, leaving the registry
        // correctly reflecting that ancestor's state, and the subsequent per-block
        // Activate() calls on the reconnect side then bring the registry up to the
        // new tip.
        //
        // Doing this in-line (rather than deferring to a flag picked up on next
        // startup) trades a few seconds of frozen-wallet time for closing the fork
        // window: with the deferred approach, any block validated between this reorg
        // and the next restart could consult the broken registry and diverge from
        // healthy peers. In-line rebuild is bounded by the chain walk from V11 to
        // tip, which is dominated by block-index iteration -- on SSD typically
        // single-digit seconds. See doc/block_corruption_recovery_design.md.
        //
        // Single-SB reorgs are NOT rebuilt: the existing Deactivate path is
        // fidelity-correct for that case and the rebuild would be wasted work on
        // the most common case.
        if (sb_cross_count >= 2) {
            LogPrintf("WARN: %s: reorg disconnected %d superblock(s); beacon registry expired-pending "
                      "fidelity is degraded for the prior SB(s). Rebuilding beacon registry in-line "
                      "to maintain consensus.",
                      __func__, sb_cross_count);
            GRC::RebuildBeaconRegistry();
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

        {
            // This lock protects the time period between the GridcoinConnectBlock, which also connects validated transaction
            // contracts and causes contract handlers to fire, and the committing of the txindex changes to disk. Any contract
            // handlers that generate signals whose downstream handlers make use of transaction data on disk via leveldb (txdb)
            // on another thread need to take this lock to ensure that the write to leveldb and the access of the transaction data
            // by the signal handlers is appropriately serialized.
            LOCK(cs_tx_val_commit_to_disk);
            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: cs_tx_val_commit_to_disk locked", __func__);

            if (pindexGenesisBlock == nullptr) {
                if (hash != (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet)) {
                    txdb.TxnAbort();
                    return error("%s: genesis block hash does not match", __func__);
                }

                pindexGenesisBlock = pindex;
            } else {
                assert(pindex->GetBlockHash()==block.GetHash(true));
                assert(pindex->pprev == pindexBest);

                CValidationState connect_state;
                if (!ConnectBlock(block, connect_state, txdb, pindex, false)) {
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

            LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: cs_tx_val_commit_to_disk unlocked", __func__);
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
        // Canonical order: cs_main (held by SetBestChain) -> cs_setpwalletRegistered -> cs_wallet.
        LOCK(cs_setpwalletRegistered);
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

bool GridcoinServices() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
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
    LOCK2(cs_main, cs_vNodes);
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

bool ProcessBlock(CNode* pfrom, CBlock* pblock, bool generated_by_me, CValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    // Check for duplicate
    uint256 hash = pblock->GetHash(true);
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().c_str());
    if (g_orphan_blocks.Contains(hash))
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
    if (!CheckBlock(*pblock, state, pindexBest->nHeight + 1))
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
                && !g_orphan_blocks.HasChildrenOf(hash))
            {
                return error(
                    "%s: ignored duplicate proof-of-stake for orphan %s",
                    __func__,
                    hash.ToString());
            } else {
                g_seen_stakes.RememberOrphan(pblock->vtx[1]);
            }
        }

        if (!g_orphan_blocks.Add(hash, *pblock, GetTime())) {
            return true;
        }

        // Ask this guy to fill in what we're missing
        const CBlock* pblock_root = g_orphan_blocks.GetRootBlock(hash);

        if (pblock_root) {
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
                {
                    LOCK(cs_mapAlreadyAskedFor);
                    mapAlreadyAskedFor[ancestor_request] = 0;
                }
                pfrom->AskFor(ancestor_request);
            }
        }

        return true;
    }

    // Store to disk
    if (!AcceptBlock(*pblock, state, generated_by_me))
        return error("ProcessBlock() : AcceptBlock FAILED");

    // Recursively process any orphan blocks that depended on this one.
    // ProcessQueue handles BFS traversal and SeenStakes cleanup internally.
    // ProcessQueue is EXCLUSIVE_LOCKS_REQUIRED(cs_main) so the lambda runs
    // with cs_main held, but TSA cannot propagate that into the lambda
    // body — suppress the analyzer here rather than tag every chain-state
    // access AcceptBlock makes downstream.
    g_orphan_blocks.ProcessQueue(hash, [&](CBlock& orphan) NO_THREAD_SAFETY_ANALYSIS -> bool {
        CValidationState orphan_state;
        return AcceptBlock(orphan, orphan_state, generated_by_me);
    });

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

bool AbandonChainTo(CBlockIndex* pindex_target, CTxDB& txdb) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    assert(pindex_target != nullptr);

    // No early return when pindex_target == pindexBest. In the ghost-only
    // case (a prior Phase 2 rewound the chain in memory and on hashBestChain
    // but failed to persist pindex_target's hashNext=null), pindexBest is
    // already the target but pindex_target->pnext still points at the first
    // ghost (because LoadBlockIndex rebuilt that linkage from the stale
    // on-disk CDiskBlockIndex.hashNext). We still need to (a) clear pnext
    // in memory and (b) persist the new CDiskBlockIndex with hashNext=null
    // so the next restart doesn't reconstruct the same ghost chain.

    if (pindex_target == pindexBest) {
        LogPrintf("INFO: %s: in-memory tip already at target %s @ %d; persisting "
                  "hashNext=null to clear ghost linkage.", __func__,
                  pindex_target->GetBlockHash().GetHex(), pindex_target->nHeight);
    } else {
        LogPrintf("INFO: %s: abandoning chain tip %s @ %d down to %s @ %d.", __func__,
                  pindexBest->GetBlockHash().GetHex(), nBestHeight,
                  pindex_target->GetBlockHash().GetHex(), pindex_target->nHeight);
    }

    // Sever the abandoned range from the in-memory tree by clearing the new tip's pnext.
    // The abandoned CBlockIndex entries are removed from mapBlockIndex separately by
    // PurgeOrphanedBlockIndexEntries (called from the Phase 2 recovery hook after this
    // function returns and CTxDB::CleanAbandonedRange has completed the chainstate
    // rollback). We do not erase them here because the caller still needs the abandoned
    // CBlockIndex* values for the surgical cleanup pass.
    //
    // Asymmetric linkage note: we null pindex_target->pnext (forward linkage from the
    // new tip into the abandoned range), but we deliberately do NOT walk the abandoned
    // range and null each entry's pprev. The abandoned blocks still point pprev back
    // toward pindex_target until PurgeOrphanedBlockIndexEntries removes them entirely
    // a few steps later. This is benign because:
    //   - The Phase 2 recovery hook holds cs_main and runs at init-time, before
    //     wallet/Quorum/Tally/mempool/net start, so no live consumer is walking pprev
    //     from an abandoned CBlockIndex* during the in-between window.
    //   - The caller (RunStartupCoherenceRecovery) needs those abandoned entries'
    //     pprev intact briefly for any diagnostic code that might iterate them (e.g.
    //     a debug LogPrintf could call IsSuperblock() which uses pprev).
    // If a future code path adds runtime use of this rewind primitive (outside the
    // init-time hook), it must either null pprev on each abandoned entry here OR
    // ensure no consumer walks pprev from the abandoned range.
    pindex_target->pnext = nullptr;

    pindexBest = pindex_target;
    nBestHeight = pindex_target->nHeight;
    hashBestChain = pindex_target->GetBlockHash();
    g_chain_trust.SetBest(pindex_target);
    UpdateSyncTime(pindex_target);

    if (!txdb.WriteHashBestChain(pindex_target->GetBlockHash())) {
        return error("%s: WriteHashBestChain failed for %s", __func__,
                     pindex_target->GetBlockHash().GetHex());
    }

    // Persist the new tip's CDiskBlockIndex so its on-disk hashNext is null.
    // Without this, the next LoadBlockIndex would rebuild pindex_target->pnext
    // from the stale CDiskBlockIndex.hashNext stored when the chain was longer,
    // and the ghost chain would resurrect on every restart. The serialized
    // CDiskBlockIndex captures pnext via GetBlockHash on the in-memory pnext
    // (block.h CDiskBlockIndex.hashNext init) -- which we just nulled above,
    // so this write captures the severed state.
    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex_target))) {
        return error("%s: WriteBlockIndex failed for %s (could not persist severed "
                     "hashNext; ghost chain would resurrect on next restart)", __func__,
                     pindex_target->GetBlockHash().GetHex());
    }

    if (!txdb.Sync()) {
        return error("%s: CTxDB::Sync failed after abandonment", __func__);
    }

    return true;
}

void PurgeOrphanedBlockIndexEntries(CTxDB& txdb, std::vector<CBlockIndex*>& abandoned)
{
    AssertLockHeld(cs_main);

    // The abandoned vector is what VerifyChainCoherence collected: every block
    // past the last consistent one. After the chainstate cleanup, none of these
    // are reachable from pindexBest's ancestry. We need to:
    //
    //   (1) erase each entry from mapBlockIndex so AddToBlockIndex can re-add
    //       the same hashes when P2P delivers the canonical-chain blocks back
    //       (otherwise the "already exists" guard at validation.cpp:1109 would
    //       silently reject every re-supplied block forever);
    //
    //   (2) erase each entry's CDiskBlockIndex record from LevelDB so the next
    //       LoadBlockIndex doesn't rebuild the ghost forward linkage from the
    //       stale hashNext fields stored when the chain was longer. (Without
    //       this, Phase 2 looks successful in this run but the recovered tip
    //       resurrects the ghost chain on every subsequent boot -- the
    //       backward walk in VerifyChainCoherence finds the tip coherent,
    //       early-returns, and DisconnectBlocksBatch then trips
    //       `assert(!pindexBest->pnext)` on the first P2P-delivered block.
    //       Hit 2026-05-16 on isolated testnet slot 10.)
    //
    // DO NOT call `delete` on these pointers. CBlockIndex objects are allocated
    // from GRC::BlockIndexPool (see src/gridcoin/block_index.h), which is
    // backed by std::array<CBlockIndex, CHUNK_SIZE> in a forward_list of
    // chunks. The pool's explicit design (per the class comment at
    // block_index.h:54-55) is that "the application never removes or destroys
    // block index entries"; it has no recycling path. Calling `delete` on a
    // pointer into the middle of a std::array invokes undefined behavior:
    // operator delete tries to free a heap-managed chunk at that address, but
    // the address is not a heap allocation -- in practice it corrupts the C++
    // runtime's free list, manifesting much later as bad_alloc or assertion
    // failures in unrelated code.
    //
    // (We learned this the hard way during the 2026-05-16 isolated-testnet
    // Tier 3 run: Phase 2 reported clean completion, but the very first
    // P2P-delivered block tripped `assert(!pindexBest->pnext)` in
    // DisconnectBlocksBatch because heap corruption from the bogus `delete`
    // calls had overwritten pindex_target's pnext slot with garbage that
    // happened to dereference as a valid-looking CBlockIndex pointer. See
    // .claude/memory/reference_block_index_pool.md.)
    //
    // The cost of not freeing is that the abandoned pool slots remain
    // allocated forever -- a small permanent leak per recovery event, at most
    // a few hundred bytes per abandoned block (well under 1 MB even at the
    // -coherencewalkmax cap). The pool is designed for this. The alternative
    // would be a much larger redesign of BlockIndexPool to support per-object
    // recycling, which is not justified for a rare recovery path.
    unsigned int leveldb_erased = 0;
    unsigned int map_erased = 0;
    for (CBlockIndex*& p : abandoned) {
        if (!p) continue;
        const uint256 hash = p->GetBlockHash();
        if (txdb.EraseBlockIndex(hash)) {
            ++leveldb_erased;
        } else {
            LogPrintf("WARN: %s: EraseBlockIndex failed for %s; on-disk record may persist.",
                      __func__, hash.GetHex());
        }
        mapBlockIndex.erase(hash);
        ++map_erased;
        // No `delete p` -- see comment above.
        p = nullptr;
    }

    if (!txdb.Sync()) {
        LogPrintf("WARN: %s: CTxDB::Sync failed after CDiskBlockIndex erase; will be retried "
                  "on the next durable write.", __func__);
    }

    LogPrintf("INFO: %s: purged %u orphaned block index entries (%u from mapBlockIndex, "
              "%u CDiskBlockIndex records from LevelDB; pool slots remain allocated, "
              "see block_index.h).",
              __func__, (unsigned) abandoned.size(), map_erased, leveldb_erased);
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

        CMutableTransaction txNew;
        //GENESIS TIME
        txNew.nVersion = 1;
        txNew.nTime = 1413033777;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 0 << CScriptNum(42) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        CBlock block;
        block.vtx.push_back(CTransaction(txNew));
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
        { CValidationState genesis_state; assert(CheckBlock(block, genesis_state, 1)); }

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

        {
            // PrintBlockTree is EXCLUSIVE_LOCKS_REQUIRED(cs_main); add the
            // wallet-registry lock here in the canonical order before dispatch.
            LOCK(cs_setpwalletRegistered);
            PrintWallets(block);
        }

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
                    CValidationState load_state;
                    if (ProcessBlock(nullptr, &block, false, load_state)) {
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

extern CCriticalSection cs_mapAlerts;
extern map<uint256, CAlert> mapAlerts GUARDED_BY(cs_mapAlerts);

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
