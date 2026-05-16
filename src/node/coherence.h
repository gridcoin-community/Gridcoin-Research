// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_COHERENCE_H
#define GRIDCOIN_NODE_COHERENCE_H

#include "main.h"  // cs_main (required for clang's EXCLUSIVE_LOCKS_REQUIRED thread-safety analyzer)
#include "sync.h"

class CBlockIndex;

namespace GRC {

//! Default cap on how far backward VerifyChainCoherence will walk before
//! giving up. Sized well above Phase 1's 5000-block IBD fsync interval so
//! atypical fsync skips still have headroom. Overridable via -coherencewalkmax.
constexpr int DEFAULT_COHERENCE_WALK_MAX = 10000;

//! Result of a startup chain-coherence verification.
struct CoherenceResult {
    //! Pointer to the highest block whose on-disk data hash matches its
    //! CBlockIndex hash. In the common case (no corruption) this equals
    //! pindexBest. Never nullptr when status is OK.
    CBlockIndex* pindex_consistent {nullptr};
    //! Number of superblocks encountered between the original pindexBest
    //! (inclusive) and pindex_consistent (exclusive). Greater than zero
    //! indicates a rewind that crosses an SB boundary, which requires the
    //! beacon registry to be rebuilt from scratch instead of clamped.
    //! See doc/block_corruption_recovery_design.md.
    int sb_cross_count {0};
    //! True iff the walk hit -coherencewalkmax without finding a consistent
    //! block. When set, pindex_consistent is unchanged from input and the
    //! caller should refuse to start (the user must -reindex).
    bool exhausted {false};
};

//! Walk backward from pindexBest, hash-verifying each block's on-disk
//! representation against its CBlockIndex hash. Stops at the first block
//! whose on-disk data is coherent with the index. Bounded by max_walkback
//! to keep the startup cost predictable.
//!
//! In the common case (no corruption), the very first check on pindexBest
//! succeeds and we return immediately with pindex_consistent == pindexBest
//! and sb_cross_count == 0. The expensive walk only runs when corruption
//! is actually present.
//!
//! Must be called with cs_main held.
CoherenceResult VerifyChainCoherence(int max_walkback) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Abandonment-style rewind of the chain to the supplied target block.
//! Updates in-memory pindexBest, nBestHeight, hashBestChain, g_chain_trust
//! and persists the new hashBestChain to LevelDB. Does NOT call
//! DisconnectBlock on the abandoned range -- the on-disk data for those
//! blocks is by definition unreadable/uninvertible, so we abandon rather
//! than disconnect. P2P will re-supply the missing blocks through the
//! normal AcceptBlock path.
//!
//! Returns false on LevelDB write failure (rare; treated as a startup-fatal
//! error by the caller).
//!
//! Must be called with cs_main held.
bool RewindToConsistentTip(CBlockIndex* pindex_target) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Top-level Phase 2 recovery entry point. Wraps VerifyChainCoherence +
//! RewindToConsistentTip + (conditionally) BeaconRegistry::Reset() +
//! RegistryBookmarks::UpdateRegistryBlockHeights, with logging at each
//! step so debug.log shows a clear narrative of what was detected and what
//! was done.
//!
//! Also services the deferred-rebuild flag set by the runtime reorg path
//! (see SetBeaconRebuildPending / IsBeaconRebuildPending below) -- if a
//! prior multi-SB reorg requested a beacon rebuild, this is where it
//! actually happens, before the registries reload from LevelDB.
//!
//! Returns true on success (including the no-rewind-needed case).
//! Returns false on either of:
//!   - exhausted: corruption beyond -coherencewalkmax; user must -reindex.
//!   - LevelDB write failure during the rewind.
//!
//! Skipped entirely if -reindex is set (the caller is already rebuilding).
//!
//! Must run after LoadBlockIndex (so pindexBest is populated) and BEFORE
//! GRC::Initialize / InitializeContracts (so registries have not yet
//! loaded their LevelDB state -- this is what lets us clamp / reset
//! bookmarks safely without invalidating in-memory state). See the design
//! doc at doc/block_corruption_recovery_design.md.
//!
//! Must be called with cs_main held.
bool RunStartupCoherenceRecovery() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Deferred beacon-registry rebuild flag, persisted to LevelDB.
//!
//! Set by the runtime reorg path (DisconnectBlocksBatch) when a reorg
//! crosses two or more superblock boundaries -- a scenario where
//! BeaconRegistry::Deactivate cannot resurrect expired pending beacons
//! from prior SBs because m_expired_pending only carries the LATEST SB's
//! set (the limitation acknowledged at beacon.cpp:1265-1273).
//!
//! Checked at startup by RunStartupCoherenceRecovery, which calls
//! BeaconRegistry::Reset() and clears the flag before the registries
//! reload, so InitializeContracts will replay beacons from V11_height
//! forward and rebuild cleanly.
//!
//! Single-SB reorgs are NOT flagged -- the existing Deactivate path is
//! fidelity-correct for that case and the rebuild would be wasted work.
//!
//! All three functions write to a single LevelDB key
//! ("beacon_db", "needs_rebuild"). Failures are logged but not fatal --
//! the worst case is "user notices stale beacon state and restarts again,"
//! which still recovers via the next pass.
bool IsBeaconRebuildPending();
void SetBeaconRebuildPending();
void ClearBeaconRebuildPending();

} // namespace GRC

#endif // GRIDCOIN_NODE_COHERENCE_H
