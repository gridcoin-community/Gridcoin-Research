// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/mrcmodeltests.h"

#include "interfaces/wallet_coin_source.h"
#include "qt/bitcoinunits.h"
#include "qt/clientmodel.h"
#include "qt/mrcmodel.h"
#include "qt/mrcrequestpage.h"
#include "qt/optionsmodel.h"
#include "qt/researcher/researchermodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"

#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QTest>

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

    // The researcher part the node pushes beside it: the MRC page reads the
    // researcher model for eligibility, which the node's snapshot carries.
    snapshot.has_researcher = true;
    snapshot.researcher.has_active_beacon = true;
    snapshot.researcher.out_of_sync = false;
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

    Fixture()
        : client_model(node, gui_state, nullptr)
        , researcher_model(researcher_context, gui_state, *client_model.guiState())
        , model(mrc, gui_state, nullptr, &client_model, &researcher_model)
    {
    }

    void Push(uint64_t sequence, const interfaces::MRCSnapshot& figures)
    {
        gui_state.Push(Pushed(sequence, figures));
        Deliver();
    }
};

//! A fixture with a wallet model on a fake wallet, to follow the submit's
//! unlock context. The model is heap-held so a test can destroy it while the
//! rest stands.
struct WalletFixture
{
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    qt_test::FakeGuiState gui_state;
    qt_test::FakeMRC mrc;
    qt_test::FakeResearcherContext researcher_context;
    qt_test::FakeWallet wallet;
    qt_test::FakeWalletTxSource tx_source;
    std::shared_ptr<interfaces::WalletCoinSource> coin_source = interfaces::MakeSyntheticCoinSource(0, 0);
    OptionsModel options;
    ClientModel client_model;
    ResearcherModel researcher_model;
    WalletModel wallet_model;
    std::unique_ptr<MRCModel> model;

    WalletFixture()
        : options(node, sidestakes)
        , client_model(node, gui_state, nullptr)
        , researcher_model(researcher_context, gui_state, *client_model.guiState())
        , wallet_model(wallet, tx_source, *coin_source, &options)
        , model(std::make_unique<MRCModel>(mrc, gui_state, &wallet_model, &client_model, &researcher_model))
    {
        gui_state.Push(Pushed(1, Figures(100, 10)));
        Deliver();
        mrc.m_confirmed_result.success = true;
        mrc.m_confirmed_result.submitted_height = 100;
        mrc.m_confirmed_result.research_subsidy = 100;
    }

    //! A valid context that ends its capability when released.
    std::unique_ptr<WalletModel::UnlockContext> CapabilityContext()
    {
        return std::make_unique<WalletModel::UnlockContext>(&wallet_model, true, true);
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
    QVERIFY(f.model.submitMRC(status, error, f.model.getMRCTotalFee(), f.model.getMRCNetPayout(), nullptr));
    QVERIFY(f.model.isSubmitting());
    QTRY_VERIFY(!f.model.isSubmitting());

    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 1);
    QCOMPARE(f.mrc.m_last_confirmed_fee.load(), CAmount{10});
    QCOMPARE(f.mrc.m_last_confirmed_net.load(), CAmount{90});
    QCOMPARE(f.mrc.m_submit_calls.load(), 0);
    QCOMPARE(f.model.getMRCRequestStatus(), MRCRequestStatus::PENDING);
}

void MRCModelTests::the_submit_unlock_is_released_when_the_submit_finishes()
{
    WalletFixture f;

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model->submitMRC(status, error, f.model->getMRCTotalFee(), f.model->getMRCNetPayout(),
                               f.CapabilityContext()));
    QVERIFY(f.model->isSubmitting());

    // Held while the submit runs and until it is reported finished.
    QCOMPARE(f.wallet.m_end_elevation_calls, 0);

    QTRY_VERIFY(!f.model->isSubmitting());
    QCOMPARE(f.wallet.m_end_elevation_calls, 1);
}

void MRCModelTests::the_submit_unlock_is_released_when_the_model_is_destroyed_mid_submit()
{
    WalletFixture f;

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model->submitMRC(status, error, f.model->getMRCTotalFee(), f.model->getMRCNetPayout(),
                               f.CapabilityContext()));
    QCOMPARE(f.wallet.m_end_elevation_calls, 0);

    // The finished signal is never delivered: the event loop does not run.
    f.model.reset();

    QCOMPARE(f.wallet.m_end_elevation_calls, 1);
}

