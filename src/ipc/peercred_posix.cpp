// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// POSIX backend of the peer-check hooks declared in ipc/peercred.h. The build
// lists it on every platform; on Windows it compiles to nothing.

#include "ipc/peercred.h"

#ifndef WIN32

#include "logging.h"
#include "sync.h"
#include "util/strencodings.h"
#include "tinyformat.h"

#include <algorithm>
#include <cerrno>
#include <grp.h>
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace ipc {
namespace {

//! The attach group ConfigureAttachGroup resolved. A leaf lock: nothing is
//! called while it is held.
Mutex g_attach_group_mutex;
std::optional<gid_t> g_attach_gid GUARDED_BY(g_attach_group_mutex);

//! The gid of the group \p name, or nullopt when there is no such group or the
//! group database cannot be read.
std::optional<gid_t> LookUpGroupGid(const std::string& name)
{
    const long hint = ::sysconf(_SC_GETGR_R_SIZE_MAX);
    std::vector<char> buf(hint > 0 ? static_cast<size_t>(hint) : 1024);
    struct group grp;
    struct group* result = nullptr;
    int err;
    while ((err = ::getgrnam_r(name.c_str(), &grp, buf.data(), buf.size(), &result)) == ERANGE &&
           buf.size() < 1024 * 1024) {
        buf.resize(buf.size() * 2);
    }
    if (err != 0) {
        LogPrintf("IPC: the group database lookup for the attach group failed (errno %d)", err);
        return std::nullopt;
    }
    if (result == nullptr) {
        LogPrintf("IPC: the attach group is not in the group database");
        return std::nullopt;
    }
    return grp.gr_gid;
}

#ifdef __linux__
//! Whether this process holds \p gid as its effective or a supplementary group.
bool ProcessHoldsGroup(gid_t gid)
{
    if (::getegid() == gid) return true;
    // The group list can change between the two calls, so retry once if it grew.
    for (int attempt = 0; attempt < 2; ++attempt) {
        const int count = ::getgroups(0, nullptr);
        if (count < 0) return false;
        std::vector<gid_t> held(static_cast<size_t>(count) + 1);
        const int got = ::getgroups(static_cast<int>(held.size()), held.data());
        if (got < 0) continue;
        held.resize(static_cast<size_t>(got));
        return std::find(held.begin(), held.end(), gid) != held.end();
    }
    return false;
}
#endif

} // namespace

PeerGroupsError PeerGroupsErrorAction(int err)
{
    return err == ENOPROTOOPT ? PeerGroupsError::EmptyGroups : PeerGroupsError::Refuse;
}

std::optional<PeerIdentity> ReadPeerIdentity(int fd)
{
    if (fd < 0) {
        LogPrintf("IPC: cannot read the peer's identity: no socket fd is available");
        return std::nullopt;
    }

    PeerIdentity peer;
#ifdef __linux__
    // Linux is the one target where SO_PEERCRED carries struct ucred. OpenBSD
    // also defines SO_PEERCRED, with struct sockpeercred, so the test is on
    // __linux__ rather than on the option name. PeerCredentialEnforcement()
    // makes the same test.
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        const int err = errno;
        LogPrintf("IPC: cannot read the peer's identity: SO_PEERCRED failed (errno %d)", err);
        return std::nullopt;
    }
    if (len != sizeof(cred)) {
        LogPrintf("IPC: cannot read the peer's identity: SO_PEERCRED returned %u bytes, expected %u",
                  static_cast<unsigned int>(len), static_cast<unsigned int>(sizeof(cred)));
        return std::nullopt;
    }
    peer.uid = cred.uid;
    peer.gid = cred.gid;
#ifdef SO_PEERGROUPS
    // The supplementary groups of the peer. Start with room for 64 and, when the
    // kernel says that was too small (ERANGE), retry once with the size it
    // reports. Any other failure refuses the peer.
    peer.groups.resize(64);
    for (int attempt = 0; attempt < 2; ++attempt) {
        socklen_t glen = static_cast<socklen_t>(peer.groups.size() * sizeof(gid_t));
        if (::getsockopt(fd, SOL_SOCKET, SO_PEERGROUPS, peer.groups.data(), &glen) == 0) {
            peer.groups.resize(glen / sizeof(gid_t));
            return peer;
        }
        const int err = errno;
        if (PeerGroupsErrorAction(err) == PeerGroupsError::EmptyGroups) {
            // Empty groups still refuse a peer admitted only through a
            // supplementary group, so group admission stays closed while a
            // same-account peer keeps working. Logged once per process.
            static std::once_flag logged;
            std::call_once(logged, [] {
                LogPrintf("IPC: this kernel does not report peer supplementary groups (SO_PEERGROUPS); "
                          "attach group membership is unavailable and only same-account peers and peers whose "
                          "primary group is the attach group are admitted\n");
            });
            peer.groups.clear();
            return peer;
        }
        if (err != ERANGE || attempt != 0 || glen == 0 || glen % sizeof(gid_t) != 0) {
            LogPrintf("IPC: cannot read the peer's identity: SO_PEERGROUPS failed (errno %d)", err);
            return std::nullopt;
        }
        peer.groups.resize(glen / sizeof(gid_t));
    }
    LogPrintf("IPC: cannot read the peer's identity: SO_PEERGROUPS did not settle");
    return std::nullopt;
#endif
#else
    // macOS, FreeBSD, OpenBSD and the other BSDs.
    if (::getpeereid(fd, &peer.uid, &peer.gid) != 0) {
        const int err = errno;
        LogPrintf("IPC: cannot read the peer's identity: getpeereid failed (errno %d)", err);
        return std::nullopt;
    }
#endif
    return peer;
}

bool ConfigureAttachGroup(const std::string& name)
{
    std::optional<gid_t> gid;
    if (!name.empty()) {
        gid = LookUpGroupGid(name);
        if (!gid) return false;
#ifdef __linux__
        // The kernel reports the peers' groups, but a socket the core owns is
        // only shared with the group if the core is in it.
        if (!ProcessHoldsGroup(*gid)) {
            const std::string shown = SanitizeString(name);
            LogPrintf("IPC: this process is not a member of the attach group '%s'; add SupplementaryGroups=%s to "
                      "the service unit, or run the core with that group\n",
                      shown, shown);
            return false;
        }
#endif
    }
    {
        LOCK(g_attach_group_mutex);
        g_attach_gid = gid;
    }
    if (gid) {
        LogPrintf("IPC: -ipcattachgroup: non-root peers in the group with gid %u are also admitted", *gid);
    }
    return true;
}

std::optional<std::string> AttachGroupProblem(const std::string& name)
{
    if (name.empty()) return std::nullopt;
    const std::string shown = SanitizeString(name);
    const std::optional<gid_t> gid = LookUpGroupGid(name);
    if (!gid) {
        return strprintf("The IPC attach group '%s' does not exist.", shown);
    }
#ifdef __linux__
    if (!ProcessHoldsGroup(*gid)) {
        return strprintf("This process is not a member of the IPC attach group '%s'. Add "
                         "SupplementaryGroups=%s to the service unit (or run the core with that group).",
                         shown, shown);
    }
#endif
    return std::nullopt;
}

std::optional<gid_t> AttachGroupGid()
{
    LOCK(g_attach_group_mutex);
    return g_attach_gid;
}

} // namespace ipc

#endif // !WIN32
