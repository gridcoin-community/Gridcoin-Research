// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"
#include "ipc/process.h"

#include "fs.h"
#include "logging.h"
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
#include <functional>
#include <list>
#include <grp.h>
#include <optional>
#include <stdexcept>
#include <tuple>
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

//! The supplementary groups of this process, sized by getgroups(0, nullptr).
std::vector<gid_t> SupplementaryGroups()
{
    const int count = ::getgroups(0, nullptr);
    std::vector<gid_t> groups(count > 0 ? static_cast<size_t>(count) : 0);
    const int n = groups.empty() ? 0 : ::getgroups(count, groups.data());
    groups.resize(n > 0 ? static_cast<size_t>(n) : 0);
    return groups;
}

//! A group this account belongs to other than its effective group, from
//! getgroups(), or nullopt when there is none.
std::optional<gid_t> OtherMemberGroup()
{
    for (const gid_t g : SupplementaryGroups()) {
        if (g != ::getegid()) return g;
    }
    return std::nullopt;
}

//! The name of a group this account is not a member of, or nullopt.
std::optional<std::string> ForeignGroupName()
{
    const std::vector<gid_t> mine = SupplementaryGroups();
    std::optional<std::string> foreign;
    ::setgrent();
    while (const struct group* gr = ::getgrent()) {
        if (gr->gr_gid == ::getegid()) continue;
        if (std::find(mine.begin(), mine.end(), gr->gr_gid) != mine.end()) continue;
        foreign = std::string{gr->gr_name};
        break;
    }
    ::endgrent();
    return foreign;
}

//! Collects the lines the logger prints while in scope.
struct CaptureLog
{
    std::vector<std::string> lines;
    std::list<std::function<void(const std::string&)>>::iterator callback;

    CaptureLog()
        : callback(LogInstance().PushBackCallback([this](const std::string& s) { lines.push_back(s); }))
    {
    }
    ~CaptureLog() { LogInstance().DeleteCallback(callback); }
    CaptureLog(const CaptureLog&) = delete;
    CaptureLog& operator=(const CaptureLog&) = delete;

    bool Contains(const std::string& needle) const
    {
        return std::any_of(lines.begin(), lines.end(),
                           [&](const std::string& l) { return l.find(needle) != std::string::npos; });
    }
};
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

// A group this account is not a member of is refused when the attach group is
// configured, before any path is regrouped. ApplyAttachGroupAccess keeps its own
// message for the same case as a second line of defence.
BOOST_AUTO_TEST_CASE(attach_group_not_a_member_is_refused_at_configuration)
{
    AttachGroupReset reset;
#ifdef __linux__
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("running as root, which can set any group; skipping");
        return;
    }
    const std::optional<std::string> foreign = ForeignGroupName();
    if (!foreign) {
        BOOST_TEST_MESSAGE("no group this account is outside of; skipping");
        return;
    }
    BOOST_CHECK(!ipc::ConfigureAttachGroup(*foreign));
    BOOST_CHECK(!ipc::AttachGroupGid());
#else
    BOOST_TEST_MESSAGE("the membership check at configuration is Linux-only; skipping");
#endif
}

// The startup message names the group and the remedy. A group that does not
// exist keeps its own wording, and a group the core holds has no problem.
BOOST_AUTO_TEST_CASE(attach_group_problem_names_the_remedy)
{
    const std::optional<std::string> unknown = ipc::AttachGroupProblem("no-such-gridcoin-group-xyz");
    BOOST_REQUIRE(unknown);
    BOOST_CHECK(unknown->find("no-such-gridcoin-group-xyz") != std::string::npos);
    BOOST_CHECK(unknown->find("does not exist") != std::string::npos);
    BOOST_CHECK(unknown->find("SupplementaryGroups") == std::string::npos);

    const std::optional<std::string> own = GroupName(::getegid());
    if (own) BOOST_CHECK(!ipc::AttachGroupProblem(*own));

#ifdef __linux__
    if (::geteuid() != 0) {
        const std::optional<std::string> foreign = ForeignGroupName();
        if (foreign) {
            const std::optional<std::string> problem = ipc::AttachGroupProblem(*foreign);
            BOOST_REQUIRE(problem);
            BOOST_CHECK(problem->find("'" + *foreign + "'") != std::string::npos);
            BOOST_CHECK(problem->find("not a member") != std::string::npos);
            BOOST_CHECK(problem->find("SupplementaryGroups=" + *foreign) != std::string::npos);
            BOOST_CHECK(problem->find("service unit") != std::string::npos);
            BOOST_CHECK(problem->find("does not exist") == std::string::npos);
        } else {
            BOOST_TEST_MESSAGE("no group this account is outside of; skipping the membership part");
        }
    }
#endif
}

