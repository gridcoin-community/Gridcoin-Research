// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <gridcoin/voting/poll_result_cache.h>
#include <gridcoin/voting/registry.h>
#include <test/test_gridcoin.h>
#include <uint256.h>

#include <deque>
#include <utility>

namespace {
//! A block index entry with its own hash. The entries outlive every test (a deque keeps their addresses).
const CBlockIndex* Block(const char* hex)
{
    static std::deque<std::pair<uint256, CBlockIndex>> blocks;
    blocks.emplace_back(uint256S(hex), CBlockIndex {});
    blocks.back().second.phashBlock = &blocks.back().first;

    return &blocks.back().second;
}

//! A snapshot of one poll on \p tip, finished with \p past_end as its first block past the end when that is set.
GRC::PollSnapshot Snap(const CBlockIndex* tip, const CBlockIndex* past_end, std::vector<uint256> votes = {})
{
    GRC::PollSnapshot snapshot;
    snapshot.m_txid = uint256S("01");
    snapshot.m_pindex_tip = tip;
    snapshot.m_pindex_past_end = past_end;
    snapshot.m_finished = past_end != nullptr;
    snapshot.m_votes = std::move(votes);

    return snapshot;
}

const CBlockIndex* const TIP_A = Block("a1");
const CBlockIndex* const TIP_B = Block("b2");
const CBlockIndex* const PAST_END = Block("e1");
const CBlockIndex* const OTHER_PAST_END = Block("e2");
const uint256 VOTE_1 = uint256S("11");
const uint256 VOTE_2 = uint256S("12");
} // namespace

BOOST_AUTO_TEST_SUITE(poll_result_cache_tests)

BOOST_AUTO_TEST_CASE(a_finished_polls_key_holds_while_the_tip_advances)
{
    BOOST_CHECK(GRC::KeyFor(Snap(TIP_A, PAST_END)) == GRC::KeyFor(Snap(TIP_B, PAST_END)));
}

BOOST_AUTO_TEST_CASE(a_finished_polls_key_changes_with_its_first_block_past_the_end)
{
    BOOST_CHECK(GRC::KeyFor(Snap(TIP_A, PAST_END)) != GRC::KeyFor(Snap(TIP_A, OTHER_PAST_END)));
}

BOOST_AUTO_TEST_CASE(an_open_polls_key_is_its_tip)
{
    BOOST_CHECK(GRC::KeyFor(Snap(TIP_A, nullptr)) == GRC::KeyFor(Snap(TIP_A, nullptr)));
    BOOST_CHECK(GRC::KeyFor(Snap(TIP_A, nullptr)) != GRC::KeyFor(Snap(TIP_B, nullptr)));
}

BOOST_AUTO_TEST_CASE(a_poll_that_finishes_at_its_tip_changes_its_key)
{
    // Open at TIP_A, then finished with TIP_A as its first block past the end: same block, different key.
    BOOST_CHECK(GRC::KeyFor(Snap(TIP_A, nullptr)) != GRC::KeyFor(Snap(TIP_A, TIP_A)));
}

BOOST_AUTO_TEST_CASE(a_vote_linked_unlinked_or_reordered_changes_the_key)
{
    BOOST_CHECK(GRC::KeyFor(Snap(TIP_A, PAST_END, {VOTE_1})) != GRC::KeyFor(Snap(TIP_A, PAST_END, {VOTE_1, VOTE_2})));
    BOOST_CHECK(GRC::KeyFor(Snap(TIP_A, PAST_END, {VOTE_1, VOTE_2})) != GRC::KeyFor(Snap(TIP_A, PAST_END, {VOTE_2, VOTE_1})));
}

// ---------------------------------------------------------------------------
// The singleton is usable and Clear() is safe on an empty cache.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(singleton_clear_is_safe)
{
    GRC::PollResultCache& cache = GRC::GetPollResultCache();
    BOOST_CHECK(&cache == &GRC::GetPollResultCache());
    BOOST_CHECK_NO_THROW(cache.Clear());
}

BOOST_AUTO_TEST_SUITE_END()
