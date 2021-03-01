// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <logging.h>
#include <util/threadnames.h>
#include "util/time.h"

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <mutex>
#include <set>

// externs unavoidable because these are in util.h.
extern fs::path &GetDataDir(bool fNetSpecific = true);
extern bool GetBoolArg(const std::string& strArg, bool fDefault);
extern int64_t GetArg(const std::string& strArg, int64_t nDefault);

const char * const DEFAULT_DEBUGLOGFILE = "debug.log";

BCLog::Logger& LogInstance()
{
/**
 * NOTE: the logger instances is leaked on exit. This is ugly, but will be
 * cleaned up by the OS/libc. Defining a logger as a global object doesn't work
 * since the order of destruction of static/global objects is undefined.
 * Consider if the logger gets destroyed, and then some later destructor calls
 * LogPrintf, maybe indirectly, and you get a core dump at shutdown trying to
 * access the logger. When the shutdown sequence is fully audited and tested,
 * explicit destruction of these objects can be implemented by changing this
 * from a raw pointer to a std::unique_ptr.
 * Since the destructor is never called, the logger and all its members must
 * have a trivial destructor.
 *
 * This method of initialization was originally introduced in
 * Bitcoin core commit ee3374234c60aba2cc4c5cd5cac1c0aefc2d817c.
 */
    static BCLog::Logger* g_logger{new BCLog::Logger()};
    return *g_logger;
}

boost::gregorian::date BCLog::Logger::PrevArchiveCheckDate = boost::posix_time::from_time_t(GetAdjustedTime()).date();
bool fLogIPs = DEFAULT_LOGIPS;

static int FileWriteStr(const std::string &str, FILE *fp)
{
    return fwrite(str.data(), 1, str.size(), fp);
}

bool BCLog::Logger::StartLogging()
{
    std::lock_guard<std::mutex> scoped_lock(m_cs);

    assert(m_buffering);
    assert(m_fileout == nullptr);

    if (m_print_to_file) {
        assert(!m_file_path.empty());
        m_fileout = fsbridge::fopen(m_file_path, "a");
        if (!m_fileout) {
            return false;
        }

        setbuf(m_fileout, nullptr); // unbuffered

        // Add newlines to the logfile to distinguish this execution from the
        // last one.
        FileWriteStr("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", m_fileout);
    }

    // dump buffered messages from before we opened the log
    m_buffering = false;
    while (!m_msgs_before_open.empty()) {
        const std::string& s = m_msgs_before_open.front();

        if (m_print_to_file) FileWriteStr(s, m_fileout);
        if (m_print_to_console) fwrite(s.data(), 1, s.size(), stdout);
        for (const auto& cb : m_print_callbacks) {
            cb(s);
        }

        m_msgs_before_open.pop_front();
    }
    if (m_print_to_console) fflush(stdout);

    return true;
}

void BCLog::Logger::DisconnectTestLogger()
{
    std::lock_guard<std::mutex> scoped_lock(m_cs);
    m_buffering = true;
    if (m_fileout != nullptr) fclose(m_fileout);
    m_fileout = nullptr;
    m_print_callbacks.clear();
}

void BCLog::Logger::EnableCategory(BCLog::LogFlags flag)
{
    m_categories |= flag;
}

bool BCLog::Logger::EnableCategory(const std::string& str)
{
    BCLog::LogFlags flag;
    if (!GetLogCategory(flag, str)) {
        if (str == "db") {
            // DEPRECATION: Added in 0.20, should start returning an error in 0.21
            LogPrintf("Warning: logging category 'db' is deprecated, use 'walletdb' instead\n");
            EnableCategory(BCLog::WALLETDB);
            return true;
        }
        return false;
    }
    EnableCategory(flag);
    return true;
}

void BCLog::Logger::DisableCategory(BCLog::LogFlags flag)
{
    m_categories &= ~flag;
}

bool BCLog::Logger::DisableCategory(const std::string& str)
{
    BCLog::LogFlags flag;
    if (!GetLogCategory(flag, str)) return false;
    DisableCategory(flag);
    return true;
}

bool BCLog::Logger::WillLogCategory(BCLog::LogFlags category) const
{
    return (m_categories.load(std::memory_order_relaxed) & category) != 0;
}

