// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/connect.h"

#include "ipc/handshake.h"
#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "tinyformat.h"

#include <exception>
#include <string>

namespace ipc {

std::optional<GuiConnection> ConnectToNode(const fs::path& data_dir,
                                           interfaces::Init& local_init,
                                           std::string& error_out,
                                           std::function<void()> on_disconnect)
{
    // 1. The node writes a fresh cookie on every startup; its absence means no
    //    node is running on this datadir, so there is nothing to dial.
    std::optional<std::string> cookie = ReadCookie(data_dir);
    if (!cookie) {
        error_out = strprintf("no IPC cookie in %s -- is the Gridcoin daemon running with -multiprocess?",
                              data_dir.string());
        return std::nullopt;
    }

    GuiConnection conn;
    // MakeIpc requires the Init this process would serve when listening; a
    // connect-only GUI never listens, but the reference must stay valid for the
    // Ipc's lifetime -- local_init (owned by the caller) provides it.
    conn.ipc = interfaces::MakeIpc("gridcoinresearch", local_init);

    // 2. Connect to the node's default socket (<data_dir>/<network>/node.sock,
    //    resolved from GetDataDir() inside connectAddress).
    std::string address = "unix";
    try {
        conn.init = conn.ipc->connectAddress(address, std::move(on_disconnect));
    } catch (const std::exception& e) {
        error_out = strprintf("could not connect to the daemon on %s: %s", address, e.what());
        return std::nullopt;
    }
    if (!conn.init) {
        error_out = strprintf("no daemon is listening on %s", address);
        return std::nullopt;
    }

    // 3. Authenticate with the cookie and verify schema/protocol compatibility.
    //    (Identity binding and the git-commit soft-warn are layered on GUI-side.)
    std::string auth_error;
    if (!ClientAuthenticateAndCheck(*conn.init, *cookie, auth_error)) {
        error_out = auth_error;
        return std::nullopt;
    }

    return conn;
}

} // namespace ipc
