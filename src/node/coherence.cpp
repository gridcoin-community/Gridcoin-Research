// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/coherence.h"

#include "chainparams.h"
#include "dbwrapper.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/spam.h"
#include "main.h"
#include "node/blockstorage.h"
#include "node/chainman.h"
#include "util.h"

#include <limits>

extern GRC::SeenStakes g_seen_stakes GUARDED_BY(cs_main);
extern GRC::ChainTrustCache g_chain_trust GUARDED_BY(cs_main);

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
        // Use the (block, nFile, nBlockPos, params, fReadTransactions) overload --
        // the lower-level one that actually touches disk. We deliberately do
        // NOT use ReadBlockFromDisk(block, pindex, ...) with
        // fReadTransactions=false: that overload's no-transactions branch
        // copies the header from the in-memory pindex and returns true
        // without touching disk (blockstorage.cpp:99-104), which would make
        // this entire coherence check a no-op.
        //
        // fReadTransactions=true is mandatory here. With header-only reads, a
        // truncation that lands mid-block (header intact at pindex->nBlockPos
        // but the tx payload past EOF) passes the hash check on the
        // recomputed header and gets declared consistent. Forward sync then
        // appends new blocks past the truncation point, but later validation
        // that reads transactions from inside the half-truncated block (e.g.
        // ReadStakedInput resolving a coinstake's prev_tx via CTxIndex) seeks
        // into the truncated region and deserializes whatever happens to be
        // there. The bogus tx fails VerifySignature in CheckProofOfStakeV8
        // and the chain wedges at the first peer-supplied block whose
        // coinstake input lives in the partially-truncated block. Caught
        // 2026-05-17 on isolated testnet slot 10 mid-Phase-2 PR #2941
        // testing: truncation at 179477059 cut block 2763517 in half;
        // forward sync ran fine for 688 blocks then failed at 2764206
        // because its coinstake input (b8bdb239 in block 2763517) read back
        // as a different tx (17dec1fe) from the truncated tail.
        const bool read_ok = ReadBlockFromDisk(
            block, pindex->nFile, pindex->nBlockPos,
            Params().GetConsensus(), /*fReadTransactions=*/true);

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
    // in AbandonChainTo below (moved here from main.cpp under issue #3125, C4).
    CTxDB txdb;
    return AbandonChainTo(pindex_target, txdb);
}

// NOTE: the previous deferred-rebuild flag mechanism
// (IsBeaconRebuildPending / SetBeaconRebuildPending /
// ClearBeaconRebuildPending) was removed when DisconnectBlocksBatch
// switched to an in-line rebuild via GRC::RebuildBeaconRegistry (see
// gridcoin/gridcoin.{h,cpp}). The deferred approach left a fork window
// between the runtime reorg and the next restart; in-line rebuild closes
// it for the cost of a few seconds of frozen wallet on SSD. See
// doc/block_corruption_recovery_design.md.

bool RunStartupCoherenceRecovery()
{
    AssertLockHeld(cs_main);

    if (gArgs.GetBoolArg("-reindex", false)) {
        // User is already rebuilding; the walk would be redundant.
        LogPrintf("INFO: %s: -reindex set; skipping coherence walk.", __func__);
        return true;
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
        LogPrintf("WARN: %s: invalid -coherencewalkmax=%" PRId64 " (must be in [1, %d]); using default %d.",
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

// Phase 2 recovery primitives moved out of main.cpp (issue #3125, workstream
// C4): these are the chain-abandonment halves of the coherence recovery hook
// above (issue #2865). They live outside namespace GRC because the callers and
// the chain globals they mutate are global-namespace.
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
