// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "callrunnertests.h"

#include "qmltestutil.h"

#include <qt/quick/qmlcallrunner.h>

#include <QCoreApplication>
#include <QThread>
#include <QThreadPool>

#include <atomic>
#include <chrono>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

//!
//! \file callrunnertests.cpp
//! \brief QmlCallRunner: its two lanes, delivery on the GUI thread under a
//! generation, the disconnect policy, failure routing, and refusal once closed.
//!
//! Every test builds its own runner: closing is one-way, so none reuses a
//! runner after drain() or close().
//!

namespace {

using namespace std::chrono_literals;

//! A policy that treats both of libmultiprocess's disconnect messages as a lost
//! connection and counts each notification into `notified`.
QmlCallRunner::DisconnectPolicy CountingPolicy(std::atomic<int>& notified)
{
    return QmlCallRunner::DisconnectPolicy{
        [](const std::string& what) {
            return what.find("interrupted by disconnect") != std::string::npos ||
                   what.find("called after disconnect") != std::string::npos;
        },
        [&notified](const std::string&) { ++notified; }};
}

//! Waits without processing events, 1 ms at a time, until `flag` or `bound`.
bool SleepUntil(const std::atomic<bool>& flag, std::chrono::milliseconds bound)
{
    const auto deadline = std::chrono::steady_clock::now() + bound;
    while (!flag && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    return flag;
}

//! Waits for every global-pool job when a test leaves, on every path, so no job
//! outlives the locals it captured.
class PoolReaper
{
public:
    ~PoolReaper() { QThreadPool::globalInstance()->waitForDone(); }
};

} // namespace

void QmlCallRunnerTests::init()
{
    QVERIFY2(QThreadPool::globalInstance()->waitForDone(5000), "global pool busy before the test");
}

void QmlCallRunnerTests::cleanup()
{
    QThreadPool::globalInstance()->waitForDone();
}

void QmlCallRunnerTests::serialLaneNotBlockedByPooled()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    qml_test::Latch latch;
    std::atomic<bool> started{false};
    std::atomic<bool> serial_delivered{false};
    PoolReaper reaper;
    qml_test::ScopeExit open_latch([&latch] { latch.open = true; });

    const bool posted_pooled = runner.postPooled([&] { started = true; latch.wait(); }, {});
    qml_test::PumpUntil([&] { return started.load(); }, 2s);
    const bool posted_serial = runner.postSerial([] {}, [&] { serial_delivered = true; });
    const bool serial_ok = qml_test::PumpUntil([&] { return serial_delivered.load(); }, qml_test::DEADLOCK_BOUND);

    QVERIFY(posted_pooled && posted_serial);
    QVERIFY2(serial_ok, "a serial call waited behind a pooled one");
}

void QmlCallRunnerTests::drainDoesNotWaitForPooled()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    qml_test::Latch latch;
    std::atomic<bool> started{false};
    std::atomic<bool> delivered{false};
    PoolReaper reaper;
    qml_test::ScopeExit open_latch([&latch] { latch.open = true; });

    const bool posted = runner.postPooled([&] { started = true; latch.wait(); }, [&] { delivered = true; });
    qml_test::PumpUntil([&] { return started.load(); }, 2s);
    {
        qml_test::Watchdog dog("drainDoesNotWaitForPooled");
        runner.drain();
    }
    latch.open = true;
    QThreadPool::globalInstance()->waitForDone();
    qml_test::PumpFor(200ms);

    QVERIFY(posted);
    QVERIFY2(!delivered, "a pooled result was delivered after drain()");
}

void QmlCallRunnerTests::deliverRunsOnGuiThread()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    PoolReaper reaper;
    std::atomic<QThread*> serial_thread{nullptr};
    std::atomic<QThread*> pooled_thread{nullptr};

    const bool posted_serial = runner.postSerial([] {}, [&] { serial_thread = QThread::currentThread(); });
    const bool posted_pooled = runner.postPooled([] {}, [&] { pooled_thread = QThread::currentThread(); });
    qml_test::PumpUntil([&] { return serial_thread.load() != nullptr && pooled_thread.load() != nullptr; }, 2s);

    QVERIFY(posted_serial && posted_pooled);
    QVERIFY2(serial_thread == qApp->thread(), "a serial result was delivered off the GUI thread");
    QVERIFY2(pooled_thread == qApp->thread(), "a pooled result was delivered off the GUI thread");
}

