#include "uint256.h"
#include "util.h"
#include "main.h"
#include "global_objects_noui.hpp"
#include "appcache.h"
#include "contract/superblock.h"

#include "data/superblock.txt.h"
#include "data/superblock_packed.bin.h"
#include "data/superblock_unpacked.txt.h"

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>
#include <map>
#include <string>

extern double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime, double& total_owed, double block_magnitude);
extern std::map<std::string, StructCPID> mvDPOR;
extern std::string GetQuorumHash(const std::string& data);
extern bool fTestNet;
double RoundFromString(std::string s, int place);
std::string LowerUnderscore(std::string data);

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

BOOST_AUTO_TEST_CASE(gridcoin_VerifyGetQuorumHash)
{
    const std::string contract = "<MAGNITUDES>0390450eff5f5cd6d7a7d95a6d898d8d,1480;1878ecb566ac8e62beb7d141e1922460,6.25;1963a6f109ea770c195a0e1afacd2eba,70;285ff8d5014ef73cc83580338a9c0345,820;46f64d69eb8c5ee9cd24178b589af83f,12.5;0,15;4f0fecd04be3a74c46aa9678f780d028,750;55cd02be28521073d367f7ca38615682,720;58e565221db80d168621187c36c26c3e,12.5;59900fe7ef44fe33aa2afdf98301ec1c,530;5a094d7d93f6d6370e78a2ac8c008407,1400;0,15;7d0d73fe026d66fd4ab8d5d8da32a611,84000;8cfe9864e18db32a334b7de997f5a4f2,35;8f2a530cf6f73647af4c680c3471ea65,90;96c18bb4a02d15c90224a7138a540cf7,4520;9b67756a05f76842de1e88226b79deb9,0;9ce6f19e20f69790601c9bf9c0b03928,3.75;9ff2b091f67327b7d8e5b75fb5337514,310;a7a537ff8ad4d8fff4b3dad444e681ef,0;a914eba952be5dfcf73d926b508fd5fa,6720;d5924e4750f0f1c1c97b9635914afb9e,0;db250f4451dc39632e52e157f034316d,5;e7f90818e3e87c0bbefe83ad3cfe27e1,13500;</MAGNITUDES><QUOTES>btc,0;grc,0;</QUOTES><AVERAGES>amicable numbers,536000,5900000;asteroids@home,158666.67,793333.33;citizen science grid,575333.33,2881428.57;collatz conjecture,4027142.86,36286380;cosmology@home,47000,282666.67;einstein@home,435333.33,5661428.57;gpugrid,1804285.71,9035714.29;leiden classical,2080,10500;lhc@home classic,26166.67,210000;milkyway@home,2094285.71,8395714.29;moowrap,996666.67,7981428.57;nfs@home,96000,385333.33;numberfields@home,89333.33,626666.67;primegrid,248000,1735714.29;seti@home,52333.33,367333.33;srbase,89666.67,896666.67;sztaki desktop grid,8320,41666.67;theskynet pogs,45500,409333.33;tn-grid,39500,514666.67;universe@home,47833.33,335333.33;vgtu project@home,20666.67,124000;world community grid,29166.67,263333.33;yafu,93000,838666.67;yoyo@home,7040,56333.33;NeuralNetwork,2000000,20000000;</AVERAGES>";
    BOOST_CHECK_EQUAL(GetQuorumHash(contract), "0f099cab261bb562ff553f3b9c7bf942");
}

BOOST_AUTO_TEST_CASE(gridcoin_QuorumHashShouldBeCorrectAfterPackingAndUnpackingBinarySuperblock)
{
    const std::string contract(superblock_txt, superblock_txt + superblock_txt_len);
    const std::string packed = PackBinarySuperblock(contract);
    const std::string unpacked = UnpackBinarySuperblock(packed);
    //BOOST_CHECK_EQUAL(unpacked, contract);
    BOOST_CHECK_EQUAL(GetQuorumHash(contract), GetQuorumHash(unpacked));
}

BOOST_AUTO_TEST_CASE(gridcoin_ValidatePackBinarySuperblock)
{
    const std::string contract(superblock_txt, superblock_txt + superblock_txt_len);
    const std::string expected(superblock_packed_bin, superblock_packed_bin + superblock_packed_bin_len);
    BOOST_CHECK_EQUAL(PackBinarySuperblock(contract), expected);
}

BOOST_AUTO_TEST_CASE(gridcoin_ValidateUnpackBinarySuperblock)
{
    const std::string packed(superblock_packed_bin, superblock_packed_bin + superblock_packed_bin_len);
    const std::string expected(superblock_unpacked_txt, superblock_unpacked_txt + superblock_unpacked_txt_len);
    BOOST_CHECK_EQUAL(UnpackBinarySuperblock(packed), expected);
}

BOOST_AUTO_TEST_CASE(gridcoin_LowerUnderscoreShouldConvertToLowerCaseAndReplaceUnderscoresWithSpaces)
{
    BOOST_CHECK_EQUAL("hello test string", LowerUnderscore("Hello_TEST_string"));
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

