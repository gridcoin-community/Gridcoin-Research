// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file chain_setup_tests.cpp
//! \brief The regtest chain fixture's own clock (test/chain_setup.h).
//!
//! In a suite of their own: these cases mine, and each mine stakes one of the
//! ten premine outputs that a suite's cases share. A suite starts from genesis
//! with all ten unspent, so mining here takes none from another suite.
//!

#include "chain.h"
#include "gridcoin/staking/kernel.h"
#include "primitives/block.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "util.h"
#include "util/time.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <string>

namespace {

//! The latest time of any block this process holds an index entry for. That
//! includes the blocks of fixture suites that ran before this one, which this
//! fixture's LoadBlockIndex reloaded.
int64_t LatestKnownBlockTime()
{
    LOCK(cs_main);

    int64_t latest = 0;

    for (const auto& entry : mapBlockIndex) {
        latest = std::max(latest, entry.second->GetBlockTime());
    }

    return latest;
}

//! Puts the stake minimum age back however the case exits.
struct StakeMinAgeReset
{
    const unsigned int m_stake_min_age{nStakeMinAge};

    ~StakeMinAgeReset() { nStakeMinAge = m_stake_min_age; }
};

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(chain_setup_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

//! A case that mines reads the result against GetAdjustedTime() -- an output may
//! stake only from its confirming block's time, and the miner leaves out a
//! transaction stamped after the block -- so each mined block must be stamped
//! after its parent and not after the clock the case reads next. The fixture
//! stamps it exactly at the clock, in a slot later than any block the process
//! holds an index entry for. Those include an earlier fixture suite's blocks:
//! their index records and stake proofs outlive that suite, and a block mined
//! in one of their slots can be refused as a duplicate. On the wall clock,
//! blocks mined back to back fell in one 16-second slot.
BOOST_AUTO_TEST_CASE(each_mined_block_is_stamped_in_a_new_slot_at_the_clock)
{
    for (int i = 0; i < 3; ++i) {
        const int64_t latest_known = LatestKnownBlockTime();

        CBlock block;
        std::string err;
        BOOST_REQUIRE_MESSAGE(grc_test::CreateAndProcessBlock(block, err), "could not mine: " << err);

        BOOST_CHECK_GT(block.GetBlockTime(), latest_known);
        BOOST_CHECK_EQUAL(block.GetBlockTime(), GetAdjustedTime());
    }
}

//! An attempt that finds no stake is retried in the next slot, and the block the
//! retry mines is stamped at the clock, not a slot ahead of it. A retry is how
//! the wall-clock fixture stamped a block after the clock: an attempt refused
//! as a duplicate stake, then a block stamped 16 s ahead. Raising the stake
//! minimum age so that the premine reaches it only at the second attempt's slot
//! makes the first attempt find no stake.
BOOST_AUTO_TEST_CASE(a_retried_mine_is_stamped_at_the_clock)
{
    const StakeMinAgeReset reset_stake_min_age;

    const int64_t second_attempt = GRC::MaskStakeTime(GetMockTime().count()) + 2 * (GRC::STAKE_TIMESTAMP_MASK + 1);

    {
        LOCK(cs_main);

        // Genesis confirms the premine. An output a mined block confirmed is
        // younger, so it cannot stake at either attempt.
        nStakeMinAge = static_cast<unsigned int>(second_attempt - pindexGenesisBlock->GetBlockTime());
    }

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(grc_test::CreateAndProcessBlock(block, err), "could not mine: " << err);

    BOOST_CHECK_EQUAL(block.GetBlockTime(), second_attempt);
    BOOST_CHECK_EQUAL(block.GetBlockTime(), GetAdjustedTime());
}

//! With the clock behind the chain tip, a block could be stamped before its
//! parent, or after the clock, so the fixture refuses to mine. Mock time 0,
//! which turns the mock clock off, counts as behind.
BOOST_AUTO_TEST_CASE(a_mine_is_refused_while_the_clock_is_behind_the_tip)
{
    const std::chrono::seconds clock = GetMockTime();

    int height_before = 0;
    int64_t tip_time = 0;

    {
        LOCK(cs_main);
        height_before = nBestHeight;
        tip_time = pindexBest->GetBlockTime();
    }

    for (const int64_t behind : {tip_time - 2 * (GRC::STAKE_TIMESTAMP_MASK + 1), int64_t{0}}) {
        SetMockTime(behind);

        CBlock block;
        std::string err;
        const bool mined = grc_test::CreateAndProcessBlock(block, err);

        SetMockTime(clock);

        BOOST_CHECK_MESSAGE(!mined, "mined with the clock at " << behind);
        BOOST_CHECK_MESSAGE(err.find("behind the chain tip") != std::string::npos, "unexpected error: " << err);
    }

    LOCK(cs_main);
    BOOST_CHECK_EQUAL(nBestHeight, height_before);
}

BOOST_AUTO_TEST_SUITE_END()