bool BCLog::Logger::DefaultShrinkDebugFile() const
{
    return m_categories == BCLog::NONE;
}

struct CLogCategoryDesc
{
    BCLog::LogFlags flag;
    std::string category;
};

const CLogCategoryDesc LogCategories[] =
{
    {BCLog::NONE, "0"},
    {BCLog::NONE, "none"},
    {BCLog::NET, "net"},
    {BCLog::TOR, "tor"},
    {BCLog::MEMPOOL, "mempool"},
    {BCLog::HTTP, "http"},
    {BCLog::BENCH, "bench"},
    {BCLog::ZMQ, "zmq"},
    {BCLog::WALLETDB, "walletdb"},
    {BCLog::RPC, "rpc"},
    {BCLog::ESTIMATEFEE, "estimatefee"},
    {BCLog::ADDRMAN, "addrman"},
    {BCLog::SELECTCOINS, "selectcoins"},
    {BCLog::REINDEX, "reindex"},
    {BCLog::CMPCTBLOCK, "cmpctblock"},
    {BCLog::RAND, "rand"},
    {BCLog::MISC, "misc"},
    {BCLog::PROXY, "proxy"},
    {BCLog::MEMPOOLREJ, "mempoolrej"},
    {BCLog::LIBEVENT, "libevent"},
    {BCLog::COINDB, "coindb"},
    {BCLog::QT, "qt"},
    {BCLog::LEVELDB, "leveldb"},
    {BCLog::SCRAPER, "scraper"},
    {BCLog::MANIFEST, "manifest"},
    {BCLog::SB, "superblock"},
    {BCLog::BEACON, "beacon"},
    {BCLog::TALLY, "tally"},
    {BCLog::ACCRUAL, "accrual"},
    {BCLog::CONTRACT, "contract"},
    {BCLog::MINER, "miner"},
    {BCLog::VOTE, "vote"},
    {BCLog::VERBOSE, "verbose"},
    {BCLog::NOISY, "noisy"},
    {BCLog::ALL, "1"},
    {BCLog::ALL, "all"},
};

bool GetLogCategory(BCLog::LogFlags& flag, const std::string& str)
{
    if (str == "") {
        flag = BCLog::ALL;
        return true;
    }
    for (const CLogCategoryDesc& category_desc : LogCategories) {
        if (category_desc.category == str) {
            flag = category_desc.flag;
            return true;
        }
    }
    return false;
}

std::string ListLogCategories()
{
    std::string ret;
    int outcount = 0;
    for (const CLogCategoryDesc& category_desc : LogCategories) {
        // Omit the special cases.
        if (category_desc.flag != BCLog::NONE && category_desc.flag != BCLog::ALL) {
            if (outcount != 0) ret += ", ";
            ret += category_desc.category;
            outcount++;
        }
    }
    return ret;
}

std::vector<CLogCategoryActive> ListActiveLogCategories()
{
    std::vector<CLogCategoryActive> ret;
    for (const CLogCategoryDesc& category_desc : LogCategories) {
        // Omit the special cases.
        if (category_desc.flag != BCLog::NONE && category_desc.flag != BCLog::ALL) {
            CLogCategoryActive catActive;
            catActive.category = category_desc.category;
            catActive.active = LogAcceptCategory(category_desc.flag);
            ret.push_back(catActive);
        }
    }
    return ret;
}

std::string BCLog::Logger::LogTimestampStr(const std::string& str)
{
    std::string strStamped;

    if (!m_log_timestamps)
        return str;

    if (m_started_new_line) {
        int64_t nTimeMicros = GetTimeMicros();
        strStamped = FormatISO8601DateTime(nTimeMicros/1000000);
        if (m_log_time_micros) {
            strStamped.pop_back();
            strStamped += strprintf(".%06dZ", nTimeMicros%1000000);
        }
        int64_t mocktime = GetMockTime();
        if (mocktime) {
            strStamped += " (mocktime: " + FormatISO8601DateTime(mocktime) + ")";
        }
        strStamped += ' ' + str;
    } else
        strStamped = str;

    return strStamped;
}

