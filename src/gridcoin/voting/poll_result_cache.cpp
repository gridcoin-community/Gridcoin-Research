// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/voting/poll_result_cache.h"
#include "gridcoin/voting/registry.h"
#include "hash.h"
#include "logging.h"
#include "util/time.h"
#include "chain.h"

using namespace GRC;

namespace {
//! The process-wide instance backing GetPollResultCache().
PollResultCache g_poll_result_cache;

//! Whether a poll that has (\p finished) or has not finished at the pinned tip passes \p flags. Judged from the
//! snapshot, by blocks, not by the wall clock as PollRegistry::Sequence filters. No flag, or both, passes every poll.
bool Wanted(const PollFilterFlag flags, const bool finished)
{
    const bool active = flags & ACTIVE;
    const bool done = flags & FINISHED;

    if (active == done) {
        return true;
    }

    return finished ? done : active;
}
} // anonymous namespace

PollResultCache& GRC::GetPollResultCache()
{
    return g_poll_result_cache;
}

PollResultKey GRC::KeyFor(const PollSnapshot& snapshot)
{
    PollResultKey key;
    key.m_finished = snapshot.m_finished;

    // A finished poll is keyed by its first block past the end; an open one, or a skeleton reference with no block,
    // by the pinned tip.
    const CBlockIndex* const pindex = snapshot.m_finished && snapshot.m_pindex_past_end
        ? snapshot.m_pindex_past_end
        : snapshot.m_pindex_tip;

    if (pindex) {
        key.m_block_hash = pindex->GetBlockHash();
    }

    key.m_votes_digest = SerializeHash(snapshot.m_votes);

    return key;
}

std::optional<PollResultItem> PollResultCache::GetOrBuild(const PollSnapshot& snapshot)
{
    const PollResultKey key = KeyFor(snapshot);

    {
        LOCK(m_mutex);

        auto it = m_entries.find(snapshot.m_txid);
        if (it != m_entries.end() && it->second.key == key) {
            return it->second.item;
        }
    }

    // Not cached, or cached under another key. Tally without holding m_mutex: BuildFor is the expensive step and may
    // throw InvalidDuetoReorgFork, which propagates to BuildPollTable to restart.
    PollResultOption result = PollResult::BuildFor(snapshot);

    if (!result) {
        return std::nullopt;
    }

    PollResultItem item(snapshot.m_txid, snapshot.m_payload_version, std::move(*result));

    // A reorg flagged during the traversal can change what the tally read, so the result is not kept.
    if (GetPollRegistry().reorg_occurred_during_reg_traversal) {
        return item;
    }

    {
        LOCK(m_mutex);

        // PollResult holds a const Poll, so it is move-constructible but not move-assignable; use erase + emplace
        // (construction only) rather than operator[] / insert_or_assign, which would need assignment.
        m_entries.erase(snapshot.m_txid);
        m_entries.emplace(snapshot.m_txid, CacheEntry{item, key});
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

        // Pin the chain tip once for this whole attempt: every poll's snapshot below is taken on it, and the
        // tallies run on it without holding cs_main.
        const CBlockIndex* pindex_tip = nullptr;
        {
            LOCK(cs_main);
            pindex_tip = pindexBest;
        }

        // No chain yet (early startup / not synced): nothing to tally.
        if (!pindex_tip) {
            break;
        }

        // Collect the txids under the registry lock, then take each poll's snapshot under cs_main and the registry
        // lock, looking it up again by txid. No registry reference or iterator outlives a lock, and the tally reads
        // only the snapshot. The lock order is cs_main before cs_poll_registry, as block connection takes them.
        std::vector<uint256> txids;
        {
            LOCK(PollRegistry::cs_poll_registry);

            for (const auto& iter : registry.Polls()) {
                txids.push_back(iter->Ref().Txid());
            }
        }

        for (const uint256& txid : txids) {
            try {
                std::optional<PollSnapshot> snapshot;
                {
                    LOCK2(cs_main, PollRegistry::cs_poll_registry);

                    // A poll removed since the txids were collected is skipped.
                    if (const PollReference* ref = registry.TryByTxid(txid)) {
                        snapshot = registry.Snapshot(*ref, pindex_tip);
                    }
                }

                if (snapshot && Wanted(flags, snapshot->m_finished)) {
                    if (std::optional<PollResultItem> item = GetOrBuild(*snapshot)) {
                        items.push_back(std::move(*item));
                    }
                }
            } catch (const InvalidDuetoReorgFork&) {
                // A reorg aborted this poll's snapshot or tally. Abandon the whole attempt and retry from a freshly
                // pinned tip, rather than return a table missing this poll.
                LogPrint(BCLog::LogFlags::VOTE, "INFO: %s: Invalidated due to reorg/fork. Starting over.", __func__);
                items.clear();
                fork_reorg_during_run = true;
                break;
            }

            // A reorg flagged during the attempt can change what the tallies read; abandon it and retry.
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

        // Wait for the reorg to clear before retrying. The TraversalScope opened
        // at the top of this function is still held here, so DetectReorg re-arms
        // reorg_occurred_during_reg_traversal for as long as g_reorg_in_progress
        // is set: this loop ends when the in-progress reorg exits -- success or
        // failure alike -- since SetBestChain and ForceReorganizeToHash clear
        // that flag on every exit. No later block connect is needed to unwedge.
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
