// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLET_EVENT_QUEUE_H
#define BITCOIN_QT_WALLET_EVENT_QUEUE_H

#include "transactionrecord.h"
#include "uint256.h"

#include <QList>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <variant>
#include <vector>

namespace GRC {

//!
//! \brief View identifiers stamped on per-view events (PR3).
//!
//! VIEW_FULL is the native unfiltered/unsorted stream the TransactionTableModel
//! consumes today (PR2/PR2.5 behaviour, epoch 0). Registered cursors (server-side
//! filter/sort) use VIEW_OVERVIEW and up. The consumer ignores events whose viewId
//! it does not own.
//!
constexpr int VIEW_FULL = 0;
constexpr int VIEW_OVERVIEW = 1;
constexpr int VIEW_DETAILED = 2;   //!< the detailed history view (PR4)

//!
//! \brief Producer-side payload: rows were inserted into the ordered wallet
//! view at a producer-computed position.
//!
//! Windowed-model PR2 moved the ORDERING to the producer-side GRC::WalletTxStore:
//! the producer runs TransactionRecord::decomposeTransaction under the locks it
//! already holds, the store computes the TxOrderLess insert position, and this
//! payload carries that position plus the records. The consumer inserts the
//! records into its replica at exactly \p position inside a single
//! beginInsertRows/endInsertRows bracket — it reads ONLY this payload, never the
//! live store, so its replica tracks the begin/end replay even when a drain
//! batch contains several inserts that reorder rows.
//!
//! \p records are all the decomposed parts of one transaction; under TxOrderLess
//! they occupy a contiguous range, so one payload == one bracket.
//!
struct RowsInsertedPayload
{
    int position;
    std::vector<TransactionRecord> records;
    int viewId = VIEW_FULL;   //!< which view this insert belongs to (PR3)
    uint64_t epoch = 0;       //!< source cursor epoch, for stale-fetch reconciliation (PR3)
};

//!
//! \brief Producer-side payload: a contiguous run of rows was removed at a
//! producer-computed position. Carries position + count (resolved from the tx
//! hash by the store, where same-hash rows are guaranteed contiguous). The
//! consumer erases [position, position + count) inside one
//! beginRemoveRows/endRemoveRows bracket. A removal that matched nothing emits
//! no event at all, so this payload always denotes a real, non-empty erase.
//!
struct RowsRemovedPayload
{
    int position;
    int count;
    int viewId = VIEW_FULL;   //!< which view this removal belongs to (PR3)
    uint64_t epoch = 0;       //!< source cursor epoch, for stale-fetch reconciliation (PR3)
};

//!
//! \brief Producer-side payload (PR3): a per-view cursor was rebuilt wholesale
//! (filter or sort change) — the consumer must drop its replica for \p viewId
//! and bulk-refill \p total served rows via the store's getRows. Carries the
//! new \p epoch so any in-flight pre-rebuild fetch can be discarded on arrival.
//!
struct RowsResetPayload
{
    int viewId;
    uint64_t epoch;
    int total;       //!< served-window row count after the rebuild
};

//!
//! \brief Producer-side payload (PR3): the TOTAL accepted row count for a view
//! changed (e.g. a row entered/left the filter off-window, so the scrollbar
//! extent moves but no served row was inserted/removed). The consumer resizes
//! its virtual rowCount to \p total_accepted without touching cached rows.
//! Decision 4 (RowCountChanged not deferred — OverviewPage is the windowed testbed).
//!
struct RowCountChangedPayload
{
    int viewId;
    uint64_t epoch;
    int total_accepted;
};

//!
//! \brief Producer-side payload (PR3): rows [first, first+count) of \p viewId
//! changed in place (a status/field update that did NOT move them) — the
//! consumer re-reads them (dataChanged), re-fetching from the store if windowed.
//!
struct RowsChangedPayload
{
    int viewId;
    uint64_t epoch;
    int first;
    int count;
};

//!
//! \brief Producer-side payload: the chain tip advanced (block connected or
//! disconnected). Replaces the role of the legacy 4-second
//! WalletModel::pollBalanceChanged poll which used to watch
//! nBestHeight != cachedNumBlocks. The consumer reacts by refreshing
//! per-row confirmation status and re-running the (rate-limited) balance
//! recompute path.
//!
//! Pre-computing the balance on the producer side is intentionally NOT done
//! here: wallet->GetBalance() iterates the full wallet (O(N) over mapWallet)
//! and we don't want to pay that on msghand for every block. The
//! consumer-side path keeps the existing TRY_LOCK + MODEL_UPDATE_DELAY-gated
//! recompute, but is now event-driven instead of timer-polled. When this
//! code becomes the consumer side of an IPC channel post multiprocess
//! separation, the producer can optionally precompute the snapshot to save
//! a round-trip; the consumer contract doesn't change.
//!
struct ChainTipChangedPayload
{
    int     height;
    int64_t best_time;
};

using WalletEventPayload = std::variant<
    RowsInsertedPayload,
    RowsRemovedPayload,
    ChainTipChangedPayload,
    RowsResetPayload,
    RowCountChangedPayload,
    RowsChangedPayload>;

//!
//! \brief A single event in the wallet→GUI event channel.
//!
//! The seqno is assigned by WalletEventQueue::push under the queue's mutex,
//! giving every event a unique monotonic ordering across all producer threads.
//! Consumers rely on this for "resync from seqno N+1" semantics that will be
//! used once the queue becomes the consumer side of an IPC channel.
//!
//! emit_time_us is for diagnostics only — it lets the consumer measure
//! end-to-end queue latency without taking any extra locks.
//!
struct WalletEvent
{
    uint64_t           seqno;
    int64_t            emit_time_us;
    WalletEventPayload payload;
};

//!
//! \brief MPSC event queue: multiple producer threads in the core push under
//! the locks they already hold; a single consumer (the Qt main thread, via a
//! periodic drain timer) pops in batches.
//!
//! Synchronisation is a single std::mutex protecting the deque and the seqno
//! counter. Producers serialise only against each other at push time; the
//! mutex hold window on the push path is one std::deque::push_back plus the
//! seqno increment — well under a microsecond.
//!
//! drain() is designed to keep the producer-facing critical section tiny: a
//! full drain (the common case — the Qt timer drains everything) swaps the
//! whole deque out under the lock in O(1) and builds the result vector after
//! releasing it, so a producer calling push() under cs_wallet is never
//! blocked behind per-element drain work. A bounded drain (explicit
//! max_batch) holds the lock for at most max_batch element moves — bounded
//! by the caller's request, not by the backlog.
//!
//! The queue is intentionally unbounded for the in-process prototype: a
//! runaway producer would exhaust memory long before queue depth becomes a
//! correctness concern. When this code is repurposed as the consumer side of
//! an IPC channel (post multiprocess separation), a soft cap with an explicit
//! overflow policy will be added.
//!
class WalletEventQueue
{
public:
    WalletEventQueue() = default;

