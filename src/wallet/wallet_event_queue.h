// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_WALLET_EVENT_QUEUE_H
#define GRIDCOIN_WALLET_WALLET_EVENT_QUEUE_H

#include "interfaces/wallet_tx_channel.h" // WalletEvent + payload value types.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace GRC {

//!
//! \brief MPSC event queue: multiple producer threads in the core push under
//! the locks they already hold; a single consumer (the Qt main thread, via a
//! periodic drain timer) pops in batches.
//!
//! The event and payload value types live in interfaces/wallet_tx_channel.h —
//! they are the wallet transaction channel's boundary contract (Phase 1c-ii);
//! this class is the node-side in-process mechanism behind it.
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
    //! and emit timestamp under its own mutex, and RETURNS that seqno. Safe to
    //! call from any thread, including while the producer holds cs_main /
    //! cs_wallet. The returned seqno is the per-view high-water the store records
    //! so a consumer can discard events already reflected in a getRows refetch
    //! (windowed-model PR4-fix B).
    //!
    uint64_t push(WalletEventPayload payload);

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

#endif // GRIDCOIN_WALLET_WALLET_EVENT_QUEUE_H
