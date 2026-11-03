// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/proc_hardening.h>

#include <boost/test/unit_test.hpp>

#include <optional>
#include <sstream>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

BOOST_AUTO_TEST_SUITE(proc_hardening_tests)

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
