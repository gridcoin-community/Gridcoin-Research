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
    //! throws on an unexpected error.
    virtual std::unique_ptr<Init> connectAddress(std::string& address) = 0;

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
//! role name (used for the socket filename and thread names); \p init is the
//! Init this process serves when listening.
std::unique_ptr<Ipc> MakeIpc(const char* exe_name, Init& init);
} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_IPC_H
