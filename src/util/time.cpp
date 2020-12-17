// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/gridcoin-config.h>
#endif

#include <util/time.h>

#include <atomic>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <ctime>
#include <tinyformat.h>

static int64_t nMockTime = 0;  // For unit testing

int64_t GetTime()
{
    if (nMockTime) return nMockTime;

    return time(NULL);
}

int64_t GetMockTime()
{
    return nMockTime;
}

void SetMockTime(int64_t nMockTimeIn)
{
    nMockTime = nMockTimeIn;
}

template <typename T>
T GetTime()
{
    const std::chrono::seconds mocktime(nMockTime);

    return std::chrono::duration_cast<T>(
        mocktime.count() ?
            mocktime :
            std::chrono::microseconds{GetTimeMicros()});
}
template std::chrono::seconds GetTime();
template std::chrono::milliseconds GetTime();
template std::chrono::microseconds GetTime();

int64_t GetTimeMillis()
{
    int64_t now = (boost::posix_time::microsec_clock::universal_time() -
                   boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
    assert(now > 0);
    return now;
}

int64_t GetTimeMicros()
{
    int64_t now = (boost::posix_time::microsec_clock::universal_time() -
                   boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
    assert(now > 0);
    return now;
}

int64_t GetSystemTimeInSeconds()
{
    return GetTimeMicros()/1000000;
}

void MilliTimer::InitTimer(std::string label)
{
    internal_timer timer;

    timer.start_time = GetTimeMillis();
    timer.checkpoint_time = timer.start_time;

    LOCK(cs_timer_map_lock);

    // the [] either creates a new timer with the label or replaces an existing one.
    timer_map[label] = timer;
}

bool MilliTimer::DeleteTimer(std::string label)
{
    bool delete_status = false;

    LOCK(cs_timer_map_lock);

    if (timer_map.find(label) != timer_map.end())
    {
        timer_map.erase(label);

        delete_status = true;
    }

    return delete_status;
}

int64_t MilliTimer::GetElapsedTime(std::string label)
{
    internal_timer internal_timer;
    timer timer;

    LOCK(cs_timer_map_lock);

    try
    {
        // This will throw an exception if the entry specified by label doesn't exist.
        internal_timer = timer_map.at(label);

        int64_t current_time = GetTimeMillis();

        timer.elapsed_time = GetTimeMillis() - internal_timer.start_time;

        internal_timer.checkpoint_time = current_time;

        //Because of the exception guard above the map entry can be updated with [].
        timer_map[label] = internal_timer;
    }
    catch (std::out_of_range)
    {
        timer.elapsed_time = 0;
    }

    return timer.elapsed_time;
}

MilliTimer::timer MilliTimer::GetTimes(std::string label)
{
    internal_timer internal_timer;
    timer timer;

    LOCK(cs_timer_map_lock);

    try
    {
        // This will throw an internal exception if the entry specified by label doesn't exist.
        internal_timer = timer_map.at(label);

        int64_t current_time = GetTimeMillis();

        timer.elapsed_time = current_time - internal_timer.start_time;
        timer.time_since_last_check = current_time - internal_timer.checkpoint_time;

        internal_timer.checkpoint_time = current_time;

        //Because of the exception guard above the map entry can be updated with [].
        timer_map[label] = internal_timer;
    }
    catch (std::out_of_range)
    {
        //Re-initialize timer if internal exception is thrown.
        timer = {};
    }

    return timer;
}

// Create global timer instance.
MilliTimer g_timer;

void MilliSleep(int64_t n)
{

/**
 * Boost's sleep_for was uninterruptible when backed by nanosleep from 1.50
 * until fixed in 1.52. Use the deprecated sleep method for the broken case.
 * See: https://svn.boost.org/trac/boost/ticket/7238
 */
#if defined(HAVE_WORKING_BOOST_SLEEP_FOR)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
#elif defined(HAVE_WORKING_BOOST_SLEEP)
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
#else
//should never get here
#error missing boost sleep implementation
#endif
}

std::string FormatISO8601DateTime(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef HAVE_GMTIME_R
    if (gmtime_r(&time_val, &ts) == nullptr) {
#else
    if (gmtime_s(&ts, &time_val) != 0) {
#endif
        return {};
    }
    return strprintf("%04i-%02i-%02iT%02i:%02i:%02iZ", ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);
}

std::string FormatISO8601Date(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef HAVE_GMTIME_R
    if (gmtime_r(&time_val, &ts) == nullptr) {
#else
    if (gmtime_s(&ts, &time_val) != 0) {
#endif
        return {};
    }
    return strprintf("%04i-%02i-%02i", ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday);
}

int64_t ParseISO8601DateTime(const std::string& str)
{
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time() || epoch > ptime)
        return 0;
    return (ptime - epoch).total_seconds();
}
