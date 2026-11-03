// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#endif

#include "util/sock.h"

#include "compat.h"

#include <algorithm>
#include <cerrno>
#include <vector>

#ifndef WIN32
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

Sock::Sock() : m_socket(INVALID_SOCKET) {}

Sock::Sock(SOCKET s) : m_socket(s) {}

Sock::~Sock()
{
    Reset();
}

SOCKET Sock::Get() const
{
    return m_socket;
}

int Sock::Send(const void* data, size_t len, int flags) const
{
    return send(m_socket, static_cast<const char*>(data), len, flags);
}

int Sock::Recv(void* buf, size_t len, int flags) const
{
    return recv(m_socket, static_cast<char*>(buf), len, flags);
}

void Sock::Reset()
{
    if (m_socket != INVALID_SOCKET) {
        // closesocket() is compat.h's myclosesocket(): it closes the fd and
        // sets the argument back to INVALID_SOCKET.
        closesocket(m_socket);
    }
}

bool Sock::WaitMany(std::chrono::milliseconds timeout, EventsPerSock& events_per_sock)
{
#ifndef WIN32
    // POSIX: poll(2). The two passes over events_per_sock iterate in the same
    // order (the map is not modified between them), so positional matching of
    // pollfd entries to map entries is valid.
    std::vector<pollfd> pfds;
    pfds.reserve(events_per_sock.size());
    for (const auto& [sock, events] : events_per_sock) {
        pollfd pfd{};
        pfd.fd = sock->Get();
        if (events.requested & RECV) pfd.events |= POLLIN;
        if (events.requested & SEND) pfd.events |= POLLOUT;
        pfds.push_back(pfd);
    }

    if (poll(pfds.data(), pfds.size(), static_cast<int>(timeout.count())) == SOCKET_ERROR) {
        return false;
    }

    size_t i = 0;
    for (auto& [sock, events] : events_per_sock) {
        const auto revents = pfds[i].revents;
        events.occurred = 0;
        if (revents & POLLIN) events.occurred |= RECV;
        if (revents & POLLOUT) events.occurred |= SEND;
        if (revents & (POLLERR | POLLHUP | POLLNVAL)) events.occurred |= ERR;
        ++i;
    }

    return true;
#else
    // Windows: select(2). The connection count is capped well under
    // FD_SETSIZE (1024, set in compat.h), as it was for the old select() loop.
    fd_set recv_set, send_set, err_set;
    FD_ZERO(&recv_set);
    FD_ZERO(&send_set);
    FD_ZERO(&err_set);
    SOCKET socket_max = 0;

    for (const auto& [sock, events] : events_per_sock) {
        const SOCKET s = sock->Get();
        if (events.requested & RECV) FD_SET(s, &recv_set);
        if (events.requested & SEND) FD_SET(s, &send_set);
        FD_SET(s, &err_set);
        if (s > socket_max) socket_max = s;
    }

    struct timeval tv;
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    if (select(socket_max + 1, &recv_set, &send_set, &err_set, &tv) == SOCKET_ERROR) {
        return false;
    }

    for (auto& [sock, events] : events_per_sock) {
        const SOCKET s = sock->Get();
        events.occurred = 0;
        if (FD_ISSET(s, &recv_set)) events.occurred |= RECV;
        if (FD_ISSET(s, &send_set)) events.occurred |= SEND;
        if (FD_ISSET(s, &err_set)) events.occurred |= ERR;
    }

    return true;
#endif
}

SOCKET CreateCloexecSocket(int domain, int type, int protocol)
{
#ifdef WIN32
    // WSA_FLAG_OVERLAPPED keeps the socket usable with overlapped (IOCP) I/O.
    return ::WSASocketW(domain, type, protocol, nullptr, 0,
                        WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
#elif defined(SOCK_CLOEXEC)
    return ::socket(domain, type | SOCK_CLOEXEC, protocol);
#else
    SOCKET s = ::socket(domain, type, protocol);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    const int flags = ::fcntl(s, F_GETFD);
    if (flags == -1 || ::fcntl(s, F_SETFD, flags | FD_CLOEXEC) == -1) {
        const int saved = errno;
        ::close(s);
        errno = saved;
        return INVALID_SOCKET;
    }
    return s;
#endif
}

SOCKET AcceptCloexec(SOCKET listen_socket, sockaddr* addr, socklen_t* addr_len)
{
#ifdef WIN32
    SOCKET s = ::accept(listen_socket, addr, addr_len);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    if (!::SetHandleInformation(reinterpret_cast<HANDLE>(s), HANDLE_FLAG_INHERIT, 0)) {
        const int saved = ::WSAGetLastError();
        ::closesocket(s);
        ::WSASetLastError(saved);
        return INVALID_SOCKET;
    }
    return s;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    return ::accept4(listen_socket, addr, addr_len, SOCK_CLOEXEC);
#else
    SOCKET s = ::accept(listen_socket, addr, addr_len);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    const int flags = ::fcntl(s, F_GETFD);
    if (flags == -1 || ::fcntl(s, F_SETFD, flags | FD_CLOEXEC) == -1) {
        const int saved = errno;
        ::close(s);
        errno = saved;
        return INVALID_SOCKET;
    }
    return s;
#endif
}

void DrainBeforeClose(SOCKET s, std::chrono::milliseconds bound)
{
    if (s == INVALID_SOCKET) return;

    // Windows has no portable way to bound the read of an arbitrary socket mode here, so
    // it shuts down the send side and always resets on close.
#ifdef WIN32
    (void)bound;
    ::shutdown(s, SD_SEND);
    linger lg;
    lg.l_onoff = 1;
    lg.l_linger = 0;
    ::setsockopt(s, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&lg), sizeof(lg));
#else
    const auto deadline = std::chrono::steady_clock::now() + bound;

    ::shutdown(s, SHUT_WR);

    bool peer_closed = false;
    char buf[4096];
    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) break;

        const auto ms = std::max<std::chrono::milliseconds::rep>(remaining.count(), 1);
        timeval tv;
        tv.tv_sec = static_cast<time_t>(ms / 1000);
        tv.tv_usec = static_cast<suseconds_t>((ms % 1000) * 1000);
        if (::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) break;

        const ssize_t n = ::recv(s, buf, sizeof(buf), 0);
        if (n == 0) {
            peer_closed = true;
            break;
        }
        if (n < 0) {
            // Interrupted reads retry against the remaining bound; any other error,
            // including the receive timeout, ends the drain.
            if (errno == EINTR) continue;
            break;
        }
    }

    if (!peer_closed) {
        linger lg;
        lg.l_onoff = 1;
        lg.l_linger = 0;
        ::setsockopt(s, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
    }
#endif
}
