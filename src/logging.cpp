// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <logging.h>
#include <util/threadnames.h>
#include "util/time.h"
#include "util/system.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <istream>
#include <mutex>
#include <ostream>
#include <set>
#include <streambuf>

#ifndef WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

extern ArgsManager gArgs;

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
        m_fileout = OpenOwnFile("a");
        if (!m_fileout) {
            if (!m_secure_open) {
                return false;
            }

            // The secure open refused the file. Carry on without a log file, so the lines buffered so far are
            // delivered and later ones are not buffered without bound; the caller reports the failure.
            m_print_to_file = false;
            m_file_open_failed = true;
        } else {
            setbuf(m_fileout, nullptr); // unbuffered

            // Add newlines to the logfile to distinguish this execution from the
            // last one.
            FileWriteStr("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", m_fileout);
        }
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

    return !m_file_open_failed;
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
    if (!GetLogCategory(flag, str)) return false;
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
    {BCLog::IPC, "ipc"},
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
        std::chrono::seconds mocktime = GetMockTime();
        if (mocktime > 0s) {
            strStamped += " (mocktime: " + FormatISO8601DateTime(count_seconds(mocktime)) + ")";
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
    std::string LogEscapeMessage(const std::string& str, bool escape_newlines) {
        std::string ret;
        for (size_t i = 0; i < str.size(); ++i) {
            const char ch_in = str[i];
            uint8_t ch = (uint8_t)ch_in;
            // With escape_newlines only a final newline is kept as a newline.
            const bool keep_newline = ch == '\n' && (!escape_newlines || i + 1 == str.size());
            if ((ch >= 32 || keep_newline) && ch != '\x7f') {
                ret += ch_in;
            } else {
                ret += strprintf("\\x%02x", ch);
            }
        }
        return ret;
    }

    FILE* OpenLogFile(const fs::path& path, const char* mode, bool secure)
    {
        if (!secure) return fsbridge::fopen(path, mode);

#ifdef WIN32
        boost::system::error_code ec;
        const fs::file_status status = fs::symlink_status(path, ec);
        if (status.type() == fs::symlink_file || status.type() == fs::reparse_file) return nullptr;
        return fsbridge::fopen(path, mode);
#else
        return OpenLogFileAt(AT_FDCWD, path.c_str(), mode);
#endif
    }

#ifndef WIN32
    FILE* OpenLogFileAt(int dirfd, const char* name, const char* mode)
    {
        int flags = O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
        if (std::strcmp(mode, "r") == 0) {
            flags |= O_RDONLY;
        } else if (std::strcmp(mode, "a") == 0) {
            flags |= O_WRONLY | O_CREAT | O_APPEND;
        } else if (std::strcmp(mode, "w") == 0) {
            // Truncation waits until the file has passed the checks below.
            flags |= O_WRONLY | O_CREAT;
        } else {
            return nullptr;
        }

        const int fd = ::openat(dirfd, name, flags, static_cast<mode_t>(0600));
        if (fd < 0) return nullptr;

        struct stat st;
        if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != ::geteuid() || st.st_nlink != 1) {
            ::close(fd);
            return nullptr;
        }

        const int fl = ::fcntl(fd, F_GETFL);
        if (fl < 0 || ::fcntl(fd, F_SETFL, fl & ~O_NONBLOCK) != 0) {
            ::close(fd);
            return nullptr;
        }

        if (std::strcmp(mode, "w") == 0 && ::ftruncate(fd, 0) != 0) {
            ::close(fd);
            return nullptr;
        }

        FILE* file = ::fdopen(fd, mode);
        if (file == nullptr) ::close(fd);
        return file;
    }

    int OpenDirectoryUnder(const fs::path& root, const fs::path& dir)
    {
        // The components of dir below root, or none of root is a prefix.
        std::vector<std::string> below;
        bool under_root = !root.empty();

        if (under_root) {
            auto significant = [](const fs::path& p) {
                std::vector<std::string> parts;
                for (const fs::path& part : p) {
                    const std::string name = part.string();
                    if (!name.empty() && name != ".") parts.push_back(name);
                }
                return parts;
            };

            const std::vector<std::string> root_parts = significant(root);
            const std::vector<std::string> dir_parts = significant(dir);

            if (dir_parts.size() < root_parts.size()
                || !std::equal(root_parts.begin(), root_parts.end(), dir_parts.begin())) {
                under_root = false;
            } else {
                below.assign(dir_parts.begin() + root_parts.size(), dir_parts.end());
            }
        }

        if (!under_root) {
            return ::open(dir.c_str(), O_DIRECTORY | O_CLOEXEC);
        }

        int cur = ::open(root.c_str(), O_DIRECTORY | O_CLOEXEC);
        if (cur < 0) return -1;

        for (const std::string& name : below) {
            if (name == "..") {
                ::close(cur);
                return -1;
            }

            const int next = ::openat(cur, name.c_str(), O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            ::close(cur);
            if (next < 0) return -1;

            struct stat st;
            if (::fstat(next, &st) != 0 || st.st_uid != ::geteuid()) {
                ::close(next);
                return -1;
            }

            cur = next;
        }

        return cur;
    }
#endif

    FILE* OpenLogFileUnder(const fs::path& root, const fs::path& path, const char* mode)
    {
#ifdef WIN32
        return OpenLogFile(path, mode, true);
#else
        if (root.empty()) return OpenLogFile(path, mode, true);

        const int dirfd = OpenDirectoryUnder(root, path.parent_path());
        if (dirfd < 0) return nullptr;

        FILE* file = OpenLogFileAt(dirfd, path.filename().c_str(), mode);
        ::close(dirfd);
        return file;
#endif
    }
}

