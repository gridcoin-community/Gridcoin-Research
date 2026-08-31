// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_COINSELECTIONMODELTESTS_H
#define BITCOIN_QT_TEST_COINSELECTIONMODELTESTS_H

#include <QObject>
#include <QTest>

//! #3228 item 3: expansion continuity across a directory RE-SLOT. When a
//! group's sort key moves, CoinViews publishes GroupRemove + GroupInsert (an
//! in-place GroupChange would desynchronize the consumer's positional
//! coordinates), and the removal drops the realized child cache along with
//! QTreeView's expansion -- though the user never collapsed anything. The
//! model has to tell that apart from a real collapse and from a real removal,
//! and report the branches to restore by stable id.
//!
//! Driven through the model's test seam (a bare interfaces::WalletCoinSource,
//! no WalletModel) over the synthetic coin source, with batches injected
//! straight into applyCoinEventBatch. The view half -- QTreeView re-expansion,
//! the scroll restore, and the continuation guards -- is covered separately by
//! CoinSelectionViewTests, which drives a real QTreeView under a real
//! QApplication on the offscreen platform.
class CoinSelectionModelTests : public QObject
{
    Q_OBJECT

private slots:
    void reslotOfAnExpandedGroupIsReported();
    void reslotOfACollapsedGroupIsNotReported();
    void realRemovalIsNotReported();
    void userCollapseForgetsTheExpansion();
    void reportedIdResolvesToTheMovedRow();
};

#endif // BITCOIN_QT_TEST_COINSELECTIONMODELTESTS_H