// A directory regrouped from 0770 to 0750 ends with the new group and without
// group write.
BOOST_AUTO_TEST_CASE(regroup_wide_directory_ends_narrow)
{
    const auto other = OtherMemberGroup();
    if (!other) {
        BOOST_TEST_MESSAGE("no supplementary group other than the primary one; skipping");
        return;
    }
    TempDir dir;
    BOOST_REQUIRE_EQUAL(::chmod(dir.path.c_str(), 0770), 0);
    ipc::ApplyPathAccessNoFollow(dir.path, ipc::PathKind::Directory, 0750, other);
    BOOST_CHECK_EQUAL(ModeOf(dir.path), static_cast<mode_t>(0750));
    BOOST_CHECK_EQUAL(GroupOf(dir.path), *other);
}

// The order of the steps cannot be seen from outside without a hook, so it is
// seen through a failure: when the group change is refused, the mode has
// already been narrowed to what the old and the final mode share, and the group
// is unchanged.
BOOST_AUTO_TEST_CASE(regroup_narrows_before_the_group_change)
{
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("running as root, which can set any group; skipping");
        return;
    }
    const auto foreign_name = ForeignGroupName();
    const struct group* gr = foreign_name ? ::getgrnam(foreign_name->c_str()) : nullptr;
    if (gr == nullptr) {
        BOOST_TEST_MESSAGE("no group this account is outside of; skipping");
        return;
    }
    TempDir dir;
    BOOST_REQUIRE_EQUAL(::chmod(dir.path.c_str(), 0770), 0);
    const gid_t before = GroupOf(dir.path);
    BOOST_CHECK_THROW(ipc::ApplyPathAccessNoFollow(dir.path, ipc::PathKind::Directory, 0750, gr->gr_gid),
                      std::system_error);
    BOOST_CHECK_EQUAL(ModeOf(dir.path), static_cast<mode_t>(0750));
    BOOST_CHECK_EQUAL(GroupOf(dir.path), before);
}

BOOST_AUTO_TEST_CASE(ipc_dir_warning_by_mode_and_group)
{
    const uid_t self = ::geteuid();
    const gid_t grp = 4242;
    // No access for group or others: nothing to say, in either mode.
    BOOST_CHECK(!ipc::IpcDirWarning("/d", 0700, self, grp, self, std::nullopt));
    BOOST_CHECK(!ipc::IpcDirWarning("/d", 0700, self, grp, self, grp));
    // The state ApplyAttachGroupAccess leaves: nothing to say.
    BOOST_CHECK(!ipc::IpcDirWarning("/d", 0750, self, grp, self, grp));
    // Without an attach group the socket and cookie are owner-only.
    auto w = ipc::IpcDirWarning("/d", 0750, self, grp, self, std::nullopt);
    BOOST_REQUIRE(w);
    BOOST_CHECK(w->find("owner-only") != std::string::npos);
    // With one, a directory that is not exactly the expected state is warned
    // about and the text does not claim owner-only.
    for (const auto& [mode, owner, group] : {std::tuple<mode_t, uid_t, gid_t>{0755, self, grp},
                                              {0770, self, grp},
                                              {0750, self, grp + 1},
                                              {0750, self + 1, grp}}) {
        w = ipc::IpcDirWarning("/d", mode, owner, group, self, grp);
        BOOST_REQUIRE(w);
        BOOST_CHECK(w->find("accessible to the attach group") != std::string::npos);
        BOOST_CHECK(w->find("owner-only") == std::string::npos);
    }
}

// PrepareIpcDirectory on a directory ApplyAttachGroupAccess has prepared logs no
// warning, and does log one for the same mode without the group.
BOOST_AUTO_TEST_CASE(prepare_ipc_directory_attach_group_state_is_quiet)
{
    AttachGroupReset reset;
    const auto name = GroupName(::getegid());
    if (!name) {
        BOOST_TEST_MESSAGE("the effective group has no entry in the group database; skipping");
        return;
    }
    TempDir dir;
    BOOST_REQUIRE_EQUAL(::chmod(dir.path.c_str(), 0750), 0);
    {
        CaptureLog log;
        ipc::PrepareIpcDirectory(dir.path);
        BOOST_CHECK(log.Contains("accessible to other users"));
    }
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*name));
    ipc::ApplyAttachGroupAccess(dir.path);
    {
        CaptureLog log;
        ipc::PrepareIpcDirectory(dir.path);
        BOOST_CHECK(!log.Contains("accessible to other users"));
    }
}

