// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_COINSELECTIONVIEWTESTS_H
#define BITCOIN_QT_TEST_COINSELECTIONVIEWTESTS_H

#include <QObject>
#include <QTest>

//! #3228 item 3, the VIEW half: CoinSelectionModel reports a re-slotted branch
//! by stable id (covered by CoinSelectionModelTests) and CoinSelectionView has
//! to turn that report back into an expanded, REALIZED branch without moving
//! the scrollbar.
//!
//! Both halves of that are easy to get silently wrong, which is why they are
//! asserted separately here. QTreeView::expand() calls fetchMore()
//! synchronously and CoinSelectionModel::canFetchMore() answers false while a
//! batch bracket is open, so an expand issued too early marks the node
//! expanded with the realization dropped -- isExpanded() alone would report
//! success on a branch that renders empty. Every test therefore also asserts
//! rowCount() on the restored branch.
//!
//! Runs under the offscreen platform (see test_main.cpp); it needs a real
//! QTreeView with a laid-out viewport, not a mock.
class CoinSelectionViewTests : public QObject
{
    Q_OBJECT

private slots:
    void reslotReExpandsAndRealizesTheBranch();
    void reslotOfACollapsedBranchStaysCollapsed();
    void userCollapseIsNotUndoneByAReslot();
    void userCollapseSurvivesAReslotInTheSameTurn();
    void resetBetweenEmissionAndContinuationDoesNotReExpand();
    void reslotHoldsTheScrollOffset();
    void swappingTheModelDropsTheOldConnection();
};

#endif // BITCOIN_QT_TEST_COINSELECTIONVIEWTESTS_H
