// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include "ipc/win_attest.h"
#include "logging.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/system.h"

#ifdef WIN32
#include "ipc/win_endpoint.h"
#include "util/win_acl.h"

#include <exception>
#endif

#ifndef WIN32
#include <algorithm>
#include <cerrno>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#endif

namespace ipc {
namespace {

#ifndef WIN32
//! The uid of the account \p name, or nullopt when there is no such account or
//! the account database cannot be read.
std::optional<uid_t> LookUpAccountUid(const std::string& name)
{
    const long hint = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    std::vector<char> buf(hint > 0 ? static_cast<size_t>(hint) : 1024);
    struct passwd pwd;
    struct passwd* result = nullptr;
    int err;
    while ((err = ::getpwnam_r(name.c_str(), &pwd, buf.data(), buf.size(), &result)) == ERANGE &&
           buf.size() < 1024 * 1024) {
        buf.resize(buf.size() * 2);
    }
    if (err != 0 || result == nullptr) return std::nullopt;
    return pwd.pw_uid;
}
#endif

//! A Windows account as a refusal names it.
std::string ShownSid(const std::string& sid)
{
    return sid.empty() ? std::string{"(none)"} : SanitizeString(sid);
}

} // namespace

std::string ConfiguredOption(const std::string& arg)
{
    return ::GetConfiguredOption(arg);
}

fs::path SocketDirOf(const std::string& canonical_address)
{
    static const std::string prefix{"unix:"};
    if (canonical_address.size() <= prefix.size() || canonical_address.compare(0, prefix.size(), prefix) != 0) {
        return {};
    }
    return fs::path(canonical_address.substr(prefix.size())).parent_path();
}

PeerVerdict ServerPeerDecision(const WinPeerIdentity& peer, const WinServerPeerContext& ctx)
{
    const auto refuse = [&](const std::string& rule) {
        LogPrintf("IPC: refusing the peer %s (this process runs as %s): %s", ShownSid(peer.sid),
                  ShownSid(ctx.self_sid), rule);
        return PeerVerdict::Refuse;
    };

    if (peer.sid.empty()) return refuse("its account was not read");
    if (peer.integrity_rid != win::INTEGRITY_MEDIUM_RID) {
        return refuse(strprintf("it runs at integrity level 0x%x, and only Medium (0x%x) is admitted",
                                peer.integrity_rid, win::INTEGRITY_MEDIUM_RID));
    }

    if (!ctx.self_sid.empty() && peer.sid == ctx.self_sid) return PeerVerdict::Allow;
    if (ctx.attach_group_configured && peer.attach_group_member) return PeerVerdict::Allow;

    return refuse(ctx.attach_group_configured
                      ? "not this process's account and not a member of the -ipcattachgroup group"
                      : "not this process's account, and no -ipcattachgroup is configured");
}

PeerVerdict ClientPeerDecision(const WinPeerIdentity& server, const std::string& expected_sid)
{
    if (!server.sid.empty() && server.sid == expected_sid) return PeerVerdict::Allow;

    LogPrintf("IPC: refusing the core %s: not the expected node account %s", ShownSid(server.sid),
              ShownSid(expected_sid));
    return PeerVerdict::Refuse;
}

#ifndef WIN32
bool PeerInGroup(const PeerIdentity& peer, gid_t gid)
{
    return peer.gid == gid || std::find(peer.groups.begin(), peer.groups.end(), gid) != peer.groups.end();
}

PeerVerdict ServerPeerDecision(const PeerIdentity& peer, const ServerPeerContext& ctx)
{
    if (peer.uid == ctx.self_uid) return PeerVerdict::Allow;

    if (peer.uid == 0) {
        LogPrintf("IPC: refusing peer uid 0 (this process runs as uid %u): root is not an IPC peer\n",
                  ctx.self_uid);
        return PeerVerdict::Refuse;
    }

    if (ctx.attach_gid && PeerInGroup(peer, *ctx.attach_gid)) return PeerVerdict::Allow;

    if (ctx.attach_gid) {
        LogPrintf("IPC: refusing peer uid %u (gid %u): not this process's uid %u and not in the "
                  "-ipcattachgroup group (gid %u)\n",
                  peer.uid, peer.gid, ctx.self_uid, *ctx.attach_gid);
    } else {
        LogPrintf("IPC: refusing peer uid %u: not this process's uid %u, and no -ipcattachgroup is "
                  "configured\n",
                  peer.uid, ctx.self_uid);
    }
    return PeerVerdict::Refuse;
}

PeerVerdict ClientPeerDecision(const PeerIdentity& server, const ClientPeerContext& ctx)
{
    if (server.uid == ctx.self_uid) return PeerVerdict::Allow;

    const auto refuse = [&](const std::string& rule) {
        LogPrintf("IPC: refusing the core at uid %u (this process runs as uid %u): %s\n",
                  server.uid, ctx.self_uid, rule);
        return PeerVerdict::Refuse;
    };

    if (!ctx.service_uid) {
        return refuse("another account, and no -ipcserviceuser is configured");
    }
    if (server.uid != *ctx.service_uid) {
        return refuse(strprintf("not the -ipcserviceuser account (uid %u)", *ctx.service_uid));
    }

    const IpcDirFacts& dir = ctx.dir;
    if (!dir.exists || !dir.is_directory || dir.is_symlink) {
        return refuse("the IPC directory is missing, not a directory, or a symlink");
    }
    if (dir.owner != *ctx.service_uid && dir.owner != 0) {
        return refuse(strprintf("the IPC directory is owned by uid %u, neither the service account nor root",
                                dir.owner));
    }
    if ((dir.mode & 022) != 0) {
        return refuse(strprintf("the IPC directory is writable by group or others (mode %04o)", dir.mode));
    }
    return PeerVerdict::Allow;
}

PeerVerdict ServerPeerPolicy(const PeerIdentity& peer)
{
    return ServerPeerDecision(peer, ServerPeerContext{::geteuid(), AttachGroupGid()});
}

PeerVerdict ClientPeerPolicy(const PeerIdentity& server, const fs::path& ipcdir)
{
    ClientPeerContext ctx;
    ctx.self_uid = ::geteuid();

    // A core at this account's own uid is allowed whatever else is configured,
    // so the common connect needs neither the account lookup nor the lstat.
    if (server.uid == ctx.self_uid) return ClientPeerDecision(server, ctx);

    const std::string service_user = ConfiguredOption("-ipcserviceuser");
    if (!service_user.empty()) {
        ctx.service_uid = LookUpAccountUid(service_user);
        // Said here rather than by ClientPeerDecision, which would report the
        // account as not configured.
        if (!ctx.service_uid) {
            LogPrintf("IPC: refusing the core at uid %u (this process runs as uid %u): the -ipcserviceuser "
                      "account '%s' does not exist\n",
                      server.uid, ctx.self_uid, SanitizeString(service_user));
            return PeerVerdict::Refuse;
        }
    }

    // lstat, so a symlink at the directory itself is seen as one. Any failure
    // leaves the facts at "does not exist", which refuses another account.
    struct stat st;
    if (::lstat(ipcdir.c_str(), &st) == 0) {
        ctx.dir.exists = true;
        ctx.dir.is_directory = S_ISDIR(st.st_mode);
        ctx.dir.is_symlink = S_ISLNK(st.st_mode);
        ctx.dir.owner = st.st_uid;
        ctx.dir.mode = st.st_mode & 07777;
    }

    return ClientPeerDecision(server, ctx);
}

#else // WIN32

// The DACLs on node.sock and ipc.cookie decide who may connect (ipc/win_endpoint.h).
// Who the peer is comes from the identity check on the connection
// (ipc/win_attest.h), in the same-account mode and the service mode alike.

PeerVerdict ServerPeerPolicy(const PeerIdentity& peer)
{
    WinServerPeerContext ctx;
    try {
        ctx.self_sid = util::winacl::FormatSid(util::winacl::CurrentTokenUser());
    } catch (const std::exception& e) {
        LogPrintf("IPC: refusing the peer %s: this process's account cannot be read (%s)", ShownSid(peer.sid),
                  e.what());
        return PeerVerdict::Refuse;
    }
    ctx.attach_group_configured = win::AttachGroupSid().has_value();

    return ServerPeerDecision(peer, ctx);
}

PeerVerdict ClientPeerPolicy(const PeerIdentity& server, const fs::path&)
{
    std::string expected;
    try {
        expected = util::winacl::FormatSid(win::ExpectedServerSid());
    } catch (const std::exception& e) {
        LogPrintf("IPC: refusing the core %s: the node account to expect cannot be determined (%s)",
                  ShownSid(server.sid), e.what());
        return PeerVerdict::Refuse;
    }

    return ClientPeerDecision(server, expected);
}

#endif // WIN32

const char* PeerCredentialEnforcement()
{
#ifdef WIN32
    return "the named-pipe identity check before authentication (Windows AF_UNIX exposes no peer credentials; the "
           "peer's token is read through the pipe at the identification level, and this account, or with "
           "-ipcattachgroup also a member of that group, is admitted at Medium integrity only). The DACLs on "
           "node.sock and ipc.cookie decide who may connect and read the cookie";
#elif defined(__linux__)
    // peercred_posix.cpp reads SO_PEERCRED under the same __linux__ test.
#ifdef SO_PEERGROUPS
    return "SO_PEERCRED and SO_PEERGROUPS (this OS user; with -ipcattachgroup also a non-root peer whose primary "
           "or supplementary group is that group)";
#else
    return "SO_PEERCRED (this OS user; with -ipcattachgroup also a non-root peer whose primary group is that "
           "group; supplementary attach-group membership is unavailable on this build)";
#endif
#else
    return "getpeereid (this OS user; with -ipcattachgroup also a non-root peer whose primary group is that "
           "group)";
#endif
}

} // namespace ipc
