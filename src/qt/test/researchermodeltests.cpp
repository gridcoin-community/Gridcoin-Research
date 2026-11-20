// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/researchermodeltests.h"

#include "qt/researcher/researchermodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/test/researcherfixture.h"

#include <QSignalSpy>
#include <QStringList>

namespace {
const QString VERIFICATION_CODE = QStringLiteral("verification-code");

//! The researcher part in sync, with a pending beacon to authenticate.
interfaces::ResearcherSnapshot InSyncWithPendingBeacon()
{
    interfaces::ResearcherSnapshot snapshot;
    snapshot.out_of_sync = false;
    snapshot.pending_beacon_present = true;
    snapshot.beacon_verification_code = VERIFICATION_CODE.toStdString();
    return snapshot;
}

//! The signals the pages repaint on, counted.
struct Spies
{
    explicit Spies(ResearcherModel& model)
        : researcher(&model, &ResearcherModel::researcherChanged)
        , magnitude(&model, &ResearcherModel::magnitudeChanged)
        , accrual(&model, &ResearcherModel::accrualChanged)
        , beacon(&model, &ResearcherModel::beaconChanged)
    {
    }

    QSignalSpy researcher;
    QSignalSpy magnitude;
    QSignalSpy accrual;
    QSignalSpy beacon;
};
} // anonymous namespace

void ResearcherModelTests::absent_researcher_part_shows_out_of_sync_not_investor()
{
    // No researcher part has ever been pushed, as at startup and while the node
    // syncs. The model must not show a researcher that has no CPID as an
    // investor: it shows the out-of-sync state the overview and the wizard use.
    qt_test::ResearcherFixture f;

    QVERIFY(f.m_model.outOfSync());
    QCOMPARE(f.m_model.formatStatus(), ResearcherModel::tr("Waiting for sync..."));
    QCOMPARE(f.m_model.formatMagnitude(), QStringLiteral("..."));

    bool near_limit = false;
    QCOMPARE(f.m_model.formatAccrual(0, near_limit), QStringLiteral("..."));

    // An absent part pushed later changes none of that.
    f.Push(qt_test::ResearcherFixture::PushedAbsent(1));
    QVERIFY(f.m_model.outOfSync());
    QCOMPARE(f.m_model.formatStatus(), ResearcherModel::tr("Waiting for sync..."));
}

void ResearcherModelTests::researcher_push_takes_no_core_call()
{
    qt_test::ResearcherFixture f;
    Spies spies(f.m_model);

    interfaces::ResearcherSnapshot researcher;
    researcher.out_of_sync = false;
    researcher.magnitude = 50.0;
    researcher.magnitude_text = "50";
    researcher.has_magnitude = true;

    f.Push(qt_test::ResearcherFixture::Pushed(1, researcher));

    QCOMPARE(spies.magnitude.count(), 1);
    QVERIFY(f.m_model.hasMagnitude());
    QCOMPARE(f.m_model.formatMagnitude(), QStringLiteral("50"));
    QCOMPARE(f.m_context.m_snapshot_calls, 0);
    QCOMPARE(f.m_context.m_try_snapshot_calls, 0);
    QCOMPARE(f.m_context.m_reload_calls.load(), 0);
}

void ResearcherModelTests::a_snapshot_computed_before_the_model_is_read_at_construction()
{
    // A state the core pushed before the cache was built emits no change, so
    // the model reads the cache once when it is constructed.
    qt_test::FakeGuiState gui_state;
    gui_state.m_snapshot = qt_test::ResearcherFixture::Pushed(5, InSyncWithPendingBeacon());

    qt_test::FakeResearcherContext context;
    GuiStateCache cache(gui_state);
    ResearcherModel model(context, gui_state, cache);

    QVERIFY(!model.outOfSync());
    QCOMPARE(model.formatBeaconVerificationCode(), VERIFICATION_CODE);
    QCOMPARE(context.m_snapshot_calls, 0);
    QCOMPARE(context.m_try_snapshot_calls, 0);
}

void ResearcherModelTests::a_node_falling_out_of_sync_shows_as_out_of_sync_once()
{
    qt_test::ResearcherFixture f;
    f.Push(qt_test::ResearcherFixture::Pushed(1, InSyncWithPendingBeacon()));
    QVERIFY(!f.m_model.outOfSync());

    // The pages repaint on these: the wizard hides its send button on
    // researcherChanged, and the overview repaints magnitude and accrual only
    // on their own signals.
    Spies spies(f.m_model);

    // The node stops being synced, so the next state has no researcher part.
    f.Push(qt_test::ResearcherFixture::PushedAbsent(2));

    QVERIFY(f.m_model.outOfSync());
    QCOMPARE(f.m_model.formatStatus(), ResearcherModel::tr("Waiting for sync..."));
    QCOMPARE(spies.researcher.count(), 1);
    QCOMPARE(spies.magnitude.count(), 1);
    QCOMPARE(spies.accrual.count(), 1);
    QCOMPARE(spies.beacon.count(), 1);

    // Once per change, not once per push.
    f.Push(qt_test::ResearcherFixture::PushedAbsent(3));
    QCOMPARE(spies.researcher.count(), 1);
    QCOMPARE(spies.magnitude.count(), 1);
    QCOMPARE(spies.accrual.count(), 1);
    QCOMPARE(spies.beacon.count(), 1);
}

