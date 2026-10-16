// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/researchermodeltests.h"

#include "qt/researcher/researchermodel.h"
#include "qt/test/interfacefakes.h"

#include <QSignalSpy>
#include <QStringList>
#include <QTimer>

namespace {
//! The model's sync-state poll, shortened so a case does not wait 30 seconds.
QTimer* ShortenSyncStatePoll(ResearcherModel& model)
{
    auto* timer = model.findChild<QTimer*>("syncStateTimer");

    if (timer) {
        timer->setInterval(10);
    }

    return timer;
}

const QString VERIFICATION_CODE = QStringLiteral("verification-code");

//! The node's snapshot in sync, with a pending beacon to authenticate.
void SetInSyncWithPendingBeacon(qt_test::FakeResearcherContext& context)
{
    context.m_snapshot.out_of_sync = false;
    context.m_snapshot.pending_beacon_present = true;
    context.m_snapshot.beacon_verification_code = VERIFICATION_CODE.toStdString();
}

//! The node's snapshot out of sync: it is built without the beacon fields.
void SetOutOfSync(qt_test::FakeResearcherContext& context)
{
    context.m_snapshot.out_of_sync = true;
    context.m_snapshot.pending_beacon_present = false;
    context.m_snapshot.beacon_verification_code.clear();
}
} // anonymous namespace

void ResearcherModelTests::aStalledNodeShowsAsOutOfSyncWithoutANotification()
{
    qt_test::FakeResearcherContext context;
    ResearcherModel model(context);

    QVERIFY(!model.outOfSync());

    QTimer* timer = ShortenSyncStatePoll(model);
    QVERIFY(timer);
    QVERIFY(timer->isActive());

    // The pages repaint on these: the wizard hides its send button on
    // researcherChanged, and the overview repaints magnitude and accrual only
    // on their own signals.
    QSignalSpy researcher_changed(&model, &ResearcherModel::researcherChanged);
    QSignalSpy magnitude_changed(&model, &ResearcherModel::magnitudeChanged);
    QSignalSpy accrual_changed(&model, &ResearcherModel::accrualChanged);
    QSignalSpy beacon_changed(&model, &ResearcherModel::beaconChanged);

    // Blocks stop arriving. The node's flag turns true, and nothing notifies.
    context.m_snapshot.out_of_sync = true;

    QTRY_VERIFY(model.outOfSync());
    QCOMPARE(model.formatStatus(), ResearcherModel::tr("Waiting for sync..."));

    // Once per change, not once per poll.
    QTest::qWait(100);
    QCOMPARE(researcher_changed.count(), 1);
    QCOMPARE(magnitude_changed.count(), 1);
    QCOMPARE(accrual_changed.count(), 1);
    QCOMPARE(beacon_changed.count(), 1);
}

void ResearcherModelTests::aPollWithoutAChangeRefreshesNothing()
{
    qt_test::FakeResearcherContext context;
    ResearcherModel model(context);

    QTimer* timer = ShortenSyncStatePoll(model);
    QVERIFY(timer);

    QSignalSpy researcher_changed(&model, &ResearcherModel::researcherChanged);
    QSignalSpy magnitude_changed(&model, &ResearcherModel::magnitudeChanged);
    QSignalSpy accrual_changed(&model, &ResearcherModel::accrualChanged);
    QSignalSpy beacon_changed(&model, &ResearcherModel::beaconChanged);

    // Several polls, with the node in sync throughout.
    QTest::qWait(100);

    QVERIFY(!model.outOfSync());
    QCOMPARE(researcher_changed.count(), 0);
    QCOMPARE(magnitude_changed.count(), 0);
    QCOMPARE(accrual_changed.count(), 0);
    QCOMPARE(beacon_changed.count(), 0);
}

void ResearcherModelTests::aSnapshotSkippedWhileMainWasBusyIsRetried()
{
    qt_test::FakeResearcherContext context;
    ResearcherModel model(context);

    QTimer* timer = ShortenSyncStatePoll(model);
    QVERIFY(timer);

    QSignalSpy magnitude_changed(&model, &ResearcherModel::magnitudeChanged);
    QSignalSpy accrual_changed(&model, &ResearcherModel::accrualChanged);

    // The poll that sees the change cannot take the snapshot, so magnitude and
    // accrual are not repainted, though the flag itself is updated.
    context.m_main_busy = true;
    context.m_snapshot.out_of_sync = true;

    QTRY_VERIFY(model.outOfSync());
    QTest::qWait(50);
    QCOMPARE(magnitude_changed.count(), 0);
    QCOMPARE(accrual_changed.count(), 0);

    // With no change left to see, a later poll still takes the snapshot.
    context.m_main_busy = false;

    QTRY_COMPARE(magnitude_changed.count(), 1);
    QCOMPARE(accrual_changed.count(), 1);
}

