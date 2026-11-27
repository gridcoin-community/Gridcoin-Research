// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/voting/poll_entry.h"

using namespace GRC;

PollEntry::PollEntry()
    : m_poll_txid()
    , m_title()
    , m_payload_version(0)
    , m_type(0)
    , m_weight_type(0)
    , m_timestamp(0)
    , m_duration_days(0)
    , m_block_hash()
    , m_block_height(0)
    , m_hash()
    , m_previous_hash()
    , m_status(PollEntryStatus::UNKNOWN)
{
}

std::pair<std::string, std::string> PollEntry::KeyValueToString() const
{
    return std::make_pair(m_poll_txid.GetHex(), m_title);
}

std::string PollEntry::StatusToString() const
{
    switch (m_status.Value()) {
    case PollEntryStatus::UNKNOWN:      return "Unknown";
    case PollEntryStatus::DELETED:      return "Deleted";
    case PollEntryStatus::ACTIVE:       return "Active";
    case PollEntryStatus::OUT_OF_BOUND: break;
    }

    return "Out of bound";
}

VoteLinkEntry::VoteLinkEntry()
    : m_poll_txid()
    , m_timestamp(0)
    , m_hash()
    , m_previous_hash()
    , m_block_hash()
    , m_block_height(0)
    , m_status(VoteLinkStatus::UNKNOWN)
{
}

std::pair<std::string, std::string> VoteLinkEntry::KeyValueToString() const
{
    return std::make_pair(m_poll_txid.GetHex(), m_hash.GetHex());
}

std::string VoteLinkEntry::StatusToString() const
{
    switch (m_status.Value()) {
    case VoteLinkStatus::UNKNOWN:      return "Unknown";
    case VoteLinkStatus::ACTIVE:       return "Active";
    case VoteLinkStatus::OUT_OF_BOUND: break;
    }

    return "Out of bound";
}

template<> const std::string PollEntryDB::KeyType()
{
    return std::string("poll");
}

template<> const std::string VoteLinkDB::KeyType()
{
    return std::string("vote");
}
