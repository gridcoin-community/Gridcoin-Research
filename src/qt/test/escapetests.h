// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_ESCAPETESTS_H
#define BITCOIN_QT_TEST_ESCAPETESTS_H

#include <QObject>
#include <QTest>

//! First argument that makes test_gridcoin-qt run as the child EscapeTests
//! starts. The second argument is the mode: "throw" runs EscapePassingTests,
//! EscapeThrowingTests and EscapeMarkerTests; "abort" runs EscapePassingTests
//! and EscapeAbortingTests; "fail" runs EscapePassingTests and
//! EscapeFailingTests. The child runs no other class.
inline constexpr char ESCAPE_CHILD_ARG[] = "--escape-child";

//! Child class run first in every mode, so that QTest's crash handler has been
//! installed once before the class that throws, aborts or fails. Its slot also
//! sets SIGPIPE to ignored, standing in for the ignore Qt sets once per
//! process inside a qExec, which RunTestClass must not undo.
class EscapePassingTests : public QObject
{
    Q_OBJECT

private slots:
    void passesAndIgnoresSigpipe();
};

//! Child class whose slot lets an exception escape.
class EscapeThrowingTests : public QObject
{
    Q_OBJECT

private slots:
    void throwsOutOfTheSlot();
};

//! Child class whose slot aborts.
class EscapeAbortingTests : public QObject
{
    Q_OBJECT

private slots:
    void abortsInTheSlot();
};

//! Child class that only shows, by its "Start testing of" line, that it ran.
class EscapeMarkerTests : public QObject
{
    Q_OBJECT

private slots:
    void runs();
};

//! Child class whose slot fails by QFAIL, with no throw and no signal.
class EscapeFailingTests : public QObject
{
    Q_OBJECT

private slots:
    void failsByQFail();
};

//! Starts this binary as a child in each mode and checks how the child ends.
class EscapeTests : public QObject
{
    Q_OBJECT

private slots:
    void anEscapedExceptionFailsTheRunAndLaterClassesRun();
    void anAbortAfterAnEarlierClassEndsTheChildOnce();
    void anOrdinaryFailureFailsTheRun();
};

#endif // BITCOIN_QT_TEST_ESCAPETESTS_H
