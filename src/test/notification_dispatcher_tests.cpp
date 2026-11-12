// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/node.h"
#include "interfaces/notification_dispatcher.h"
#include "logging.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

//! Switches the process to dispatched delivery with a running dispatcher, and puts
//! it back to inline delivery with the dispatcher stopped on every exit path.
struct DispatchedScope
{
    explicit DispatchedScope(bool start = true)
    {
        interfaces::SetNotificationDelivery(interfaces::NotificationDelivery::Dispatched);
        if (start) interfaces::StartNotificationDispatcher();
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

//! A handler body that waits for the gate. The wait is bounded, so a regression to
//! inline delivery fails the case instead of hanging the binary.
void WaitGate(const std::shared_future<void>& gate)
{
    gate.wait_for(std::chrono::seconds{10});
}

//! Collects the lines the logger prints while in scope.
struct CaptureLog
{
    mutable std::mutex mutex;
    std::vector<std::string> lines;
    std::list<std::function<void(const std::string&)>>::iterator callback;

    CaptureLog()
        : callback(LogInstance().PushBackCallback([this](const std::string& s) {
              std::lock_guard<std::mutex> guard(mutex);
              lines.push_back(s);
          }))
    {
    }
    ~CaptureLog() { LogInstance().DeleteCallback(callback); }
    CaptureLog(const CaptureLog&) = delete;
    CaptureLog& operator=(const CaptureLog&) = delete;

    std::size_t Count(const std::string& needle) const
    {
        std::lock_guard<std::mutex> guard(mutex);
        std::size_t n = 0;
        for (const std::string& l : lines) {
            if (l.find(needle) != std::string::npos) ++n;
        }
        return n;
    }
};

//! An argument whose destructor emits another notification. A queued copy that is
//! destroyed while the dispatcher mutex is held would take that mutex again.
struct PostingArg
{
    std::function<void()>* on_destroy{nullptr};
    std::atomic<std::thread::id>* stop_thread{nullptr};
    std::atomic<bool>* destroyed_on_stop_thread{nullptr};

    PostingArg() = default;
    PostingArg(const PostingArg&) = default;
    ~PostingArg()
    {
        if (on_destroy == nullptr) return;
        if (std::this_thread::get_id() == stop_thread->load()) {
            (*on_destroy)();
            *destroyed_on_stop_thread = true;
        }
    }
};

//! An argument that cannot be copied once armed.
struct ThrowingCopy
{
    ThrowingCopy() = default;
    ThrowingCopy(const ThrowingCopy&) { throw std::runtime_error("copy failed"); }
};

} // namespace

BOOST_AUTO_TEST_SUITE(notification_dispatcher_tests)

BOOST_AUTO_TEST_CASE(dispatched_emit_does_not_block_on_handler)
{
    DispatchedScope scope;
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<int> calls{0};

    auto fwd = interfaces::ForwardNotify([&](int) {
        WaitGate(gate);
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
    // The queue bound is pinned.
    BOOST_CHECK_EQUAL(interfaces::NOTIFICATION_QUEUE_MAX, 10000U);

    DispatchedScope scope;
    // Call number n returns once allowed > n. The waits are bounded.
    std::atomic<int> allowed{0};
    std::atomic<int> started{0};
    std::atomic<std::size_t> calls{0};

    auto fwd = interfaces::ForwardNotify([&] {
        const int n = started++;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (allowed <= n && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        ++calls;
    });

    CaptureLog log;
    const uint64_t before = interfaces::DroppedNotificationCount();

    // The first handler blocks on the dispatcher thread, so the queue is empty
    // behind it and then fills with the next NOTIFICATION_QUEUE_MAX posts.
    fwd();
    BOOST_REQUIRE(WaitFor([&] { return started == 1; }));

    for (std::size_t i = 0; i < interfaces::NOTIFICATION_QUEUE_MAX + 5; ++i) {
        fwd();
    }

    BOOST_CHECK(interfaces::DroppedNotificationCount() >= before + 5);

    // Five drops in one episode log the warning once and no resume line yet.
    BOOST_CHECK_EQUAL(log.Count("the GUI is not taking notifications"), 1U);
    BOOST_CHECK_EQUAL(log.Count("taking notifications again"), 0U);

    // Let one delivery through, then overload again while the queue is still not
    // empty. That is the same episode, so there is no new warning and no resume line.
    allowed = 1;
    BOOST_REQUIRE(WaitFor([&] { return started == 2; }));
    fwd();
    fwd();
    BOOST_CHECK(interfaces::DroppedNotificationCount() >= before + 6);
    BOOST_CHECK_EQUAL(log.Count("the GUI is not taking notifications"), 1U);
    BOOST_CHECK_EQUAL(log.Count("taking notifications again"), 0U);

    allowed = 1 << 30;
    BOOST_CHECK(WaitFor([&] { return calls == interfaces::NOTIFICATION_QUEUE_MAX + 2; }));
    BOOST_CHECK_EQUAL(calls, interfaces::NOTIFICATION_QUEUE_MAX + 2);

    // The episode ends once the dispatcher has caught up, and only then is the
    // count logged, once.
    BOOST_CHECK(WaitFor([&] { return log.Count("taking notifications again; dropped 6 ") == 1; }));
    BOOST_CHECK_EQUAL(log.Count("taking notifications again"), 1U);
    BOOST_CHECK_EQUAL(log.Count("the GUI is not taking notifications"), 1U);
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
    DispatchedScope scope(/*start=*/false);
    std::atomic<int> calls{0};
    auto fwd = interfaces::ForwardNotify([&] { ++calls; });

    const uint64_t before = interfaces::DroppedNotificationCount();
    fwd();

    BOOST_CHECK_EQUAL(interfaces::DroppedNotificationCount(), before + 1);
    BOOST_CHECK_EQUAL(calls, 0);
}

BOOST_AUTO_TEST_CASE(stop_destroys_queued_closures_outside_the_queue_mutex)
{
    DispatchedScope scope;
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<bool> started{false};
    std::atomic<bool> destroyed_on_stop_thread{false};
    std::atomic<std::thread::id> stop_thread{std::thread::id{}};

    auto blocker = interfaces::ForwardNotify([&] {
        started = true;
        WaitGate(gate);
    });
    auto fwd = interfaces::ForwardNotify([](const PostingArg&) {});
    std::function<void()> emit_again = [&] { blocker(); };

    blocker();
    BOOST_REQUIRE(WaitFor([&] { return started.load(); }));

    {
        PostingArg arg;
        arg.on_destroy = &emit_again;
        arg.stop_thread = &stop_thread;
        arg.destroyed_on_stop_thread = &destroyed_on_stop_thread;
        fwd(arg);
    }

    // Stop discards the queued copy on its own thread. Destroying it emits a
    // notification, which must not find the queue mutex held.
    std::thread stopper([&] {
        stop_thread = std::this_thread::get_id();
        interfaces::StopNotificationDispatcher();
    });
    const bool destroyed = WaitFor([&] { return destroyed_on_stop_thread.load(); });
    release.set_value();
    stopper.join();
    BOOST_CHECK(destroyed);
}

BOOST_AUTO_TEST_CASE(a_failing_post_is_contained_and_counted)
{
    DispatchedScope scope;
    auto fwd = interfaces::ForwardNotify([](const ThrowingCopy&) {});
    const uint64_t before = interfaces::DroppedNotificationCount();

    ThrowingCopy arg;
    BOOST_CHECK_NO_THROW(fwd(arg));
    BOOST_CHECK_EQUAL(interfaces::DroppedNotificationCount(), before + 1);
}

BOOST_AUTO_TEST_CASE(start_and_stop_are_idempotent_and_restartable)
{
    DispatchedScope scope;
    interfaces::StartNotificationDispatcher();

    std::thread a([] { interfaces::StopNotificationDispatcher(); });
    std::thread b([] { interfaces::StopNotificationDispatcher(); });
    a.join();
    b.join();
    interfaces::StopNotificationDispatcher();

    std::atomic<int> calls{0};
    auto fwd = interfaces::ForwardNotify([&] { ++calls; });
    for (int round = 0; round < 3; ++round) {
        interfaces::StartNotificationDispatcher();
        fwd();
        BOOST_REQUIRE(WaitFor([&] { return calls == round + 1; }));
        interfaces::StopNotificationDispatcher();
    }
}

BOOST_AUTO_TEST_CASE(real_forwarder_delivers_on_the_dispatcher_thread)
{
    DispatchedScope scope;
    const std::thread::id emitter = std::this_thread::get_id();
    std::thread::id ran;
    std::atomic<bool> done{false};

    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();
    std::unique_ptr<interfaces::Handler> handler = node->handleBannedListChanged([&] {
        ran = std::this_thread::get_id();
        done = true;
    });

    uiInterface.BannedListChanged();
    BOOST_REQUIRE(WaitFor([&] { return done.load(); }));
    BOOST_CHECK(ran != emitter);

    handler->disconnect();
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
