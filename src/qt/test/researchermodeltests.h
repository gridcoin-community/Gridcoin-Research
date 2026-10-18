// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_RESEARCHERMODELTESTS_H
#define BITCOIN_QT_TEST_RESEARCHERMODELTESTS_H

#include <QObject>
#include <QTest>

//! The researcher model refreshes on node notifications, and none reports a
//! node falling out of sync: that happens when blocks stop arriving. These
//! cases cover the timer that polls for it, and what the pages that repaint on
//! researcherChanged read when it reports a change.
class ResearcherModelTests : public QObject
{
    Q_OBJECT

private slots:
    void aStalledNodeShowsAsOutOfSyncWithoutANotification();
    void aPollWithoutAChangeRefreshesNothing();
    void aSnapshotSkippedWhileMainWasBusyIsRetried();
    void aPageRepaintedOnRecoverySeesTheRestoredSnapshot();
    void aRecoveryReportedWithoutItsSnapshotIsReportedAgainWithIt();
    void aRecoveryTheFlagReadMissedIsReportedFromTheSnapshot();
};

#endif // BITCOIN_QT_TEST_RESEARCHERMODELTESTS_H
