// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_CORECONNECTWAITTESTS_H
#define BITCOIN_QT_TEST_CORECONNECTWAITTESTS_H

#include <QObject>
#include <QTest>

//! WaitForCore (qt/coreconnectwait.h) driven by fake attempts: no core and no
//! socket. Each attempt runs on a thread of WaitForCore's; the cases observe the
//! waiting dialog from the GUI thread while WaitForCore's event loop runs.
class CoreConnectWaitTests : public QObject
{
    Q_OBJECT

private slots:
    void firstAttemptThatConnectsShowsNoDialog();
    void failureNotRetriedGivesUpWithoutDialog();
    void retriedFailureShowsDialogUntilConnected();
    void slowAttemptShowsDialog();
    void cancelWaitsForAttemptWithoutBlocking();
    void cancelWhileIdleStartsNoAttempt();
    void attemptThatThrowsEndsTheWait();
    // Last: it tells the application to quit, then clears that again.
    void loopExitWaitsForAttemptWithoutBlocking();
};

#endif // BITCOIN_QT_TEST_CORECONNECTWAITTESTS_H
