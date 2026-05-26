// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/orphan_blocks.h"

#include "main.h"
#include "random.h"
#include "gridcoin/staking/spam.h"
#include "util.h"

extern GRC::SeenStakes g_seen_stakes GUARDED_BY(cs_main);

OrphanBlockManager g_orphan_blocks GUARDED_BY(cs_main);

bool OrphanBlockManager::Add(const uint256& hash, const CBlock& block, int64_t now) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Reject duplicates.
    if (m_orphans.count(hash)) {
        return false;
    }

    // Expire stale orphans first, then evict randomly if still full.
    EraseExpired(now);

    while (m_orphans.size() >= MAX_ORPHAN_BLOCKS) {
        EvictRandom();
    }

    OrphanEntry entry;
    entry.block = std::make_unique<CBlock>(block);
    entry.time_received = now;

    const uint256 prev_hash = block.hashPrevBlock;

    m_orphans.emplace(hash, std::move(entry));
    m_by_prev.emplace(prev_hash, hash);

    LogPrint(BCLog::LogFlags::VERBOSE, "OrphanBlockManager: added orphan %s (prev=%s, map size %u)",
             hash.ToString(), prev_hash.ToString(), m_orphans.size());

    return true;
}

size_t OrphanBlockManager::ProcessQueue(
    const uint256& accepted_hash,
    std::function<bool(CBlock&)> accept_fn) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    std::vector<uint256> work_queue;
    work_queue.push_back(accepted_hash);

    size_t accepted_count = 0;

    for (size_t i = 0; i < work_queue.size(); ++i) {
        const uint256 parent_hash = work_queue[i];

        // Collect all orphan hashes that depend on this parent. We must
        // collect first because EraseInternal modifies m_by_prev.
        std::vector<uint256> children;
        auto range = m_by_prev.equal_range(parent_hash);
        for (auto it = range.first; it != range.second; ++it) {
            children.push_back(it->second);
        }

        for (const uint256& orphan_hash : children) {
            auto it = m_orphans.find(orphan_hash);
            if (it == m_orphans.end()) {
                continue;
            }

            CBlock& orphan_block = *it->second.block;

            if (accept_fn(orphan_block)) {
                work_queue.push_back(orphan_hash);
                ++accepted_count;
            }

            // Clean up SeenStakes tracking for the orphan's coinstake before
            // erasing. The block must be a PoS block to have orphan stake
            // tracking (PoW blocks and empty vtx won't have a coinstake).
            if (orphan_block.IsProofOfStake()) {
                g_seen_stakes.ForgetOrphan(orphan_block.vtx[1]);
            }

            EraseInternal(orphan_hash);
        }
    }

    return accepted_count;
}

const CBlock* OrphanBlockManager::GetRootBlock(const uint256& hash) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return FindRootBlock(hash);
}

bool OrphanBlockManager::Contains(const uint256& hash) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return m_orphans.count(hash) > 0;
}

bool OrphanBlockManager::HasChildrenOf(const uint256& prev_hash) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return m_by_prev.count(prev_hash) > 0;
}

size_t OrphanBlockManager::EraseExpired(int64_t now) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    size_t count = 0;

    for (auto it = m_orphans.begin(); it != m_orphans.end(); ) {
        if (now - it->second.time_received > MAX_ORPHAN_AGE_SECONDS) {
            const uint256 hash = it->first;
            const int64_t age = now - it->second.time_received;

            LogPrint(BCLog::LogFlags::VERBOSE, "OrphanBlockManager: expiring orphan %s (age %" PRId64 "s, map size %u)",
                     hash.ToString(), age, m_orphans.size());

            // Clean up SeenStakes before erasing.
            if (it->second.block->IsProofOfStake()) {
                g_seen_stakes.ForgetOrphan(it->second.block->vtx[1]);
            }

            // EraseInternal invalidates the iterator, so advance first.
            ++it;
            EraseInternal(hash);
            ++count;
        } else {
            ++it;
        }
    }

    if (count > 0) {
        LogPrint(BCLog::LogFlags::VERBOSE, "OrphanBlockManager: expired %u orphans, %u remaining",
                 count, m_orphans.size());
    }

    return count;
}

size_t OrphanBlockManager::Size() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return m_orphans.size();
}

void OrphanBlockManager::Clear() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    for (auto& [hash, entry] : m_orphans) {
        if (entry.block->IsProofOfStake()) {
            g_seen_stakes.ForgetOrphan(entry.block->vtx[1]);
        }
    }

    m_orphans.clear();
    m_by_prev.clear();
}

std::unique_ptr<CBlock> OrphanBlockManager::EraseInternal(const uint256& hash)
{
    auto it = m_orphans.find(hash);
    if (it == m_orphans.end()) {
        return nullptr;
    }

    const uint256 prev_hash = it->second.block->hashPrevBlock;
    std::unique_ptr<CBlock> block = std::move(it->second.block);

    m_orphans.erase(it);

    // Remove the corresponding entry from the reverse index.
    auto range = m_by_prev.equal_range(prev_hash);
    for (auto mi = range.first; mi != range.second; ++mi) {
        if (mi->second == hash) {
            m_by_prev.erase(mi);
            break;
        }
    }

    return block;
}

void OrphanBlockManager::EvictRandom() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (m_orphans.empty()) {
        return;
    }

    // Advance a random number of buckets into the unordered_map for
    // approximately uniform random eviction.
    size_t offset = GetRand<size_t>(m_orphans.size());
    auto it = m_orphans.begin();
    std::advance(it, offset);

    const uint256 hash = it->first;

    LogPrint(BCLog::LogFlags::VERBOSE, "OrphanBlockManager: evicting orphan %s (%.0f seconds old)",
             hash.ToString(), static_cast<double>(GetTime() - it->second.time_received));

    if (it->second.block->IsProofOfStake()) {
        g_seen_stakes.ForgetOrphan(it->second.block->vtx[1]);
    }

    EraseInternal(hash);
}

const CBlock* OrphanBlockManager::FindRootBlock(const uint256& hash) const
{
    auto it = m_orphans.find(hash);
    if (it == m_orphans.end()) {
        return nullptr;
    }

    const CBlock* root = it->second.block.get();
    size_t depth = 0;

    // Walk back through the orphan chain. Bounded by map size to prevent
    // infinite loops from any hypothetical circular references.
    while (depth < m_orphans.size()) {
        auto parent_it = m_orphans.find(root->hashPrevBlock);
        if (parent_it == m_orphans.end()) {
            break;
        }
        root = parent_it->second.block.get();
        ++depth;
    }

    return root;
}
