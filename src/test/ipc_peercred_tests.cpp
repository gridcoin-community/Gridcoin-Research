// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include "fs.h"
#include "test/state_guard.h"
#include "util.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#ifdef WIN32
#include "util/win_acl.h"
#endif

#ifndef WIN32
#include <grp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#endif

namespace {

//! Puts gArgs back (StateGuard) and clears the configured attach group when a
//! case ends, on every exit path.
struct PeerPolicySetup : grc_test::StateGuard
{
    ~PeerPolicySetup() { ipc::ConfigureAttachGroup(""); }
};

#ifndef WIN32
ipc::PeerIdentity P(uid_t u, gid_t g, std::vector<gid_t> gs = {})
{
    ipc::PeerIdentity p;
    p.uid = u;
    p.gid = g;
    p.groups = gs;
    return p;
}

//! The name of this process's effective group, or nullopt, with a message, when
//! the group database has no entry for it. The cases that need a group
//! ConfigureAttachGroup can resolve skip in that case.
std::optional<std::string> OwnGroupName()
{
    const struct group* gr = ::getgrgid(::getegid());
    if (gr == nullptr) {
        BOOST_TEST_MESSAGE("the effective group has no entry in the group database; skipping");
        return std::nullopt;
    }
    return std::string{gr->gr_name};
}

//! Make -no<name> the setting of \p arg ("-ipcserviceuser" and so on), the way
//! -noipcserviceuser on the command line does.
void NegateArg(const std::string& arg)
{
    gArgs.ClearForcedArg(arg);
    gArgs.LockSettings([&](util::Settings& settings) {
        util::SettingsValue negated;
        negated.setBool(false);
        settings.command_line_options[arg.substr(1)] = {negated};
    });
    BOOST_REQUIRE(gArgs.IsArgNegated(arg));
}

//! A mkdtemp directory removed when the case ends.
struct TempDir
{
    fs::path path;

    TempDir()
    {
        char tmpl[] = "/tmp/ipcpeer_XXXXXX";
        BOOST_REQUIRE(::mkdtemp(tmpl) != nullptr);
        path = fs::path(tmpl);
    }

    ~TempDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};
#endif

#ifdef SO_PEERGROUPS
//! Whether this process can read SO_PEERGROUPS. qemu-user hands the option to the
//! kernel with room for one int, so a process with supplementary groups gets ERANGE
//! whatever size it asks for; a real kernel never does for room for every group.
bool PeerGroupsReadable()
{
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) return true;
    std::vector<gid_t> groups(static_cast<size_t>(std::max(::sysconf(_SC_NGROUPS_MAX), 1L)));
    socklen_t len = static_cast<socklen_t>(groups.size() * sizeof(gid_t));
    const int result = ::getsockopt(fds[0], SOL_SOCKET, SO_PEERGROUPS, groups.data(), &len);
    const int err = errno;
    ::close(fds[0]);
    ::close(fds[1]);
    return result == 0 || err != ERANGE;
}
#endif

//! A Windows peer's identity, as the rules read it.
ipc::WinPeerIdentity WinPeer(std::string sid, uint32_t integrity_rid, bool attach_group_member)
{
    ipc::WinPeerIdentity peer;
    peer.sid = std::move(sid);
    peer.integrity_rid = integrity_rid;
    peer.attach_group_member = attach_group_member;
    return peer;
}

//! SECURITY_MANDATORY_LOW_RID, MEDIUM_RID, MEDIUM_PLUS_RID and HIGH_RID.
constexpr uint32_t LOW_IL = 0x1000;
constexpr uint32_t MEDIUM_IL = 0x2000;
constexpr uint32_t MEDIUM_PLUS_IL = 0x2100;
constexpr uint32_t HIGH_IL = 0x3000;

} // namespace

BOOST_AUTO_TEST_SUITE(ipc_peercred_tests)

// The daemon logs this at startup, so an operator can see which mechanism reads
// the peer's identity and that -ipcattachgroup is the only widening of it.
BOOST_AUTO_TEST_CASE(peercred_enforcement_is_described)
{
    const std::string mode = ipc::PeerCredentialEnforcement();
#ifdef WIN32
    BOOST_CHECK(mode.find("DACL") != std::string::npos);
#elif defined(__linux__)
    BOOST_CHECK(mode.find("SO_PEERCRED") != std::string::npos);
#else
    BOOST_CHECK(mode.find("getpeereid") != std::string::npos);
#endif
    BOOST_CHECK(mode.find("-ipcattachgroup") != std::string::npos);
}

