// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/coreconnectwaittests.h"

#include "qt/coreconnectwait.h"
#include "qt/guiipcworker.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QProgressDialog>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

//! What a fake attempt produced.
struct FakeAttempt
{
    int number{0};
    bool connected{false};
    bool retryable{false};
};

bool Connected(const FakeAttempt& attempt) { return attempt.connected; }
bool Retryable(const FakeAttempt& attempt, std::chrono::seconds) { return attempt.retryable; }

//! Fast pacing, so each case takes a fraction of a second.
CoreWaitTimings FastTimings()
{
    CoreWaitTimings timings;
    timings.poll = 10ms;
    timings.retry_interval = 200ms;
    return timings;
}

//! The waiting dialog, if one is showing.
QProgressDialog* VisibleWaitDialog()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* dialog = qobject_cast<QProgressDialog*>(widget);
        if (dialog && dialog->isVisible()) return dialog;
    }
    return nullptr;
}

//! QCoreApplication::exit() leaves the application marked as quitting, and
//! every later QEventLoop::exec() then returns at once. QCoreApplication::exec()
//! clears the mark; run it once, ended by an exit it has queued for itself.
void ClearApplicationQuit()
{
    QTimer::singleShot(0, qApp, [] { QCoreApplication::exit(0); });
    QCoreApplication::exec();
}

//! The thread \p worker runs its jobs on, or no thread if it does not run them.
std::thread::id ThreadOf(GuiIpcWorker& worker)
{
    std::thread::id id;
    const std::future<void> done = worker.Post([&id] { id = std::this_thread::get_id(); });
    if (!done.valid() || done.wait_for(10s) != std::future_status::ready) return {};
    return id;
}

const QString WAITING_TEXT{"Waiting for the Gridcoin core to start..."};
const QString CANCELING_TEXT{"Canceling..."};
} // namespace

void CoreConnectWaitTests::initTestCase()
{
    m_worker = std::make_unique<GuiIpcWorker>(GuiIpcWorker::Role::Modal);
}

void CoreConnectWaitTests::cleanupTestCase()
{
    m_worker.reset();
}

// A core that is up is attached at once: no dialog, and no wait for a retry
// interval before the result is looked at.
void CoreConnectWaitTests::firstAttemptThatConnectsShowsNoDialog()
{
    CoreWaitTimings timings;
    timings.retry_interval = 2000ms;
    std::atomic<int> attempts{0};
    const Clock::time_point start = Clock::now();
    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts] { return FakeAttempt{++attempts, /*connected=*/true, /*retryable=*/false}; },
        Connected, Retryable, timings, "test");
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);

    QVERIFY(outcome.last.connected);
    QCOMPARE(attempts.load(), 1);
    QVERIFY(!outcome.dialog_shown);
    QVERIFY(!outcome.canceled);
    QVERIFY2(took < 1500ms, qPrintable(QString("took %1 ms with a 2000 ms retry interval").arg(took.count())));
}

// A failure that is not retried (a timeout, another GUI attached) ends the wait
// at once, without the dialog.
void CoreConnectWaitTests::failureNotRetriedGivesUpWithoutDialog()
{
    std::atomic<int> attempts{0};
    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts] { return FakeAttempt{++attempts, /*connected=*/false, /*retryable=*/false}; },
        Connected, Retryable, FastTimings(), "test");

    QVERIFY(!outcome.last.connected);
    QCOMPARE(outcome.last.number, 1);
    QCOMPARE(attempts.load(), 1);
    QVERIFY(!outcome.dialog_shown);
    QVERIFY(!outcome.canceled);
}

