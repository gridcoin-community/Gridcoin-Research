// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/voting.h"
#include "qt/voting/poll_types.h"

#include <QCoreApplication>

// -----------------------------------------------------------------------------
// Class: PollTypes
// -----------------------------------------------------------------------------

PollTypes::PollTypes(const std::vector<interfaces::PollTypeMeta>& types)
{
    // The node resolves the poll-type metadata (name, description, minimum
    // duration and required fields) from the core Poll tables and hands it over
    // as value rows in enum order (OUT_OF_BOUND already dropped), so the GUI no
    // longer reaches into gridcoin/voting/poll.h. Note that pre-v3 the UI still
    // presents every type even though the core only submits SURVEY; the node
    // side preserves that by returning the full type list here.
    for (const interfaces::PollTypeMeta& type : types) {
        emplace_back();
        back().m_name = QString::fromStdString(type.name);
        back().m_description = QString::fromStdString(type.description);
        back().m_min_duration_days = type.min_duration_days;

        std::vector<QString> required_fields;
        for (const auto& field : type.required_fields) {
            required_fields.push_back(QString::fromStdString(field));
        }

        back().m_required_fields = required_fields;
    }
}
