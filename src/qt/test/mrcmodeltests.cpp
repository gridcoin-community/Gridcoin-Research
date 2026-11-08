// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/mrcmodeltests.h"

#include "qt/bitcoinunits.h"
#include "qt/clientmodel.h"
#include "qt/mrcmodel.h"
#include "qt/mrcrequestpage.h"
#include "qt/researcher/researchermodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"

#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>

namespace {
interfaces::MRCSnapshot Figures(CAmount reward, CAmount min_fee)
{
    interfaces::MRCSnapshot mrc;
    mrc.block_height = 100;
    mrc.block_version_valid = true;
    mrc.output_limit = 10;
    mrc.reward = reward;
    mrc.min_fee = min_fee;
    mrc.mature_balance = 5 * COIN;
    return mrc;
}

interfaces::GuiStateSnapshot Pushed(uint64_t sequence, const interfaces::MRCSnapshot& mrc)
{
    interfaces::GuiStateSnapshot snapshot;
    snapshot.sequence = sequence;
    snapshot.out_of_sync = false;
    snapshot.has_mrc = true;
    snapshot.mrc = mrc;
    return snapshot;
}

interfaces::GuiStateSnapshot PushedAbsent(uint64_t sequence)
{
    interfaces::GuiStateSnapshot snapshot;
    snapshot.sequence = sequence;
    return snapshot;
}

void Deliver()
{
    QCoreApplication::sendPostedEvents(nullptr, 0);
}

//! The model and the fakes it stands on. There is no wallet model: the page
//! and the model tolerate its absence, and the lock state is set by calling
//! walletStatusChanged.
struct Fixture
{
    qt_test::FakeNode node;
    qt_test::FakeGuiState gui_state;
    qt_test::FakeMRC mrc;
    qt_test::FakeResearcherContext researcher_context;
    ClientModel client_model;
    ResearcherModel researcher_model;
    MRCModel model;

    static interfaces::ResearcherSnapshot Researcher()
    {
        interfaces::ResearcherSnapshot snapshot;
        snapshot.has_active_beacon = true;
        snapshot.out_of_sync = false;
        return snapshot;
    }

    Fixture()
        : client_model(node, gui_state, nullptr)
        , researcher_model((researcher_context.m_snapshot = Researcher(), researcher_context))
        , model(mrc, gui_state, nullptr, &client_model, &researcher_model)
    {
    }

    void Push(uint64_t sequence, const interfaces::MRCSnapshot& figures)
    {
        gui_state.Push(Pushed(sequence, figures));
        Deliver();
    }
};

QString Money(CAmount amount)
{
    return BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, amount);
}
} // anonymous namespace

void MRCModelTests::mrc_model_takes_no_core_call_on_a_push()
{
    Fixture f;

    f.Push(1, Figures(100, 10));

    MRCRequestStatus status;
    QString error;
    QVERIFY(!f.model.isMRCError(status, error));
    QCOMPARE(f.model.getMRCRequestStatus(), MRCRequestStatus::ELIGIBLE);
    QCOMPARE(f.mrc.m_snapshot_calls.load(), 0);
    QCOMPARE(f.mrc.m_submit_calls.load(), 0);
    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 0);
    QCOMPARE(f.gui_state.m_quote_requests, 0);
}

void MRCModelTests::mrc_model_shows_out_of_sync_while_absent()
{
    Fixture f;

    f.gui_state.Push(PushedAbsent(1));
    Deliver();

    QCOMPARE(f.model.getMRCModelStatus(), MRCModel::ModelStatus::OUT_OF_SYNC);

    // Present, then absent again.
    f.Push(2, Figures(100, 10));
    QCOMPARE(f.model.getMRCRequestStatus(), MRCRequestStatus::ELIGIBLE);

    f.gui_state.Push(PushedAbsent(3));
    Deliver();
    QCOMPARE(f.model.getMRCModelStatus(), MRCModel::ModelStatus::OUT_OF_SYNC);
    QCOMPARE(f.mrc.m_snapshot_calls.load(), 0);
}

