// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "rootwiringtests.h"

#include "interfacefakes_qml.h"
#include "qmltestutil.h"

#include <qt/guifrontend.h>
#include <qt/quick/qmlfrontend.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/shelladapter.h>
#include <qt/quick/splashadapter.h>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQmlError>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QWindow>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <exception>
#include <thread>
#include <utility>

//!
//! \file rootwiringtests.cpp
//! \brief The carried root, WindowManager.qml, wired to Gridcoin.Qml: its
//! splash, main window, tray, core messages and quit, driven through the real
//! QmlFrontEnd. Under QApplication, QCoreApplication::quit() and a posted quit
//! event close only top-level widgets: no quit closes or consults a QML window,
//! so the root's close handler sees only the closes of its own windows.
//!

namespace {

using namespace std::chrono_literals;

//! The carried root, as gridcoinresearch-qml loads it.
const QUrl ROOT_URL{QStringLiteral("qrc:/qt/qml/Gridcoin/App/WindowManager.qml")};

//! Every message text Qt's message handler receives while CollectMessage is
//! installed; the previous handler still gets each message.
QStringList g_messages;
QtMessageHandler g_previous_handler = nullptr;

void CollectMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    g_messages << message;
    if (g_previous_handler) g_previous_handler(type, context, message);
}

//! A window the root holds in the QML property `name`, or null.
QQuickWindow* RootWindow(QmlFrontEnd& fe, const char* name)
{
    QObject* root = fe.rootObject();
    return root ? qobject_cast<QQuickWindow*>(root->property(name).value<QObject*>()) : nullptr;
}

//! The window of the core message on screen. Each message has its own window,
//! so callers read it afresh at each check.
QQuickWindow* MessageWindow(QmlFrontEnd& fe)
{
    return RootWindow(fe, "coreMessageWindow");
}

//! True while the message window is visible and shows this caption.
bool Showing(QmlFrontEnd& fe, const char* caption)
{
    QQuickWindow* window = MessageWindow(fe);
    return window && window->isVisible() && window->property("caption").toString() == QLatin1String(caption);
}

//! Clicks the message window's OK button: emits its clicked() signal.
bool ClickOk(QmlFrontEnd& fe)
{
    QQuickWindow* window = MessageWindow(fe);
    QObject* ok = window ? window->findChild<QObject*>(QStringLiteral("coreMessageOk")) : nullptr;
    return ok && QMetaObject::invokeMethod(ok, "clicked");
}

//! A core message, as the core bridge's slot delivers it to the shell.
void Notify(QmlFrontEnd& fe, const char* caption, const char* message, bool modal)
{
    fe.provider().shell().notifyCoreMessage(QString::fromLatin1(caption), QString::fromLatin1(message), modal);
}

//! Evaluates `expression` in the root's own context, where the root's ids and
//! imports resolve. `error` holds the engine's message when it fails.
QVariant Evaluate(QmlFrontEnd& fe, const QString& expression, QString& error)
{
    QObject* root = fe.rootObject();
    QQmlExpression expr(QQmlEngine::contextForObject(root), root, expression);
    QVariant value = expr.evaluate();
    error = expr.hasError() ? expr.error().toString() : QString();
    return value;
}

//! The front end's construct(), as GuiMain calls it, returning the core
//! bridge. If construct() throws, the throw is logged and nullptr returned,
//! so the test fails on its own root check instead of the exception ending
//! the test binary.
QObject* Construct(QmlFrontEnd& fe)
{
    try {
        return fe.construct();
    } catch (const std::exception& e) {
        qWarning().noquote() << "construct() threw:" << e.what();
        return nullptr;
    }
}

//! What GuiMain does before its event loop: the splash, the root, then the
//! main window. False when the root did not load.
bool Start(QmlFrontEnd& fe)
{
    fe.createSplash();
    Construct(fe);
    if (!fe.rootObject()) return false;
    fe.showMain(false);
    return true;
}

