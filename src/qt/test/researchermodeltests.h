// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_RESEARCHERMODELTESTS_H
#define BITCOIN_QT_TEST_RESEARCHERMODELTESTS_H

#include <QObject>
#include <QTest>

//! The researcher model shows the researcher part of the state the core pushes
//! and makes no core call to refresh it. These cases cover what it shows while
//! the part is absent, what it reports when the part changes, what the pages
//! that repaint on researcherChanged read when it reports, and the user actions
//! that keep a core call.
class ResearcherModelTests : public QObject
{
    Q_OBJECT

private slots:
    void absent_researcher_part_shows_out_of_sync_not_investor();
    void researcher_push_takes_no_core_call();
    void a_snapshot_computed_before_the_model_is_read_at_construction();
    void a_node_falling_out_of_sync_shows_as_out_of_sync_once();
    void a_push_without_a_change_refreshes_nothing();
    void an_absent_part_keeps_the_last_figures_out_of_sync();
    void a_page_repainted_on_recovery_sees_the_restored_snapshot();
    void a_reload_is_made_off_the_gui_thread_and_asks_for_an_update();
    void masking_repaints_with_no_core_call();
};

#endif // BITCOIN_QT_TEST_RESEARCHERMODELTESTS_H
