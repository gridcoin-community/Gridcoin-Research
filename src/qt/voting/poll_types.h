// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLL_TYPES_H
#define GRIDCOIN_QT_VOTING_POLL_TYPES_H

#include <QString>
#include <vector>

namespace interfaces {
struct PollTypeMeta;
} // namespace interfaces

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
    //! Build the GUI poll-type rows from the node-side poll-type metadata
    //! (interfaces::VotingManager::getPollTypes()), preserving enum order so the
    //! wizard's button ids still index the PollType raw values.
    explicit PollTypes(const std::vector<interfaces::PollTypeMeta>& types);
};

#endif // GRIDCOIN_QT_VOTING_POLL_TYPES_H
