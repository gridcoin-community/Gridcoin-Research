// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/connect.h"

#include "chainparams.h"
#include "ipc/handshake.h"
#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "tinyformat.h"

#include <exception>
#include <string>

namespace ipc {

std::optional<GuiConnection> ConnectToNode(const fs::path& data_dir,
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
    // A connect-only GUI never listens, so it serves no Init: pass an empty
    // serve-init factory (MakeIpc only ever calls it on the listen path).
    conn.ipc = interfaces::MakeIpc("gridcoinresearch", {});

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

    // 3. Authenticate, verify compatibility (schema/protocol/network), and fetch
    //    the node's build + identity. Identity *binding* (vs the GUI's stored
    //    expectation) and the git_commit soft-warn banner are the caller's
    //    GUI-side steps, driven off conn.handshake.
    HandshakeResult hr = ClientHandshake(*conn.init, *cookie, Params().NetworkIDString());
    if (!hr.ok) {
        error_out = hr.error;
        return std::nullopt;
    }
    conn.handshake = std::move(hr);

    return conn;
}

} // namespace ipc
