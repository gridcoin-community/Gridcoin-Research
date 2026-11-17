// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/escapetests.h"

#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <cstdlib>
#include <stdexcept>

#ifndef Q_OS_WIN
#include <signal.h>
#endif

//!
//! \file escapetests.cpp
//! \brief A test class that throws or aborts ends test_gridcoin-qt once, with
//! the failure reported, instead of ending it in terminate or looping in
//! QTest's crash handler.
//!

//! EscapeTests starts test_gridcoin-qt again as a child, which needs POSIX
//! signals and QProcess. The depends build's Qt is configured without
//! QProcess on Linux, so there the cases skip.
#if !defined(Q_OS_WIN) && QT_CONFIG(process)
#define ESCAPE_TESTS_RUN_CHILD
#endif

#ifdef ESCAPE_TESTS_RUN_CHILD
namespace {
//! How long the parent waits for a child. A child looping in QTest's crash
//! handler never finishes on its own; it is killed at this bound.
constexpr int CHILD_BOUND_MS = 120000;

struct ChildRun {
    bool finished = false;
    QProcess::ExitStatus status = QProcess::NormalExit;
    int code = -1;
    QString out;
    QString err;
};

ChildRun RunChild(const QString& mode)
{
    ChildRun run;
    QProcess child;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // A crash in the child is expected: no debugger stack dump, no core file.
    env.insert(QStringLiteral("QTEST_DISABLE_STACK_DUMP"), QStringLiteral("1"));
    env.insert(QStringLiteral("QTEST_DISABLE_CORE_DUMP"), QStringLiteral("1"));
    child.setProcessEnvironment(env);
    child.start(QCoreApplication::applicationFilePath(), {QString::fromLatin1(ESCAPE_CHILD_ARG), mode});
    if (!child.waitForStarted()) {
        return run;
    }
    run.finished = child.waitForFinished(CHILD_BOUND_MS);
    if (!run.finished) {
        child.kill();
        child.waitForFinished();
    }
    run.status = child.exitStatus();
    run.code = child.exitCode();
    run.out = QString::fromLocal8Bit(child.readAllStandardOutput());
    run.err = QString::fromLocal8Bit(child.readAllStandardError());
    return run;
}
} // namespace
#endif // ESCAPE_TESTS_RUN_CHILD

void EscapePassingTests::passesAndIgnoresSigpipe()
{
#ifndef Q_OS_WIN
    struct sigaction ignore{};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    QCOMPARE(sigaction(SIGPIPE, &ignore, nullptr), 0);
#endif
}

void EscapeThrowingTests::throwsOutOfTheSlot()
{
    throw std::runtime_error("thrown by EscapeThrowingTests");
}

void EscapeAbortingTests::abortsInTheSlot()
{
    std::abort();
}

void EscapeMarkerTests::runs()
{
}

void EscapeFailingTests::failsByQFail()
{
    QFAIL("fails by QFAIL in EscapeFailingTests");
}

void EscapeTests::anEscapedExceptionFailsTheRunAndLaterClassesRun()
{
#ifndef ESCAPE_TESTS_RUN_CHILD
    QSKIP("needs POSIX signals and QProcess");
#else
    const ChildRun run = RunChild(QStringLiteral("throw"));
    QVERIFY2(run.finished, "the child did not finish within the bound");
    QCOMPARE(run.status, QProcess::NormalExit);
    QCOMPARE(run.code, 1);
    QVERIFY2(run.err.contains(QStringLiteral("UNCAUGHT EXCEPTION from EscapeThrowingTests")), qPrintable(run.err));
    QVERIFY2(run.out.contains(QStringLiteral("Start testing of EscapeMarkerTests")), qPrintable(run.out));
    QVERIFY2(run.out.contains(QStringLiteral("SIGPIPE ignored")), qPrintable(run.out));
#endif
}

void EscapeTests::anAbortAfterAnEarlierClassEndsTheChildOnce()
{
#ifndef ESCAPE_TESTS_RUN_CHILD
    QSKIP("needs POSIX signals and QProcess");
#else
    const ChildRun run = RunChild(QStringLiteral("abort"));
    QVERIFY2(run.finished, "the child did not finish within the bound");
    QCOMPARE(run.status, QProcess::CrashExit);
    // QTest in Qt 6.4 and later handles SIGABRT and reports it, once here. Qt
    // 5.15 leaves SIGABRT alone and reports nothing, and Qt 6.2 and 6.3 were not
    // checked, so before 6.4 the bound is at most one report.
    const auto reports = (run.out + run.err).count(QStringLiteral("Received signal"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    QCOMPARE(reports, 1);
#else
    QVERIFY2(reports <= 1, qPrintable(run.out + run.err));
#endif
#endif
}

void EscapeTests::anOrdinaryFailureFailsTheRun()
{
#ifndef ESCAPE_TESTS_RUN_CHILD
    QSKIP("needs POSIX signals and QProcess");
#else
    const ChildRun run = RunChild(QStringLiteral("fail"));
    QVERIFY2(run.finished, "the child did not finish within the bound");
    QCOMPARE(run.status, QProcess::NormalExit);
    QCOMPARE(run.code, 1);
    QVERIFY2(run.out.contains(QStringLiteral("FAIL!  : EscapeFailingTests::failsByQFail()")), qPrintable(run.out));
#endif
}
