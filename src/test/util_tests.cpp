#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "main.h"
#include "wallet.h"
#include "util.h"

#include <cstdint>

using namespace std;

BOOST_AUTO_TEST_SUITE(util_tests)

BOOST_AUTO_TEST_CASE(util_criticalsection)
{
    CCriticalSection cs;

    do {
        LOCK(cs);
        break;

        BOOST_ERROR("break was swallowed!");
    } while(0);

    do {
        TRY_LOCK(cs, lockTest);
        if (lockTest)
            break;

        BOOST_ERROR("break was swallowed!");
    } while(0);
}

BOOST_AUTO_TEST_CASE(util_MedianFilter)
{    
    CMedianFilter<int> filter(5, 15);

    BOOST_CHECK_EQUAL(filter.median(), 15);

    filter.input(20); // [15 20]
    BOOST_CHECK_EQUAL(filter.median(), 17);

    filter.input(30); // [15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 20);

    filter.input(3); // [3 15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 17);

    filter.input(7); // [3 7 15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 15);

    filter.input(18); // [3 7 18 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 18);

    filter.input(0); // [0 3 7 18 30]
    BOOST_CHECK_EQUAL(filter.median(), 7);
}

static const unsigned char ParseHex_expected[65] = {
    0x04, 0x67, 0x8a, 0xfd, 0xb0, 0xfe, 0x55, 0x48, 0x27, 0x19, 0x67, 0xf1, 0xa6, 0x71, 0x30, 0xb7, 
    0x10, 0x5c, 0xd6, 0xa8, 0x28, 0xe0, 0x39, 0x09, 0xa6, 0x79, 0x62, 0xe0, 0xea, 0x1f, 0x61, 0xde, 
    0xb6, 0x49, 0xf6, 0xbc, 0x3f, 0x4c, 0xef, 0x38, 0xc4, 0xf3, 0x55, 0x04, 0xe5, 0x1e, 0xc1, 0x12, 
    0xde, 0x5c, 0x38, 0x4d, 0xf7, 0xba, 0x0b, 0x8d, 0x57, 0x8a, 0x4c, 0x70, 0x2b, 0x6b, 0xf1, 0x1d, 
    0x5f
};
BOOST_AUTO_TEST_CASE(util_ParseHex)
{
    std::vector<unsigned char> result;
    std::vector<unsigned char> expected(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected));
    // Basic test vector
    result = ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());

    // Spaces between bytes must be supported
    result = ParseHex("12 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x12 && result[1] == 0x34 && result[2] == 0x56 && result[3] == 0x78);

    // Stop parsing at invalid value
    result = ParseHex("1234 invalid 1234");
    BOOST_CHECK(result.size() == 2 && result[0] == 0x12 && result[1] == 0x34);
}

BOOST_AUTO_TEST_CASE(util_HexStr)
{
    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected)),
        "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + 5, true),
        "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected, true),
        "");

    std::vector<unsigned char> ParseHex_vec(ParseHex_expected, ParseHex_expected + 5);

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec, true),
        "04 67 8a fd b0");
}


BOOST_AUTO_TEST_CASE(util_DateTimeStrFormat)
{
/*These are platform-dependant and thus removed to avoid useless test failures
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 0), "01/01/70 00:00:00");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 0x7FFFFFFF), "01/19/38 03:14:07");
    // Formats used within Bitcoin
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 1317425777), "09/30/11 23:36:17");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M", 1317425777), "09/30/11 23:36");
*/
}

BOOST_AUTO_TEST_CASE(util_ParseParameters)
{
    const char *argv_test[] = {"-ignored", "-a", "-b", "-ccc=argument", "-ccc=multiple", "f", "-d=e"};

    ParseParameters(0, (char**)argv_test);
    BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    ParseParameters(1, (char**)argv_test);
    BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    ParseParameters(5, (char**)argv_test);
    // expectation: -ignored is ignored (program name argument), 
    // -a, -b and -ccc end up in map, -d ignored because it is after
    // a non-option argument (non-GNU option parsing)
    BOOST_CHECK(mapArgs.size() == 3 && mapMultiArgs.size() == 3);
    BOOST_CHECK(mapArgs.count("-a") && mapArgs.count("-b") && mapArgs.count("-ccc") 
                && !mapArgs.count("f") && !mapArgs.count("-d"));
    BOOST_CHECK(mapMultiArgs.count("-a") && mapMultiArgs.count("-b") && mapMultiArgs.count("-ccc") 
                && !mapMultiArgs.count("f") && !mapMultiArgs.count("-d"));

    BOOST_CHECK(mapArgs["-a"] == "" && mapArgs["-ccc"] == "multiple");
    BOOST_CHECK(mapMultiArgs["-ccc"].size() == 2);
}

