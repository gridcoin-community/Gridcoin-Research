// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_MODULETESTS_H
#define BITCOIN_QT_QUICK_TEST_MODULETESTS_H

#include <QObject>
#include <QTest>

class QmlModuleTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void moduleResolves();
    void singletonsOutliveEngine();
    void singletonIsProviderInstance();
};

#endif // BITCOIN_QT_QUICK_TEST_MODULETESTS_H