#ifndef WIN32

BOOST_AUTO_TEST_CASE(server_same_uid_allowed)
{
    BOOST_CHECK(ipc::ServerPeerDecision(P(1000, 1000), {1000, std::nullopt}) == ipc::PeerVerdict::Allow);
}

BOOST_AUTO_TEST_CASE(server_other_uid_refused_without_group)
{
    BOOST_CHECK(ipc::ServerPeerDecision(P(1001, 1001), {1000, std::nullopt}) == ipc::PeerVerdict::Refuse);
}

BOOST_AUTO_TEST_CASE(server_attach_group_member_allowed)
{
    BOOST_CHECK(ipc::ServerPeerDecision(P(1001, 1001, {5000}), {999, gid_t{5000}}) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerDecision(P(1001, 5000), {999, gid_t{5000}}) == ipc::PeerVerdict::Allow);
}

BOOST_AUTO_TEST_CASE(server_root_refused_even_in_group)
{
    BOOST_CHECK(ipc::ServerPeerDecision(P(0, 0, {5000}), {999, gid_t{5000}}) == ipc::PeerVerdict::Refuse);
}

// The same-uid rule is applied before the root refusal, so a core running as
// root serves a root peer of its own uid.
BOOST_AUTO_TEST_CASE(server_root_peer_of_root_core_allowed)
{
    BOOST_CHECK(ipc::ServerPeerDecision(P(0, 0), {0, std::nullopt}) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerDecision(P(0, 0), {0, gid_t{5000}}) == ipc::PeerVerdict::Allow);
}

BOOST_AUTO_TEST_CASE(client_same_uid_allowed_any_dir)
{
    BOOST_CHECK(ipc::ClientPeerDecision(P(1000, 1000), {1000, std::nullopt, {}}) == ipc::PeerVerdict::Allow);
}

BOOST_AUTO_TEST_CASE(client_service_uid_needs_configuration)
{
    ipc::IpcDirFacts d{true, true, false, 999, 0750};
    BOOST_CHECK(ipc::ClientPeerDecision(P(999, 999), {1000, std::nullopt, d}) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerDecision(P(999, 999), {1000, uid_t{999}, d}) == ipc::PeerVerdict::Allow);
}