void ResearcherModelTests::a_push_without_a_change_refreshes_nothing()
{
    qt_test::ResearcherFixture f;
    f.Push(qt_test::ResearcherFixture::Pushed(1, InSyncWithPendingBeacon()));

    Spies spies(f.m_model);

    // Several pushes, with the researcher part the same throughout.
    f.Push(qt_test::ResearcherFixture::Pushed(2, InSyncWithPendingBeacon()));
    f.Push(qt_test::ResearcherFixture::Pushed(3, InSyncWithPendingBeacon()));

    QVERIFY(!f.m_model.outOfSync());
    QCOMPARE(spies.researcher.count(), 0);
    QCOMPARE(spies.magnitude.count(), 0);
    QCOMPARE(spies.accrual.count(), 0);
    QCOMPARE(spies.beacon.count(), 0);
}

void ResearcherModelTests::an_absent_part_keeps_the_last_figures_out_of_sync()
{
    qt_test::ResearcherFixture f;

    interfaces::ResearcherSnapshot researcher = InSyncWithPendingBeacon();
    researcher.email = "someone@example.com";
    f.Push(qt_test::ResearcherFixture::Pushed(1, researcher));

    f.Push(qt_test::ResearcherFixture::PushedAbsent(2));

    // The last snapshot is kept, marked out of sync.
    QVERIFY(f.m_model.outOfSync());
    QCOMPARE(f.m_model.email(), QStringLiteral("someone@example.com"));

    // The part comes back, and the model is in sync with it.
    f.Push(qt_test::ResearcherFixture::Pushed(3, researcher));
    QVERIFY(!f.m_model.outOfSync());
}

void ResearcherModelTests::a_page_repainted_on_recovery_sees_the_restored_snapshot()
{
    // The wizard's authentication page repaints its verification code on
    // researcherChanged alone, so the signal that reports recovery has to come
    // after the snapshot that has the code again.
    qt_test::ResearcherFixture f;
    f.Push(qt_test::ResearcherFixture::Pushed(1, InSyncWithPendingBeacon()));
    QCOMPARE(f.m_model.formatBeaconVerificationCode(), VERIFICATION_CODE);

    QStringList codes_seen;
    QObject::connect(&f.m_model, &ResearcherModel::researcherChanged, &f.m_model, [&] {
        codes_seen << f.m_model.formatBeaconVerificationCode();
    });

    // Out of sync, the node's snapshot has no beacon fields; the pushed part
    // is absent and the model still holds the last snapshot.
    f.Push(qt_test::ResearcherFixture::PushedAbsent(2));
    QVERIFY(f.m_model.outOfSync());

    f.Push(qt_test::ResearcherFixture::Pushed(3, InSyncWithPendingBeacon()));
    QVERIFY(!f.m_model.outOfSync());

    QVERIFY(!codes_seen.isEmpty());
    QCOMPARE(codes_seen.last(), VERIFICATION_CODE);
}

void ResearcherModelTests::a_reload_is_made_off_the_gui_thread_and_asks_for_an_update()
{
    qt_test::ResearcherFixture f;
    QSignalSpy reloaded(&f.m_model, &ResearcherModel::reloaded);

    f.m_model.reload();

    // The core's reload runs on a worker, and the model then asks for the update
    // that carries its result. It takes no snapshot itself.
    QTRY_COMPARE(reloaded.count(), 1);
    QCOMPARE(f.m_context.m_reload_calls.load(), 1);
    QCOMPARE(f.m_gui_state.m_request_update_calls, 1);
    QCOMPARE(f.m_context.m_snapshot_calls, 0);
    QCOMPARE(f.m_context.m_try_snapshot_calls, 0);
}

void ResearcherModelTests::masking_repaints_with_no_core_call()
{
    qt_test::ResearcherFixture f;
    interfaces::ResearcherSnapshot researcher = InSyncWithPendingBeacon();
    researcher.cpid = "abcdef";
    f.Push(qt_test::ResearcherFixture::Pushed(1, researcher));
    QCOMPARE(f.m_model.formatCpid(), QStringLiteral("abcdef"));

    Spies spies(f.m_model);

    f.m_model.setMaskCpidMagnitudeAccrual(true);

    QCOMPARE(spies.researcher.count(), 1);
    QCOMPARE(spies.magnitude.count(), 1);
    QCOMPARE(spies.accrual.count(), 1);
    QVERIFY(f.m_model.formatCpid() != QStringLiteral("abcdef"));
    QCOMPARE(f.m_context.m_snapshot_calls, 0);
    QCOMPARE(f.m_context.m_try_snapshot_calls, 0);
}
