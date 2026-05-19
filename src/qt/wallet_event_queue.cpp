// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet_event_queue.h"

#include "util/time.h"

#include <algorithm>
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
    std::vector<WalletEvent> out;

    std::lock_guard<std::mutex> lock(m_mutex);
    const std::size_t n = std::min(max_batch, m_queue.size());
    out.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(std::move(m_queue.front()));
        m_queue.pop_front();
    }

    return out;
}

std::size_t WalletEventQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

} // namespace GRC