namespace BCLog {
    /** Belts and suspenders: make sure outgoing log messages don't contain
     * potentially suspicious characters, such as terminal control codes.
     *
     * This escapes control characters except newline ('\n') in C syntax.
     * It escapes instead of removes them to still allow for troubleshooting
     * issues where they accidentally end up in strings.
     */
    std::string LogEscapeMessage(const std::string& str) {
        std::string ret;
        for (char ch_in : str) {
            uint8_t ch = (uint8_t)ch_in;
            if ((ch >= 32 || ch == '\n') && ch != '\x7f') {
                ret += ch_in;
            } else {
                ret += strprintf("\\x%02x", ch);
            }
        }
        return ret;
    }
}

void BCLog::Logger::LogPrintStr(const std::string& str)
{
    std::lock_guard<std::mutex> scoped_lock(m_cs);
    std::string str_prefixed = LogEscapeMessage(str);

    if (m_log_threadnames && m_started_new_line) {
        str_prefixed.insert(0, "[" + util::ThreadGetInternalName() + "] ");
    }

    str_prefixed = LogTimestampStr(str_prefixed);

    m_started_new_line = !str.empty() && str[str.size()-1] == '\n';

    if (m_buffering) {
        // buffer if we haven't started logging yet
        m_msgs_before_open.push_back(str_prefixed);
        return;
    }

    if (m_print_to_console) {
        // print to console
        fwrite(str_prefixed.data(), 1, str_prefixed.size(), stdout);
        fflush(stdout);
    }
    for (const auto& cb : m_print_callbacks) {
        cb(str_prefixed);
    }
    if (m_print_to_file) {
        assert(m_fileout != nullptr);

        // reopen the log file, if requested
        if (m_reopen_file) {
            m_reopen_file = false;
            FILE* new_fileout = fsbridge::fopen(m_file_path, "a");
            if (new_fileout) {
                setbuf(new_fileout, nullptr); // unbuffered
                fclose(m_fileout);
                m_fileout = new_fileout;
            }
        }
        FileWriteStr(str_prefixed, m_fileout);
    }
}

void BCLog::Logger::ShrinkDebugFile()
{
    // Amount of debug.log to save at end when shrinking (must fit in memory)
    constexpr size_t RECENT_DEBUG_HISTORY_SIZE = 10 * 1000000;

    assert(!m_file_path.empty());

    // Scroll debug.log if it's getting too big
    FILE* file = fsbridge::fopen(m_file_path, "r");

    // Special files (e.g. device nodes) may not have a size.
    size_t log_size = 0;
    try {
        log_size = fs::file_size(m_file_path);
    } catch (const fs::filesystem_error&) {}

    // If debug.log file is more than 10% bigger the RECENT_DEBUG_HISTORY_SIZE
    // trim it down by saving only the last RECENT_DEBUG_HISTORY_SIZE bytes
    if (file && log_size > 11 * (RECENT_DEBUG_HISTORY_SIZE / 10))
    {
        // Restart the file with some of the end
        std::vector<char> vch(RECENT_DEBUG_HISTORY_SIZE, 0);
        if (fseek(file, -((long)vch.size()), SEEK_END)) {
            LogPrintf("Failed to shrink debug log file: fseek(...) failed\n");
            fclose(file);
            return;
        }
        int nBytes = fread(vch.data(), 1, vch.size(), file);
        fclose(file);

        file = fsbridge::fopen(m_file_path, "w");
        if (file)
        {
            fwrite(vch.data(), 1, nBytes, file);
            fclose(file);
        }
    }
    else if (file != nullptr)
        fclose(file);
}