//! Two modal messages from a core thread: the thread never waits, and both are
//! shown in turn.
void CoreThreadMessages()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    fe.createSplash();
    QObject* bridge = Construct(fe);
    QVERIFY2(fe.rootObject(), "the root did not load");
    fe.showMain(false);

    std::atomic<bool> returned{false};
    CoreMessageDelivery first = CoreMessageDelivery::Blocking;
    CoreMessageDelivery second = CoreMessageDelivery::Blocking;
    bool returned_unpumped = false;
    {
        qml_test::Watchdog dog("coreMessages");
        std::thread poster([&] {
            first = DeliverCoreMessage(bridge, &fe, "c1", "m1", true);
            second = DeliverCoreMessage(bridge, &fe, "c2", "m2", true);
            returned = true;
        });
        // No event is processed here, so a poster that waits for the GUI
        // thread cannot return.
        const auto deadline = std::chrono::steady_clock::now() + 2000ms;
        while (!returned && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        returned_unpumped = returned.load();
        // A poster that waits is released here, so the join cannot hang.
        qml_test::PumpUntil([&] { return returned.load(); }, 2s);
        poster.join();
    }
    QVERIFY2(returned_unpumped, "DeliverCoreMessage waited for the GUI thread");
    QVERIFY(first == CoreMessageDelivery::QueuedAndLogged);
    QVERIFY(second == CoreMessageDelivery::QueuedAndLogged);

    QVERIFY2(qml_test::PumpUntil([&] {
                 return Showing(fe, "c1") && MessageWindow(fe)->property("text").toString() == QStringLiteral("m1");
             }, 2s),
             "the first message was not shown");
    QVERIFY(ClickOk(fe));
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c2"); }, 2s),
             "the second message was not shown after the first was dismissed");
    QVERIFY(ClickOk(fe));
    QVERIFY2(qml_test::PumpUntil([&] { return MessageWindow(fe) == nullptr; }, 2s),
             "the last message's window was not cleared");
}

//! A message closed by its window's close button, not by OK, is dismissed too.
void CloseShowsNext()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");

    Notify(fe, "c1", "m1", false);
    Notify(fe, "c2", "m2", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c1"); }, 2s), "the first message was not shown");
    MessageWindow(fe)->close();
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c2"); }, 2s),
             "closing the first message did not show the second");
}

//! After a quit, neither a queued message nor a later one is shown.
void QuitClearsQueue()
{
    std::atomic<int> disconnects{0};
    qml_test::ForwardingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");

    Notify(fe, "c1", "m1", false);
    Notify(fe, "c2", "m2", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c1"); }, 2s), "the first message was not shown");
    // Outside the event loop the quit only sets Shell.quitRequested.
    fe.requestQuit();
    MessageWindow(fe)->close();
    qml_test::PumpFor(200ms);
    QVERIFY2(MessageWindow(fe) == nullptr, "a message queued before the quit was shown after it");

    Notify(fe, "q1", "late", true);
    qml_test::PumpFor(200ms);
    QVERIFY2(MessageWindow(fe) == nullptr, "a message posted after the quit was shown");
}

//! One message at a time: arrivals while a message is open wait for it, in
//! arrival order.
void OneAtATime()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");

    Notify(fe, "c1", "m1", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c1"); }, 2s), "the first message was not shown");
    MessageWindow(fe)->close();
    Notify(fe, "c2", "m2", false);
    Notify(fe, "c3", "m3", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c2"); }, 2s), "the second message was not shown");
    qml_test::PumpFor(200ms);
    QVERIFY2(Showing(fe, "c2"), "a later message replaced the open one");
    Notify(fe, "c4", "m4", false);
    qml_test::PumpFor(200ms);
    QVERIFY2(Showing(fe, "c2"), "a message that arrived while one was open replaced it");
    QVERIFY(ClickOk(fe));
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c3"); }, 2s),
             "the next message in arrival order was not shown");
}

//! The next message is shown only after the closing window's close has
//! finished: QWindow::close() runs the close handler before it returns.
void DeferredShow()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");

    Notify(fe, "c1", "m1", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c1"); }, 2s), "the first message was not shown");
    Notify(fe, "c2", "m2", false);
    qml_test::PumpFor(200ms);
    QVERIFY2(Showing(fe, "c1"), "the second message replaced the open one");

    QPointer<QQuickWindow> w1 = MessageWindow(fe);
    w1->close();
    QVERIFY2(MessageWindow(fe) == w1.data() && !w1->isVisible(),
             "the next message was shown inside the closing window's close");
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c2"); }, 2s), "the second message was not shown");
    // destroy() is a deferred delete: process it now, so the check does not
    // depend on when the event loop gets to it.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY2(w1.isNull(), "the dismissed message's window was not destroyed");
}

//! A message shown before the main window stays above the main window.
void ShownBeforeMain()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    fe.createSplash();
    Construct(fe);
    QVERIFY2(fe.rootObject(), "the root did not load");

    Notify(fe, "c1", "m1", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "c1"); }, 2s), "the message was not shown");
    fe.showMain(false);
    QVERIFY2(qml_test::PumpUntil([&] {
                 return MessageWindow(fe) != nullptr && QGuiApplication::focusWindow() == MessageWindow(fe);
             }, 2s),
             "showing the main window took the focus from the message");
    QQuickWindow* main = RootWindow(fe, "mainWindow");
    QVERIFY(main && main->isVisible());
}

