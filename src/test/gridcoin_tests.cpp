#include "uint256.h"
#include "util.h"
#include "main.h"
#include "global_objects_noui.hpp"
#include "appcache.h"

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>
#include <map>
#include <string>

extern double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime, double& total_owed, double block_magnitude);
extern std::map<std::string, StructCPID> mvDPOR;
extern bool fTestNet;
double RoundFromString(std::string s, int place);

namespace
{
   // Arbitrary random characters generated with Python UUID.
   const std::string TEST_CPID("17c65330c0924259b2f93c31d25b03ac");

   struct GridcoinTestsConfig
   {
      GridcoinTestsConfig()
      {
         // Create a fake CPID
         StructCPID& cpid = GetInitializedStructCPID2(TEST_CPID, mvMagnitudes);
         cpid.cpid = TEST_CPID;
         cpid.payments = 123;
         //cpid.EarliestPaymentTime = 1490260000;
         mvMagnitudes[TEST_CPID] = cpid;

         // Create a fake DPOR
         StructCPID& dpor = GetInitializedStructCPID2(TEST_CPID, mvDPOR);
         dpor.Magnitude = 125;
         mvDPOR[TEST_CPID] = dpor;
      }

      ~GridcoinTestsConfig()
      {
         // Clean up.
         mvMagnitudes.erase(TEST_CPID);
         mvDPOR.erase(TEST_CPID);
      }
   };
}

BOOST_GLOBAL_FIXTURE(GridcoinTestsConfig);

BOOST_AUTO_TEST_SUITE(gridcoin_tests)

BOOST_AUTO_TEST_CASE(gridcoin_GetOutstandingAmountOwedShouldReturnCorrectSum)
{
   // Update the CPID earliest pay time to 24 hours ago.
   StructCPID& cpid = GetInitializedStructCPID2(TEST_CPID, mvMagnitudes);
   cpid.EarliestPaymentTime = GetAdjustedTime() - 24 * 3600;
   mvMagnitudes[TEST_CPID] = cpid;

   double total_owed;
   double magnitude = 190;
   double outstanding = GetOutstandingAmountOwed(cpid, TEST_CPID, GetAdjustedTime(), total_owed, magnitude);

   // Value taken from GetOutstandingAmountOwed prior to refactoring given
   // the setup in this test and the GridcoinTestsConfig constructor.
   BOOST_CHECK_CLOSE(total_owed, 140.625, 0.00001);
   BOOST_CHECK_CLOSE(outstanding, total_owed - cpid.payments, 0.000001);
}

BOOST_AUTO_TEST_CASE(gridcoin_V8ShouldBeEnabledOnBlock1010000InProduction)
{
    bool was_testnet = fTestNet;
    fTestNet = false;
    BOOST_CHECK(IsV8Enabled(1009999) == false);
    BOOST_CHECK(IsV8Enabled(1010000) == false);
    BOOST_CHECK(IsV8Enabled(1010001) == true);
    fTestNet = was_testnet;
}

BOOST_AUTO_TEST_CASE(gridcoin_V8ShouldBeEnabledOnBlock312000InTestnet)
{
    bool was_testnet = fTestNet;
    fTestNet = true;

    // With testnet block 312000 was created as the first V8 block,
    // hence the difference in testing setup compared to the production
    // tests.
    BOOST_CHECK(IsV8Enabled(311999) == false);
    BOOST_CHECK(IsV8Enabled(312000) == true);
    fTestNet = was_testnet;
}

BOOST_AUTO_TEST_CASE(gridcoin_InvestorsShouldNotBeResearchers)
{
    BOOST_CHECK(IsResearcher("INVESTOR") == false);
    BOOST_CHECK(IsResearcher("investor") == false);
}

BOOST_AUTO_TEST_CASE(gridcoin_EmptyCpidShouldNotBeResearcher)
{
    BOOST_CHECK(IsResearcher("") == false);
}

BOOST_AUTO_TEST_CASE(gridcoin_IncompleteCpidShouldNotBeResearcher)
{
    BOOST_CHECK(IsResearcher("9c508a9e20f0415755db0ca27375c5") == false);
}

BOOST_AUTO_TEST_CASE(gridcoin_ValidCpidShouldBeResearcher)
{
    BOOST_CHECK(IsResearcher("9c508a9e20f0415755db0ca27375c5fe") == true);
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
    BOOST_CHECK_EQUAL(GetConstantBlockReward(&index), DEFAULT_CBR);
}

BOOST_AUTO_TEST_CASE(gridcoin_ConfigurableCBRShouldOverrideDefault)
{
    const int64_t time = 123456;
    const int64_t cbr = 14.9 * COIN;

    CBlockIndex index;
    index.nVersion = 10;
    index.nTime = time;

    WriteCache(Section::PROTOCOL, "blockreward1", ToString(cbr), time);
    BOOST_CHECK_EQUAL(GetConstantBlockReward(&index), cbr);
}

BOOST_AUTO_TEST_CASE(gridcoin_NegativeCBRShouldClampTo0)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;

    WriteCache(Section::PROTOCOL, "blockreward1", ToString(-1 * COIN), time);
    BOOST_CHECK_EQUAL(GetConstantBlockReward(&index), 0);
}

BOOST_AUTO_TEST_CASE(gridcoin_ConfigurableCBRShouldClampTo2xDefault)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;

    WriteCache(Section::PROTOCOL, "blockreward1", ToString(DEFAULT_CBR * 2.1), time);
    BOOST_CHECK_EQUAL(GetConstantBlockReward(&index), DEFAULT_CBR * 2);
}

BOOST_AUTO_TEST_CASE(gridcoin_ObsoleteConfigurableCBRShouldResortToDefault)
{
    CBlockIndex index;
    index.nTime = 1538066417;
    const int64_t max_message_age = 60 * 24 * 30 * 6 * 60;

    // Make the block reward message 1 second older than the max age
    // relative to the block.
    WriteCache(Section::PROTOCOL, "blockreward1", ToString(3 * COIN), index.nTime - max_message_age - 1);

    BOOST_CHECK_EQUAL(GetConstantBlockReward(&index), DEFAULT_CBR);
}


BOOST_AUTO_TEST_SUITE_END()

