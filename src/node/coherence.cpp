// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/coherence.h"

#include "chainparams.h"
#include "dbwrapper.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/staking/spam.h"
#include "main.h"
#include "node/blockstorage.h"
#include "util.h"

#include <limits>

extern GRC::SeenStakes g_seen_stakes;

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

    // Phase 1 -- backward walk: find the last coherent tip.
    //
    // We do NOT collect abandoned_indexes/positions here. The Phase 2
    // forward walk below catches everything past pindex_consistent, whether
    // the backward walk found inconsistency (truncation/corruption case) or
    // the in-memory chain has forward ghosts left over from a prior
    // interrupted Phase 2 (the ghost-only case where pindexBest is itself
    // coherent on disk but pindexBest->pnext is non-null because
    // LoadBlockIndex rebuilt the broken pnext chain from on-disk hashNext
    // values that the prior Phase 2 failed to persist). Collecting in the
    // forward walk avoids double-accounting when both conditions coexist.
    int walked = 0;
    CBlock block;

    for (CBlockIndex* pindex = pindexBest; pindex; pindex = pindex->pprev) {
        // Use the (block, nFile, nBlockPos, params, fReadTransactions=false)
        // overload -- the lower-level one that actually touches disk. We
        // deliberately do NOT use ReadBlockFromDisk(block, pindex, ...) with
        // fReadTransactions=false: that overload's no-transactions branch
        // copies the header from the in-memory pindex and returns true
        // without touching disk (blockstorage.cpp:99-104), which would make
        // this entire coherence check a no-op.
        const bool read_ok = ReadBlockFromDisk(
            block, pindex->nFile, pindex->nBlockPos,
            Params().GetConsensus(), /*fReadTransactions=*/false);

        const bool coherent = read_ok && (block.GetHash(true) == pindex->GetBlockHash());

        if (coherent) {
            result.pindex_consistent = pindex;
            break;  // proceed to Phase 2 forward walk
        }

        LogPrintf("WARN: %s: block at height %d (hash %s, blk%05u.dat:%u) failed coherence check "
                  "(read_ok=%s); walking back.",
                  __func__, pindex->nHeight, pindex->GetBlockHash().GetHex(),
                  pindex->nFile, pindex->nBlockPos, read_ok ? "true" : "false");

        ++walked;
        if (walked >= max_walkback) {
            result.exhausted = true;
            result.pindex_consistent = nullptr;
            return result;
        }

        if (!pindex->pprev) {
            // Walked off genesis without finding a consistent block.
            result.exhausted = true;
            result.pindex_consistent = nullptr;
            return result;
        }
    }

    // Phase 2 -- forward walk: enumerate every block past pindex_consistent.
    //
    // Works for both the corruption case AND the ghost case because in both,
    // the pnext linkages from pindex_consistent forward are still intact in
    // mapBlockIndex (either from the initial LoadBlockIndex pass in this
    // run, or because no prior Phase 2 successfully severed them on disk).
    //
    // Each ghost gets cleaned up by the downstream pipeline:
    //   abandoned_positions -> CleanAbandonedRange (CTxIndex / vSpent)
    //   abandoned_indexes   -> PurgeOrphanedBlockIndexEntries (mapBlockIndex
    //                          + LevelDB CDiskBlockIndex)
    //
    // sb_cross_count drives whether the beacon registry must be Reset()
    // rather than clamped (see doc/block_corruption_recovery_design.md).
    std::set<uint256> active_walk;
    for (CBlockIndex* ghost = result.pindex_consistent->pnext; ghost; ghost = ghost->pnext) {
        result.abandoned_indexes.push_back(ghost);
        result.abandoned_positions.insert(PackBlockFilePos(ghost->nFile, ghost->nBlockPos));
        active_walk.insert(ghost->GetBlockHash());
        if (ghost->IsSuperblock()) {
            ++result.sb_cross_count;
        }
    }

    // Phase 2.5 -- catch side-chain CBlockIndex* entries above the consistent
    // tip. pnext only follows the active chain, so a competing-fork block at
    // height > pindex_consistent->nHeight that was AcceptBlock'ed earlier
    // (e.g. during a prior reorg or alt-chain delivery from peers) is invisible
    // to the walk above. Its pprev chain still runs through the abandoned
    // range. If we don't purge it now alongside the active-chain ghosts, the
    // NEXT LoadBlockIndex sees a CDiskBlockIndex whose hashPrev resolves to an
    // entry already purged in this run, leaves pprev null, and CheckBlockIndex
    // trips on the dangling pprev (originally caught 2026-05-17 on isolated
    // testnet slot 10, mid-Phase-2 PR #2941 testing). Position is intentionally
    // NOT added to abandoned_positions: side-chain blocks were never connected
    // so they wrote nothing to CTxIndex / vSpent. SB crossings are also counted
    // strictly from the active walk above, since only active-chain SB
    // activations mutated registry state.
    unsigned int side_chain_purged = 0;
    for (const auto& kv : mapBlockIndex) {
        CBlockIndex* p = kv.second;
        if (p
            && p->nHeight > result.pindex_consistent->nHeight
            && !active_walk.count(kv.first))
        {
            result.abandoned_indexes.push_back(p);
            ++side_chain_purged;
        }
    }
    if (side_chain_purged > 0) {
        LogPrintf("INFO: %s: extending abandonment to %u side-chain index "
                  "entries above consistent tip height %d.",
                  __func__, side_chain_purged, result.pindex_consistent->nHeight);
    }

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

