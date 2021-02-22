// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LOGGING_H
#define BITCOIN_LOGGING_H

#include <fs.h>
#include <tinyformat.h>

#include <atomic>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>

static const bool DEFAULT_LOGTIMEMICROS = false;
static const bool DEFAULT_LOGIPS        = false;
static const bool DEFAULT_LOGTIMESTAMPS = true;
static const bool DEFAULT_LOGTHREADNAMES = false;
extern const char* const DEFAULT_DEBUGLOGFILE;

extern bool fLogIPs;

// Unavoidable because this is in util.h.
extern int64_t GetAdjustedTime();

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

struct CLogCategoryActive
{
    std::string category;
    bool active;
};

namespace BCLog {
    enum LogFlags : uint32_t {
        NONE        = 0,
        NET         = (1 <<  0),
        TOR         = (1 <<  1),
        MEMPOOL     = (1 <<  2),
        HTTP        = (1 <<  3),
        BENCH       = (1 <<  4),
        ZMQ         = (1 <<  5),
        WALLETDB    = (1 <<  6),
        RPC         = (1 <<  7),
        ESTIMATEFEE = (1 <<  8),
        ADDRMAN     = (1 <<  9),
        SELECTCOINS = (1 << 10),
        REINDEX     = (1 << 11),
        CMPCTBLOCK  = (1 << 12),
        RAND        = (1 << 13),
        MISC        = (1 << 14),
        PROXY       = (1 << 15),
        MEMPOOLREJ  = (1 << 16),
        LIBEVENT    = (1 << 17),
        COINDB      = (1 << 18),
        QT          = (1 << 19),
        LEVELDB     = (1 << 20),
        SCRAPER     = (1 << 21),
        MANIFEST    = (1 << 22),
        SB          = (1 << 23),
        BEACON      = (1 << 24),
        TALLY       = (1 << 25),
        ACCRUAL     = (1 << 26),
        CONTRACT    = (1 << 27),
        MINER       = (1 << 28),
        VOTE        = (1 << 29),
        VERBOSE     = (1 << 30), // This corresponds to the old horizontal category "debug".
        NOISY       = (1u << 31), // This corresponds to the old horizontal category "debug10".
        ALL         = ~(uint32_t)0,
    };

    class Logger
    {
    private:
        mutable std::mutex m_cs;                   // Can not use Mutex from sync.h because in debug mode it would cause a deadlock when a potential deadlock was detected
        FILE* m_fileout = nullptr;                 // GUARDED_BY(m_cs)
        std::list<std::string> m_msgs_before_open; // GUARDED_BY(m_cs)
        bool m_buffering{true};                    //!< Buffer messages before logging can be started. GUARDED_BY(m_cs)

        /**
         * m_started_new_line is a state variable that will suppress printing of
         * the timestamp when multiple calls are made that don't end in a
         * newline.
         */
        std::atomic_bool m_started_new_line{true};

        /** Log categories bitfield. */
        std::atomic<uint32_t> m_categories{0};

        std::string LogTimestampStr(const std::string& str);

        /** Slots that connect to the print signal */
        std::list<std::function<void(const std::string&)>> m_print_callbacks /* GUARDED_BY(m_cs) */ {};

    public:
        bool m_print_to_console = false;
        bool m_print_to_file = false;

        bool m_log_timestamps = DEFAULT_LOGTIMESTAMPS;
        bool m_log_time_micros = DEFAULT_LOGTIMEMICROS;
        bool m_log_threadnames = DEFAULT_LOGTHREADNAMES;

        fs::path m_file_path;
        std::atomic<bool> m_reopen_file{false};
        static boost::gregorian::date PrevArchiveCheckDate;

        /** Send a string to the log output */
        void LogPrintStr(const std::string& str);

        /** Returns whether logs will be written to any output */
        bool Enabled() const
        {
            std::lock_guard<std::mutex> scoped_lock(m_cs);
            return m_buffering || m_print_to_console || m_print_to_file || !m_print_callbacks.empty();
        }

        /** Connect a slot to the print signal and return the connection */
        std::list<std::function<void(const std::string&)>>::iterator PushBackCallback(std::function<void(const std::string&)> fun)
        {
            std::lock_guard<std::mutex> scoped_lock(m_cs);
            m_print_callbacks.push_back(std::move(fun));
            return --m_print_callbacks.end();
        }

