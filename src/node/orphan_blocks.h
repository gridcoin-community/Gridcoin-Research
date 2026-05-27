// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_ORPHAN_BLOCKS_H
#define GRIDCOIN_NODE_ORPHAN_BLOCKS_H

#include "main.h"
#include "uint256.h"

#include <functional>
#include <memory>
#include <unordered_map>

class CBlock;

//!
//! \brief Manages orphan blocks — blocks received before their parent.
//!
//! Replaces the legacy global mapOrphanBlocks / mapOrphanBlocksByPrev with
//! a bounded, expiry-aware, testable container.
//!
class OrphanBlockManager
{
public:
    //! Maximum number of orphan blocks to hold in memory. Set above the
    //! superblock interval (~960 blocks) to avoid evicting orphans from a
    //! legitimate missing-block chain before it can be resolved.
    static constexpr size_t MAX_ORPHAN_BLOCKS = 1000;

    //! Maximum age in seconds before an orphan is eligible for eviction.
    static constexpr int64_t MAX_ORPHAN_AGE_SECONDS = 20 * 60;

    //! Add an orphan block. Returns false if the block is a duplicate.
    //! The caller must hold cs_main.
    bool Add(const uint256& hash, const CBlock& block, int64_t now) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Process the orphan chain rooted at \p accepted_hash using breadth-first
    //! traversal. For each orphan whose parent has been accepted, calls
    //! \p accept_fn. If it returns true, that orphan's children are queued.
    //! All processed orphans are removed regardless of acceptance result.
    //! Returns the count of orphans for which accept_fn returned true.
    size_t ProcessQueue(
        const uint256& accepted_hash,
        std::function<bool(CBlock&)> accept_fn) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Get the root block of the orphan chain containing \p hash.
    //! Returns nullptr if \p hash is not a known orphan.
    const CBlock* GetRootBlock(const uint256& hash) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Returns true if \p hash is a known orphan.
    bool Contains(const uint256& hash) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Returns true if any orphan claims \p prev_hash as its parent.
    bool HasChildrenOf(const uint256& prev_hash) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Evict orphans older than MAX_ORPHAN_AGE_SECONDS relative to \p now.
    //! Returns the number evicted.
    size_t EraseExpired(int64_t now) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Current number of stored orphans.
    size_t Size() const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Remove all orphans and clean up associated SeenStakes entries.
    void Clear() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

private:
    struct OrphanEntry
    {
        std::unique_ptr<CBlock> block;
        int64_t time_received;
    };

    //! Primary storage: orphan block hash -> entry.
    std::unordered_map<uint256, OrphanEntry, BlockHasher> m_orphans;

    //! Reverse index: parent block hash -> set of orphan block hashes that
    //! claim it as their previous block.
    std::unordered_multimap<uint256, uint256, BlockHasher> m_by_prev;

    //! Remove an orphan from all internal indices. Returns the block
    //! (moved out) so the caller can inspect it if needed.
    //! Returns nullptr if not found.
    std::unique_ptr<CBlock> EraseInternal(const uint256& hash);

    //! Evict one random orphan to make room. Calls EraseInternal.
    void EvictRandom();

    //! Walk back through m_orphans to find the root of the chain containing
    //! \p hash. Bounded by m_orphans.size() to prevent infinite loops.
    const CBlock* FindRootBlock(const uint256& hash) const;
};

//! Global orphan block manager instance. Requires cs_main for all access.
extern OrphanBlockManager g_orphan_blocks GUARDED_BY(cs_main);

#endif // GRIDCOIN_NODE_ORPHAN_BLOCKS_H
