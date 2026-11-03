// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"
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

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <grp.h>
#include <optional>
#include <stdexcept>
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

//! Clears the configured attach group when a case ends, on every exit path.
struct AttachGroupReset
{
    ~AttachGroupReset() { ipc::ConfigureAttachGroup(""); }
};

//! The name of the group with gid \p gid, or nullopt when the group database
//! has no entry for it.
std::optional<std::string> GroupName(gid_t gid)
{
    const struct group* gr = ::getgrgid(gid);
    if (gr == nullptr) return std::nullopt;
    return std::string{gr->gr_name};
}

//! A group this account belongs to other than its effective group, from
//! getgroups(), or nullopt when there is none.
std::optional<gid_t> OtherMemberGroup()
{
    std::vector<gid_t> groups(256);
    const int n = ::getgroups(static_cast<int>(groups.size()), groups.data());
    for (int i = 0; i < n; ++i) {
        if (groups[i] != ::getegid()) return groups[i];
    }
    return std::nullopt;
}
#endif

} // namespace

BOOST_AUTO_TEST_SUITE(ipc_process_tests, *boost::unit_test::fixture<grc_test::StateGuard>())

// The -ipcdir cases change gArgs. Each runs under its own StateGuard, which puts
// the settings back when the case ends, also when a BOOST_REQUIRE ends it early.

BOOST_FIXTURE_TEST_CASE(ipcdir_defaults_to_datadir, grc_test::StateGuard)
{
    gArgs.ClearForcedArg("-ipcdir");
    BOOST_CHECK(ipc::GetIpcDir() == GetDataDir());
}

BOOST_FIXTURE_TEST_CASE(ipcdir_absolute_is_used_verbatim, grc_test::StateGuard)
{
    gArgs.ForceSetArg("-ipcdir", "/run/gridcointestnetd");
    BOOST_CHECK(ipc::GetIpcDir() == fs::path("/run/gridcointestnetd"));
}

BOOST_FIXTURE_TEST_CASE(ipcdir_relative_resolves_against_datadir, grc_test::StateGuard)
{
    gArgs.ForceSetArg("-ipcdir", "ipc");
    BOOST_CHECK(ipc::GetIpcDir() == GetDataDir() / "ipc");
}

BOOST_FIXTURE_TEST_CASE(ipcdir_negated_counts_as_unset, grc_test::StateGuard)
{
    // A forced value would take precedence over the command line one.
    gArgs.ClearForcedArg("-ipcdir");
    // -noipcdir is stored as the setting false, which IsArgSet still reports as set.
    gArgs.LockSettings([](util::Settings& settings) {
        util::SettingsValue negated;
        negated.setBool(false);
        settings.command_line_options["ipcdir"] = {negated};
    });
    BOOST_REQUIRE(gArgs.IsArgSet("-ipcdir"));
    BOOST_REQUIRE(gArgs.IsArgNegated("-ipcdir"));
    BOOST_CHECK(ipc::GetIpcDir() == GetDataDir());
}

#ifndef WIN32

// Expected modes are written as mode_t: Boost.Test compares them with the
// mode_t read back, and an int literal there raises -Wsign-compare.

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
    BOOST_CHECK_EQUAL(st.st_mode & 07777, static_cast<mode_t>(0600));
    if (fd != -1) ::close(fd);
}

BOOST_AUTO_TEST_CASE(bind_creates_missing_parent_owner_only)
{
    TempDir dir;
    const fs::path sub = dir.path / "sub";
    std::string addr = "unix:" + (sub / "node.sock").string();
    int fd = -1;
    BOOST_CHECK_NO_THROW(fd = ipc::MakeProcess()->bind(dir.path, addr));
    BOOST_CHECK_EQUAL(ModeOf(sub), static_cast<mode_t>(0700));
    if (fd != -1) ::close(fd);
}

BOOST_AUTO_TEST_CASE(prepare_ipc_directory_creates_owner_only_and_leaves_existing)
{
    TempDir dir;
    const fs::path sub = dir.path / "a" / "b";
    ipc::PrepareIpcDirectory(sub);
    BOOST_CHECK_EQUAL(ModeOf(sub), static_cast<mode_t>(0700));

    // An existing directory keeps whatever mode it has.
    BOOST_REQUIRE_EQUAL(::chmod(sub.c_str(), 0750), 0);
    ipc::PrepareIpcDirectory(sub);
    BOOST_CHECK_EQUAL(ModeOf(sub), static_cast<mode_t>(0750));

    // A path under a regular file cannot be created and must throw.
    const fs::path file = dir.path / "file";
    { std::ofstream(file.string()) << "x"; }
    BOOST_CHECK_THROW(ipc::PrepareIpcDirectory(file / "d"), std::system_error);
}

