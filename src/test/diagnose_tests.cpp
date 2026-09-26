// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/diagnose.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(diagnose_tests)

//! A port test site whose name does not resolve is a failed check, not an
//! exception: walletdiagnose and the Diagnostics dialog both call runCheck()
//! with nothing to catch one. ".invalid" is reserved (RFC 6761) and answered
//! with NXDOMAIN by a conforming resolver; with no resolver at all the lookup
//! fails as well, which is the case being guarded.
BOOST_AUTO_TEST_CASE(an_unresolvable_port_test_site_is_reported_not_thrown)
{
    DiagnoseLib::VerifyTCPPort check("gridcoin-port-test.invalid");

    BOOST_CHECK_NO_THROW(check.runCheck());
    BOOST_CHECK_EQUAL(check.getResults(), DiagnoseLib::Diagnose::WARNING);
    BOOST_CHECK(check.getResultsTip().find("unable to be resolved") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
