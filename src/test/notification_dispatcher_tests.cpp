// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/notification_dispatcher.h"
#include "sync.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

//! Switches the process to dispatched delivery with a running dispatcher, and puts
//! it back to inline delivery with the dispatcher stopped on every exit path.
struct DispatchedScope
{
    DispatchedScope()
    {
        interfaces::SetNotificationDelivery(interfaces::NotificationDelivery::Dispatched);
        interfaces::StartNotificationDispatcher();
    }

    ~DispatchedScope()
    {
        interfaces::StopNotificationDispatcher();
        interfaces::SetNotificationDelivery(interfaces::NotificationDelivery::Inline);
    }
};

//! Poll a condition with a generous bound. The bound only limits a failing run.
template <typename Pred>
bool WaitFor(Pred pred)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}

} // namespace

BOOST_AUTO_TEST_SUITE(notification_dispatcher_tests)

BOOST_AUTO_TEST_CASE(dispatched_emit_does_not_block_on_handler)
{
    DispatchedScope scope;
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<int> calls{0};

    auto fwd = interfaces::ForwardNotify([&](int) {
        gate.wait();
        ++calls;
    });

    const auto t0 = std::chrono::steady_clock::now();
    {
        LOCK(cs_main);
        fwd(1);
        fwd(2);
    }
    BOOST_CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds{500});

    release.set_value();
    BOOST_CHECK(WaitFor([&] { return calls == 2; }));
    BOOST_CHECK_EQUAL(calls, 2);
}

BOOST_AUTO_TEST_CASE(dispatched_preserves_order_and_copies_arguments)
{
    DispatchedScope scope;
    std::atomic<int> count{0};
    std::vector<std::string> got;
    std::string s = "a";

    auto fwd = interfaces::ForwardNotify([&](const std::string& v) {
        got.push_back(v);
        ++count;
    });

    fwd(s);
    s = "b";
    fwd(s);

    BOOST_REQUIRE(WaitFor([&] { return count == 2; }));
    BOOST_CHECK((got == std::vector<std::string>{"a", "b"}));
}

BOOST_AUTO_TEST_CASE(dispatched_handler_runs_off_the_emitting_thread)
{
    DispatchedScope scope;
    const std::thread::id emitter = std::this_thread::get_id();
    std::thread::id ran;
    std::atomic<bool> done{false};

    auto fwd = interfaces::ForwardNotify([&] {
        ran = std::this_thread::get_id();
        done = true;
    });

    fwd();
    BOOST_REQUIRE(WaitFor([&] { return done.load(); }));
    BOOST_CHECK(ran != emitter);
}

BOOST_AUTO_TEST_CASE(dispatched_queue_overflow_drops_and_counts)
{
    DispatchedScope scope;
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<bool> started{false};
    std::atomic<std::size_t> calls{0};

    auto fwd = interfaces::ForwardNotify([&] {
        started = true;
        gate.wait();
        ++calls;
    });

    const uint64_t before = interfaces::DroppedNotificationCount();

    // The first handler blocks on the dispatcher thread, so the queue is empty
    // behind it and then fills with the next NOTIFICATION_QUEUE_MAX posts.
    fwd();
    BOOST_REQUIRE(WaitFor([&] { return started.load(); }));

    for (std::size_t i = 0; i < interfaces::NOTIFICATION_QUEUE_MAX + 5; ++i) {
        fwd();
    }

    BOOST_CHECK(interfaces::DroppedNotificationCount() >= before + 5);

    release.set_value();
    BOOST_CHECK(WaitFor([&] { return calls == interfaces::NOTIFICATION_QUEUE_MAX + 1; }));
    BOOST_CHECK_EQUAL(calls, interfaces::NOTIFICATION_QUEUE_MAX + 1);
}

BOOST_AUTO_TEST_CASE(dispatched_throwing_handler_is_contained)
{
    DispatchedScope scope;
    std::atomic<int> thrown{0};
    std::atomic<int> delivered{0};

    auto bad = interfaces::ForwardNotify([&] {
        ++thrown;
        throw std::runtime_error("x");
    });
    auto good = interfaces::ForwardNotify([&] { ++delivered; });

    bad();
    BOOST_REQUIRE(WaitFor([&] { return thrown == 1; }));

    // The dispatcher survives the exception and delivers the next post.
    good();
    BOOST_CHECK(WaitFor([&] { return delivered == 1; }));
}

BOOST_AUTO_TEST_CASE(posting_when_stopped_drops_and_counts)
{
    interfaces::SetNotificationDelivery(interfaces::NotificationDelivery::Dispatched);
    std::atomic<int> calls{0};
    auto fwd = interfaces::ForwardNotify([&] { ++calls; });

    const uint64_t before = interfaces::DroppedNotificationCount();
    fwd();
    interfaces::SetNotificationDelivery(interfaces::NotificationDelivery::Inline);

    BOOST_CHECK_EQUAL(interfaces::DroppedNotificationCount(), before + 1);
    BOOST_CHECK_EQUAL(calls, 0);
}

BOOST_AUTO_TEST_CASE(inline_mode_calls_synchronously)
{
    BOOST_CHECK(interfaces::GetNotificationDelivery() == interfaces::NotificationDelivery::Inline);

    const std::thread::id emitter = std::this_thread::get_id();
    std::thread::id ran;
    int calls = 0;

    auto fwd = interfaces::ForwardNotify([&](int v) {
        ran = std::this_thread::get_id();
        calls += v;
    });

    fwd(3);
    BOOST_CHECK_EQUAL(calls, 3);
    BOOST_CHECK(ran == emitter);
}

BOOST_AUTO_TEST_SUITE_END()
