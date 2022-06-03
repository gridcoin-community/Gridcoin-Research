// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_VOTING_POLL_TYPES_H
#define GRIDCOIN_QT_VOTING_POLL_TYPES_H

#include <QString>
#include <vector>

class PollTypeItem
{
public:
    QString m_name;
    QString m_description;
    int m_min_duration_days;
};

class PollTypes : public std::vector<PollTypeItem>
{
public:
    PollTypes();
};

#endif // GRIDCOIN_QT_VOTING_POLL_TYPES_H
