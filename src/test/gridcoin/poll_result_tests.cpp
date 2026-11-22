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

    // sign-compare checked: m_total_weight is Weight (uint64_t); 700U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_total_weight, 700U);
    // sign-compare checked: m_weight is Weight (uint64_t); 300U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_responses[0].m_weight, 300U);
    // sign-compare checked: m_weight is Weight (uint64_t); 200U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_responses[1].m_weight, 200U);
    // sign-compare checked: m_weight is Weight (uint64_t); 200U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_responses[2].m_weight, 200U);
    // sign-compare checked: m_invalid_votes is size_t; 0U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_invalid_votes, 0U);
}

//!
//! The wallet holder's counted votes combine into one detail: their amounts and
//! their weight for each choice add up, and the CPID and magnitude are those of
//! the newest, which the counter tallies first.
//!
BOOST_AUTO_TEST_CASE(tally_combines_the_wallet_holders_votes)
{
    GRC::PollResult result = MakeResult();

    GRC::PollResult::VoteDetail newest = MakeVote({{0, 300}});
    newest.m_ismine = ISMINE_SPENDABLE;
    newest.m_amount = 300;
    newest.m_mining_id = GRC::Cpid::Parse("00010203040506070809101112131415");
    newest.m_magnitude = GRC::Magnitude::RoundFrom(10);

    GRC::PollResult::VoteDetail older = MakeVote({{0, 100}, {1, 100}});
    older.m_ismine = ISMINE_SPENDABLE;
    older.m_amount = 200;
    older.m_mining_id = GRC::MiningId::ForNoncruncher();

    result.TallyVote(newest);
    result.TallyVote(older);

    BOOST_REQUIRE(result.m_self_voted);
    // sign-compare checked: m_amount is Weight (uint64_t); 500U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_self_vote_detail.m_amount, 500U);
    BOOST_CHECK(result.m_self_vote_detail.m_mining_id == newest.m_mining_id);
    BOOST_CHECK_EQUAL(result.m_self_vote_detail.m_magnitude.Scaled(), newest.m_magnitude.Scaled());
    BOOST_REQUIRE_EQUAL(result.m_self_vote_detail.m_responses.size(), 2U);
    // sign-compare checked: .second is Weight (uint64_t); 400U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_self_vote_detail.m_responses[0].second, 400U);
    // sign-compare checked: .second is Weight (uint64_t); 100U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_self_vote_detail.m_responses[1].second, 100U);
}

BOOST_AUTO_TEST_CASE(tally_counts_a_vote_that_would_overflow_the_total_as_invalid)
{
    GRC::PollResult result = MakeResult();

    result.TallyVote(MakeVote({{0, MAX_WEIGHT - 10}}));
    result.TallyVote(MakeVote({{1, 100}}));

    BOOST_CHECK_EQUAL(result.m_total_weight, MAX_WEIGHT - 10);
    // sign-compare checked: m_weight is Weight (uint64_t); 0U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_responses[1].m_weight, 0U);
    // sign-compare checked: m_invalid_votes is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_invalid_votes, 1U);
}

BOOST_AUTO_TEST_CASE(tally_counts_a_vote_whose_own_responses_overflow_as_invalid)
{
    GRC::PollResult result = MakeResult();

    result.TallyVote(MakeVote({{0, MAX_WEIGHT / 2 + 1}, {1, MAX_WEIGHT / 2 + 1}}));

    // sign-compare checked: m_total_weight is Weight (uint64_t); 0U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_total_weight, 0U);
    // sign-compare checked: m_weight is Weight (uint64_t); 0U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_responses[0].m_weight, 0U);
    // sign-compare checked: m_invalid_votes is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_CHECK_EQUAL(result.m_invalid_votes, 1U);
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

BOOST_AUTO_TEST_CASE(legacy_vote_amount_converts_a_balance_that_fits_a_weight)
{
    BOOST_CHECK(GRC::LegacyVoteAmount(0) == Weight{0});
    BOOST_CHECK(GRC::LegacyVoteAmount(1.5) == Weight{150'000'000});

    // Truncation toward zero: a negative zero, or a fraction of a unit below
    // zero, converts to 0.
    BOOST_CHECK(GRC::LegacyVoteAmount(-0.0) == Weight{0});
    BOOST_CHECK(GRC::LegacyVoteAmount(-0.5 / COIN) == Weight{0});

    // A large balance that still fits a weight converts unchanged.
    BOOST_CHECK(GRC::LegacyVoteAmount(100'000'000'000.0) == Weight{100'000'000'000} * COIN);
}

BOOST_AUTO_TEST_CASE(legacy_vote_amount_refuses_a_balance_no_weight_can_hold)
{
    BOOST_CHECK(!GRC::LegacyVoteAmount(-1));
    BOOST_CHECK(!GRC::LegacyVoteAmount(-1.0 / COIN));
    BOOST_CHECK(!GRC::LegacyVoteAmount(std::numeric_limits<double>::quiet_NaN()));
    BOOST_CHECK(!GRC::LegacyVoteAmount(std::numeric_limits<double>::infinity()));
    BOOST_CHECK(!GRC::LegacyVoteAmount(1e20));

    // A 64-bit weight holds less than 2^64 base units, about 184467440737.1 GRC.
    BOOST_CHECK(GRC::LegacyVoteAmount(184'467'440'737.0));
    BOOST_CHECK(!GRC::LegacyVoteAmount(184'467'440'738.0));
}

BOOST_AUTO_TEST_SUITE_END()
