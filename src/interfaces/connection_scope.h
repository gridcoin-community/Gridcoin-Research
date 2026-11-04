// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_CONNECTION_SCOPE_H
#define GRIDCOIN_INTERFACES_CONNECTION_SCOPE_H

#include <cstdint>
#include <functional>
#include <memory>

namespace interfaces {

//! Teardown hooks for one IPC connection. close() runs every registered hook
//! exactly once, in registration order, OUTSIDE the scope's mutex; hooks must not
//! block on cs_main or cs_wallet, and must not make IPC calls.
//!
//! The core makes one for each connection it serves (ipc::MakeServeInit). When
//! the connection is torn down, the listener closes it on the IPC event-loop
//! thread before it can serve another GUI, so whatever the hooks end -- the
//! attached GUI's wallet capability, a transfer in progress -- is gone by the time
//! another connection can claim the attach slot. A hook therefore does only what
//! is safe there: end a capability (CWallet::EndElevation / EndIpcElevation),
//! delete a core-private temp file, set a flag.
//!
//! The scope's mutex is a leaf. It is never held while a hook runs or while a
//! hook is destroyed, and closed() does not take it.
class ConnectionScope
{
public:
    using HookId = uint64_t;

    virtual ~ConnectionScope();

    //! Register \p fn to run at close(). Returns a nonzero id for
    //! removeCloseHook(). When the scope is already closed, \p fn runs now, on the
    //! calling thread, and 0 is returned.
    virtual HookId addCloseHook(std::function<void()> fn) = 0;

    //! Unregister a hook, so it does not run. If close() is running hooks, waits
    //! until it has finished, so a hook never outlives its removal: an owner may
    //! free what its hook uses as soon as this returns. From inside a hook (the
    //! thread running close()) it does not wait, since that would wait for
    //! itself, but a hook still pending is erased and does not run. An id that
    //! is unknown, already removed or 0 is ignored.
    //!
    //! Because it can wait for running hooks, never call it holding cs_KeyStore,
    //! the wallet's capability mutex, or any other lock a hook takes: the hook
    //! could then never finish.
    virtual void removeCloseHook(HookId id) = 0;

    //! Mark the scope closed, then run the registered hooks, once each, in
    //! registration order, taking them one at a time so that a hook removed by
    //! an earlier one does not run. A hook that throws is logged and the rest
    //! still run.
    //! Idempotent: a later call runs nothing, but if another thread is still
    //! running the hooks it waits for them, so every caller returns only once the
    //! hooks have run. A call from inside a hook returns at once.
    virtual void close() = 0;

    //! Whether close() has been called. True from the moment close() begins,
    //! before any hook runs. Lock-free, so it may be asked under any lock. The
    //! wallet asks it inside its capability-mutex sections that install, read
    //! and end the IPC capability (ConnectionClosedCheck below).
    virtual bool closed() const = 0;
};

//! Return a new, open scope.
std::shared_ptr<ConnectionScope> MakeConnectionScope();

//! The check an interface served on \p scope's connection passes to
//! CWallet::BeginIpcElevation, EndIpcElevation and IpcCapability, which ask it
//! under the capability mutex: whether that connection has closed. Empty (no
//! check) when \p scope is null, as it is in process, where there is no
//! connection.
std::function<bool()> ConnectionClosedCheck(std::shared_ptr<ConnectionScope> scope);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_CONNECTION_SCOPE_H