// With an attach group the daemon prepares its IPC directory with
// PrepareAttachGroupDirectory. An existing directory that gives other access is
// changed, not "left as configured", and the line logged says what it was. A
// second start, and bind()'s own check, find it as it should be and say
// nothing. A directory that does not exist yet is made and given the group's
// access.
BOOST_AUTO_TEST_CASE(attach_group_directory_logs_the_change_it_makes)
{
    AttachGroupReset reset;
    const auto name = GroupName(::getegid());
    if (!name) {
        BOOST_TEST_MESSAGE("the effective group has no entry in the group database; skipping");
        return;
    }
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*name));

    TempDir dir;
    BOOST_REQUIRE_EQUAL(::chmod(dir.path.c_str(), 0755), 0);
    {
        CaptureLog log;
        ipc::PrepareAttachGroupDirectory(dir.path);
        BOOST_CHECK(!log.Contains("Leaving it as configured"));
        BOOST_CHECK(!log.Contains("accessible to other users"));
        BOOST_CHECK_MESSAGE(log.Contains("it was mode 755"), "no line says what the directory was");
    }
    BOOST_CHECK_EQUAL(ModeOf(dir.path), static_cast<mode_t>(0750));
    BOOST_CHECK_EQUAL(GroupOf(dir.path), ::getegid());
    {
        CaptureLog log;
        ipc::PrepareAttachGroupDirectory(dir.path);
        ipc::PrepareIpcDirectory(dir.path);
        BOOST_CHECK(!log.Contains(dir.path.string()));
    }

    const fs::path fresh = dir.path / "fresh";
    {
        CaptureLog log;
        ipc::PrepareAttachGroupDirectory(fresh);
        BOOST_CHECK(!log.Contains(fresh.string()));
    }
    BOOST_CHECK_EQUAL(ModeOf(fresh), static_cast<mode_t>(0750));
    BOOST_CHECK_EQUAL(GroupOf(fresh), ::getegid());
}

// The attach group is given access to the IPC directory, so the core refuses an
// attach group without -ipcdir, whose default is the data directory. The text
// names both options. -noipcdir and an empty -ipcdir count as unset, as
// GetIpcDir() takes them, and -noipcattachgroup counts as no group.
BOOST_FIXTURE_TEST_CASE(attach_group_without_ipcdir_is_refused, grc_test::StateGuard)
{
    // Forced values would take precedence over the command line ones below.
    gArgs.ClearForcedArg("-ipcdir");
    gArgs.ClearForcedArg("-ipcattachgroup");
    BOOST_CHECK(!ipc::AttachGroupIpcDirRefusal());

    gArgs.ForceSetArg("-ipcattachgroup", "gridcoin");
    const std::optional<std::string> refusal = ipc::AttachGroupIpcDirRefusal();
    BOOST_REQUIRE(refusal);
    BOOST_CHECK_MESSAGE(refusal->find("-ipcattachgroup") != std::string::npos, *refusal);
    BOOST_CHECK_MESSAGE(refusal->find("-ipcdir") != std::string::npos, *refusal);

    gArgs.ForceSetArg("-ipcdir", "");
    BOOST_CHECK(ipc::AttachGroupIpcDirRefusal());

    gArgs.ForceSetArg("-ipcdir", "/run/gridcoinresearchd");
    BOOST_CHECK(!ipc::AttachGroupIpcDirRefusal());

    gArgs.ClearForcedArg("-ipcdir");
    gArgs.LockSettings([](util::Settings& settings) {
        util::SettingsValue negated;
        negated.setBool(false);
        settings.command_line_options["ipcdir"] = {negated};
    });
    BOOST_REQUIRE(gArgs.IsArgNegated("-ipcdir"));
    BOOST_CHECK(ipc::AttachGroupIpcDirRefusal());

    gArgs.ClearForcedArg("-ipcattachgroup");
    gArgs.LockSettings([](util::Settings& settings) {
        util::SettingsValue negated;
        negated.setBool(false);
        settings.command_line_options["ipcattachgroup"] = {negated};
    });
    BOOST_REQUIRE(gArgs.IsArgNegated("-ipcattachgroup"));
    BOOST_CHECK(!ipc::AttachGroupIpcDirRefusal());
}

