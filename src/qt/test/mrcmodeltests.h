// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_MRCMODELTESTS_H
#define BITCOIN_QT_TEST_MRCMODELTESTS_H

#include <QObject>
#include <QTest>

//! The MRC model classifies from the state the core pushes, takes fee-boost
//! quotes from the core on request, and submits on the terms the user saw.
class MRCModelTests : public QObject
{
    Q_OBJECT

private slots:
    void mrc_model_takes_no_core_call_on_a_push();
    void mrc_model_shows_out_of_sync_while_absent();
    void fee_boost_quote_latest_answer_wins();
    void a_failed_quote_is_a_create_error_not_a_balance_error();
    void a_late_quote_does_not_undo_an_out_of_sync_push();
    void submit_sends_the_confirmed_terms();
    void submit_refused_on_changed_terms_shows_fresh_figures();
    void submit_error_shows_the_error_and_requotes();
    void mrc_page_follows_lock_state_and_refuses_when_locked();
};

#endif // BITCOIN_QT_TEST_MRCMODELTESTS_H
