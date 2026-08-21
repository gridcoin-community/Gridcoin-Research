// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_WALLET_EVENT_QUEUE_H
#define GRIDCOIN_WALLET_WALLET_EVENT_QUEUE_H

#include "interfaces/wallet_coin_channel.h" // WalletCoinEvent + payload value types.
#include "interfaces/wallet_tx_channel.h"   // WalletEvent + payload value types.

#include "util/time.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <mutex>
#include <utility>
#include <vector>

namespace GRC {

//!
//! \brief MPSC event queue: multiple producer threads in the core push under
//! the locks they already hold; a single consumer (the Qt main thread, via a
//! periodic drain timer) pops in batches.
//!
//! Templated on the channel's payload/event pair so the wallet transaction
//! channel (interfaces/wallet_tx_channel.h) and the wallet coin channel
//! (interfaces/wallet_coin_channel.h) share one queue mechanism — the
//! aliases below are the two instantiations. EventT must be an aggregate with
//! seqno / emit_time_us / payload members (both channels' event structs are).
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
//! overflow policy will be added. (The coin channel additionally suppresses
//! enqueueing entirely while no view is registered, so an idle GUI node does
//! not accumulate events between dialog opens.)
//!
template <class PayloadT, class EventT>
class BasicEventQueue
{
public:
    BasicEventQueue() = default;

    BasicEventQueue(const BasicEventQueue&) = delete;
    BasicEventQueue& operator=(const BasicEventQueue&) = delete;

    //!
    //! \brief Push an event payload. The queue assigns a fresh monotonic seqno
    //! and emit timestamp under its own mutex, and RETURNS that seqno. Safe to
    //! call from any thread, including while the producer holds cs_main /
    //! cs_wallet. The returned seqno is the per-view high-water the store records
    //! so a consumer can discard events already reflected in a getRows refetch
    //! (windowed-model PR4-fix B).
    //!
    uint64_t push(PayloadT payload)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        EventT ev;
        ev.seqno        = m_next_seqno++;
        ev.emit_time_us = GetTimeMicros();
        ev.payload      = std::move(payload);

        const uint64_t seqno = ev.seqno;
        m_queue.push_back(std::move(ev));
        return seqno;
    }

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
    std::vector<EventT> drain(std::size_t max_batch = static_cast<std::size_t>(-1))
    {
        std::deque<EventT> taken;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (max_batch >= m_queue.size()) {
                // Full drain: O(1) deque swap. The producer-facing critical
                // section is just the swap, so a producer in push() (holding
                // cs_wallet) is never blocked behind per-element drain work.
                taken.swap(m_queue);
            } else {
                // Bounded drain: move only max_batch elements. The lock hold
                // time is bounded by the caller's requested batch size, not by
                // the (potentially large) backlog.
                for (std::size_t i = 0; i < max_batch; ++i) {
                    taken.push_back(std::move(m_queue.front()));
                    m_queue.pop_front();
                }
            }
        }

        // Lock released. The result vector — heap allocation plus per-element
        // moves — is built outside the critical section.
        return std::vector<EventT>(std::make_move_iterator(taken.begin()),
                                   std::make_move_iterator(taken.end()));
    }

    //!
    //! \brief Discard every pending event in one O(1) swap, without touching
    //! the seqno counter (seqnos stay monotonic across a clear — required by
    //! the reseed-from-high-water reconciliation on both channels). Used when
    //! the last consumer view unregisters and on a full resync.
    //!
    void clear()
    {
        std::deque<EventT> discard;
        std::lock_guard<std::mutex> lock(m_mutex);
        discard.swap(m_queue);
    }

    //!
    //! \brief Snapshot of current queue depth, for diagnostics. May be stale
    //! by the time the caller observes it; never use it for correctness logic.
    //!
    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    mutable std::mutex m_mutex;
    std::deque<EventT> m_queue;
    uint64_t           m_next_seqno{0};
};

//! The wallet transaction channel's queue (Phase 1c-ii; the historical
//! WalletEventQueue class, unchanged in behavior).
using WalletEventQueue = BasicEventQueue<WalletEventPayload, WalletEvent>;

//! The wallet coin channel's queue (issue #3183).
using WalletCoinEventQueue = BasicEventQueue<WalletCoinEventPayload, WalletCoinEvent>;

} // namespace GRC

#endif // GRIDCOIN_WALLET_WALLET_EVENT_QUEUE_H
