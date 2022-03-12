// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/gridcoin-config.h>
#endif

#include <compat.h>
#include <util/time.h>

#include <util/check.h>

#include <atomic>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <ctime>
#include <thread>
#include <inttypes.h>

#include <tinyformat.h>
#include "logging.h"

CThreadInterrupt g_thread_interrupt;

[[nodiscard]] bool MilliSleep(int64_t n)
{
    return g_thread_interrupt.sleep_for(std::chrono::milliseconds{n});
}

void UninterruptibleSleep(const std::chrono::microseconds& n) { std::this_thread::sleep_for(n); }

static std::atomic<int64_t> nMockTime(0); //!< For testing

int64_t GetTime()
{
    int64_t mocktime = nMockTime.load(std::memory_order_relaxed);
    if (mocktime) return mocktime;

    time_t now = time(nullptr);
    assert(now > 0);
    return now;
}

bool ChronoSanityCheck()
{
    // std::chrono::system_clock.time_since_epoch and time_t(0) are not guaranteed
    // to use the Unix epoch timestamp, prior to C++20, but in practice they almost
    // certainly will. Any differing behavior will be assumed to be an error, unless
    // certain platforms prove to consistently deviate, at which point we'll cope
    // with it by adding offsets.

    // Create a new clock from time_t(0) and make sure that it represents 0
    // seconds from the system_clock's time_since_epoch. Then convert that back
    // to a time_t and verify that it's the same as before.
    const time_t time_t_epoch{};
    auto clock = std::chrono::system_clock::from_time_t(time_t_epoch);
    if (std::chrono::duration_cast<std::chrono::seconds>(clock.time_since_epoch()).count() != 0) {
        return false;
    }

    time_t time_val = std::chrono::system_clock::to_time_t(clock);
    if (time_val != time_t_epoch) {
        return false;
    }

    // Check that the above zero time is actually equal to the known unix timestamp.
    struct tm epoch;
#ifdef HAVE_GMTIME_R
    if (gmtime_r(&time_val, &epoch) == nullptr) {
#else
    if (gmtime_s(&epoch, &time_val) != 0) {
#endif
        return false;
    }

    if ((epoch.tm_sec != 0)  ||
       (epoch.tm_min  != 0)  ||
       (epoch.tm_hour != 0)  ||
       (epoch.tm_mday != 1)  ||
       (epoch.tm_mon  != 0)  ||
       (epoch.tm_year != 70)) {
        return false;
    }
    return true;
}

template <typename T>
T GetTime()
{
    const std::chrono::seconds mocktime{nMockTime.load(std::memory_order_relaxed)};

    return std::chrono::duration_cast<T>(
        mocktime.count() ?
            mocktime :
            std::chrono::microseconds{GetTimeMicros()});
}
template std::chrono::seconds GetTime();
template std::chrono::milliseconds GetTime();
template std::chrono::microseconds GetTime();

template <typename T>
static T GetSystemTime()
{
    const auto now = std::chrono::duration_cast<T>(std::chrono::system_clock::now().time_since_epoch());
    assert(now.count() > 0);
    return now;
}

void SetMockTime(int64_t nMockTimeIn)
{
    Assert(nMockTimeIn >= 0);
    nMockTime.store(nMockTimeIn, std::memory_order_relaxed);
}

void SetMockTime(std::chrono::seconds mock_time_in)
{
    nMockTime.store(mock_time_in.count(), std::memory_order_relaxed);
}

std::chrono::seconds GetMockTime()
{
    return std::chrono::seconds(nMockTime.load(std::memory_order_relaxed));
}

int64_t GetTimeMillis()
{
    return int64_t{GetSystemTime<std::chrono::milliseconds>().count()};
}

int64_t GetTimeMicros()
{
    return int64_t{GetSystemTime<std::chrono::microseconds>().count()};
}

int64_t GetTimeSeconds()
{
    return int64_t{GetSystemTime<std::chrono::seconds>().count()};
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

std::string FormatISO8601DateTimeDashSep(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef HAVE_GMTIME_R
    if (gmtime_r(&time_val, &ts) == nullptr) {
#else
    if (gmtime_s(&ts, &time_val) != 0) {
#endif
        return {};
    }
    return strprintf("%04i-%02i-%02iT%02i-%02i-%02iZ", ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec);
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

struct timeval MillisToTimeval(int64_t nTimeout)
{
    struct timeval timeout;
    timeout.tv_sec  = nTimeout / 1000;
    timeout.tv_usec = (nTimeout % 1000) * 1000;
    return timeout;
}

struct timeval MillisToTimeval(std::chrono::milliseconds ms)
{
    return MillisToTimeval(count_milliseconds(ms));
}

void MilliTimer::InitTimer(const std::string& label, bool log)
{
    internal_timer timer;

    timer.start_time = GetTimeMillis();
    timer.checkpoint_time = timer.start_time;
    timer.log = log;

    LOCK(cs_timer_map_lock);

    // the [] either creates a new timer with the label or replaces an existing one.
    timer_map[label] = timer;
}

bool MilliTimer::DeleteTimer(const std::string& label)
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

bool MilliTimer::LogTimer(const std::string& label, bool log)
{
    LOCK(cs_timer_map_lock);

    auto it = timer_map.find(label);

    if (it != timer_map.end())
    {
        it->second.log = log;
        return true;
    }

    return false;
}

int64_t MilliTimer::GetStartTime(const std::string& label)
{
    internal_timer internal_timer;

    try
    {
        LOCK(cs_timer_map_lock);

        // This will throw an internal exception if the entry specified by label doesn't exist.
        internal_timer = timer_map.at(label);
    }
    catch (std::out_of_range&) {
        LogPrintf("WARNING: %s: Timer with specified label does not exist. Returning zero start time.");
    }

    return internal_timer.start_time;
}

const MilliTimer::timer MilliTimer::GetTimes(const std::string& log_string, const std::string& label)
{
    internal_timer internal_timer;
    timer timer;

    try
    {
        LOCK(cs_timer_map_lock);

        // This will throw an internal exception if the entry specified by label doesn't exist.
        internal_timer = timer_map.at(label);

        int64_t current_time = GetTimeMillis();

        timer.elapsed_time = current_time - internal_timer.start_time;
        timer.time_since_last_check = current_time - internal_timer.checkpoint_time;

        internal_timer.checkpoint_time = current_time;

        //Because of the exception guard above the map entry can be updated with [].
        timer_map[label] = internal_timer;
    }
    catch (std::out_of_range&)
    {
        LogPrintf("WARNING: %s: Timer with specified label does not exist. Returning zeroed timer.");
        timer = {};
        return timer;
    }

    // Only log if no exception above, and also done after the release of the lock on the map to
    // minimize lock time.
    if (internal_timer.log)
    {
        LogPrintf("timer %s: %s: elapsed time: %" PRId64 " ms, time since last check: %" PRId64 " ms.",
                  label, log_string, timer.elapsed_time, timer.time_since_last_check);
    }

    return timer;
}

int64_t MilliTimer::GetElapsedTime(const std::string& log_string, const std::string& label)
{
    return GetTimes(log_string, label).elapsed_time;
}

// Create global timer instance.
MilliTimer g_timer;