// A core that is still starting: after the first failure that is retried the
// dialog shows, each attempt starts a retry interval after the previous one
// ended, and the wait ends when one connects.
void CoreConnectWaitTests::retriedFailureShowsDialogUntilConnected()
{
    const CoreWaitTimings timings = FastTimings();
    std::atomic<int> attempts{0};
    std::array<std::atomic<Clock::rep>, 3> start_ticks{};
    std::array<std::atomic<Clock::rep>, 3> end_ticks{};
    QString seen_label;
    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&seen_label] {
        if (QProgressDialog* dialog = VisibleWaitDialog()) seen_label = dialog->labelText();
    });
    observer.start(5);

    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts, &start_ticks, &end_ticks, &timings] {
            const Clock::rep start = Clock::now().time_since_epoch().count();
            const int n = ++attempts;
            // The first attempt takes a while (less than the interval), so
            // spacing from its end differs from spacing from its start.
            if (n == 1) std::this_thread::sleep_for(timings.retry_interval / 2);
            // Numbers start at 1, and the third attempt connects.
            if (n <= 3) {
                start_ticks[static_cast<size_t>(n - 1)] = start;
                end_ticks[static_cast<size_t>(n - 1)] = Clock::now().time_since_epoch().count();
            }
            return FakeAttempt{n, /*connected=*/n == 3, /*retryable=*/n < 3};
        },
        Connected, Retryable, timings, "test");
    observer.stop();

    QVERIFY(outcome.last.connected);
    QCOMPARE(outcome.last.number, 3);
    QCOMPARE(attempts.load(), 3);
    QVERIFY(outcome.dialog_shown);
    QVERIFY(!outcome.canceled);
    QCOMPARE(seen_label, WAITING_TEXT);
    const Clock::duration interval = timings.retry_interval;
    QVERIFY(Clock::duration{start_ticks[1] - end_ticks[0]} >= interval);
    QVERIFY(Clock::duration{start_ticks[2] - end_ticks[1]} >= interval);
}

// An attempt in flight for the retry interval brings the dialog up, so a core
// that is busy or does not answer is waited for visibly; it still attaches
// when it answers.
void CoreConnectWaitTests::slowAttemptShowsDialog()
{
    std::promise<void> dialog_seen;
    const std::shared_future<void> seen = dialog_seen.get_future().share();
    bool released = false;
    QString seen_label;
    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        QProgressDialog* dialog = VisibleWaitDialog();
        if (!dialog || released) return;
        seen_label = dialog->labelText();
        released = true;
        dialog_seen.set_value();
    });
    observer.start(5);

    std::atomic<int> attempts{0};
    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts, seen] {
            const int n = ++attempts;
            // Bounded: only a failing run waits this long.
            seen.wait_for(5s);
            return FakeAttempt{n, /*connected=*/true, /*retryable=*/false};
        },
        Connected, Retryable, FastTimings(), "test");
    observer.stop();

    QVERIFY(outcome.dialog_shown);
    QVERIFY2(released, "the dialog was not shown while the attempt was in flight");
    QCOMPARE(seen_label, WAITING_TEXT);
    QVERIFY(outcome.last.connected);
    QCOMPARE(attempts.load(), 1);
    QVERIFY(!outcome.canceled);
}

