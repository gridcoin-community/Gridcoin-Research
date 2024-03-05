// Copyright (c) 2011-2020 The Bitcoin Core developers
// Copyright (c) 2024 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <vector>
#include <boost/test/unit_test.hpp>
#include "univalue/include/univalue.h"

#include <main.h>
#include <wallet/wallet.h>
#include <util.h>
//#include <util/system.h>
#include <util/settings.h>

#include <cstdint>

namespace {
// This version, which is recommended by some resources on the web, is actually slower, and has several issues. See
// the unit tests below.
int msb(const int64_t& n)
{
    // Can't take the log of 0.
    if (n == 0) {
        return 0;
    }

    // Log2 is O(1) both time and space-wise.
    return (static_cast<int>(floor(log2(std::abs(n)))) + 1);
}

int msb2(const int64_t& n_in)
{
    int64_t n = std::abs(n_in);

    int index = 0;

    if (n == 0) {
        return 0;
    }

    for (int i = 0; i <= 63; ++i) {
        if (n % 2 == 1) {
            index = i;
        }

        n /= 2;
    }

    return index + 1;
}

// This is the one currently used in the Fraction class
int msb3(const int64_t& n_in)
{
    int64_t n = std::abs(n_in);

    int index = 0;

    for (; index <= 63; ++index) {
        if (n >> index == 0) {
            break;
        }
    }

    return index;
}
} //anonymous namespace

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

    // Leading space must be supported (used in BerkeleyEnvironment::Salvage)
    result = ParseHex(" 89 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x89 && result[1] == 0x34 && result[2] == 0x56 && result[3] == 0x78);

    // Stop parsing at invalid value
    result = ParseHex("1234 invalid 1234");
    BOOST_CHECK(result.size() == 2 && result[0] == 0x12 && result[1] == 0x34);
}

BOOST_AUTO_TEST_CASE(util_HexStr)
{
    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected),
        "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");

    BOOST_CHECK_EQUAL(
        HexStr(Span{ParseHex_expected}.last(0)),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(Span{ParseHex_expected}.first(0)),
        "");

    {
        const std::vector<char> in_s{ParseHex_expected, ParseHex_expected + 5};
        const Span<const uint8_t> in_u{MakeUCharSpan(in_s)};
        const Span<const std::byte> in_b{MakeByteSpan(in_s)};
        const std::string out_exp{"04678afdb0"};

        BOOST_CHECK_EQUAL(HexStr(in_u), out_exp);
        BOOST_CHECK_EQUAL(HexStr(in_s), out_exp);
        BOOST_CHECK_EQUAL(HexStr(in_b), out_exp);
    }
}

BOOST_AUTO_TEST_CASE(util_DateTimeStrFormat)
{
/*These are platform-dependent and thus removed to avoid useless test failures
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 0), "01/01/70 00:00:00");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 0x7FFFFFFF), "01/19/38 03:14:07");
    // Formats used within Bitcoin
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M:%S", 1317425777), "09/30/11 23:36:17");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%x %H:%M", 1317425777), "09/30/11 23:36");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%a, %d %b %Y %H:%M:%S +0000", 1317425777), "Fri, 30 Sep 2011 23:36:17 +0000");
*/
}

struct TestArgsManager : public ArgsManager
{
    TestArgsManager() { m_network_only_args.clear(); }
    void ReadConfigString(const std::string str_config)
    {
        std::istringstream streamConfig(str_config);
        {
            LOCK(cs_args);
            m_settings.ro_config.clear();
            m_config_sections.clear();
        }
        std::string error;
        BOOST_REQUIRE(ReadConfigStream(streamConfig, "", error));
    }
    void SetNetworkOnlyArg(const std::string arg)
    {
        LOCK(cs_args);
        m_network_only_args.insert(arg);
    }
    void SetupArgs(const std::vector<std::pair<std::string, unsigned int>>& args)
    {
        for (const auto& arg : args) {
            AddArg(arg.first, "", arg.second, OptionsCategory::OPTIONS);
        }
    }
    using ArgsManager::GetSetting;
    using ArgsManager::GetSettingsList;
    using ArgsManager::ReadConfigStream;
    using ArgsManager::cs_args;
    using ArgsManager::m_network;
    using ArgsManager::m_settings;
};

/* This is reserved when we port over the TestChain 100 block setup class from Bitcoin.
//! Test GetSetting and GetArg type coercion, negation, and default value handling.
class CheckValueTest : public TestChain100Setup
{
public:
    struct Expect {
        util::SettingsValue setting;
        bool default_string = false;
        bool default_int = false;
        bool default_bool = false;
        const char* string_value = nullptr;
        std::optional<int64_t> int_value;
        std::optional<bool> bool_value;
        std::optional<std::vector<std::string>> list_value;
        const char* error = nullptr;

        explicit Expect(util::SettingsValue s) : setting(std::move(s)) {}
        Expect& DefaultString() { default_string = true; return *this; }
        Expect& DefaultInt() { default_int = true; return *this; }
        Expect& DefaultBool() { default_bool = true; return *this; }
        Expect& String(const char* s) { string_value = s; return *this; }
        Expect& Int(int64_t i) { int_value = i; return *this; }
        Expect& Bool(bool b) { bool_value = b; return *this; }
        Expect& List(std::vector<std::string> m) { list_value = std::move(m); return *this; }
        Expect& Error(const char* e) { error = e; return *this; }
    };

    void CheckValue(unsigned int flags, const char* arg, const Expect& expect)
    {
        TestArgsManager test;
        test.SetupArgs({{"-value", flags}});
        const char* argv[] = {"ignored", arg};
        std::string error;
        bool success = test.ParseParameters(arg ? 2 : 1, (char**)argv, error);

        BOOST_CHECK_EQUAL(test.GetSetting("-value").write(), expect.setting.write());
        auto settings_list = test.GetSettingsList("-value");
        if (expect.setting.isNull() || expect.setting.isFalse()) {
            BOOST_CHECK_EQUAL(settings_list.size(), 0U);
        } else {
            BOOST_CHECK_EQUAL(settings_list.size(), 1U);
            BOOST_CHECK_EQUAL(settings_list[0].write(), expect.setting.write());
        }

        if (expect.error) {
            BOOST_CHECK(!success);
            BOOST_CHECK_NE(error.find(expect.error), std::string::npos);
        } else {
            BOOST_CHECK(success);
            BOOST_CHECK_EQUAL(error, "");
        }

        if (expect.default_string) {
            BOOST_CHECK_EQUAL(test.GetArg("-value", "zzzzz"), "zzzzz");
        } else if (expect.string_value) {
            BOOST_CHECK_EQUAL(test.GetArg("-value", "zzzzz"), expect.string_value);
        } else {
            BOOST_CHECK(!success);
        }

        if (expect.default_int) {
            BOOST_CHECK_EQUAL(test.GetIntArg("-value", 99999), 99999);
        } else if (expect.int_value) {
            BOOST_CHECK_EQUAL(test.GetIntArg("-value", 99999), *expect.int_value);
        } else {
            BOOST_CHECK(!success);
        }

        if (expect.default_bool) {
            BOOST_CHECK_EQUAL(test.GetBoolArg("-value", false), false);
            BOOST_CHECK_EQUAL(test.GetBoolArg("-value", true), true);
        } else if (expect.bool_value) {
            BOOST_CHECK_EQUAL(test.GetBoolArg("-value", false), *expect.bool_value);
            BOOST_CHECK_EQUAL(test.GetBoolArg("-value", true), *expect.bool_value);
        } else {
            BOOST_CHECK(!success);
        }

        if (expect.list_value) {
            auto l = test.GetArgs("-value");
            BOOST_CHECK_EQUAL_COLLECTIONS(l.begin(), l.end(), expect.list_value->begin(), expect.list_value->end());
        } else {
            BOOST_CHECK(!success);
        }
    }
};

BOOST_FIXTURE_TEST_CASE(util_CheckValue, CheckValueTest)
{
    using M = ArgsManager;

    CheckValue(M::ALLOW_ANY, nullptr, Expect{{}}.DefaultString().DefaultInt().DefaultBool().List({}));
    CheckValue(M::ALLOW_ANY, "-novalue", Expect{false}.String("0").Int(0).Bool(false).List({}));
    CheckValue(M::ALLOW_ANY, "-novalue=", Expect{false}.String("0").Int(0).Bool(false).List({}));
    CheckValue(M::ALLOW_ANY, "-novalue=0", Expect{true}.String("1").Int(1).Bool(true).List({"1"}));
    CheckValue(M::ALLOW_ANY, "-novalue=1", Expect{false}.String("0").Int(0).Bool(false).List({}));
    CheckValue(M::ALLOW_ANY, "-novalue=2", Expect{false}.String("0").Int(0).Bool(false).List({}));
    CheckValue(M::ALLOW_ANY, "-novalue=abc", Expect{true}.String("1").Int(1).Bool(true).List({"1"}));
    CheckValue(M::ALLOW_ANY, "-value", Expect{""}.String("").Int(0).Bool(true).List({""}));
    CheckValue(M::ALLOW_ANY, "-value=", Expect{""}.String("").Int(0).Bool(true).List({""}));
    CheckValue(M::ALLOW_ANY, "-value=0", Expect{"0"}.String("0").Int(0).Bool(false).List({"0"}));
    CheckValue(M::ALLOW_ANY, "-value=1", Expect{"1"}.String("1").Int(1).Bool(true).List({"1"}));
    CheckValue(M::ALLOW_ANY, "-value=2", Expect{"2"}.String("2").Int(2).Bool(true).List({"2"}));
    CheckValue(M::ALLOW_ANY, "-value=abc", Expect{"abc"}.String("abc").Int(0).Bool(false).List({"abc"}));
}
*/