BOOST_AUTO_TEST_CASE(util_GetArg)
{
    mapArgs.clear();
    mapArgs["strtest1"] = "string...";
    // strtest2 undefined on purpose
    mapArgs["inttest1"] = "12345";
    mapArgs["inttest2"] = "81985529216486895";
    // inttest3 undefined on purpose
    mapArgs["booltest1"] = "";
    // booltest2 undefined on purpose
    mapArgs["booltest3"] = "0";
    mapArgs["booltest4"] = "1";

    BOOST_CHECK_EQUAL(GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(GetArg("inttest1", -1), 12345);
    BOOST_CHECK_EQUAL(GetArg("inttest2", -1), 81985529216486895LL);
    BOOST_CHECK_EQUAL(GetArg("inttest3", -1), -1);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest1"), true);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest2"), false);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest3"), false);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest4"), true);
}

BOOST_AUTO_TEST_CASE(util_WildcardMatch)
{
    BOOST_CHECK(WildcardMatch("127.0.0.1", "*"));
    BOOST_CHECK(WildcardMatch("127.0.0.1", "127.*"));
    BOOST_CHECK(WildcardMatch("abcdef", "a?cde?"));
    BOOST_CHECK(!WildcardMatch("abcdef", "a?cde??"));
    BOOST_CHECK(WildcardMatch("abcdef", "a*f"));
    BOOST_CHECK(!WildcardMatch("abcdef", "a*x"));
    BOOST_CHECK(WildcardMatch("", "*"));
}

BOOST_AUTO_TEST_CASE(util_FormatMoney)
{
    BOOST_CHECK_EQUAL(FormatMoney(0, false), "0.00");
    BOOST_CHECK_EQUAL(FormatMoney((COIN/10000)*123456789, false), "12345.6789");
    BOOST_CHECK_EQUAL(FormatMoney(COIN, true), "+1.00");
    BOOST_CHECK_EQUAL(FormatMoney(-COIN, false), "-1.00");
    BOOST_CHECK_EQUAL(FormatMoney(-COIN, true), "-1.00");

    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000000, false), "100000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000000, false), "10000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000000, false), "1000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000, false), "100000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000, false), "10000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000, false), "1000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100, false), "100.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10, false), "10.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN, false), "1.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10, false), "0.10");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100, false), "0.01");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000, false), "0.001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000, false), "0.0001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000, false), "0.00001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000000, false), "0.000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000000, false), "0.0000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000000, false), "0.00000001");
}