FILE* BCLog::Logger::OpenOwnFile(const char* mode) const
{
    if (m_secure_open && !m_secure_root.empty()) {
        return BCLog::OpenLogFileUnder(m_secure_root, m_file_path, mode);
    }

    return BCLog::OpenLogFile(m_file_path, mode, m_secure_open);
}

void BCLog::Logger::LogPrintStr(const std::string& str)
{
    std::lock_guard<std::mutex> scoped_lock(m_cs);
    std::string str_prefixed = LogEscapeMessage(str, m_escape_newlines);

    if (m_log_threadnames && m_started_new_line) {
        const auto threadname = util::ThreadGetInternalName();
        str_prefixed.insert(0, "[" + (threadname.empty() ? "unknown" : threadname) + "] ");
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
        // reopen the log file, if requested
        if (m_reopen_file) {
            m_reopen_file = false;
            FILE* new_fileout = OpenOwnFile("a");
            if (new_fileout) {
                setbuf(new_fileout, nullptr); // unbuffered
                if (m_fileout != nullptr) fclose(m_fileout);
                m_fileout = new_fileout;
            }
        }

        // The file is not open after archive() failed to reopen it. A reopen
        // request (SIGHUP, or the next archive() call) opens it again.
        if (m_fileout != nullptr) FileWriteStr(str_prefixed, m_fileout);
    }
}

void BCLog::Logger::ShrinkDebugFile()
{
    // Amount of debug.log to save at end when shrinking (must fit in memory)
    constexpr size_t RECENT_DEBUG_HISTORY_SIZE = 10 * 1000000;

    assert(!m_file_path.empty());

    // Scroll debug.log if it's getting too big
    FILE* file = OpenOwnFile("r");

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

        file = OpenOwnFile("w");
        if (file)
        {
            fwrite(vch.data(), 1, nBytes, file);
            fclose(file);
        }
    }
    else if (file != nullptr)
        fclose(file);
}

