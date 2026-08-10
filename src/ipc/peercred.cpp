// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include "logging.h" // error()

#include <cerrno>

#ifndef WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ipc {

#ifndef WIN32
bool PeerUidAllowed(uid_t peer_uid, uid_t self_uid)
{
    return peer_uid == self_uid;
}
#endif

bool CheckPeerCredentials(int peer_fd)
{
#ifdef WIN32
    // AF_UNIX on Windows exposes no peer-credential API (no SO_PEERCRED /
    // getpeereid); the owner+SYSTEM PROTECTED DACL that ipc/process.cpp applies to
    // node.sock and ipc.cookie -- and verifies by reading it back -- is the guard.
    (void)peer_fd;
    return true;
#else
    // Fail closed from here down. See the header: a peer we cannot inspect is
    // refused rather than admitted, so a platform or backend that stops exposing
    // the fd turns MP off loudly instead of turning the check off silently.
    if (peer_fd < 0) {
        return error("%s: refusing IPC connection: no peer fd is available, so the peer's uid "
                     "cannot be verified", __func__);
    }

    const uid_t self_uid = ::geteuid();
    uid_t peer_uid;

#if defined(SO_PEERCRED)
    // Linux: struct ucred captured at connect time.
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (::getsockopt(peer_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        return error("%s: refusing IPC connection: SO_PEERCRED failed (errno %d), so the peer's "
                     "uid cannot be verified", __func__, errno);
    }
    if (len != sizeof(cred)) {
        return error("%s: refusing IPC connection: SO_PEERCRED returned %d bytes, expected %d",
                     __func__, static_cast<int>(len), static_cast<int>(sizeof(cred)));
    }
    peer_uid = cred.uid;
#else
    // *BSD / macOS.
    gid_t peer_gid;
    if (::getpeereid(peer_fd, &peer_uid, &peer_gid) != 0) {
        return error("%s: refusing IPC connection: getpeereid failed (errno %d), so the peer's "
                     "uid cannot be verified", __func__, errno);
    }
#endif

    if (!PeerUidAllowed(peer_uid, self_uid)) {
        return error("%s: rejecting IPC connection: peer uid %d does not match serving uid %d",
                     __func__, static_cast<int>(peer_uid), static_cast<int>(self_uid));
    }
    return true;
#endif // WIN32
}

const char* PeerCredentialEnforcement()
{
#ifdef WIN32
    return "none (Windows AF_UNIX exposes no peer credentials; the owner-only DACL on node.sock "
           "and ipc.cookie is the guard)";
#elif defined(SO_PEERCRED)
    return "SO_PEERCRED (connections from another OS user are refused)";
#else
    return "getpeereid (connections from another OS user are refused)";
#endif
}

} // namespace ipc
