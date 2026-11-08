// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_OVERVIEWTXMODELTESTS_H
#define BITCOIN_QT_TEST_OVERVIEWTXMODELTESTS_H

#include <QObject>
#include <QTest>

//! The overview's recent-transactions list follows the wallet model's drains.
class OverviewTxModelTests : public QObject
{
    Q_OBJECT

private slots:
    void transaction_updated_follows_the_applied_rows();
};

#endif // BITCOIN_QT_TEST_OVERVIEWTXMODELTESTS_H
