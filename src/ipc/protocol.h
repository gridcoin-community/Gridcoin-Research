// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PROTOCOL_H
#define GRIDCOIN_IPC_PROTOCOL_H

#include "interfaces/init.h"

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
    //! background thread.
    virtual std::unique_ptr<interfaces::Init> connect(int fd, const char* exe_name) = 0;

    //! Listen for connections on \p listen_fd, accept them, and serve \p init on
    //! each. Non-blocking; I/O runs on a background thread.
    virtual void listen(int listen_fd, const char* exe_name, interfaces::Init& init) = 0;

    //! Disconnect any incoming connections that are still connected.
    virtual void disconnectIncoming() = 0;

    //! Add a cleanup callback to an interface, run when it is deleted.
    virtual void addCleanup(std::type_index type, void* iface, std::function<void()> cleanup) = 0;

    //! Context accessor.
    virtual Context& context() = 0;
};
} // namespace ipc

#endif // GRIDCOIN_IPC_PROTOCOL_H
