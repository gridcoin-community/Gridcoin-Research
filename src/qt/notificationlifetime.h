// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_NOTIFICATIONLIFETIME_H
#define GRIDCOIN_QT_NOTIFICATIONLIFETIME_H

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>

//! Keeps a core-thread notification callback from running while -- or after --
//! the GUI model that owns it is being destroyed.
//!
//! THE PROBLEM. Every Qt model registers notification callbacks that capture raw
//! `this`, and retains the interfaces::Handler so the destructor can disconnect.
//! Disconnecting is not enough. Neither backing implementation waits for a
//! callback that is ALREADY EXECUTING on another thread:
//!
//!   * monolithic: ~Handler drops a boost::signals2::scoped_connection, and
//!     signals2 releases its internal lock before invoking a slot, so a slot in
//!     flight keeps running after disconnect() returns;
//!   * multiprocess: the Handler's destroy() is itself a blocking IPC round trip,
//!     which widens the window rather than closing it.
//!
//! And the models die early: bitcoin.cpp declares them as stack objects whose
//! scope closes BEFORE Shutdown() stops ThreadStakeMiner, ThreadScraper and
//! ThreadMessageHandler -- and only the monolithic build calls Shutdown() at all.
//! So every model destructor runs with core threads live and emitting.
//! MinerStatusChanged fires from ThreadStakeMiner and NotifyScraperEvent fires
//! per scraper log line, so the window is not theoretical.
//!
//! THE FIX. Callbacks wrapped by guard() hold a shared lock for the duration of
//! the call; retire() takes it exclusively. That gives both halves of what is
//! needed and neither is sufficient alone:
//!
//!   * a callback that has not started yet sees alive == false and does nothing;
//!   * a callback already in flight completes before retire() returns, so the
//!     object it is touching is still alive while it touches it.
//!
//! Call retire() from the owner's destructor (or its unsubscribe helper) BEFORE
//! any member is destroyed, so `this` remains valid for anything still running.
//!
//! The lock is shared, not exclusive, so concurrent notifications from different
//! core threads still run concurrently. retire() blocks only for as long as the
//! callbacks it is waiting on, which is bounded by the handler contract in
//! interfaces/handler.h: a handler must only marshal to the GUI thread with a
//! queued connection, never block and never call back into the node. A handler
//! that violated that contract would stall this destructor -- one more reason the
//! contract is what it is.
//!
//! wallet/wallet_tx_source.cpp solves the same race with weak_from_this(), which
//! works there because the object is shared_ptr-owned. The Qt models are not, so
//! they get the equivalent guarantee through a separately-owned state block.
class NotificationLifetime
{
public:
    NotificationLifetime() = default;

    NotificationLifetime(const NotificationLifetime&) = delete;
    NotificationLifetime& operator=(const NotificationLifetime&) = delete;

    //! Wrap a notification callback. The returned callable is safe to invoke from
    //! any thread at any time, including after the owner has been destroyed.
    template <typename Fn>
    auto guard(Fn fn) const
    {
        return [state = m_state, fn = std::move(fn)](auto&&... args) {
            std::shared_lock<std::shared_mutex> lock(state->mutex);
            if (!state->alive) return;
            fn(std::forward<decltype(args)>(args)...);
        };
    }

    //! Stop delivering, and wait for any callback still running to finish.
    //! Idempotent. Must be called before the owner's members are destroyed.
    void retire()
    {
        std::unique_lock<std::shared_mutex> lock(m_state->mutex);
        m_state->alive = false;
    }

private:
    struct State {
        std::shared_mutex mutex;
        bool alive = true;
    };

    //! Owned separately from `this` so a callback can safely consult it even
    //! after the owning model is gone.
    std::shared_ptr<State> m_state = std::make_shared<State>();
};

#endif // GRIDCOIN_QT_NOTIFICATIONLIFETIME_H
