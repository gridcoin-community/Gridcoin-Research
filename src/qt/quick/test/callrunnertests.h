// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_CALLRUNNERTESTS_H
#define BITCOIN_QT_QUICK_TEST_CALLRUNNERTESTS_H

#include <QObject>
#include <QTest>

class QmlCallRunnerTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void serialLaneNotBlockedByPooled();
    void drainDoesNotWaitForPooled();
    void deliverRunsOnGuiThread();
    void staleResultDropped();
    void disconnectRoutedOnce();
    void failureRethrownOnGuiThread();
    void failureAfterCloseNotRethrown();
    void drainWaitsForSerialCall();
    void pooledLaneUsesGlobalPool();
    void pooledPostRefusedAfterDrain();
    void serialPostRefusedAfterDrain();
    void queuedPooledJobDroppedAfterGenerationBump();
};

#endif // BITCOIN_QT_QUICK_TEST_CALLRUNNERTESTS_H