void QmlCallRunnerTests::staleResultDropped()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    std::atomic<bool> ran{false};
    std::atomic<bool> delivered{false};

    const bool posted = runner.postSerial([&] { ran = true; }, [&] { delivered = true; });
    // No events are processed until the generation moves, so the delivery
    // cannot run first.
    SleepUntil(ran, 2000ms);
    std::this_thread::sleep_for(50ms);
    runner.bumpGeneration();
    qml_test::PumpFor(200ms);

    QVERIFY(posted);
    QVERIFY(ran);
    QVERIFY2(!delivered, "a result posted before bumpGeneration() was delivered");
}

void QmlCallRunnerTests::disconnectRoutedOnce()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    PoolReaper reaper;
    std::atomic<bool> delivered{false};

    const bool posted_serial = runner.postSerial(
        [] { throw std::runtime_error("IPC client method called after disconnect."); },
        [&] { delivered = true; });
    qml_test::PumpUntil([&] { return notified == 1; }, 2s);
    qml_test::PumpFor(200ms);
    const int serial_count = notified.load();

    const bool posted_pooled = runner.postPooled(
        [] { throw std::runtime_error("IPC client method called after disconnect."); },
        [&] { delivered = true; });
    qml_test::PumpUntil([&] { return notified == 2; }, 2s);
    qml_test::PumpFor(200ms);
    const int total_count = notified.load();

    QVERIFY(posted_serial && posted_pooled);
    QCOMPARE(serial_count, 1);
    QCOMPARE(total_count, 2);
    QVERIFY(!delivered);
}

void QmlCallRunnerTests::failureRethrownOnGuiThread()
{
    QVERIFY2(qml_test::g_rethrown.empty(), "an earlier test left a rethrown exception");
    qml_test::ScopeExit clear_rethrown([] { qml_test::g_rethrown.clear(); });

    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    PoolReaper reaper;
    std::atomic<bool> delivered{false};

    const bool posted_a = runner.postSerial([] { throw std::runtime_error("boom-serial"); }, [&] { delivered = true; });
    const bool posted_b = runner.postPooled([] { throw std::runtime_error("boom-pooled"); }, [&] { delivered = true; });
    const bool posted_c = runner.postSerial([] { throw 42; }, [&] { delivered = true; });
    qml_test::PumpUntil([&] { return qml_test::g_rethrown.size() == 3; }, 2s);
    qml_test::PumpFor(200ms);

    std::set<std::string> texts;
    bool all_on_gui_thread = true;
    for (const auto& entry : qml_test::g_rethrown) {
        texts.insert(entry.first);
        if (entry.second != qApp->thread()) all_on_gui_thread = false;
    }
    const std::set<std::string> expected{"boom-serial", "boom-pooled", "<non-std>"};

    QVERIFY(posted_a && posted_b && posted_c);
    QCOMPARE(notified.load(), 0);
    QCOMPARE(qml_test::g_rethrown.size(), size_t{3});
    QVERIFY2(texts == expected, "the rethrown texts are not the three the calls threw");
    QVERIFY2(all_on_gui_thread, "an exception was rethrown off the GUI thread");
    QVERIFY(!delivered);
}

