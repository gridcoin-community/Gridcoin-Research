// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_SERVE_INIT_H
#define GRIDCOIN_IPC_SERVE_INIT_H

#include "interfaces/init.h"

#include <chrono>
#include <memory>
#include <string>

namespace ipc {

//! How long an accepted connection may remain unauthenticated before the node
//! drops it (doc/multiprocess_design.md section 4.3).
//!
//! Why this exists: the listener accepts ONE simultaneous connection, and the
//! accept is not re-armed while that slot is occupied. Without a deadline, a local
//! process that connects and then says nothing -- or presents a wrong cookie and
//! sits there -- denies the real GUI service for as long as it lives, and nothing
//! in the transport ever reclaims the slot. It also bounds the window in which an
//! unauthenticated peer can reach the pre-auth surface of the capnp interface
//! (Init.construct -> ThreadMap; see MAX_SERVER_THREADS_PER_CONNECTION).
//!
//! The value is generous on purpose. A real GUI authenticates in the same
//! millisecond it connects (ClientHandshake's first act is authenticate()), so
//! anything above a second or two is already unreachable for legitimate traffic;
//! 30s only avoids punishing a heavily loaded or debugger-stopped client.
inline constexpr std::chrono::seconds IPC_AUTH_DEADLINE{30};

//! Wrap the node's real (in-process) Init so it can be served over IPC: it adds
//! the connect handshake (authenticate / getBuildInfo / getIdentity) and gates
//! the delegated methods so nothing is served before authenticate() succeeds
//! (doc/multiprocess_design.md section 4.3). Only the IPC-exposed Init methods
//! are wrapped; this must grow as init.capnp exposes more factories.
std::unique_ptr<interfaces::Init> MakeServeInit(std::unique_ptr<interfaces::Init> inner,
                                                std::string cookie,
                                                interfaces::NodeIdentity identity);

} // namespace ipc

#endif // GRIDCOIN_IPC_SERVE_INIT_H
