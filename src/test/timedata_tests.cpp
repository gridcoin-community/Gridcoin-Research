// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <netbase.h>
#include <timedata.h>
#include <util.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {
//! A distinct peer group per index. TimeOffsetVotes only compares group bytes,
//! so these need not look like a real CNetAddr::GetGroup() result.
TimeOffsetVotes::Group G(uint32_t i)
{
    return {1,
            static_cast<unsigned char>(i >> 16),
            static_cast<unsigned char>(i >> 8),
            static_cast<unsigned char>(i)};
}

CNetAddr Addr(const std::string& ip)
{
    CNetAddr addr;
    BOOST_REQUIRE(LookupHost(ip.c_str(), addr, false));
    return addr;
}

//! The offset AddTimeData() stores for a median: the same expression.
int64_t Weighted(int64_t median)
{
    return static_cast<int64_t>(0.95 * median);
}

//! Starts and ends a test with no time votes and a zero offset, so a failed
//! check cannot leave the process-wide offset set for later suites.
struct TimeDataReset
{
    TimeDataReset() { ResetTimeDataForTesting(); }
    ~TimeDataReset() { ResetTimeDataForTesting(); }
};
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(timedata_tests)

BOOST_AUTO_TEST_CASE(own_clock_is_the_only_vote_initially)
{
    TimeOffsetVotes votes;

    BOOST_CHECK_EQUAL(votes.Size(), 1U);
    BOOST_CHECK(votes.Sorted() == std::vector<int64_t>{0});
}

BOOST_AUTO_TEST_CASE(median_needs_four_peer_groups)
{
    TimeOffsetVotes votes;

    BOOST_CHECK(!votes.Add(1, G(1), 10));
    BOOST_CHECK(!votes.Add(2, G(2), 20));
    BOOST_CHECK(!votes.Add(3, G(3), 30));

    // The fourth peer group makes five votes with the node's own 0:
    // {0, 10, 20, 30, 40}.
    const std::optional<int64_t> median = votes.Add(4, G(4), 40);

    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, 20);
}

BOOST_AUTO_TEST_CASE(even_vote_count_still_produces_a_median)
{
    TimeOffsetVotes votes;

    for (uint32_t i = 1; i <= 4; ++i) {
        votes.Add(i, G(i), 10 * i);
    }

    // Six votes: {0, 10, 20, 30, 40, 50}.
    const std::optional<int64_t> median = votes.Add(5, G(5), 50);

    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, 25);
}

BOOST_AUTO_TEST_CASE(median_matches_the_old_filter_until_its_window_fills)
{
    // Before its 200-sample window filled, the old CMedianFilter-based code
    // produced the same median whenever it updated (odd sizes). Check the
    // arithmetic is identical at every size, odd and even, including the
    // truncating average of the two middle values and negative offsets.
    std::mt19937_64 rng(3402);
    std::uniform_int_distribution<int64_t> offset(-480, 480);

    TimeOffsetVotes votes;
    CMedianFilter<int64_t> old_filter(200, 0);

    for (uint32_t i = 0; i < TimeOffsetVotes::MAX_PEER_VOTES; ++i) {
        const int64_t sample = offset(rng);
        const std::optional<int64_t> median = votes.Add(i, G(i), sample);
        old_filter.input(sample);

        BOOST_CHECK_EQUAL(votes.Size(), static_cast<size_t>(old_filter.size()));

        if (old_filter.size() >= 5) {
            BOOST_REQUIRE(median);
            BOOST_CHECK_EQUAL(*median, old_filter.median());
        } else {
            BOOST_CHECK(!median);
        }
    }
}

BOOST_AUTO_TEST_CASE(extreme_votes_do_not_overflow_the_even_count_median)
{
    // Averaging the two middle votes of an even count must not overflow when
    // both are extreme with the same sign. The old code never averaged in
    // practice (it only updated at odd sizes).
    const int64_t max = std::numeric_limits<int64_t>::max();
    const int64_t min = std::numeric_limits<int64_t>::min();

    TimeOffsetVotes high;
    std::optional<int64_t> median;

    for (uint32_t i = 1; i <= 5; ++i) {
        median = high.Add(i, G(i), max);
    }

    // {0, max, max, max, max, max}: middles max and max.
    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, max);

    TimeOffsetVotes low;

    for (uint32_t i = 1; i <= 5; ++i) {
        median = low.Add(i, G(i), min);
    }

    // {min, min, min, min, min, 0}: middles min and min.
    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, min);

    TimeOffsetVotes mixed;

    for (uint32_t i = 1; i <= 3; ++i) {
        mixed.Add(i, G(i), min);
    }

    mixed.Add(4, G(4), max);
    median = mixed.Add(5, G(5), max);

    // {min, min, min, 0, max, max}: middles min and 0.
    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, min / 2);
}