// Cancel while an attempt is in flight: the dialog stays up saying
// "Canceling..." with Cancel disabled, the GUI thread keeps running its event
// loop, and the wait ends once that attempt has finished. No further attempt
// starts.
void CoreConnectWaitTests::cancelWaitsForAttemptWithoutBlocking()
{
    std::promise<void> release;
    const std::shared_future<void> released = release.get_future().share();
    std::atomic<int> attempts{0};
    std::atomic<bool> second_started{false};
    std::atomic<bool> second_finished{false};
    bool clicked = false;
    bool saw_canceling = false;
    bool cancel_disabled = false;
    bool release_set = false;
    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        QProgressDialog* dialog = VisibleWaitDialog();
        if (!dialog) return;
        if (!clicked) {
            QPushButton* button = dialog->findChild<QPushButton*>();
            if (!second_started || !button) return;
            clicked = true;
            button->click();
            return;
        }
        if (release_set || dialog->labelText() != CANCELING_TEXT) return;
        // Seen from the GUI thread while the attempt is still in flight.
        saw_canceling = !second_finished;
        QPushButton* button = dialog->findChild<QPushButton*>();
        cancel_disabled = button && !button->isEnabled();
        release_set = true;
        release.set_value();
    });
    observer.start(5);

    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts, &second_started, &second_finished, released] {
            const int n = ++attempts;
            if (n == 1) return FakeAttempt{n, /*connected=*/false, /*retryable=*/true};
            second_started = true;
            // Bounded: only a failing run waits this long.
            released.wait_for(5s);
            second_finished = true;
            return FakeAttempt{n, /*connected=*/false, /*retryable=*/true};
        },
        Connected, Retryable, FastTimings(), "test");
    const bool finished_at_return = second_finished.load();
    observer.stop();

    QVERIFY(clicked);
    QVERIFY(outcome.canceled);
    QVERIFY(outcome.dialog_shown);
    QVERIFY2(saw_canceling, "the dialog did not stay up saying Canceling... while the attempt was in flight");
    QVERIFY(cancel_disabled);
    QVERIFY(finished_at_return);
    QCOMPARE(outcome.last.number, 2);
    QCOMPARE(attempts.load(), 2);
    QVERIFY(VisibleWaitDialog() == nullptr);
}

// Cancel while no attempt is in flight ends the wait, and no attempt starts
// after it, even from a timer event delivered after the loop was told to quit.
// The observer clicks Cancel in the idle time between attempts, waits until the
// next attempt would have been due, and lets the waiter's timer fire before the
// loop returns.
void CoreConnectWaitTests::cancelWhileIdleStartsNoAttempt()
{
    const CoreWaitTimings timings = FastTimings();
    std::atomic<int> attempts{0};
    bool clicked = false;
    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        QProgressDialog* dialog = VisibleWaitDialog();
        if (clicked || !dialog || attempts != 1) return;
        QPushButton* button = dialog->findChild<QPushButton*>();
        if (!button) return;
        clicked = true;
        const Clock::time_point clicked_at = Clock::now();
        button->click();
        // The first failure was taken before the dialog showed, so the next
        // attempt was due a retry interval from now at the latest.
        while (Clock::now() < clicked_at + timings.retry_interval + 2 * timings.poll) {
            std::this_thread::sleep_for(1ms);
        }
        QCoreApplication::processEvents();
    });
    observer.start(5);

    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts] { return FakeAttempt{++attempts, /*connected=*/false, /*retryable=*/true}; },
        Connected, Retryable, timings, "test");
    observer.stop();

    QVERIFY(clicked);
    QVERIFY(outcome.canceled);
    QCOMPARE(attempts.load(), 1);
    QCOMPARE(outcome.last.number, 1);
    QVERIFY(VisibleWaitDialog() == nullptr);
}

// An attempt that throws ends the wait as a failure that is not retried, even
// where every failure would be; what it threw is reported, and nothing escapes
// the event loop.
void CoreConnectWaitTests::attemptThatThrowsEndsTheWait()
{
    std::atomic<int> attempts{0};
    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts]() -> FakeAttempt {
            ++attempts;
            throw std::runtime_error("attempt failed");
        },
        Connected, [](const FakeAttempt&, std::chrono::seconds) { return true; }, FastTimings(), "test");

    QCOMPARE(QString::fromStdString(outcome.error), QString("attempt failed"));
    QVERIFY(!outcome.last.connected);
    QCOMPARE(outcome.last.number, 0);
    QCOMPARE(attempts.load(), 1);
    QVERIFY(!outcome.canceled);
}

