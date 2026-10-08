// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "bridgetests.h"

#include "interfacefakes_qml.h"
#include "qmltestutil.h"

#include <qt/guifrontend.h>
#include <qt/quick/qmlcallrunner.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/shelladapter.h>
#include <qt/quick/splashadapter.h>

#include <QMetaObject>
#include <QSignalSpy>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

//!
//! \file bridgetests.cpp
//! \brief The core bridges' slots, the splash's parsing of the core's start-up
//! messages, and the front end's queued delivery of core messages, detach and
//! quit.
//!

namespace {

using namespace std::chrono_literals;

//! A front end whose first detach hook throws.
class ThrowingHideFrontEnd : public QmlFrontEnd
{
public:
    using QmlFrontEnd::QmlFrontEnd;

protected:
    void hideMain() override { throw std::runtime_error("hideMain threw"); }
};

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

//! Waits for every global-pool job when a test leaves, on every path.
class PoolReaper
{
public:
    ~PoolReaper() { QThreadPool::globalInstance()->waitForDone(); }
};

} // namespace

void QmlBridgeTests::bridgeSlotSignatures()
{
    qml_test::QueuingFrontEnd fe;
    QmlProvider provider(fe);
    QmlCoreBridge bridge(provider.shell());
    SplashAdapter splash(1);

    const bool error_ok = QMetaObject::invokeMethod(&bridge, "error", Qt::DirectConnection,
                                                    Q_ARG(QString, QStringLiteral("c")),
                                                    Q_ARG(QString, QStringLiteral("m")),
                                                    Q_ARG(bool, false));
    const bool update_ok = QMetaObject::invokeMethod(&bridge, "update", Qt::DirectConnection,
                                                     Q_ARG(QString, QStringLiteral("c")),
                                                     Q_ARG(QString, QStringLiteral("v")),
                                                     Q_ARG(int, 1),
                                                     Q_ARG(QString, QStringLiteral("m")));
    const bool uri_ok = QMetaObject::invokeMethod(&bridge, "handleURI", Qt::DirectConnection,
                                                  Q_ARG(QString, QStringLiteral("gridcoin:x")));
    const bool splash_ok = QMetaObject::invokeMethod(&splash, "showMessage", Qt::DirectConnection,
                                                     Q_ARG(QString, QStringLiteral("m")),
                                                     Q_ARG(int, 0));

    QVERIFY2(error_ok, "error(QString,QString,bool)");
    QVERIFY2(update_ok, "update(QString,QString,int,QString)");
    QVERIFY2(uri_ok, "handleURI(QString)");
    QVERIFY2(splash_ok, "showMessage(QString,int)");
}

void QmlBridgeTests::splashParsesInitMessages()
{
    SplashAdapter s(1);

    s.showMessage(QStringLiteral("1234/5678 Blocks Loaded (21%)"), 0);
    QCOMPARE(s.loaded(), 1234);
    QCOMPARE(s.total(), 5678);
    QCOMPARE(s.message(), QStringLiteral("1234/5678 Blocks Loaded (21%)"));

    s.showMessage(QStringLiteral("12/100 Blocks Verified"), 0);
    QCOMPARE(s.loaded(), 12);
    QCOMPARE(s.total(), 100);

    s.showMessage(QStringLiteral("Loading wallet..."), 0);
    QCOMPARE(s.loaded(), 0);
    QCOMPARE(s.total(), 0);
    QCOMPARE(s.message(), QStringLiteral("Loading wallet..."));
}

void QmlBridgeTests::splashMessageArrivesOnGuiThread()
{
    SplashAdapter s(1);
    std::atomic<QThread*> arrived_on{nullptr};
    QObject::connect(&s, &SplashAdapter::messageChanged, &s, [&arrived_on] { arrived_on = QThread::currentThread(); });

    // As bitcoin.cpp's InitMessage bridge does, from another thread.
    std::thread poster([&s] {
        QMetaObject::invokeMethod(&s, "showMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("1/2 Blocks Loaded (50%)")), Q_ARG(int, 0));
    });
    poster.join();
    qml_test::PumpUntil([&] { return arrived_on.load() != nullptr; }, 2s);

    QVERIFY(arrived_on.load() != nullptr);
    QCOMPARE(arrived_on.load(), qApp->thread());
}

void QmlBridgeTests::showMainSetsShellShown()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    ShellAdapter& shell = fe.provider().shell();
    QSignalSpy spy(&shell, &ShellAdapter::shownChanged);
    std::vector<std::string> order;
    QObject::connect(&shell, &ShellAdapter::showMainRequested, &shell,
                     [&order] { order.emplace_back("showMainRequested"); });
    QObject::connect(&shell, &ShellAdapter::shownChanged, &shell,
                     [&order] { order.emplace_back("shownChanged"); });

    fe.showMain(false);

    QCOMPARE(spy.count(), 1);
    QVERIFY(shell.shown());
    QVERIFY2(order == std::vector<std::string>({"showMainRequested", "shownChanged"}),
             "shownChanged was not emitted after showMainRequested");
}

