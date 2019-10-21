// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include "attributes.h"

#include "uint256.h"
#include "fs.h"
#include "fwd.h"
#include "hash.h"

#include <memory>
#include <utility>
#include <map>
#include <vector>
#include <string>
#include <ostream>
#include <locale>
#include <strings.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

#include <compat.h>

// After merging some more of Bitcoin's utilities, we can split them out
// of this file to reduce the header load:
#include "tinyformat.h"
#include <util/strencodings.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

// to obtain PRId64 on some old systems
#define __STDC_FORMAT_MACROS 1

#include <stdint.h>
#include <inttypes.h>

static const int64_t COIN = 100000000;
static const int64_t COIN_PLACES = 8;
static const int64_t CENT = 1000000;

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

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2)    std::pair<t1, t2>

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

void SetupEnvironment();

//! Substitute for C++14 std::make_unique.
template <typename T, typename... Args>
std::unique_ptr<T> MakeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

void MilliSleep(int64_t n);

extern int GetDayOfYear(int64_t timestamp);

/**
 * Allows search of mapArgs and mapMultiArgs in a case insensitive way
 */

struct mapArgscomp
{
   bool operator() (const std::string& lhs, const std::string& rhs) const
   {
       return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
   }
};

typedef std::map<std::string, std::string, mapArgscomp> ArgsMap;
typedef std::map<std::string, std::vector<std::string>, mapArgscomp> ArgsMultiMap;

extern ArgsMap mapArgs;
extern ArgsMultiMap mapMultiArgs;

extern bool fDebug;
extern bool fDebugNet;
extern bool fDebug2;
extern bool fDebug3;
extern bool fDebug4;
extern bool fDebug10;

extern bool fPrintToConsole;
extern bool fPrintToDebugger;
extern bool fRequestShutdown;
extern bool fShutdown;
extern bool fDaemon;
extern bool fServer;
extern bool fCommandLine;
extern std::string strMiscWarning;
extern bool fTestNet;
extern bool fNoListen;
extern bool fLogTimestamps;
extern bool fReopenDebugLog;
extern bool fDevbuildCripple;

void RandAddSeed();
void RandAddSeedPerfmon();

/* Return true if log accepts specified category */
bool LogAcceptCategory(const char* category);
/* Send a string to the log output */
void LogPrintStr(const std::string &str);

#define LogPrintf(...) do { LogPrint(NULL, __VA_ARGS__); } while (0)

/* When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define MAKE_ERROR_AND_LOG_FUNC(n)                                        \
    /*   Print to debug.log if -debug=category switch is given OR category is NULL. */ \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline void LogPrint(const char* category, const char* format, TINYFORMAT_VARARGS(n))  \
    {                                                                         \
        if(!LogAcceptCategory(category)) return;                            \
        LogPrintStr(tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n");      \
        return;                                                               \
    }                                                                         \
    /*   Log error and return false */                                        \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline bool error(const char* format, TINYFORMAT_VARARGS(n))                     \
    {                                                                         \
        LogPrintStr("ERROR: " + tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n"); \
        return false;                                                         \
    }

TINYFORMAT_FOREACH_ARGNUM(MAKE_ERROR_AND_LOG_FUNC)

/* Zero-arg versions of logging and error, these are not covered by
 * TINYFORMAT_FOREACH_ARGNUM
*/
static inline void LogPrint(const char* category, const char* format)
{
    if(!LogAcceptCategory(category)) return;
    LogPrintStr(format + std::string("\n"));
    return;
}
static inline bool error(const char* format)
{
    LogPrintStr(std::string("ERROR: ") + format + std::string("\n"));
    return false;
}

void LogException(std::exception* pex, const char* pszThread);
void PrintException(std::exception* pex, const char* pszThread);
void PrintExceptionContinue(std::exception* pex, const char* pszThread);
void ParseString(const std::string& str, char c, std::vector<std::string>& v);
std::string FormatMoney(int64_t n, bool fPlus=false);
bool ParseMoney(const std::string& str, int64_t& nRet);
bool ParseMoney(const char* pszIn, int64_t& nRet);
void ParseParameters(int argc, const char*const argv[]);
bool WildcardMatch(const char* psz, const char* mask);
bool WildcardMatch(const std::string& str, const std::string& mask);
bool FileCommit(FILE *fileout);

std::string TimestampToHRDate(double dtm);

bool RenameOver(fs::path src, fs::path dest);
fs::path GetDefaultDataDir();
fs::path GetProgramDir();

