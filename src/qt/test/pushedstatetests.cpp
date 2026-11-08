// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/pushedstatetests.h"

#include "interfaces/wallet_coin_source.h"
#include "qt/clientmodel.h"
#include "qt/optionsmodel.h"
#include "qt/psgtpoolpage.h"
#include "qt/test/interfacefakes.h"
#include "qt/voting/votingmodel.h"
#include "qt/voting/votingpage.h"
#include "qt/walletmodel.h"

#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

namespace {
void Deliver()
{
    QCoreApplication::sendPostedEvents(nullptr, 0);
}

//! The label under \p parent whose text contains \p needle, or null.
QLabel* LabelWith(QWidget& parent, const QString& needle)
{
    for (QLabel* label : parent.findChildren<QLabel*>()) {
        if (label->text().contains(needle)) return label;
    }

    return nullptr;
}

interfaces::GuiStateSnapshot Synced(uint64_t sequence, bool out_of_sync)
{
    interfaces::GuiStateSnapshot snapshot;
    snapshot.sequence = sequence;
    snapshot.initial_download = false;
    snapshot.out_of_sync = out_of_sync;
    return snapshot;
}
} // anonymous namespace

void PushedStateTests::psgt_page_status_from_push()
{
    qt_test::FakeNode node;
    qt_test::FakeGuiState gui_state;
    qt_test::FakePSGTPoolContext psgt;
    ClientModel client_model(node, gui_state, nullptr);

    // The canned answer disagrees with every push below, so a call would show.
    psgt.m_status.active = true;
    psgt.m_status.v15_enabled = true;

    PSGTPoolPage page;
    page.setPSGTPoolContext(&psgt);
    page.setClientModel(&client_model);

    interfaces::GuiStateSnapshot snapshot = Synced(1, true);
    snapshot.psgt_status.active = false;
    snapshot.psgt_status.v15_enabled = true;
    snapshot.psgt_status.out_of_sync = true;
    gui_state.Push(snapshot);
    Deliver();

    QLabel* syncing = LabelWith(page, "unavailable while this node is syncing");
    QVERIFY(syncing != nullptr);
    QVERIFY(!syncing->isHidden());
    QCOMPARE(psgt.m_pool_status_calls, 0);

    // Active: the banner goes.
    snapshot = Synced(2, false);
    snapshot.psgt_status.active = true;
    snapshot.psgt_status.v15_enabled = true;
    gui_state.Push(snapshot);
    Deliver();

    QVERIFY(syncing->isHidden());
    QCOMPARE(psgt.m_pool_status_calls, 0);

    // Before v15: the other banner.
    snapshot = Synced(3, false);
    gui_state.Push(snapshot);
    Deliver();

    QLabel* inactive = LabelWith(page, "not active yet");
    QVERIFY(inactive != nullptr);
    QVERIFY(!inactive->isHidden());
    QCOMPARE(psgt.m_pool_status_calls, 0);
}

void PushedStateTests::wallet_balances_from_push()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    qt_test::FakeGuiState gui_state;
    qt_test::FakeWallet wallet;
    qt_test::FakeWalletTxSource tx_source;
    auto coin_source = interfaces::MakeSyntheticCoinSource(0, 0);
    OptionsModel options(node, sidestakes);
    ClientModel client_model(node, gui_state, nullptr);
    WalletModel wallet_model(wallet, tx_source, *coin_source, &options);
    wallet_model.setGuiState(client_model.guiState());

    QSignalSpy spy(&wallet_model, &WalletModel::balanceChanged);

    interfaces::GuiStateSnapshot snapshot = Synced(1, false);
    snapshot.has_balances = true;
    snapshot.balances.balance = 7;
    snapshot.balances.stake = 3;
    gui_state.Push(snapshot);
    Deliver();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), qint64(7));
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(3));

    // The same figures again: nothing changed, so nothing is emitted.
    snapshot.sequence = 2;
    gui_state.Push(snapshot);
    Deliver();
    QCOMPARE(spy.count(), 1);

    // A node with no wallet pushes no balances.
    snapshot.sequence = 3;
    snapshot.has_balances = false;
    snapshot.balances.balance = 99;
    gui_state.Push(snapshot);
    Deliver();
    QCOMPARE(spy.count(), 1);

    QCOMPARE(wallet.m_try_get_balances_calls, 0);
}

