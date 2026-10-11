// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/result.h"

#include <boost/test/unit_test.hpp>

#include <limits>

namespace {
using Weight = GRC::PollResult::Weight;

//! Largest total weight a poll result may report: every weight is shown
//! through ValueFromAmount, which takes a CAmount.
constexpr Weight MAX_WEIGHT = std::numeric_limits<CAmount>::max();

GRC::PollResult MakeResult()
{
    // Yes/no/abstain: three responses.
    return GRC::PollResult(GRC::Poll(
        GRC::PollType::SURVEY,
        GRC::PollWeightType::BALANCE,
        GRC::PollResponseType::YES_NO_ABSTAIN,
        7,
        "Title",
        "https://gridcoin.us",
        "Question?",
        GRC::Poll::ChoiceList(),
        1700000000,
        Fraction()));
}

GRC::PollResult::VoteDetail MakeVote(const std::vector<std::pair<uint8_t, Weight>>& responses)
{
    GRC::PollResult::VoteDetail vote;
    vote.m_ismine = ISMINE_NO;
    vote.m_responses = responses;

    return vote;
}
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(poll_result_tests)

BOOST_AUTO_TEST_CASE(tally_adds_each_vote_to_its_responses)
{
    GRC::PollResult result = MakeResult();

    result.TallyVote(MakeVote({{0, 300}}));
    result.TallyVote(MakeVote({{1, 200}, {2, 200}}));

    BOOST_CHECK_EQUAL(result.m_total_weight, 700);
    BOOST_CHECK_EQUAL(result.m_responses[0].m_weight, 300);
    BOOST_CHECK_EQUAL(result.m_responses[1].m_weight, 200);
    BOOST_CHECK_EQUAL(result.m_responses[2].m_weight, 200);
    BOOST_CHECK_EQUAL(result.m_invalid_votes, 0);
}

BOOST_AUTO_TEST_CASE(tally_counts_a_vote_that_would_overflow_the_total_as_invalid)
{
    GRC::PollResult result = MakeResult();

    result.TallyVote(MakeVote({{0, MAX_WEIGHT - 10}}));
    result.TallyVote(MakeVote({{1, 100}}));

    BOOST_CHECK_EQUAL(result.m_total_weight, MAX_WEIGHT - 10);
    BOOST_CHECK_EQUAL(result.m_responses[1].m_weight, 0);
    BOOST_CHECK_EQUAL(result.m_invalid_votes, 1);
}

BOOST_AUTO_TEST_CASE(tally_counts_a_vote_whose_own_responses_overflow_as_invalid)
{
    GRC::PollResult result = MakeResult();

    result.TallyVote(MakeVote({{0, MAX_WEIGHT / 2 + 1}, {1, MAX_WEIGHT / 2 + 1}}));

    BOOST_CHECK_EQUAL(result.m_total_weight, 0);
    BOOST_CHECK_EQUAL(result.m_responses[0].m_weight, 0);
    BOOST_CHECK_EQUAL(result.m_invalid_votes, 1);
}

BOOST_AUTO_TEST_CASE(minimum_vote_percent_is_the_floor_of_the_percentage)
{
    // 15% of an active vote weight of 1000 is 150.
    BOOST_CHECK(GRC::MeetsMinimumVotePercentAVW(150, 1000, 15));
    BOOST_CHECK(GRC::MeetsMinimumVotePercentAVW(159, 1000, 15)); // 15.9% floors to 15
    BOOST_CHECK(!GRC::MeetsMinimumVotePercentAVW(149, 1000, 15)); // 14.9% floors to 14
    BOOST_CHECK(GRC::MeetsMinimumVotePercentAVW(0, 1000, 0));
}

BOOST_AUTO_TEST_CASE(minimum_vote_percent_holds_at_the_top_of_the_range)
{
    BOOST_CHECK(GRC::MeetsMinimumVotePercentAVW(MAX_WEIGHT / 2 + 1, MAX_WEIGHT, 50));
    BOOST_CHECK(!GRC::MeetsMinimumVotePercentAVW(MAX_WEIGHT / 2 + 1, MAX_WEIGHT, 51));
    BOOST_CHECK(GRC::MeetsMinimumVotePercentAVW(MAX_WEIGHT, MAX_WEIGHT, 100));
}

BOOST_AUTO_TEST_CASE(minimum_vote_percent_needs_a_positive_active_vote_weight)
{
    BOOST_CHECK(!GRC::MeetsMinimumVotePercentAVW(100, 0, 15));
    BOOST_CHECK(!GRC::MeetsMinimumVotePercentAVW(100, -1, 15));
}

BOOST_AUTO_TEST_SUITE_END()
