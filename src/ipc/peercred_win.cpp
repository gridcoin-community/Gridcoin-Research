// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Windows placeholders of the peer-check hooks declared in ipc/peercred.h.
// AF_UNIX on Windows exposes no peer credentials, and kj hands the server no
// socket fd, so nothing is read here: the owner-only DACL on node.sock and
// ipc.cookie is the guard, and a different account cannot be configured. The
// build lists this file on every platform; elsewhere it compiles to nothing.

#include "ipc/peercred.h"

#ifdef WIN32

#include "logging.h"

namespace ipc {

std::optional<PeerIdentity> ReadPeerIdentity(int fd)
{
    (void)fd;
    return PeerIdentity{};
}

bool ConfigureAttachGroup(const std::string& name)
{
    if (name.empty()) return true;
    LogPrintf("IPC: the Windows peer check for a different account is not available in this build\n");
    return false;
}

std::optional<std::string> AttachGroupProblem(const std::string& name)
{
    if (name.empty()) return std::nullopt;
    return "The Windows peer check for a different account is not available in this build.";
}

} // namespace ipc

#endif // WIN32
