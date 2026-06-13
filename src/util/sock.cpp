// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#endif

#include "util/sock.h"

#include "compat.h"

#include <vector>

#ifndef WIN32
#include <poll.h>
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