void QmlBridgeTests::requestCloseDecidesQuitOrHide()
{
    std::atomic<int> disconnects{0};

    // Minimize-on-close off, tray available: the close quits.
    {
        qml_test::CountingQuitFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
        ShellAdapter& shell = fe.provider().shell();
        shell.setTrayAvailableProbe([] { return true; });
        shell.setMinimizeOnClose(false);
        QSignalSpy spy(&shell, &ShellAdapter::hideToTrayRequested);

        const bool closes = shell.requestClose();

        QVERIFY2(closes, "a close with minimize-on-close off did not go ahead");
        QCOMPARE(fe.quits, 1);
        QCOMPARE(spy.count(), 0);
    }

    // Minimize-on-close on, tray available: the window hides to the tray.
    {
        qml_test::CountingQuitFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
        ShellAdapter& shell = fe.provider().shell();
        shell.setTrayAvailableProbe([] { return true; });
        shell.setMinimizeOnClose(true);
        QSignalSpy spy(&shell, &ShellAdapter::hideToTrayRequested);

        const bool closes = shell.requestClose();

        QVERIFY2(!closes, "a close with a tray and minimize-on-close did not hide");
        QCOMPARE(fe.quits, 0);
        QCOMPARE(spy.count(), 1);
    }

    // Minimize-on-close on, no tray: a hidden window could not be restored, so
    // the close quits.
    {
        qml_test::CountingQuitFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
        ShellAdapter& shell = fe.provider().shell();
        shell.setTrayAvailableProbe([] { return false; });
        shell.setMinimizeOnClose(true);
        QSignalSpy spy(&shell, &ShellAdapter::hideToTrayRequested);

        const bool closes = shell.requestClose();

        QVERIFY2(closes, "a close with no tray did not go ahead");
        QCOMPARE(fe.quits, 1);
        QCOMPARE(spy.count(), 0);
    }

    // A quit already requested: the close goes ahead and asks for nothing more.
    {
        qml_test::CountingQuitFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
        ShellAdapter& shell = fe.provider().shell();
        shell.setQuitRequested();
        shell.setTrayAvailableProbe([] { return true; });
        shell.setMinimizeOnClose(true);
        QSignalSpy spy(&shell, &ShellAdapter::hideToTrayRequested);

        const bool closes = shell.requestClose();

        QVERIFY2(closes, "a close after a quit request did not go ahead");
        QCOMPARE(fe.quits, 0);
        QCOMPARE(spy.count(), 0);
    }
}

void QmlBridgeTests::throwingHideMainStillClosesRunner()
{
    QVERIFY2(QThreadPool::globalInstance()->waitForDone(5000), "global pool busy before the test");
    PoolReaper reaper;

    std::atomic<bool> flag{false};
    std::atomic<int> disconnects{0};
    ThrowingHideFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));

    try {
        fe.detachModels();
    } catch (const std::runtime_error&) {
    }
    const bool posted = fe.callRunner().postPooled([&] { flag = true; }, {});
    qml_test::PumpFor(500ms);

    QVERIFY2(!flag, "a pooled call ran after a detach whose first hook threw");
    QVERIFY(QThreadPool::globalInstance()->waitForDone(0));
    QVERIFY(!posted);
}

void QmlBridgeTests::requestQuitSetsQuitRequested()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QSignalSpy spy(&fe.provider().shell(), &ShellAdapter::quitRequestedChanged);

    // The call RequestGuiQuit() makes on the registered front end, outside
    // any event loop.
    fe.requestQuit();

    QVERIFY2(fe.provider().shell().quitRequested(), "requestQuit() did not record the quit on the shell");
    QCOMPARE(spy.count(), 1);
}