BOOST_AUTO_TEST_CASE(client_service_dir_must_be_owned_and_not_writable)
{
    const ipc::ClientPeerContext base{1000, uid_t{999}, {true, true, false, 999, 0750}};
    auto with = [&](auto f) {
        auto c = base;
        f(c.dir);
        return ipc::ClientPeerDecision(P(999, 999), c);
    };
    BOOST_CHECK(with([](auto& d) { d.owner = 0; }) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(with([](auto& d) { d.owner = 1234; }) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(with([](auto& d) { d.mode = 0770; }) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(with([](auto& d) { d.mode = 0752; }) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(with([](auto& d) { d.is_directory = false; }) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(with([](auto& d) { d.is_symlink = true; }) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(with([](auto& d) { d.exists = false; }) == ipc::PeerVerdict::Refuse);
}

BOOST_AUTO_TEST_CASE(client_wrong_service_uid_refused)
{
    BOOST_CHECK(ipc::ClientPeerDecision(P(998, 998), {1000, uid_t{999}, {true, true, false, 999, 0750}}) ==
                ipc::PeerVerdict::Refuse);
}

// Both ends of a socketpair belong to this process, so each end reads back
// this process's own identity.
BOOST_AUTO_TEST_CASE(read_peer_identity_socketpair_is_self)
{
#ifdef SO_PEERGROUPS
    if (!PeerGroupsReadable()) {
        BOOST_TEST_MESSAGE("SO_PEERGROUPS cannot be read here (qemu-user gives it room for one int); skipping");
        return;
    }
#endif
    int sv[2];
    BOOST_REQUIRE_EQUAL(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    for (const int fd : sv) {
        auto id = ipc::ReadPeerIdentity(fd);
        BOOST_CHECK(id);
        if (!id) continue;
        BOOST_CHECK_EQUAL(id->uid, ::geteuid());
        BOOST_CHECK_EQUAL(id->gid, ::getegid());
        BOOST_CHECK(ipc::ServerPeerPolicy(*id) == ipc::PeerVerdict::Allow);
    }
    ::close(sv[0]);
    ::close(sv[1]);
}

// A peer that cannot be read is nullopt, which every caller refuses: no fd at
// all, a regular file and a pipe.
BOOST_AUTO_TEST_CASE(read_peer_identity_bad_fd_is_nullopt)
{
    BOOST_CHECK(!ipc::ReadPeerIdentity(-1));

    std::FILE* file = std::tmpfile();
    BOOST_REQUIRE(file != nullptr);
    BOOST_CHECK(!ipc::ReadPeerIdentity(::fileno(file)));
    std::fclose(file);

    int fds[2];
    BOOST_REQUIRE_EQUAL(::pipe(fds), 0);
    BOOST_CHECK(!ipc::ReadPeerIdentity(fds[0]));
    ::close(fds[0]);
    ::close(fds[1]);
}

BOOST_FIXTURE_TEST_CASE(configure_attach_group, PeerPolicySetup)
{
    BOOST_CHECK(!ipc::ConfigureAttachGroup("gridcoin-no-such-group-xyz"));
    const auto group_name = OwnGroupName();
    if (!group_name) return;
    BOOST_CHECK(ipc::ConfigureAttachGroup(*group_name));
    BOOST_CHECK(ipc::AttachGroupGid() == ::getegid());
    BOOST_CHECK(ipc::ConfigureAttachGroup(""));
    BOOST_CHECK(!ipc::AttachGroupGid());
}

#ifndef WIN32
// Kernels before 4.13 define SO_PEERGROUPS in the headers but answer
// ENOPROTOOPT. Only that errno leaves the groups empty; every other failure
// refuses the peer.
BOOST_AUTO_TEST_CASE(peer_groups_error_action)
{
    BOOST_CHECK(ipc::PeerGroupsErrorAction(ENOPROTOOPT) == ipc::PeerGroupsError::EmptyGroups);
    for (const int err : {EPERM, EBADF, EFAULT, EINVAL, ENOMEM, ERANGE, ENOTSOCK, EOPNOTSUPP}) {
        BOOST_CHECK_MESSAGE(ipc::PeerGroupsErrorAction(err) == ipc::PeerGroupsError::Refuse, "errno " << err);
    }
}
#endif

#ifdef __linux__
// The supplementary groups of the peer come from SO_PEERGROUPS, so on a
// socketpair the peer is this process and its groups are getgroups().
BOOST_AUTO_TEST_CASE(peer_identity_reads_groups)
{
#ifdef SO_PEERGROUPS
    if (!PeerGroupsReadable()) {
        BOOST_TEST_MESSAGE("SO_PEERGROUPS cannot be read here (qemu-user gives it room for one int); skipping");
        return;
    }
#endif
    int fds[2];
    BOOST_REQUIRE_EQUAL(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    const auto id = ipc::ReadPeerIdentity(fds[0]);
    ::close(fds[0]);
    ::close(fds[1]);
    BOOST_REQUIRE(id);
    BOOST_CHECK_EQUAL(id->uid, ::geteuid());
    BOOST_CHECK_EQUAL(id->gid, ::getegid());

    const int count = ::getgroups(0, nullptr);
    BOOST_REQUIRE(count >= 0);
    std::vector<gid_t> expected(static_cast<size_t>(count));
    if (count > 0) {
        BOOST_REQUIRE_EQUAL(::getgroups(count, expected.data()), count);
    }
    std::vector<gid_t> actual = id->groups;
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());
    BOOST_CHECK(actual == expected);
}

// A group the process belongs to only as a supplementary group can be the
// attach group, and a peer in that group is admitted through it.
BOOST_FIXTURE_TEST_CASE(attach_group_admits_member_by_supplementary_group, PeerPolicySetup)
{
    const int count = ::getgroups(0, nullptr);
    BOOST_REQUIRE(count >= 0);
    std::vector<gid_t> held(static_cast<size_t>(count));
    if (count > 0) {
        BOOST_REQUIRE_EQUAL(::getgroups(count, held.data()), count);
    }
    for (gid_t gid : held) {
        const struct group* gr = ::getgrgid(gid);
        if (gr == nullptr || gid == ::getegid()) continue;
        BOOST_REQUIRE(ipc::ConfigureAttachGroup(gr->gr_name));
        BOOST_CHECK(ipc::ServerPeerPolicy(P(::geteuid() + 1, 12345, {gid})) == ipc::PeerVerdict::Allow);
        BOOST_CHECK(ipc::ServerPeerPolicy(P(::geteuid() + 1, 12345, {12346})) == ipc::PeerVerdict::Refuse);
        return;
    }
    BOOST_TEST_MESSAGE("this process has no named supplementary group; skipping");
}

// The core must hold the attach group itself, or the service unit is missing
// SupplementaryGroups=.
BOOST_FIXTURE_TEST_CASE(configure_rejects_unknown_and_foreign_groups, PeerPolicySetup)
{
    BOOST_CHECK(!ipc::ConfigureAttachGroup("grc-no-such-group-xyz"));
    BOOST_CHECK(!ipc::AttachGroupGid());

    const int count = ::getgroups(0, nullptr);
    BOOST_REQUIRE(count >= 0);
    std::vector<gid_t> held(static_cast<size_t>(count));
    if (count > 0) {
        BOOST_REQUIRE_EQUAL(::getgroups(count, held.data()), count);
    }
    held.push_back(::getegid());

    ::setgrent();
    std::optional<std::string> foreign;
    while (const struct group* gr = ::getgrent()) {
        if (std::find(held.begin(), held.end(), gr->gr_gid) == held.end()) {
            foreign = std::string{gr->gr_name};
            break;
        }
    }
    ::endgrent();
    if (!foreign) {
        BOOST_TEST_MESSAGE("no group in /etc/group is foreign to this process; skipping");
        return;
    }
    BOOST_CHECK(!ipc::ConfigureAttachGroup(*foreign));
    BOOST_CHECK(!ipc::AttachGroupGid());
}
#endif

// ConfiguredOption is the one rule for a name-valued option: unset and negated
// both read as "". Without the negation branch GetArg returns "0" for the
// negated form.
BOOST_FIXTURE_TEST_CASE(configured_option_unset_set_and_negated, PeerPolicySetup)
{
    gArgs.ClearForcedArg("-ipcserviceuser");
    BOOST_CHECK_EQUAL(ipc::ConfiguredOption("-ipcserviceuser"), "");
    gArgs.ForceSetArg("-ipcserviceuser", "svc");
    BOOST_CHECK_EQUAL(ipc::ConfiguredOption("-ipcserviceuser"), "svc");
    NegateArg("-ipcserviceuser");
    BOOST_CHECK_EQUAL(ipc::ConfiguredOption("-ipcserviceuser"), "");
}

// The daemon configures the attach group from ConfiguredOption, so
// -noipcattachgroup configures no group (and does not look for one named "0").
BOOST_FIXTURE_TEST_CASE(negated_attach_group_configures_no_group, PeerPolicySetup)
{
    NegateArg("-ipcattachgroup");
    BOOST_CHECK(ipc::ConfigureAttachGroup(ipc::ConfiguredOption("-ipcattachgroup")));
    BOOST_CHECK(!ipc::AttachGroupGid());
}

// connectAddress takes the directory it checks from the canonical address.
BOOST_AUTO_TEST_CASE(socket_dir_of_canonical_address)
{
    BOOST_CHECK(ipc::SocketDirOf("unix:/a/b/node.sock") == fs::path("/a/b"));
    BOOST_CHECK(ipc::SocketDirOf("unix:/node.sock") == fs::path("/"));
    BOOST_CHECK(ipc::SocketDirOf("unix").empty());
    BOOST_CHECK(ipc::SocketDirOf("unix:").empty());
    BOOST_CHECK(ipc::SocketDirOf("tcp:/a/b/node.sock").empty());
    BOOST_CHECK(ipc::SocketDirOf("").empty());
}

// ServerPeerPolicy decides with this process's uid and the configured attach
// group.
BOOST_FIXTURE_TEST_CASE(server_policy_uses_own_uid_and_attach_group, PeerPolicySetup)
{
    const uid_t self = ::geteuid();
    const uid_t other = self + 1;
    const gid_t group = ::getegid();

    BOOST_REQUIRE(ipc::ConfigureAttachGroup(""));
    BOOST_CHECK(ipc::ServerPeerPolicy(P(self, group)) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerPolicy(P(other, group)) == ipc::PeerVerdict::Refuse);

    const auto group_name = OwnGroupName();
    if (!group_name) return;
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(*group_name));
    BOOST_CHECK(ipc::ServerPeerPolicy(P(other, group)) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerPolicy(P(other, group + 1)) == ipc::PeerVerdict::Refuse);
    if (self != 0) {
        BOOST_CHECK(ipc::ServerPeerPolicy(P(0, group)) == ipc::PeerVerdict::Refuse);
    }
}

// ClientPeerPolicy resolves -ipcserviceuser and reads the IPC directory with
// lstat on each call. root and "/" stand in for a service account and its
// directory, because every POSIX system has both.
BOOST_FIXTURE_TEST_CASE(client_policy_reads_service_user_and_ipcdir, PeerPolicySetup)
{
    if (::geteuid() == 0) {
        BOOST_TEST_MESSAGE("running as root, so uid 0 is this account; skipping");
        return;
    }
    struct stat root_dir;
    BOOST_REQUIRE_EQUAL(::lstat("/", &root_dir), 0);
    if (root_dir.st_uid != 0 || (root_dir.st_mode & 022) != 0) {
        BOOST_TEST_MESSAGE("/ is not owned by root with mode 0755 or tighter; skipping");
        return;
    }

    const ipc::PeerIdentity root_core = P(0, 0);
    const ipc::PeerIdentity own_core = P(::geteuid(), ::getegid());
    TempDir dir;
    const fs::path missing = dir.path / "missing";
    const fs::path link = dir.path / "link";
    BOOST_REQUIRE_EQUAL(::symlink("/", link.c_str()), 0);

    // With it unset, only this account is admitted, wherever the socket is.
    gArgs.ClearForcedArg("-ipcserviceuser");
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerPolicy(own_core, missing) == ipc::PeerVerdict::Allow);

    gArgs.ForceSetArg("-ipcserviceuser", "root");
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Allow);
    // A core at another uid is not the service account.
    BOOST_CHECK(ipc::ClientPeerPolicy(P(::geteuid() + 1, 0), "/") == ipc::PeerVerdict::Refuse);
    // A directory owned by this account, not by root or the service account.
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, dir.path) == ipc::PeerVerdict::Refuse);
    // A symlink to an acceptable directory is refused, because the directory is
    // read with lstat and not stat.
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, link) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, missing) == ipc::PeerVerdict::Refuse);

    // An account that does not exist admits no other account.
    gArgs.ForceSetArg("-ipcserviceuser", "gridcoin-no-such-user-xyz");
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerPolicy(own_core, "/") == ipc::PeerVerdict::Allow);

    // -noipcserviceuser counts as unset.
    NegateArg("-ipcserviceuser");
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Refuse);
}