bool BCLog::Logger::archive(bool fImmediate, fs::path pfile_out)
{
    bool fArchiveDaily = GetBoolArg("-logarchivedaily", true);

    int64_t nTime = GetAdjustedTime();
    boost::gregorian::date ArchiveCheckDate = boost::posix_time::from_time_t(nTime).date();
    fs::path plogfile;
    fs::path pfile_temp;
    fs::path pathDataDir = GetDataDir();

    std::stringstream ssArchiveCheckDate, ssPrevArchiveCheckDate;

    ssArchiveCheckDate << ArchiveCheckDate;
    ssPrevArchiveCheckDate << PrevArchiveCheckDate;

    fs::path LogArchiveDir = pathDataDir / "logarchive";

    // Check to see if the log archive directory exists and is a directory. If not create it.
    if (fs::exists(LogArchiveDir))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the log archive directory.
        if (fs::is_regular_file(LogArchiveDir))
        {
            fs::remove(LogArchiveDir);
            fs::create_directory(LogArchiveDir);
        }
    }
    else
    {
        fs::create_directory(LogArchiveDir);
    }

    if (fImmediate || (fArchiveDaily && ArchiveCheckDate > PrevArchiveCheckDate))
    {
        std::string rename_error_msg;

        {
            std::lock_guard<std::mutex> scoped_lock(m_cs);

            fclose(m_fileout);

            plogfile = m_file_path;

            pfile_temp = LogArchiveDir / static_cast<fs::path>(m_file_path.stem().string() + "-" + DateTimeStrFormat("%Y%m%d%H%M%S", nTime) + m_file_path.extension().string());

            pfile_out = LogArchiveDir / static_cast<fs::path>((m_file_path.filename().stem().string() + "-" + DateTimeStrFormat("%Y%m%d%H%M%S", nTime)
                                         + m_file_path.filename().extension().string() + ".gz"));

            try
            {
                fs::rename(plogfile, pfile_temp);
            }
            catch(const std::exception& e)
            {
                rename_error_msg = "Failed to rename logging file: ";
                rename_error_msg += e.what();
            }

            // Re-open logging file. (This is subtly different than the flag based reopen above, because the file must be closed first, renamed for compression,
            // and then a new one opened.
            FILE* new_fileout = fsbridge::fopen(m_file_path, "a");
            if (new_fileout)
            {
                setbuf(new_fileout, nullptr); // unbuffered
                m_fileout = new_fileout;
            }

            PrevArchiveCheckDate = ArchiveCheckDate;
        }

        if (!rename_error_msg.empty())
        {
            LogPrintf("ERROR: Logger: archive: %s", rename_error_msg);
            return false;
        }

        fsbridge::ifstream infile(pfile_temp, std::ios_base::in | std::ios_base::binary);

        if (!infile)
        {
            LogPrintf("ERROR: Logger: Failed to open archive log file for compression %s.", pfile_temp.string());
            return false;
        }

        fsbridge::ofstream outgzfile(pfile_out, std::ios_base::out | std::ios_base::binary);

        if (!outgzfile)
        {
            LogPrintf("ERROR: Logger: Failed to open archive gzip file %s.", pfile_out.string());
            return false;
        }

        boost::iostreams::filtering_ostream out;
        out.push(boost::iostreams::gzip_compressor());
        out.push(outgzfile);

        boost::iostreams::copy(infile, out);

        infile.close();
        outgzfile.flush();
        outgzfile.close();

        fs::remove(pfile_temp);

        bool fDeleteOldLogArchives = GetBoolArg("-deleteoldlogarchives", true);

        if (fDeleteOldLogArchives)
        {
            unsigned int nRetention = (unsigned int)GetArg("-logarchiveretainnumfiles", 30);
            LogPrintf ("INFO: Logger: nRetention %i.", nRetention);

            std::set<fs::directory_entry, std::greater <fs::directory_entry>> SortedDirEntries;

            // Iterate through the log archive directory and delete the oldest files beyond the retention rule
            // The names are in format <logname base>-YYYYMMDDHHMMSS.log for the logs, so filter by containing <logname base>.
            // The greater than sort in the set should then return descending order by datetime.
            for (fs::directory_entry& DirEntry : fs::directory_iterator(LogArchiveDir))
            {
                std::string sFilename = DirEntry.path().filename().string();
                size_t FoundPos = sFilename.find(m_file_path.filename().stem().string());

                if (FoundPos != std::string::npos) SortedDirEntries.insert(DirEntry);
            }

            // Now iterate through set of filtered filenames. Delete all files greater than retention count.
            unsigned int i = 0;
            for (auto const& iter : SortedDirEntries)
            {
                if (i >= nRetention)
                {
                    fs::remove(iter.path());

                    LogPrintf("INFO: Logger: Removed old archive gzip file %s.", iter.path().filename().string());
                }

                ++i;
            }
        }

        return true; // archive condition was satisfied. Return true after rotating and archiving.
    }
    else
    {
        return false; // archive condition was not satisfied. Do nothing and return false.
    }
}
