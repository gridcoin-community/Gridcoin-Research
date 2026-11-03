// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "logging.h"
#include "test/test_gridcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(logging_tests)

BOOST_AUTO_TEST_CASE(archive_does_nothing_without_a_log_file)
{
    // A logger that never opened its file, as with -nodebuglogfile.
    BCLog::Logger logger;
    BOOST_REQUIRE(!logger.m_print_to_file);

    BOOST_CHECK(!logger.archive(/*fImmediate=*/true, fs::path()));
}

BOOST_AUTO_TEST_SUITE_END()
