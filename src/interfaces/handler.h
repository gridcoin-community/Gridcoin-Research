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
//! is a no-op by definition, so swallow it. In the monolithic build the wrapped
//! callback does not throw, so the guard is inert.
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
