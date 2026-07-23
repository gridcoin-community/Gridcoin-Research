// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_SIDESTAKEMODELTESTS_H
#define BITCOIN_QT_TEST_SIDESTAKEMODELTESTS_H

#include <QObject>
#include <QTest>

//! Phase 1f decoupling proof for SideStakeTableModel: the model is driven
//! entirely by a fake interfaces::SideStakeManager (qt_test::FakeSideStakeManager)
//! with no wallet, sidestake registry, or core global involved. The tests assert
//! the model (a) maps interface value rows onto the table and (b) delegates every
//! add/delete command back across the interface with the arguments it parsed.
class SideStakeModelTests : public QObject
{
    Q_OBJECT

private slots:
    void emptyUntilRefresh();
    void mapsSnapshotToTable();
    void addRowDelegatesToInterface();
    void addRowInvalidAllocationSkipsInterface();
    void removeRowDelegatesToInterface();
    void removeRowRefusesMandatory();
};

#endif // BITCOIN_QT_TEST_SIDESTAKEMODELTESTS_H