//! A window takes its message's modality before it is shown, so a modal
//! message's window is registered as the application's modal window.
void Modality()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");

    Notify(fe, "g1", "x", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "g1"); }, 2s), "the non-modal message was not shown");
    QCOMPARE(MessageWindow(fe)->modality(), Qt::NonModal);
    QVERIFY(QGuiApplication::modalWindow() != MessageWindow(fe));
    QVERIFY(ClickOk(fe));

    Notify(fe, "g2", "y", true);
    QVERIFY2(qml_test::PumpUntil([&] {
                 return Showing(fe, "g2") && MessageWindow(fe)->modality() == Qt::ApplicationModal;
             }, 2s),
             "the modal message was not shown application-modal");
    QVERIFY2(QGuiApplication::modalWindow() == MessageWindow(fe),
             "the modal message's window is not the application's modal window");
}

//! The root logs a non-modal message once and a modal one never: the front end
//! logs each modal message when it is posted.
void WhoLogs()
{
    g_messages.clear();
    g_previous_handler = qInstallMessageHandler(CollectMessage);
    qml_test::ScopeExit restore_handler([] { qInstallMessageHandler(g_previous_handler); });

    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");

    Notify(fe, "mc", "mtext", true);
    Notify(fe, "nc", "ntext", false);
    const bool logged = qml_test::PumpUntil([&] {
        for (const QString& message : std::as_const(g_messages)) {
            if (message.contains(QLatin1String("ntext"))) return true;
        }
        return false;
    }, 2s);
    QVERIFY2(logged, "the root did not log the non-modal message");
    qml_test::PumpFor(200ms);
    int nonmodal = 0;
    bool modal = false;
    for (const QString& message : std::as_const(g_messages)) {
        if (message.contains(QLatin1String("core message: nc: ntext"))) ++nonmodal;
        if (message.contains(QLatin1String("mtext"))) modal = true;
    }
    QCOMPARE(nonmodal, 1);
    QVERIFY2(!modal, "the root logged a modal message, which the front end logs when it is posted");
}

//! Arrival order whatever the modality: a modal message waits for an open
//! non-modal one.
void FifoAcrossModality()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");

    Notify(fe, "n1", "x", false);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "n1"); }, 2s), "the non-modal message was not shown");
    Notify(fe, "m1", "y", true);
    qml_test::PumpFor(200ms);
    QVERIFY2(Showing(fe, "n1"), "the modal message replaced the open non-modal one");
    QCOMPARE(MessageWindow(fe)->modality(), Qt::NonModal);
    QVERIFY(QGuiApplication::modalWindow() != MessageWindow(fe));
    QVERIFY(ClickOk(fe));
    QVERIFY2(qml_test::PumpUntil([&] {
                 return Showing(fe, "m1") && MessageWindow(fe)->modality() == Qt::ApplicationModal;
             }, 2s),
             "the modal message was not shown after the non-modal one");
    QVERIFY2(QGuiApplication::modalWindow() == MessageWindow(fe),
             "the modal message's window is not the application's modal window");
}

} // namespace

