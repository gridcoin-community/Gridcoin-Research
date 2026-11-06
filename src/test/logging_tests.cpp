// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "logging.h"
#include "test/test_gridcoin.h"

#include "util/system.h"
#include "util/time.h"

#include "test/state_guard.h"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/test/unit_test.hpp>

#include <sstream>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef WIN32
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef WIN32
namespace {
// Only the POSIX cases below read a file back.
std::string ReadAll(const fs::path& path)
{
    std::ifstream in(path.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
} // namespace
#endif

BOOST_AUTO_TEST_SUITE(logging_tests)

BOOST_AUTO_TEST_CASE(archive_does_nothing_without_a_log_file)
{
    // A logger that never opened its file, as with -nodebuglogfile.
    BCLog::Logger logger;
    BOOST_REQUIRE(!logger.m_print_to_file);

    BOOST_CHECK(!logger.archive(/*fImmediate=*/true, fs::path()));
}

BOOST_AUTO_TEST_CASE(escape_newlines_keeps_only_a_trailing_one)
{
    BOOST_CHECK_EQUAL(BCLog::LogEscapeMessage("a\nb\n", true), "a\\x0ab\n");
    BOOST_CHECK_EQUAL(BCLog::LogEscapeMessage("a\nb\n", false), "a\nb\n");
    BOOST_CHECK_EQUAL(BCLog::LogEscapeMessage("x\r\n", true), "x\\x0d\n");
    BOOST_CHECK_EQUAL(BCLog::LogEscapeMessage("\n", true), "\n");
    BOOST_CHECK_EQUAL(BCLog::LogEscapeMessage("a\n\n", true), "a\\x0a\n");
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(secure_open_refuses_symlink_hardlink_and_fifo)
{
    const fs::path dir = GetDataDir() / "logging_tests_secure_open";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const fs::path target = dir / "T";
    {
        std::ofstream out(target.string(), std::ios::binary);
        out << "original";
    }
    // The symlink points at a file with a single link, so only O_NOFOLLOW can refuse it.
    const fs::path link_target = dir / "S";
    {
        std::ofstream out(link_target.string(), std::ios::binary);
        out << "linked";
    }
    BOOST_REQUIRE_EQUAL(::symlink(link_target.string().c_str(), (dir / "L").string().c_str()), 0);
    BOOST_REQUIRE_EQUAL(::link(target.string().c_str(), (dir / "H").string().c_str()), 0);
    BOOST_REQUIRE_EQUAL(::mkfifo((dir / "F").string().c_str(), 0600), 0);

    BOOST_CHECK(BCLog::OpenLogFile(dir / "L", "a", true) == nullptr);
    BOOST_CHECK(BCLog::OpenLogFile(dir / "H", "a", true) == nullptr);
    // No reader on the FIFO, so this must return at once rather than block.
    BOOST_CHECK(BCLog::OpenLogFile(dir / "F", "a", true) == nullptr);
    // Not a regular file, in a mode that would open a FIFO that has a writer.
    BOOST_CHECK(BCLog::OpenLogFile(dir / "F", "r", true) == nullptr);
    BOOST_CHECK(BCLog::OpenLogFile(dir / "L", "w", true) == nullptr);
    BOOST_CHECK_EQUAL(ReadAll(link_target), "linked");
    // A "w" open on the hard link refuses before anything is truncated.
    BOOST_CHECK(BCLog::OpenLogFile(dir / "H", "w", true) == nullptr);
    BOOST_CHECK(BCLog::OpenLogFile(dir / "H", "r", true) == nullptr);
    BOOST_CHECK_EQUAL(ReadAll(target), "original");

    // A plain file of our own is accepted in every mode; "w" truncates, "a" appends.
    const fs::path plain = dir / "plain.log";
    {
        std::ofstream out(plain.string(), std::ios::binary);
        out << "old";
    }
    FILE* f = BCLog::OpenLogFile(plain, "a", true);
    BOOST_REQUIRE(f);
    fputs("+new", f);
    fclose(f);
    BOOST_CHECK_EQUAL(ReadAll(plain), "old+new");
    f = BCLog::OpenLogFile(plain, "r", true);
    BOOST_REQUIRE(f);
    fclose(f);
    f = BCLog::OpenLogFile(plain, "w", true);
    BOOST_REQUIRE(f);
    fputs("z", f);
    fclose(f);
    BOOST_CHECK_EQUAL(ReadAll(plain), "z");

    fs::remove_all(dir);
}

BOOST_AUTO_TEST_CASE(secure_open_creates_0600_under_permissive_umask)
{
    const fs::path dir = GetDataDir() / "logging_tests_secure_open_mode";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const mode_t old = ::umask(0);
    FILE* f = BCLog::OpenLogFile(dir / "new.log", "a", true);
    ::umask(old);
    BOOST_REQUIRE(f);
    fclose(f);

    struct stat st;
    BOOST_REQUIRE_EQUAL(::stat((dir / "new.log").string().c_str(), &st), 0);
    BOOST_CHECK_EQUAL(st.st_mode & 07777, static_cast<mode_t>(0600));

    fs::remove_all(dir);
}
BOOST_AUTO_TEST_CASE(refused_open_does_not_buffer_log_lines)
{
    const fs::path dir = GetDataDir() / "logging_tests_refused_open";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const fs::path target = dir / "T";
    {
        std::ofstream out(target.string(), std::ios::binary);
        out << "original";
    }
    BOOST_REQUIRE_EQUAL(::symlink(target.string().c_str(), (dir / "L").string().c_str()), 0);

    BCLog::Logger logger;
    logger.m_print_to_file = true;
    logger.m_secure_open = true;
    logger.m_file_path = dir / "L";

    std::vector<std::string> seen;
    logger.PushBackCallback([&seen](const std::string& s) { seen.push_back(s); });

    logger.LogPrintStr("before\n");
    BOOST_CHECK(seen.empty());

    BOOST_CHECK(!logger.StartLogging());
    BOOST_CHECK(logger.m_file_open_failed);
    BOOST_CHECK(!logger.m_print_to_file);
    BOOST_REQUIRE_EQUAL(seen.size(), 1u);

    // A later line reaches the callback at once: nothing is held back for a file that never opened.
    logger.LogPrintStr("after\n");
    BOOST_CHECK_EQUAL(seen.size(), 2u);
    BOOST_CHECK_EQUAL(ReadAll(target), "original");

    logger.DisconnectTestLogger();
    fs::remove_all(dir);
}

BOOST_AUTO_TEST_CASE(secure_open_refuses_symlinked_directories)
{
    const fs::path root = GetDataDir() / "logging_tests_secure_dirs";
    fs::remove_all(root);
    fs::create_directories(root / "real");
    BOOST_REQUIRE_EQUAL(::symlink((root / "real").string().c_str(), (root / "link").string().c_str()), 0);

    FILE* f = BCLog::OpenLogFileUnder(root, root / "real" / "x.log", "a");
    BOOST_REQUIRE(f);
    fclose(f);

    BOOST_CHECK(BCLog::OpenLogFileUnder(root, root / "link" / "x.log", "a") == nullptr);
    BOOST_CHECK(!fs::exists(root / "real" / "y.log"));
    BOOST_CHECK(BCLog::OpenLogFileUnder(root, root / "link" / "y.log", "a") == nullptr);
    BOOST_CHECK(!fs::exists(root / "real" / "y.log"));

    const int bad = BCLog::OpenDirectoryUnder(root, root / "link");
    BOOST_CHECK(bad < 0);
    if (bad >= 0) ::close(bad);
    const int good = BCLog::OpenDirectoryUnder(root, root / "real");
    BOOST_CHECK(good >= 0);
    if (good >= 0) ::close(good);

    fs::remove_all(root);
}

namespace {
//! A logger that logs to a file in the data directory the way the multiprocess GUI's does.
void SetUpSecureLogger(BCLog::Logger& logger, const std::string& file_name)
{
    logger.m_print_to_file = true;
    logger.m_secure_open = true;
    logger.m_secure_root = GetDataDir(/*fNetSpecific=*/false);
    logger.m_file_path = GetDataDir() / file_name;
}
} // namespace

BOOST_AUTO_TEST_CASE(archive_does_not_follow_a_link_at_the_archive_name)
{
    const fs::path archive_dir = GetDataDir() / "logarchive";
    fs::create_directories(archive_dir);

    const fs::path victim = GetDataDir() / "logging_tests_archive_victim";
    {
        std::ofstream out(victim.string(), std::ios::binary);
        out << "original";
    }

    BCLog::Logger logger;
    SetUpSecureLogger(logger, "logging_tests_archive.log");
    BOOST_REQUIRE(logger.StartLogging());
    logger.LogPrintStr("line\n");

    // The archive name carries the time to the second, so plant the names around now.
    const int64_t now = GetAdjustedTime();
    std::vector<fs::path> planted;
    for (int64_t t = now - 3; t <= now + 3; ++t) {
        const fs::path link = archive_dir / ("logging_tests_archive-" + DateTimeStrFormat("%Y%m%d%H%M%S", t) + ".log.gz");
        BOOST_REQUIRE_EQUAL(::symlink(victim.string().c_str(), link.string().c_str()), 0);
        planted.push_back(link);
    }

    BOOST_CHECK(!logger.archive(/*fImmediate=*/true, fs::path()));
    BOOST_CHECK_EQUAL(ReadAll(victim), "original");

    logger.DisconnectTestLogger();
    for (const fs::path& link : planted) fs::remove(link);
    for (fs::directory_iterator it(archive_dir), end; it != end; ++it) {
        if (it->path().filename().string().rfind("logging_tests_archive-", 0) == 0) fs::remove(it->path());
    }
    fs::remove(victim);
    fs::remove(GetDataDir() / "logging_tests_archive.log");
}

BOOST_AUTO_TEST_CASE(secure_archive_writes_a_gzip_file)
{
    const fs::path archive_dir = GetDataDir() / "logarchive";
    fs::create_directories(archive_dir);

    BCLog::Logger logger;
    SetUpSecureLogger(logger, "logging_tests_archive_ok.log");
    BOOST_REQUIRE(logger.StartLogging());
    logger.LogPrintStr("line\n");

    BOOST_CHECK(logger.archive(/*fImmediate=*/true, fs::path()));

    size_t found = 0;
    for (fs::directory_iterator it(archive_dir), end; it != end; ++it) {
        const std::string name = it->path().filename().string();
        if (name.rfind("logging_tests_archive_ok-", 0) != 0) continue;
        ++found;
        const std::string data = ReadAll(it->path());
        std::istringstream compressed(data);
        std::ostringstream plain;
        boost::iostreams::filtering_istream in;
        in.push(boost::iostreams::gzip_decompressor());
        in.push(compressed);
        boost::iostreams::copy(in, plain);
        // The logger writes a run of newlines first, then the line.
        const std::string text = plain.str();
        BOOST_CHECK(text.size() >= 5 && text.compare(text.size() - 5, 5, "line\n") == 0);
        fs::remove(it->path());
    }
    BOOST_CHECK_EQUAL(found, 1u);

    // The log was reopened at its old name.
    logger.LogPrintStr("again\n");
    BOOST_CHECK(ReadAll(GetDataDir() / "logging_tests_archive_ok.log").find("again") != std::string::npos);

    logger.DisconnectTestLogger();
    fs::remove(GetDataDir() / "logging_tests_archive_ok.log");
}

// The log that is recreated at its name after a rotation has the mode the rotated log had, so a
// log the operator made group-readable stays so. Production change that would fail this:
// archive() reopening the log with the mode it creates a new log with (0600).
BOOST_AUTO_TEST_CASE(archive_recreates_the_log_with_the_mode_it_had)
{
    const fs::path archive_dir = GetDataDir() / "logarchive";
    fs::create_directories(archive_dir);

    BCLog::Logger logger;
    SetUpSecureLogger(logger, "logging_tests_archive_mode.log");
    BOOST_REQUIRE(logger.StartLogging());
    logger.LogPrintStr("line\n");
    const fs::path log = GetDataDir() / "logging_tests_archive_mode.log";
    BOOST_REQUIRE_EQUAL(::chmod(log.string().c_str(), 0640), 0);

    BOOST_CHECK(logger.archive(/*fImmediate=*/true, fs::path()));

    struct stat st;
    BOOST_REQUIRE_EQUAL(::stat(log.string().c_str(), &st), 0);
    BOOST_CHECK_EQUAL(st.st_mode & 07777, 0640U);

    logger.DisconnectTestLogger();
    for (fs::directory_iterator it(archive_dir), end; it != end; ++it) {
        if (it->path().filename().string().rfind("logging_tests_archive_mode-", 0) == 0) fs::remove(it->path());
    }
    fs::remove(log);
}

BOOST_AUTO_TEST_CASE(archive_refuses_a_symlinked_archive_directory)
{
    const fs::path archive_dir = GetDataDir() / "logarchive";
    const fs::path aside = GetDataDir() / "logarchive.logging_tests_aside";
    const fs::path real = GetDataDir() / "logging_tests_real_archive";
    fs::remove_all(aside);
    fs::remove_all(real);
    const bool had_archive_dir = fs::exists(archive_dir);
    if (had_archive_dir) fs::rename(archive_dir, aside);
    fs::create_directories(real);
    BOOST_REQUIRE_EQUAL(::symlink(real.string().c_str(), archive_dir.string().c_str()), 0);

    BCLog::Logger logger;
    SetUpSecureLogger(logger, "logging_tests_archive_dir.log");
    BOOST_REQUIRE(logger.StartLogging());
    logger.LogPrintStr("line\n");

    BOOST_CHECK(!logger.archive(/*fImmediate=*/true, fs::path()));
    BOOST_CHECK(fs::is_empty(real));
    // The log file was not moved out from under the logger.
    BOOST_CHECK(fs::exists(GetDataDir() / "logging_tests_archive_dir.log"));

    logger.DisconnectTestLogger();
    fs::remove(archive_dir);
    if (had_archive_dir) fs::rename(aside, archive_dir);
    fs::remove_all(real);
    fs::remove(GetDataDir() / "logging_tests_archive_dir.log");
}
BOOST_AUTO_TEST_CASE(secure_archive_failure_keeps_the_log_and_leaves_no_partial_archive)
{
    const fs::path archive_dir = GetDataDir() / "logarchive";
    fs::create_directories(archive_dir);

    BCLog::Logger logger;
    SetUpSecureLogger(logger, "logging_tests_archive_fail.log");
    BOOST_REQUIRE(logger.StartLogging());
    logger.LogPrintStr("line\n");

    // A file size limit makes the write of the archive fail part way.
    struct rlimit old_limit;
    BOOST_REQUIRE_EQUAL(::getrlimit(RLIMIT_FSIZE, &old_limit), 0);
    void (*old_handler)(int) = ::signal(SIGXFSZ, SIG_IGN);
    struct rlimit limit = old_limit;
    limit.rlim_cur = 20;
    BOOST_REQUIRE_EQUAL(::setrlimit(RLIMIT_FSIZE, &limit), 0);

    const bool archived = logger.archive(/*fImmediate=*/true, fs::path());

    BOOST_REQUIRE_EQUAL(::setrlimit(RLIMIT_FSIZE, &old_limit), 0);
    ::signal(SIGXFSZ, old_handler);

    BOOST_CHECK(!archived);

    size_t gz = 0, temp = 0;
    for (fs::directory_iterator it(archive_dir), end; it != end; ++it) {
        const std::string name = it->path().filename().string();
        if (name.rfind("logging_tests_archive_fail-", 0) != 0) continue;
        if (name.size() > 3 && name.compare(name.size() - 3, 3, ".gz") == 0) ++gz; else ++temp;
        fs::remove(it->path());
    }
    BOOST_CHECK_EQUAL(gz, 0u);
    // The log stays in the temp file.
    BOOST_CHECK_EQUAL(temp, 1u);

    logger.DisconnectTestLogger();
    fs::remove(GetDataDir() / "logging_tests_archive_fail.log");
}

BOOST_AUTO_TEST_CASE(secure_archive_retention_removes_the_oldest_by_name)
{
    grc_test::StateGuard guard;
    gArgs.ForceSetArg("-logarchiveretainnumfiles", "2");

    const fs::path archive_dir = GetDataDir() / "logarchive";
    fs::create_directories(archive_dir);
    for (const char* stamp : {"99990101000001", "99990101000002", "99990101000003"}) {
        std::ofstream out((archive_dir / (std::string("logging_tests_ret-") + stamp + ".log.gz")).string());
        out << "x";
    }
    // Another log's archive in the same directory is left alone.
    {
        std::ofstream out((archive_dir / "logging_tests_ret_other-20000101000001.log.gz").string());
        out << "x";
    }

    BCLog::Logger logger;
    SetUpSecureLogger(logger, "logging_tests_ret.log");
    BOOST_REQUIRE(logger.StartLogging());
    logger.LogPrintStr("line\n");
    BOOST_CHECK(logger.archive(/*fImmediate=*/true, fs::path()));

    // Only the two newest names are kept (the new archive, stamped with the current time, sorts below these).
    BOOST_CHECK(!fs::exists(archive_dir / "logging_tests_ret-99990101000001.log.gz"));
    BOOST_CHECK(fs::exists(archive_dir / "logging_tests_ret-99990101000002.log.gz"));
    BOOST_CHECK(fs::exists(archive_dir / "logging_tests_ret-99990101000003.log.gz"));
    BOOST_CHECK(fs::exists(archive_dir / "logging_tests_ret_other-20000101000001.log.gz"));

    logger.DisconnectTestLogger();
    for (fs::directory_iterator it(archive_dir), end; it != end; ++it) {
        if (it->path().filename().string().rfind("logging_tests_ret", 0) == 0) fs::remove(it->path());
    }
    fs::remove(GetDataDir() / "logging_tests_ret.log");
}

BOOST_AUTO_TEST_CASE(failed_archive_directory_open_counts_as_the_daily_check)
{
    const fs::path archive_dir = GetDataDir() / "logarchive";
    const fs::path aside = GetDataDir() / "logarchive.logging_tests_aside2";
    const fs::path real = GetDataDir() / "logging_tests_real_archive2";
    fs::remove_all(aside);
    fs::remove_all(real);
    const bool had_archive_dir = fs::exists(archive_dir);
    if (had_archive_dir) fs::rename(archive_dir, aside);
    fs::create_directories(real);
    BOOST_REQUIRE_EQUAL(::symlink(real.string().c_str(), archive_dir.string().c_str()), 0);

    BCLog::Logger logger;
    SetUpSecureLogger(logger, "logging_tests_daily.log");
    BOOST_REQUIRE(logger.StartLogging());

    const boost::gregorian::date saved = BCLog::Logger::PrevArchiveCheckDate;
    BCLog::Logger::PrevArchiveCheckDate = boost::gregorian::date(2000, 1, 1);

    BOOST_CHECK(!logger.archive(/*fImmediate=*/false, fs::path()));
    const boost::gregorian::date today = boost::posix_time::from_time_t(GetAdjustedTime()).date();
    BOOST_CHECK(BCLog::Logger::PrevArchiveCheckDate == today);

    BCLog::Logger::PrevArchiveCheckDate = saved;
    logger.DisconnectTestLogger();
    fs::remove(archive_dir);
    if (had_archive_dir) fs::rename(aside, archive_dir);
    fs::remove_all(real);
    fs::remove(GetDataDir() / "logging_tests_daily.log");
}

BOOST_AUTO_TEST_CASE(create_directory_under_does_not_follow_a_link)
{
    const fs::path root = GetDataDir() / "logging_tests_mkdir";
    fs::remove_all(root);
    fs::create_directories(root / "real");
    BOOST_REQUIRE_EQUAL(::symlink((root / "real").string().c_str(), (root / "link").string().c_str()), 0);

    BOOST_CHECK(BCLog::CreateDirectoryUnder(root, root / "real", "sub"));
    BOOST_CHECK(fs::is_directory(root / "real" / "sub"));
    // Already there is fine.
    BOOST_CHECK(BCLog::CreateDirectoryUnder(root, root / "real", "sub"));

    BOOST_CHECK(!BCLog::CreateDirectoryUnder(root, root / "link", "other"));
    BOOST_CHECK(!fs::exists(root / "real" / "other"));

    // A link in place of the directory itself is not accepted either.
    BOOST_REQUIRE_EQUAL(::symlink((root / "real").string().c_str(), (root / "real" / "lnk").string().c_str()), 0);
    BOOST_CHECK(!BCLog::CreateDirectoryUnder(root, root / "real", "lnk"));

    fs::remove_all(root);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