#ifndef WIN32
namespace {
//! Closes a directory descriptor at scope exit.
struct DirFd
{
    int fd{-1};
    DirFd() = default;
    explicit DirFd(int fd_in) : fd(fd_in) {}
    DirFd(const DirFd&) = delete;
    DirFd& operator=(const DirFd&) = delete;
    ~DirFd() { if (fd >= 0) ::close(fd); }
};

//! An unbuffered output stream buffer that writes to a file descriptor it does not own.
class FdStreamBuf : public std::streambuf
{
public:
    explicit FdStreamBuf(int fd) : m_fd(fd) {}

protected:
    std::streamsize xsputn(const char* data, std::streamsize count) override
    {
        std::streamsize done = 0;
        while (done < count) {
            const ssize_t n = ::write(m_fd, data + done, static_cast<size_t>(count - done));
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            done += n;
        }
        return done;
    }

    int_type overflow(int_type ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
        const char c = traits_type::to_char_type(ch);
        return xsputn(&c, 1) == 1 ? ch : traits_type::eof();
    }

private:
    int m_fd;
};

//! Compresses temp_name into out_name, both entries of the directory archive_fd. The input is opened with the
//! secure open and the output is created exclusively without following a link, so nothing that already exists
//! under the output name is overwritten or followed. The input is removed afterwards.
bool CompressSecure(int archive_fd, const std::string& temp_name, const std::string& out_name)
{
    FILE* infile = BCLog::OpenLogFileAt(archive_fd, temp_name.c_str(), "r");
    if (infile == nullptr) {
        LogPrintf("ERROR: Logger: Failed to open archive log file for compression %s.", temp_name);
        return false;
    }

    const int out_fd = ::openat(archive_fd, out_name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                                static_cast<mode_t>(0600));
    if (out_fd < 0) {
        fclose(infile);
        LogPrintf("ERROR: Logger: Failed to create archive gzip file %s.", out_name);
        return false;
    }

    {
        FdStreamBuf out_buf(out_fd);
        std::ostream out_file(&out_buf);
        boost::iostreams::filtering_ostream out;
        out.push(boost::iostreams::gzip_compressor());
        out.push(out_file);

        char buffer[65536];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), infile)) > 0) {
            out.write(buffer, static_cast<std::streamsize>(n));
        }

        // reset() flushes the compressor into the file stream.
        out.reset();
    }

    ::close(out_fd);
    fclose(infile);
    ::unlinkat(archive_fd, temp_name.c_str(), 0);

    return true;
}
} // namespace
#endif

