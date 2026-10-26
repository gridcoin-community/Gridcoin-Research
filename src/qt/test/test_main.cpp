#include <QApplication>
#include <QTest>
#include <QObject>

#include "logging.h"

#include "aboutdialogtests.h"
#include "autostarttests.h"
#include "bitcoinunitstests.h"
#include "coinselectionmodeltests.h"
#include "coinselectionviewtests.h"
#include "guiutiltests.h"
#include "optionsmodeltests.h"
#include "psgttoastdamptests.h"
#include "researchermodeltests.h"
#include "researcherwizardpagetests.h"
#include "rpcconsoletests.h"
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

    // Log to the console, as test_gridcoin does. Until StartLogging() the
    // logger only buffers, and nothing here ever started it, so whatever the
    // code under test logged was discarded -- the lock-order checker's reports
    // among it, which the CI sanitizer job looks for in this output.
    LogInstance().m_print_to_file = false;
    LogInstance().m_print_to_console = true;
    if (!LogInstance().StartLogging()) {
        return 1;
    }

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

    ResearcherWizardPageTests test7;
    if (QTest::qExec(&test7) != 0)
        fInvalid = true;

    AutoStartTests test8;
    if (QTest::qExec(&test8) != 0)
        fInvalid = true;

    OptionsModelTests test9;
    if (QTest::qExec(&test9) != 0)
        fInvalid = true;

    GUIUtilTests test10;
    if (QTest::qExec(&test10) != 0)
        fInvalid = true;

    AboutDialogTests test11;
    if (QTest::qExec(&test11) != 0)
        fInvalid = true;

    ResearcherModelTests test12;
    if (QTest::qExec(&test12) != 0)
        fInvalid = true;

    RPCConsoleTests test13;
    if (QTest::qExec(&test13) != 0)
        fInvalid = true;

    return fInvalid;
}
