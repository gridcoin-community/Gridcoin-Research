// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include "arith_uint256.h"
#include "uint256.h"
#include "fwd.h"
#include "hash.h"

#include <memory>
#include <numeric>
#include <utility>
#include <map>
#include <vector>
#include <string>
#include <locale>
#include <strings.h>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

// After merging some more of Bitcoin's utilities, we can split them out
// of this file to reduce the header load:
#include <util/strencodings.h>
#include "util/system.h"
#include "util/string.h"
#include "util/fraction.h"
#include <util/threadnames.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

// to obtain PRId64 on some old systems
#define __STDC_FORMAT_MACROS 1

#include <stdint.h>
#include <inttypes.h>

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define UBEGIN(a)           ((unsigned char*)&(a))
#define UEND(a)             ((unsigned char*)&((&(a))[1]))

#define UVOIDBEGIN(a)        ((void*)&(a))
#define CVOIDBEGIN(a)        ((const void*)&(a))
#define UINTBEGIN(a)        ((uint32_t*)&(a))
#define CUINTBEGIN(a)        ((const uint32_t*)&(a))

/* Format characters for (s)size_t and ptrdiff_t (C99 standard) */
#define PRIszx    "zx"
#define PRIszu    "zu"
#define PRIszd    "zd"
#define PRIpdx    "tx"
#define PRIpdu    "tu"
#define PRIpdd    "td"


#ifdef WIN32
#define MSG_NOSIGNAL        0
#define MSG_DONTWAIT        0

#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif
#else
#define MAX_PATH            1024
#endif

//void MilliSleep(int64_t n);

extern int GetDayOfYear(int64_t timestamp);

//! Prefix mixed into signed user messages so a message signature cannot be
//! replayed as a transaction signature. Defined in util.cpp (moved from
//! main.cpp, issue #3125 C9).
extern const std::string strMessageMagic;

void LogException(std::exception* pex, const char* pszThread);
void PrintException(std::exception* pex, const char* pszThread);
void PrintExceptionContinue(std::exception* pex, const char* pszThread);
std::string FormatMoney(int64_t n, bool fPlus=false);

//! Convert satoshis to a human coin amount (moved from main.cpp, issue
//! #3125 C9).
double CoinToDouble(double surrogate);
bool ParseMoney(const std::string& str, int64_t& nRet);
bool ParseMoney(const char* pszIn, int64_t& nRet);
bool WildcardMatch(const char* psz, const char* mask);
bool WildcardMatch(const std::string& str, const std::string& mask);
bool TryCreateDirectories(const fs::path& p);

std::string TimestampToHRDate(double dtm);

bool DirIsWritable(const fs::path& directory);
bool LockDirectory(const fs::path& directory, const std::string lockfile_name, bool probe_only=false);
bool TryCreateDirectories(const fs::path& p);

//!
//! \brief Read the contents of the specified file into memory.
//!
//! \param filepath The path to the file. Provide absolute paths when possible.
//!
//! \return The file contents as a string.
//!
std::string GetFileContents(const fs::path filepath);

