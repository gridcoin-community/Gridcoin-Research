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

bool GRC::PollResultReusable(bool cached_finished,
                             bool now_finished,
                             const uint256& tallied_tip_hash,
                             int tallied_tip_height,
                             const uint256& current_tip_hash,
                             int current_tip_height)
{
    // Finished state is time-based (Poll::Expired(GetAdjustedTime())), so a poll
    // can transition active -> finished even with a stalled tip. If that state has
    // changed since the tally, the cached m_finished and AVW-derived fields are
    // stale regardless of the tip, so force a rebuild.
    if (cached_finished != now_finished) {
        return false;
    }

    if (cached_finished) {
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
    // Re-evaluate the poll's finished state against the current adjusted time so a
    // poll that expired by wall-clock time (with no new block) invalidates its
    // cached "active" tally.
    const bool now_finished = entry.item.result.m_poll.Expired(GetAdjustedTime());

    return PollResultReusable(entry.finished, now_finished, entry.tallied_tip_hash,
                              entry.tallied_tip_height, tip_hash, tip_height);
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

    // Mark a traversal in progress for the whole build (RAII, so it is cleared on
    // every exit path — including the early return below). The count-based scope
    // keeps the reorg detector armed even if another traversal (e.g. an RPC tally)
    // overlaps this one.
    PollRegistry::TraversalScope traversal(registry);

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

        // THREAD-SAFETY EXCEPTION, deliberate and load-bearing.
        //
        // This is the one traversal in the tree that walks the poll registry
        // WITHOUT holding cs_poll_registry, and it cannot hold it: GetOrBuild()
        // tallies each poll, which takes cs_main, and the lock order is
        // cs_main -> cs_poll_registry. Holding the registry lock across the walk
        // would invert it and deadlock against the validation thread.
        //
        // What protects the walk instead is the TraversalScope opened at the top
        // of this function plus the reorg_occurred_during_reg_traversal recheck
        // after every element: a reorg that could invalidate the iterator sets the
        // flag, and the loop abandons the attempt and rebuilds the sequence from a
        // freshly pinned tip. Clang's analyzer cannot express "guarded by a
        // different protocol", so the diagnostic is suppressed for this traversal.
        // (It is suppressed in a few other places too -- rpc/voting.cpp, result.cpp;
        // this is the only one where an unlocked registry WALK is the deliberate
        // design rather than an unreviewed legacy.)
        //
        // If you are copying this pattern: don't. Every other caller holds the
        // lock for the whole traversal (gridcoin.cpp NotifyPoll, rpc/voting.cpp
        // listpolls, interfaces.cpp latestActivePollTime). Dropping the lock
        // without ALSO opening a TraversalScope and rechecking the flag per
        // element is an iterator use-after-free -- which is precisely the bug that
        // was live in latestActivePollTime until the lock requirements were moved
        // onto the declarations in registry.h and made visible to callers.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
        for (const auto& iter : WITH_LOCK(PollRegistry::cs_poll_registry, return registry.Polls().Where(flags))) {
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
                // A reorg aborted this poll's tally. Treat that as a hard signal to
                // abandon the whole attempt and retry from a freshly pinned tip,
                // rather than falling through and possibly returning a table that
                // is missing this poll (the flag check below could miss it if the
                // reorg clears between the throw and the check).
                LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Invalidated due to reorg/fork. Starting over.", __func__);
                items.clear();
                fork_reorg_during_run = true;
                break;
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
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

        // Done if no fork/reorg interrupted this attempt.
        if (!fork_reorg_during_run) {
            break;
        }

        // Wait for the reorg to clear before retrying. DetectReorg clears the flag
        // once ReorganizeChain has finished; a failed reorg leaves it set and the
        // node will shut down, interrupting the sleep.
        while (registry.reorg_occurred_during_reg_traversal) {
            if (!MilliSleep(1000)) {
                return items;
            }

            registry.DetectReorg();
        }

        fork_reorg_during_run = false;
    }

    g_timer.GetTimes(std::string{"End "} + std::string{__func__}, __func__);

    return items;
}

void PollResultCache::Clear()
{
    LOCK(m_mutex);
    m_entries.clear();
}
