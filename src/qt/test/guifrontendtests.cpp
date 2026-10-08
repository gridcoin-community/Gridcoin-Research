// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/guifrontendtests.h"

#include "logging.h"
#include "qt/bitcoingui.h"
#include "qt/guifrontend.h"
#include "qt/splashscreen.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

//!
//! \file guifrontendtests.cpp
//! \brief The GuiFrontEnd seam's teardown contract (the detach order, its
//! idempotence, its resumption after a hook throws and the exception-path
//! guard), the slot signatures the string-invoked core bridges in
//! bitcoin.cpp depend on, and how DeliverCoreMessage routes a core message:
//! blocking for a front end that does not queue, queued and logged for one
//! that does.
//!

namespace {

//! Records each detach hook it receives, in call order. Its public virtuals are
//! trivial: the tests drive only the non-virtual detachModels() and the guard.
class FakeGuiFrontEnd : public GuiFrontEnd
{
public:
    std::vector<std::string> calls;
    //! Hooks named here record their call and then throw std::runtime_error, on
    //! their first call only.
    std::set<std::string> throw_in;
    //! How many times onDetachStarting() ran.
    int closing_calls{0};
    //! The value of closing_calls when hideMain() ran.
    int closing_calls_seen_by_hide{-1};
    //! Run by hideMain() after it records, when set.
    std::function<void()> on_hide_main;

    QObject* construct() override { return nullptr; }
    void destroyMain() override {}
    QObject* createSplash() override { return nullptr; }
    void finishSplash() override {}
    void destroySplash() override {}
    void showBuildMismatchWarning(const QString&, const QString&) override {}
    void setIpcConnectionInfo(const GuiIpcInfo&) override {}
    void attachModels(const ModelBundle&) override {}
    void showMain(bool) override {}
    WId nativeWindowId() override { return 0; }
    void requestQuit() override {}

protected:
    void hideMain() override
    {
        closing_calls_seen_by_hide = closing_calls;
        Record("hideMain");
        if (on_hide_main) on_hide_main();
    }
    void detachClient() override { Record("detachClient"); }
    void detachWallet() override { Record("detachWallet"); }
    void detachMRC() override { Record("detachMRC"); }
    void detachResearcher() override { Record("detachResearcher"); }
    void detachVoting() override { Record("detachVoting"); }
    void detachPSGT() override { Record("detachPSGT"); }
    void onDetachStarting() noexcept override { ++closing_calls; }

private:
    void Record(const std::string& name)
    {
        calls.emplace_back(name);
        if (throw_in.erase(name) > 0) throw std::runtime_error(name + " threw");
    }
};

//! A front end that queues modal core messages.
class QueuingFakeGuiFrontEnd : public FakeGuiFrontEnd
{
public:
    bool queuesModalCoreMessages() const override { return true; }
};

//! A deadlock never resolves, so a generous bound loses no power and keeps slow
//! CI legs green.
constexpr std::chrono::seconds DEADLOCK_BOUND{10};

//! Processes events until `done()` holds or `bound` passes; returns done().
bool PumpUntil(const std::function<bool()>& done, std::chrono::milliseconds bound)
{
    const auto deadline = std::chrono::steady_clock::now() + bound;
    while (std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return done();
}

//! Aborts the process, naming the test, if it is not destroyed within
//! DEADLOCK_BOUND: a test that deadlocks then fails loudly instead of hanging.
class Watchdog
{
public:
    explicit Watchdog(const char* name)
        : m_thread([this, name] {
              std::unique_lock<std::mutex> lock(m_mutex);
              if (!m_cv.wait_for(lock, DEADLOCK_BOUND, [this] { return m_disarmed; })) {
                  // C stdio: a test that points std::cerr elsewhere still
                  // gets its name reported.
                  std::fputs("WATCHDOG: ", stderr);
                  std::fputs(name, stderr);
                  std::fputs(" did not finish within 10 s\n", stderr);
                  std::fflush(stderr);
                  // With QTest's SIGABRT handler installed, an abort from this
                  // thread printed its signal report without end. Restore the
                  // default action first, so the process ends at once.
                  std::signal(SIGABRT, SIG_DFL);
                  std::abort();
              }
          })
    {
    }

    ~Watchdog()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_disarmed = true;
        }
        m_cv.notify_all();
        m_thread.join();
    }

    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_disarmed{false};
    std::thread m_thread;
};

