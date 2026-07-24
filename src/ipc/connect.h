// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CONNECT_H
#define GRIDCOIN_IPC_CONNECT_H

#include "fs.h"
#include "interfaces/ipc.h"

#include <memory>
#include <optional>
#include <string>

namespace interfaces {
class Init;
} // namespace interfaces

namespace ipc {

//! A live connection to a node process: the remote Init capability plus the Ipc
//! that owns the underlying socket and event loop. The Init is a proxy valid
//! only while the Ipc lives, so a caller keeps the whole struct alive for as
//! long as it uses any interface reached through init.
struct GuiConnection {
    std::unique_ptr<interfaces::Ipc> ipc;
    std::unique_ptr<interfaces::Init> init;
};

//! GUI side: perform the connect handshake against a node listening on
//! <data_dir>/<network>/node.sock (doc/multiprocess_design.md section 4.3):
//!
//!   1. read the node's ipc.cookie -- its absence means the node has never run
//!      on this datadir (the cookie is written at startup), so there is no
//!      credential to authenticate with and nothing to dial. The cookie is not
//!      removed at shutdown, so a stale one may outlive a stopped node; that is
//!      harmless -- connectAddress in step 2 then simply fails with no listener;
//!   2. MakeIpc + connectAddress("unix") to obtain the remote Init;
//!   3. ClientAuthenticateAndCheck -- authenticate with the cookie and verify
//!      schema/protocol compatibility.
//!
//! On success returns the connection. On any failure returns std::nullopt and
//! sets \p error_out to a human-readable reason. \p local_init is the GUI's own
//! Init, which MakeIpc requires (the Init this process would serve if it
//! listened; a connect-only GUI never does) and which must outlive the returned
//! connection.
std::optional<GuiConnection> ConnectToNode(const fs::path& data_dir,
                                           interfaces::Init& local_init,
                                           std::string& error_out);

} // namespace ipc

#endif // GRIDCOIN_IPC_CONNECT_H