void MRCModelTests::submit_sends_the_terms_given_at_the_click()
{
    Fixture f;
    f.Push(1, Figures(100, 10));
    f.mrc.m_confirmed_result.success = true;
    f.mrc.m_confirmed_result.submitted_height = 100;
    f.mrc.m_confirmed_result.research_subsidy = 100;

    // The terms the user confirmed at the click.
    const CAmount clicked_fee = f.model.getMRCTotalFee();
    const CAmount clicked_net = f.model.getMRCNetPayout();
    QCOMPARE(clicked_fee, CAmount{10});
    QCOMPARE(clicked_net, CAmount{90});

    // A push lands before the submit starts (the unlock prompt's event loop)
    // and moves the figures to worse ones.
    f.Push(2, Figures(80, 30));
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{30});
    QCOMPARE(f.model.getMRCNetPayout(), CAmount{50});

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model.submitMRC(status, error, clicked_fee, clicked_net, nullptr));
    QTRY_VERIFY(!f.model.isSubmitting());

    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 1);
    QCOMPARE(f.mrc.m_last_confirmed_fee.load(), clicked_fee);
    QCOMPARE(f.mrc.m_last_confirmed_net.load(), clicked_net);
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
    QVERIFY(f.model.submitMRC(status, error, f.model.getMRCTotalFee(), f.model.getMRCNetPayout(), nullptr));
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
    QVERIFY(f.model.submitMRC(status, error, f.model.getMRCTotalFee(), f.model.getMRCNetPayout(), nullptr));
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
    QVERIFY(!f.model.submitMRC(status, error, f.model.getMRCTotalFee(), f.model.getMRCNetPayout(), nullptr));
    QCOMPARE(f.mrc.m_submit_confirmed_calls.load(), 0);

    // The same figures once unlocked.
    f.model.walletStatusChanged(static_cast<int>(WalletModel::EncryptionStatus::Unlocked));
    QCOMPARE(f.model.getMRCRequestStatus(), MRCRequestStatus::ELIGIBLE);
    QVERIFY(submit->isEnabled());
}

void MRCModelTests::a_push_with_an_unchanged_mrc_part_does_not_requote()
{
    Fixture f;
    f.Push(1, Figures(100, 10));

    CAmount boost = 5;
    f.model.setMRCFeeBoost(boost);
    QTRY_COMPARE(f.gui_state.m_quote_requests, 1);
    const uint64_t id = f.gui_state.m_last_quote_request_id;

    // The quote lands; its figures are on the page.
    interfaces::MRCSnapshot quote = Figures(100, 10);
    quote.fee = 15;
    f.gui_state.AnswerQuote(id, quote);
    Deliver();
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{15});
    QVERIFY(!f.model.isQuoteUpdating());

    // A push with the same MRC part asks for no quote and leaves the quoted
    // figures and the settled state alone.
    f.Push(2, Figures(100, 10));
    QTest::qWait(300);
    QCOMPARE(f.gui_state.m_quote_requests, 1);
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{15});
    QVERIFY(!f.model.isQuoteUpdating());

    // A push whose MRC part changed does.
    f.Push(3, Figures(120, 10));
    QTRY_COMPARE(f.gui_state.m_quote_requests, 2);
    QVERIFY(f.model.isQuoteUpdating());
}

void MRCModelTests::fresh_terms_survive_an_unchanged_push()
{
    Fixture f;
    f.Push(1, Figures(100, 10));

    f.mrc.m_confirmed_result.terms_changed = true;
    f.mrc.m_confirmed_result.error = "terms changed";
    f.mrc.m_confirmed_result.fresh_fee = 12;
    f.mrc.m_confirmed_result.fresh_reward = 100;

    MRCRequestStatus status;
    QString error;
    QVERIFY(f.model.submitMRC(status, error, f.model.getMRCTotalFee(), f.model.getMRCNetPayout(), nullptr));
    QTRY_VERIFY(!f.model.isSubmitting());
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{12});

    // An unchanged push keeps the figures the notice quoted.
    f.Push(2, Figures(100, 10));
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{12});
    QCOMPARE(f.model.getMRCNetPayout(), CAmount{88});

    // A changed one replaces them.
    f.Push(3, Figures(120, 10));
    QCOMPARE(f.model.getMRCTotalFee(), CAmount{10});
}
