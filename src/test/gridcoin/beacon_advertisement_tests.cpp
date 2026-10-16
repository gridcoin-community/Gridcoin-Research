// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! \file beacon_advertisement_tests.cpp
//! \brief The sync check every beacon send makes.

#include "chain.h"
#include "gridcoin/researcher.h"
#include "test/state_guard.h"
#include "util.h"
#include "util/settings.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <string>

namespace {

//! Points pwalletMain at another wallet for the life of the object.
struct SwapWallet
{
    CWallet* m_saved;
    explicit SwapWallet(CWallet* replacement) : m_saved(pwalletMain) { pwalletMain = replacement; }
    ~SwapWallet() { pwalletMain = m_saved; }
};

//! Gives the global researcher a forced CPID for the life of the object, then
//! returns it to non-cruncher mode, as the researcher suite's own cases leave
//! it, and puts the settings back as they were.
struct ForcedResearcher
{
    util::Settings m_saved;

    explicit ForcedResearcher(const std::string& cpid_hex)
    {
        gArgs.LockSettings([&](util::Settings& settings) { m_saved = settings; });

        gArgs.ForceSetArg("forcecpid", cpid_hex);
        gArgs.ForceSetArg("noncruncher", "0");

        LOCK(cs_main);
        GRC::Researcher::Reload(GRC::MiningProjectMap());
    }

    ~ForcedResearcher()
    {
        gArgs.ForceSetArg("noncruncher", "1");

        {
            LOCK(cs_main);
            GRC::Researcher::Reload(GRC::MiningProjectMap());
        }

        gArgs.LockSettings([&](util::Settings& settings) { settings = m_saved; });
    }
};

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(beacon_advertisement_tests)

BOOST_AUTO_TEST_SUITE(sync_check)

BOOST_AUTO_TEST_CASE(a_beacon_is_not_advertised_while_out_of_sync)
{
    grc_test::MempoolStateGuard guard;

    // Empty and unencrypted: in sync, the send gets as far as the balance check.
    CWallet wallet;
    SwapWallet swap(&wallet);

    ForcedResearcher researcher("c0ffee00c0ffee00c0ffee00c0ffee00");
    BOOST_REQUIRE(GRC::Researcher::Get()->Id().Which() == GRC::MiningId::Kind::CPID);

    g_previous_block_time.store(0);
    BOOST_REQUIRE(OutOfSyncByAge());

    BOOST_CHECK(GRC::Researcher::Get()->AdvertiseBeacon().Error() == GRC::BeaconError::OUT_OF_SYNC);

    // Not kept as the researcher's beacon error: the snapshot would show it as
    // the beacon status after the node had caught up.
    BOOST_CHECK(GRC::Researcher::Get()->BeaconError() == GRC::BeaconError::NONE);

    g_previous_block_time.store(GetAdjustedTime());
    BOOST_REQUIRE(!OutOfSyncByAge());

    BOOST_CHECK(GRC::Researcher::Get()->AdvertiseBeacon().Error() == GRC::BeaconError::INSUFFICIENT_FUNDS);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END() // beacon_advertisement_tests