void MRCModelTests::fee_boost_quote_latest_answer_wins()
{
    Fixture f;
    f.Push(1, Figures(100, 10));

    CAmount boost = 5;
    f.model.setMRCFeeBoost(boost);
    QVERIFY(f.model.isQuoteUpdating());
    QTRY_COMPARE(f.gui_state.m_quote_requests, 1);
    QCOMPARE(f.gui_state.m_last_quote_fee_boost, CAmount{5});
    const uint64_t id1 = f.gui_state.m_last_quote_request_id;

    boost = 7;
    f.model.setMRCFeeBoost(boost);
    QTRY_COMPARE(f.gui_state.m_quote_requests, 2);
    QCOMPARE(f.gui_state.m_last_quote_fee_boost, CAmount{7});
    const uint64_t id2 = f.gui_state.m_last_quote_request_id;
    QVERIFY(id2 != id1);

    interfaces::MRCSnapshot stale = Figures(111, 10);
    stale.fee = 15;
    f.gui_state.AnswerQuote(id1, stale);
    Deliver();
    QCOMPARE(f.model.getMRCReward(), CAmount{100});
    QVERIFY(f.model.isQuoteUpdating());

    interfaces::MRCSnapshot latest = Figures(222, 10);
    latest.fee = 17;
    f.gui_state.AnswerQuote(id2, latest);
    Deliver();
    QCOMPARE(f.model.getMRCReward(), CAmount{222});
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{17});
    QCOMPARE(f.model.getMRCNetPayout(), CAmount{205});
    QVERIFY(!f.model.isQuoteUpdating());
    QCOMPARE(f.mrc.m_snapshot_calls.load(), 0);
}

void MRCModelTests::a_failed_quote_is_a_create_error_not_a_balance_error()
{
    Fixture f;
    f.Push(1, Figures(100, 10));
    // A later block, so the model is past its first one.
    interfaces::MRCSnapshot later = Figures(100, 10);
    later.block_height = 101;
    f.Push(2, later);
    MRCRequestPage page(nullptr, &f.model);
    QCOMPARE(f.model.getMRCModelStatus(), MRCModel::ModelStatus::VALID);

    CAmount boost = 5;
    f.model.setMRCFeeBoost(boost);
    QTRY_COMPARE(f.gui_state.m_quote_requests, 1);

    // What the node answers when the quote itself failed: nothing else filled in.
    interfaces::MRCSnapshot failed;
    failed.create_error = true;
    failed.create_error_msg = "The MRC figures could not be computed: x";
    f.gui_state.AnswerQuote(f.gui_state.m_last_quote_request_id, failed);
    Deliver();

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model.isMRCError(status, error));
    QCOMPARE(status, MRCRequestStatus::CREATE_ERROR);
    QCOMPARE(error, QString("The MRC figures could not be computed: x"));
    QVERIFY(!f.model.isQuoteUpdating());

    // The chain state of the last good figures stands: the page shows the
    // error, not a block version message.
    QCOMPARE(f.model.getMRCModelStatus(), MRCModel::ModelStatus::VALID);
    QVERIFY(!page.findChild<QWidget*>("mrcStatusSubmitFrame")->isHidden());
    QVERIFY(page.findChild<QWidget*>("waitForNextBlockUpdateFrame")->isHidden());
    QVERIFY(page.findChild<QLabel*>("ErrorIconLabel")->toolTip().contains("could not be computed"));
    QVERIFY(!page.findChild<QPushButton*>("mrcSubmitButton")->isEnabled());
}

void MRCModelTests::a_late_quote_does_not_undo_an_out_of_sync_push()
{
    Fixture f;
    f.Push(1, Figures(100, 10));

    CAmount boost = 5;
    f.model.setMRCFeeBoost(boost);
    QTRY_COMPARE(f.gui_state.m_quote_requests, 1);
    const uint64_t id = f.gui_state.m_last_quote_request_id;

    f.gui_state.Push(PushedAbsent(2));
    Deliver();
    QCOMPARE(f.model.getMRCModelStatus(), MRCModel::ModelStatus::OUT_OF_SYNC);

    f.gui_state.AnswerQuote(id, Figures(222, 10));
    Deliver();
    QCOMPARE(f.model.getMRCModelStatus(), MRCModel::ModelStatus::OUT_OF_SYNC);
}

