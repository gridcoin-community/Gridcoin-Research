// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file validation_tests.cpp
//! \brief The sync state the scraper and superblock code uses from block
//! version 15.
//!
//! The state reads the age of the block before the best block. It is out of
//! sync at one hour or more and in sync under fifteen minutes; between the two
//! it stays on the side it was last on, and it starts out of sync. Below block
//! version 15 the scraper and superblock code uses OutOfSyncByAge().
//!

#include "chain.h"
#include "node/chainman.h"
#include "test/state_guard.h"
#include "validation.h"
#include <util/time.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <cstdint>

namespace {
//! The state at the start of the process, read before any test runs. The
//! global is constant-initialized, so this initializer sees its first value.
const bool SCRAPER_SYNC_LATCHED_AT_START{g_scraper_sync_latched.load()};

const int64_t NOW = 1800000000;

//! Block version 15 begins at this height in the cases below.
const int V15_HEIGHT = 100;

//! Fixes the clock and puts the previous values of the globals back.
struct ScraperSyncSetup : grc_test::StateGuard
{
    const int64_t m_time_best_received{g_nTimeBestReceived.load()};
    const grc_test::V15HeightGuard m_v15{V15_HEIGHT};

    ScraperSyncSetup() { SetMockTime(NOW); }
    ~ScraperSyncSetup() { g_nTimeBestReceived.store(m_time_best_received); }
};

//! Gives the block before the best block this age in seconds.
void SetAge(const int64_t age)
{
    g_previous_block_time.store(GetAdjustedTime() - age);
}
} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(validation_tests, ScraperSyncSetup)

BOOST_AUTO_TEST_CASE(the_scraper_sync_state_starts_out_of_sync)
{
    BOOST_CHECK(SCRAPER_SYNC_LATCHED_AT_START);
}

BOOST_AUTO_TEST_CASE(the_scraper_sync_state_changes_under_fifteen_minutes_and_at_one_hour)
{
    g_scraper_sync_latched = true;

    // From out of sync, an age of fifteen minutes and more leaves it there.
    SetAge(3599);
    BOOST_CHECK(EvaluateScraperSync());
    SetAge(900);
    BOOST_CHECK(EvaluateScraperSync());

    // An age under fifteen minutes puts it in sync.
    SetAge(899);
    BOOST_CHECK(!EvaluateScraperSync());

    // From in sync, an age under one hour leaves it there.
    SetAge(900);
    BOOST_CHECK(!EvaluateScraperSync());
    SetAge(3599);
    BOOST_CHECK(!EvaluateScraperSync());

    // An age of one hour puts it out of sync.
    SetAge(3600);
    BOOST_CHECK(EvaluateScraperSync());

    // It stays there until the age is under fifteen minutes again.
    SetAge(3599);
    BOOST_CHECK(EvaluateScraperSync());
    SetAge(900);
    BOOST_CHECK(EvaluateScraperSync());
    SetAge(899);
    BOOST_CHECK(!EvaluateScraperSync());
}

BOOST_AUTO_TEST_CASE(the_scraper_sync_state_at_one_hour_and_under_fifteen_minutes_does_not_depend_on_its_past)
{
    for (const bool latched : {false, true}) {
        g_scraper_sync_latched = latched;
        SetAge(3600);
        BOOST_CHECK(EvaluateScraperSync());

        g_scraper_sync_latched = latched;
        SetAge(0);
        BOOST_CHECK(!EvaluateScraperSync());
    }
}

BOOST_AUTO_TEST_CASE(the_scraper_sync_state_is_out_of_sync_only_where_the_age_test_is)
{
    for (const bool latched : {false, true}) {
        for (const int64_t age : {0, 899, 900, 901, 3599, 3600, 7200}) {
            g_scraper_sync_latched = latched;
            SetAge(age);

            const bool out_of_sync = EvaluateScraperSync();

            BOOST_CHECK(!out_of_sync || OutOfSyncByAge());

            // From out of sync the state is the age test, so it leaves where the age test does.
            if (latched) BOOST_CHECK_EQUAL(out_of_sync, OutOfSyncByAge());
        }
    }
}