void QmlCallRunnerTests::failureAfterCloseNotRethrown()
{
    QVERIFY2(qml_test::g_rethrown.empty(), "an earlier test left a rethrown exception");
    qml_test::ScopeExit clear_rethrown([] { qml_test::g_rethrown.clear(); });

    std::atomic<int> notified{0};
    qml_test::Latch latch;
    std::atomic<bool> started_s{false};
    std::atomic<bool> started_p{false};
    std::atomic<bool> delivered{false};
    QmlCallRunner runner(CountingPolicy(notified));
    PoolReaper reaper;
    qml_test::ScopeExit open_latch([&latch] { latch.open = true; });

    const bool posted_s = runner.postSerial(
        [&] {
            started_s = true;
            latch.wait();
            throw std::runtime_error("boom-closed");
        },
        [&] { delivered = true; });
    const bool posted_p = runner.postPooled(
        [&] {
            started_p = true;
            latch.wait();
            throw 42;
        },
        [&] { delivered = true; });
    const auto deadline = std::chrono::steady_clock::now() + 2000ms;
    while (!(started_s && started_p) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    runner.close();
    latch.open = true;
    runner.drain();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(2000));
    qml_test::PumpFor(200ms);

    QVERIFY(posted_s && posted_p);
    QVERIFY2(qml_test::g_rethrown.empty(), "a call that threw after close() was rethrown on the GUI thread");
    QCOMPARE(notified.load(), 0);
    QVERIFY(!delivered);
}

void QmlCallRunnerTests::drainWaitsForSerialCall()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    std::atomic<bool> started{false};
    std::atomic<bool> detached{false};
    std::atomic<int> hits{0};

    const bool posted = runner.postSerial(
        [&] {
            started = true;
            std::this_thread::sleep_for(200ms);
            if (detached) ++hits;
        },
        {});
    SleepUntil(started, 2000ms);
    runner.drain();
    detached = true;
    std::this_thread::sleep_for(300ms);

    QVERIFY(posted);
    QVERIFY2(hits == 0, "the serial call ran on after drain() returned");
}

void QmlCallRunnerTests::pooledLaneUsesGlobalPool()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    qml_test::Latch latch;
    std::atomic<bool> started{false};
    PoolReaper reaper;
    qml_test::ScopeExit open_latch([&latch] { latch.open = true; });

    const bool posted = runner.postPooled([&] { started = true; latch.wait(); }, {});
    qml_test::PumpUntil([&] { return started.load(); }, 2s);
    const bool busy = !QThreadPool::globalInstance()->waitForDone(0);
    latch.open = true;

    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QVERIFY(posted);
    QVERIFY2(busy, "the pooled job is not on the global pool");
}

void QmlCallRunnerTests::pooledPostRefusedAfterDrain()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    std::atomic<bool> flag{false};
    PoolReaper reaper;

    runner.drain();
    const bool posted = runner.postPooled([&] { flag = true; }, {});
    qml_test::PumpFor(500ms);

    QVERIFY2(!flag, "a pooled call ran after drain()");
    QVERIFY(QThreadPool::globalInstance()->waitForDone(0));
    QVERIFY2(!posted, "postPooled() accepted a call after drain()");
}

void QmlCallRunnerTests::serialPostRefusedAfterDrain()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    std::atomic<bool> flag{false};

    runner.drain();
    const bool posted = runner.postSerial([&] { flag = true; }, {});
    qml_test::PumpFor(500ms);

    QVERIFY2(!flag, "a serial call ran after drain()");
    QVERIFY2(!posted, "postSerial() accepted a call after drain()");
}

void QmlCallRunnerTests::queuedPooledJobDroppedAfterGenerationBump()
{
    std::atomic<int> notified{0};
    QmlCallRunner runner(CountingPolicy(notified));
    qml_test::Latch latch;
    std::atomic<int> started{0};
    std::atomic<bool> ran{false};
    PoolReaper reaper;
    qml_test::ScopeExit open_latch([&latch] { latch.open = true; });

    const int n = QThreadPool::globalInstance()->maxThreadCount();
    for (int i = 0; i < n; ++i) {
        QThreadPool::globalInstance()->start([&] {
            ++started;
            latch.wait();
        });
    }
    QTRY_COMPARE_WITH_TIMEOUT(started.load(), n, 2000);

    // Every pool thread is busy, so this job is queued, not started.
    const bool posted = runner.postPooled([&] { ran = true; }, {});
    runner.bumpGeneration();
    latch.open = true;
    const bool pool_idle = QThreadPool::globalInstance()->waitForDone(2000);

    QVERIFY(pool_idle);
    QVERIFY(posted);
    QVERIFY2(!ran, "a queued pooled call ran after bumpGeneration()");
}