struct NoIncludeConfTest {
    std::string Parse(const char* arg)
    {
        TestArgsManager test;
        test.SetupArgs({{"-includeconf", ArgsManager::ALLOW_ANY}});
        std::array argv{"ignored", arg};
        std::string error;
        (void)test.ParseParameters(argv.size(), argv.data(), error);
        return error;
    }
};

BOOST_FIXTURE_TEST_CASE(util_NoIncludeConf, NoIncludeConfTest)
{
    BOOST_CHECK_EQUAL(Parse("-noincludeconf"), "");
    BOOST_CHECK_EQUAL(Parse("-includeconf"), "-includeconf cannot be used from commandline; -includeconf=\"\"");
    BOOST_CHECK_EQUAL(Parse("-includeconf=file"), "-includeconf cannot be used from commandline; -includeconf=\"file\"");
}

BOOST_AUTO_TEST_CASE(util_ParseParameters)
{
    const char *argv_test[] = {"-ignored", "-a", "-b", "-ccc=argument", "-ccc=multiple", "f", "-d=e"};

    gArgs.AddArg("-a", "-a", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-b", "-b", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-ccc", "--ccc", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);

    std::string error;

    // TODO: Finish fixing this for the new Bitcoin port.

    //BOOST_CHECK(gArgs.ParseParameters(0, (char**)argv_test, error));
    //BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    //BOOST_CHECK(gArgs.ParseParameters(1, (char**)argv_test, error));
    //BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    BOOST_CHECK(gArgs.ParseParameters(7, (char**)argv_test, error));
    // expectation: -ignored is ignored (program name argument),
    // -a, -b and -ccc end up in map, -d ignored because it is after
    // a non-option argument (non-GNU option parsing)
    // BOOST_CHECK(mapArgs.size() == 3 && mapMultiArgs.size() == 3);
    BOOST_CHECK(gArgs.GetArgs("-a").size() == 1 && gArgs.GetArgs("-b").size() == 1 && gArgs.GetArgs("-ccc").size() == 2
                && gArgs.GetArgs("f").empty() && gArgs.GetArgs("-d").empty());
    //BOOST_CHECK(mapMultiArgs.count("-a") && mapMultiArgs.count("-b") && mapMultiArgs.count("-ccc")
    //            && !mapMultiArgs.count("f") && !mapMultiArgs.count("-d"));

    //BOOST_CHECK(mapArgs["-a"] == "" && mapArgs["-ccc"] == "multiple");
    //BOOST_CHECK(mapMultiArgs["-ccc"].size() == 2);
}