#else // WIN32

// kj gives the server no Windows socket, so no identity is read from one, and an
// identity that was never read is refused by both policies, whatever is
// configured.
BOOST_FIXTURE_TEST_CASE(peercred_read_identity_from_fd_is_nullopt, PeerPolicySetup)
{
    BOOST_CHECK(!ipc::ReadPeerIdentity(-1));
    BOOST_CHECK(!ipc::ConfigureAttachGroup("gridcoin-test-no-such-group"));

    gArgs.ClearForcedArg("-ipcattachgroup");
    gArgs.ClearForcedArg("-ipcserviceuser");
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(""));
    const ipc::PeerIdentity unread;
    BOOST_CHECK(ipc::ServerPeerPolicy(unread) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer("", MEDIUM_IL, true)) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerPolicy(unread, GetDataDir()) == ipc::PeerVerdict::Refuse);
}

// ServerPeerPolicy decides with this process's token user and the configured
// attach group, at Medium integrity only. The group must be a local group of this
// computer; the case for one runs where "Gridcoin GUI Users" exists, and
// windows_server_peer_decision covers the rule everywhere.
BOOST_FIXTURE_TEST_CASE(server_policy_windows_fields, PeerPolicySetup)
{
    const std::string self = util::winacl::FormatSid(util::winacl::CurrentTokenUser());
    // An account of a domain that does not exist, so never this one.
    const std::string other = "S-1-5-21-1-2-3-1000";

    gArgs.ClearForcedArg("-ipcattachgroup");
    BOOST_REQUIRE(ipc::ConfigureAttachGroup(""));
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(self, MEDIUM_IL, false)) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(self, HIGH_IL, false)) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(self, LOW_IL, false)) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(other, MEDIUM_IL, true)) == ipc::PeerVerdict::Refuse);

    if (!ipc::ConfigureAttachGroup("Gridcoin GUI Users")) {
        BOOST_TEST_MESSAGE("this computer has no local group \"Gridcoin GUI Users\"; skipping the attach-group case");
        return;
    }
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(other, MEDIUM_IL, true)) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(other, MEDIUM_IL, false)) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(other, HIGH_IL, true)) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerPolicy(WinPeer(self, MEDIUM_IL, false)) == ipc::PeerVerdict::Allow);
}

