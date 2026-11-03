// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/process.h"

#include "fs.h"
#include "test/state_guard.h"
#include "util.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include <system_error>
#include <vector>

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <optional>
#endif

namespace {

#ifndef WIN32
//! A mkdtemp directory removed when the case ends. It is under /tmp, not the
//! datadir, to keep the socket path within the sockaddr_un limit.
struct TempDir
{
    fs::path path;

    TempDir()
    {
        std::string tmpl = "/tmp/ipcproc_XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        BOOST_REQUIRE(::mkdtemp(buf.data()) != nullptr);
        path = fs::path(buf.data());
    }

    ~TempDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

mode_t ModeOf(const fs::path& p)
{
    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat(p.c_str(), &st), 0);
    return st.st_mode & 07777;
}

gid_t GroupOf(const fs::path& p)
{
    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat(p.c_str(), &st), 0);
    return st.st_gid;
}
#endif

} // namespace

BOOST_AUTO_TEST_SUITE(ipc_process_tests, *boost::unit_test::fixture<grc_test::StateGuard>())

BOOST_AUTO_TEST_CASE(ipcdir_defaults_to_datadir)
{
    gArgs.ClearForcedArg("-ipcdir");
    BOOST_CHECK(ipc::GetIpcDir() == GetDataDir());
}

BOOST_AUTO_TEST_CASE(ipcdir_absolute_is_used_verbatim)
{
    gArgs.ForceSetArg("-ipcdir", "/run/gridcointestnetd");
    BOOST_CHECK(ipc::GetIpcDir() == fs::path("/run/gridcointestnetd"));
}

BOOST_AUTO_TEST_CASE(ipcdir_relative_resolves_against_datadir)
{
    gArgs.ForceSetArg("-ipcdir", "ipc");
    BOOST_CHECK(ipc::GetIpcDir() == GetDataDir() / "ipc");
}

#ifndef WIN32

BOOST_AUTO_TEST_CASE(bind_leaves_process_umask_unchanged)
{
    TempDir dir;
    const mode_t before = ::umask(022);
    ::umask(022);

    std::string addr = "unix";
    int fd = -1;
    BOOST_CHECK_NO_THROW(fd = ipc::MakeProcess()->bind(dir.path, addr));

    const mode_t after = ::umask(022);
    ::umask(before);
    BOOST_CHECK_EQUAL(after, static_cast<mode_t>(022));

    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat((dir.path / "node.sock").c_str(), &st), 0);
    BOOST_CHECK(S_ISSOCK(st.st_mode));
    BOOST_CHECK_EQUAL(st.st_mode & 07777, 0600);
    if (fd != -1) ::close(fd);
}

BOOST_AUTO_TEST_CASE(apply_path_access_refuses_symlink)
{
    TempDir dir;
    const fs::path target = dir.path / "target";
    { std::ofstream(target.string()) << "x"; }
    BOOST_REQUIRE_EQUAL(::chmod(target.c_str(), 0644), 0);
    BOOST_REQUIRE_EQUAL(::symlink(target.c_str(), (dir.path / "link").c_str()), 0);

    BOOST_CHECK_THROW(ipc::ApplyPathAccessNoFollow(dir.path / "link", ipc::PathKind::RegularFile, 0600, std::nullopt),
                      std::system_error);
    BOOST_CHECK_EQUAL(ModeOf(target), 0644);
}

BOOST_AUTO_TEST_CASE(apply_path_access_refuses_wrong_type)
{
    TempDir dir;
    BOOST_CHECK_THROW(ipc::ApplyPathAccessNoFollow(dir.path, ipc::PathKind::RegularFile, 0600, std::nullopt),
                      std::system_error);
}

BOOST_AUTO_TEST_CASE(apply_path_access_sets_mode_on_directory)
{
    TempDir dir;
    ipc::ApplyPathAccessNoFollow(dir.path, ipc::PathKind::Directory, 0700, std::nullopt);
    BOOST_CHECK_EQUAL(ModeOf(dir.path), 0700);

    // The process's own primary group is always permitted.
    ipc::ApplyPathAccessNoFollow(dir.path, ipc::PathKind::Directory, 0750, ::getegid());
    BOOST_CHECK_EQUAL(ModeOf(dir.path), 0750);
    BOOST_CHECK_EQUAL(GroupOf(dir.path), ::getegid());
}

#endif

BOOST_AUTO_TEST_SUITE_END()
