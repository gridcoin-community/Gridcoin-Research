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

bool CheckPeerCredentials(int peer_fd, PeerCredPolicy policy)
{
#ifdef WIN32
    (void)policy;
    // AF_UNIX on Windows exposes no peer-credential API (no SO_PEERCRED /
    // getpeereid); the owner+SYSTEM PROTECTED DACL that ipc/process.cpp applies to
    // node.sock and ipc.cookie -- and verifies by reading it back -- is the guard.
    (void)peer_fd;
    return true;
#else
    // "Cannot determine the peer's uid" is refused under Enforcing and
    // allowed-with-a-log under Advisory; a DEFINITE mismatch is refused under both.
    // See PeerCredPolicy for why the two callers differ.
    const auto undetermined = [policy](const char* what, int err) {
        if (policy == PeerCredPolicy::Enforcing) {
            return error("CheckPeerCredentials: refusing IPC connection: %s (errno %d), so the "
                         "peer's uid cannot be verified", what, err);
        }
        LogPrintf("WARN: CheckPeerCredentials: %s (errno %d); could not verify the peer's uid, "
                  "continuing on the cookie alone", what, err);
        return true;
    };

    if (peer_fd < 0) {
        return undetermined("no peer fd is available", 0);
    }

    const uid_t self_uid = ::geteuid();
    uid_t peer_uid;

#if defined(SO_PEERCRED)
    // Linux: struct ucred captured at connect time.
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (::getsockopt(peer_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        return undetermined("SO_PEERCRED failed", errno);
    }
    if (len != sizeof(cred)) {
        return undetermined("SO_PEERCRED returned an unexpected size", 0);
    }
    peer_uid = cred.uid;
#else
    // *BSD / macOS.
    gid_t peer_gid;
    if (::getpeereid(peer_fd, &peer_uid, &peer_gid) != 0) {
        return undetermined("getpeereid failed", errno);
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
