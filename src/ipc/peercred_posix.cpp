// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// POSIX backend of the peer-check hooks declared in ipc/peercred.h. The build
// lists it on every platform; on Windows it compiles to nothing.

#include "ipc/peercred.h"

#ifndef WIN32

#include "logging.h"
#include "sync.h"

#include <cerrno>
#include <grp.h>
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
        LogPrintf("IPC: the group database lookup for the attach group failed (errno %d)\n", err);
        return std::nullopt;
    }
    if (result == nullptr) {
        LogPrintf("IPC: the attach group is not in the group database\n");
        return std::nullopt;
    }
    return grp.gr_gid;
}

} // namespace

std::optional<PeerIdentity> ReadPeerIdentity(int fd)
{
    if (fd < 0) {
        LogPrintf("IPC: cannot read the peer's identity: no socket fd is available\n");
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
        LogPrintf("IPC: cannot read the peer's identity: SO_PEERCRED failed (errno %d)\n", err);
        return std::nullopt;
    }
    if (len != sizeof(cred)) {
        LogPrintf("IPC: cannot read the peer's identity: SO_PEERCRED returned %u bytes, expected %u\n",
                  static_cast<unsigned int>(len), static_cast<unsigned int>(sizeof(cred)));
        return std::nullopt;
    }
    peer.uid = cred.uid;
    peer.gid = cred.gid;
#else
    // macOS, FreeBSD, OpenBSD and the other BSDs.
    if (::getpeereid(fd, &peer.uid, &peer.gid) != 0) {
        const int err = errno;
        LogPrintf("IPC: cannot read the peer's identity: getpeereid failed (errno %d)\n", err);
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
    }
    {
        LOCK(g_attach_group_mutex);
        g_attach_gid = gid;
    }
    if (gid) {
        LogPrintf("IPC: -ipcattachgroup: non-root peers in the group with gid %u are also admitted\n", *gid);
    }
    return true;
}

std::optional<gid_t> AttachGroupGid()
{
    LOCK(g_attach_group_mutex);
    return g_attach_gid;
}

} // namespace ipc

#endif // !WIN32
