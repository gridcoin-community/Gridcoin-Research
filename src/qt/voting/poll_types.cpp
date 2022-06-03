// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/voting/poll.h"
#include "qt/voting/poll_types.h"

#include <QCoreApplication>

namespace {
struct PollTypeDefinition
{
    const char* m_name;
    const char* m_description;
    int m_min_duration_days;
};
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: PollTypes
// -----------------------------------------------------------------------------

PollTypes::PollTypes()
{
    // Load from core Poll class.
    for (const auto& type : GRC::Poll::POLL_TYPES) {
        if (type == GRC::PollType::OUT_OF_BOUND) continue;

        emplace_back();
        back().m_name = QString::fromStdString(GRC::Poll::PollTypeToString(type));
        back().m_description = QString::fromStdString(GRC::Poll::PollTypeToDescString(type));
        back().m_min_duration_days = GRC::Poll::POLL_TYPE_RULES[(int) type].m_mininum_duration;
    }
}
