// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_HANDLER_H
#define GRIDCOIN_INTERFACES_HANDLER_H

#include <exception>
#include <functional>
#include <memory>
#include <utility>

namespace boost {
namespace signals2 {
class connection;
} // namespace signals2
} // namespace boost

namespace interfaces {

//! Generic interface for managing an event handler or callback function
//! registered with another interface. Has a single disconnect method to cancel
//! the registration and prevent any future notifications. Destroying the
//! Handler also disconnects.
class Handler
{
public:
    virtual ~Handler() = default;

    //! Disconnect the handler.
    virtual void disconnect() = 0;
};

//! THE HANDLER CONTRACT — a notification handler must not call back into the node.
//!
//! Every handler registered through this header must do nothing but hand the
//! notification off asynchronously; in the Qt models that means
//! QMetaObject::invokeMethod(..., Qt::QueuedConnection) and nothing else. It must
//! not make a synchronous interface call, and it must not take a node lock.
//!
//! This is not a style preference. Notifications are ordinary request/response IPC
//! calls, not one-way sends, so the emitting CORE thread blocks until the handler
//! returns -- and it is usually blocked while holding a core lock (cs_main plus an
//! open CTxDB batch for BeaconChanged/MRCChanged, cs_main+cs_wallet for
//! NotifyTransactionChanged, cs_Scraper for the scraper events). Two consequences
//! follow, and the handlers only avoid both by being pure hand-offs:
//!
//!  * A handler that BLOCKS holds the node's lock for as long as it blocks. A
//!    paged-out or SIGSTOP'd GUI would stall every RPC, the P2P paths and
//!    ThreadStakeMiner for the duration.
//!  * A handler that CALLS BACK is worse. libmultiprocess routes a nested
//!    server->client call onto the caller's own blocked thread, so the call
//!    re-enters node code on the core thread, mid-block-connection, with cs_main
//!    held and a batch open. It would NOT deadlock -- cs_main is recursive -- which
//!    is precisely the danger: it would quietly succeed against half-connected
//!    chain state.
//!
//! The invariant is currently satisfied by every handler in the tree, and that is
//! the only reason the blocking-notification design does not deadlock today.
//! Nothing enforces it mechanically, so it is stated here, at the point every
//! handler is created.
//!
//! Related, and checked mechanically: an interface method declared in a .capnp
//! schema WITHOUT `context :Proxy.Context` runs inline on the IPC event-loop
//! thread instead of a worker, where one contended lock stalls the whole channel.
//! test/lint/lint-capnp-context.py fails the build on that.

//! Return a Handler wrapping a boost::signals2 connection.
std::unique_ptr<Handler> MakeSignalHandler(boost::signals2::connection connection);

//! Return a Handler that runs a cleanup function on disconnect/destruction.
std::unique_ptr<Handler> MakeCleanupHandler(std::function<void()> cleanup);

//! Log (at the ipc category) that a one-way notification was dropped because
//! the IPC client had already disconnected. Defined out of line so this header
//! stays free of the logging dependency; used by GuardNotify below.
void LogDroppedNotification(const char* what);

//! Wrap a one-way (void) notification callback so that an exception raised while
//! delivering it is dropped instead of propagating into the core thread that
//! emitted the uiInterface signal.
//!
//! In the multiprocess build the server-side forwarder registered on a uiInterface
//! signal calls the GUI's proxy client. If the GUI has disconnected (crash, close,
//! or a shutdown race), libmultiprocess raises "IPC client method called after
//! disconnect." at Log::Raise, which Gridcoin's IpcLogFn turns into a
//! std::runtime_error -- and that would otherwise unwind into core threads such as
//! ThreadMessageHandler / ProcessMessages. A notification to a client that is gone
//! is a no-op by definition, so swallow it.
//!
//! It is NOT limited to that case, though, because it cannot be: the callback body
//! is arbitrary. So the guard deliberately does not treat every exception alike --
//! LogDroppedNotification keeps the disconnect case quiet (IPC category) and logs
//! anything else at the default level, so a genuine defect that happens to escape
//! through a notification stays visible instead of disappearing here. This matters
//! in the monolithic build too, where no disconnect exception can occur and every
//! throw reaching this guard is by definition a real bug.
template <typename Fn>
auto GuardNotify(Fn fn)
{
    return [fn = std::move(fn)](auto&&... args) {
        try {
            fn(std::forward<decltype(args)>(args)...);
        } catch (const std::exception& e) {
            LogDroppedNotification(e.what());
        } catch (...) {
            LogDroppedNotification("non-standard exception");
        }
    };
}

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_HANDLER_H
