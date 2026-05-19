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
//! \brief Producer-side payload: a transaction was added to the wallet, with its
//! pre-decomposed display records.
//!
//! The producer thread (the one firing CWallet::NotifyTransactionChanged) runs
//! TransactionRecord::decomposeTransaction under the cs_wallet lock it already
//! holds and ships the resulting list of records to the consumer. The consumer
//! never touches the wallet.
//!
struct TxAddedPayload
{
    QList<TransactionRecord> records;
};

//!
//! \brief Producer-side payload: an existing transaction's status changed (depth,
//! confirmation, etc.). The consumer re-queries / refreshes display state for
//! the matching row.
//!
struct TxUpdatedPayload
{
    uint256 hash;
    int     status;  //!< ChangeType (CT_UPDATED / CT_DELETED / CT_NEW after fold).
};

//!
//! \brief Producer-side payload: an existing transaction was removed from the
//! wallet. The consumer drops the matching rows from its model.
//!
struct TxRemovedPayload
{
    uint256 hash;
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
    TxAddedPayload,
    TxUpdatedPayload,
    TxRemovedPayload,
    ChainTipChangedPayload>;

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
//! counter. The consumer never blocks the producer (drain() is non-blocking);
//! producers serialise only against each other at push time. The mutex hold
//! window on the push path is the cost of one std::deque::push_back plus the
//! seqno increment — well under a microsecond on modern hardware.
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
    //! \brief Pop up to \p max_batch events in seqno order. Non-blocking;
    //! returns an empty vector if the queue is empty. Intended to be called
    //! by the Qt-side drain timer.
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
