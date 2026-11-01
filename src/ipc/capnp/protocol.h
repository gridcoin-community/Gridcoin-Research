// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_PROTOCOL_H
#define GRIDCOIN_IPC_CAPNP_PROTOCOL_H

#include "ipc/protocol.h"

#include <memory>

namespace ipc {
namespace capnp {
//! Construct the Cap'n Proto IPC protocol implementation.
std::unique_ptr<Protocol> MakeCapnpProtocol();
} // namespace capnp
} // namespace ipc

#endif // GRIDCOIN_IPC_CAPNP_PROTOCOL_H
