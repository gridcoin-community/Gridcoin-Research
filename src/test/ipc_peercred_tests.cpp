// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include "fs.h"
#include "test/state_guard.h"
#include "util.h"

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <string>
#include <vector>

#ifndef WIN32
#include <grp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

//! The name of this process's effective group, which ConfigureAttachGroup can
//! always resolve.
std::string OwnGroupName()
{
    const struct group* gr = ::getgrgid(::getegid());
    BOOST_REQUIRE_MESSAGE(gr != nullptr, "the effective group has no entry in the group database");
    return gr->gr_name;
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
    BOOST_CHECK(ipc::ConfigureAttachGroup(OwnGroupName()));
    BOOST_CHECK(ipc::AttachGroupGid() == ::getegid());
    BOOST_CHECK(ipc::ConfigureAttachGroup(""));
    BOOST_CHECK(!ipc::AttachGroupGid());
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

    BOOST_REQUIRE(ipc::ConfigureAttachGroup(OwnGroupName()));
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

    // Unset: only this account, wherever the socket is.
    gArgs.ClearForcedArg("-ipcserviceuser");
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerPolicy(own_core, missing) == ipc::PeerVerdict::Allow);

    gArgs.ForceSetArg("-ipcserviceuser", "root");
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Allow);
    // A core at another uid is not the service account.
    BOOST_CHECK(ipc::ClientPeerPolicy(P(::geteuid() + 1, 0), "/") == ipc::PeerVerdict::Refuse);
    // A directory owned by this account, not by root or the service account.
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, dir.path) == ipc::PeerVerdict::Refuse);
    // lstat, not stat: a symlink to an acceptable directory is refused.
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, link) == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, missing) == ipc::PeerVerdict::Refuse);

    // An account that does not exist admits no other account.
    gArgs.ForceSetArg("-ipcserviceuser", "gridcoin-no-such-user-xyz");
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Refuse);
    BOOST_CHECK(ipc::ClientPeerPolicy(own_core, "/") == ipc::PeerVerdict::Allow);

    // -noipcserviceuser counts as unset.
    gArgs.ClearForcedArg("-ipcserviceuser");
    gArgs.LockSettings([](util::Settings& settings) {
        util::SettingsValue negated;
        negated.setBool(false);
        settings.command_line_options["ipcserviceuser"] = {negated};
    });
    BOOST_REQUIRE(gArgs.IsArgNegated("-ipcserviceuser"));
    BOOST_CHECK(ipc::ClientPeerPolicy(root_core, "/") == ipc::PeerVerdict::Refuse);
}

#else // WIN32

// Until the Windows peer check exists, the hooks keep the owner-only DACL
// behaviour: an empty identity, no attach group, and a refusal as soon as a
// different account is configured on either side.
BOOST_FIXTURE_TEST_CASE(windows_placeholder_admits_only_without_other_account, PeerPolicySetup)
{
    const auto id = ipc::ReadPeerIdentity(-1);
    BOOST_REQUIRE(id);
    BOOST_CHECK(ipc::ConfigureAttachGroup(""));
    BOOST_CHECK(!ipc::ConfigureAttachGroup("Gridcoin GUI Users"));

    gArgs.ClearForcedArg("-ipcattachgroup");
    gArgs.ClearForcedArg("-ipcserviceuser");
    BOOST_CHECK(ipc::ServerPeerPolicy(*id) == ipc::PeerVerdict::Allow);
    BOOST_CHECK(ipc::ClientPeerPolicy(*id, GetDataDir()) == ipc::PeerVerdict::Allow);

    gArgs.ForceSetArg("-ipcattachgroup", "Gridcoin GUI Users");
    BOOST_CHECK(ipc::ServerPeerPolicy(*id) == ipc::PeerVerdict::Refuse);
    gArgs.ForceSetArg("-ipcserviceuser", "NT SERVICE\\gridcoinresearchd");
    BOOST_CHECK(ipc::ClientPeerPolicy(*id, GetDataDir()) == ipc::PeerVerdict::Refuse);
}

#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
