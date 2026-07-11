// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/chainman.h"

#include "chainparams.h"
#include "checkpoints.h"
#include "main.h"
#include "net.h"
#include "txdb.h"
#include "node/blockstorage.h"
#include "node/orphan_blocks.h"
#include "node/ui_interface.h"
#include "validation.h"
#include "validationinterface.h"
#include "gridcoin/staking/spam.h"
#include "gridcoin/beacon.h"
#include "gridcoin/gridcoin.h"
#include "gridcoin/mrc.h"
#include "gridcoin/quorum.h"
#include "gridcoin/tally.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/staking/chain_trust.h"
#include "util.h"
#include "util/time.h"
#include "wallet/wallet.h"

#include <boost/range/adaptor/reversed.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <list>
#include <set>
#include <string>

using namespace std;

// Chain-state globals defined elsewhere that the chain-management functions
// reference. Kept where they are to limit churn (issue #3030, A4):
//   g_chain_trust       -> defined in main.cpp (also extern'd in validation.cpp)
//   g_seen_stakes / g_v11_timestamp -> defined in main.cpp (same ad-hoc extern
//                          pattern as node/orphan_blocks.cpp / node/coherence.cpp)
//   g_previous_block_time / g_nTimeBestReceived -> defined in main.cpp, declared in main.h
//   pwalletMain         -> defined in init.cpp
extern GRC::ChainTrustCache g_chain_trust GUARDED_BY(cs_main);
extern GRC::SeenStakes g_seen_stakes GUARDED_BY(cs_main);
extern int64_t g_v11_timestamp;
extern CWallet* pwalletMain;

void UpdateSyncTime(const CBlockIndex* const pindexBest)
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

static bool DisconnectBlocksBatch(CTxDB& txdb, list<CTransaction>& vResurrect, unsigned& cnt_dis, CBlockIndex* pcommon) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
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
        // Resurrect memory transactions that were in the disconnected branch.
        // Note: these are re-accepted but NOT re-added to the unbroadcast set --
        // provenance ("was this ours?") is not tracked here. A locally-originated
        // non-wallet tx (e.g. sendrawtransaction/HTLC) that had briefly confirmed
        // and is then resurrected by a reorg will therefore not be rebroadcast by
        // the unbroadcast machinery; wallet-originated txs remain covered by
        // ResendWalletTransactions. Acceptable given how rare a same-tx reorg is.
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

static bool ReorganizeChain(CTxDB& txdb, unsigned &cnt_dis, unsigned &cnt_con, CBlock &blockNew, CBlockIndex* pindexNew)
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
                const uint256 expected_genesis = Params().IsMockableChain()
                    ? (hashGenesisBlockRegTest.IsNull() ? hash : hashGenesisBlockRegTest)
                    : (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet);
                if (hash != expected_genesis) {
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

            // Only the MRC entries are inspected (via the m_mrc_by_cpid index),
            // not the entire pool. An MRC is stale when its anchor block is no
            // longer the chain head.
            for (const uint256& stale_hash : mempool.GetStaleMRCs(hashBestChain)) {
                CTransaction stale_tx;
                if (mempool.lookup(stale_hash, stale_tx)) {
                    to_be_erased.push_back(stale_tx);
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
        // Persist the wallet best-block locator. Emitted synchronously under
        // cs_main (held by SetBestChain), so the CValidationInterface subscriber
        // (CWallet::ChainStateFlushed) runs in the canonical cs_main -> signals
        // order -- replacing the legacy cs_setpwalletRegistered wrapper (issue
        // #3030 setpwalletRegistered retirement).
        GetMainSignals().ChainStateFlushed(locator);
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
        ReplaceAll(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }
    #endif

    // Notify the validation-signal layer that the chain tip advanced. The UI
    // bridge registered in init.cpp re-emits uiInterface.NotifyBlocksChanged()
    // for the Qt models, so GUI block notifications now flow through the
    // validation interface (issue #3030, workstream B3).
    //
    // pindexFork is passed as nullptr: the true reorg fork point is the common
    // ancestor (pcommon, computed in ReorganizeChain) and is not plumbed up to
    // this layer, so origBestIndex (the previous tip) would be wrong for any
    // non-trivial reorg. No current subscriber reads pindexFork (the UI bridge
    // ignores it), so nullptr is an honest "not supplied" rather than a
    // misleading value; a future fork-point-dependent subscriber (e.g. the
    // deferred PeerManager) should thread pcommon through instead. Tracked in
    // issue #3104.
    GetMainSignals().UpdatedBlockTip(pindexNew, nullptr, fIsInitialDownload);

    return GridcoinServices();
}

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

// Block-arrival orchestration moved out of main.cpp (issue #3125, workstream
// C1): GridcoinServices, AskForOutstandingBlocks and ProcessBlock live next to
// the chain-management functions above, which are their main collaborators.

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
    LOCK(cs_main);

    // Resolve the start block index once under cs_main (issue #2558 PR 9c);
    // it does not depend on the peer, so hoist it out of the per-node loop.
    CBlockIndex* pindexStart = pindexBest;
    if (hashStart != uint256())
    {
        const auto it = mapBlockIndex.find(hashStart);
        if (it == mapBlockIndex.end() || !it->second)
            return error("Unable to find block index %s", hashStart.ToString().c_str());
        pindexStart = it->second;
    }

    // Read the cs_main-guarded height into a local so the iteration callback
    // touches no cs_main-guarded state (keeps -Werror=thread-safety happy).
    const int nBestHeightLocal = nBestHeight;
    int iAsked = 0;
    if (g_connman)
    {
        g_connman->ForEachNodeUnderLock([&](CNode* pNode) {
            // Once 10 nodes have been asked, skip the rest. ForEachNodeUnderLock
            // has no early-exit, so this keeps iterating (cheaply) rather than
            // break-ing as the pre-API loop did -- same set of nodes asked.
            if (iAsked > 10) return;
            if (!pNode->fClient && !pNode->fOneShot && (pNode->nStartingHeight > (nBestHeightLocal - 144)))
            {
                pNode->PushGetBlocks(pindexStart, uint256());
                LogPrintf("Asked for blocks");
                iAsked++;
            }
        });
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
