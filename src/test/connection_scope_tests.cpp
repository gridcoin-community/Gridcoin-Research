// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/connection_scope.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
//! Long enough that only a hang reaches it, so a loaded machine cannot turn a
//! correct run into a failure.
constexpr std::chrono::seconds WAIT_CAP{10};

//! Joins on every exit. A failed BOOST_REQUIRE throws past the explicit join, and
//! destroying a joinable std::thread calls std::terminate, which would take the
//! whole test binary down. The threads it joins wait on bounded gates only.
struct JoinOnExit {
    std::thread& m_thread;
    ~JoinOnExit() { if (m_thread.joinable()) m_thread.join(); }
};
} // namespace

BOOST_AUTO_TEST_SUITE(connection_scope_tests)

BOOST_AUTO_TEST_CASE(close_runs_hooks_once_in_order)
{
    auto s = interfaces::MakeConnectionScope();
    std::vector<int> seen;
    s->addCloseHook([&] { seen.push_back(1); });
    s->addCloseHook([&] { seen.push_back(2); });

    BOOST_CHECK(!s->closed());
    s->close();
    s->close();

    BOOST_CHECK((seen == std::vector<int>{1, 2}));
    BOOST_CHECK(s->closed());
}

BOOST_AUTO_TEST_CASE(removed_hook_does_not_run)
{
    auto s = interfaces::MakeConnectionScope();
    bool ran = false;
    bool other_ran = false;
    const auto id = s->addCloseHook([&] { ran = true; });
    s->addCloseHook([&] { other_ran = true; });

    s->removeCloseHook(id);
    s->close();

    BOOST_CHECK(!ran);
    BOOST_CHECK(other_ran);
}

BOOST_AUTO_TEST_CASE(hook_ids_are_nonzero_and_distinct)
{
    auto s = interfaces::MakeConnectionScope();
    const auto a = s->addCloseHook([] {});
    const auto b = s->addCloseHook([] {});
    BOOST_CHECK(a != 0U);
    BOOST_CHECK(b != 0U);
    BOOST_CHECK(a != b);

    // Removing an id twice, or one that was never issued, is harmless.
    s->removeCloseHook(a);
    s->removeCloseHook(a);
    s->removeCloseHook(0);
}

BOOST_AUTO_TEST_CASE(add_after_close_runs_immediately)
{
    auto s = interfaces::MakeConnectionScope();
    s->close();
    bool ran = false;

    BOOST_CHECK_EQUAL(s->addCloseHook([&] { ran = true; }), 0U);
    BOOST_CHECK(ran);

    // Removing the 0 it returned after close is a no-op.
    s->removeCloseHook(0);
}

// A hook that throws is logged and the remaining hooks still run: one broken
// teardown step must not leave a later one (a capability end, say) undone, and
// close() runs on the IPC event-loop thread, where an escaping exception would
// unwind through libmultiprocess.
BOOST_AUTO_TEST_CASE(throwing_hook_does_not_stop_the_others)
{
    auto s = interfaces::MakeConnectionScope();
    bool later_ran = false;
    s->addCloseHook([] { throw std::runtime_error("hook failure for the test"); });
    s->addCloseHook([&] { later_ran = true; });

    BOOST_CHECK_NO_THROW(s->close());
    BOOST_CHECK(later_ran);
    BOOST_CHECK(s->closed());
}

// removeCloseHook() returns only once a close() already running hooks has
// finished, so an owner that removes its hook and then frees what the hook uses
// cannot race the hook.
//
// The check is made where the remover returns: at that moment the hook must
// already have been released. A remover that did not wait would return while the
// hook is still blocked, and the check fails. The short pause before the release
// only gives the remover time to reach the wait; it is not what the test asserts,
// so a slow machine that has not got there yet passes rather than flakes.
BOOST_AUTO_TEST_CASE(remove_waits_for_running_hook)
{
    auto s = interfaces::MakeConnectionScope();

    std::promise<void> entered;
    std::promise<void> latch;
    const std::shared_future<void> latch_future{latch.get_future()};
    std::atomic<bool> released{false};
    std::atomic<int> runs{0};
    std::promise<void> remover_started;
    std::atomic<bool> remover_returned{false};
    std::atomic<bool> released_when_remover_returned{false};

    const auto id = s->addCloseHook([&] {
        ++runs;
        entered.set_value();
        latch_future.wait_for(WAIT_CAP);
    });

    std::thread closer;
    std::thread remover;
    JoinOnExit join_closer{closer};
    JoinOnExit join_remover{remover};

    closer = std::thread([&] { s->close(); });
    BOOST_REQUIRE(entered.get_future().wait_for(WAIT_CAP) == std::future_status::ready);

    remover = std::thread([&] {
        remover_started.set_value();
        s->removeCloseHook(id);
        released_when_remover_returned = released.load();
        remover_returned = true;
    });
    BOOST_REQUIRE(remover_started.get_future().wait_for(WAIT_CAP) == std::future_status::ready);
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    released = true;
    latch.set_value();
    closer.join();
    remover.join();

    BOOST_CHECK(remover_returned.load());
    BOOST_CHECK(released_when_remover_returned.load());
    BOOST_CHECK_EQUAL(runs.load(), 1);
    BOOST_CHECK(s->closed());
}

// A hook may let go of an object whose destructor removes its own hook from the
// same scope. That runs on the closing thread, so it must not wait for close() to
// finish -- it would wait for itself, and on the IPC event-loop thread that is a
// core that never serves another GUI. A regression here hangs the case.
BOOST_AUTO_TEST_CASE(remove_from_inside_a_hook_does_not_wait_for_itself)
{
    auto s = interfaces::MakeConnectionScope();
    interfaces::ConnectionScope::HookId self_id{0};
    bool later_ran = false;

    self_id = s->addCloseHook([&] { s->removeCloseHook(self_id); });
    s->addCloseHook([&] { later_ran = true; });

    s->close();
    BOOST_CHECK(later_ran);
    BOOST_CHECK(s->closed());
}

// Hooks run outside the scope's mutex, so a hook can use the scope itself: ask
// closed(), or add a hook, which the scope being closed runs at once.
BOOST_AUTO_TEST_CASE(hook_can_use_the_scope)
{
    auto s = interfaces::MakeConnectionScope();
    bool saw_closed = false;
    bool nested_ran = false;
    interfaces::ConnectionScope::HookId nested_id{1};

    s->addCloseHook([&] {
        saw_closed = s->closed();
        nested_id = s->addCloseHook([&] { nested_ran = true; });
    });

    s->close();
    BOOST_CHECK(saw_closed);
    BOOST_CHECK_EQUAL(nested_id, 0U);
    BOOST_CHECK(nested_ran);
}

BOOST_AUTO_TEST_SUITE_END()
