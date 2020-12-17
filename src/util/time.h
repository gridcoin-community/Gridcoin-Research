// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_TIME_H
#define BITCOIN_UTIL_TIME_H

#include <stdint.h>
#include <string>
#include <chrono>
#include <unordered_map>

#include "sync.h"

/**
 * Helper to count the seconds of a duration.
 *
 * All durations should be using std::chrono and calling this should generally be avoided in code. Though, it is still
 * preferred to an inline t.count() to protect against a reliance on the exact type of t.
 */
inline int64_t count_seconds(std::chrono::seconds t) { return t.count(); }

/**
 * DEPRECATED
 * Use either GetSystemTimeInSeconds (not mockable) or GetTime<T> (mockable)
 */
int64_t GetTime();

/** Returns the system time (not mockable) */
int64_t GetTimeMillis();
/** Returns the system time (not mockable) */
int64_t GetTimeMicros();
/** Returns the system time (not mockable) */
int64_t GetSystemTimeInSeconds(); // Like GetTime(), but not mockable

/** For testing. Set e.g. with the setmocktime rpc, or -mocktime argument */
void SetMockTime(int64_t nMockTimeIn);
/** For testing */
int64_t GetMockTime();

void MilliSleep(int64_t n);

/**
 * @brief The MilliTimer class
 *
 * This is a small class that implements a stopwatch style timer. The class can be used locally (usually using
 * the "default" label) or as a global map of timers.
 *
 */
class MilliTimer
{
public:
    MilliTimer()
    {
        InitTimer("default");
    }

    MilliTimer(std::string label)
    {
        InitTimer(label);
    }

    struct timer
    {
        int64_t elapsed_time = 0;
        int64_t time_since_last_check = 0;
    };

    void InitTimer(std::string label);
    bool DeleteTimer(std::string label);

    int64_t GetElapsedTime(std::string label = "default");
    timer GetTimes(std::string label = "default");
private:
    CCriticalSection cs_timer_map_lock;

    struct internal_timer
    {
        int64_t start_time = 0;
        int64_t checkpoint_time = 0;
    };

    std::unordered_map<std::string, internal_timer> timer_map;
};

extern MilliTimer g_timer;

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

#endif // BITCOIN_UTIL_TIME_H