// Every attempt runs on the process's modal worker, a thread that outlives the
// connection an attempt hands over, never on a thread that exits after it.
void CoreConnectWaitTests::attemptsRunOnTheModalWorker()
{
    QVERIFY(m_worker);
    const std::thread::id worker_thread = ThreadOf(*m_worker);
    QVERIFY(worker_thread != std::thread::id{});
    QVERIFY(worker_thread != std::this_thread::get_id());

    std::atomic<int> attempts{0};
    std::array<std::atomic<std::thread::id>, 3> threads{};
    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts, &threads] {
            const int n = ++attempts;
            if (n <= 3) threads[static_cast<size_t>(n - 1)] = std::this_thread::get_id();
            // The third attempt connects.
            return FakeAttempt{n, /*connected=*/n == 3, /*retryable=*/n < 3};
        },
        Connected, Retryable, FastTimings(), "test");

    QVERIFY(outcome.last.connected);
    QVERIFY(outcome.error.empty());
    QCOMPARE(attempts.load(), 3);
    for (const std::atomic<std::thread::id>& thread : threads) {
        QVERIFY(thread.load() == worker_thread);
    }
}

// Without the modal worker no attempt is made, on any thread, and the wait ends
// at once as for an attempt that failed unexpectedly.
void CoreConnectWaitTests::withoutTheModalWorkerNothingIsAttempted()
{
    m_worker.reset();
    QVERIFY(GuiIpcWorker::Current(GuiIpcWorker::Role::Modal) == nullptr);

    std::atomic<int> attempts{0};
    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts] { return FakeAttempt{++attempts, /*connected=*/true, /*retryable=*/false}; },
        Connected, [](const FakeAttempt&, std::chrono::seconds) { return true; }, FastTimings(), "test");

    m_worker = std::make_unique<GuiIpcWorker>(GuiIpcWorker::Role::Modal);

    QCOMPARE(attempts.load(), 0);
    QVERIFY(!outcome.last.connected);
    QVERIFY(!outcome.error.empty());
    QVERIFY(!outcome.canceled);
    QVERIFY(!outcome.dialog_shown);
}

// The event loop is told to quit from outside, as the application quitting
// does, while an attempt is in flight: the wait reports itself canceled and
// returns only after that attempt has ended, with events still processed
// meanwhile and the dialog saying "Canceling...".
void CoreConnectWaitTests::loopExitWaitsForAttemptWithoutBlocking()
{
    std::promise<void> release;
    const std::shared_future<void> released = release.get_future().share();
    std::atomic<int> attempts{0};
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    bool exited = false;
    int ticks_after_exit = 0;
    bool saw_canceling = false;
    bool release_set = false;
    QTimer observer;
    connect(&observer, &QTimer::timeout, this, [&] {
        if (!exited) {
            if (!started) return;
            exited = true;
            QCoreApplication::exit(0);
            return;
        }
        if (release_set || finished) return;
        // Seen from the GUI thread while the attempt is still in flight.
        ++ticks_after_exit;
        QProgressDialog* dialog = VisibleWaitDialog();
        if (dialog && dialog->labelText() == CANCELING_TEXT) saw_canceling = true;
        if (ticks_after_exit >= 3 && saw_canceling) {
            release_set = true;
            release.set_value();
        }
    });
    observer.start(5);

    const CoreWaitOutcome<FakeAttempt> outcome = WaitForCore<FakeAttempt>(
        [&attempts, &started, &finished, released] {
            const int n = ++attempts;
            started = true;
            // Bounded: only a failing run waits this long.
            released.wait_for(5s);
            finished = true;
            return FakeAttempt{n, /*connected=*/true, /*retryable=*/false};
        },
        Connected, Retryable, FastTimings(), "test");
    const bool finished_at_return = finished.load();
    observer.stop();

    // Before any check can return early: later event loops in this binary must run.
    ClearApplicationQuit();
    QEventLoop check;
    QTimer::singleShot(0, &check, [&check] { check.exit(7); });
    QCOMPARE(check.exec(), 7);

    QVERIFY(exited);
    QVERIFY2(ticks_after_exit >= 3, "events were not processed while the attempt was finished");
    QVERIFY(saw_canceling);
    QVERIFY(finished_at_return);
    QVERIFY(outcome.canceled);
    QVERIFY(outcome.dialog_shown);
    QVERIFY(outcome.last.connected);
    QCOMPARE(attempts.load(), 1);
    QVERIFY(VisibleWaitDialog() == nullptr);
}
