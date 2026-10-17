// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_CARRIEDQMLTESTS_H
#define BITCOIN_QT_QUICK_TEST_CARRIEDQMLTESTS_H

#include <QObject>
#include <QTest>

class QmlCarriedQmlTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void carriedFilesLoad();
    void themeUsesUiFamily();
    void iconResourcesDoNotCollide();
};

#endif // BITCOIN_QT_QUICK_TEST_CARRIEDQMLTESTS_H
