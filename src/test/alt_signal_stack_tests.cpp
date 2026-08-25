// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <test/alt_signal_stack.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(alt_signal_stack_tests)

//!
//! The alternate signal stack installed before main() must meet the size the
//! kernel reports for this CPU.
//!
//! This asserts on a RECORD taken at static-initialisation time, not on the live
//! sigaltstack(2) state, and that distinction is the whole point. Boost's
//! signal_handler destructor disables the alternate stack at the end of every
//! monitored scope, so by the time this test case runs there is no stack
//! installed at all -- querying it here would report zero bytes and prove
//! nothing about whether the fix worked. An earlier version of this test did
//! exactly that and failed for that reason.
//!
//! What this catches: the pre-install being removed, being sized from SIGSTKSZ
//! again rather than from AT_MINSIGSTKSZ, or silently failing. What it cannot
//! catch is the original musl abort -- on a host where that fires, the process
//! dies during framework::init() and no test case ever runs. That case is
//! verified with strace instead; see alt_signal_stack.cpp.
//!
BOOST_AUTO_TEST_CASE(the_pre_main_alternate_stack_meets_the_kernel_minimum)
{
    const AltSignalStackReport& report = GetAltSignalStackReport();

#if !defined(__linux__)
    BOOST_CHECK(!report.attempted);
    BOOST_TEST_MESSAGE("not Linux; no alternate stack is installed by us");
    return;
#else
    BOOST_REQUIRE_MESSAGE(report.attempted,
                          "the static initialiser in alt_signal_stack.cpp did not run");

    BOOST_CHECK_MESSAGE(report.installed,
                        "sigaltstack(2) rejected our alternate stack of "
                        << report.requested << " bytes");

    // Zero means the kernel did not publish AT_MINSIGSTKSZ. Nothing to compare
    // against, so the size check is skipped rather than failed.
    if (report.kernel_minimum == 0) {
        BOOST_TEST_MESSAGE("AT_MINSIGSTKSZ unavailable; size comparison skipped");
        return;
    }

    BOOST_CHECK_MESSAGE(report.requested >= report.kernel_minimum,
                        "requested " << report.requested
                        << " bytes, below the kernel minimum of "
                        << report.kernel_minimum);
#endif
}

BOOST_AUTO_TEST_SUITE_END()