void MRCModelTests::submit_sends_the_confirmed_terms()
{
    Fixture f;
    f.Push(1, Figures(100, 10));
    f.mrc.m_confirmed_result.success = true;
    f.mrc.m_confirmed_result.submitted_height = 100;
    f.mrc.m_confirmed_result.research_subsidy = 100;

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model.submitMRC(status, error));
    QVERIFY(f.model.isSubmitting());
    QTRY_VERIFY(!f.model.isSubmitting());

    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 1);
    QCOMPARE(f.mrc.m_last_confirmed_fee.load(), CAmount{10});
    QCOMPARE(f.mrc.m_last_confirmed_net.load(), CAmount{90});
    QCOMPARE(f.mrc.m_submit_calls.load(), 0);
    QCOMPARE(f.model.getMRCRequestStatus(), MRCRequestStatus::PENDING);
}

void MRCModelTests::submit_refused_on_changed_terms_shows_fresh_figures()
{
    Fixture f;
    f.Push(1, Figures(100, 10));
    MRCRequestPage page(nullptr, &f.model);

    f.mrc.m_confirmed_result.terms_changed = true;
    f.mrc.m_confirmed_result.error = "terms changed";
    f.mrc.m_confirmed_result.fresh_fee = 12;
    f.mrc.m_confirmed_result.fresh_reward = 100;

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model.submitMRC(status, error));
    QTRY_VERIFY(!f.model.isSubmitting());

    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 1);
    QVERIFY(f.model.getMRCRequestStatus() != MRCRequestStatus::PENDING);
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{12});
    QCOMPARE(f.model.getMRCNetPayout(), CAmount{88});

    auto* total_fee = page.findChild<QLabel*>("mrcTotalFee");
    auto* net_payout = page.findChild<QLabel*>("mrcNetPayout");
    QVERIFY(total_fee && net_payout);
    QCOMPARE(total_fee->text(), Money(12));
    QCOMPARE(net_payout->text(), Money(88));

    // Never resubmitted by itself.
    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 1);
}

void MRCModelTests::submit_error_shows_the_error_and_requotes()
{
    Fixture f;
    f.Push(1, Figures(100, 10));
    f.mrc.m_confirmed_result.error = "the boost now exceeds the reward";

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model.submitMRC(status, error));
    QTRY_VERIFY(!f.model.isSubmitting());

    QVERIFY(f.model.isMRCError(status, error));
    QCOMPARE(status, MRCRequestStatus::SUBMIT_ERROR);
    QVERIFY(error.contains("the boost now exceeds the reward"));
    QCOMPARE(f.gui_state.m_quote_requests, 1);
    QVERIFY(f.model.isQuoteUpdating());
    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 1);
}

void MRCModelTests::mrc_page_follows_lock_state_and_refuses_when_locked()
{
    Fixture f;
    MRCRequestPage page(nullptr, &f.model);

    f.model.walletStatusChanged(static_cast<int>(WalletModel::EncryptionStatus::Locked));
    f.Push(1, Figures(100, 10));

    QVERIFY(f.model.isWalletLocked());
    QCOMPARE(f.model.getMRCRequestStatus(), MRCRequestStatus::WALLET_LOCKED);

    auto* submit = page.findChild<QPushButton*>("mrcSubmitButton");
    QVERIFY(submit);
    QVERIFY(!submit->isEnabled());
    QVERIFY(submit->toolTip().contains("locked"));

    MRCRequestStatus status;
    QString error;
    QVERIFY(!f.model.submitMRC(status, error));
    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 0);

    // The same figures once unlocked.
    f.model.walletStatusChanged(static_cast<int>(WalletModel::EncryptionStatus::Unlocked));
    QCOMPARE(f.model.getMRCRequestStatus(), MRCRequestStatus::ELIGIBLE);
    QVERIFY(submit->isEnabled());
}