//! Runs a function when it goes out of scope, on every exit path of a test.
class ScopeExit
{
public:
    explicit ScopeExit(std::function<void()> fn) : m_fn(std::move(fn)) {}
    ~ScopeExit()
    {
        if (m_fn) m_fn();
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

private:
    std::function<void()> m_fn;
};

//! Compare the recorded hooks element by element, so a failure names the first
//! hook that arrived out of order.
void CompareCalls(const std::vector<std::string>& calls, const std::vector<std::string>& expected)
{
    QCOMPARE(calls.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        QCOMPARE(QString::fromStdString(calls[i]), QString::fromStdString(expected[i]));
    }
}

} // namespace

// Outside the anonymous namespace: moc cannot process a Q_OBJECT class in one.
namespace gui_frontend_test {

//! A core-message target: error() counts into `calls` and records the last
//! `modal` flag and the thread it ran on.
class CoreMessageTarget : public QObject
{
    Q_OBJECT

public:
    std::atomic<int> calls{0};
    std::atomic<bool>* caller_returned{nullptr};
    bool saw_caller_returned{false};
    std::atomic<bool> last_modal{false};
    std::atomic<QThread*> slot_thread{nullptr};

public Q_SLOTS:
    void error(const QString& caption, const QString& message, bool modal)
    {
        Q_UNUSED(caption);
        Q_UNUSED(message);
        saw_caller_returned = caller_returned && caller_returned->load();
        last_modal = modal;
        slot_thread = QThread::currentThread();
        ++calls;
    }
};

} // namespace gui_frontend_test

void GUIFrontEndTests::detachSequence()
{
    FakeGuiFrontEnd fake;

    fake.detachModels();

    const std::vector<std::string> expected{
        "hideMain", "detachClient", "detachWallet", "detachMRC",
        "detachResearcher", "detachVoting", "detachPSGT"};
    CompareCalls(fake.calls, expected);
    if (QTest::currentTestFailed()) return;

    // Idempotent: the normal-path call and the exception-path guard can both run.
    fake.detachModels();
    CompareCalls(fake.calls, expected);
}

void GUIFrontEndTests::guardRunsDetachOnThrow()
{
    FakeGuiFrontEnd fake;

    try {
        FrontEndDetachGuard guard{fake};
        throw std::runtime_error("x");
    } catch (const std::runtime_error&) {
    }

    QCOMPARE(fake.calls.size(), size_t{7});
    QCOMPARE(QString::fromStdString(fake.calls.front()), QStringLiteral("hideMain"));
}

void GUIFrontEndTests::detachResumesAfterThrowingHook()
{
    FakeGuiFrontEnd fake;
    fake.throw_in = {"detachClient"};

    {
        FrontEndDetachGuard guard{fake};
        bool threw = false;
        size_t calls_at_throw = 0;
        try {
            fake.detachModels();
        } catch (const std::runtime_error&) {
            // detachClient threw out of the explicit call. Leaving the block runs
            // the guard, which must resume at the next hook.
            threw = true;
            calls_at_throw = fake.calls.size();
        }
        // detachModels() lets a hook's exception propagate to its caller.
        QVERIFY(threw);
        // The explicit call stopped at the hook that threw: only hideMain and
        // detachClient ran before the exception left it.
        QCOMPARE(calls_at_throw, size_t{2});
    }

    CompareCalls(fake.calls, {"hideMain", "detachClient", "detachWallet", "detachMRC",
                              "detachResearcher", "detachVoting", "detachPSGT"});
}

void GUIFrontEndTests::guardRunsEveryHookPastTwoThrows()
{
    FakeGuiFrontEnd fake;
    fake.throw_in = {"detachClient", "detachResearcher"};

    try {
        FrontEndDetachGuard guard{fake};
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error&) {
    }

    // The guard alone, during unwinding, gets past both throwing hooks.
    CompareCalls(fake.calls, {"hideMain", "detachClient", "detachWallet", "detachMRC",
                              "detachResearcher", "detachVoting", "detachPSGT"});
}

