// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_CLIENTMODELTESTS_H
#define BITCOIN_QT_TEST_CLIENTMODELTESTS_H

#include <QObject>
#include <QTest>

//! ClientModel shows the chain and staking state from the copy the core pushes
//! (interfaces::GuiState), and reads the node for none of it: those reads take
//! cs_main, which the GUI thread must not wait on. The pushes come from a fake
//! GuiState, synchronously on the test's thread; the model hands each to the
//! GUI thread by a queued call, which the cases then process.
class ClientModelTests : public QObject
{
    Q_OBJECT

private slots:
    void client_model_before_first_publish_reports_unknown();
    void push_updates_blocks_and_emits_once();
    void cache_accepts_lower_height_with_higher_sequence();
    void miner_status_comes_from_the_push();
    void an_identical_push_emits_no_signal_and_a_miner_change_only_the_miner_one();
};

#endif // BITCOIN_QT_TEST_CLIENTMODELTESTS_H