BOOST_AUTO_TEST_CASE(util_GetArg)
{
    gArgs.ClearArgs();

    gArgs.ForceSetArg("-strtest1", "string...");
    //mapArgs["strtest1"] = "string...";

    // strtest2 undefined on purpose

    gArgs.ForceSetArg("-inttest1", "12345");
    //mapArgs["inttest1"] = "12345";
    gArgs.ForceSetArg("-inttest2", "81985529216486895");
    //mapArgs["inttest2"] = "81985529216486895";

    // inttest3 undefined on purpose

    gArgs.ForceSetArg("-booltest1", "");
    //mapArgs["booltest1"] = "";

    // booltest2 undefined on purpose

    gArgs.ForceSetArg("-booltest3", "0");
    //mapArgs["booltest3"] = "0";
    gArgs.ForceSetArg("-booltest4", "1");
    //mapArgs["booltest4"] = "1";

    BOOST_CHECK_EQUAL(gArgs.GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(gArgs.GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(gArgs.GetArg("inttest1", -1), 12345);
    BOOST_CHECK_EQUAL(gArgs.GetArg("inttest2", -1), 81985529216486895LL);
    BOOST_CHECK_EQUAL(gArgs.GetArg("inttest3", -1), -1);
    BOOST_CHECK_EQUAL(gArgs.GetBoolArg("booltest1"), true);
    BOOST_CHECK_EQUAL(gArgs.GetBoolArg("booltest2"), false);
    BOOST_CHECK_EQUAL(gArgs.GetBoolArg("booltest3"), false);
    BOOST_CHECK_EQUAL(gArgs.GetBoolArg("booltest4"), true);
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
    BOOST_CHECK(ParseMoney("1", ret));
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

    // Parsing negative amounts must fail
    BOOST_CHECK(!ParseMoney("-1", ret));

    // Parsing strings with embedded NUL characters should fail
    BOOST_CHECK(!ParseMoney(std::string("\0-1", 3), ret));
    BOOST_CHECK(!ParseMoney(std::string("\01", 2), ret));
    BOOST_CHECK(!ParseMoney(std::string("1\0", 2), ret));
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

BOOST_AUTO_TEST_CASE(util_IsHexNumber)
{
    BOOST_CHECK(IsHexNumber("0x0"));
    BOOST_CHECK(IsHexNumber("0"));
    BOOST_CHECK(IsHexNumber("0x10"));
    BOOST_CHECK(IsHexNumber("10"));
    BOOST_CHECK(IsHexNumber("0xff"));
    BOOST_CHECK(IsHexNumber("ff"));
    BOOST_CHECK(IsHexNumber("0xFfa"));
    BOOST_CHECK(IsHexNumber("Ffa"));
    BOOST_CHECK(IsHexNumber("0x00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHexNumber("00112233445566778899aabbccddeeffAABBCCDDEEFF"));

    BOOST_CHECK(!IsHexNumber(""));   // empty string not allowed
    BOOST_CHECK(!IsHexNumber("0x")); // empty string after prefix not allowed
    BOOST_CHECK(!IsHexNumber("0x0 ")); // no spaces at end,
    BOOST_CHECK(!IsHexNumber(" 0x0")); // or beginning,
    BOOST_CHECK(!IsHexNumber("0x 0")); // or middle,
    BOOST_CHECK(!IsHexNumber(" "));    // etc.
    BOOST_CHECK(!IsHexNumber("0x0ga")); // invalid character
    BOOST_CHECK(!IsHexNumber("x0"));    // broken prefix
    BOOST_CHECK(!IsHexNumber("0x0x00")); // two prefixes not allowed

}

BOOST_AUTO_TEST_CASE(util_TimingResistantEqual)
{
    BOOST_CHECK(TimingResistantEqual(std::string(""), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string(""), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("a"), std::string("aa")));
    BOOST_CHECK(!TimingResistantEqual(std::string("aa"), std::string("a")));
    BOOST_CHECK(TimingResistantEqual(std::string("abc"), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("aba")));
}

/* Test strprintf formatting directives.
 * Put a string before and after to ensure sanity of element sizes on stack. */
#define B "check_prefix"
#define E "check_postfix"
BOOST_AUTO_TEST_CASE(strprintf_numbers)
{
    int64_t s64t = -9223372036854775807LL; /* signed 64 bit test value */
    uint64_t u64t = 18446744073709551615ULL; /* unsigned 64 bit test value */
    BOOST_CHECK(strprintf("%s %d %s", B, s64t, E) == B" -9223372036854775807 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, u64t, E) == B" 18446744073709551615 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, u64t, E) == B" ffffffffffffffff " E);

    size_t st = 12345678; /* unsigned size_t test value */
    ssize_t sst = -12345678; /* signed size_t test value */
    BOOST_CHECK(strprintf("%s %d %s", B, sst, E) == B" -12345678 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, st, E) == B" 12345678 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, st, E) == B" bc614e " E);

    ptrdiff_t pt = 87654321; /* positive ptrdiff_t test value */
    ptrdiff_t spt = -87654321; /* negative ptrdiff_t test value */
    BOOST_CHECK(strprintf("%s %d %s", B, spt, E) == B" -87654321 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, pt, E) == B" 87654321 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, pt, E) == B" 5397fb1 " E);
}
#undef B
#undef E

BOOST_AUTO_TEST_CASE(test_IsDigit)
{
    BOOST_CHECK_EQUAL(IsDigit('0'), true);
    BOOST_CHECK_EQUAL(IsDigit('1'), true);
    BOOST_CHECK_EQUAL(IsDigit('8'), true);
    BOOST_CHECK_EQUAL(IsDigit('9'), true);

    BOOST_CHECK_EQUAL(IsDigit('0' - 1), false);
    BOOST_CHECK_EQUAL(IsDigit('9' + 1), false);
    BOOST_CHECK_EQUAL(IsDigit(0), false);
    BOOST_CHECK_EQUAL(IsDigit(1), false);
    BOOST_CHECK_EQUAL(IsDigit(8), false);
    BOOST_CHECK_EQUAL(IsDigit(9), false);
}

BOOST_AUTO_TEST_CASE(test_LocaleIndependentAtoi)
{
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("1234"), 1'234);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("0"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("01234"), 1'234);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("-1234"), -1'234);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>(" 1"), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("1 "), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("1a"), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("1.1"), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("1.9"), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("+01.9"), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("-1"), -1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>(" -1"), -1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("-1 "), -1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>(" -1 "), -1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("+1"), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>(" +1"), 1);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>(" +1 "), 1);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("+-1"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("-+1"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("++1"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("--1"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>(""), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("aap"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("0x1"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("-32482348723847471234"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("32482348723847471234"), 0);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int64_t>("-9223372036854775809"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int64_t>("-9223372036854775808"), -9'223'372'036'854'775'807LL - 1LL);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int64_t>("9223372036854775807"), 9'223'372'036'854'775'807);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int64_t>("9223372036854775808"), 0);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint64_t>("-1"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint64_t>("0"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint64_t>("18446744073709551615"), 18'446'744'073'709'551'615ULL);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint64_t>("18446744073709551616"), 0U);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("-2147483649"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("-2147483648"), -2'147'483'648LL);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("2147483647"), 2'147'483'647);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int32_t>("2147483648"), 0);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint32_t>("-1"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint32_t>("0"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint32_t>("4294967295"), 4'294'967'295U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint32_t>("4294967296"), 0U);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int16_t>("-32769"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int16_t>("-32768"), -32'768);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int16_t>("32767"), 32'767);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int16_t>("32768"), 0);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint16_t>("-1"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint16_t>("0"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint16_t>("65535"), 65'535U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint16_t>("65536"), 0U);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int8_t>("-129"), 0);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int8_t>("-128"), -128);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int8_t>("127"), 127);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<int8_t>("128"), 0);

    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint8_t>("-1"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint8_t>("0"), 0U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint8_t>("255"), 255U);
    BOOST_CHECK_EQUAL(LocaleIndependentAtoi<uint8_t>("256"), 0U);
}

BOOST_AUTO_TEST_CASE(test_ParseInt32)
{
    int32_t n;
    // Valid values
    BOOST_CHECK(ParseInt32("1234", nullptr));
    BOOST_CHECK(ParseInt32("0", &n) && n == 0);
    BOOST_CHECK(ParseInt32("1234", &n) && n == 1234);
    BOOST_CHECK(ParseInt32("01234", &n) && n == 1234); // no octal
    BOOST_CHECK(ParseInt32("2147483647", &n) && n == 2147483647);
    BOOST_CHECK(ParseInt32("-2147483648", &n) && n == (-2147483647 - 1)); // (-2147483647 - 1) equals INT_MIN
    BOOST_CHECK(ParseInt32("-1234", &n) && n == -1234);
    BOOST_CHECK(ParseInt32("00000000000000001234", &n) && n == 1234);
    BOOST_CHECK(ParseInt32("-00000000000000001234", &n) && n == -1234);
    BOOST_CHECK(ParseInt32("00000000000000000000", &n) && n == 0);
    BOOST_CHECK(ParseInt32("-00000000000000000000", &n) && n == 0);
    // Invalid values
    BOOST_CHECK(!ParseInt32("", &n));
    BOOST_CHECK(!ParseInt32(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseInt32("1 ", &n));
    BOOST_CHECK(!ParseInt32("++1", &n));
    BOOST_CHECK(!ParseInt32("+-1", &n));
    BOOST_CHECK(!ParseInt32("-+1", &n));
    BOOST_CHECK(!ParseInt32("--1", &n));
    BOOST_CHECK(!ParseInt32("1a", &n));
    BOOST_CHECK(!ParseInt32("aap", &n));
    BOOST_CHECK(!ParseInt32("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseInt32(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseInt32("-2147483649", nullptr));
    BOOST_CHECK(!ParseInt32("2147483648", nullptr));
    BOOST_CHECK(!ParseInt32("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseInt32("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ToIntegral)
{
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("1234").value(), 1'234);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("0").value(), 0);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("01234").value(), 1'234);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("00000000000000001234").value(), 1'234);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("-00000000000000001234").value(), -1'234);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("00000000000000000000").value(), 0);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("-00000000000000000000").value(), 0);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("-1234").value(), -1'234);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("-1").value(), -1);

    BOOST_CHECK(!ToIntegral<int32_t>(" 1"));
    BOOST_CHECK(!ToIntegral<int32_t>("1 "));
    BOOST_CHECK(!ToIntegral<int32_t>("1a"));
    BOOST_CHECK(!ToIntegral<int32_t>("1.1"));
    BOOST_CHECK(!ToIntegral<int32_t>("1.9"));
    BOOST_CHECK(!ToIntegral<int32_t>("+01.9"));
    BOOST_CHECK(!ToIntegral<int32_t>(" -1"));
    BOOST_CHECK(!ToIntegral<int32_t>("-1 "));
    BOOST_CHECK(!ToIntegral<int32_t>(" -1 "));
    BOOST_CHECK(!ToIntegral<int32_t>("+1"));
    BOOST_CHECK(!ToIntegral<int32_t>(" +1"));
    BOOST_CHECK(!ToIntegral<int32_t>(" +1 "));
    BOOST_CHECK(!ToIntegral<int32_t>("+-1"));
    BOOST_CHECK(!ToIntegral<int32_t>("-+1"));
    BOOST_CHECK(!ToIntegral<int32_t>("++1"));
    BOOST_CHECK(!ToIntegral<int32_t>("--1"));
    BOOST_CHECK(!ToIntegral<int32_t>(""));
    BOOST_CHECK(!ToIntegral<int32_t>("aap"));
    BOOST_CHECK(!ToIntegral<int32_t>("0x1"));
    BOOST_CHECK(!ToIntegral<int32_t>("-32482348723847471234"));
    BOOST_CHECK(!ToIntegral<int32_t>("32482348723847471234"));

    BOOST_CHECK(!ToIntegral<int64_t>("-9223372036854775809"));
    BOOST_CHECK_EQUAL(ToIntegral<int64_t>("-9223372036854775808").value(), -9'223'372'036'854'775'807LL - 1LL);
    BOOST_CHECK_EQUAL(ToIntegral<int64_t>("9223372036854775807").value(), 9'223'372'036'854'775'807);
    BOOST_CHECK(!ToIntegral<int64_t>("9223372036854775808"));

    BOOST_CHECK(!ToIntegral<uint64_t>("-1"));
    BOOST_CHECK_EQUAL(ToIntegral<uint64_t>("0").value(), 0U);
    BOOST_CHECK_EQUAL(ToIntegral<uint64_t>("18446744073709551615").value(), 18'446'744'073'709'551'615ULL);
    BOOST_CHECK(!ToIntegral<uint64_t>("18446744073709551616"));

    BOOST_CHECK(!ToIntegral<int32_t>("-2147483649"));
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("-2147483648").value(), -2'147'483'648LL);
    BOOST_CHECK_EQUAL(ToIntegral<int32_t>("2147483647").value(), 2'147'483'647);
    BOOST_CHECK(!ToIntegral<int32_t>("2147483648"));

    BOOST_CHECK(!ToIntegral<uint32_t>("-1"));
    BOOST_CHECK_EQUAL(ToIntegral<uint32_t>("0").value(), 0U);
    BOOST_CHECK_EQUAL(ToIntegral<uint32_t>("4294967295").value(), 4'294'967'295U);
    BOOST_CHECK(!ToIntegral<uint32_t>("4294967296"));

    BOOST_CHECK(!ToIntegral<int16_t>("-32769"));
    BOOST_CHECK_EQUAL(ToIntegral<int16_t>("-32768").value(), -32'768);
    BOOST_CHECK_EQUAL(ToIntegral<int16_t>("32767").value(), 32'767);
    BOOST_CHECK(!ToIntegral<int16_t>("32768"));

    BOOST_CHECK(!ToIntegral<uint16_t>("-1"));
    BOOST_CHECK_EQUAL(ToIntegral<uint16_t>("0").value(), 0U);
    BOOST_CHECK_EQUAL(ToIntegral<uint16_t>("65535").value(), 65'535U);
    BOOST_CHECK(!ToIntegral<uint16_t>("65536"));

    BOOST_CHECK(!ToIntegral<int8_t>("-129"));
    BOOST_CHECK_EQUAL(ToIntegral<int8_t>("-128").value(), -128);
    BOOST_CHECK_EQUAL(ToIntegral<int8_t>("127").value(), 127);
    BOOST_CHECK(!ToIntegral<int8_t>("128"));

    BOOST_CHECK(!ToIntegral<uint8_t>("-1"));
    BOOST_CHECK_EQUAL(ToIntegral<uint8_t>("0").value(), 0U);
    BOOST_CHECK_EQUAL(ToIntegral<uint8_t>("255").value(), 255U);
    BOOST_CHECK(!ToIntegral<uint8_t>("256"));
}

BOOST_AUTO_TEST_CASE(test_ParseInt64)
{
    int64_t n;
    // Valid values
    BOOST_CHECK(ParseInt64("1234", nullptr));
    BOOST_CHECK(ParseInt64("0", &n) && n == 0LL);
    BOOST_CHECK(ParseInt64("1234", &n) && n == 1234LL);
    BOOST_CHECK(ParseInt64("01234", &n) && n == 1234LL); // no octal
    BOOST_CHECK(ParseInt64("2147483647", &n) && n == 2147483647LL);
    BOOST_CHECK(ParseInt64("-2147483648", &n) && n == -2147483648LL);
    BOOST_CHECK(ParseInt64("9223372036854775807", &n) && n == (int64_t)9223372036854775807);
    BOOST_CHECK(ParseInt64("-9223372036854775808", &n) && n == (int64_t)-9223372036854775807-1);
    BOOST_CHECK(ParseInt64("-1234", &n) && n == -1234LL);
    // Invalid values
    BOOST_CHECK(!ParseInt64("", &n));
    BOOST_CHECK(!ParseInt64(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseInt64("1 ", &n));
    BOOST_CHECK(!ParseInt64("1a", &n));
    BOOST_CHECK(!ParseInt64("aap", &n));
    BOOST_CHECK(!ParseInt64("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseInt64(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseInt64("-9223372036854775809", nullptr));
    BOOST_CHECK(!ParseInt64("9223372036854775808", nullptr));
    BOOST_CHECK(!ParseInt64("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseInt64("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt32)
{
    uint32_t n;
    // Valid values
    BOOST_CHECK(ParseUInt32("1234", nullptr));
    BOOST_CHECK(ParseUInt32("0", &n) && n == 0);
    BOOST_CHECK(ParseUInt32("1234", &n) && n == 1234);
    BOOST_CHECK(ParseUInt32("01234", &n) && n == 1234); // no octal
    BOOST_CHECK(ParseUInt32("2147483647", &n) && n == 2147483647);
    BOOST_CHECK(ParseUInt32("2147483648", &n) && n == (uint32_t)2147483648);
    BOOST_CHECK(ParseUInt32("4294967295", &n) && n == (uint32_t)4294967295);
    BOOST_CHECK(ParseUInt32("+1234", &n) && n == 1234);
    BOOST_CHECK(ParseUInt32("00000000000000001234", &n) && n == 1234);
    BOOST_CHECK(ParseUInt32("00000000000000000000", &n) && n == 0);
    // Invalid values
    BOOST_CHECK(!ParseUInt32("-00000000000000000000", &n));
    BOOST_CHECK(!ParseUInt32("", &n));
    BOOST_CHECK(!ParseUInt32(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseUInt32(" -1", &n));
    BOOST_CHECK(!ParseUInt32("++1", &n));
    BOOST_CHECK(!ParseUInt32("+-1", &n));
    BOOST_CHECK(!ParseUInt32("-+1", &n));
    BOOST_CHECK(!ParseUInt32("--1", &n));
    BOOST_CHECK(!ParseUInt32("-1", &n));
    BOOST_CHECK(!ParseUInt32("1 ", &n));
    BOOST_CHECK(!ParseUInt32("1a", &n));
    BOOST_CHECK(!ParseUInt32("aap", &n));
    BOOST_CHECK(!ParseUInt32("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseUInt32(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseUInt32("-2147483648", &n));
    BOOST_CHECK(!ParseUInt32("4294967296", &n));
    BOOST_CHECK(!ParseUInt32("-1234", &n));
    BOOST_CHECK(!ParseUInt32("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseUInt32("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt64)
{
    uint64_t n;
    // Valid values
    BOOST_CHECK(ParseUInt64("1234", nullptr));
    BOOST_CHECK(ParseUInt64("0", &n) && n == 0LL);
    BOOST_CHECK(ParseUInt64("1234", &n) && n == 1234LL);
    BOOST_CHECK(ParseUInt64("01234", &n) && n == 1234LL); // no octal
    BOOST_CHECK(ParseUInt64("2147483647", &n) && n == 2147483647LL);
    BOOST_CHECK(ParseUInt64("9223372036854775807", &n) && n == 9223372036854775807ULL);
    BOOST_CHECK(ParseUInt64("9223372036854775808", &n) && n == 9223372036854775808ULL);
    BOOST_CHECK(ParseUInt64("18446744073709551615", &n) && n == 18446744073709551615ULL);
    // Invalid values
    BOOST_CHECK(!ParseUInt64("", &n));
    BOOST_CHECK(!ParseUInt64(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseUInt64(" -1", &n));
    BOOST_CHECK(!ParseUInt64("1 ", &n));
    BOOST_CHECK(!ParseUInt64("1a", &n));
    BOOST_CHECK(!ParseUInt64("aap", &n));
    BOOST_CHECK(!ParseUInt64("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseUInt64(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseUInt64("-9223372036854775809", nullptr));
    BOOST_CHECK(!ParseUInt64("18446744073709551616", nullptr));
    BOOST_CHECK(!ParseUInt64("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseUInt64("-2147483648", &n));
    BOOST_CHECK(!ParseUInt64("-9223372036854775808", &n));
    BOOST_CHECK(!ParseUInt64("-1234", &n));
}

BOOST_AUTO_TEST_CASE(test_ParseDouble)
{
    double n;
    // Valid values
    BOOST_CHECK(ParseDouble("1234", nullptr));
    BOOST_CHECK(ParseDouble("0", &n) && n == 0.0);
    BOOST_CHECK(ParseDouble("1234", &n) && n == 1234.0);
    BOOST_CHECK(ParseDouble("01234", &n) && n == 1234.0); // no octal
    BOOST_CHECK(ParseDouble("2147483647", &n) && n == 2147483647.0);
    BOOST_CHECK(ParseDouble("-2147483648", &n) && n == -2147483648.0);
    BOOST_CHECK(ParseDouble("-1234", &n) && n == -1234.0);
    BOOST_CHECK(ParseDouble("1e6", &n) && n == 1e6);
    BOOST_CHECK(ParseDouble("-1e6", &n) && n == -1e6);
    // Invalid values
    BOOST_CHECK(!ParseDouble("", &n));
    BOOST_CHECK(!ParseDouble(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseDouble("1 ", &n));
    BOOST_CHECK(!ParseDouble("1a", &n));
    BOOST_CHECK(!ParseDouble("aap", &n));
    BOOST_CHECK(!ParseDouble("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseDouble(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseDouble("-1e10000", nullptr));
    BOOST_CHECK(!ParseDouble("1e10000", nullptr));
}

BOOST_AUTO_TEST_CASE(test_FormatParagraph)
{
    BOOST_CHECK_EQUAL(FormatParagraph("", 79, 0), "");
    BOOST_CHECK_EQUAL(FormatParagraph("test", 79, 0), "test");
    BOOST_CHECK_EQUAL(FormatParagraph(" test", 79, 0), " test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 79, 0), "test test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 0), "test\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("testerde test", 4, 0), "testerde\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 4), "test\n    test");

    // Make sure we don't indent a fully-new line following a too-long line ending
    BOOST_CHECK_EQUAL(FormatParagraph("test test\nabc", 4, 4), "test\n    test\nabc");

    BOOST_CHECK_EQUAL(FormatParagraph("This_is_a_very_long_test_string_without_any_spaces_so_it_should_just_get_returned_as_is_despite_the_length until it gets here", 79), "This_is_a_very_long_test_string_without_any_spaces_so_it_should_just_get_returned_as_is_despite_the_length\nuntil it gets here");

    // Test wrap length is exact
    BOOST_CHECK_EQUAL(FormatParagraph("a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p", 79), "a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\nf g h i j k l m n o p");
    BOOST_CHECK_EQUAL(FormatParagraph("x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p", 79), "x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\nf g h i j k l m n o p");
    // Indent should be included in length of lines
    BOOST_CHECK_EQUAL(FormatParagraph("x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 a b c d e fg h i j k", 79, 4), "x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\n    f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 a b c d e fg\n    h i j k");

    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string. This is a second sentence in the very long test string.", 79), "This is a very long test string. This is a second sentence in the very long\ntest string.");
    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string.\nThis is a second sentence in the very long test string. This is a third sentence in the very long test string.", 79), "This is a very long test string.\nThis is a second sentence in the very long test string. This is a third\nsentence in the very long test string.");
    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string.\n\nThis is a second sentence in the very long test string. This is a third sentence in the very long test string.", 79), "This is a very long test string.\n\nThis is a second sentence in the very long test string. This is a third\nsentence in the very long test string.");
    BOOST_CHECK_EQUAL(FormatParagraph("Testing that normal newlines do not get indented.\nLike here.", 79), "Testing that normal newlines do not get indented.\nLike here.");
}

BOOST_AUTO_TEST_CASE(test_ToLower)
{
    BOOST_CHECK_EQUAL(ToLower('@'), '@');
    BOOST_CHECK_EQUAL(ToLower('A'), 'a');
    BOOST_CHECK_EQUAL(ToLower('Z'), 'z');
    BOOST_CHECK_EQUAL(ToLower('['), '[');
    BOOST_CHECK_EQUAL(ToLower(0), 0);
    BOOST_CHECK_EQUAL(ToLower('\xff'), '\xff');

    BOOST_CHECK_EQUAL(ToLower(""), "");
    BOOST_CHECK_EQUAL(ToLower("#HODL"), "#hodl");
    BOOST_CHECK_EQUAL(ToLower("\x00\xfe\xff"), "\x00\xfe\xff");
}

BOOST_AUTO_TEST_CASE(test_ToUpper)
{
    BOOST_CHECK_EQUAL(ToUpper('`'), '`');
    BOOST_CHECK_EQUAL(ToUpper('a'), 'A');
    BOOST_CHECK_EQUAL(ToUpper('z'), 'Z');
    BOOST_CHECK_EQUAL(ToUpper('{'), '{');
    BOOST_CHECK_EQUAL(ToUpper(0), 0);
    BOOST_CHECK_EQUAL(ToUpper('\xff'), '\xff');

    BOOST_CHECK_EQUAL(ToUpper(""), "");
    BOOST_CHECK_EQUAL(ToUpper("#hodl"), "#HODL");
    BOOST_CHECK_EQUAL(ToUpper("\x00\xfe\xff"), "\x00\xfe\xff");
}

BOOST_AUTO_TEST_CASE(test_Capitalize)
{
    BOOST_CHECK_EQUAL(Capitalize(""), "");
    BOOST_CHECK_EQUAL(Capitalize("bitcoin"), "Bitcoin");
    BOOST_CHECK_EQUAL(Capitalize("\x00\xfe\xff"), "\x00\xfe\xff");
}

BOOST_AUTO_TEST_CASE(util_VerifyRound)
{
    BOOST_CHECK_CLOSE(1.2346, Round(1.23456789, 4), 0.00000001);
    BOOST_CHECK_CLOSE(1,      Round(1.23456789, 0), 0.00000001);
    BOOST_CHECK_CLOSE(2,      Round(1.5, 0), 0.00000001);
}

BOOST_AUTO_TEST_CASE(util_VerifyRoundToString)
{
    BOOST_CHECK_EQUAL("1.2346", RoundToString(1.23456789, 4));
}

BOOST_AUTO_TEST_CASE(util_RoundFromStringShouldRoundToDouble)
{
    BOOST_CHECK_CLOSE(3.14, RoundFromString("3.1415", 2), 0.00000001);
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

BOOST_AUTO_TEST_CASE(util_VerifySplit2)
{
    const std::string str(";;");
    const auto res = split(str, ";;");
    BOOST_CHECK(res.size() == 2);
    BOOST_CHECK_EQUAL("",       res[0]);
    BOOST_CHECK_EQUAL("",       res[1]);
}

BOOST_AUTO_TEST_CASE(util_VerifySplit3)
{
    const std::string str("");
    const auto res = split(str, ";;");
    BOOST_CHECK(res.size() == 1);
    BOOST_CHECK_EQUAL("",       res[0]);
}

/* TODO: Replace this outdated with new Bitcoin equivalent.
BOOST_AUTO_TEST_CASE(util_mapArgsComparator)
{
    mapArgs.clear();

    mapArgs["-UPPERCASE"] = "uppertest";
    mapArgs["-MuLtIcAsE"] = "multitest";
    mapArgs["-lowercase"] = "lowertest";

    BOOST_CHECK_EQUAL(mapArgs["-UpPeRcAsE"], mapArgs["-uppercase"]);
    BOOST_CHECK_EQUAL(mapArgs["-uppercase"], mapArgs["-UPPERCASE"]);
    BOOST_CHECK_EQUAL(mapArgs["-multicase"], mapArgs["-multicase"]);
    BOOST_CHECK_EQUAL(mapArgs["-MULTICASE"], mapArgs["-MuLtIcAsE"]);
    BOOST_CHECK_EQUAL(mapArgs["-LOWERCASE"], mapArgs["-LoWeRcAsE"]);
    BOOST_CHECK_EQUAL(mapArgs["-LoWeRcAsE"], mapArgs["-lowercase"]);

    mapArgs["-modify"] = "testa";

    BOOST_CHECK_EQUAL(mapArgs["-modify"], "testa");

    mapArgs["-MoDiFy"] = "testb";

    BOOST_CHECK_EQUAL(mapArgs["-modify"], "testb");
    BOOST_CHECK_NE(mapArgs["-modify"], "testa");

    mapArgs["-MODIFY"] = "testc";

    BOOST_CHECK_EQUAL(mapArgs["-modify"], "testc");
    BOOST_CHECK_NE(mapArgs["-modify"], "testb");

    BOOST_CHECK_EQUAL(mapArgs.count("-modify"), 1);
    BOOST_CHECK_EQUAL(mapArgs.count("-MODIFY"), 1);
    BOOST_CHECK_EQUAL(mapArgs.count("-MoDiFy"), 1);

    BOOST_CHECK_EQUAL(mapArgs.size(), 4);
}
*/

/* Check for mingw/wine issue #3494
 * Remove this test before time.ctime(0xffffffff) == 'Sun Feb  7 07:28:15 2106'
 */
BOOST_AUTO_TEST_CASE(gettime)
{
    BOOST_CHECK((GetTime() & ~0xFFFFFFFFLL) == 0);
}

BOOST_AUTO_TEST_CASE(util_TrimString)
{
    BOOST_CHECK_EQUAL(TrimString(" foo bar "), "foo bar");
    BOOST_CHECK_EQUAL(TrimString("\t \n  \n \f\n\r\t\v\tfoo \n \f\n\r\t\v\tbar\t  \n \f\n\r\t\v\t\n "), "foo \n \f\n\r\t\v\tbar");
    BOOST_CHECK_EQUAL(TrimString("\t \n foo \n\tbar\t \n "), "foo \n\tbar");
    BOOST_CHECK_EQUAL(TrimString("\t \n foo \n\tbar\t \n ", "fobar"), "\t \n foo \n\tbar\t \n ");
    BOOST_CHECK_EQUAL(TrimString("foo bar"), "foo bar");
    BOOST_CHECK_EQUAL(TrimString("foo bar", "fobar"), " ");
    BOOST_CHECK_EQUAL(TrimString(std::string("\0 foo \0 ", 8)), std::string("\0 foo \0", 7));
    BOOST_CHECK_EQUAL(TrimString(std::string(" foo ", 5)), std::string("foo", 3));
    BOOST_CHECK_EQUAL(TrimString(std::string("\t\t\0\0\n\n", 6)), std::string("\0\0", 2));
    BOOST_CHECK_EQUAL(TrimString(std::string("\x05\x04\x03\x02\x01\x00", 6)), std::string("\x05\x04\x03\x02\x01\x00", 6));
    BOOST_CHECK_EQUAL(TrimString(std::string("\x05\x04\x03\x02\x01\x00", 6), std::string("\x05\x04\x03\x02\x01", 5)), std::string("\0", 1));
    BOOST_CHECK_EQUAL(TrimString(std::string("\x05\x04\x03\x02\x01\x00", 6), std::string("\x05\x04\x03\x02\x01\x00", 6)), "");
}

BOOST_AUTO_TEST_CASE(Fraction_msb_algorithm_equivalence)
{
    for (unsigned int i = 0; i <= 63; ++i) {
        int64_t n = 0;

        if (i > 0) {
            n = (int64_t {1} << (i - 1));
        }

        BOOST_CHECK_EQUAL(msb(n), i);
    }

    int bias_for_msb_result_63 = 0;

    // msb ugly, ugly, ugly. Log2 looses resolution near the top of the range...
    for (int i = 0; i < 16; ++i) {
        bias_for_msb_result_63 = (int64_t {1} << i);

        int msb_result = msb(std::numeric_limits<int64_t>::max() - bias_for_msb_result_63);

        if (msb_result == 63) {
            LogPrintf("INFO: %s: bias_for_msb_result_63 = %i, msb_result = %i", __func__, bias_for_msb_result_63, msb_result);
            break;
        } else {
        }
    }

    // bias_for_msb_result_63 is currently 32768! It should be zero! This disqualifies the log2 based approach based on
    // a correctness check.
    BOOST_CHECK_EQUAL(msb(std::numeric_limits<int64_t>::max() - bias_for_msb_result_63), 63);

    BOOST_CHECK_EQUAL(msb2(std::numeric_limits<int64_t>::max()), 63);
    BOOST_CHECK_EQUAL(msb3(std::numeric_limits<int64_t>::max()), 63);

    std::vector<std::pair<int64_t, int>> msb_results, msb2_results, msb3_results;

    unsigned int iterations = 1000;

    FastRandomContext rand(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        int64_t n = rand.rand32();

        msb_results.push_back(std::make_pair(n, msb(n)));
    }

    FastRandomContext rand2(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        int64_t n = rand2.rand32();

        msb2_results.push_back(std::make_pair(n, msb2(n)));
    }

    FastRandomContext rand3(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        int64_t n = rand3.rand32();

        msb3_results.push_back(std::make_pair(n, msb3(n)));
    }

    bool success = true;

    for (unsigned int i = 0; i < iterations; ++i) {
        if (msb_results[i] != msb2_results[i] || msb_results[i] != msb3_results[i]) {
            success = false;
            error("%s: iteration %u: mismatch: %" PRId64 ", msb = %i, %" PRId64 " msb2 = %i, %" PRId64 " msb3 = %i",
                  __func__,
                  i,
                  msb_results[i].first,
                  msb_results[i].second,
                  msb2_results[i].first,
                  msb2_results[i].second,
                  msb3_results[i].first,
                  msb3_results[i].second
                  );
        }
    }

    BOOST_CHECK(success);
}

BOOST_AUTO_TEST_CASE(Fraction_msb_performance_test)
{
    // This is a test to bracket the three different algorithms above in anonymous namespace for doing msb calcs. The first is O(1),
    // the second and third are O(log n), but the O(1) straight from the C++ library is pretty heavyweight and highly dependent on CPU
    // architecture.

    FastRandomContext rand(uint256 {0});

    unsigned int iterations = 10000000;

    g_timer.InitTimer("msb_test", true);

    for (unsigned int i = 0; i < iterations; ++i) {
        msb((int64_t)rand.rand64());
    }

    int64_t msb_test_time = g_timer.GetTimes(strprintf("msb %u iterations", iterations), "msb_test").time_since_last_check;

    FastRandomContext rand2(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        msb2((int64_t)rand2.rand64());
    }

    int64_t msb2_test_time = g_timer.GetTimes(strprintf("msb2 %u iterations", iterations), "msb_test").time_since_last_check;

    FastRandomContext rand3(uint256 {0});

    for (unsigned int i = 0; i < iterations; ++i) {
        msb3((int64_t)rand3.rand64());
    }

    int64_t msb3_test_time = g_timer.GetTimes(strprintf("msb3 %u iterations", iterations), "msb_test").time_since_last_check;

    // The execution time of the above on a 13900K is

    // INFO: GetTimes: timer msb_test: msb 10000000 iterations: elapsed time: 86 ms, time since last check: 86 ms.
    // INFO: GetTimes: timer msb_test: msb2 10000000 iterations: elapsed time: 166 ms, time since last check: 80 ms.
    // INFO: GetTimes: timer msb_test: msb3 10000000 iterations: elapsed time: 246 ms, time since last check: 80 ms.

    // Which is almost identical. msb appears to be much slower on 32 bit architectures.

    // One can easily have T1 = k1 * O(1) and T2 = k2 * O(n) = k2 * n * O(1) where T1 > T2 for n < q if k1 > q * k2, so the O(1)
    // algorithm is by no means the best choice.

    // This test makes sure that the three algorithms are within 20x of the one with the minimum execution time. If not, it will
    // fail to prompt us to look at this again.

    double minimum_time = std::min<double>(std::min<double>(msb_test_time, msb2_test_time), msb3_test_time);

    BOOST_CHECK((double) msb_test_time / minimum_time < 20.0);
    BOOST_CHECK((double) msb2_test_time / minimum_time < 20.0);
    BOOST_CHECK((double) msb3_test_time / minimum_time < 20.0);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_trivial)
{
    Fraction fraction;

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), true);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_already_simplified)
{
    Fraction fraction(2, 3);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}


BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_not_simplified)
{
    Fraction fraction(4, 6);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 4);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 6);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), false);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification)
{
    Fraction fraction(4, 6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification_neg_pos)
{
    Fraction fraction(-4, 6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification_pos_neg)
{
    Fraction fraction(4, -6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_num_denom_with_simplification_neg_neg)
{
    Fraction fraction(-4, -6, true);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Copy_Constructor)
{
    Fraction fraction(4, 6);

    Fraction fraction2(fraction);

    BOOST_CHECK_EQUAL(fraction2.GetNumerator(), 4);
    BOOST_CHECK_EQUAL(fraction2.GetDenominator(), 6);
    BOOST_CHECK_EQUAL(fraction2.IsSimplified(), false);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Initialization_from_int64_t)
{
    Fraction fraction((int64_t) -2);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), false);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_Simplify)
{
    Fraction fraction(-4, -6);

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), false);

    fraction.Simplify();

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 2);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 3);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.IsZero(), false);
    BOOST_CHECK_EQUAL(fraction.IsPositive(), true);
    BOOST_CHECK_EQUAL(fraction.IsNonNegative(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_ToDouble)
{
    Fraction fraction (1, 4);

    BOOST_CHECK_EQUAL(fraction.ToDouble(), 0.25);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition)
{
    Fraction lhs(2, 3);
    Fraction rhs(3, 4);

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 17);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 12);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_with_internal_simplification_common_denominator)
{
    Fraction lhs(3, 10);
    Fraction rhs(2, 10);

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_with_internal_simplification)
{
    Fraction lhs(3, 10);
    Fraction rhs(1, 5);

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_with_internal_gcd_simplification)
{
    Fraction lhs(1, 6);
    Fraction rhs(2, 15);

    // gcd(6, 15) = 3, so this really is
    //
    // 1 * (15/3) + 2 * (6/3)   1 * 5 + 2 * 2    3
    // ---------------------- = ------------- = --
    //   3 * (6/3) * (15/3)       3 * 2 * 5     10

    Fraction sum = lhs + rhs;

    BOOST_CHECK_EQUAL(sum.GetNumerator(), 3);
    BOOST_CHECK_EQUAL(sum.GetDenominator(), 10);
    BOOST_CHECK_EQUAL(sum.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_subtraction)
{
    Fraction lhs(2, 3);
    Fraction rhs(3, 4);

    Fraction difference = lhs - rhs;

    BOOST_CHECK_EQUAL(difference.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(difference.GetDenominator(), 12);
    BOOST_CHECK_EQUAL(difference.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_subtraction_with_internal_simplification)
{
    Fraction lhs(2, 10);
    Fraction rhs(7, 10);

    Fraction difference = lhs - rhs;

    BOOST_CHECK_EQUAL(difference.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(difference.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(difference.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_with_internal_simplification)
{
    Fraction lhs(-2, 3);
    Fraction rhs(3, 4);

    Fraction product = lhs * rhs;

    BOOST_CHECK_EQUAL(product.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(product.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(product.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_with_cross_simplification_overflow_resistance)
{

    Fraction lhs(std::numeric_limits<int64_t>::max() - 3, std::numeric_limits<int64_t>::max() - 1, false);
    Fraction rhs((std::numeric_limits<int64_t>::max() - 1) / (int64_t) 2, (std::numeric_limits<int64_t>::max() - 3) / (int64_t) 2);

    Fraction product;

    // This should NOT overflow
    bool overflow = false;
    try {
        product = lhs * rhs;
    } catch (std::overflow_error& e) {
        overflow = true;
    }

    BOOST_CHECK_EQUAL(overflow, false);

    if (!overflow) {
        BOOST_CHECK(product == Fraction(1));
    }
}

BOOST_AUTO_TEST_CASE(util_Fraction_division_with_internal_simplification)
{
    Fraction lhs(-2, 3);
    Fraction rhs(4, 3);

    Fraction quotient = lhs / rhs;

    BOOST_CHECK_EQUAL(quotient.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(quotient.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(quotient.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_addition_with_internal_simplification)
{
    Fraction fraction(3, 10);

    fraction += Fraction(2, 10);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_subtraction_with_internal_simplification)
{
    Fraction fraction(7, 10);

    fraction -= Fraction(2, 10);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), 1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_multiplication_with_internal_simplification)
{
    Fraction fraction(-2, 3);

    fraction *= Fraction(3, 4);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_self_division_with_internal_simplification)
{
    Fraction fraction(-2, 3);

    fraction /= Fraction(4, 3);

    BOOST_CHECK_EQUAL(fraction.GetNumerator(), -1);
    BOOST_CHECK_EQUAL(fraction.GetDenominator(), 2);
    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_by_zero_Fraction)
{
    Fraction lhs(-2, 3);
    Fraction rhs(0);

    Fraction product = lhs * rhs;

    BOOST_CHECK_EQUAL(product.GetNumerator(), 0);
    BOOST_CHECK_EQUAL(product.GetDenominator(), 1);
    BOOST_CHECK_EQUAL(product.IsSimplified(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_division_by_zero_Fraction)
{
    Fraction lhs(-2, 3);
    Fraction rhs(0);

    std::string err;

    try {
    Fraction quotient = lhs / rhs;
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, "denominator specified is zero");
}

BOOST_AUTO_TEST_CASE(util_Fraction_division_by_zero_int64_t)
{
    Fraction lhs(-2, 3);
    int64_t rhs = 0;

    std::string err;

    try {
        Fraction quotient = lhs / rhs;
    } catch (std::out_of_range& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string{"denominator specified is zero"});
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_overflow_1)
{
    Fraction lhs((int64_t) 1 << 30, 1);
    Fraction rhs((int64_t) 1 << 31, 1);

    LogPrintf("INFO: %s: msb((int64_t) 1 << 30) = %i", __func__, msb3((int64_t) 1 << 30));
    LogPrintf("INFO: %s: msb((int64_t) 1 << 31) = %i", __func__, msb3((int64_t) 1 << 31));

    std::string err;

    try {
        Fraction product = lhs * rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {});
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_overflow_2)
{
    Fraction lhs(((int64_t) 1 << 31) - 1, 1);
    Fraction rhs(((int64_t) 1 << 31) - 1, 1);

    std::string err;

    try {
        Fraction product = lhs * rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {""});
}

BOOST_AUTO_TEST_CASE(util_Fraction_multiplication_overflow_3)
{
    Fraction lhs((int64_t) 1 << 31, 1);
    Fraction rhs((int64_t) 1 << 31, 1);

    std::string err;

    try {
        Fraction product = lhs * rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {"fraction multiplication results in an overflow"});
}


BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_1)
{
    Fraction lhs(std::numeric_limits<int64_t>::max() / 2, 1);
    Fraction rhs(std::numeric_limits<int64_t>::max() / 2 + 1, 1);

    std::string err;

    try {
        Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {});
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_2)
{
    Fraction lhs(std::numeric_limits<int64_t>::max() / 2 + 1, 1);
    Fraction rhs(std::numeric_limits<int64_t>::max() / 2 + 1, 1);

    std::string err;

    try {
        Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {"fraction addition of a + b where a > 0 and b > 0 results in an overflow"});
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_3)
{
    Fraction lhs(std::numeric_limits<int64_t>::min() / 2 + 1, 1);
    Fraction rhs(std::numeric_limits<int64_t>::min() / 2, 1);

    std::string err;

    try {
        Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {});
}

BOOST_AUTO_TEST_CASE(util_Fraction_addition_overflow_4)
{
    Fraction lhs(std::numeric_limits<int64_t>::min() / 2, 1);
    Fraction rhs(std::numeric_limits<int64_t>::min() / 2, 1);

    std::string err;

    try {
        Fraction addition = lhs + rhs;
    } catch (std::overflow_error& e) {
        err = e.what();
    }

    BOOST_CHECK_EQUAL(err, std::string {"fraction addition of a + b where a < 0 and b < 0 results in an overflow"});
}

BOOST_AUTO_TEST_CASE(util_Fraction_equal)
{
    BOOST_CHECK_EQUAL(Fraction(1, 2) == Fraction(2, 4), true);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) == Fraction(1, -2), true);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) == Fraction(1, 2), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_not_equal)
{
    BOOST_CHECK_EQUAL(Fraction(1, 2) != Fraction(2, 4), false);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) != Fraction(1, -2), false);
    BOOST_CHECK_EQUAL(Fraction(-1, 2) != Fraction(1, 2), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_less_than_or_equal)
{
    BOOST_CHECK_EQUAL(Fraction(3, 4) <= Fraction(4, 5), true);
    BOOST_CHECK_EQUAL(Fraction(3, 4) <= Fraction(6, 8), true);
    BOOST_CHECK_EQUAL(Fraction(3, 4) <= Fraction(2, 3), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_greater_than_or_equal)
{
    BOOST_CHECK_EQUAL(Fraction(4, 5) >= Fraction(3, 4), true);
    BOOST_CHECK_EQUAL(Fraction(6, 8) >= Fraction(3, 4), true);
    BOOST_CHECK_EQUAL(Fraction(2, 3) >= Fraction(3, 4), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_less_than)
{
    BOOST_CHECK_EQUAL(Fraction(3, 4) < Fraction(4, 5), true);
    BOOST_CHECK_EQUAL(Fraction(3, 4) < Fraction(6, 8), false);
    BOOST_CHECK_EQUAL(Fraction(3, 4) < Fraction(2, 3), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_greater_than)
{
    BOOST_CHECK_EQUAL(Fraction(4, 5) > Fraction(3, 4), true);
    BOOST_CHECK_EQUAL(Fraction(6, 8) > Fraction(3, 4), false);
    BOOST_CHECK_EQUAL(Fraction(2, 3) > Fraction(3, 4), false);
}

BOOST_AUTO_TEST_CASE(util_Fraction_logic_negation)
{
    BOOST_CHECK_EQUAL(!Fraction(1, 2), false);
    BOOST_CHECK_EQUAL(!Fraction(-1, 2), false);
    BOOST_CHECK_EQUAL(!Fraction(), true);
}

BOOST_AUTO_TEST_CASE(util_Fraction_ToString)
{
    Fraction fraction(123, 10000);

    BOOST_CHECK_EQUAL(fraction.IsSimplified(), true);
    BOOST_CHECK_EQUAL(fraction.ToString(),"123/10000");
}

BOOST_AUTO_TEST_SUITE_END()
