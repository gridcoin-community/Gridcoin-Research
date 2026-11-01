// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/threadnames.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <thread>

BOOST_AUTO_TEST_SUITE(util_threadnames_tests)

BOOST_AUTO_TEST_CASE(internal_name_is_per_thread)
{
    const std::string original = util::ThreadGetInternalName();

    std::string seen_in_thread;
    std::thread worker([&] {
        util::ThreadSetInternalName("threadnames-test-worker");
        seen_in_thread = util::ThreadGetInternalName();
    });
    worker.join();

    BOOST_CHECK_EQUAL(seen_in_thread, "threadnames-test-worker");
    // The worker's name stays with the worker.
    BOOST_CHECK_EQUAL(util::ThreadGetInternalName(), original);
}

BOOST_AUTO_TEST_CASE(internal_name_round_trips_and_truncates)
{
    std::string got_short;
    std::string got_long;
    std::thread worker([&] {
        util::ThreadSetInternalName("grc-short");
        got_short = util::ThreadGetInternalName();
        util::ThreadSetInternalName(std::string(300, 'x'));
        got_long = util::ThreadGetInternalName();
    });
    worker.join();

    BOOST_CHECK_EQUAL(got_short, "grc-short");
    // The name is kept in a fixed buffer; anything past 127 characters is dropped.
    BOOST_CHECK_EQUAL(got_long, std::string(127, 'x'));
}

BOOST_AUTO_TEST_SUITE_END()