void QmlBridgeTests::modalMessageReachesShellWithoutWaiting()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QmlCoreBridge bridge(fe.provider().shell());
    QSignalSpy spy(&fe.provider().shell(), &ShellAdapter::coreMessage);
    std::mutex m;
    std::atomic<bool> returned{false};

    bool returned_unpumped = false;
    int spy_before_pump = -1;
    {
        qml_test::Watchdog dog("modalMessageReachesShellWithoutWaiting");
        std::thread poster([&] {
            std::lock_guard<std::mutex> lock(m);
            DeliverCoreMessage(&bridge, &fe, "caption", "message", true);
            returned = true;
        });
        // No event is processed here, so a poster that waits for the GUI
        // thread cannot return.
        const auto deadline = std::chrono::steady_clock::now() + 2000ms;
        while (!returned && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        returned_unpumped = returned.load();
        spy_before_pump = spy.count();
        if (returned_unpumped) {
            // The poster has let go of the lock the GUI thread wants.
            std::lock_guard<std::mutex> lock(m);
        }
        qml_test::PumpUntil([&] { return spy.count() == 1; }, std::chrono::seconds(5));
        poster.join();
    }

    QVERIFY2(returned_unpumped, "the modal caller waited for the QML front end");
    QCOMPARE(spy_before_pump, 0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("caption"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("message"));
    QCOMPARE(spy.at(0).at(2).toBool(), true);
}

void QmlBridgeTests::serialRunnerThreadNeverWaits()
{
    qml_test::QueuingFrontEnd ack;
    qml_test::CoreMessageTarget target;
    std::atomic<int> notified{0};
    std::mutex m;
    std::atomic<bool> holds{false};
    std::atomic<bool> done{false};
    CoreMessageDelivery outcome = CoreMessageDelivery::Queued;
    QmlCallRunner runner(CountingPolicy(notified));

    bool posted = false;
    bool held = false;
    {
        qml_test::Watchdog dog("serialRunnerThreadNeverWaits");
        posted = runner.postSerial(
            [&] {
                std::lock_guard<std::mutex> lock(m);
                holds = true;
                outcome = DeliverCoreMessage(&target, &ack, "c", "m", true);
                done = true;
            },
            {});
        // No event is pumped before the GUI thread takes `m`, so a worker that
        // waits for the GUI thread while holding `m` deadlocks deterministically.
        const auto deadline = std::chrono::steady_clock::now() + 2000ms;
        while (!holds && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        held = holds.load();
        if (posted && held) {
            {
                std::lock_guard<std::mutex> lock(m);
            }
            qml_test::PumpUntil([&] { return done.load(); }, qml_test::DEADLOCK_BOUND);
        }
        runner.drain();
    }

    QVERIFY(posted);
    QVERIFY2(held, "the job never took the lock");
    QVERIFY2(outcome == CoreMessageDelivery::QueuedAndLogged, "a serial-lane modal message was not delivered queued and logged");
}

void QmlBridgeTests::pooledRunnerThreadNeverWaits()
{
    QVERIFY2(QThreadPool::globalInstance()->waitForDone(5000), "global pool busy before the test");

    qml_test::QueuingFrontEnd ack;
    qml_test::CoreMessageTarget target;
    std::atomic<int> notified{0};
    std::mutex m;
    std::atomic<bool> holds{false};
    std::atomic<bool> done{false};
    CoreMessageDelivery outcome = CoreMessageDelivery::Queued;
    QmlCallRunner runner(CountingPolicy(notified));
    PoolReaper reaper;

    bool posted = false;
    bool held = false;
    {
        qml_test::Watchdog dog("pooledRunnerThreadNeverWaits");
        posted = runner.postPooled(
            [&] {
                std::lock_guard<std::mutex> lock(m);
                holds = true;
                outcome = DeliverCoreMessage(&target, &ack, "c", "m", true);
                done = true;
            },
            {});
        const auto deadline = std::chrono::steady_clock::now() + 2000ms;
        while (!holds && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        held = holds.load();
        if (posted && held) {
            {
                std::lock_guard<std::mutex> lock(m);
            }
            qml_test::PumpUntil([&] { return done.load(); }, qml_test::DEADLOCK_BOUND);
        }
        runner.drain();
        QThreadPool::globalInstance()->waitForDone();
    }

    QVERIFY(posted);
    QVERIFY2(held, "the job never took the lock");
    QVERIFY2(outcome == CoreMessageDelivery::QueuedAndLogged, "a pooled-lane modal message was not delivered queued and logged");
}
