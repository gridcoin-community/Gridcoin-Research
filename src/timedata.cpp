// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "timedata.h"

#include <algorithm>

namespace {
//!
//! \brief (a + b) / 2, truncating toward zero exactly as CMedianFilter does,
//! without the overflow a + b risks when both are extreme with the same sign.
//!
int64_t Midpoint(int64_t a, int64_t b)
{
    if ((a < 0) != (b < 0)) {
        return (a + b) / 2; // opposite signs cannot overflow
    }

    return a / 2 + b / 2 + (a % 2 + b % 2) / 2;
}
} // anonymous namespace

std::optional<int64_t> TimeOffsetVotes::Add(Owner owner, const Group& group, int64_t offset)
{
    const auto existing = m_votes.find(owner);

    if (existing != m_votes.end()) {
        existing->second = Vote{group, offset, ++m_sequence};
    } else {
        if (m_votes.size() >= MAX_PEER_VOTES) {
            m_votes.erase(std::min_element(
                m_votes.begin(),
                m_votes.end(),
                [](const auto& a, const auto& b) { return a.second.sequence < b.second.sequence; }));
        }

        m_votes.emplace(owner, Vote{group, offset, ++m_sequence});
    }

    return Median();
}

std::optional<int64_t> TimeOffsetVotes::Median() const
{
    // Recompute at every size, odd or even. AddTimeData() used to recompute
    // only at odd sizes of its 200-sample window; once the window filled, its
    // size never became odd again, and the offset froze for the rest of the
    // process.
    const std::vector<int64_t> sorted = Sorted();
    const size_t n = sorted.size();

    if (n < MIN_VOTES) {
        return std::nullopt;
    }

    if (n % 2 == 1) {
        return sorted[n / 2];
    }

    return Midpoint(sorted[n / 2 - 1], sorted[n / 2]);
}

bool TimeOffsetVotes::Remove(Owner owner)
{
    return m_votes.erase(owner) > 0;
}

std::vector<int64_t> TimeOffsetVotes::Sorted() const
{
    // The most recently cast vote of each network group.
    std::map<Group, const Vote*> latest;

    for (const auto& entry : m_votes) {
        const Vote& vote = entry.second;
        const auto inserted = latest.emplace(vote.group, &vote);

        if (!inserted.second && vote.sequence > inserted.first->second->sequence) {
            inserted.first->second = &vote;
        }
    }

    std::vector<int64_t> sorted;
    sorted.reserve(latest.size() + 1);

    sorted.push_back(0); // the node's own clock

    for (const auto& entry : latest) {
        sorted.push_back(entry.second->offset);
    }

    std::sort(sorted.begin(), sorted.end());

    return sorted;
}
