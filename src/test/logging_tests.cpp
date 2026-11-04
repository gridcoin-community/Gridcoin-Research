// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "logging.h"
#include "test/test_gridcoin.h"

#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <iterator>
#include <string>

#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
std::string ReadAll(const fs::path& path)
{
    std::ifstream in(path.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
} // namespace

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
    BOOST_REQUIRE_EQUAL(::symlink(target.string().c_str(), (dir / "L").string().c_str()), 0);
    BOOST_REQUIRE_EQUAL(::link(target.string().c_str(), (dir / "H").string().c_str()), 0);
    BOOST_REQUIRE_EQUAL(::mkfifo((dir / "F").string().c_str(), 0600), 0);

    BOOST_CHECK(BCLog::OpenLogFile(dir / "L", "a", true) == nullptr);
    BOOST_CHECK(BCLog::OpenLogFile(dir / "H", "a", true) == nullptr);
    // No reader on the FIFO, so this must return at once rather than block.
    BOOST_CHECK(BCLog::OpenLogFile(dir / "F", "a", true) == nullptr);
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
#endif

BOOST_AUTO_TEST_SUITE_END()