void GUIFrontEndTests::bridgeSlotSignatures()
{
    // These are the slots bitcoin.cpp's bridges invoke by name through
    // QMetaObject::invokeMethod. A rename or a parameter-type change compiles
    // cleanly and only fails at run time, so pin them here. A second front
    // end's targets must provide the same slots.
    const QMetaObject& gui = BitcoinGUI::staticMetaObject;
    for (const char* slot : {"error(QString,QString,bool)",
                             "update(QString,QString,int,QString)",
                             "handleURI(QString)"}) {
        QVERIFY2(gui.indexOfSlot(QMetaObject::normalizedSignature(slot)) >= 0, slot);
    }

    const QMetaObject& splash = SplashScreen::staticMetaObject;
    QVERIFY2(splash.indexOfSlot(QMetaObject::normalizedSignature("showMessage(QString,int)")) >= 0,
             "showMessage(QString,int)");
}

void GUIFrontEndTests::widgetsPathBlocksUntilSlotReturns()
{
    FakeGuiFrontEnd fake;
    gui_frontend_test::CoreMessageTarget target;
    std::atomic<bool> caller_returned{false};
    target.caller_returned = &caller_returned;

    CoreMessageDelivery outcome = CoreMessageDelivery::Queued;
    {
        Watchdog dog("widgetsPathBlocksUntilSlotReturns");
        std::thread caller([&] {
            outcome = DeliverCoreMessage(&target, &fake, "caption", "message", true);
            caller_returned = true;
        });
        PumpUntil([&] { return target.calls == 1; }, DEADLOCK_BOUND);
        caller.join();
    }

    QVERIFY2(outcome == CoreMessageDelivery::Blocking, "the Widgets path did not use the blocking connection");
    QVERIFY2(target.calls == 1, "the target's error slot did not run exactly once");
    QVERIFY2(!target.saw_caller_returned, "the caller returned before the target's error slot ran");
}

void GUIFrontEndTests::detachClosesBeforeFirstHook()
{
    FakeGuiFrontEnd fake;
    fake.detachModels();
    QCOMPARE(fake.closing_calls, 1);
    QVERIFY2(fake.closing_calls_seen_by_hide == 1, "hideMain ran before onDetachStarting()");

    // A detach resumed after a throwing hook does not run the step again.
    FakeGuiFrontEnd fake2;
    fake2.throw_in = {"hideMain"};
    try {
        fake2.detachModels();
    } catch (const std::runtime_error&) {
    }
    fake2.detachModels();
    QCOMPARE(fake2.closing_calls, 1);
}

void GUIFrontEndTests::queuedModalMessageNeverBlocksPostingThread()
{
    QueuingFakeGuiFrontEnd fe;
    gui_frontend_test::CoreMessageTarget target;
    std::mutex m;
    std::atomic<bool> returned{false};

    CoreMessageDelivery outcome = CoreMessageDelivery::Queued;
    bool returned_unpumped = false;
    int calls_before_pump = -1;
    int calls_after_gui_post = -1;
    {
        Watchdog dog("queuedModalMessageNeverBlocksPostingThread");
        std::thread poster([&] {
            std::lock_guard<std::mutex> lock(m);
            outcome = DeliverCoreMessage(&target, &fe, "caption", "message", true);
            returned = true;
        });
        // No event is processed here, so a poster that waits for the GUI
        // thread cannot return.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!returned && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        returned_unpumped = returned.load();
        calls_before_pump = target.calls.load();
        if (returned_unpumped) {
            // The poster has let go of the lock the GUI thread wants.
            std::lock_guard<std::mutex> lock(m);
        }
        PumpUntil([&] { return target.calls == 1; }, DEADLOCK_BOUND);
        poster.join();

        // On the GUI thread too, the message is posted, not run inline.
        DeliverCoreMessage(&target, &fe, "c2", "m2", true);
        calls_after_gui_post = target.calls.load();
        PumpUntil([&] { return target.calls == 2; }, std::chrono::seconds(2));
    }

    QVERIFY2(returned_unpumped, "the modal caller waited for the GUI thread");
    QCOMPARE(calls_before_pump, 0);
    QCOMPARE(target.calls.load(), 2);
    QVERIFY(target.last_modal.load());
    QVERIFY(target.slot_thread.load() == qApp->thread());
    QVERIFY(outcome == CoreMessageDelivery::QueuedAndLogged);
    QCOMPARE(calls_after_gui_post, 1);
}