BOOST_AUTO_TEST_CASE(keeps_updating_as_outbound_peers_churn)
{
    // The old code stopped updating for good after 199 outbound samples. Here
    // four honest peers are replaced, one at a time, by peers in new groups
    // reporting +300, well past the old freeze point.
    TimeOffsetVotes votes;
    std::deque<uint32_t> live;

    for (uint32_t i = 0; i < 4; ++i) {
        votes.Add(i, G(i), 0);
        live.push_back(i);
    }

    std::optional<int64_t> median;

    for (uint32_t i = 0; i < 400; ++i) {
        votes.Remove(live.front()); // the longest-connected peer disconnects
        live.pop_front();

        const uint32_t peer = 1000 + i;
        median = votes.Add(peer, G(peer), i < 396 ? 0 : 300);
        live.push_back(peer);
        BOOST_REQUIRE(median);
    }

    // The four live votes are the last four peers, all +300: {0, 300 x 4}.
    BOOST_CHECK_EQUAL(votes.Size(), 5U);
    BOOST_CHECK_EQUAL(*median, 300);
}

BOOST_AUTO_TEST_CASE(a_disconnected_peer_leaves_nothing_behind)
{
    TimeOffsetVotes votes;

    for (uint32_t i = 1; i <= 4; ++i) {
        votes.Add(i, G(i), 0);
    }

    // A thousand peers, each in a new group, connect, vote -480 and disconnect.
    // None of them displaces an honest vote.
    for (uint32_t i = 0; i < 1000; ++i) {
        const std::optional<int64_t> median = votes.Add(100 + i, G(100 + i), -480);
        BOOST_REQUIRE(median);
        BOOST_CHECK_EQUAL(*median, 0); // {-480, 0, 0, 0, 0}
        votes.Remove(100 + i);
    }

    BOOST_CHECK(votes.Sorted() == (std::vector<int64_t>{0, 0, 0, 0, 0}));
}

BOOST_AUTO_TEST_CASE(moving_the_median_takes_a_simultaneous_majority)
{
    TimeOffsetVotes votes;

    for (uint32_t i = 1; i <= 4; ++i) {
        votes.Add(i, G(i), 0);
    }

    // Four live attacker votes against four honest ones and the node's own:
    // {-480 x 4, 0 x 5}. Still the honest median.
    std::optional<int64_t> median;

    for (uint32_t i = 11; i <= 14; ++i) {
        median = votes.Add(i, G(i), -480);
    }

    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, 0);

    // A fifth ties them with the four honest votes and the node's own, and a
    // tie already moves the median halfway: {-480 x 5, 0 x 5}.
    median = votes.Add(15, G(15), -480);

    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, -240);

    // A sixth, simultaneous, attacker vote takes the majority.
    median = votes.Add(16, G(16), -480);

    BOOST_REQUIRE(median);
    BOOST_CHECK_EQUAL(*median, -480);
}

BOOST_AUTO_TEST_CASE(a_group_counts_once_with_its_latest_live_vote)
{
    TimeOffsetVotes votes;

    for (uint32_t i = 1; i <= 4; ++i) {
        votes.Add(i, G(i), 0);
    }

    // Many connected peers in one group (one IPv4 /16 or IPv6 /32) count once.
    for (uint32_t i = 0; i < 100; ++i) {
        votes.Add(100 + i, G(99), 400);
    }

    BOOST_CHECK_EQUAL(votes.Size(), 6U);

    // The group's most recently cast vote is the one counted, and when that
    // peer goes, the group falls back to its other live votes.
    votes.Add(500, G(99), -200);
    BOOST_CHECK(votes.Sorted() == (std::vector<int64_t>{-200, 0, 0, 0, 0, 0}));

    votes.Remove(500);
    BOOST_CHECK(votes.Sorted() == (std::vector<int64_t>{0, 0, 0, 0, 0, 400}));
}

BOOST_AUTO_TEST_CASE(a_new_sample_replaces_the_peers_earlier_vote)
{
    TimeOffsetVotes votes;

    votes.Add(1, G(1), 100);
    votes.Add(1, G(1), -100);

    BOOST_CHECK(votes.Sorted() == (std::vector<int64_t>{-100, 0}));
}

BOOST_AUTO_TEST_CASE(remove_reports_whether_a_vote_was_withdrawn)
{
    TimeOffsetVotes votes;

    votes.Add(1, G(1), 100);

    BOOST_CHECK(!votes.Remove(2));
    BOOST_CHECK(votes.Sorted() == (std::vector<int64_t>{0, 100}));

    BOOST_CHECK(votes.Remove(1));
    BOOST_CHECK(!votes.Remove(1));
    BOOST_CHECK(votes.Sorted() == std::vector<int64_t>{0});
}

