// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_POLL_ENTRY_H
#define GRIDCOIN_VOTING_POLL_ENTRY_H

#include "gridcoin/contract/registry_db.h"
#include "gridcoin/support/enumbytes.h"
#include "serialize.h"
#include "uint256.h"

#include <map>
#include <memory>
#include <string>

namespace GRC {

//!
//! \brief Status of a poll record. ACTIVE is a poll's own ADD record; DELETED is the record a master-key REMOVE
//! stores. UNKNOWN and OUT_OF_BOUND are required by RegistryDB, which skips them on load.
//!
enum class PollEntryStatus
{
    UNKNOWN,
    DELETED,
    ACTIVE,
    OUT_OF_BOUND
};

//!
//! \brief One record of the poll db: a poll's ADD, or the DELETED record of the master-key REMOVE that deleted it.
//!
//! The record chain of a poll is keyed by the poll's txid. The ADD record's hash is the poll's txid and its back-link
//! is null; a DELETED record's hash is the REMOVE transaction's hash and its back-link is the ADD record's hash.
//! The poll's fields are copied into each record so the load rebuilds a poll reference from the records alone.
//!
class PollEntry
{
public:
    using Status = EnumByte<PollEntryStatus>;

    uint256 m_poll_txid;          //!< Key: the poll's txid.
    std::string m_title;          //!< The poll's title as in its contract (display form).
    uint32_t m_payload_version;   //!< The poll payload's version.
    uint32_t m_type;              //!< PollType, as its integer value.
    uint32_t m_weight_type;       //!< PollWeightType, as its integer value.
    int64_t m_timestamp;          //!< Time of the poll's transaction.
    uint32_t m_duration_days;     //!< Number of days the poll runs.
    uint256 m_block_hash;         //!< The block that contains the poll.
    int m_block_height;           //!< Height of that block.
    uint256 m_hash;               //!< ADD: the poll's txid. DELETED: the REMOVE transaction's hash.
    uint256 m_previous_hash;      //!< ADD: null. DELETED: the ADD record's hash.
    Status m_status;              //!< ACTIVE or DELETED.

    PollEntry();

    uint256 Key() const { return m_poll_txid; }
    std::pair<std::string, std::string> KeyValueToString() const;
    std::string StatusToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_poll_txid);
        READWRITE(m_title);
        READWRITE(m_payload_version);
        READWRITE(m_type);
        READWRITE(m_weight_type);
        READWRITE(m_timestamp);
        READWRITE(m_duration_days);
        READWRITE(m_block_hash);
        READWRITE(m_block_height);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_status);
    }
};

//!
//! \brief Status of a vote link. Links are only ever ACTIVE: a reverted link is erased, not marked.
//!
enum class VoteLinkStatus
{
    UNKNOWN,
    ACTIVE,
    OUT_OF_BOUND
};

//!
//! \brief One record of the vote db: a vote linked to a poll.
//!
//! The links of a poll form a record chain keyed by the poll's txid, in connection order (block order, then contract
//! application order in the block). Each link's back-link is the poll's previous link, null for the first. Only the
//! newest link of a poll stays in memory; older links stay in LevelDB and are read with RegistryDB::peek().
//!
class VoteLinkEntry
{
public:
    using Status = EnumByte<VoteLinkStatus>;

    uint256 m_poll_txid;          //!< Key: the poll's txid.
    int64_t m_timestamp;          //!< Time of the vote's transaction (logs only).
    uint256 m_hash;               //!< The vote transaction's hash (one vote contract per transaction).
    uint256 m_previous_hash;      //!< The poll's previous link, or null for the first.
    uint256 m_block_hash;         //!< The block that contains the vote.
    int m_block_height;           //!< Height of that block.
    Status m_status;              //!< Always ACTIVE.

    VoteLinkEntry();

    uint256 Key() const { return m_poll_txid; }
    std::pair<std::string, std::string> KeyValueToString() const;
    std::string StatusToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_poll_txid);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_block_hash);
        READWRITE(m_block_height);
        READWRITE(m_status);
    }
};

using PollEntry_ptr = std::shared_ptr<PollEntry>;
using PollEntryMap = std::map<uint256, PollEntry_ptr>;           //!< Poll txid to its newest record.
using HistoricalPollEntryMap = std::map<uint256, PollEntry_ptr>; //!< Record hash to record.

using VoteLinkEntry_ptr = std::shared_ptr<VoteLinkEntry>;
using VoteLinkMap = std::map<uint256, VoteLinkEntry_ptr>;           //!< Poll txid to its newest link.
using HistoricalVoteLinkMap = std::map<uint256, VoteLinkEntry_ptr>; //!< Vote txid to link.

using PollEntryDB = RegistryDB<PollEntry, PollEntry, PollEntryStatus, PollEntryMap, PollEntryMap, PollEntryMap,
                               HistoricalPollEntryMap>;
using VoteLinkDB = RegistryDB<VoteLinkEntry, VoteLinkEntry, VoteLinkStatus, VoteLinkMap, VoteLinkMap, VoteLinkMap,
                              HistoricalVoteLinkMap>;

} // namespace GRC

#endif // GRIDCOIN_VOTING_POLL_ENTRY_H
