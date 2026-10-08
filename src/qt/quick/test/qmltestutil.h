// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_QMLTESTUTIL_H
#define BITCOIN_QT_QUICK_TEST_QMLTESTUTIL_H

#include <qt/quick/qmlfrontend.h>

#include <QApplication>
#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace qml_test {

//! A deadlock never resolves, so a generous bound loses no power and keeps slow
//! CI legs green.
constexpr std::chrono::seconds DEADLOCK_BOUND{10};

//! Processes events until `done()` holds or `bound` passes; returns done().
inline bool PumpUntil(const std::function<bool()>& done, std::chrono::milliseconds bound)
{
    const auto deadline = std::chrono::steady_clock::now() + bound;
    while (std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return done();
}

//! Processes events for `span`.
inline void PumpFor(std::chrono::milliseconds span)
{
    const auto deadline = std::chrono::steady_clock::now() + span;
    while (std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
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
                  std::cerr << "WATCHDOG: " << name << " did not finish within 10 s" << std::endl;
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

//! A gate a worker waits on until a test opens it; bounded, so a test that
//! forgets to open it fails instead of hanging.
class Latch
{
public:
    std::atomic<bool> open{false};

    void wait()
    {
        for (int i = 0; i < 30000 && !open; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
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

//! A core-message target: error() counts into `calls`.
class CoreMessageTarget : public QObject
{
    Q_OBJECT

public:
    std::atomic<int> calls{0};
    std::atomic<bool>* caller_returned{nullptr};
    bool saw_caller_returned{false};

public Q_SLOTS:
    void error(const QString& caption, const QString& message, bool modal)
    {
        Q_UNUSED(caption);
        Q_UNUSED(message);
        Q_UNUSED(modal);
        saw_caller_returned = caller_returned && caller_returned->load();
        ++calls;
    }
};

//! What QmlTestApplication swallowed: the text and the thread of each
//! exception, GUI thread only. main() checks it is empty at the end.
inline std::vector<std::pair<std::string, QThread*>> g_rethrown;

//! Stands in for GridcoinApplication::notify: it catches an exception from an
//! event handler instead of letting it abort the test binary, and records it.
//! What it swallows is checked at the end of main().
class QmlTestApplication : public QApplication
{
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override
    {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::exception& ex) {
            g_rethrown.push_back({ex.what(), QThread::currentThread()});
            return true;
        } catch (...) {
            g_rethrown.push_back({"<non-std>", QThread::currentThread()});
            return true;
        }
    }
};

//! A front end that counts the quits it is asked for instead of quitting.
class CountingQuitFrontEnd : public QmlFrontEnd
{
public:
    using QmlFrontEnd::QmlFrontEnd;

    //! GUI thread only.
    int quits{0};

    void requestQuit() override { ++quits; }
};

} // namespace qml_test

#endif // BITCOIN_QT_QUICK_TEST_QMLTESTUTIL_H