void GUIFrontEndTests::queuedModalMessageIsLogged()
{
    QueuingFakeGuiFrontEnd fe;
    gui_frontend_test::CoreMessageTarget target;

    std::mutex log_mutex;
    std::vector<std::string> log_lines;
    std::ostringstream err_capture;
    std::streambuf* const old_cerr = std::cerr.rdbuf();
    // Restores std::cerr and puts the core logger back to buffering with no
    // callbacks, on every exit path.
    ScopeExit restore([old_cerr] {
        LogInstance().DisconnectTestLogger();
        std::cerr.rdbuf(old_cerr);
    });
    std::cerr.rdbuf(err_capture.rdbuf());
    LogInstance().PushBackCallback([&log_mutex, &log_lines](const std::string& line) {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_lines.push_back(line);
    });
    LogInstance().StartLogging();

    bool pumped = false;
    {
        Watchdog dog("queuedModalMessageIsLogged");
        std::thread poster([&] {
            DeliverCoreMessage(&target, &fe, "mcap", "mtext", true);
            DeliverCoreMessage(&target, &fe, "ncap", "ntext", false);
        });
        // Pumped before the join: a modal call that waits for the GUI thread
        // returns only once its slot has run.
        pumped = PumpUntil([&] { return target.calls == 2; }, std::chrono::seconds(5));
        poster.join();
    }

    const std::string err = err_capture.str();
    int err_count = 0;
    for (auto pos = err.find("mcap: mtext"); pos != std::string::npos; pos = err.find("mcap: mtext", pos + 1)) {
        ++err_count;
    }
    int log_count = 0;
    int log_ncap = 0;
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        for (const std::string& line : log_lines) {
            if (line.find("mcap: mtext") != std::string::npos) ++log_count;
            if (line.find("ncap") != std::string::npos) ++log_ncap;
        }
    }

    QVERIFY2(pumped, "the target's error slot did not run for both messages");
    QVERIFY2(err_count == 1, "a modal message was not written to stderr");
    QVERIFY2(err.find("ncap") == std::string::npos, "a non-modal message was written to stderr");
    QVERIFY2(log_count == 1, "a modal message was not written to the GUI log");
    QVERIFY2(log_ncap == 0, "a non-modal message was written to the GUI log");
}

void GUIFrontEndTests::detachGuardReapsGlobalPool()
{
    QVERIFY2(QThreadPool::globalInstance()->waitForDone(5000), "global pool busy before the test");

    bool done_after_normal = false;
    bool done_after_throw = false;
    for (const bool throwing : {false, true}) {
        FakeGuiFrontEnd fake;
        std::atomic<bool> done{false};
        std::atomic<bool> release{false};
        // Spins until released, so the pool is busy while the guard runs.
        QThreadPool::globalInstance()->start([&done, &release] {
            for (int i = 0; i < 30000 && !release; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            done = true;
        });
        std::thread helper([&release] {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            release = true;
        });
        try {
            FrontEndDetachGuard guard{fake};
            if (throwing) throw std::runtime_error("unwind");
        } catch (const std::runtime_error&) {
        }
        (throwing ? done_after_throw : done_after_normal) = done.load();
        helper.join();
        QThreadPool::globalInstance()->waitForDone();
    }

    QVERIFY2(done_after_normal, "the guard returned with a global-pool job still running");
    QVERIFY2(done_after_throw, "the guard returned with a global-pool job still running (throwing exit)");
}

void GUIFrontEndTests::detachGuardReapsJobStartedByHook()
{
    QVERIFY2(QThreadPool::globalInstance()->waitForDone(5000), "global pool busy before the test");

    std::atomic<bool> done{false};
    FakeGuiFrontEnd fake;
    fake.on_hide_main = [&done] {
        QThreadPool::globalInstance()->start([&done] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            done = true;
        });
    };
    try {
        FrontEndDetachGuard guard{fake};
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error&) {
    }
    const bool done_after_throw = done.load();
    QThreadPool::globalInstance()->waitForDone();

    QVERIFY2(done_after_throw, "the guard returned before a job its detach started had finished");
}

#include "guifrontendtests.moc"
