// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CONNECT_H
#define GRIDCOIN_IPC_CONNECT_H

#include "fs.h"
#include "interfaces/ipc.h"
#include "ipc/handshake.h" // HandshakeResult, ConnectFailure, IPC_CLIENT_HANDSHAKE_TIMEOUT

#include <chrono>
#include <functional>
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
    //! The successful handshake's build fingerprint, node identity token, and any
    //! soft (non-fatal) findings -- consumed GUI-side for identity binding and the
    //! mixed-build banner.
    HandshakeResult handshake;
    //! The core runs as another OS account (the -ipcserviceuser account) rather
    //! than as this process's own. Always false on Windows.
    bool core_other_account{false};
};

//! GUI side: perform the connect handshake against a node listening on
//! <ipc_dir>/node.sock (doc/multiprocess_design.md section 4.3). \p ipc_dir, not
//! GetIpcDir(), decides where the cookie is read and which socket is dialled.
//!
//!   1. read the node's ipc.cookie -- its absence means no -multiprocess node
//!      is serving this datadir: none is running with -multiprocess, one is
//!      still starting (the cookie is written after core init, just before the
//!      node starts listening), or the last one shut down cleanly and deleted
//!      it. There is then no credential to authenticate with and nothing to
//!      dial (NoCookie). A node that crashed or was killed leaves a stale cookie
//!      behind; that is harmless -- connectAddress in step 2 then simply fails
//!      with no listener (NotListening);
//!   2. MakeIpc + connectAddress("unix:<ipc_dir>/node.sock") to obtain the
//!      remote Init, after the client peer check (PeerRefused). This makes the
//!      transport's bootstrap round trip (Init.construct);
//!   3. ClientHandshake -- authenticate with the cookie, verify
//!      schema/protocol/network compatibility, and ask for the attach slot
//!      (AlreadyAttached; any other failure is Handshake).
//!
//! The round trips of steps 2 and 3 run on the calling thread, bounded by
//! \p handshake_timeout from the start of step 2: a watchdog thread, which makes
//! no IPC calls, then shuts the connection's socket down, which fails the call
//! in progress as a disconnect, and the result is Timeout. The watchdog is
//! joined before the shutdown handle is closed and before the connection is
//! destroyed. A connect() the core's listener keeps waiting (its queue full) is
//! not interrupted: it ends when the core accepts or exits, and if the deadline
//! has passed by then the socket is shut down at once and the result is Timeout.
//!
//! On success returns the connection. On any failure returns std::nullopt and
//! sets \p failure_out. If \p on_disconnect is set it is invoked (on the IPC
//! event-loop thread) when the node drops the connection after a successful
//! handshake (crash / SIGKILL, or a stop that raced the shutdown handshake) —
//! the GUI uses it to quit cleanly rather than fault on the next call to a
//! now-dead proxy. A drop before the handshake has succeeded is the failure this
//! call reports, and does not reach \p on_disconnect.
std::optional<GuiConnection> ConnectToNode(const fs::path& ipc_dir, ConnectFailure& failure_out,
                                           std::function<void()> on_disconnect = {},
                                           std::chrono::seconds handshake_timeout = IPC_CLIENT_HANDSHAKE_TIMEOUT);

} // namespace ipc

#endif // GRIDCOIN_IPC_CONNECT_H
