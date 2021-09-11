// Copyright (c) 2012-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <scheduler.h>

#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(scheduler_tests)

static void microTask(CScheduler& s, boost::mutex& mutex, int& counter, int delta, boost::chrono::system_clock::time_point rescheduleTime)
{
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        counter += delta;
    }
    boost::chrono::system_clock::time_point noTime = boost::chrono::system_clock::time_point::min();
    if (rescheduleTime != noTime) {
        CScheduler::Function f = std::bind(&microTask, std::ref(s), std::ref(mutex), std::ref(counter), -delta + 1, noTime);
        s.schedule(f, rescheduleTime);
    }
}

BOOST_AUTO_TEST_CASE(wait_until_past)
{
    boost::condition_variable condvar;
    boost::mutex mtx;
    boost::unique_lock<boost::mutex> lock{mtx};

    const auto wait_until = [&](const boost::chrono::seconds& d) {
        return condvar.wait_until<>(lock, boost::chrono::system_clock::now() - d);
    };

    BOOST_CHECK(boost::cv_status::timeout == wait_until(boost::chrono::seconds{1}));
    BOOST_CHECK(boost::cv_status::timeout == wait_until(boost::chrono::minutes{1}));
    BOOST_CHECK(boost::cv_status::timeout == wait_until(boost::chrono::hours{1}));
    BOOST_CHECK(boost::cv_status::timeout == wait_until(boost::chrono::hours{10}));
    BOOST_CHECK(boost::cv_status::timeout == wait_until(boost::chrono::hours{100}));
    BOOST_CHECK(boost::cv_status::timeout == wait_until(boost::chrono::hours{1000}));
}

BOOST_AUTO_TEST_SUITE_END()