int64_t GetTimeOffset();
int64_t GetAdjustedTime();
//! Record an outbound peer's VERSION time offset as its time vote (see TimeOffsetVotes).
void AddTimeData(int64_t node_id, const CNetAddr& ip, int64_t nOffsetSample);
//! Withdraw a peer's time vote. Call only once no other thread can still reach the peer.
void RemoveTimeData(int64_t node_id);
//! Tests only: forget every time vote and reset the offset to 0.
void ResetTimeDataForTesting();
//! Whether the live time votes form a quorum, so that the offset follows the peer median.
bool HasTimeQuorum();
//! The peer time median the live votes give, in seconds, or 0 without a quorum.
int64_t GetPeerTimeMedian();
//! The clock warning, or an empty string: a staking node without a time quorum (it uses its own clock), or a peer
//! time median more than 24 s from this computer's clock.
std::string GetClockWarning(bool staking);
//! Tell the clock warning whether the stake miner is trying to stake. The miner calls it every round. Each change of
//! the clock warning is logged once, whether it follows a miner round or a change to the time votes.
void SetClockWarningStaking(bool staking);
//! The clock warning for the staking state the stake miner last reported.
std::string GetCurrentClockWarning();
//! Count a received block's lead over the adjusted time, in seconds, for the getclockstats diagnostics.
void RecordBlockLead(int64_t lead);
//! Cumulative counts of received blocks, and of those more than 16, 32, 48 and 128 s ahead of the adjusted time.
struct BlockLeadCounts {
    uint64_t seen;
    uint64_t over16;
    uint64_t over32;
    uint64_t over48;
    uint64_t over128;
};
BlockLeadCounts GetBlockLeadCounts();
#if HAVE_SYSTEM
void runCommand(std::string strCommand);
#endif

//!
//! \brief Round double value to N decimal places.
//! \param d Value to round.
//! \param place Number of decimal places.
//!
double Round(double d, int place);

//!
//! \brief Round a double value and convert it to a string.
//! \param d Value to round.
//! \param place Number of decimal places.
//! \note This always produces an output with dot as decimal separator.
//!
std::string RoundToString(double d, int place);

//!
//! \brief Round a double value contained in a string.
//!
//! Does \c atof on \p s and rounds the result.
//!
//! \returns \p s represented as a double rounded to \p place decimals.
//!
double RoundFromString(const std::string& s, int place);

bool Contains(const std::string& data, const std::string& instring);
std::vector<std::string> split(const std::string& s, const std::string& delim);

inline int roundint(double d)
{
    return (int)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t abs64(int64_t n)
{
    return (n >= 0 ? n : -n);
}

inline std::string leftTrim(std::string src, char chr)
{
    std::string::size_type pos = src.find_first_not_of(chr, 0);

    if(pos > 0)
        src.erase(0, pos);

    return src;
}

/** Median filter over a stream of values.
 * Returns the median of the last N numbers
 */
template <typename T> class CMedianFilter
{
private:
    std::vector<T> vValues;
    std::vector<T> vSorted;
    unsigned int nSize;
public:
    CMedianFilter(unsigned int size, T initial_value):
        nSize(size)
    {
        vValues.reserve(size);
        vValues.push_back(initial_value);
        vSorted = vValues;
    }

    void input(T value)
    {
        if(vValues.size() == nSize)
        {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const
    {
        int size = vSorted.size();
        assert(size>0);
        if(size & 1) // Odd number of elements
        {
            return vSorted[size/2];
        }
        else // Even number of elements
        {
            return (vSorted[size/2-1] + vSorted[size/2]) / 2;
        }
    }

    int size() const
    {
        return vValues.size();
    }

    std::vector<T> sorted () const
    {
        return vSorted;
    }
};

bool NewThread(void(*pfn)(void*), void* parg);
void RenameThread(const char* name);

class ThreadHandler
{
public:
    ThreadHandler(){};
    bool createThread(void(*pfn)(ThreadHandlerPtr), ThreadHandlerPtr parg, const std::string tname);
    bool createThread(void(*pfn)(void*), void* parg, const std::string tname);
    int numThreads();
    bool threadExists(const std::string tname);
    void interruptAll();
    void removeAll();
    void removeByName(const std::string tname);
private:
    boost::thread_group threadGroup;
    std::map<std::string, boost::thread*> threadMap;
};


/**
 * .. A wrapper that just calls func once
 */
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    RenameThread(name);
    // RenameThread only sets the OS thread name; also set the internal name so
    // the log-line thread tag ([<name>]) is populated instead of "[unknown]".
    util::ThreadSetInternalName(name);
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("%s thread interrupt\n", name);
        throw;
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...) {
        PrintExceptionContinue(nullptr, name);
        throw;
    }
}

#endif
