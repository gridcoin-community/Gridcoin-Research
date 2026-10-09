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
