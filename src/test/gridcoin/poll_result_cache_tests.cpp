// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <gridcoin/voting/poll_result_cache.h>
#include <test/test_gridcoin.h>
#include <uint256.h>

// These tests cover the PollResultCache's invalidation policy — the novel logic
// the cache adds on top of PollResult::BuildFor. The policy is exercised through
// the pure GRC::PollResultReusable() the cache delegates to, so it needs no poll,
// no chain and no disk.
//
// The full BuildPollTable -> BuildFor path is NOT unit-tested here: BuildFor
// reads the poll transaction from a block file on disk (PollReference::
// TryReadFromDisk), and the test harness (TestingSetup) builds no chain and
// connects no blocks, so a poll cannot be made disk-readable in a unit test.
// That end-to-end path is covered by the testnet (mesh) soak instead.

namespace {
//! Two distinct, non-equal tip hashes for the reuse-policy cases.
const uint256 TIP_A = uint256S("00000000000000000000000000000000000000000000000000000000000000a1");
const uint256 TIP_B = uint256S("00000000000000000000000000000000000000000000000000000000000000b2");
} // namespace

BOOST_AUTO_TEST_SUITE(poll_result_cache_tests)

// Signature: PollResultReusable(cached_finished, now_finished,
//                               tallied_tip_hash, tallied_tip_height,
//                               current_tip_hash, current_tip_height)

// ---------------------------------------------------------------------------
// Active polls (still active now): reusable only while the tip is exactly the
// one tallied against (the active-vote-weight range ends at the tip and grows
// every block).
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(active_poll_reusable_when_tip_unchanged)
{
    BOOST_CHECK(GRC::PollResultReusable(/*cached_finished=*/false, /*now_finished=*/false,
                                        /*tallied_tip_hash=*/TIP_A, /*tallied_tip_height=*/100,
                                        /*current_tip_hash=*/TIP_A, /*current_tip_height=*/100));
}

BOOST_AUTO_TEST_CASE(active_poll_not_reusable_when_tip_advances)
{
    // A new block (new tip hash, higher height) must re-tally an active poll even
    // though nothing else changed — the AVW range now includes the new block.
    BOOST_CHECK(!GRC::PollResultReusable(false, false, TIP_A, 100, TIP_B, 101));
}

BOOST_AUTO_TEST_CASE(active_poll_not_reusable_on_same_height_reorg)
{
    // A same-height reorg to a different block changes the tip hash, so the active
    // tally is stale and must be rebuilt.
    BOOST_CHECK(!GRC::PollResultReusable(false, false, TIP_A, 100, TIP_B, 100));
}

BOOST_AUTO_TEST_CASE(active_poll_not_reusable_when_expired_by_time_on_stalled_tip)
{
    // The poll was active when tallied but has since expired by wall-clock time,
    // with the tip unchanged (stalled). The finished-state transition alone must
    // invalidate the entry, or the GUI would keep showing it active indefinitely.
    BOOST_CHECK(!GRC::PollResultReusable(/*cached_finished=*/false, /*now_finished=*/true,
                                         TIP_A, 100, TIP_A, 100));
}

// ---------------------------------------------------------------------------
// Closed polls (finished when tallied and still finished): immutable while the
// tip only advances; reusable exactly while the current height has not dropped
// below the height they were tallied at.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(closed_poll_reusable_when_tip_unchanged)
{
    BOOST_CHECK(GRC::PollResultReusable(/*cached_finished=*/true, /*now_finished=*/true,
                                        TIP_A, 100, TIP_A, 100));
}

BOOST_AUTO_TEST_CASE(closed_poll_reusable_when_tip_advances)
{
    // The tip moving forward (new hash, higher height) does not affect a closed
    // poll's fixed window, so the cached tally is still served.
    BOOST_CHECK(GRC::PollResultReusable(true, true, TIP_A, 100, TIP_B, 250));
}

BOOST_AUTO_TEST_CASE(closed_poll_reusable_on_same_height_reorg)
{
    // Closed polls gate on height only, not hash: a same-height reorg leaves the
    // (older) poll window untouched, so the tally stays valid.
    BOOST_CHECK(GRC::PollResultReusable(true, true, TIP_A, 100, TIP_B, 100));
}

BOOST_AUTO_TEST_CASE(closed_poll_not_reusable_on_backward_reorg)
{
    // A backward reorg (current height below the tally height) could drag the tip
    // below the poll's end block and reopen it, so the closed tally is discarded.
    BOOST_CHECK(!GRC::PollResultReusable(true, true, TIP_A, 100, TIP_B, 99));
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
