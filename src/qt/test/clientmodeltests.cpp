// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/clientmodeltests.h"

#include "qt/clientmodel.h"
#include "qt/test/interfacefakes.h"

#include <QCoreApplication>
#include <QSignalSpy>

namespace {
interfaces::GuiStateSnapshot Snapshot(uint64_t sequence, int height, int peers_height)
{
    interfaces::GuiStateSnapshot snapshot;
    snapshot.sequence = sequence;
    snapshot.height = height;
    snapshot.peers_height = peers_height;
    snapshot.tip_time = 1700000000;
    snapshot.difficulty = 2.5;
    snapshot.initial_download = false;
    return snapshot;
}

//! Run the queued calls the model made from the push.
void Deliver()
{
    QCoreApplication::sendPostedEvents(nullptr, 0);
}
} // anonymous namespace

void ClientModelTests::client_model_before_first_publish_reports_unknown()
{
    qt_test::FakeNode node;
    qt_test::FakeGuiState gui_state;
    ClientModel model(node, gui_state, nullptr);

    QCOMPARE(model.getNumBlocks(), -1);
    QCOMPARE(model.getNumBlocksOfPeers(), 0);
    QVERIFY(model.inInitialBlockDownload());
    QVERIFY(!model.guiState()->computed());
    QCOMPARE(node.m_chain_reads, 0);
}

void ClientModelTests::push_updates_blocks_and_emits_once()
{
    qt_test::FakeNode node;
    qt_test::FakeGuiState gui_state;
    ClientModel model(node, gui_state, nullptr);
    QSignalSpy blocks(&model, &ClientModel::numBlocksChanged);
    QSignalSpy difficulty(&model, &ClientModel::difficultyChanged);

    gui_state.Push(Snapshot(1, 500, 600));
    Deliver();

    QCOMPARE(blocks.count(), 1);
    QCOMPARE(blocks.at(0).at(0).toInt(), 500);
    QCOMPARE(blocks.at(0).at(1).toInt(), 600);
    QCOMPARE(difficulty.count(), 1);
    QCOMPARE(model.getNumBlocks(), 500);
    QCOMPARE(model.getNumBlocksOfPeers(), 600);
    QVERIFY(!model.inInitialBlockDownload());
    QCOMPARE(node.m_chain_reads, 0);
}

void ClientModelTests::cache_accepts_lower_height_with_higher_sequence()
{
    qt_test::FakeNode node;
    qt_test::FakeGuiState gui_state;
    ClientModel model(node, gui_state, nullptr);

    gui_state.Push(Snapshot(1, 500, 600));
    Deliver();
    gui_state.Push(Snapshot(2, 490, 600));
    Deliver();
    QCOMPARE(model.getNumBlocks(), 490);

    gui_state.Push(Snapshot(1, 999, 1000));
    Deliver();
    QCOMPARE(model.getNumBlocks(), 490);
    QCOMPARE(node.m_chain_reads, 0);
}

void ClientModelTests::miner_status_comes_from_the_push()
{
    qt_test::FakeNode node;
    qt_test::FakeGuiState gui_state;
    ClientModel model(node, gui_state, nullptr);
    QSignalSpy miner(&model, &ClientModel::minerStatusChanged);

    interfaces::GuiStateSnapshot snapshot = Snapshot(1, 500, 600);
    snapshot.staking = true;
    snapshot.net_weight = 8000.0;
    snapshot.coin_weight = 123.0;
    snapshot.has_etts = true;
    snapshot.etts_seconds = 3600.0;
    gui_state.Push(snapshot);
    Deliver();

    QCOMPARE(miner.count(), 1);
    QCOMPARE(miner.at(0).at(0).toBool(), true);
    QCOMPARE(miner.at(0).at(1).toDouble(), 100.0);
    QCOMPARE(miner.at(0).at(2).toDouble(), 123.0);
    QCOMPARE(miner.at(0).at(3).toDouble(), 3600.0);
    QCOMPARE(node.m_chain_reads, 0);
}

void ClientModelTests::an_identical_push_emits_no_signal_and_a_miner_change_only_the_miner_one()
{
    qt_test::FakeNode node;
    qt_test::FakeGuiState gui_state;
    ClientModel model(node, gui_state, nullptr);
    QSignalSpy blocks(&model, &ClientModel::numBlocksChanged);
    QSignalSpy difficulty(&model, &ClientModel::difficultyChanged);
    QSignalSpy miner(&model, &ClientModel::minerStatusChanged);

    // The first push emits all three.
    gui_state.Push(Snapshot(1, 500, 600));
    Deliver();
    QCOMPARE(blocks.count(), 1);
    QCOMPARE(difficulty.count(), 1);
    QCOMPARE(miner.count(), 1);

    // The same state under a new sequence emits none.
    gui_state.Push(Snapshot(2, 500, 600));
    Deliver();
    QCOMPARE(blocks.count(), 1);
    QCOMPARE(difficulty.count(), 1);
    QCOMPARE(miner.count(), 1);

    // A push that changes only the miner fields emits only the miner signal.
    interfaces::GuiStateSnapshot snapshot = Snapshot(3, 500, 600);
    snapshot.coin_weight = 77.0;
    gui_state.Push(snapshot);
    Deliver();
    QCOMPARE(blocks.count(), 1);
    QCOMPARE(difficulty.count(), 1);
    QCOMPARE(miner.count(), 2);
    QCOMPARE(miner.at(1).at(2).toDouble(), 77.0);

    // A new block emits the block signal.
    snapshot = Snapshot(4, 501, 600);
    snapshot.coin_weight = 77.0;
    gui_state.Push(snapshot);
    Deliver();
    QCOMPARE(blocks.count(), 2);
    QCOMPARE(miner.count(), 2);
}
