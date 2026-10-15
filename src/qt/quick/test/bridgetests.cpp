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

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QMetaObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
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

namespace {

//! Set in the child that unloadableRootEndsTheGui starts, before GuiMain
//! builds its application object.
std::atomic<bool> g_guimain_child{false};

//! The child's front-end factory: a QML front end whose root does not exist.
std::unique_ptr<GuiFrontEnd> MakeUnloadableQmlFrontEnd()
{
    QmlFrontEnd::Callbacks callbacks{
        IsDaemonDisconnectMessage,
        [](const std::string&) {
            QuitOnDaemonConnectionLost("a QML front-end call was interrupted by the daemon disconnecting");
        }};
    return std::make_unique<QmlFrontEnd>(QUrl(QStringLiteral("qrc:/qmltest/missing.qml")), std::move(callbacks));
}

//! Runs when an application object is built. In the child only, it closes each
//! modal message box GuiMain shows, after printing its text, so the child never
//! waits for a user.
void CloseGuiMainMessageBoxes()
{
    if (!g_guimain_child) return;
    auto* timer = new QTimer(QCoreApplication::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            std::cout << "MESSAGE BOX: " << box->text().toStdString() << std::endl;
            box->close();
        }
    });
    timer->start(50);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(CloseGuiMainMessageBoxes)

int qml_test::RunGuiMainChild(int argc, char* argv[])
{
    // Every setting GuiMain reads or writes goes to an INI store under the
    // directory the parent names, on every platform: the native stores of macOS
    // and Windows are the user's own, and regtest shares mainnet's.
    const QByteArray settings_dir = qgetenv("GRC_QMLTEST_SETTINGS_DIR");
    if (settings_dir.isEmpty()) {
        std::cerr << "the GuiMain child needs GRC_QMLTEST_SETTINGS_DIR; it will not open the real settings" << std::endl;
        return EXIT_FAILURE;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QString::fromLocal8Bit(settings_dir));
    g_guimain_child = true;
    // GuiMain gets every argument but the child flag.
    std::vector<char*> args{argv[0]};
    for (int i = 2; i < argc; ++i) args.push_back(argv[i]);
    args.push_back(nullptr);
    const int rc = GuiMain(static_cast<int>(args.size()) - 1, args.data(), MakeUnloadableQmlFrontEnd);
    std::cout << "GUIMAIN RETURNED " << rc << std::endl;
    return rc;
}

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

void QmlBridgeTests::nativeWindowIdIsAHiddenWindow()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    const QWindowList before = QGuiApplication::topLevelWindows();
    QVERIFY(fe.construct());
    QWindowList added;
    for (QWindow* w : QGuiApplication::topLevelWindows()) {
        if (!before.contains(w)) added << w;
    }

    QVERIFY2(added.size() == 1, "construct() did not add exactly one top-level window");
    // Read before anything calls nativeWindowId(): winId() would create the
    // native window itself.
    QVERIFY2(added[0]->handle() != nullptr, "the anchor window has no native window after construct()");
    QVERIFY2(!added[0]->isVisible(), "the anchor window is visible");
    QCOMPARE(fe.nativeWindowId(), added[0]->winId());
    fe.destroyMain();
    QCOMPARE(fe.nativeWindowId(), WId(0));
}

void QmlBridgeTests::destroyMainClosesRunner()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY(fe.construct());
    fe.destroyMain();

    QVERIFY2(fe.callRunner().closed(), "destroyMain() left the call runner open");
    QVERIFY2(!fe.callRunner().postSerial([] {}, [] {}), "the call runner took a post after destroyMain()");
}

void QmlBridgeTests::hideMainHidesBeforeDrain()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    std::atomic<bool> hidden{false};
    std::atomic<bool> started{false};
    std::atomic<bool> saw_hidden{false};
    QObject::connect(&fe.provider().shell(), &ShellAdapter::hideMainRequested, &fe.provider().shell(),
                     [&hidden] { hidden = true; });
    const bool posted = fe.callRunner().postSerial(
        [&] {
            started = true;
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (!hidden && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }
            saw_hidden = hidden.load();
        },
        {});
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (!started && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    fe.detachModels();

    QVERIFY(posted);
    QVERIFY(started);
    QVERIFY2(saw_hidden, "the main window was hidden only after the in-flight serial call finished");
}

