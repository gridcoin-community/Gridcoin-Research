#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include "util/system.h"

BOOST_AUTO_TEST_SUITE(getarg_tests)

static void AddArgs(const std::string& strArg)
{
    std::vector<std::string> vecArg;
    boost::split(vecArg, strArg, boost::is_space(), boost::token_compress_on);

    for (const auto& arg : vecArg)
    {
        gArgs.AddArg(arg, arg, ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    }
}

static bool ResetArgs(const std::string& strAddArg, const std::string& strArgIn = std::string())
{
    gArgs.ClearArgs();
    AddArgs(strAddArg);

    std::string strArg = strArgIn.empty() ? strAddArg : strArgIn;

    std::vector<std::string> vecArg;
    boost::split(vecArg, strArg, boost::is_space(), boost::token_compress_on);


    // Insert dummy executable name:
    vecArg.insert(vecArg.begin(), "testgridcoin");

    // Convert to char*:
    std::vector<const char*> vecChar;
    for (std::string& s : vecArg)
        vecChar.push_back(s.c_str());

    std::string error;

    bool status = gArgs.ParseParameters(vecChar.size(), &vecChar[0], error);

    if (!status) printf("ERROR: %s: %s", __func__, error.c_str());

    return status;
}

BOOST_AUTO_TEST_CASE(boolarg)
{

    BOOST_CHECK(ResetArgs("-foo"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", true));

    BOOST_CHECK(!gArgs.GetBoolArg("-fo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-fo", false));
    BOOST_CHECK(gArgs.GetBoolArg("-fo", true));

    BOOST_CHECK(!gArgs.GetBoolArg("-fooo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-fooo", false));
    BOOST_CHECK(gArgs.GetBoolArg("-fooo", true));

    BOOST_CHECK(ResetArgs("-foo", "-foo=0"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));

    BOOST_CHECK(ResetArgs("-foo", "-foo=1"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", true));

    // New 0.6 feature: auto-map -nosomething to !-something:
    BOOST_CHECK(ResetArgs("-foo", "-nofoo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));

    BOOST_CHECK(ResetArgs("-foo", "-nofoo=1"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));

    // TODO: Did Bitcoin change the order of precedence of negation?
    // Before -foo should win. Now it is the last argument?
    BOOST_CHECK(ResetArgs("-foo", "-foo -nofoo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));

    BOOST_CHECK(ResetArgs("-foo", "-foo=1 -nofoo=1"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));

    // A double negative?
    BOOST_CHECK(ResetArgs("-foo", "-foo=0 -nofoo=0"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", true));

    // New 0.6 feature: treat -- same as -:
    BOOST_CHECK(ResetArgs("-foo", "--foo=1"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", true));

    BOOST_CHECK(ResetArgs("-foo", "--nofoo=1"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));
}

BOOST_AUTO_TEST_CASE(stringarg)
{
    BOOST_CHECK(ResetArgs(""));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", ""), "");
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", "eleven"), "eleven");

    BOOST_CHECK(ResetArgs("-foo -bar"));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", ""), "");
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", "eleven"), "");

    BOOST_CHECK(ResetArgs("-foo", "-foo="));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", ""), "");
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", "eleven"), "");

    BOOST_CHECK(ResetArgs("-foo", "-foo=11"));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", ""), "11");
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", "eleven"), "11");

    BOOST_CHECK(ResetArgs("-foo", "-foo=eleven"));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", ""), "eleven");
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", "eleven"), "eleven");

}

BOOST_AUTO_TEST_CASE(intarg)
{
    BOOST_CHECK(ResetArgs(""));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", 11), 11);
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", 0), 0);

    BOOST_CHECK(ResetArgs("-foo -bar"));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", 11), 0);
    BOOST_CHECK_EQUAL(gArgs.GetArg("-bar", 11), 0);

    BOOST_CHECK(ResetArgs("-foo -bar", "-foo=11 -bar=12"));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", 0), 11);
    BOOST_CHECK_EQUAL(gArgs.GetArg("-bar", 11), 12);

    BOOST_CHECK(ResetArgs("-foo -bar", "-foo=NaN -bar=NotANumber"));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", 1), 0);
    BOOST_CHECK_EQUAL(gArgs.GetArg("-bar", 11), 0);
}

BOOST_AUTO_TEST_CASE(doubledash)
{
    BOOST_CHECK(ResetArgs("-foo", "--foo"));
    BOOST_CHECK_EQUAL(gArgs.GetBoolArg("-foo"), true);

    BOOST_CHECK(ResetArgs("-foo -bar", "--foo=verbose --bar=1"));
    BOOST_CHECK_EQUAL(gArgs.GetArg("-foo", ""), "verbose");
    BOOST_CHECK_EQUAL(gArgs.GetArg("-bar", 0), 1);
}

BOOST_AUTO_TEST_CASE(boolargno)
{
    BOOST_CHECK(ResetArgs("-foo", "-nofoo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));

    BOOST_CHECK(ResetArgs("-foo", "-nofoo=1"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", true));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo", false));

    BOOST_CHECK(ResetArgs("-foo", "-nofoo=0"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", true));
    BOOST_CHECK(gArgs.GetBoolArg("-foo", false));

    // TODO: Did Bitcoin change the order of precedence of negation?
    // Before -foo should win. Now it is the last argument?
    BOOST_CHECK(ResetArgs("-foo", "-foo --nofoo"));
    BOOST_CHECK(!gArgs.GetBoolArg("-foo"));

    BOOST_CHECK(ResetArgs("-foo", "-nofoo -foo"));
    BOOST_CHECK(gArgs.GetBoolArg("-foo"));
}

BOOST_AUTO_TEST_SUITE_END()
