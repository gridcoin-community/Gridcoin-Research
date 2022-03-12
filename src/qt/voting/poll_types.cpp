// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/voting/poll_types.h"

#include <QCoreApplication>

namespace {
struct PollTypeDefinition
{
    const char* m_name;
    const char* m_description;
    int m_min_duration_days;
};

const PollTypeDefinition g_poll_types[] = {
    { "Unknown", "Unknown", 0 },
    {
        QT_TRANSLATE_NOOP("PollTypes", "Project Listing"),
        QT_TRANSLATE_NOOP("PollTypes", "Propose additions or removals of computing projects for research reward eligibility."),
        21, // min duration days
    },
    {
        QT_TRANSLATE_NOOP("PollTypes", "Protocol Development"),
        QT_TRANSLATE_NOOP("PollTypes", "Propose a change to Gridcoin at the protocol level."),
        42, // min duration days
    },
    {
        QT_TRANSLATE_NOOP("PollTypes", "Governance"),
        QT_TRANSLATE_NOOP("PollTypes", "Proposals related to Gridcoin management like poll requirements and funding."),
        21, // min duration days
    },
    {
        QT_TRANSLATE_NOOP("PollTypes", "Marketing"),
        QT_TRANSLATE_NOOP("PollTypes", "Propose marketing initiatives like ad campaigns."),
        21, // min duration days
    },
    {
        QT_TRANSLATE_NOOP("PollTypes", "Outreach"),
        QT_TRANSLATE_NOOP("PollTypes", "For polls about community representation, public relations, and communications."),
        21, // min duration days
    },
    {
        QT_TRANSLATE_NOOP("PollTypes", "Community"),
        QT_TRANSLATE_NOOP("PollTypes", "For other initiatives related to the Gridcoin community."),
        21, // min duration days
    },
    {
        QT_TRANSLATE_NOOP("PollTypes", "Survey"),
        QT_TRANSLATE_NOOP("PollTypes", "For opinion or casual polls without any particular requirements."),
        7, // min duration days
    },
};
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: PollTypes
// -----------------------------------------------------------------------------

PollTypes::PollTypes()
{
    for (const auto& type : g_poll_types) {
        emplace_back();
        back().m_name = QCoreApplication::translate("PollTypes", type.m_name);
        back().m_description = QCoreApplication::translate("PollTypes", type.m_description);
        back().m_min_duration_days = type.m_min_duration_days;
    }
}
