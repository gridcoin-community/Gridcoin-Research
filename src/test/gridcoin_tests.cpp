// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "uint256.h"
#include "util.h"
#include "main.h"
#include "gridcoin/appcache.h"
#include "gridcoin/staking/reward.h"

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>
#include <map>
#include <string>

extern bool fTestNet;

namespace
{
   // Arbitrary random characters generated with Python UUID.
   const std::string TEST_CPID("17c65330c0924259b2f93c31d25b03ac");

   struct GridcoinTestsConfig
   {
      GridcoinTestsConfig()
      {
      }

      ~GridcoinTestsConfig()
      {
      }
   };
}

BOOST_GLOBAL_FIXTURE(GridcoinTestsConfig);

BOOST_AUTO_TEST_SUITE(gridcoin_tests)

BOOST_AUTO_TEST_CASE(gridcoin_V8ShouldBeEnabledOnBlock1010000InProduction)
{
    bool was_testnet = fTestNet;
    fTestNet = false;
    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK(IsV8Enabled(1009999) == false);
    BOOST_CHECK(IsV8Enabled(1010000) == false);
    BOOST_CHECK(IsV8Enabled(1010001) == true);
    fTestNet = was_testnet;
}

BOOST_AUTO_TEST_CASE(gridcoin_V8ShouldBeEnabledOnBlock312000InTestnet)
{
    bool was_testnet = fTestNet;
    fTestNet = true;
    SelectParams(CBaseChainParams::TESTNET);
    // With testnet block 312000 was created as the first V8 block,
    // hence the difference in testing setup compared to the production
    // tests.
    BOOST_CHECK(IsV8Enabled(311999) == false);
    BOOST_CHECK(IsV8Enabled(312000) == true);
    fTestNet = was_testnet;
}

BOOST_AUTO_TEST_SUITE_END()

//
// CBR tests
//
struct GridcoinCBRTestConfig
{
    GridcoinCBRTestConfig()
    {
        // Clear out previous CBR settings.
        DeleteCache(Section::PROTOCOL, "blockreward1");
    }
};

BOOST_AUTO_TEST_SUITE(gridcoin_cbr_tests)
BOOST_GLOBAL_FIXTURE(GridcoinCBRTestConfig);

BOOST_AUTO_TEST_CASE(gridcoin_DefaultCBRShouldBe10)
{
    CBlockIndex index;
    index.nTime = 1538066417;
    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), DEFAULT_CBR);
}

BOOST_AUTO_TEST_CASE(gridcoin_ConfigurableCBRShouldOverrideDefault)
{
    const int64_t time = 123456;
    const int64_t cbr = 14.9 * COIN;

    CBlockIndex index;
    index.nVersion = 10;
    index.nTime = time;

    WriteCache(Section::PROTOCOL, "blockreward1", ToString(cbr), time);
    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), cbr);
}

BOOST_AUTO_TEST_CASE(gridcoin_NegativeCBRShouldClampTo0)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;

    WriteCache(Section::PROTOCOL, "blockreward1", ToString(-1 * COIN), time);
    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), 0);
}

BOOST_AUTO_TEST_CASE(gridcoin_ConfigurableCBRShouldClampTo2xDefault)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;

    WriteCache(Section::PROTOCOL, "blockreward1", ToString(DEFAULT_CBR * 2.1), time);
    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), DEFAULT_CBR * 2);
}

BOOST_AUTO_TEST_CASE(gridcoin_ObsoleteConfigurableCBRShouldResortToDefault)
{
    CBlockIndex index;
    index.nTime = 1538066417;
    const int64_t max_message_age = 60 * 24 * 30 * 6 * 60;

    // Make the block reward message 1 second older than the max age
    // relative to the block.
    WriteCache(Section::PROTOCOL, "blockreward1", ToString(3 * COIN), index.nTime - max_message_age - 1);

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), DEFAULT_CBR);
}

BOOST_AUTO_TEST_SUITE_END()
