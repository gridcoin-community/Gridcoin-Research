// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_NOTIFICATION_DISPATCHER_H
#define GRIDCOIN_INTERFACES_NOTIFICATION_DISPATCHER_H

#include "interfaces/handler.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace interfaces {

//! How a forwarded client notification is delivered. Inline calls the client on the
//! emitting thread (the monolithic build and the GUI process). Dispatched posts the
//! call to the dispatcher thread, so emitting never waits for the client.
enum class NotificationDelivery { Inline, Dispatched };

//! The most notifications the dispatcher queue holds. A notification posted to a
//! full queue is dropped and counted.
static constexpr std::size_t NOTIFICATION_QUEUE_MAX = 4096;

//! Select the delivery mode for the whole process. The default is Inline. The mode
//! is read each time a forwarded notification is emitted.
void SetNotificationDelivery(NotificationDelivery mode);
NotificationDelivery GetNotificationDelivery();

//! Start the one dispatcher thread, named "grc-ipc-notify". Does nothing when it is
//! already running.
void StartNotificationDispatcher();

//! Drop the queued notifications and join the dispatcher thread. A notification
//! being delivered is allowed to finish. Does nothing when it is not running.
void StopNotificationDispatcher();

//! The number of notifications dropped because the queue was full or the
//! dispatcher was stopped.
uint64_t DroppedNotificationCount();

namespace detail {
//! Queue a call for the dispatcher thread. Never blocks on the client. Drops and
//! counts the call when the queue is full or the dispatcher is not running.
void PostNotification(std::function<void()> call);
} // namespace detail

//! Wrap a one-way (void) client callback registered on a uiInterface signal.
//!
//! Inline mode is exactly GuardNotify(fn). Dispatched mode copies the arguments
//! (std::decay_t) at emission and posts the call, which then runs on the dispatcher
//! thread under the same GuardNotify catch. The emitting thread therefore never
//! waits for the client, whatever core locks it holds. Arguments that are pointers
//! are copied as pointers, so a callback must not dereference one; the callbacks
//! that receive a CWallet* or CCryptoKeyStore* ignore it. A callback that returns
//! a value cannot be dispatched and must stay on GuardNotify.
template <typename Fn>
auto ForwardNotify(Fn fn)
{
    using Guarded = decltype(GuardNotify(std::move(fn)));
    auto guarded = std::make_shared<Guarded>(GuardNotify(std::move(fn)));

    return [guarded](auto&&... args) {
        if (GetNotificationDelivery() == NotificationDelivery::Inline) {
            (*guarded)(std::forward<decltype(args)>(args)...);
            return;
        }

        detail::PostNotification(
            [guarded, saved = std::tuple<std::decay_t<decltype(args)>...>(args...)]() {
                std::apply(*guarded, saved);
            });
    };
}

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_NOTIFICATION_DISPATCHER_H
