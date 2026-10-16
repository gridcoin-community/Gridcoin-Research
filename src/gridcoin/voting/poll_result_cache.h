// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_POLL_RESULT_CACHE_H
#define GRIDCOIN_VOTING_POLL_RESULT_CACHE_H

#include "gridcoin/voting/filter.h"
#include "gridcoin/voting/result.h"
#include "sync.h"
#include "uint256.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace GRC {

struct PollSnapshot;

//! \brief One fully-tallied poll, ready for presentation.
//!
//! The tally (PollResult, which already carries the whole Poll) plus the two
//! identifiers the tally itself does not hold: the poll's transaction id and its
//! contract payload version. Every field a consumer renders is derivable from
//! this bundle; it holds no chain pointers and no registry references, so it can
//! be handed across a process boundary (Phase 1d-iii interfaces::VotingManager)
//! and rendered without touching core state.
struct PollResultItem
{
    uint256 txid;
    uint32_t payload_version = 0;
    PollResult result;

    PollResultItem(uint256 txid_in, uint32_t payload_version_in, PollResult result_in)
        : txid(txid_in)
        , payload_version(payload_version_in)
        , result(std::move(result_in))
    {
    }
};

//!
//! \brief What a cached tally depends on besides the poll itself.
//!
//! A tally is served from the cache while a fresh snapshot's key equals the key it was tallied under. No part of the
//! key reads the wall clock:
//!   - m_finished: a poll that has finished since the tally needs its final result.
//!   - m_block_hash: for a finished poll, its first block past the end. The result is fixed by the chain through that
//!     block, so it holds while that block stays on the main chain, whatever the tip does after it. For an open poll,
//!     the pinned tip: the active vote weight range ends at the tip and grows with every block.
//!   - m_votes_digest: the poll's linked votes, in order.
//!
struct PollResultKey
{
    bool m_finished = false;
    uint256 m_block_hash;
    uint256 m_votes_digest;

    bool operator==(const PollResultKey& other) const
    {
        return m_finished == other.m_finished
            && m_block_hash == other.m_block_hash
            && m_votes_digest == other.m_votes_digest;
    }

    bool operator!=(const PollResultKey& other) const { return !(*this == other); }
};

//! \brief The key a tally of \p snapshot is cached under.
PollResultKey KeyFor(const PollSnapshot& snapshot);

//! \brief Core engine that turns the poll registry into a table of tallied
//! results, memoizing each poll's tally so the expensive PollResult::BuildFor
//! runs only when the result can actually have changed.
//!
//! Ownership of the poll tally, the cache, the registry traversal and the
//! reorg-retry moves here from the GUI (Phase 1d-iii). The GUI used to own the
//! cache (VotingModel::m_pollitems plus a per-item stale flag), the three-try
//! reorg-retry loop and the registry walk; all of that now lives in the core,
//! behind this class's own lock, so the GUI just renders the snapshots it is
//! handed.
//!
//! Consistency model:
//!   - BuildPollTable pins the chain tip ONCE per attempt and tallies every poll
//!     against that one tip, so the whole table is internally consistent. Each
//!     poll's snapshot is taken under cs_main and then cs_poll_registry, the
//!     order block connection takes them.
//!   - A reorg detected mid-traversal (PollRegistry::reorg_occurred_during_reg_traversal)
//!     discards the partial table and retries with a freshly pinned tip.
//!
//! Cache invalidation: see PollResultKey. Each poll's snapshot is taken under
//! cs_main and cs_poll_registry; the tally and the key read only the snapshot.
class PollResultCache
{
public:
    //! \brief Build the table of results for the polls matching \p flags,
    //! serving each from cache when still valid and (re)tallying otherwise.
    //!
    //! Thread-safe. May block for the duration of a tally and, on a reorg, for up
    //! to a few one-second retries while the reorg clears, so callers should run
    //! it off the UI thread (the interface facade drives it on a core worker).
    std::vector<PollResultItem> BuildPollTable(PollFilterFlag flags);

    //! \brief Drop all cached results. Thread-safe.
    void Clear();

private:
    //! A cached tally together with the key it was computed under.
    struct CacheEntry
    {
        PollResultItem item;      //!< The cached bundle (txid, version, result).
        PollResultKey key;        //!< What the tally depended on.
    };

    //! \brief Serve the poll in \p snapshot from cache if its key still matches, otherwise tally it and cache the
    //! result. Returns std::nullopt if the poll could not be read; may throw InvalidDuetoReorgFork if a reorg
    //! interrupts the tally (the caller restarts the whole table in that case).
    std::optional<PollResultItem> GetOrBuild(const PollSnapshot& snapshot);

    mutable Mutex m_mutex;
    std::map<uint256, CacheEntry> m_entries GUARDED_BY(m_mutex);
};

//! \brief The process-wide poll result cache (a singleton like the registries).
PollResultCache& GetPollResultCache();

} // namespace GRC

#endif // GRIDCOIN_VOTING_POLL_RESULT_CACHE_H