bool BCLog::Logger::archive(bool fImmediate, fs::path pfile_out)
{
    bool fArchiveDaily = gArgs.GetBoolArg("-logarchivedaily", true);

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
    if (m_secure_open)
    {
        // A link at the name is not touched here; the directory is opened without following links below.
        boost::system::error_code ec;
        fs::create_directory(LogArchiveDir, ec);
    }
    else if (fs::exists(LogArchiveDir))
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
        bool reopened = true;

#ifndef WIN32
        // In secure mode the log and archive directories are opened once, each through every directory component
        // below the data directory, and the rename and the archive file are made relative to those descriptors.
        DirFd log_dir;
        DirFd archive_dir;
        if (m_secure_open) {
            archive_dir.fd = BCLog::OpenDirectoryUnder(m_secure_root, LogArchiveDir);
            log_dir.fd = BCLog::OpenDirectoryUnder(m_secure_root, m_file_path.parent_path());
            if (archive_dir.fd < 0 || log_dir.fd < 0) {
                LogPrintf("ERROR: Logger: archive: the log or archive directory could not be opened safely (%s).",
                          LogArchiveDir.string());
                return false;
            }
        }
#endif

        {
            std::lock_guard<std::mutex> scoped_lock(m_cs);

            // There is no log file to archive when the node runs with
            // -nodebuglogfile, or after the reopen below failed. In the second
            // case, ask the next log line to open the file again, as a reopen
            // request (SIGHUP) does.
            if (m_fileout == nullptr) {
                if (m_print_to_file) m_reopen_file = true;
                return false;
            }

            fclose(m_fileout);
            m_fileout = nullptr;

            plogfile = m_file_path;

            pfile_temp = LogArchiveDir / static_cast<fs::path>(m_file_path.stem().string() + "-" + DateTimeStrFormat("%Y%m%d%H%M%S", nTime) + m_file_path.extension().string());

            pfile_out = LogArchiveDir / static_cast<fs::path>((m_file_path.filename().stem().string() + "-" + DateTimeStrFormat("%Y%m%d%H%M%S", nTime)
                                         + m_file_path.filename().extension().string() + ".gz"));

#ifndef WIN32
            if (m_secure_open) {
                if (::renameat(log_dir.fd, plogfile.filename().c_str(), archive_dir.fd, pfile_temp.filename().c_str()) != 0) {
                    rename_error_msg = "Failed to rename logging file: ";
                    rename_error_msg += std::strerror(errno);
                }
            } else
#endif
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
            FILE* new_fileout = OpenOwnFile("a");
            if (new_fileout)
            {
                setbuf(new_fileout, nullptr); // unbuffered
                m_fileout = new_fileout;
            }
            else
            {
                reopened = false;
            }

            PrevArchiveCheckDate = ArchiveCheckDate;
        }

        if (!reopened)
        {
            LogPrintf("ERROR: Logger: archive: Failed to reopen %s. Logging to the file stops until it is reopened.",
                      plogfile.string());
        }

        if (!rename_error_msg.empty())
        {
            LogPrintf("ERROR: Logger: archive: %s", rename_error_msg);
            return false;
        }

        bool compressed_securely = false;
#ifndef WIN32
        if (m_secure_open) {
            if (!CompressSecure(archive_dir.fd, pfile_temp.filename().string(), pfile_out.filename().string())) {
                return false;
            }
            compressed_securely = true;
        }
#endif

        if (!compressed_securely) {
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
        }

        bool fDeleteOldLogArchives = gArgs.GetBoolArg("-deleteoldlogarchives", true);

        if (fDeleteOldLogArchives)
        {
            unsigned int nRetention = (unsigned int)gArgs.GetArg("-logarchiveretainnumfiles", 30);
            LogPrintf ("INFO: Logger: nRetention %i.", nRetention);

            std::set<fs::directory_entry, std::greater <fs::directory_entry>> SortedDirEntries;

            // Archive names are "<stem>-YYYYMMDDHHMMSS<ext>.gz" (see pfile_out
            // above). Match only THIS log's archives by an exact "<stem>-" PREFIX,
            // not a substring search: with a substring search a stem that is a
            // prefix of another (e.g. the node's "debug" vs the multiprocess GUI's
            // "debug_gui") would also match — and delete — the other log's archives,
            // since they share this one logarchive directory. The '-' separator is
            // included so "debug-" cannot match "debug_gui-".
            const std::string archive_prefix = m_file_path.filename().stem().string() + "-";

            // The logarchive directory can be modified concurrently by another
            // process that shares it: in -multiprocess mode the GUI archives its own
            // debug_gui.log alongside the node's debug.log. A file vanishing between
            // the directory read and its use can throw a filesystem_error mid-scan;
            // contain it so retention degrades to a skipped pass instead of
            // propagating out of archive() (the log file is already reopened above,
            // so logging continues; the next archive retries). Each process only
            // ever removes its OWN "<stem>-" archives, so their cleanups never
            // target the same files.
            try
            {
                // The greater than sort in the set should then return descending order by datetime.
                for (fs::directory_entry& DirEntry : fs::directory_iterator(LogArchiveDir))
                {
                    std::string sFilename = DirEntry.path().filename().string();

                    if (sFilename.rfind(archive_prefix, 0) == 0) SortedDirEntries.insert(DirEntry);
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
            catch (const std::exception& e)
            {
                LogPrintf("WARNING: Logger: archive retention pass skipped: %s", e.what());
            }
        }

        return true; // archive condition was satisfied. Return true after rotating and archiving.
    }
    else
    {
        return false; // archive condition was not satisfied. Do nothing and return false.
    }
}

std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    // std::locale takes ownership of the pointer
    std::locale loc(std::locale::classic(), new boost::posix_time::time_facet(pszFormat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::posix_time::from_time_t(nTime);
    return ss.str();
}
