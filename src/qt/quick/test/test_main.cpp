// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "bridgetests.h"
#include "callrunnertests.h"
#include "moduletests.h"
#include "qmltestutil.h"
#include "quittests.h"

#include <QTest>
#include <QtGlobal>

#include <iostream>

int main(int argc, char* argv[])
{
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

    // First: it is the only class that runs the application's event loop.
    QmlQuitTests test1;
    if (QTest::qExec(&test1) != 0) fInvalid = true;

    QmlModuleTests test2;
    if (QTest::qExec(&test2) != 0) fInvalid = true;

    QmlCallRunnerTests test3;
    if (QTest::qExec(&test3) != 0) fInvalid = true;

    QmlBridgeTests test4;
    if (QTest::qExec(&test4) != 0) fInvalid = true;

    // The test application swallows what its notify() catches; nothing a test
    // did not expect may be left.
    if (!qml_test::g_rethrown.empty()) {
        for (const auto& entry : qml_test::g_rethrown) {
            std::cerr << "UNEXPECTED RETHROW: " << entry.first << std::endl;
        }
        fInvalid = true;
    }

    return fInvalid;
}