// So is an -ipcdir that resolves to the data directory, however it is written:
// the path itself, "." (resolved against the data directory), or a link to it.
// On a test network the base data directory, above the network's own, counts
// too. Another directory, or one that does not exist yet, is not refused, and
// without an attach group the data directory is a valid -ipcdir.
BOOST_FIXTURE_TEST_CASE(attach_group_with_the_data_directory_as_ipcdir_is_refused, grc_test::StateGuard)
{
    gArgs.ClearForcedArg("-ipcdir");
    gArgs.ForceSetArg("-ipcattachgroup", "gridcoin");

    const fs::path datadir = GetDataDir();
    BOOST_REQUIRE(fs::is_directory(datadir));

    gArgs.ForceSetArg("-ipcdir", datadir.string());
    const std::optional<std::string> refusal = ipc::AttachGroupIpcDirRefusal();
    BOOST_REQUIRE(refusal);
    BOOST_CHECK_MESSAGE(refusal->find("-ipcattachgroup") != std::string::npos, *refusal);
    BOOST_CHECK_MESSAGE(refusal->find("-ipcdir") != std::string::npos, *refusal);
    BOOST_CHECK_MESSAGE(refusal->find("data directory") != std::string::npos, *refusal);

    gArgs.ForceSetArg("-ipcdir", ".");
    BOOST_CHECK(ipc::AttachGroupIpcDirRefusal());

    TempDir dir;
    const fs::path link = dir.path / "datadir-link";
    BOOST_REQUIRE_EQUAL(::symlink(datadir.c_str(), link.c_str()), 0);
    gArgs.ForceSetArg("-ipcdir", link.string());
    BOOST_CHECK(ipc::AttachGroupIpcDirRefusal());

    const fs::path base = GetDataDir(/*fNetSpecific=*/false);
    if (fs::canonical(base) != fs::canonical(datadir)) {
        gArgs.ForceSetArg("-ipcdir", base.string());
        BOOST_CHECK(ipc::AttachGroupIpcDirRefusal());
    }

    gArgs.ForceSetArg("-ipcdir", dir.path.string());
    BOOST_CHECK(!ipc::AttachGroupIpcDirRefusal());

    gArgs.ForceSetArg("-ipcdir", (dir.path / "not-yet").string());
    BOOST_CHECK(!ipc::AttachGroupIpcDirRefusal());

    gArgs.ClearForcedArg("-ipcattachgroup");
    gArgs.ForceSetArg("-ipcdir", datadir.string());
    BOOST_CHECK(!ipc::AttachGroupIpcDirRefusal());
}

// The attach group is never given access to the data directory, however
// -ipcdir reaches it. A spelling the canonical comparison cannot resolve, such
// as "<datadir>/x/.." with no x (or a bind mount, or another case on a
// case-insensitive filesystem), opens as the data directory itself. Once
// opened, it is refused by its device and inode before its mode or group is
// changed, and the text names both options. So is the data directory by its own
// path.
BOOST_AUTO_TEST_CASE(attach_group_directory_that_opens_as_the_data_directory_is_refused)
{
    AttachGroupReset reset;
    const auto name = GroupName(::getegid());
    if (!name) {
        BOOST_TEST_MESSAGE("the effective group has no entry in the group database; skipping");
        return;
    }
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*name));

    const fs::path datadir = GetDataDir();
    const fs::path missing = datadir / "ipc_process_tests_missing";
    fs::remove_all(missing);

    // What the comparison before start-up sees: a path it cannot resolve.
    boost::system::error_code resolve_ec;
    fs::canonical(missing / "..", resolve_ec);
    BOOST_CHECK(resolve_ec);

    struct stat before;
    BOOST_REQUIRE_EQUAL(::stat(datadir.c_str(), &before), 0);

    const auto refused_naming_both = [](const std::function<void()>& prepare) {
        try {
            prepare();
        } catch (const std::exception& e) {
            const std::string what = e.what();
            BOOST_CHECK_MESSAGE(what.find("-ipcattachgroup") != std::string::npos, what);
            BOOST_CHECK_MESSAGE(what.find("-ipcdir") != std::string::npos, what);
            return;
        }
        BOOST_ERROR("the data directory was not refused");
    };

    refused_naming_both([&] { ipc::PrepareAttachGroupDirectory(missing / ".."); });
    refused_naming_both([&] { ipc::ApplyAttachGroupAccess(datadir); });

    struct stat after;
    BOOST_REQUIRE_EQUAL(::stat(datadir.c_str(), &after), 0);
    BOOST_CHECK_EQUAL(after.st_mode & 07777, before.st_mode & 07777);
    BOOST_CHECK_EQUAL(after.st_gid, before.st_gid);

    // Put back whatever a failing run changed, for the cases after this one.
    ::chmod(datadir.c_str(), before.st_mode & 07777);
    if (::chown(datadir.c_str(), static_cast<uid_t>(-1), before.st_gid) != 0) {
        BOOST_TEST_MESSAGE("could not put the data directory's group back");
    }
    boost::system::error_code remove_ec;
    fs::remove_all(missing, remove_ec);
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