void QmlRootWiringTests::rootBindsNoLegacyNames()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    g_messages.clear();
    g_previous_handler = qInstallMessageHandler(CollectMessage);
    qml_test::ScopeExit restore_handler([] { qInstallMessageHandler(g_previous_handler); });

    std::atomic<int> disconnects{0};
    {
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        fe.createSplash();
        Construct(fe);
        QVERIFY2(fe.rootObject(), "the root did not load");
        fe.showMain(false);
        // Only a shown message creates its window, so one message loads the
        // message window's file.
        Notify(fe, "t1", "x", false);
        QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "t1"); }, 2s), "the core message was not shown");
        MessageWindow(fe)->close();
        QVERIFY2(qml_test::PumpUntil([&] { return MessageWindow(fe) == nullptr; }, 2s),
                 "the dismissed message's window was not cleared");
        // showAboutWindow is the About window's only creator, so the call
        // loads the window's file.
        QMetaObject::invokeMethod(fe.rootObject(), "showAboutWindow");
        QVERIFY2(qml_test::PumpUntil([&] { QQuickWindow* about = RootWindow(fe, "aboutWindow"); return about && about->isVisible(); }, 2s), "the About window was not shown");
    }

    // No models are attached, so no version window and no recent-list
    // delegate is created: those two files are listed, but only the others
    // are evaluated here.
    const QStringList files{QStringLiteral("SplashScreen.qml"), QStringLiteral("WindowManager.qml"),
                            QStringLiteral("MainWindow.qml"), QStringLiteral("TabMenu.qml"),
                            QStringLiteral("CoreMessageWindow.qml"), QStringLiteral("OverviewView.qml"),
                            QStringLiteral("StatusFooter.qml"), QStringLiteral("RecentTransactionItem.qml"),
                            QStringLiteral("AboutWindow.qml"), QStringLiteral("BuildMismatchBanner.qml"),
                            QStringLiteral("VersionInfoWindow.qml"), QStringLiteral("UnlockDialog.qml"),
                            QStringLiteral("WalletPromptDialog.qml"), QStringLiteral("WalletPrompts.qml")};
    QStringList problems;
    for (const QString& message : std::as_const(g_messages)) {
        for (const QString& file : files) {
            if (message.contains(QStringLiteral("qrc:/qt/qml/Gridcoin/App/") + file)) problems << message;
        }
    }
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void QmlRootWiringTests::splashFollowsSplashState()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    // As in GuiMain, the splash is active before the root loads.
    fe.createSplash();
    Construct(fe);
    QVERIFY2(fe.rootObject(), "the root did not load");

    QPointer<QQuickWindow> splash = RootWindow(fe, "splashWindow");
    QVERIFY2(splash, "the root did not create the splash");
    QCOMPARE(splash->objectName(), QStringLiteral("splashScreen"));
    QVERIFY2(qml_test::PumpUntil([&] { return splash->isVisible(); }, 2s), "the splash was not shown");
    QObject* progress = splash->findChild<QObject*>(QStringLiteral("splashProgress"));
    QVERIFY(progress);

    fe.provider().splash().showMessage(QStringLiteral("12/100 Blocks Loaded (12%)"), 0);
    QVERIFY2(qml_test::PumpUntil([&] { return qAbs(progress->property("value").toReal() - 0.12) < 1e-6; }, 2s),
             "the progress bar does not follow Splash.loaded and Splash.total");

    fe.finishSplash();
    QVERIFY2(qml_test::PumpUntil([&] { return splash.isNull() || !splash->isVisible(); }, 2s),
             "the splash did not close when Splash.finished was set");
}

void QmlRootWiringTests::showMainFollowsShell()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    std::atomic<int> disconnects{0};
    {
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        fe.createSplash();
        Construct(fe);
        QVERIFY2(fe.rootObject(), "the root did not load");

        fe.showMain(false);
        QPointer<QQuickWindow> main = RootWindow(fe, "mainWindow");
        QVERIFY2(main, "showMain did not create the main window");
        QVERIFY2(qml_test::PumpUntil([&] { return main->isVisible(); }, 2s), "showMain did not show the main window");

        fe.showMain(false);
        QCOMPARE(RootWindow(fe, "mainWindow"), main.data());
        int mains = 0;
        for (QQuickWindow* window : fe.rootObject()->findChildren<QQuickWindow*>()) {
            if (window->objectName() == QStringLiteral("mainWindow")) ++mains;
        }
        QCOMPARE(mains, 1);

        fe.showMain(true);
        QVERIFY2(qml_test::PumpUntil([&] { return main->visibility() == QWindow::Minimized; }, 2s),
                 "showMain(true) did not minimize the main window");

        // The next show un-minimizes the window.
        fe.showMain(false);
        QCOMPARE(RootWindow(fe, "mainWindow"), main.data());
        QVERIFY2(qml_test::PumpUntil([&] { return main->isVisible() && main->visibility() == QWindow::Windowed; }, 2s),
                 "showMain(false) did not restore the minimized main window");
    }

    // The -min start: GuiMain creates no splash, and the first show minimizes
    // a window that was never shown.
    {
        qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        Construct(fe);
        QVERIFY2(fe.rootObject(), "the root did not load");

        fe.showMain(true);
        QPointer<QQuickWindow> main = RootWindow(fe, "mainWindow");
        QVERIFY2(main, "showMain(true) did not create the main window");
        QVERIFY2(qml_test::PumpUntil([&] { return main->isVisible() && main->visibility() == QWindow::Minimized; }, 2s),
                 "a first showMain(true) did not show the main window minimized");

        // showMain has set Shell.shown, so a tray click restores the window.
        QString error;
        Evaluate(fe, QStringLiteral("trayIcon.activated(SystemTrayIcon.Trigger)"), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(qml_test::PumpUntil([&] { return main->visibility() == QWindow::Windowed; }, 2s),
                 "a tray click did not restore the minimized main window");
    }
}