BOOST_AUTO_TEST_CASE(the_memory_bound_drops_the_vote_cast_longest_ago)
{
    TimeOffsetVotes votes;

    votes.Add(0, G(0), 55);  // cast first
    votes.Add(1, G(1), -77); // cast second

    for (uint32_t i = 2; i < TimeOffsetVotes::MAX_PEER_VOTES; ++i) {
        votes.Add(i, G(i), 0);
    }

    // Recasting a stored vote must not drop anything, even at the bound. It
    // makes peer 1's the vote cast longest ago.
    votes.Add(0, G(0), 55);

    std::vector<int64_t> sorted = votes.Sorted();

    BOOST_CHECK_EQUAL(votes.Size(), 200U);
    BOOST_CHECK_EQUAL(std::count(sorted.begin(), sorted.end(), -77), 1);

    // A new peer past the bound drops peer 1's vote, not the recast peer 0's.
    votes.Add(5000, G(5000), 0);
    sorted = votes.Sorted();

    BOOST_CHECK_EQUAL(votes.Size(), 200U);
    BOOST_CHECK_EQUAL(std::count(sorted.begin(), sorted.end(), 55), 1);
    BOOST_CHECK_EQUAL(std::count(sorted.begin(), sorted.end(), -77), 0);
}

BOOST_AUTO_TEST_CASE(add_time_data_sets_the_weighted_offset)
{
    // Through the real entry points: group keying from CNetAddr, the odd and
    // even vote counts, the 0.95 weight, and vote withdrawal.
    const TimeDataReset reset;

    AddTimeData(1, Addr("1.1.0.1"), 100);
    AddTimeData(2, Addr("2.2.0.1"), 100);
    AddTimeData(3, Addr("3.3.0.1"), 200);
    BOOST_CHECK_EQUAL(GetTimeOffset(), 0); // three peer groups: no median yet

    AddTimeData(4, Addr("4.4.0.1"), 200); // {0, 100, 100, 200, 200}
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(100));

    AddTimeData(5, Addr("5.5.0.1"), 200); // {0, 100, 100, 200, 200, 200}: even count
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(150));

    // Three more peers inside 1.1.0.0/16 count once, with the latest vote:
    // {-400, 0, 100, 200, 200, 200}. Keyed per address instead, the nine
    // votes {-400 x 3, 0, 100 x 2, 200 x 3} would give a median of 100.
    AddTimeData(6, Addr("1.1.7.1"), -400);
    AddTimeData(7, Addr("1.1.8.1"), -400);
    AddTimeData(8, Addr("1.1.9.1"), -400);
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(150));

    // Peers 5 and 8 disconnect, and the offset follows at once. 1.1/16 still
    // counts, now with peer 7's -400: {-400, 0, 100, 200, 200}.
    RemoveTimeData(5);
    RemoveTimeData(8);
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(100));

    // The next sample is counted over live votes only:
    // {-400, 0, 0, 100, 200, 200}. Had the withdrawn votes stayed, the seven
    // votes {-400, 0, 0, 100, 200, 200, 200} would give a median of 100.
    AddTimeData(9, Addr("9.9.0.1"), 0);
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(50));
}

BOOST_AUTO_TEST_CASE(a_departed_vote_leaves_no_trace_in_the_offset)
{
    const TimeDataReset reset;

    AddTimeData(1, Addr("1.1.0.1"), -100);
    AddTimeData(2, Addr("2.2.0.1"), -100);
    AddTimeData(3, Addr("3.3.0.1"), 100);
    AddTimeData(4, Addr("4.4.0.1"), 100); // {-100, -100, 0, 100, 100}
    BOOST_CHECK_EQUAL(GetTimeOffset(), 0);

    // A peer connects, votes +480 and disconnects:
    // {-100, -100, 0, 100, 100, 480} while it is connected.
    AddTimeData(5, Addr("5.5.0.1"), 480);
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(50));

    RemoveTimeData(5);
    BOOST_CHECK_EQUAL(GetTimeOffset(), 0);
}

BOOST_AUTO_TEST_CASE(without_a_quorum_the_offset_falls_back_to_zero)
{
    const TimeDataReset reset;

    AddTimeData(1, Addr("1.1.0.1"), 100);
    AddTimeData(2, Addr("2.2.0.1"), 100);
    AddTimeData(3, Addr("3.3.0.1"), 200);
    AddTimeData(4, Addr("4.4.0.1"), 200); // {0, 100, 100, 200, 200}
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(100));

    // Down to three live peer groups: no quorum, so the node's own clock.
    RemoveTimeData(4);
    BOOST_CHECK_EQUAL(GetTimeOffset(), 0);

    // A fourth group again restores the median: {0, 100, 100, 200, 300}.
    AddTimeData(5, Addr("5.5.0.1"), 300);
    BOOST_CHECK_EQUAL(GetTimeOffset(), Weighted(100));
}

BOOST_AUTO_TEST_SUITE_END()