BOOST_AUTO_TEST_CASE(apply_path_access_sets_supplementary_group)
{
    std::vector<gid_t> groups(64);
    const int n = ::getgroups(static_cast<int>(groups.size()), groups.data());
    std::optional<gid_t> other;
    for (int i = 0; i < n; ++i) {
        if (groups[i] != ::getegid()) {
            other = groups[i];
            break;
        }
    }
    if (!other) {
        BOOST_TEST_MESSAGE("no supplementary group other than the primary one; skipping");
        return;
    }
    TempDir dir;
    ipc::ApplyPathAccessNoFollow(dir.path, ipc::PathKind::Directory, 0750, other);
    BOOST_CHECK_EQUAL(ModeOf(dir.path), static_cast<mode_t>(0750));
    BOOST_CHECK_EQUAL(GroupOf(dir.path), *other);
}

// With a group configured, bind leaves the socket 0660 in that group.
BOOST_AUTO_TEST_CASE(bind_with_attach_group_sets_0660_and_group)
{
    AttachGroupReset reset;
    const auto name = GroupName(::getegid());
    if (!name) {
        BOOST_TEST_MESSAGE("the effective group has no entry in the group database; skipping");
        return;
    }
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*name));

    TempDir dir;
    std::string addr = "unix";
    int fd = -1;
    BOOST_CHECK_NO_THROW(fd = ipc::MakeProcess()->bind(dir.path, addr));
    const fs::path sock = dir.path / "node.sock";
    BOOST_CHECK_EQUAL(ModeOf(sock), static_cast<mode_t>(0660));
    BOOST_CHECK_EQUAL(GroupOf(sock), ::getegid());
    if (fd != -1) ::close(fd);
}

// A group that is a supplementary group of this account, not its effective
// group, is applied as well.
BOOST_AUTO_TEST_CASE(bind_with_supplementary_attach_group_sets_that_group)
{
    AttachGroupReset reset;
    const auto other = OtherMemberGroup();
    const auto name = other ? GroupName(*other) : std::nullopt;
    if (!name) {
        BOOST_TEST_MESSAGE("no named supplementary group other than the effective one; skipping");
        return;
    }
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*name));

    TempDir dir;
    std::string addr = "unix";
    int fd = -1;
    BOOST_CHECK_NO_THROW(fd = ipc::MakeProcess()->bind(dir.path, addr));
    BOOST_CHECK_EQUAL(ModeOf(dir.path / "node.sock"), static_cast<mode_t>(0660));
    BOOST_CHECK_EQUAL(GroupOf(dir.path / "node.sock"), *other);
    if (fd != -1) ::close(fd);
}

BOOST_AUTO_TEST_CASE(attach_group_dir_is_0750)
{
    AttachGroupReset reset;
    const auto name = GroupName(::getegid());
    if (!name) {
        BOOST_TEST_MESSAGE("the effective group has no entry in the group database; skipping");
        return;
    }
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*name));

    TempDir dir;
    ipc::ApplyAttachGroupAccess(dir.path);
    BOOST_CHECK_EQUAL(ModeOf(dir.path), static_cast<mode_t>(0750));
    BOOST_CHECK_EQUAL(GroupOf(dir.path), ::getegid());
}

// Without a configured group there is nothing to apply, which is a caller error.
BOOST_AUTO_TEST_CASE(attach_group_access_needs_a_configured_group)
{
    AttachGroupReset reset;
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(""));
    TempDir dir;
    BOOST_CHECK_THROW(ipc::ApplyAttachGroupAccess(dir.path), std::logic_error);
}

// A group this account is not a member of cannot be applied by a non-root
// process, and the error says what to change.
BOOST_AUTO_TEST_CASE(attach_group_not_a_member_names_the_remedy)
{
    AttachGroupReset reset;
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("running as root, which can set any group; skipping");
        return;
    }
    std::vector<gid_t> mine(256);
    const int n = ::getgroups(static_cast<int>(mine.size()), mine.data());
    mine.resize(n > 0 ? static_cast<size_t>(n) : 0);

    std::optional<std::string> foreign;
    ::setgrent();
    while (const struct group* gr = ::getgrent()) {
        if (gr->gr_gid == ::getegid()) continue;
        if (std::find(mine.begin(), mine.end(), gr->gr_gid) != mine.end()) continue;
        foreign = std::string{gr->gr_name};
        break;
    }
    ::endgrent();
    if (!foreign) {
        BOOST_TEST_MESSAGE("no group this account is outside of; skipping");
        return;
    }
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*foreign));

    TempDir dir;
    try {
        ipc::ApplyAttachGroupAccess(dir.path);
        BOOST_ERROR("ApplyAttachGroupAccess did not throw");
    } catch (const std::system_error& e) {
        BOOST_CHECK_MESSAGE(std::string(e.what()).find(
                                "the core must run with the attach group as a supplementary group") !=
                                std::string::npos,
                            e.what());
    }
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
    BOOST_CHECK_EQUAL(ModeOf(target), static_cast<mode_t>(0644));
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
    BOOST_CHECK_EQUAL(ModeOf(dir.path), static_cast<mode_t>(0700));

    // The process's own primary group is always permitted.
    ipc::ApplyPathAccessNoFollow(dir.path, ipc::PathKind::Directory, 0750, ::getegid());
    BOOST_CHECK_EQUAL(ModeOf(dir.path), static_cast<mode_t>(0750));
    BOOST_CHECK_EQUAL(GroupOf(dir.path), ::getegid());
}

#endif

BOOST_AUTO_TEST_SUITE_END()