void QmlBridgeTests::hideMainHidesEveryTopLevelWindow()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    // A window the root does not know, as a view's createObject(window) makes.
    QWindow stray;
    stray.show();
    QVERIFY(stray.isVisible());
    fe.detachModels();

    QVERIFY2(!stray.isVisible(), "a top-level window the root does not know stayed visible after hideMain");
}

void QmlBridgeTests::rootLoadErrorsReachStderr()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(QUrl(QStringLiteral("qrc:/qmltest/missing.qml")), qml_test::CountingCallbacks(disconnects));
    std::ostringstream err;
    std::streambuf* const old_cerr = std::cerr.rdbuf(err.rdbuf());
    qml_test::ScopeExit restore([old_cerr] { std::cerr.rdbuf(old_cerr); });
    try {
        fe.construct();
    } catch (const GuiFrontEndUnusable&) {
    }
    std::cerr.rdbuf(old_cerr);

    QVERIFY2(err.str().find("QML: qrc:/qmltest/missing.qml") != std::string::npos,
             "the QML errors were not written to stderr");
}

void QmlBridgeTests::unloadableRootEndsTheGui()
{
    // GuiMain builds its own application object, so it runs in a child copy of
    // this binary, with a data directory and settings of its own.
    constexpr int CHILD_BOUND_MS = 120000;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString datadir = dir.filePath(QStringLiteral("datadir"));
    QVERIFY(QDir().mkpath(datadir));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), dir.filePath(QStringLiteral("config")));
    env.insert(QStringLiteral("GRC_QMLTEST_SETTINGS_DIR"), dir.filePath(QStringLiteral("config")));
    QProcess child;
    child.setProcessEnvironment(env);
    child.start(QCoreApplication::applicationFilePath(),
                {QString::fromLatin1(qml_test::GUIMAIN_CHILD_ARG), QStringLiteral("-regtest"),
                 QStringLiteral("-listen=0"), QStringLiteral("-datadir=") + datadir});
    QVERIFY2(child.waitForStarted(), "the GuiMain child did not start");
    const bool ended = child.waitForFinished(CHILD_BOUND_MS);
    if (!ended) {
        child.kill();
        child.waitForFinished();
    }
    const QString out = QString::fromLocal8Bit(child.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(child.readAllStandardError());
    const QByteArray tail = (QStringLiteral("\nchild stdout: ") + out.right(600) +
                             QStringLiteral("\nchild stderr: ") + err.right(600)).toLocal8Bit();

    QVERIFY2(ended, (QByteArray("the GuiMain child did not end") + tail).constData());
    QVERIFY2(out.contains(QStringLiteral("GUIMAIN RETURNED 1")),
             (QByteArray("GuiMain did not return: the process ended inside it") + tail).constData());
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), EXIT_FAILURE);
    QVERIFY2(out.contains(QStringLiteral("MESSAGE BOX: The GUI could not start:")),
             (QByteArray("no message box named the failure") + tail).constData());
    QVERIFY2(err.contains(QStringLiteral("Error: the GUI could not start: QML: the root qrc:/qmltest/missing.qml")),
             (QByteArray("the failure was not written to stderr") + tail).constData());
    QVERIFY2(QFileInfo::exists(dir.filePath(QStringLiteral("config/Gridcoin/Gridcoin-Qt.ini"))),
             "the child's settings did not go to the temp directory");
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

void QmlBridgeTests::constructFailsWhenRootDoesNotLoad()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(QUrl(QStringLiteral("qrc:/qmltest/missing.qml")), qml_test::CountingCallbacks(disconnects));

    // StartGridcoinQt then reports it in a message box and on stderr and
    // returns EXIT_FAILURE (unloadableRootEndsTheGui), instead of running
    // with no window.
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, fe.construct());
}
