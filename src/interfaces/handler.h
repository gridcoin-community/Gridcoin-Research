// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_HANDLER_H
#define GRIDCOIN_INTERFACES_HANDLER_H

#include <functional>
#include <memory>

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

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_HANDLER_H
