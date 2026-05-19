// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet_event_queue.h"

#include "util/time.h"

#include <iterator>
#include <utility>

namespace GRC {

void WalletEventQueue::push(WalletEventPayload payload)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    WalletEvent ev;
    ev.seqno        = m_next_seqno++;
    ev.emit_time_us = GetTimeMicros();
    ev.payload      = std::move(payload);

    m_queue.push_back(std::move(ev));
}

std::vector<WalletEvent> WalletEventQueue::drain(std::size_t max_batch)
{
    std::deque<WalletEvent> taken;

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
    return std::vector<WalletEvent>(std::make_move_iterator(taken.begin()),
                                    std::make_move_iterator(taken.end()));
}

std::size_t WalletEventQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

} // namespace GRC