BOOST_AUTO_TEST_CASE(util_ParseMoney)
{
    int64_t ret = 0;
    BOOST_CHECK(ParseMoney("0.0", ret));
    BOOST_CHECK_EQUAL(ret, 0);

    BOOST_CHECK(ParseMoney("12345.6789", ret));
    BOOST_CHECK_EQUAL(ret, (COIN/10000)*123456789);

    BOOST_CHECK(ParseMoney("100000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000000);
    BOOST_CHECK(ParseMoney("10000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000000);
    BOOST_CHECK(ParseMoney("1000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000000);
    BOOST_CHECK(ParseMoney("100000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000);
    BOOST_CHECK(ParseMoney("10000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000);
    BOOST_CHECK(ParseMoney("1000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000);
    BOOST_CHECK(ParseMoney("100.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100);
    BOOST_CHECK(ParseMoney("10.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10);
    BOOST_CHECK(ParseMoney("1.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN);
    BOOST_CHECK(ParseMoney("0.1", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10);
    BOOST_CHECK(ParseMoney("0.01", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100);
    BOOST_CHECK(ParseMoney("0.001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000);
    BOOST_CHECK(ParseMoney("0.0001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000);
    BOOST_CHECK(ParseMoney("0.00001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000);
    BOOST_CHECK(ParseMoney("0.000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000000);
    BOOST_CHECK(ParseMoney("0.0000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000000);
    BOOST_CHECK(ParseMoney("0.00000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000000);

    // Attempted 63 bit overflow should fail
    BOOST_CHECK(!ParseMoney("92233720368.54775808", ret));
}

BOOST_AUTO_TEST_CASE(util_IsHex)
{
    BOOST_CHECK(IsHex("00"));
    BOOST_CHECK(IsHex("00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHex("ff"));
    BOOST_CHECK(IsHex("FF"));

    BOOST_CHECK(!IsHex(""));
    BOOST_CHECK(!IsHex("0"));
    BOOST_CHECK(!IsHex("a"));
    BOOST_CHECK(!IsHex("eleven"));
    BOOST_CHECK(!IsHex("00xx00"));
    BOOST_CHECK(!IsHex("0x0000"));
}

BOOST_AUTO_TEST_CASE(util_seed_insecure_rand)
{
    // Expected results for the determinstic seed.
    const uint32_t exp_vals[11] = {  91632771U,1889679809U,3842137544U,3256031132U,
                                   1761911779U, 489223532U,2692793790U,2737472863U,
                                   2796262275U,1309899767U,840571781U};
    // Expected 0s in rand()%(idx+2) for the determinstic seed.
    const int exp_count[9] = {5013,3346,2415,1972,1644,1386,1176,1096,1009};
    int i;
    int count=0;

    seed_insecure_rand();

    //Does the non-determistic rand give us results that look too like the determinstic one?
    for (i=0;i<10;i++)
    {
        int match = 0;
        uint32_t rval = insecure_rand();
        for (int j=0;j<11;j++)match |= rval==exp_vals[j];
        count += match;
    }
    // sum(binomial(10,i)*(11/(2^32))^i*(1-(11/(2^32)))^(10-i),i,0,4) ~= 1-1/2^134.73
    // So _very_ unlikely to throw a false failure here.
    BOOST_CHECK(count<=4);

    for (int mod=2;mod<11;mod++)
    {
        int mask = 1;
        // Really rough binomal confidence approximation.
        int err = 30*10000./mod*sqrt((1./mod*(1-1./mod))/10000.);
        //mask is 2^ceil(log2(mod))-1
        while(mask<mod-1)mask=(mask<<1)+1;

        count = 0;
        //How often does it get a zero from the uniform range [0,mod)?
        for (i=0;i<10000;i++)
        {
            uint32_t rval;
            do{
                rval=insecure_rand()&mask;
            }while(rval>=(uint32_t)mod);
            count += rval==0;
        }
        BOOST_CHECK(count<=10000/mod+err);
        BOOST_CHECK(count>=10000/mod-err);
    }

    seed_insecure_rand(true);

    for (i=0;i<11;i++)
    {
        BOOST_CHECK_EQUAL(insecure_rand(),exp_vals[i]);
    }

    for (int mod=2;mod<11;mod++)
    {
        count = 0;
        for (i=0;i<10000;i++) count += insecure_rand()%mod==0;
        BOOST_CHECK_EQUAL(count,exp_count[mod-2]);
    }
}

BOOST_AUTO_TEST_CASE(util_VerifyIsLockTimeWithin14days)
{
    int64_t now = 1494060475;
    int64_t twoWeeksInSeconds = 1209600;
    int64_t time = now - twoWeeksInSeconds;

    BOOST_CHECK(IsLockTimeWithin14days(time, now) == true);
    BOOST_CHECK(IsLockTimeWithin14days(time - 1, now) == false);
}

BOOST_AUTO_TEST_CASE(util_IsLockTimeWithinMinutes)
{    
    int64_t now = 1494060475;
    int64_t minutes = 6;
    int64_t minutesInSeconds = minutes * 60;
    int64_t time = now - minutesInSeconds;

    BOOST_CHECK(IsLockTimeWithinMinutes(time, now, minutes) == true);
    BOOST_CHECK(IsLockTimeWithinMinutes(time - 1, now, minutes) == false);
}

BOOST_AUTO_TEST_CASE(util_VerifyRound)
{
    BOOST_CHECK_EQUAL(1.2346, Round(1.23456789, 4));
    BOOST_CHECK_EQUAL(1,      Round(1.23456789, 0));
    BOOST_CHECK_EQUAL(2,      Round(1.5, 0));
}

BOOST_AUTO_TEST_CASE(util_VerifyRoundToString)
{
    BOOST_CHECK_EQUAL("1.2346", RoundToString(1.23456789, 4));
}

BOOST_AUTO_TEST_CASE(util_RoundFromStringShouldRoundToDouble)
{
    BOOST_CHECK_EQUAL(3.14, RoundFromString("3.1415", 2));
}

BOOST_AUTO_TEST_CASE(util_VerifySplit)
{
    const std::string str("Hello;;My;;String;;");
    const auto res = split(str, ";;");
    BOOST_CHECK(res.size() == 4);
    BOOST_CHECK_EQUAL("Hello",  res[0]);
    BOOST_CHECK_EQUAL("My",     res[1]);
    BOOST_CHECK_EQUAL("String", res[2]);
    BOOST_CHECK_EQUAL("",       res[3]);
}

BOOST_AUTO_TEST_SUITE_END()
