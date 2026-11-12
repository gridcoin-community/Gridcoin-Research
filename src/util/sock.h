// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_SOCK_H
#define BITCOIN_UTIL_SOCK_H

#include "compat.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

/**
 * RAII helper that owns a socket and closes it on destruction.
 *
 * Trimmed backport of Bitcoin Core's util/sock (issue #2558 PR 5a): just the
 * ownership and the send/recv/get-fd surface that Gridcoin's net layer needs.
 * The multiplexing helper (WaitMany) arrives with its first consumer in PR 5b;
 * proxy/I2P/BIP324 bits are intentionally omitted.
 */
class Sock
{
public:
    /** Construct with an invalid socket (nothing owned). */
    Sock();

    /** Take ownership of an existing socket. */
    explicit Sock(SOCKET s);

    Sock(const Sock&) = delete;
    Sock& operator=(const Sock&) = delete;

    /** Close the owned socket, if any. */
    ~Sock();

    /** Return the wrapped socket, or INVALID_SOCKET if none is owned. */
    SOCKET Get() const;

    /** send(2) on the wrapped socket. */
    int Send(const void* data, size_t len, int flags) const;

    /** recv(2) on the wrapped socket. */
    int Recv(void* buf, size_t len, int flags) const;

    /** Close the owned socket now (idempotent); leaves the wrapper empty. */
    void Reset();

    using Event = uint8_t;

    /** Wait for the socket to become readable. */
    static constexpr Event RECV = 0b001;
    /** Wait for the socket to become writable. */
    static constexpr Event SEND = 0b010;
    /** Wait for an error or other exceptional condition on the socket. */
    static constexpr Event ERR = 0b100;

    /** Requested/occurred event flags for one socket in a WaitMany() call. */
    struct Events
    {
        Events() : requested{0}, occurred{0} {}
        explicit Events(Event req) : requested{req}, occurred{0} {}
        Event requested;
        Event occurred;
    };

    /** Map of sockets to wait on (keyed by the shared_ptr that owns each). */
    using EventsPerSock = std::unordered_map<std::shared_ptr<const Sock>, Events>;

    /**
     * Wait for one or more events on a set of sockets (issue #2558 PR 5b).
     * Uses poll(2) on POSIX and select(2) on Windows. On success each entry's
     * Events::occurred is filled in; returns false on a syscall error.
     */
    [[nodiscard]] static bool WaitMany(std::chrono::milliseconds timeout, EventsPerSock& events_per_sock);

private:
    SOCKET m_socket;
};

//! A socket that child processes do not inherit: SOCK_CLOEXEC where defined, else FD_CLOEXEC set before
//! return; Windows: WSASocketW(..., WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT).
//! INVALID_SOCKET with the platform error set on failure.
SOCKET CreateCloexecSocket(int domain, int type, int protocol);

//! accept() whose result child processes do not inherit (see the window noted in sock.cpp): accept4(SOCK_CLOEXEC) on Linux and the BSDs,
//! accept + FD_CLOEXEC on macOS, accept + SetHandleInformation(HANDLE_FLAG_INHERIT, 0) on Windows.
SOCKET AcceptCloexec(SOCKET listen_socket, sockaddr* addr, socklen_t* addr_len);

//! Prepare a connected socket for close: shutdown(send), then wait with poll (select on Windows)
//! and read and discard until the peer closes or `bound` passes, whatever the socket's blocking
//! mode. If the peer did not close within the bound, set SO_LINGER {1, 0} so the close that follows
//! releases the connection at once (this resets the connection, so unsent data may be lost).
//! Never throws; the caller closes. Where the creation functions above note a window, a child
//! started concurrently on another thread may still inherit the socket.
void DrainBeforeClose(SOCKET s, std::chrono::milliseconds bound);

#endif // BITCOIN_UTIL_SOCK_H
