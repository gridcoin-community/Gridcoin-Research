// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_SOCK_H
#define BITCOIN_UTIL_SOCK_H

#include "compat.h"

#include <cstddef>

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

private:
    SOCKET m_socket;
};

#endif // BITCOIN_UTIL_SOCK_H