void QmlRootWiringTests::coreMessages()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    // Each case loads its own front end and root, so each starts with no
    // message queued or shown.
    CoreThreadMessages();
    if (QTest::currentTestFailed()) return;
    CloseShowsNext();
    if (QTest::currentTestFailed()) return;
    QuitClearsQueue();
    if (QTest::currentTestFailed()) return;
    OneAtATime();
    if (QTest::currentTestFailed()) return;
    DeferredShow();
    if (QTest::currentTestFailed()) return;
    ShownBeforeMain();
    if (QTest::currentTestFailed()) return;
    Modality();
    if (QTest::currentTestFailed()) return;
    WhoLogs();
    if (QTest::currentTestFailed()) return;
    FifoAcrossModality();
}

void QmlRootWiringTests::closeQuitsOrHidesToTray()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    std::atomic<int> disconnects{0};

    // No tray: closing the main window quits, inside the event loop.
    {
        qml_test::ForwardingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        fe.provider().shell().setTrayAvailableProbe([] { return false; });
        QVERIFY2(Start(fe), "the root did not load");
        QQuickWindow* main = RootWindow(fe, "mainWindow");
        QVERIFY(main);

        // The event loop's bound: a close that does not quit ends it with 2.
        QTimer bound;
        bound.setSingleShot(true);
        bound.setInterval(2000);
        QObject::connect(&bound, &QTimer::timeout, &bound, [] { QCoreApplication::exit(2); });
        bound.start();
        QMetaObject::invokeMethod(main, "close", Qt::QueuedConnection);
        int rc = -1;
        {
            qml_test::Watchdog dog("closeQuitsOrHidesToTray");
            rc = qApp->exec();
        }
        bound.stop();
        QCOMPARE(fe.quits, 1);
        QVERIFY(fe.provider().shell().quitRequested());
        QCOMPARE(rc, 0);
    }

    // A tray and minimize-on-close: the close hides the window, and the tray
    // brings the same window back.
    {
        qml_test::ForwardingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
        ShellAdapter& shell = fe.provider().shell();
        QVERIFY2(Start(fe), "the root did not load");
        QPointer<QQuickWindow> main = RootWindow(fe, "mainWindow");
        QVERIFY(main);

        shell.setTrayAvailableProbe([] { return true; });
        shell.setMinimizeOnClose(true);
        QString error;
        Evaluate(fe, QStringLiteral("mainWindow.showMaximized()"), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(qml_test::PumpUntil([&] { return main->visibility() == QWindow::Maximized; }, 2s),
                 "the main window was not maximized");
        main->close();
        QVERIFY2(qml_test::PumpUntil([&] { return !main->isVisible(); }, 2s),
                 "a close with a tray and minimize-on-close did not hide the main window");
        QCOMPARE(fe.quits, 0);

        // A core message shown while the main window is hidden leaves it hidden.
        Notify(fe, "t5", "x", false);
        QVERIFY2(qml_test::PumpUntil([&] {
                     return Showing(fe, "t5") && QGuiApplication::focusWindow() == MessageWindow(fe);
                 }, 2s),
                 "the core message was not shown and activated");
        QVERIFY2(!main->isVisible(), "showing a core message brought the hidden main window back");

        Evaluate(fe, QStringLiteral("trayIcon.activated(SystemTrayIcon.Trigger)"), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(qml_test::PumpUntil([&] {
                     return main && main->isVisible() && main->visibility() == QWindow::Maximized;
                 }, 2s),
                 "the tray did not restore the main window maximized");
        QCOMPARE(RootWindow(fe, "mainWindow"), main.data());

        fe.showMain(false);
        QCOMPARE(RootWindow(fe, "mainWindow"), main.data());

        // Once a quit was requested the tray restores nothing.
        main->hide();
        shell.setQuitRequested();
        Evaluate(fe, QStringLiteral("trayIcon.activated(SystemTrayIcon.Trigger)"), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        qml_test::PumpFor(200ms);
        QVERIFY2(!main->isVisible(), "the tray restored the main window after a quit was requested");
    }
}

void QmlRootWiringTests::quitBeforeTheLoopStillQuits()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    std::atomic<int> disconnects{0};
    qml_test::ForwardingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    fe.createSplash();
    Construct(fe);
    QVERIFY2(fe.rootObject(), "the root did not load");

    QString error;
    const bool enabled_before = Evaluate(fe, QStringLiteral("trayQuit.enabled"), error).toBool();
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY2(!enabled_before, "the tray's Quit was enabled before the main window was shown");

    // A quit handled before the event loop runs, as StartGridcoinQt's readiness wait
    // handles a queued quit request: outside the loop it only records the quit.
    fe.requestQuit();
    fe.showMain(false);
    QVERIFY2(RootWindow(fe, "mainWindow") == nullptr, "the main window was created after a quit was requested");
    const bool enabled_after = Evaluate(fe, QStringLiteral("trayQuit.enabled"), error).toBool();
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY2(enabled_after, "the tray's Quit was not enabled once the main window was requested");

    // Nothing processes events before the loop starts, as in StartGridcoinQt.
    QTimer bound;
    bound.setSingleShot(true);
    bound.setInterval(2000);
    QObject::connect(&bound, &QTimer::timeout, &bound, [] { QCoreApplication::exit(2); });
    bound.start();
    int rc = -1;
    {
        qml_test::Watchdog dog("quitBeforeTheLoopStillQuits");
        rc = qApp->exec();
    }
    bound.stop();
    QCOMPARE(rc, 0);
    QCOMPARE(fe.quits, 1);
}

void QmlRootWiringTests::hideMainHidesTheRootsWindows()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");
    QString error;
    Evaluate(fe, QStringLiteral("showAboutWindow()"), error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    Notify(fe, "hidetest", "m", true);
    QVERIFY2(qml_test::PumpUntil([&] { return Showing(fe, "hidetest"); }, 2s), "the core message was not shown");
    Evaluate(fe, QStringLiteral("showOnboardingWindow()"), error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const QPointer<QQuickWindow> main = RootWindow(fe, "mainWindow");
    const QPointer<QQuickWindow> about = RootWindow(fe, "aboutWindow");
    const QPointer<QQuickWindow> message = MessageWindow(fe);
    const QPointer<QQuickWindow> splash = RootWindow(fe, "splashWindow");
    const QPointer<QQuickWindow> onboarding = RootWindow(fe, "onboardingWindow");
    QVERIFY2(qml_test::PumpUntil([&] {
                 return main && main->isVisible() && about && about->isVisible() && message &&
                        message->isVisible() && splash && splash->isVisible() && onboarding &&
                        onboarding->isVisible();
             }, 2s),
             "not every one of the root's five windows was shown");

    // The root's own hide, without QmlFrontEnd::hideMain()'s sweep of every
    // top-level window, which would hide them all anyway.
    fe.provider().shell().notifyHideMain();

    QVERIFY2(!main->isVisible(), "the main window stayed visible after hideMainRequested");
    QVERIFY2(!about->isVisible(), "the About window stayed visible after hideMainRequested");
    QVERIFY2(!message->isVisible(), "the core-message window stayed visible after hideMainRequested");
    QVERIFY2(!splash->isVisible(), "the splash stayed visible after hideMainRequested");
    QVERIFY2(!onboarding->isVisible(), "the onboarding window stayed visible after hideMainRequested");
}

void QmlRootWiringTests::hideMainHidesAViewsWindow()
{
    const bool old_quit = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);
    qml_test::ScopeExit restore_quit([old_quit] { qApp->setQuitOnLastWindowClosed(old_quit); });

    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY2(Start(fe), "the root did not load");
    // As SettingsView opens its debug console: createObject(window), so the
    // root keeps no reference and the main window is its transient parent.
    QString error;
    QObject* const opened = Evaluate(fe,
                                     QStringLiteral("(function() { var w = Qt.createComponent('DebugConsoleWindow.qml')"
                                                    ".createObject(mainWindow); w.show(); return w; })()"),
                                     error)
                                .value<QObject*>();
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QPointer<QQuickWindow> console = qobject_cast<QQuickWindow*>(opened);
    QVERIFY2(console, "the debug console was not created");
    QVERIFY2(qml_test::PumpUntil([&] { return console->isVisible(); }, 2s), "the debug console was not shown");

    fe.detachModels();

    QVERIFY2(!console->isVisible(), "a window a view opened stayed visible after hideMain");
}