BOOST_AUTO_TEST_CASE(below_block_version_15_the_scraper_sync_test_is_the_age_test)
{
    g_scraper_sync_in_force = false;

    for (const bool latched : {false, true}) {
        for (const int64_t age : {0, 899, 900, 901, 3599, 3600, 7200}) {
            SetAge(age);

            g_scraper_sync_latched = latched;
            BOOST_CHECK_EQUAL(ScraperOutOfSync(V15_HEIGHT - 1), OutOfSyncByAge());

            g_scraper_sync_latched = latched;
            BOOST_CHECK_EQUAL(ScraperOutOfSync(), OutOfSyncByAge());
        }
    }
}

BOOST_AUTO_TEST_CASE(from_block_version_15_the_scraper_sync_test_is_the_state)
{
    // Twenty minutes: out of sync by the age test, and where it was by the state.
    SetAge(1200);
    BOOST_REQUIRE(OutOfSyncByAge());

    for (const bool latched : {false, true}) {
        g_scraper_sync_latched = latched;
        BOOST_CHECK_EQUAL(ScraperOutOfSync(V15_HEIGHT), latched);
        BOOST_CHECK(ScraperOutOfSync(V15_HEIGHT - 1));

        g_scraper_sync_in_force = true;
        BOOST_CHECK_EQUAL(ScraperOutOfSync(), latched);

        g_scraper_sync_in_force = false;
        BOOST_CHECK(ScraperOutOfSync());
    }
}

BOOST_AUTO_TEST_CASE(the_scraper_sync_state_is_kept_below_block_version_15)
{
    g_scraper_sync_in_force = false;

    for (const bool with_height : {false, true}) {
        SetAge(0);
        BOOST_REQUIRE(!EvaluateScraperSync());

        // A read below block version 15 at one hour.
        SetAge(3600);
        BOOST_CHECK(with_height ? ScraperOutOfSync(V15_HEIGHT - 1) : ScraperOutOfSync());

        SetAge(1200);
        BOOST_CHECK(ScraperOutOfSync(V15_HEIGHT));

        // A read below block version 15 under fifteen minutes.
        SetAge(899);
        BOOST_CHECK(with_height ? !ScraperOutOfSync(V15_HEIGHT - 1) : !ScraperOutOfSync());

        SetAge(1200);
        BOOST_CHECK(!ScraperOutOfSync(V15_HEIGHT));
    }
}

BOOST_AUTO_TEST_CASE(a_change_of_the_best_block_gives_the_scraper_sync_state_the_age_before_it_and_after_it)
{
    CBlockIndex before_best;
    CBlockIndex best;

    best.pprev = &before_best;
    best.nHeight = V15_HEIGHT - 2;

    // In sync, and then the age passes one hour with no read of the state.
    SetAge(0);
    BOOST_REQUIRE(!EvaluateScraperSync());
    SetAge(3600);

    // The new best block puts the age at twenty minutes.
    before_best.nTime = NOW - 1200;
    UpdateSyncTime(&best);

    BOOST_CHECK_EQUAL(g_previous_block_time.load(), NOW - 1200);
    BOOST_CHECK(g_scraper_sync_latched);
    BOOST_CHECK(EvaluateScraperSync());

    // The next one puts it under fifteen minutes, and the state is in sync
    // with no read of it.
    before_best.nTime = NOW - 899;
    UpdateSyncTime(&best);

    BOOST_CHECK(!g_scraper_sync_latched);

    // Twenty minutes again: it stays in sync.
    before_best.nTime = NOW - 1200;
    UpdateSyncTime(&best);

    BOOST_CHECK(!g_scraper_sync_latched);
    BOOST_CHECK(!EvaluateScraperSync());
}

BOOST_AUTO_TEST_CASE(the_scraper_sync_state_is_in_force_when_the_next_block_is_at_block_version_15)
{
    CBlockIndex before_best;
    CBlockIndex best;

    best.pprev = &before_best;
    before_best.nTime = NOW;

    best.nHeight = V15_HEIGHT - 2;
    UpdateSyncTime(&best);
    BOOST_CHECK(!g_scraper_sync_in_force);

    best.nHeight = V15_HEIGHT - 1;
    UpdateSyncTime(&best);
    BOOST_CHECK(g_scraper_sync_in_force);

    best.nHeight = V15_HEIGHT;
    UpdateSyncTime(&best);
    BOOST_CHECK(g_scraper_sync_in_force);

    UpdateSyncTime(nullptr);
    BOOST_CHECK(!g_scraper_sync_in_force);
}

BOOST_AUTO_TEST_SUITE_END()
