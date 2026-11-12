// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <init.h>
#include <util.h>
#include <util/proc_hardening.h>

#include <boost/test/unit_test.hpp>

#include <optional>
#include <sstream>
#include <string>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace {
struct ConfArgsManager : public ArgsManager
{
    bool ReadConf(const std::string& conf)
    {
        std::istringstream stream(conf);
        std::string error;
        return ReadConfigStream(stream, "", error);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(proc_hardening_tests)

// The override is registered command line only (SetupServerArgs), so a config file line cannot enable it.
BOOST_AUTO_TEST_CASE(override_is_command_line_only)
{
    gArgs.ClearArgs();
    struct ClearOnExit { ~ClearOnExit() { gArgs.ClearArgs(); } } clear_on_exit;
    SetupServerArgs();

    const auto flags = gArgs.GetRegisteredArgFlags();
    BOOST_REQUIRE(flags.count("-debugallowtracer"));
    BOOST_CHECK(flags.at("-debugallowtracer") & ArgsManager::COMMAND_LINE_ONLY);

    ConfArgsManager args;
    args.AddArg("-debugallowtracer", "", flags.at("-debugallowtracer"), OptionsCategory::OPTIONS);

    const char* argv[] = {"gridcoinresearchd"};
    std::string error;
    BOOST_REQUIRE(args.ParseParameters(1, argv, error));
    BOOST_REQUIRE(args.ReadConf("debugallowtracer=1\n"));

    BOOST_CHECK(!args.GetBoolArg("-debugallowtracer", false));
    BOOST_CHECK(DecideTracer(4242, args.GetBoolArg("-debugallowtracer", false)) == TracerVerdict::Refuse);
}

BOOST_AUTO_TEST_CASE(parse_tracer_pid)
{
    std::istringstream zero("Name:\tx\nTracerPid:\t0\n");
    BOOST_CHECK(ParseTracerPid(zero) == std::optional<int>(0));

    std::istringstream attached("TracerPid:\t4242\n");
    BOOST_CHECK(ParseTracerPid(attached) == std::optional<int>(4242));

    std::istringstream absent("Name:\tx\n");
    BOOST_CHECK(!ParseTracerPid(absent).has_value());

    std::istringstream garbage("TracerPid:\tabc\n");
    BOOST_CHECK(!ParseTracerPid(garbage).has_value());
}

BOOST_AUTO_TEST_CASE(decide_tracer_table)
{
    BOOST_CHECK(DecideTracer(0, false) == TracerVerdict::Start);
    BOOST_CHECK(DecideTracer(4242, false) == TracerVerdict::Refuse);
    BOOST_CHECK(DecideTracer(std::nullopt, false) == TracerVerdict::Start);
    BOOST_CHECK(DecideTracer(4242, true) == TracerVerdict::StartDumpableForDebugging);
    BOOST_CHECK(DecideTracer(0, true) == TracerVerdict::StartDumpableForDebugging);
}

// Only Linux has a tracer check. Elsewhere there is nothing to read, which is no failure and
// is not worth a warning on every start; a read that fails where the check exists is.
BOOST_AUTO_TEST_CASE(tracer_warning_only_where_the_check_exists)
{
#if defined(__linux__)
    BOOST_CHECK(TracerCheckSupported());
#else
    BOOST_CHECK(!TracerCheckSupported());
#endif

    const std::optional<std::string> failed = TracerStateWarning(std::nullopt, /*supported=*/true);
    BOOST_REQUIRE(failed.has_value());
    BOOST_CHECK(failed->find("WARNING: could not read the tracer state") == 0);

    BOOST_CHECK(!TracerStateWarning(std::nullopt, /*supported=*/false).has_value());
    BOOST_CHECK(!TracerStateWarning(0, /*supported=*/true).has_value());
    // An attached tracer is DecideTracer's to refuse, not a failed read.
    BOOST_CHECK(!TracerStateWarning(4242, /*supported=*/true).has_value());

    // What both entry points log on this platform, where the read succeeds or does not exist.
    BOOST_CHECK(!TracerStateWarning(ReadOwnTracerPid(), TracerCheckSupported()).has_value());
}

#if defined(__linux__)
BOOST_AUTO_TEST_CASE(non_dumpable_takes_effect)
{
    BOOST_CHECK(SetProcessNonDumpable());
    BOOST_CHECK_EQUAL(prctl(PR_GET_DUMPABLE, 0, 0, 0, 0), 0);

    // The test binary must stay debuggable, so restore the default.
    prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
    BOOST_CHECK_EQUAL(prctl(PR_GET_DUMPABLE, 0, 0, 0, 0), 1);
}

BOOST_AUTO_TEST_CASE(read_own_tracer_pid_present)
{
    BOOST_CHECK(ReadOwnTracerPid().has_value());
}
#endif

BOOST_AUTO_TEST_SUITE_END()
