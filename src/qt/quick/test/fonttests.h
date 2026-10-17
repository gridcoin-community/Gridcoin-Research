// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_FONTTESTS_H
#define BITCOIN_QT_QUICK_TEST_FONTTESTS_H

#include <QObject>
#include <QTest>

class QmlFontTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void uiFamilyChoice();
    void uiFamilyOnThisPlatform();
    void loadsFontsNotLicenceTexts();
};

#endif // BITCOIN_QT_QUICK_TEST_FONTTESTS_H
