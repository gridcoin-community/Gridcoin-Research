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

class CBlockIndex;

namespace GRC {

class PollReference;

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
//!   - BuildPollTable pins the chain tip ONCE per attempt (captured under
//!     cs_main, which is released before cs_poll_registry is taken — preserving
//!     the cs_main -> cs_poll_registry order) and tallies every poll against that
//!     one tip, so the whole table is internally consistent.
//!   - A reorg detected mid-traversal (PollRegistry::reorg_occurred_during_reg_traversal)
//!     discards the partial table and retries with a freshly pinned tip.
//!
//! Cache invalidation (see IsEntryValid):
//!   - Active poll: the active-vote-weight range ends at the tip and grows every
//!     block, so a cached active result is reused only while the tip is unchanged
//!     since it was tallied. New votes arrive in blocks (which move the tip), so
//!     gating reuse on the tip already covers them — no separate vote signal.
//!   - Closed poll: immutable while the tip only advances, so it is cached
//!     indefinitely; a backward reorg (the tip dropping below the height the
//!     poll was tallied at) could reach the poll window, so it forces a rebuild.
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
    //! A cached tally together with the chain state it was computed against.
    struct CacheEntry
    {
        PollResultItem item;      //!< The cached bundle (txid, version, result).
        uint256 tallied_tip_hash; //!< Hash of the tip the result was tallied against.
        int tallied_tip_height;   //!< Height of that tip.
        bool finished;            //!< Whether the poll was closed when tallied.
    };

    //! \brief Whether \p entry may be served for the current pinned tip.
    bool IsEntryValid(const CacheEntry& entry, const uint256& tip_hash, int tip_height) const;

    //! \brief Serve \p ref from cache if valid, otherwise tally it against the
    //! pinned tip and cache the result. Returns std::nullopt if the poll could
    //! not be read; may throw InvalidDuetoReorgFork if a reorg interrupts the
    //! tally (the caller restarts the whole table in that case).
    std::optional<PollResultItem> GetOrBuild(const PollReference& ref,
                                             const uint256& txid,
                                             const CBlockIndex* pindex_tip,
                                             const uint256& tip_hash,
                                             int tip_height);

    mutable Mutex m_mutex;
    std::map<uint256, CacheEntry> m_entries GUARDED_BY(m_mutex);
};

//! \brief Whether a cached tally may still be served for the current pinned tip.
//!
//! This is the cache's whole invalidation policy, factored out of PollResultCache
//! as a pure function of the tally's chain state and the current tip so it can be
//! unit-tested without polls, a chain, or disk:
//!   - Finished-state transition: a poll's finished state comes from
//!     Poll::Expired(GetAdjustedTime()), i.e. wall-clock time, so an active poll
//!     can expire even while the tip is stalled (no new block). If \p now_finished
//!     differs from the \p cached_finished the entry was tallied with, the cached
//!     m_finished and AVW-derived fields are stale regardless of the tip, so the
//!     entry is rebuilt. Without this, an active poll cached just before expiry on
//!     a stalled tip would stay "active" indefinitely (and the GUI would keep
//!     offering voting on it).
//!   - Closed poll (\p cached_finished): its votes and its fixed AVW end block are
//!     immutable while the tip only advances, so it is reusable exactly while
//!     \p current_tip_height >= \p tallied_tip_height. A backward reorg (the tip
//!     dropping below the tally height) could reach the poll window, so it forces
//!     a rebuild. This is conservative — it also rebuilds on shallow backward
//!     reorgs that never touch the window — but backward reorgs are rare.
//!   - Active poll: PollReference::GetActiveVoteWeight ends the AVW range at the
//!     tip, and that range grows every block, so the result changes on every tip
//!     advance; it is reusable only while \p current_tip_hash equals the
//!     \p tallied_tip_hash it was computed against.
//!
//! \param cached_finished      Whether the poll was closed when it was tallied.
//! \param now_finished         Whether the poll is closed as of the current
//!                             adjusted time (Poll::Expired(GetAdjustedTime())).
//! \param tallied_tip_hash     Hash of the tip the tally was computed against.
//! \param tallied_tip_height   Height of that tip.
//! \param current_tip_hash     Hash of the current pinned tip.
//! \param current_tip_height   Height of the current pinned tip.
bool PollResultReusable(bool cached_finished,
                        bool now_finished,
                        const uint256& tallied_tip_hash,
                        int tallied_tip_height,
                        const uint256& current_tip_hash,
                        int current_tip_height);

//! \brief The process-wide poll result cache (a singleton like the registries).
PollResultCache& GetPollResultCache();

} // namespace GRC

#endif // GRIDCOIN_VOTING_POLL_RESULT_CACHE_H
