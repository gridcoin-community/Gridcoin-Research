// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/guieventlooptests.h"

#include "qt/guieventloop.h"
#include "qt/test/interfacefakes.h"

#include <QFuture>
#include <QThreadPool>
#include <QtConcurrent>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {
//! Bound on every wait in these tests, generous for the emulated CI legs.
constexpr std::chrono::seconds WAIT_BOUND{30};
//! How long the releaser waits before it lets the latched job finish.
constexpr std::chrono::milliseconds RELEASE_DELAY{500};

//! Stands in for StartGridcoinQt's `node`. Its checkForLatestUpdate() is the
//! call the About dialog's version check makes on the global pool. The call
//! copies its latch references on entry and never touches *this again, so a
//! job that outlives the node fails an assertion instead of reading freed
//! memory. The destructor records whether the job had finished.
class LatchedNode : public qt_test::FakeNode
{
public:
    LatchedNode(std::atomic<bool>& entered, std::atomic<bool>& release, std::atomic<bool>& job_done,
                std::atomic<bool>& timed_out, bool& destroyed, bool& job_done_at_destruction)
        : m_entered(entered), m_release(release), m_job_done(job_done), m_timed_out(timed_out),
          m_destroyed(destroyed), m_job_done_at_destruction(job_done_at_destruction)
    {
    }

    ~LatchedNode() override
    {
        m_job_done_at_destruction = m_job_done.load();
        m_destroyed = true;
    }

    interfaces::LatestVersionInfo checkForLatestUpdate() override
    {
        std::atomic<bool>& entered = m_entered;
        std::atomic<bool>& release = m_release;
        std::atomic<bool>& job_done = m_job_done;
        std::atomic<bool>& timed_out = m_timed_out;

        entered = true;
        const auto deadline = std::chrono::steady_clock::now() + WAIT_BOUND;
        while (!release.load()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        job_done = true;
        return {};
    }

private:
    std::atomic<bool>& m_entered;
    std::atomic<bool>& m_release;
    std::atomic<bool>& m_job_done;
    std::atomic<bool>& m_timed_out;
    bool& m_destroyed;
    bool& m_job_done_at_destruction;
};

//! Spins with 1 ms sleeps until `flag` is set. False if WAIT_BOUND expires first.
bool WaitUntilSet(const std::atomic<bool>& flag)
{
    const auto deadline = std::chrono::steady_clock::now() + WAIT_BOUND;
    while (!flag.load()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}
} // namespace

// Both tests record every failing condition into a local and check it only
// after the releaser is joined and the pool is idle: a QVERIFY that returned
// early would leave the job running against this frame, or destroy a joinable
// std::thread.

void GuiEventLoopTests::reapsBeforeNodeDiesOnUnwind()
{
    QThreadPool* pool = QThreadPool::globalInstance();
    QVERIFY2(pool->waitForDone(5000), "global pool busy before the test");

    std::atomic<bool> entered{false}, release{false}, job_done{false}, timed_out{false};
    bool destroyed = false, job_done_at_destruction = false;
    bool entered_ok = false, done_before_throw = true, threw = false;
    std::thread releaser;
    try {
        // Declared outside RunGuiEventLoop, as StartGridcoinQt declares `node`.
        LatchedNode latched(entered, release, job_done, timed_out, destroyed, job_done_at_destruction);
        interfaces::Node* node = &latched;
        RunGuiEventLoop([&] {
            // Dispatched the way the About dialog dispatches its version check.
            QFuture<void> job = QtConcurrent::run([node] { node->checkForLatestUpdate(); });
            entered_ok = WaitUntilSet(entered);
            // Read before the releaser exists, so false whenever the job is latched.
            done_before_throw = job_done.load();
            releaser = std::thread([&release] {
                std::this_thread::sleep_for(RELEASE_DELAY);
                release = true;
            });
            throw std::runtime_error("a teardown step threw");
        });
    } catch (const std::runtime_error&) {
        // `latched` has been destroyed by the time this handler runs.
        threw = true;
    }
    if (releaser.joinable()) releaser.join();
    pool->waitForDone();

    QVERIFY2(entered_ok, "the latched job never started");
    QVERIFY2(threw, "the teardown's exception did not reach the caller");
    QVERIFY2(!done_before_throw, "the job finished before the teardown threw: the test would be vacuous");
    QVERIFY2(destroyed, "the node was never destroyed");
    QVERIFY2(!timed_out.load(), "the latched job timed out");
    QVERIFY2(job_done_at_destruction, "the node was destroyed while a global-pool job that uses it was still running");
}

void GuiEventLoopTests::reapsBeforeReturn()
{
    QThreadPool* pool = QThreadPool::globalInstance();
    QVERIFY2(pool->waitForDone(5000), "global pool busy before the test");

    std::atomic<bool> entered{false}, release{false}, job_done{false}, timed_out{false};
    bool destroyed = false, job_done_at_destruction = false;
    bool entered_ok = false, done_in_run = true, done_at_return = false;
    std::thread releaser;
    {
        LatchedNode latched(entered, release, job_done, timed_out, destroyed, job_done_at_destruction);
        interfaces::Node* node = &latched;
        RunGuiEventLoop([&] {
            QFuture<void> job = QtConcurrent::run([node] { node->checkForLatestUpdate(); });
            entered_ok = WaitUntilSet(entered);
            // Read before the releaser exists, so false whenever the job is latched.
            done_in_run = job_done.load();
            releaser = std::thread([&release] {
                std::this_thread::sleep_for(RELEASE_DELAY);
                release = true;
            });
        });
        done_at_return = job_done.load();
    }
    if (releaser.joinable()) releaser.join();
    pool->waitForDone();

    QVERIFY2(entered_ok, "the latched job never started");
    QVERIFY2(!done_in_run, "the pool was reaped before the event loop ran");
    QVERIFY2(!timed_out.load(), "the latched job timed out");
    QVERIFY2(done_at_return, "RunGuiEventLoop returned with a global-pool job still running");
}