        /** Delete a connection */
        void DeleteCallback(std::list<std::function<void(const std::string&)>>::iterator it)
        {
            std::lock_guard<std::mutex> scoped_lock(m_cs);
            m_print_callbacks.erase(it);
        }

        /** Start logging (and flush all buffered messages) */
        bool StartLogging();
        /** Only for testing */
        void DisconnectTestLogger();

        void ShrinkDebugFile();

        bool archive(bool fImmediate, fs::path pfile_out);

        uint32_t GetCategoryMask() const { return m_categories.load(); }

        void EnableCategory(LogFlags flag);
        bool EnableCategory(const std::string& str);
        void DisableCategory(LogFlags flag);
        bool DisableCategory(const std::string& str);

        bool WillLogCategory(LogFlags category) const;

        bool DefaultShrinkDebugFile() const;
    };

} // namespace BCLog

BCLog::Logger& LogInstance();

/** Return true if log accepts specified category */
static inline bool LogAcceptCategory(BCLog::LogFlags category)
{
    return LogInstance().WillLogCategory(category);
}

/** Return true if str parses as a log category and set the flag */
bool GetLogCategory(BCLog::LogFlags& flag, const std::string& str);

static inline bool LogAcceptCategory(const char* category)
{
    BCLog::LogFlags flag;
    std::string str = category;

    // Return without logging if string doesn't match enumerated entry.
    if(!GetLogCategory(flag, str)) return false;

    return LogInstance().WillLogCategory(flag);
}

/** Returns a string with the log categories. */
std::string ListLogCategories();

/** Returns a vector of the active log categories. */
std::vector<CLogCategoryActive> ListActiveLogCategories();

// Be conservative when using LogPrintf/error or other things which
// unconditionally log to debug.log! It should not be the case that an inbound
// peer can fill up a user's disk with debug.log entries.

template <typename... Args>
static inline void LogPrintf(const char* fmt, const Args&... args)
{
    if (LogInstance().Enabled()) {
        std::string log_msg;
        try {
            log_msg = tfm::format(fmt, args...);
        } catch (tinyformat::format_error& fmterr) {
            /* Original format string will have newline so don't add one here */
            log_msg = "Error \"" + std::string(fmterr.what()) + "\" while formatting log message: " + fmt;
        }
        // The \n here is for legacy formatting of Gridcoin LogPrintf's, which usually don't have \n's on them.
        LogInstance().LogPrintStr(log_msg + "\n");
    }
}

/* When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define MAKE_ERROR_AND_LOG_FUNC(n)                                        \
    /*   Print to debug.log if -debug=category switch is given OR category is NULL. */ \
    template<typename Category, TINYFORMAT_ARGTYPES(n)>                                          \
    static inline void LogPrint(Category category, const char* format, TINYFORMAT_VARARGS(n))  \
    {                                                                         \
        if(!LogAcceptCategory(category)) return;                            \
        LogInstance().LogPrintStr(tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n");      \
        return;                                                               \
    }                                                                         \
    /*   Log error and return false */                                        \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline bool error(const char* format, TINYFORMAT_VARARGS(n))                     \
    {                                                                         \
        LogInstance().LogPrintStr("ERROR: " + tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n"); \
        return false;                                                         \
    }

TINYFORMAT_FOREACH_ARGNUM(MAKE_ERROR_AND_LOG_FUNC)

/* Zero-arg versions of logging and error, these are not covered by
 * TINYFORMAT_FOREACH_ARGNUM
*/
template <typename Category>
static inline void LogPrint(Category category, const char* format)
{
    if(!LogAcceptCategory(category)) return;
    LogInstance().LogPrintStr(format + std::string("\n"));
    return;
}

static inline bool error(const char* format)
{
    LogInstance().LogPrintStr(std::string("ERROR: ") + format + std::string("\n"));
    return false;
}

// Use a macro instead of a function for conditional logging to prevent
// evaluating arguments when logging for the category is not enabled.
#define LogPrint(category, ...)              \
    do {                                     \
        if (LogAcceptCategory((category))) { \
            LogPrintf(__VA_ARGS__);          \
        }                                    \
    } while (0)

#endif // BITCOIN_LOGGING_H