// ClientPeerPolicy admits only the node account the GUI expects: this account
// without -ipcserviceuser, and with it that account alone, so an account that
// does not resolve admits no core at all.
BOOST_FIXTURE_TEST_CASE(client_policy_windows_expected_server, PeerPolicySetup)
{
    const std::string self = util::winacl::FormatSid(util::winacl::CurrentTokenUser());

    gArgs.ClearForcedArg("-ipcserviceuser");
    BOOST_CHECK(ipc::ClientPeerPolicy(WinPeer(self, 0, false), GetDataDir()) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ClientPeerPolicy(WinPeer("S-1-5-21-1-2-3-1000", 0, false), GetDataDir()) ==
                ipc::PeerVerdict::Refuse);

    gArgs.ForceSetArg("-ipcserviceuser", "NT SERVICE\\gridcoin-test-no-such-service");
    BOOST_CHECK(ipc::ClientPeerPolicy(WinPeer(self, 0, false), GetDataDir()) == ipc::PeerVerdict::Refuse);
}

#endif // WIN32

// The core's rule for a Windows peer, which builds everywhere. Exactly Medium
// integrity, and then this account or, with an attach group, a member of it.
BOOST_AUTO_TEST_CASE(windows_server_peer_decision)
{
    const std::string self = "S-1-5-21-1004336348-1177238915-682003330-1001";
    const std::string other = "S-1-5-21-1004336348-1177238915-682003330-1002";
    const ipc::WinServerPeerContext no_group{self, false};
    const ipc::WinServerPeerContext group{self, true};

    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer(self, MEDIUM_IL, false), no_group) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer(self, MEDIUM_IL, false), group) == ipc::PeerVerdict::Allow);

    // Any level but Medium is refused, this account's own included: elevated or
    // uiAccess above it, sandboxed below it.
    for (const uint32_t rid : {uint32_t{0}, LOW_IL, MEDIUM_IL - 1, MEDIUM_PLUS_IL, HIGH_IL, uint32_t{0x4000}}) {
        BOOST_CHECK_MESSAGE(ipc::ServerPeerDecision(WinPeer(self, rid, false), no_group) == ipc::PeerVerdict::Refuse,
                            "integrity 0x" << std::hex << rid);
        BOOST_CHECK_MESSAGE(ipc::ServerPeerDecision(WinPeer(other, rid, true), group) == ipc::PeerVerdict::Refuse,
                            "integrity 0x" << std::hex << rid);
    }

    // Another account only as a member of a configured attach group.
    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer(other, MEDIUM_IL, true), group) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer(other, MEDIUM_IL, true), no_group) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer(other, MEDIUM_IL, false), group) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer(other, MEDIUM_IL, false), no_group) == ipc::PeerVerdict::Refuse);

    // An identity that was never read, even one that claims the group, and a
    // context with no account of its own match nobody.
    BOOST_CHECK(ipc::ServerPeerDecision(ipc::WinPeerIdentity{}, no_group) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer("", MEDIUM_IL, true), group) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ServerPeerDecision(WinPeer("", MEDIUM_IL, false), {"", false}) == ipc::PeerVerdict::Refuse);
}

// The GUI's rule for a Windows core: its account is the one expected, and an empty
// one on either side matches nothing.
BOOST_AUTO_TEST_CASE(windows_client_peer_decision)
{
    const std::string expected = "S-1-5-80-1234-5678-9012-3456-7890";

    BOOST_CHECK(ipc::ClientPeerDecision(WinPeer(expected, 0, false), expected) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ClientPeerDecision(WinPeer("S-1-5-21-1-2-3-1000", 0, false), expected) ==
                ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerDecision(ipc::WinPeerIdentity{}, expected) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerDecision(ipc::WinPeerIdentity{}, "") == ipc::PeerVerdict::Refuse);
}

BOOST_AUTO_TEST_SUITE_END()