const fs::path &GetDataDir(bool fNetSpecific = true);
fs::path GetConfigFile();
fs::path GetPidFile();
#ifndef WIN32
void CreatePidFile(const fs::path &path, pid_t pid);
#endif
void ReadConfigFile(ArgsMap& mapSettingsRet, ArgsMultiMap& mapMultiSettingsRet);
#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void ShrinkDebugFile();
bool DirIsWritable(const fs::path& directory);
bool LockDirectory(const fs::path& directory, const std::string lockfile_name, bool probe_only=false);

//!
//! \brief Read the contents of the specified file into memory.
//!
//! \param filepath The path to the file. Provide absolute paths when possible.
//!
//! \return The file contents as a string.
//!
std::string GetFileContents(std::string filepath);

int GetRandInt(int nMax);
uint64_t GetRand(uint64_t nMax);
uint256 GetRandHash();
int64_t GetTime();
int64_t GetTimeMicros();
int64_t GetSystemTimeInSeconds();
void SetMockTime(int64_t nMockTimeIn);
int64_t GetAdjustedTime();
int64_t GetTimeOffset();
bool IsLockTimeWithin14days(int64_t locktime, int64_t reference);
bool IsLockTimeWithinMinutes(int64_t locktime, int minutes, int64_t reference);
std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);
void AddTimeData(const CNetAddr& ip, int64_t nOffsetSample);
void runCommand(std::string strCommand);

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

//!
//! \brief Convert any value to a string.
//! \param val Value to convert.
//! \note This ignores locale settings.
//!
template<typename T>
std::string ToString(const T& val)
{
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::fixed << val;
    return ss.str();
}

bool Contains(const std::string& data, const std::string& instring);
std::vector<std::string> split(const std::string& s, const std::string& delim);

std::string MakeSafeMessage(const std::string& messagestring);

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

inline int64_t GetPerformanceCounter()
{
    int64_t nCounter = 0;
#ifdef WIN32
    QueryPerformanceCounter((LARGE_INTEGER*)&nCounter);
#else
    timeval t;
    gettimeofday(&t, NULL);
    nCounter = (int64_t) t.tv_sec * 1000000 + t.tv_usec;
#endif
    return nCounter;
}

inline int64_t GetTimeMillis()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
}

inline int64_t GetTimeMicros()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

inline std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    time_t n = nTime;
    struct tm* ptmTime = gmtime(&n);
    char pszTime[200];
    strftime(pszTime, sizeof(pszTime), pszFormat, ptmTime);
    return pszTime;
}

static const std::string strTimestampFormat = "%Y-%m-%d %H:%M:%S UTC";
inline std::string DateTimeStrFormat(int64_t nTime)
{
    return DateTimeStrFormat(strTimestampFormat.c_str(), nTime);
}

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}
/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "foo"). Will be prefixed with "-".
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArgument(const std::string& strArg, const std::string& strDefault);

/**
 * Set argument value.
 *
 * @param argKey Argument to set (e.g. "foo"). Will be prefixed with "-".
 * @param argValue New argument value (e.g. "1")
 */
void SetArgument(const std::string &argKey, const std::string &argValue);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault=false);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);

// Forces an arg setting. Called by SoftSetArg() if the arg hasn't already
// been set. Also called directly in testing.
void ForceSetArg(const std::string& strArg, const std::string& strValue);

/**
 * MWC RNG of George Marsaglia
 * This is intended to be fast. It has a period of 2^59.3, though the
 * least significant 16 bits only have a period of about 2^30.1.
 *
 * @return random value
 */
extern uint32_t insecure_rand_Rz;
extern uint32_t insecure_rand_Rw;
static inline uint32_t insecure_rand(void)
{
  insecure_rand_Rz=36969*(insecure_rand_Rz&65535)+(insecure_rand_Rz>>16);
  insecure_rand_Rw=18000*(insecure_rand_Rw&65535)+(insecure_rand_Rw>>16);
  return (insecure_rand_Rw<<16)+insecure_rand_Rz;
}

/**
 * Seed insecure_rand using the random pool.
 * @param Deterministic Use a determinstic seed
 */
void seed_insecure_rand(bool fDeterministic=false);

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
    std::map<std::string,boost::thread*> threadMap;
};


/**
 * .. A wrapper that just calls func once
 */
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    RenameThread(name);
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

namespace util {
#ifdef WIN32
class WinCmdLineArgs
{
public:
    WinCmdLineArgs();
    ~WinCmdLineArgs();
    std::pair<int, char**> get();

private:
    int argc;
    char** argv;
    std::vector<std::string> args;
};
#endif
} // namespace util

#endif
