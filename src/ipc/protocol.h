// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PROTOCOL_H
#define GRIDCOIN_IPC_PROTOCOL_H

#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "ipc/serve_init.h" // IPC_AUTH_DEADLINE

#include <chrono>
#include <functional>
#include <memory>
#include <typeindex>

namespace ipc {
struct Context;

//! IPC protocol interface for calling interface methods over a socket. A single
//! implementation (Cap'n Proto) backs it; the abstraction keeps the transport
//! seam narrow. Gridcoin only needs the connect (client) and listen (server)
//! directions -- there is no foreground serve() because the node listens on a
//! background event loop rather than being spawned.
class Protocol
{
public:
    virtual ~Protocol() = default;

    //! Return an Init interface that forwards calls over \p fd. I/O runs on a
    //! background thread. If \p on_disconnect is set it is invoked (on the
    //! event-loop thread) when the peer's connection drops unexpectedly — the
    //! GUI uses it to quit gracefully when the node process exits.
    virtual std::unique_ptr<interfaces::Init> connect(int fd, const char* exe_name,
                                                      std::function<void()> on_disconnect = {}) = 0;

    //! Listen for connections on \p listen_fd and, for each accepted connection,
    //! build a fresh Init via \p make_init and serve it (per-connection auth).
    //! \p make_init returns null to reject a connection before serving.
    //! Non-blocking; I/O runs on a background thread.
    //!
    //! \p auth_deadline is how long an accepted connection may stay
    //! unauthenticated before it is dropped and its slot reclaimed. It is a
    //! parameter rather than a hardcoded constant only so tests can drive the
    //! reclaim path in a second instead of thirty; production always takes the
    //! default. Zero disables the deadline.
    virtual void listen(int listen_fd, const char* exe_name, interfaces::MakeServeInitFn make_init,
                        std::chrono::seconds auth_deadline = IPC_AUTH_DEADLINE) = 0;

    //! Disconnect any incoming connections that are still connected.
    virtual void disconnectIncoming() = 0;

    //! Add a cleanup callback to an interface, run when it is deleted.
    virtual void addCleanup(std::type_index type, void* iface, std::function<void()> cleanup) = 0;

    //! Context accessor.
    virtual Context& context() = 0;
};
} // namespace ipc

#endif // GRIDCOIN_IPC_PROTOCOL_H
