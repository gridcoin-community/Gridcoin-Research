// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/voting/poll_result_cache.h"
#include "gridcoin/voting/registry.h"
#include "logging.h"
#include "main.h"
#include "util/time.h"

using namespace GRC;

namespace {
//! The process-wide instance backing GetPollResultCache().
PollResultCache g_poll_result_cache;
} // anonymous namespace

PollResultCache& GRC::GetPollResultCache()
{
    return g_poll_result_cache;
}

bool GRC::PollResultReusable(bool finished,
                             const uint256& tallied_tip_hash,
                             int tallied_tip_height,
                             const uint256& current_tip_hash,
                             int current_tip_height)
{
    if (finished) {
        // Closed poll: its votes and its fixed AVW end block are immutable while
        // the tip only advances. A backward reorg (the tip below the height we
        // tallied at) could reach the poll window, so rebuild to be safe. This is
        // conservative — it also rebuilds on shallow backward reorgs that never
        // touch the window — but backward reorgs are rare, so the extra work is
        // negligible and it keeps the closed-poll path free of a chain walk.
        return current_tip_height >= tallied_tip_height;
    }

    // Active poll: PollReference::GetActiveVoteWeight ends the AVW range at the
    // tip, and that range grows every block, so the result changes on every tip
    // advance. Reuse only while the tip is exactly the one we tallied against.
    return tallied_tip_hash == current_tip_hash;
}

bool PollResultCache::IsEntryValid(const CacheEntry& entry, const uint256& tip_hash, int tip_height) const
{
    return PollResultReusable(entry.finished, entry.tallied_tip_hash, entry.tallied_tip_height,
                              tip_hash, tip_height);
}

std::optional<PollResultItem> PollResultCache::GetOrBuild(const PollReference& ref,
                                                          const uint256& txid,
                                                          const CBlockIndex* pindex_tip,
                                                          const uint256& tip_hash,
                                                          int tip_height)
{
    {
        LOCK(m_mutex);

        auto it = m_entries.find(txid);
        if (it != m_entries.end() && IsEntryValid(it->second, tip_hash, tip_height)) {
            return it->second.item;
        }
    }

    // Not cached, or the cached tally is no longer valid for this tip. Tally
    // without holding m_mutex — BuildFor is the expensive step and may throw
    // InvalidDuetoReorgFork, which propagates to BuildPollTable to restart.
    PollResultOption result = PollResult::BuildFor(ref, pindex_tip);

    if (!result) {
        return std::nullopt;
    }

    PollResultItem item(txid, ref.GetPollPayloadVersion(), std::move(*result));

    {
        LOCK(m_mutex);

        // PollResult holds a const Poll, so it is move-constructible but not
        // move-assignable; use erase + emplace (construction only) rather than
        // operator[] / insert_or_assign, which would need assignment.
        m_entries.erase(txid);
        m_entries.emplace(txid, CacheEntry{item, tip_hash, tip_height, item.result.m_finished});
    }

    return item;
}

std::vector<PollResultItem> PollResultCache::BuildPollTable(PollFilterFlag flags)
{
    g_timer.InitTimer(__func__, LogInstance().WillLogCategory(BCLog::LogFlags::VOTE));

    std::vector<PollResultItem> items;
    PollRegistry& registry = GetPollRegistry();

    registry.registry_traversal_in_progress = true;

    bool fork_reorg_during_run = false;

    // Up to three attempts if a reorg/fork lands mid-run. This mirrors the loop
    // that used to live in the GUI (VotingModel::buildPollTable); it now runs in
    // the core so the GUI just consumes the finished table.
    for (unsigned int i = 0; i < 3; ++i) {
        items.clear();

        // Pin the chain tip once for this whole attempt. cs_main is taken only to
        // read the tip and is released before cs_poll_registry is acquired below,
        // preserving the cs_main -> cs_poll_registry order and keeping the tally
        // itself lock-free against a single consistent tip.
        const CBlockIndex* pindex_tip = nullptr;
        uint256 tip_hash;
        int tip_height = 0;
        {
            LOCK(cs_main);
            pindex_tip = pindexBest;
            if (pindex_tip) {
                tip_hash = pindex_tip->GetBlockHash();
                tip_height = pindex_tip->nHeight;
            }
        }

        // No chain yet (early startup / not synced): nothing to tally.
        if (!pindex_tip) {
            break;
        }

        for (const auto& iter : WITH_LOCK(registry.cs_poll_registry, return registry.Polls().Where(flags))) {
            // Read the poll's identifiers outside cs_poll_registry, matching the
            // former GUI traversal: the coarse reorg detector below guards the
            // reference against a reorg that would invalidate it mid-walk.
            const PollReference& ref = iter->Ref();
            const uint256 txid = ref.Txid();

            try {
                if (std::optional<PollResultItem> item = GetOrBuild(ref, txid, pindex_tip, tip_hash, tip_height)) {
                    items.push_back(std::move(*item));
                }
            } catch (const InvalidDuetoReorgFork&) {
                LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Invalidated due to reorg/fork. Starting over.", __func__);
            }

            // Must be checked AFTER GetOrBuild: if a reorg during traversal
            // invalidated the Ref the sequence points at, incrementing the
            // iterator to the next position would segfault. Abandon this attempt
            // and rebuild the sequence from scratch on the next try.
            if (registry.reorg_occurred_during_reg_traversal) {
                items.clear();
                fork_reorg_during_run = true;
                break;
            }
        }

        // Done if no fork/reorg interrupted this attempt.
        if (!fork_reorg_during_run) {
            break;
        }

        // Wait for the reorg to clear before retrying. DetectReorg clears the flag
        // once ReorganizeChain has finished; a failed reorg leaves it set and the
        // node will shut down, interrupting the sleep.
        while (registry.reorg_occurred_during_reg_traversal) {
            if (!MilliSleep(1000)) {
                registry.registry_traversal_in_progress = false;
                return items;
            }

            registry.DetectReorg();
        }

        fork_reorg_during_run = false;
    }

    registry.registry_traversal_in_progress = false;

    g_timer.GetTimes(std::string{"End "} + std::string{__func__}, __func__);

    return items;
}

void PollResultCache::Clear()
{
    LOCK(m_mutex);
    m_entries.clear();
}
