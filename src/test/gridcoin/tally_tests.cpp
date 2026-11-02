// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "gridcoin/account.h"
#include "gridcoin/cpid.h"
#include "gridcoin/superblock.h"
#include "gridcoin/tally.h"
#include "node/shutdown.h"
#include "sync.h"
#include "test/test_gridcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(tally_tests)

BOOST_AUTO_TEST_CASE(it_refuses_a_superblock_that_is_not_well_formed)
{
    LOCK(cs_main);

    const GRC::Cpid cpid(InsecureRandBytes(16));
    GRC::Tally::CreateAccount(cpid).m_accrual = 12345;

    // SuperblockPtr::ReadFromDisk() returns an empty superblock when it cannot
    // read a superblock block. The tally refuses it before it changes any
    // account, without stopping the node.
    BOOST_CHECK(!GRC::Tally::ApplySuperblock(GRC::SuperblockPtr::Empty(), nullptr));
    BOOST_CHECK_EQUAL(GRC::Tally::GetAccount(cpid).m_accrual, 12345);
    BOOST_CHECK(!ShutdownRequested());

    GRC::Tally::RemoveAccount(cpid);
}

BOOST_AUTO_TEST_SUITE_END()
