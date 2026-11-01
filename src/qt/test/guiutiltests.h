// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_GUIUTILTESTS_H
#define BITCOIN_QT_TEST_GUIUTILTESTS_H

#include <QObject>
#include <QTest>

class GUIUtilTests : public QObject
{
    Q_OBJECT

private slots:
    void extractFirstSuffixFromFilter_data();
    void extractFirstSuffixFromFilter();
};

#endif // BITCOIN_QT_TEST_GUIUTILTESTS_H
