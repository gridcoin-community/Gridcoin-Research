// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "test/peer_groups.h"

#ifndef WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <vector>
#endif

namespace grc_test {

bool PeerGroupsReadable()
{
#ifdef SO_PEERGROUPS
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) return true;
    std::vector<gid_t> groups(static_cast<size_t>(std::max(::sysconf(_SC_NGROUPS_MAX), 1L)));
    socklen_t len = static_cast<socklen_t>(groups.size() * sizeof(gid_t));
    const int result = ::getsockopt(fds[0], SOL_SOCKET, SO_PEERGROUPS, groups.data(), &len);
    const int err = errno;
    ::close(fds[0]);
    ::close(fds[1]);
    return result == 0 || err != ERANGE;
#else
    return true;
#endif
}

} // namespace grc_test
