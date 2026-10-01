// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "test/state_guard.h"
#include "wallet/diagnose.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(diagnose_tests)

//! A port test site whose name does not resolve is a failed check, not an
//! exception: walletdiagnose and the Diagnostics dialog both call runCheck()
//! with nothing to catch one.
//!
//! No resolver is asked. numeric_host refuses any name that is not an address
//! literal before a lookup starts, so the case is the same offline, on a
//! filtered resolver and on a working one; ".invalid" (RFC 6761) is a second
//! guard. The check does nothing on a mockable chain, so the case selects
//! mainnet itself rather than relying on the network the binary started with,
//! and the guard puts that network back.
BOOST_AUTO_TEST_CASE(an_unresolvable_port_test_site_is_reported_not_thrown)
{
    grc_test::StateGuard guard;
    SelectParams(CBaseChainParams::MAIN);
    BOOST_REQUIRE(!Params().IsMockableChain());

    DiagnoseLib::VerifyTCPPort check("gridcoin-port-test.invalid", boost::asio::ip::resolver_base::numeric_host);

    BOOST_CHECK_NO_THROW(check.runCheck());
    BOOST_CHECK_EQUAL(check.getResults(), DiagnoseLib::Diagnose::WARNING);
    BOOST_CHECK(check.getResultsTip().find("unable to be resolved") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
