// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_TEST_STATUSADAPTERTESTS_H
#define BITCOIN_QT_QUICK_TEST_STATUSADAPTERTESTS_H

#include <QObject>
#include <QTest>

//! The status adapters of Gridcoin.Qml over the real GUI models, built on fakes
//! by qml_test::ModelHarness. None runs the application's event loop.
class QmlStatusAdapterTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void harnessBuildsModels();
    void unitsFormatting();
    void syncRuleTruthTable();
    void blocksText();
    void statusReadsOffGuiThreadAndCoalesce();
    void connectionAndStakingStates();
    void scraperSnapshotPooled();
    void hydrateOnAttachAndSignalWins();
    void totalIncludesImmatureAndCopy();
    void encryptionStatusKnownAndDedup();
    void maskEveryAmount();
    void recentTransactionsRoles();
    void researcherModeAndLimit();
    void pollTitleRefresh();
    void updateCheckLaneAndGates();
    void detachOrder();
    void attachModelsAttachesAll();
    void clipboardStrips();
    void statusReadSetsConnections();
    void syncProgressAndToolTip();
    void notStakingArm();
    void scraperStates();
    void scraperReadsCoalesce();
    void roundedBalanceText();
    void lockStatesAndToolTips();
    void beaconToolTipEscapes();
    void unitChangeEmitsDataChanged();
    void pollTitleReadsCoalesce();
    void scraperStartsWaiting();
    void stakingToolTipFollowsMask();
};

#endif // BITCOIN_QT_QUICK_TEST_STATUSADAPTERTESTS_H