    WalletEventQueue(const WalletEventQueue&) = delete;
    WalletEventQueue& operator=(const WalletEventQueue&) = delete;

    //!
    //! \brief Push an event payload. The queue assigns a fresh monotonic seqno
    //! and emit timestamp under its own mutex. Safe to call from any thread,
    //! including while the producer holds cs_main / cs_wallet.
    //!
    void push(WalletEventPayload payload);

    //!
    //! \brief Pop up to \p max_batch events in seqno order. Returns an empty
    //! vector if the queue is empty. Intended to be called by the Qt-side
    //! drain timer.
    //!
    //! A full drain (max_batch >= current depth) releases the queue mutex
    //! after an O(1) deque swap; a bounded drain holds it for at most
    //! max_batch element moves. Either way the result vector is allocated
    //! and filled after the lock is released.
    //!
    std::vector<WalletEvent> drain(std::size_t max_batch = static_cast<std::size_t>(-1));

    //!
    //! \brief Snapshot of current queue depth, for diagnostics. May be stale
    //! by the time the caller observes it; never use it for correctness logic.
    //!
    std::size_t size() const;

private:
    mutable std::mutex      m_mutex;
    std::deque<WalletEvent> m_queue;
    uint64_t                m_next_seqno{0};
};

} // namespace GRC

#endif // BITCOIN_QT_WALLET_EVENT_QUEUE_H