namespace {
//! LevelDB key for the deferred beacon-registry rebuild flag set by the
//! runtime reorg path (DisconnectBlocksBatch) when 2+ SBs are crossed.
const std::pair<std::string, std::string> BEACON_REBUILD_KEY =
    std::make_pair("beacon_db", "needs_rebuild");
} // anonymous namespace

bool IsBeaconRebuildPending()
{
    CTxDB txdb("r");

    // Distinguish "key absent" (no pending rebuild) from "key present but read
    // failed" (treat as pending and log -- we'd rather pay a spurious rebuild
    // than silently skip a needed one). The presence check is cheap because
    // the key is small and the read of pending is at most 1 byte.
    if (!txdb.ExistsGenericSerializable(BEACON_REBUILD_KEY)) {
        return false;
    }

    bool pending = false;
    if (!txdb.ReadGenericSerializable(BEACON_REBUILD_KEY, pending)) {
        LogPrintf("WARN: %s: beacon-rebuild key exists but read failed; treating as pending so the "
                  "rebuild fires on this startup.", __func__);
        return true;
    }
    return pending;
}

void SetBeaconRebuildPending()
{
    CTxDB txdb;
    if (!txdb.WriteGenericSerializable(BEACON_REBUILD_KEY, true)) {
        LogPrintf("WARN: %s: failed to persist beacon-rebuild flag; rebuild will not happen on next restart "
                  "until the flag is set again or -clearallregistryhistory is used.", __func__);
    }
}

void ClearBeaconRebuildPending()
{
    CTxDB txdb;
    // Erase rather than write-false so the key doesn't accumulate as inert
    // dead weight in the DB. Erase is idempotent on a missing key, so we
    // log only on a hard error (which leaves the key as `true` and means
    // the rebuild will fire again on the next restart -- harmless but
    // wasteful, hence the warning).
    auto key = BEACON_REBUILD_KEY;
    if (!txdb.EraseGenericSerializable(key)) {
        LogPrintf("WARN: %s: failed to erase beacon-rebuild flag; rebuild will fire again on the next restart "
                  "(harmless but wasteful).", __func__);
    }
}

