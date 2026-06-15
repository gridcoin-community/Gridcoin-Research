// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#endif

#include "util/sock.h"

#include "compat.h"

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
