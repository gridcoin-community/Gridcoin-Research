// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_ROOTWIRINGTESTS_H
#define BITCOIN_QT_QUICK_TEST_ROOTWIRINGTESTS_H

#include <QObject>
#include <QTest>

class QmlRootWiringTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rootBindsNoLegacyNames();
    void splashFollowsSplashState();
    void showMainFollowsShell();
    void coreMessages();
    void closeQuitsOrHidesToTray();
    void quitBeforeTheLoopStillQuits();
};

#endif // BITCOIN_QT_QUICK_TEST_ROOTWIRINGTESTS_H
