// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_BRIDGETESTS_H
#define BITCOIN_QT_QUICK_TEST_BRIDGETESTS_H

#include <QObject>
#include <QTest>

class QmlBridgeTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bridgeSlotSignatures();
    void splashParsesInitMessages();
    void splashMessageArrivesOnGuiThread();
    void showMainSetsShellShown();
    void requestCloseDecidesQuitOrHide();
    void throwingHideMainStillClosesRunner();
    void requestQuitSetsQuitRequested();
    void modalMessageReachesShellWithoutWaiting();
    void serialRunnerThreadNeverWaits();
    void pooledRunnerThreadNeverWaits();
};

#endif // BITCOIN_QT_QUICK_TEST_BRIDGETESTS_H
