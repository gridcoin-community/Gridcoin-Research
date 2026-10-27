// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "bridgetests.h"
#include "callrunnertests.h"
#include "carriedqmltests.h"
#include "fonttests.h"
#include "moduletests.h"
#include "qmltestutil.h"
#include "quittests.h"
#include "rootwiringtests.h"

#include <QTest>
#include <QtGlobal>

#include <cstring>
#include <iostream>

int main(int argc, char* argv[])
{
    // QmlBridgeTests::unloadableRootEndsTheGui runs GuiMain in a child copy of
    // this binary, and guiThreadExceptionCaughtInPlace hands
    // HandleGuiThreadException one exception in another; neither child runs a
    // test.
    if (argc > 1 && std::strcmp(argv[1], qml_test::GUIMAIN_CHILD_ARG) == 0) {
        return qml_test::RunGuiMainChild(argc, argv);
    }
    if (argc > 2 && std::strcmp(argv[1], qml_test::GUITHREAD_EXCEPTION_CHILD_ARG) == 0) {
        return qml_test::RunGuiThreadExceptionChild(argc, argv);
    }
    // The shell asks QSystemTrayIcon and the quit test shows windows, so the
    // application needs a platform plugin and a scene-graph backend that run
    // headless; an explicit setting still wins.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
        qputenv("QT_QUICK_BACKEND", "software");
    }
    qml_test::QmlTestApplication app(argc, argv);

    bool fInvalid = false;

    // First: these two are the only classes that run the application's event
    // loop.
    QmlQuitTests test1;
    if (QTest::qExec(&test1) != 0) fInvalid = true;

    QmlRootWiringTests test2;
    if (QTest::qExec(&test2) != 0) fInvalid = true;

    QmlModuleTests test3;
    if (QTest::qExec(&test3) != 0) fInvalid = true;

    QmlCallRunnerTests test4;
    if (QTest::qExec(&test4) != 0) fInvalid = true;

    QmlBridgeTests test5;
    if (QTest::qExec(&test5) != 0) fInvalid = true;

    QmlFontTests test6;
    if (QTest::qExec(&test6) != 0) fInvalid = true;

    QmlCarriedQmlTests test7;
    if (QTest::qExec(&test7) != 0) fInvalid = true;

    // The test application swallows what its notify() catches; nothing a test
    // did not expect may be left.
    if (!qml_test::g_rethrown.empty()) {
        for (const auto& entry : qml_test::g_rethrown) {
            std::cerr << "UNEXPECTED RETHROW: " << entry.first << std::endl;
        }
        fInvalid = true;
    }
    if (!qml_test::g_failed.empty()) {
        for (const auto& entry : qml_test::g_failed) {
            std::cerr << "UNEXPECTED FAILURE: " << entry.first << std::endl;
        }
        fInvalid = true;
    }

    return fInvalid;
}
