// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_IPC_H
#define GRIDCOIN_INTERFACES_IPC_H

#include <functional>
#include <memory>
#include <string>
#include <typeindex>

namespace ipc {
struct Context;
} // namespace ipc

namespace interfaces {
class Init;

//! Factory that builds the per-connection Init a listening process serves to each
//! accepted client. Called once per accepted connection with the accepted
//! socket's fd (\p peer_fd; -1 if unavailable). Returning nullptr REJECTS the
//! connection before it is served (e.g. an OS peer-credential mismatch). A fresh
//! Init per connection is what keeps authentication per-connection -- a new peer
//! cannot ride an earlier peer's authenticated session. A connect-only process
//! (the GUI) never listens and may pass an empty factory.
using MakeServeInitFn = std::function<std::unique_ptr<Init>(int peer_fd)>;

//! Interprocess-communication (IPC) helper. Establishes a connection between a
//! controlling process (the GUI) and a controlled process (the node). When a
//! connection is established the controlled process exposes its interfaces::Init,
//! which the controlling process uses to reach the other interfaces.
//!
//! Gridcoin's Stage-2 split uses two independently-started binaries and a named
//! AF_UNIX socket -- the node listenAddress()es, the GUI connectAddress()es. The
//! GUI does not spawn the node (see doc/multiprocess_design.md), so this helper
//! is deliberately narrower than Bitcoin Core's Ipc (no spawnProcess /
//! startSpawnedProcess).
class Ipc
{
public:
    virtual ~Ipc() = default;

    //! Connect to a socket address and return a pointer to the remote process's
    //! Init interface. Returns null if \p address is empty ("") or disabled
    //! ("0"), or if a connection was refused/absent but not required ("auto");
    //! throws on an unexpected error. If \p on_disconnect is set it is invoked
    //! (on the IPC event-loop thread) when the remote drops the connection
    //! unexpectedly — the GUI uses it to quit gracefully when the node exits.
    virtual std::unique_ptr<Init> connectAddress(std::string& address,
                                                 std::function<void()> on_disconnect = {}) = 0;

    //! Listen on a socket address, exposing this process's Init interface to
    //! clients. Throws on error (including a live listener already on the path).
    virtual void listenAddress(std::string& address) = 0;

    //! Disconnect any incoming connections that are still connected.
    virtual void disconnectIncoming() = 0;

    //! Add a cleanup callback to a remote interface, run when it is deleted.
    template <typename Interface>
    void addCleanup(Interface& iface, std::function<void()> cleanup)
    {
        addCleanup(typeid(Interface), &iface, std::move(cleanup));
    }

    //! IPC context struct accessor.
    virtual ipc::Context& context() = 0;

protected:
    //! Type-erased implementation of the public addCleanup template (template
    //! methods cannot be virtual).
    virtual void addCleanup(std::type_index type, void* iface, std::function<void()> cleanup) = 0;
};

//! Return an implementation of the Ipc interface. \p exe_name is this process's
//! role name (used for the socket filename and thread names); \p make_serve_init
//! builds the per-connection Init this process serves when listening (a
//! connect-only process never listens and may pass an empty factory).
std::unique_ptr<Ipc> MakeIpc(const char* exe_name, MakeServeInitFn make_serve_init);
} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_IPC_H
