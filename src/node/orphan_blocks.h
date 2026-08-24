// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_ORPHAN_BLOCKS_H
#define GRIDCOIN_NODE_ORPHAN_BLOCKS_H

#include <consensus/consensus.h>
#include "chain.h"  // cs_main (required for clang's EXCLUSIVE_LOCKS_REQUIRED / GUARDED_BY analyzer)
#include "primitives/block.h"  // BlockHasher
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

    //! Maximum total serialized bytes of orphan blocks to hold in memory.
    //!
    //! The count above used to imply this on its own: every block was bounded
    //! by MAX_BLOCK_SIZE, so a thousand of them could not exceed a thousand
    //! times that. A block carrying a superblock is measured against a larger
    //! envelope, and once the per-block bound and the count differ the product
    //! stops being a bound on anything. Stating the byte budget keeps the
    //! ceiling where the count already put it, rather than letting it grow with
    //! whatever the largest permitted block happens to become.
    //!
    //! Orphans are held before a parent is known, so nothing about the block
    //! has been established when it is stored -- the bound cannot rely on the
    //! block being valid, or on its sender being honest.
    static constexpr size_t MAX_ORPHAN_BYTES = MAX_ORPHAN_BLOCKS * MAX_BLOCK_SIZE;

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

    //! Current total serialized bytes of stored orphans.
    size_t Bytes() const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Remove all orphans and clean up associated SeenStakes entries.
    void Clear() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

private:
    struct OrphanEntry
    {
        std::unique_ptr<CBlock> block;
        int64_t time_received;

        //! Measured once, on the way in. Recomputing it on the way out would
        //! let the running total drift if the block were ever altered in place.
        size_t bytes;
    };

    //! Primary storage: orphan block hash -> entry.
    std::unordered_map<uint256, OrphanEntry, BlockHasher> m_orphans;

    //! Reverse index: parent block hash -> set of orphan block hashes that
    //! claim it as their previous block.
    std::unordered_multimap<uint256, uint256, BlockHasher> m_by_prev;

    //! Running total of OrphanEntry::bytes, maintained by Add/EraseInternal.
    size_t m_bytes{0};

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
