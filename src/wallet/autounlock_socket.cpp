// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/autounlock_socket.h"

#include "logging.h"
#include "node/shutdown.h"
#include "support/cleanse.h"
#include "util/syserror.h"
#include "util/time.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace autounlock {
namespace {
//! The reason a path can never be connected to, or an empty string.
std::string PathProblem(const fs::path& socket_path)
{
    if (!socket_path.is_absolute()) {
        return "relative path";
    }

    if (socket_path.native().size() >= sizeof(sockaddr_un::sun_path)) {
        return "path too long";
    }

    return {};
}

//! Waits for the given time in short slices, so that a shutdown request ends the wait. Returns false on shutdown.
bool WaitUnlessShutdown(std::chrono::milliseconds delay)
{
    const auto slice = std::chrono::milliseconds(100);

    while (delay.count() > 0) {
        if (ShutdownRequested()) {
            return false;
        }

        const auto step = std::min(delay, slice);
        UninterruptibleSleep(step);
        delay -= step;
    }

    return !ShutdownRequested();
}

//! Reads to end of file into the secure string, through a stack buffer that is cleansed. Returns an empty string on
//! success and the failure reason otherwise. Neither the bytes nor a copy of them is ever in a std::string.
std::string ReadReply(int fd, const SocketFetchParams& params, SecureString& reply)
{
    const auto deadline = std::chrono::steady_clock::now() + params.receive_timeout;
    char buffer[256];
    std::string reason;

    // reserved up front so that appending never reallocates and leaves a stale copy behind
    reply.reserve(params.max_bytes + 1);

    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now());

        if (remaining.count() <= 0) {
            reason = "timed out";
            break;
        }

        pollfd pfd{fd, POLLIN, 0};
        const int ready = ::poll(&pfd, 1, static_cast<int>(remaining.count()));

        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }

            reason = "read failed: " + SysErrorString(errno);
            break;
        }

        if (ready == 0) {
            reason = "timed out";
            break;
        }

        // one byte more than the limit is enough to know that the reply is too long
        const size_t room = params.max_bytes + 1 - reply.size();
        const ssize_t n = ::recv(fd, buffer, std::min(sizeof(buffer), room), 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            reason = "read failed: " + SysErrorString(errno);
            break;
        }

        if (n == 0) {
            if (reply.empty()) {
                reason = "the helper refused (see journalctl -u <helper unit>)";
            }

            break;
        }

        reply.append(buffer, static_cast<size_t>(n));

        if (reply.size() > params.max_bytes) {
            reason = strprintf("reply exceeds %d bytes", params.max_bytes);
            break;
        }
    }

    memory_cleanse(buffer, sizeof(buffer));

    if (!reason.empty()) {
        memory_cleanse(reply.data(), reply.size());
        reply.clear();
    }

    return reason;
}
} // namespace

int ConnectAutoUnlockSocket(const fs::path& socket_path, std::string& error)
{
    error = PathProblem(socket_path);

    if (!error.empty()) {
        return -1;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (fd < 0) {
        error = "connect failed: " + SysErrorString(errno);
        return -1;
    }

    sockaddr_un address;
    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socket_path.c_str(), socket_path.native().size());

    int rc;

    do {
        rc = ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    } while (rc < 0 && errno == EINTR);

    if (rc < 0) {
        error = "connect failed: " + SysErrorString(errno);
        ::close(fd);
        return -1;
    }

    return fd;
}

std::optional<SecureString> FetchFromSocket(const fs::path& socket_path, const SocketFetchParams& params)
{
    // A path that is relative or too long never works, so there is nothing to retry.
    const std::string path_problem = PathProblem(socket_path);

    if (!path_problem.empty()) {
        LogPrintf("autounlock: attempt 1 failed: %s; the wallet stays locked", path_problem);
        return std::nullopt;
    }

    std::chrono::milliseconds delay = params.first_delay;

    for (int attempt = 1; attempt <= params.attempts; ++attempt) {
        if (ShutdownRequested()) {
            LogPrintf("autounlock: stopped by shutdown; the wallet stays locked");
            return std::nullopt;
        }

        std::string reason;
        SecureString reply;
        const int fd = ConnectAutoUnlockSocket(socket_path, reason);

        if (fd >= 0) {
            reason = ReadReply(fd, params, reply);
            ::close(fd);

            if (reason.empty()) {
                return reply;
            }
        }

        LogPrintf("autounlock: attempt %d of %d failed: %s", attempt, params.attempts, reason);

        if (attempt == params.attempts) {
            break;
        }

        if (!WaitUnlessShutdown(delay)) {
            LogPrintf("autounlock: stopped by shutdown; the wallet stays locked");
            return std::nullopt;
        }

        delay *= 2;
    }

    LogPrintf("autounlock: giving up after %d attempts; the wallet stays locked", params.attempts);
    return std::nullopt;
}
} // namespace autounlock
