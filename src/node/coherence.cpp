// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/coherence.h"

#include "chainparams.h"
#include "dbwrapper.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/registry.h"
#include "main.h"
#include "node/blockstorage.h"
#include "util.h"

namespace GRC {

CoherenceResult VerifyChainCoherence(int max_walkback)
{
    AssertLockHeld(cs_main);

    CoherenceResult result;
    result.pindex_consistent = pindexBest;

    if (!pindexBest) {
        // Empty chain (fresh datadir before genesis insert). Nothing to verify.
        return result;
    }

    int walked = 0;
    CBlock block;

    for (CBlockIndex* pindex = pindexBest; pindex; pindex = pindex->pprev) {
        // Read with fReadTransactions=false: the ReadBlockFromDisk(pindex)
        // overload still computes block.GetHash() (header hash) and compares
        // against pindex->GetBlockHash() internally. A true return means the
        // on-disk data hashes to what the index claims for this block --
        // i.e. coherent. A false return means either the read failed (file
        // missing, partial record) or the hash did not match (Scenario C
        // from issue #2865); either way we treat it as corruption at this
        // height and keep walking backward.
        if (ReadBlockFromDisk(block, pindex, Params().GetConsensus(),
                              /*fReadTransactions=*/false)) {
            result.pindex_consistent = pindex;
            return result;
        }

        // Inconsistent at this height. Record SB crossings as we go so the
        // caller can decide whether the rewind crosses an SB boundary
        // (which mandates a beacon registry rebuild rather than a clamp;
        // see doc/block_corruption_recovery_design.md).
        if (pindex->IsSuperblock()) {
            ++result.sb_cross_count;
        }

        LogPrintf("WARN: %s: block at height %d (hash %s, blk%05u.dat:%u) failed coherence check; walking back.",
                  __func__, pindex->nHeight, pindex->GetBlockHash().GetHex(),
                  pindex->nFile, pindex->nBlockPos);

        ++walked;
        if (walked >= max_walkback) {
            result.exhausted = true;
            return result;
        }

        if (!pindex->pprev) {
            // Walked off genesis without finding a consistent block. Treat
            // as exhausted -- requires manual -reindex.
            result.exhausted = true;
            return result;
        }
    }

    // Fell out of the loop -- shouldn't happen because we always either
    // return on coherence, return on exhaustion, or return on no pprev.
    // Defensive: mark exhausted so caller surfaces an error.
    result.exhausted = true;
    return result;
}

bool RewindToConsistentTip(CBlockIndex* pindex_target)
{
    AssertLockHeld(cs_main);
    assert(pindex_target != nullptr);

    // The actual chain-state mutation (in-memory globals + LevelDB hashBestChain) lives
    // in main.cpp's AbandonChainTo so it can use the file-local g_chain_trust and
    // UpdateSyncTime alongside the existing DisconnectBlocksBatch logic.
    CTxDB txdb;
    return AbandonChainTo(pindex_target, txdb);
}

bool RunStartupCoherenceRecovery()
{
    AssertLockHeld(cs_main);

    if (gArgs.GetBoolArg("-reindex", false)) {
        // User is already rebuilding; the walk would be redundant.
        LogPrintf("INFO: %s: -reindex set; skipping coherence walk.", __func__);
        return true;
    }

    const int max_walkback = gArgs.GetArg("-coherencewalkmax", DEFAULT_COHERENCE_WALK_MAX);
    LogPrintf("INFO: %s: verifying chain coherence (walkback bound %d).", __func__, max_walkback);

    CoherenceResult result = VerifyChainCoherence(max_walkback);

    if (result.exhausted) {
        LogPrintf("ERROR: %s: chain coherence walk hit the %d-block bound without finding a consistent block. "
                  "The datadir has corruption beyond the automatic-recovery window. Please restart with -reindex.",
                  __func__, max_walkback);
        return false;
    }

    if (result.pindex_consistent == pindexBest) {
        // Common case: first block (the tip) was coherent. No rewind needed.
        LogPrintf("INFO: %s: chain tip at height %d is coherent. No rewind needed.",
                  __func__, nBestHeight);
        return true;
    }

    LogPrintf("INFO: %s: detected inconsistency past height %d. Last consistent block is at height %d "
              "(%d superblocks crossed in the abandoned range).",
              __func__, result.pindex_consistent->nHeight, result.pindex_consistent->nHeight,
              result.sb_cross_count);

    if (!RewindToConsistentTip(result.pindex_consistent)) {
        LogPrintf("ERROR: %s: rewind to height %d failed; cannot continue.",
                  __func__, result.pindex_consistent->nHeight);
        return false;
    }

    // Reconcile registry state with the rewound chain.
    //
    // The four non-beacon registries (project, protocol, scraper, sidestake)
    // get the regular clamp via RegistryBookmarks::UpdateRegistryBlockHeights().
    // This is correct because:
    //   - RegistryDB::Initialize() (registry_db.h:76) loads ALL entries for
    //     its key_type from LevelDB regardless of height_stored -- the read
    //     at line 139 is unfiltered, so phantom entries from blocks past
    //     the rewound tip WILL load into m_historical and active maps.
    //   - What the clamp actually does is bring height_stored down so that
    //     the ApplyContracts skip check at contract.cpp:535-547
    //         if (db_height && pindex->nHeight < *db_height) skip
    //     no longer short-circuits the re-arriving blocks. Without the
    //     clamp, every contract in the re-download window would be silently
    //     dropped because the stale bookmark is higher than every block in
    //     range.
    //   - The phantom entries are harmless in the common case (canonical
    //     chain re-supply): same transaction hashes, same contract payloads
    //     -> registry inserts key by hash -> idempotent overwrite, no
    //     divergence, no new dead bytes. In the rare reorg case they orphan
    //     in m_historical / LevelDB as inert weight.
    //
    // The beacon registry is treated specially when the rewind crosses one
    // or more superblock boundaries. Beacons have a two-step lifecycle that
    // ties their in-memory state to superblock commits (ActivatePending) and
    // to the EXPIRED_PENDING set that is recomputed on every SB. The
    // existing Deactivate path is only fidelity-correct within a single SB
    // interval (~960 blocks) per the limitation acknowledged at
    // beacon.cpp:1265-1273. Phase 2 abandonment doesn't call Deactivate at
    // all, so any SB crossing leaves m_expired_pending and the activation
    // composite-hash entries in an inconsistent state with respect to the
    // re-arriving chain. Rather than tolerate that drift, we Reset() the
    // beacon registry entirely; the subsequent GRC::Initialize ->
    // InitializeContracts -> ApplyContracts pass will replay beacon
    // contracts from V11_height forward and rebuild the in-memory and
    // LevelDB state from scratch. Cost is a few minutes of chain walk;
    // benefit is correctness regardless of how many SBs were crossed.
    //
    // The order below matters: Reset() FIRST so the beacon registry's
    // GetDBHeight() returns 0, THEN UpdateRegistryBlockHeights() which
    // skips the beacon registry naturally (0 > target is false).
    if (result.sb_cross_count > 0) {
        LogPrintf("INFO: %s: rewind crossed %d superblock(s); resetting beacon registry for full replay.",
                  __func__, result.sb_cross_count);
        GetBeaconRegistry().Reset();
    }

    RegistryBookmarks bookmarks;
    int target_height = result.pindex_consistent->nHeight;
    bookmarks.UpdateRegistryBlockHeights(target_height);

    LogPrintf("INFO: %s: registry bookmarks reconciled to height %d. Phase 2 recovery complete; "
              "P2P sync will re-supply blocks past the rewound tip.",
              __func__, target_height);

    return true;
}

} // namespace GRC
