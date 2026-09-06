#include <QApplication>
#include <QTest>
#include <QObject>

#include "bitcoinunitstests.h"
#include "coinselectionmodeltests.h"
#include "coinselectionviewtests.h"
#include "psgttoastdamptests.h"
#include "sidestakemodeltests.h"
#include "uritests.h"

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if defined(QT_QPA_PLATFORM_MINIMAL)
Q_IMPORT_PLUGIN(QMinimalIntegrationPlugin);
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif


// This is all you need to run all the tests
int main(int argc, char *argv[])
{
    // CoinSelectionViewTests drives a real QTreeView, which needs a
    // QApplication and a platform plugin. Default to offscreen so the suite
    // still runs headless -- ctest invokes it with no display on every CI
    // runner -- while an explicit QT_QPA_PLATFORM (a developer watching the
    // widget on xcb or wayland) still wins.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication app(argc, argv);

    bool fInvalid = false;

    BitcoinUnitsTests test1;
    if (QTest::qExec(&test1) != 0)
        fInvalid = true;

    URITests test2;
    if (QTest::qExec(&test2) != 0)
        fInvalid = true;

    SideStakeModelTests test3;
    if (QTest::qExec(&test3) != 0)
        fInvalid = true;

    CoinSelectionModelTests test4;
    if (QTest::qExec(&test4) != 0)
        fInvalid = true;

    CoinSelectionViewTests test5;
    if (QTest::qExec(&test5) != 0)
        fInvalid = true;

    PSGTToastDampTests test6;
    if (QTest::qExec(&test6) != 0)
        fInvalid = true;

    return fInvalid;
}
