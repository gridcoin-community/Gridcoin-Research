// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "quittests.h"

#include "interfacefakes_qml.h"
#include "qmltestutil.h"

#include <qt/quick/qmlfrontend.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

#include <atomic>

//!
//! \file quittests.cpp
//! \brief A secondary window closing while the main window is hidden must not
//! quit the application. This is the only class that runs the application's
//! event loop.
//!

void QmlQuitTests::secondaryWindowCloseDoesNotQuit()
{
    std::atomic<int> disconnects{0};
    qml_test::CountingQuitFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY(fe.construct());

    const bool old_quit_on_last_window_closed = qApp->quitOnLastWindowClosed();
    qApp->setQuitOnLastWindowClosed(false);

    {
        QQuickWindow main_window;
        QQuickWindow other;
        QTimer::singleShot(0, [&] {
            main_window.show();
            other.show();
            QTimer::singleShot(50, [&] {
                main_window.hide();
                other.close();
                QTimer::singleShot(50, [] { QCoreApplication::exit(0); });
            });
        });
        qApp->exec();
    }
    fe.destroyMain();
    qApp->setQuitOnLastWindowClosed(old_quit_on_last_window_closed);

    QCOMPARE(fe.quits, 0);
}

void QmlQuitTests::quitBeforeTheLoopEndsTheLoop()
{
    std::atomic<int> disconnects{0};
    QmlFrontEnd fe(qml_test::FIXTURE_ROOT_URL, qml_test::CountingCallbacks(disconnects));
    QVERIFY(fe.construct());
    // As a queued quit request handled in the readiness wait: before the loop.
    fe.requestQuit();
    fe.showMain(false);
    bool fell_back = false;
    QTimer fallback;
    fallback.setSingleShot(true);
    QObject::connect(&fallback, &QTimer::timeout, &fallback, [&fell_back] {
        fell_back = true;
        QCoreApplication::exit(3);
    });
    fallback.start(10000);
    const int rc = qApp->exec();
    fallback.stop();
    fe.destroyMain();

    QVERIFY2(!fell_back, "a quit requested before the event loop did not end it");
    QCOMPARE(rc, 0);
}
