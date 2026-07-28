// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include "util.h" // LogPrintf

#include <cerrno>

#ifndef WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ipc {

bool CheckPeerCredentials(int peer_fd)
{
#ifdef WIN32
    // AF_UNIX on Windows exposes no peer-credential API (no SO_PEERCRED /
    // getpeereid); the owner-only datadir NTFS ACL on node.sock + ipc.cookie is
    // the guard (see ipc/process.cpp).
    (void)peer_fd;
    return true;
#else
    if (peer_fd < 0) {
        LogPrintf("IPC: WARNING: no peer fd available; skipping peer-credential check\n");
        return true;
    }

    const uid_t self_uid = ::geteuid();
    uid_t peer_uid;

#if defined(SO_PEERCRED)
    // Linux: struct ucred captured at connect time.
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (::getsockopt(peer_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        LogPrintf("IPC: WARNING: SO_PEERCRED failed (errno %d); skipping peer-credential check\n", errno);
        return true;
    }
    if (len != sizeof(cred)) {
        LogPrintf("IPC: WARNING: SO_PEERCRED returned an unexpected size; skipping peer-credential check\n");
        return true;
    }
    peer_uid = cred.uid;
#else
    // *BSD / macOS.
    gid_t peer_gid;
    if (::getpeereid(peer_fd, &peer_uid, &peer_gid) != 0) {
        LogPrintf("IPC: WARNING: getpeereid failed (errno %d); skipping peer-credential check\n", errno);
        return true;
    }
#endif

    if (peer_uid != self_uid) {
        LogPrintf("IPC: rejecting connection: peer uid %d does not match serving uid %d\n",
                  static_cast<int>(peer_uid), static_cast<int>(self_uid));
        return false;
    }
    return true;
#endif // WIN32
}

} // namespace ipc
