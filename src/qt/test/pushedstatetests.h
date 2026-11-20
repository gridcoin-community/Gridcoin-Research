// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_PUSHEDSTATETESTS_H
#define BITCOIN_QT_TEST_PUSHEDSTATETESTS_H

#include <QObject>
#include <QTest>

//! The PSGT pool page, the wallet model's balances, and polls and votes read
//! the state the core pushes, and polls and votes wait until the wallet is
//! synced.
class PushedStateTests : public QObject
{
    Q_OBJECT

private slots:
    void psgt_page_status_from_push();
    void a_snapshot_published_while_subscribing_is_not_lost();
    void wallet_balances_from_push();
    void voting_unavailable_until_synced();
    void poll_title_is_read_when_the_first_synced_push_arrives();
};

#endif // BITCOIN_QT_TEST_PUSHEDSTATETESTS_H
