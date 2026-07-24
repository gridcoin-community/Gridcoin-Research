// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_SERVE_INIT_H
#define GRIDCOIN_IPC_SERVE_INIT_H

#include "interfaces/init.h"

#include <memory>
#include <string>

namespace ipc {

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
