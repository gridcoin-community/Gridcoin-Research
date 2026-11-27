// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "gridcoin/voting/poll_entry.h"
#include "streams.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(poll_entry_tests)

BOOST_AUTO_TEST_CASE(a_poll_entry_round_trips_through_serialization)
{
    GRC::PollEntry entry;
    entry.m_poll_txid = uint256S("01");
    entry.m_title = "Title";
    entry.m_payload_version = 3;
    entry.m_type = 1;
    entry.m_weight_type = 3;
    entry.m_timestamp = 1700000000;
    entry.m_duration_days = 7;
    entry.m_block_hash = uint256S("02");
    entry.m_block_height = 2053001;
    entry.m_hash = uint256S("01");
    entry.m_previous_hash = uint256();
    entry.m_status = GRC::PollEntryStatus::ACTIVE;

    CDataStream stream(SER_DISK, PROTOCOL_VERSION);
    stream << entry;

    GRC::PollEntry read;
    stream >> read;

    BOOST_CHECK(read.m_poll_txid == entry.m_poll_txid);
    BOOST_CHECK_EQUAL(read.m_title, "Title");
    BOOST_CHECK_EQUAL(read.m_payload_version, 3U);
    BOOST_CHECK_EQUAL(read.m_type, 1U);
    BOOST_CHECK_EQUAL(read.m_weight_type, 3U);
    BOOST_CHECK_EQUAL(read.m_timestamp, 1700000000);
    BOOST_CHECK_EQUAL(read.m_duration_days, 7U);
    BOOST_CHECK(read.m_block_hash == entry.m_block_hash);
    BOOST_CHECK_EQUAL(read.m_block_height, 2053001);
    BOOST_CHECK(read.m_hash == entry.m_hash);
    BOOST_CHECK(read.m_previous_hash.IsNull());
    BOOST_CHECK(read.m_status == GRC::PollEntryStatus::ACTIVE);
    BOOST_CHECK(read.Key() == entry.m_poll_txid);
}

BOOST_AUTO_TEST_CASE(a_vote_link_round_trips_through_serialization)
{
    GRC::VoteLinkEntry link;
    link.m_poll_txid = uint256S("01");
    link.m_timestamp = 1700000100;
    link.m_hash = uint256S("03");
    link.m_previous_hash = uint256S("04");
    link.m_block_hash = uint256S("05");
    link.m_block_height = 2053002;
    link.m_status = GRC::VoteLinkStatus::ACTIVE;

    CDataStream stream(SER_DISK, PROTOCOL_VERSION);
    stream << link;

    GRC::VoteLinkEntry read;
    stream >> read;

    BOOST_CHECK(read.m_poll_txid == link.m_poll_txid);
    BOOST_CHECK_EQUAL(read.m_timestamp, 1700000100);
    BOOST_CHECK(read.m_hash == link.m_hash);
    BOOST_CHECK(read.m_previous_hash == link.m_previous_hash);
    BOOST_CHECK(read.m_block_hash == link.m_block_hash);
    BOOST_CHECK_EQUAL(read.m_block_height, 2053002);
    BOOST_CHECK(read.m_status == GRC::VoteLinkStatus::ACTIVE);
    BOOST_CHECK(read.Key() == link.m_poll_txid);
}

BOOST_AUTO_TEST_CASE(a_vote_link_db_loads_records_in_chain_order)
{
    GRC::VoteLinkDB db(1);
    db.clear();

    const uint256 poll = uint256S("10");
    uint256 previous;

    for (int i = 1; i <= 3; ++i) {
        GRC::VoteLinkEntry link;
        link.m_poll_txid = poll;
        link.m_hash = ArithToUint256(arith_uint256(100 + i));
        link.m_previous_hash = previous;
        link.m_block_height = 2053000 + i;
        link.m_status = GRC::VoteLinkStatus::ACTIVE;
        BOOST_REQUIRE(db.insert(link.m_hash, link.m_block_height, link));
        previous = link.m_hash;
    }

    GRC::VoteLinkMap current;
    GRC::VoteLinkMap unused_pending;
    GRC::VoteLinkMap unused_expired;
    GRC::VoteLinkMap unused_first;

    db.clear_in_memory_only();
    BOOST_CHECK_EQUAL(db.Initialize(current, unused_pending, unused_expired, unused_first, false), 2053003);

    // The current map holds the newest link of each poll.
    BOOST_REQUIRE(current.count(poll));
    BOOST_CHECK(current.at(poll)->m_hash == ArithToUint256(arith_uint256(103)));

    db.clear();
}

BOOST_AUTO_TEST_SUITE_END()
