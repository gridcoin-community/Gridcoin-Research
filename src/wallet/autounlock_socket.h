// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_AUTOUNLOCK_SOCKET_H
#define GRIDCOIN_WALLET_AUTOUNLOCK_SOCKET_H

#include "fs.h"
#include "support/allocators/secure.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace autounlock {
//! The retry budget of one fetch. The wait before attempt n+1 is first_delay * 2^(n-1), so the defaults wait 1, 2, 4
//! and 8 s between five attempts, and with a receive timeout of 10 s per attempt the whole fetch takes at most about
//! 65 s. That stays far below the start timeout of the service unit, which matters because the autounlock runs before
//! READY=1 is sent.
struct SocketFetchParams {
    int attempts{5};
    std::chrono::milliseconds first_delay{1000};
    std::chrono::seconds receive_timeout{10};
    size_t max_bytes{1024};
};

//! Connect a SOCK_STREAM, close-on-exec Unix socket to socket_path. Returns the descriptor, or -1 with the reason in
//! error: "relative path", "path too long" or "connect failed: <strerror>".
int ConnectAutoUnlockSocket(const fs::path& socket_path, std::string& error);

//! Read the passphrase from the helper listening on socket_path. The client sends nothing; the helper writes the
//! passphrase and closes, so the reply is read to end of file. Zero bytes, more than max_bytes, a timeout or a failure to
//! connect is a failed attempt, which is logged by reason (never the data) and retried within the budget. Returns
//! nullopt when the attempts are used up, when the path can never work, or once a shutdown is requested.
std::optional<SecureString> FetchFromSocket(const fs::path& socket_path, const SocketFetchParams& params = {});
} // namespace autounlock

#endif // GRIDCOIN_WALLET_AUTOUNLOCK_SOCKET_H
