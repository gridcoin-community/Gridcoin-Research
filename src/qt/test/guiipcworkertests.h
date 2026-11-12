// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_GUIIPCWORKERTESTS_H
#define BITCOIN_QT_TEST_GUIIPCWORKERTESTS_H

#include <QObject>
#include <QTest>

//! The long-lived threads the GUI calls into the core on (qt/guiipcworker.h): the order jobs run
//! in, the thread they run on, what a job's future says, and how a worker ends.
class GuiIpcWorkerTests : public QObject
{
    Q_OBJECT

private slots:
    void runsJobsInOrderOnOneThread();
    void eachRoleHasItsOwnThread();
    void futureIsReadyOnceTheJobIsGone();
    void runsQueuedJobsBeforeItGoes();
    void postWhileGoingIsRefused();
};

#endif // BITCOIN_QT_TEST_GUIIPCWORKERTESTS_H