bool RunStartupCoherenceRecovery()
{
    AssertLockHeld(cs_main);

    if (gArgs.GetBoolArg("-reindex", false)) {
        // User is already rebuilding; the walk would be redundant.
        // The pending-rebuild flag, if set, will be wiped by the reindex
        // path's own registry clear; safe to skip our handling here.
        LogPrintf("INFO: %s: -reindex set; skipping coherence walk.", __func__);
        return true;
    }

    // Service any deferred beacon-registry rebuild requested by the runtime
    // reorg path (DisconnectBlocksBatch sets the flag on a 2+ SB reorg
    // because Deactivate cannot fully resurrect prior-SB expired-pending
    // beacons -- see beacon.cpp:1265-1273). Reset() wipes both in-memory
    // and LevelDB beacon state; the subsequent GRC::Initialize ->
    // InitializeContracts -> ApplyContracts pass will replay beacon
    // contracts from V11_height forward and rebuild cleanly.
    //
    // If the coherence walk below also detects an SB crossing, it will
    // call Reset() again -- idempotent, harmless.
    if (IsBeaconRebuildPending()) {
        LogPrintf("INFO: %s: pending beacon registry rebuild detected (from prior multi-SB reorg); "
                  "resetting beacon registry now so it will be rebuilt from V11_height on the "
                  "InitializeContracts replay below.", __func__);
        GetBeaconRegistry().Reset();
        ClearBeaconRebuildPending();
    }

    // gArgs.GetArg with an integer default returns int64_t; the user-supplied
    // value comes in as a signed decimal with no validation. A negative or
    // zero value would silently disable the walk (the first non-coherent
    // block trips `walked >= max_walkback` immediately, exhausted=true, and
    // we'd force a -reindex on the user). A value above INT_MAX would
    // truncate weirdly when narrowed to int. Clamp to [1, INT_MAX] and fall
    // back to the default with a warning if the user passed garbage.
    const int64_t raw_walkback = gArgs.GetArg("-coherencewalkmax", DEFAULT_COHERENCE_WALK_MAX);
    int max_walkback;
    if (raw_walkback < 1 || raw_walkback > std::numeric_limits<int>::max()) {
        LogPrintf("WARN: %s: invalid -coherencewalkmax=%d (must be in [1, %d]); using default %d.",
                  __func__, raw_walkback, std::numeric_limits<int>::max(), DEFAULT_COHERENCE_WALK_MAX);
        max_walkback = DEFAULT_COHERENCE_WALK_MAX;
    } else {
        max_walkback = static_cast<int>(raw_walkback);
    }
    LogPrintf("INFO: %s: verifying chain coherence (walkback bound %d).", __func__, max_walkback);

    CoherenceResult result = VerifyChainCoherence(max_walkback);

    if (result.exhausted) {
        LogPrintf("ERROR: %s: chain coherence walk hit the %d-block bound without finding a consistent block. "
                  "The datadir has corruption beyond the automatic-recovery window. Please restart with -reindex.",
                  __func__, max_walkback);
        return false;
    }

    if (result.pindex_consistent == pindexBest && result.abandoned_indexes.empty()) {
        // Common case: tip is coherent on disk AND there are no forward
        // ghosts past the tip. No rewind needed.
        LogPrintf("INFO: %s: chain tip at height %d is coherent. No rewind needed.",
                  __func__, nBestHeight);
        return true;
    }

    // Distinguish in the log:
    //   * pindex_consistent < pindexBest -> backward-walk found inconsistency
    //     (truncation / interrupted write). The tip and possibly several blocks
    //     below it are unreadable on disk.
    //   * pindex_consistent == pindexBest with abandoned_indexes non-empty ->
    //     ghost-only case. The on-disk tip is fine, but LoadBlockIndex
    //     rebuilt forward pnext linkage from CDiskBlockIndex.hashNext values
    //     that a prior Phase 2 failed to persist as null. Either way, the
    //     forward-walk caught everything past the consistent tip and the
    //     pipeline below cleans it up.
    if (result.pindex_consistent != pindexBest) {
        LogPrintf("INFO: %s: detected inconsistency past height %d. Last consistent block is at height %d "
                  "(%d superblocks crossed in the abandoned range, %u blocks abandoned).",
                  __func__, nBestHeight, result.pindex_consistent->nHeight,
                  result.sb_cross_count, (unsigned) result.abandoned_indexes.size());
    } else {
        LogPrintf("INFO: %s: tip at height %d is coherent on disk but %u forward ghost block(s) "
                  "(%d superblock(s) crossed) remain in the in-memory chain from a prior interrupted "
                  "Phase 2. Cleaning up.",
                  __func__, nBestHeight, (unsigned) result.abandoned_indexes.size(),
                  result.sb_cross_count);
    }

    // Step 1: rewind in-memory chain globals + persist hashBestChain.
    if (!RewindToConsistentTip(result.pindex_consistent)) {
        LogPrintf("ERROR: %s: rewind to height %d failed; cannot continue.",
                  __func__, result.pindex_consistent->nHeight);
        return false;
    }

    // Step 2: surgical chainstate cleanup. Delete CTxIndex entries created
    // by abandoned blocks, and clear vSpent[i] markers on surviving entries
    // that point into abandoned blocks. Without this, ConnectInputs on the
    // re-supplied blocks would silently reject the same input as
    // already-spent. See the "Abandonment-style rewind + surgical
    // chainstate cleanup" section of doc/block_corruption_recovery_design.md.
    //
    // The CTxDB handle is held across step 3 as well so the mapBlockIndex
    // purge below can erase CDiskBlockIndex entries through the same DB
    // handle. Both writes commit on destructor scope-exit.
    CTxDB txdb;

    {
        uint64_t entries_deleted = 0, vspent_cleared = 0, entries_scanned = 0;
        if (!txdb.CleanAbandonedRange(result.abandoned_positions,
                                      &entries_deleted, &vspent_cleared, &entries_scanned)) {
            LogPrintf("ERROR: %s: CleanAbandonedRange failed; chainstate may be inconsistent. "
                      "Restart with -reindex.", __func__);
            return false;
        }
        LogPrintf("INFO: %s: chainstate cleanup: scanned %" PRIu64 " CTxIndex entries, "
                  "deleted %" PRIu64 ", cleared %" PRIu64 " vSpent slot(s) across surviving entries.",
                  __func__, entries_scanned, entries_deleted, vspent_cleared);
    }

    // Step 3: purge the abandoned CBlockIndex entries from in-memory
    // mapBlockIndex AND from on-disk LevelDB. The on-disk erase is what
    // makes Phase 2 durable across restarts -- without it, the next
    // LoadBlockIndex would rebuild the same ghost pnext linkages from the
    // stale CDiskBlockIndex.hashNext values and the recovered tip would
    // appear corrupt again on every subsequent boot. Safe at this init-time
    // point because no live consumer holds references (wallet / Quorum /
    // Tally / mempool / net all start later). See PurgeOrphanedBlockIndexEntries
    // doc comment for why we do this here instead of at runtime.
    //
    // NB: PurgeOrphanedBlockIndexEntries nulls each entry in `abandoned_indexes`
    // as it processes it (deliberately, to make accidental reuse of a freed
    // pool slot less likely). After this call returns `result.abandoned_indexes`
    // is a vector of nullptrs -- do not iterate it for any other purpose.
    PurgeOrphanedBlockIndexEntries(txdb, result.abandoned_indexes);

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

    // SeenStakes was populated by LoadBlockIndex's Refill(pindexBest) against
    // the pre-rewind tip, so its 2048-slot proof table still contains kernel
    // proofs from the blocks we just abandoned. Forward-sync from peers will
    // re-supply those same blocks with the same (deterministic) proofs, which
    // would each be rejected by AcceptBlock's duplicate-POS check (see
    // src/validation.cpp ContainsProof()) and pile up as unconnectable
    // orphans. Clear the table and Refill against the new tip so the only
    // remembered proofs are from blocks at or below the rewound height.
    g_seen_stakes.Clear();
    g_seen_stakes.Refill(pindexBest);

    LogPrintf("INFO: %s: registry bookmarks reconciled to height %d. Phase 2 recovery complete; "
              "P2P sync will re-supply blocks past the rewound tip.",
              __func__, target_height);

    return true;
}

} // namespace GRC
