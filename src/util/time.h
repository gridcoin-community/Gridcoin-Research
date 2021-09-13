// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_TIME_H
#define BITCOIN_UTIL_TIME_H

#include <compat.h>

#include <chrono>
#include <stdint.h>
#include <string>

#include <unordered_map>
#include <sync.h>
#include <threadinterrupt.h>

using namespace std::chrono_literals;

extern CThreadInterrupt g_thread_interrupt;

/**
 * @brief An interruptible sleep wrapper around the global CThreadInterrupt g_thread_interrupt.sleep_for() method. In
 * general, as in Bitcoin upstream, we will get away from one global CThreadInterrupt object and move towards separate
 * objects for each major area, such as net, Gridcoin services, etc. and calling the interrupt object directly.
 * @param int64_t n: number of milliseconds to sleep.
 * @returns [[nodiscard]] boolean: true if sleep timeout occurred, false if interrupted by signal. This
 * is marked as [[nodiscard]] as thread implementations of MilliSleep should use the boolean as a conditional for
 * return.
 */
[[nodiscard]] bool MilliSleep(int64_t n);

void UninterruptibleSleep(const std::chrono::microseconds& n);

/**
 * Helper to count the seconds of a duration.
 *
 * All durations should be using std::chrono and calling this should generally
 * be avoided in code. Though, it is still preferred to an inline t.count() to
 * protect against a reliance on the exact type of t.
 *
 * This helper is used to convert durations before passing them over an
 * interface that doesn't support std::chrono (e.g. RPC, debug log, or the GUI)
 */
constexpr int64_t count_seconds(std::chrono::seconds t) { return t.count(); }
constexpr int64_t count_milliseconds(std::chrono::milliseconds t) { return t.count(); }
constexpr int64_t count_microseconds(std::chrono::microseconds t) { return t.count(); }

using SecondsDouble = std::chrono::duration<double, std::chrono::seconds::period>;

/**
 * Helper to count the seconds in any std::chrono::duration type
 */
inline double CountSecondsDouble(SecondsDouble t) { return t.count(); }

/**
 * DEPRECATED
 * Use either GetTimeSeconds (not mockable) or GetTime<T> (mockable)
 */
int64_t GetTime();

/** Returns the system time (not mockable) */
int64_t GetTimeMillis();
/** Returns the system time (not mockable) */
int64_t GetTimeMicros();
/** Returns the system time (not mockable) */
int64_t GetTimeSeconds(); // Like GetTime(), but not mockable

/**
 * DEPRECATED
 * Use SetMockTime with chrono type
 *
 * @param[in] nMockTimeIn Time in seconds.
 */
void SetMockTime(int64_t nMockTimeIn);

/** For testing. Set e.g. with the setmocktime rpc, or -mocktime argument */
void SetMockTime(std::chrono::seconds mock_time_in);

/** For testing */
std::chrono::seconds GetMockTime();

/** Return system time (or mocked time, if set) */
template <typename T>
T GetTime();

/**
 * ISO 8601 formatting is preferred. Use the FormatISO8601{DateTime,Date}
 * helper functions if possible.
 */
std::string FormatISO8601DateTime(int64_t nTime);
std::string FormatISO8601Date(int64_t nTime);
int64_t ParseISO8601DateTime(const std::string& str);

/**
 * Convert milliseconds to a struct timeval for e.g. select.
 */
struct timeval MillisToTimeval(int64_t nTimeout);

/**
 * Convert milliseconds to a struct timeval for e.g. select.
 */
struct timeval MillisToTimeval(std::chrono::milliseconds ms);

/** Sanity check epoch match normal Unix epoch */
bool ChronoSanityCheck();

/**
 * @brief The MilliTimer class
 *
 * This is a small class that implements a stopwatch style timer. The class can be used locally (usually using
 * the "default" label) and/or as a global map of timers. If you want to do just a simple single timing, it
 * probably makes more sense to just use GetTimeMillis() directly, as the initialization of a map is a little
 * heavyweight for a single timing that is locally scoped. This class becomes very useful for multiple segments
 * where you need to see the "lap times", and also conveniently control the logging without a bunch of repetitive code.
 *
 * This class becomes essential for tracking the timing processes that span multiple functions/methods, that are
 * critical to measure performance-wise. This includes the init code and miner loop, for example. The default timer,
 * which is started at the main() entry-points, also provides a very convenient uptime timer for getinfo.
 *
 * The class uses a single lock which is scoped very tightly to the single map that the lock protects, and is
 * entirely internal to the class (private). As a result this class is entirely thread-safe. In general the performance
 * of the timers should be very good down to the millisecond range, even in multi-threaded access situations with
 * several global timers active in the map. Nevertheless, I would NOT recommend putting a GetTimes or GetElapsedTime
 * call within a very tight loop that executes at 1000's of times/second, as the lock could become effectively
 * partially blocking for other threads. As usual, common-sense must be used with any tool.
 *
 * I did not implement the "stop" button on the stopwatch style timer, as the "lap" time functionality is more
 * important here, and implementing a stop/start functionality on the timers makes it more heavyweight and
 * unnecessarily complicated for our use here.
 *
 */
class MilliTimer
{
public:
    MilliTimer()
    {
        InitTimer("default", false);
    }

    MilliTimer(const std::string& label)
    {
        InitTimer(label, false);
    }

    MilliTimer(const std::string& label, bool log)
    {
        InitTimer(label, log);
    }

    struct timer
    {
        int64_t elapsed_time = 0;
        int64_t time_since_last_check = 0;
    };

    void InitTimer(const std::string& label, bool log);
    bool DeleteTimer(const std::string& label);
    bool LogTimer(const std::string& label, bool log);

    int64_t GetStartTime(const std::string& label = "default");
    const timer GetTimes(const std::string& log_string, const std::string& label = "default");
    int64_t GetElapsedTime(const std::string& log_string, const std::string& label = "default");
private:
    CCriticalSection cs_timer_map_lock;

    struct internal_timer
    {
        bool log = false;
        int64_t start_time = 0;
        int64_t checkpoint_time = 0;
    };

    std::unordered_map<std::string, internal_timer> timer_map;
};

extern MilliTimer g_timer;

#endif // BITCOIN_UTIL_TIME_H