void ResearcherModelTests::aPageRepaintedOnRecoverySeesTheRestoredSnapshot()
{
    // The wizard's authentication page repaints its verification code on
    // researcherChanged alone, so the signal that reports recovery has to come
    // after the snapshot that has the code again.
    qt_test::FakeResearcherContext context;
    SetInSyncWithPendingBeacon(context);

    ResearcherModel model(context);
    QCOMPARE(model.formatBeaconVerificationCode(), VERIFICATION_CODE);

    QTimer* timer = ShortenSyncStatePoll(model);
    QVERIFY(timer);

    QStringList codes_seen;
    QObject::connect(&model, &ResearcherModel::researcherChanged, &model, [&] {
        codes_seen << model.formatBeaconVerificationCode();
    });

    SetOutOfSync(context);
    QTRY_VERIFY(model.outOfSync());

    SetInSyncWithPendingBeacon(context);
    QTRY_VERIFY(!model.outOfSync());
    QTest::qWait(50);

    QVERIFY(!codes_seen.isEmpty());
    QCOMPARE(codes_seen.last(), VERIFICATION_CODE);
}

void ResearcherModelTests::aRecoveryReportedWithoutItsSnapshotIsReportedAgainWithIt()
{
    qt_test::FakeResearcherContext context;
    SetInSyncWithPendingBeacon(context);

    ResearcherModel model(context);

    QTimer* timer = ShortenSyncStatePoll(model);
    QVERIFY(timer);

    QStringList codes_seen;
    QObject::connect(&model, &ResearcherModel::researcherChanged, &model, [&] {
        codes_seen << model.formatBeaconVerificationCode();
    });

    SetOutOfSync(context);
    QTRY_VERIFY(model.outOfSync());

    // The poll that sees the recovery cannot take the snapshot. It reports the
    // flag at once, while the pages still have the out-of-sync snapshot to read.
    context.m_main_busy = true;
    SetInSyncWithPendingBeacon(context);
    QTRY_VERIFY(!model.outOfSync());
    QTest::qWait(50);
    QVERIFY(!codes_seen.isEmpty());
    QCOMPARE(codes_seen.last(), QString());

    // The retry that takes it reports again, so the pages repaint from it.
    context.m_main_busy = false;
    QTRY_COMPARE(codes_seen.last(), VERIFICATION_CODE);
}

void ResearcherModelTests::aRecoveryTheFlagReadMissedIsReportedFromTheSnapshot()
{
    // The flag is read without cs_main and the snapshot taken under it, so a
    // block can finish between the two: the flag read says out of sync, and the
    // snapshot holds the recovery. The snapshot decides. Every later flag read
    // matches the installed snapshot, so a recovery not reported here would
    // never be, and the authentication page would keep its blank code.
    qt_test::FakeResearcherContext context;
    SetInSyncWithPendingBeacon(context);

    ResearcherModel model(context);

    QTimer* timer = ShortenSyncStatePoll(model);
    QVERIFY(timer);

    QStringList codes_seen;
    QObject::connect(&model, &ResearcherModel::researcherChanged, &model, [&] {
        codes_seen << model.formatBeaconVerificationCode();
    });

    SetOutOfSync(context);
    QTRY_VERIFY(model.outOfSync());
    QVERIFY(!codes_seen.isEmpty());
    QCOMPARE(codes_seen.last(), QString());

    // The node recovers while the flag still reads as out of sync. A new block
    // runs the refresh, which takes the recovered snapshot.
    SetInSyncWithPendingBeacon(context);
    context.m_out_of_sync_flag = true;
    model.refresh();

    QVERIFY(!model.outOfSync());
    QCOMPARE(codes_seen.last(), VERIFICATION_CODE);

    // Once the flag agrees with the snapshot, the polls have nothing to report.
    const int reported = codes_seen.size();
    context.m_out_of_sync_flag.reset();
    QTest::qWait(50);
    QCOMPARE(codes_seen.size(), reported);
}