void PushedStateTests::voting_unavailable_until_synced()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    qt_test::FakeGuiState gui_state;
    qt_test::FakeWallet wallet;
    qt_test::FakeWalletTxSource tx_source;
    qt_test::FakeVotingManager voting;
    qt_test::FakeResearcherContext researcher_context;
    auto coin_source = interfaces::MakeSyntheticCoinSource(0, 0);
    OptionsModel options(node, sidestakes);
    WalletModel wallet_model(wallet, tx_source, *coin_source, &options);

    // Syncing at startup.
    gui_state.m_snapshot = Synced(1, true);
    ClientModel syncing_client(node, gui_state, nullptr);
    VotingModel model(voting, researcher_context, syncing_client, options, wallet_model);
    VotingPage page;
    page.setVotingModel(&model);
    page.show();

    QVERIFY(!model.available());
    QVERIFY(model.getCurrentPollTitle().isEmpty());

    QLabel* unavailable = LabelWith(page, "Polls and voting are available once the wallet is synced.");
    QVERIFY(unavailable != nullptr);
    QVERIFY(!unavailable->isHidden());
    QPushButton* create = page.findChild<QPushButton*>("createPollButton");
    QVERIFY(create != nullptr);
    QVERIFY(!create->isEnabled());

    // No poll work reached the core while syncing.
    QCOMPARE(voting.m_poll_table_calls, 0);
    QCOMPARE(voting.m_current_title_calls, 0);
    QCOMPARE(voting.m_latest_poll_time_calls, 0);

    // Synced: as before.
    QSignalSpy spy(&model, &VotingModel::availableChanged);
    gui_state.Push(Synced(2, false));
    Deliver();

    QVERIFY(model.available());
    QCOMPARE(spy.count(), 1);
    QVERIFY(unavailable->isHidden());
    QVERIFY(create->isEnabled());
    QCOMPARE(voting.m_latest_poll_time_calls, 1);

    // Out of sync again.
    gui_state.Push(Synced(3, true));
    Deliver();

    QVERIFY(!model.available());
    QCOMPARE(spy.count(), 2);
    QVERIFY(!unavailable->isHidden());
    QVERIFY(!create->isEnabled());

    // Still syncing by the initial download flag alone.
    gui_state.Push(Synced(4, false));
    Deliver();
    QVERIFY(model.available());
    interfaces::GuiStateSnapshot ibd = Synced(5, false);
    ibd.initial_download = true;
    gui_state.Push(ibd);
    Deliver();
    QVERIFY(!model.available());
}

void PushedStateTests::poll_title_is_read_when_the_first_synced_push_arrives()
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    qt_test::FakeGuiState gui_state;
    qt_test::FakeWallet wallet;
    qt_test::FakeWalletTxSource tx_source;
    qt_test::FakeVotingManager voting;
    qt_test::FakeResearcherContext researcher_context;
    auto coin_source = interfaces::MakeSyntheticCoinSource(0, 0);
    OptionsModel options(node, sidestakes);
    WalletModel wallet_model(wallet, tx_source, *coin_source, &options);

    // Constructed before the first push, as at startup: the title reads empty.
    ClientModel client(node, gui_state, nullptr);
    VotingModel model(voting, researcher_context, client, options, wallet_model);
    QVERIFY(!model.available());
    QVERIFY(model.getCurrentPollTitle().isEmpty());

    // The first synced push makes polls available, announces it, and the title
    // is then the fixture poll's: this signal is what the overview refreshes on.
    QSignalSpy spy(&model, &VotingModel::availableChanged);
    gui_state.Push(Synced(1, false));
    Deliver();

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());
    QCOMPARE(model.getCurrentPollTitle(), QStringLiteral("title"));
}
