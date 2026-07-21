// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLL_TYPES_H
#define GRIDCOIN_QT_VOTING_POLL_TYPES_H

#include <QString>
#include <vector>

//! GUI-side mirror of the poll filter bit flags. The values match the core
//! GRC::PollFilterFlag (gridcoin/voting/filter.h); VotingModel casts these to
//! the int the interfaces::VotingManager takes, so the GUI needs no core header
//! for the enum. Kept in sync with the (stateless) core enum.
enum PollFilterFlag
{
    NO_FILTER = 0, //!< No active filter. Include all results.
    ACTIVE = 1,    //!< Include unfinished polls.
    FINISHED = 2,  //!< Include finished polls.
};

class PollTypeItem
{
public:
    QString m_name;
    QString m_description;
    int m_min_duration_days;
    std::vector<QString> m_required_fields;
};

class PollTypes : public std::vector<PollTypeItem>
{
public:
    PollTypes();
};

#endif // GRIDCOIN_QT_VOTING_POLL_TYPES_H
