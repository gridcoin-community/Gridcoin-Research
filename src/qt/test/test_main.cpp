#include <QApplication>
#include <QTest>
#include <QObject>

#include "logging.h"

#include "aboutdialogtests.h"
#include "askpassphrasedialogtests.h"
#include "autostarttests.h"
#include "bitcoinguitests.h"
#include "clientmodeltests.h"
#include "mrcmodeltests.h"
#include "pushedstatetests.h"
#include "overviewtxmodeltests.h"
#include "escapetests.h"
#include "bitcoinunitstests.h"
#include "coinselectionmodeltests.h"
#include "coinselectionviewtests.h"
#include "coreconnectwaittests.h"
#include "filetransferdialogtests.h"
#include "guiipcworkertests.h"
#include "guiutiltests.h"
#include "optionsmodeltests.h"
#include "psgttoastdamptests.h"
#include "researchermodeltests.h"
#include "researcherwizardpagetests.h"
#include "rpcconsoletests.h"
#include "sendcoinsdialogtests.h"
#include "sidestakemodeltests.h"
#include "tablemodelindextests.h"
#include "uritests.h"
#include "walletmodeltests.h"

#include <cstring>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>

#ifndef Q_OS_WIN
#include <signal.h>
#endif

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


//! Runs one test class. QTest rethrows an exception that escaped a test
//! function once it has logged it; that marks the run failed here instead of
//! ending the process, and the remaining classes still run. A class that threw
//! skipped its cleanup slots, so a FAIL in a later class of the same run may be
//! a consequence; fix the first UNCAUGHT EXCEPTION first.
//! Qt 6.4 to 6.8 leave QTest's crash handler installed after qExec, so a fatal
//! signal in a later class would re-enter it without end. Before and after
//! qExec, on every path, a signal held by a handler gets back the action it had
//! before the first class ran; one left ignored or default is left as it is, as
//! Qt 6.9's own handler does (Qt ignores SIGPIPE once per process: on its first
//! write to a child's stdin, or when forkfd starts a child with plain fork()).
static bool RunTestClass(QObject& test)
{
#ifndef Q_OS_WIN
    static constexpr int kSignals[] = {SIGHUP, SIGINT, SIGQUIT, SIGABRT, SIGILL,
                                       SIGBUS, SIGFPE, SIGSEGV, SIGPIPE, SIGTERM};
    struct Snapshot {
        struct sigaction saved[std::size(kSignals)];
        Snapshot() { for (size_t i = 0; i < std::size(kSignals); ++i) sigaction(kSignals[i], nullptr, &saved[i]); }
    };
    static const Snapshot snapshot; // first call, before any qExec
    struct Restore {
        static void PutBack()
        {
            for (size_t i = 0; i < std::size(kSignals); ++i) {
                struct sigaction now;
                if (sigaction(kSignals[i], nullptr, &now) != 0) continue;
                const bool handler = (now.sa_flags & SA_SIGINFO) ||
                                     (now.sa_handler != SIG_DFL && now.sa_handler != SIG_IGN);
                if (handler) sigaction(kSignals[i], &snapshot.saved[i], nullptr);
            }
        }
        Restore() { PutBack(); }
        ~Restore() { PutBack(); }
    } restore;
#endif
    try {
        return QTest::qExec(&test) == 0;
    } catch (const std::exception& e) {
        std::cerr << "UNCAUGHT EXCEPTION from " << test.metaObject()->className() << ": " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "UNCAUGHT EXCEPTION from " << test.metaObject()->className() << std::endl;
    }
    return false;
}

//! `namespace QTest = GuardedQTest;` in a function sends each QTest::qExec
//! call after it in that block through RunTestClass, so main's calls keep the
//! form they have in every other copy of this file.
namespace GuardedQTest {
static int qExec(QObject* test) { return RunTestClass(*test) ? 0 : 1; }
} // namespace GuardedQTest

//! The child EscapeTests starts (see ESCAPE_CHILD_ARG). After its last class it
//! reports whether SIGPIPE is still ignored. That is read here, outside every
//! qExec, because QTest takes SIGPIPE over for the length of each one.
static int RunEscapeChild(const std::string& mode)
{
    namespace QTest = GuardedQTest;
    bool fInvalid = false;
    EscapePassingTests passing;
    if (QTest::qExec(&passing) != 0)
        fInvalid = true;
    if (mode == "throw") {
        EscapeThrowingTests throwing;
        if (QTest::qExec(&throwing) != 0)
            fInvalid = true;
        EscapeMarkerTests marker;
        if (QTest::qExec(&marker) != 0)
            fInvalid = true;
    } else if (mode == "abort") {
        EscapeAbortingTests aborting;
        if (QTest::qExec(&aborting) != 0)
            fInvalid = true;
    } else if (mode == "fail") {
        EscapeFailingTests failing;
        if (QTest::qExec(&failing) != 0)
            fInvalid = true;
    } else {
        std::cerr << "unknown escape child mode: " << mode << std::endl;
        return 2;
    }
#ifndef Q_OS_WIN
    struct sigaction now;
    const bool ignored = sigaction(SIGPIPE, nullptr, &now) == 0 && !(now.sa_flags & SA_SIGINFO) &&
                         now.sa_handler == SIG_IGN;
    std::cout << (ignored ? "SIGPIPE ignored" : "SIGPIPE not ignored") << std::endl;
#endif
    return fInvalid;
}

// This is all you need to run all the tests
int main(int argc, char *argv[])
{
    // A test that crashes must not have QtTest attach a debugger to it or write a core.
    qputenv("QTEST_DISABLE_STACK_DUMP", "1");
    qputenv("QTEST_DISABLE_CORE_DUMP", "1");

    if (argc >= 3 && std::strcmp(argv[1], ESCAPE_CHILD_ARG) == 0) {
        return RunEscapeChild(argv[2]);
    }

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

    namespace QTest = GuardedQTest;
    bool fInvalid = false;

    EscapeTests escapeTests;
    if (QTest::qExec(&escapeTests) != 0)
        fInvalid = true;

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

    BitcoinGUITests test14;
    if (QTest::qExec(&test14) != 0)
        fInvalid = true;

    ClientModelTests test15;
    if (QTest::qExec(&test15) != 0)
        fInvalid = true;

    MRCModelTests test16;
    if (QTest::qExec(&test16) != 0)
        fInvalid = true;

    PushedStateTests test17;
    if (QTest::qExec(&test17) != 0)
        fInvalid = true;

    OverviewTxModelTests test18;
    if (QTest::qExec(&test18) != 0)
        fInvalid = true;

    AskPassphraseDialogTests test19;
    if (QTest::qExec(&test19) != 0)
        fInvalid = true;

    SendCoinsDialogTests test20;
    if (QTest::qExec(&test20) != 0)
        fInvalid = true;

    CoreConnectWaitTests test21;
    if (QTest::qExec(&test21) != 0)
        fInvalid = true;

    FileTransferDialogTests test22;
    if (QTest::qExec(&test22) != 0)
        fInvalid = true;

    GuiIpcWorkerTests test23;
    if (QTest::qExec(&test23) != 0)
        fInvalid = true;

    WalletModelTests test24;
    if (QTest::qExec(&test24) != 0)
        fInvalid = true;

    TableModelIndexTests test25;
    if (QTest::qExec(&test25) != 0)
        fInvalid = true;

    return fInvalid;
}
