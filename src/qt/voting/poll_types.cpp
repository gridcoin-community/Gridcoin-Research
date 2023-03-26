// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/voting/poll.h"
#include "qt/voting/poll_types.h"

#include <QCoreApplication>

// -----------------------------------------------------------------------------
// Class: PollTypes
// -----------------------------------------------------------------------------

PollTypes::PollTypes()
{
    // Load from core Poll class. Note we do not use PollPayload::GetValidPollTypes, because with poll v2 the UI supported
    // the other poll types from a user perspective, but in fact only submitted the polls as "SURVEY" in the core.
    // GetValidPollTypes will only return SURVEY for v2, so it is not appropriate to use here.
    //
    // I am not particularly fond of how this is crosswired into the GUI code here, but it will suffice for now.
    // TODO: refactor poll types between core and UI.
    for (const auto& type : GRC::Poll::POLL_TYPES) {
        if (type == GRC::PollType::OUT_OF_BOUND) continue;

        emplace_back();
        // Note that these use the default value for the translated boolean, which is true.
        back().m_name = QString::fromStdString(GRC::Poll::PollTypeToString(type));
        back().m_description = QString::fromStdString(GRC::Poll::PollTypeToDescString(type));
        back().m_min_duration_days = GRC::Poll::POLL_TYPE_RULES[(int) type].m_mininum_duration;

        std::vector<QString> required_fields;
        for (const auto& iter : GRC::Poll::POLL_TYPE_RULES[(int) type].m_required_fields) {
            required_fields.push_back(QString::fromStdString(iter));
        }

        back().m_required_fields = required_fields;
    }
}
