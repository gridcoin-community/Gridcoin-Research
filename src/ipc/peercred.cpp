// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include "logging.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/system.h"

#ifndef WIN32
#include <algorithm>
#include <cerrno>
#include <pwd.h>
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

} // namespace

std::string ConfiguredOption(const std::string& arg)
{
    if (gArgs.IsArgNegated(arg)) return {};
    return gArgs.GetArg(arg, "");
}

fs::path SocketDirOf(const std::string& canonical_address)
{
    static const std::string prefix{"unix:"};
    if (canonical_address.size() <= prefix.size() || canonical_address.compare(0, prefix.size(), prefix) != 0) {
        return {};
    }
    return fs::path(canonical_address.substr(prefix.size())).parent_path();
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

// The Windows placeholders keep the owner-only DACL on node.sock and ipc.cookie
// as the guard: with no other account configured only this account (and SYSTEM)
// can open them. A configured other account cannot be checked yet, so it is
// refused rather than admitted unchecked.

PeerVerdict ServerPeerPolicy(const PeerIdentity&)
{
    if (ConfiguredOption("-ipcattachgroup").empty()) return PeerVerdict::Allow;
    LogPrintf("IPC: the Windows peer check for a different account is not available in this build\n");
    return PeerVerdict::Refuse;
}

PeerVerdict ClientPeerPolicy(const PeerIdentity&, const fs::path&)
{
    if (ConfiguredOption("-ipcserviceuser").empty()) return PeerVerdict::Allow;
    LogPrintf("IPC: the Windows peer check for a different account is not available in this build\n");
    return PeerVerdict::Refuse;
}

#endif // WIN32

const char* PeerCredentialEnforcement()
{
#ifdef WIN32
    return "none (Windows AF_UNIX exposes no peer credentials; the owner-only DACL on node.sock and ipc.cookie "
           "is the guard, and -ipcattachgroup is not available)";
#elif defined(__linux__)
    // peercred_posix.cpp reads SO_PEERCRED under the same __linux__ test.
    return "SO_PEERCRED (this OS user; with -ipcattachgroup also a non-root peer whose primary group is that "
           "group)";
#else
    return "getpeereid (this OS user; with -ipcattachgroup also a non-root peer whose primary group is that "
           "group)";
#endif
}

} // namespace ipc
