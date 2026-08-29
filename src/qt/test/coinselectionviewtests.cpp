// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "coinselectionviewtests.h"

#include "interfaces/wallet.h"
#include "interfaces/wallet_coin_channel.h"
#include "interfaces/wallet_coin_source.h"
#include "qt/coinselectionmodel.h"
#include "qt/coinselectionview.h"

#include <QScrollBar>

#include <memory>
#include <string>
#include <vector>

namespace {

//! Matches CoinSelectionModelTests: enough directory rows to move a group from
//! the middle to the top, small enough to stay instant.
constexpr int kCoins = 200;
constexpr int kGroups = 20;
constexpr int kMidRow = 5;

//! Deliberately shorter than the directory so the vertical scrollbar has a
//! range; reslotHoldsTheScrollOffset asserts that it does before relying on it.
constexpr int kViewWidth = 400;
constexpr int kViewHeight = 160;

//! The restore is deferred through a singleShot(0) continuation, so every
//! assertion about it has to let the event loop turn first.
void settle()
{
    QTest::qWait(50);
}

struct Fixture {
    std::shared_ptr<interfaces::WalletCoinSource> source{
        interfaces::MakeSyntheticCoinSource(kCoins, kGroups)};
    interfaces::WalletCoinControl coin_control;
    CoinSelectionModel model{*source, &coin_control, GRC::VIEW_COIN_CONTROL};
    CoinSelectionView view;

    Fixture()
    {
        // Through CoinSelectionView::setModel, which is what wires the
        // groupsReslotted connection under test.
        view.setModel(&model);
        view.resize(kViewWidth, kViewHeight);
        view.show();
        settle();
    }

    //! The address occupying the given row of the directory.
    std::string addressAtRow(int row)
    {
        return model.addressAt(model.index(row, 0, QModelIndex())).toStdString();
    }
};

//! A re-slot as CoinViews publishes it: the row leaves "from" and comes back at
//! "to", carrying its identity in the inserted group address.
std::vector<GRC::WalletCoinEvent> reslotBatch(const std::string& address, int from, int to)
{
    GRC::CoinGroupInfo moved;
    moved.address = address;
    moved.total_amount = 1000000;
    moved.output_count = 10;

    std::vector<GRC::WalletCoinEvent> batch;
    batch.push_back({/*seqno=*/1, /*emit_time_us=*/0,
                     GRC::CoinGroupsRemovedPayload{GRC::VIEW_COIN_CONTROL, /*epoch=*/0, from,
                                                   /*count=*/1}});
    batch.push_back({/*seqno=*/2, /*emit_time_us=*/0,
                     GRC::CoinGroupsInsertedPayload{GRC::VIEW_COIN_CONTROL, /*epoch=*/0, to,
                                                    {moved}}});
    return batch;
}

} // namespace

void CoinSelectionViewTests::reslotReExpandsAndRealizesTheBranch()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.addressAtRow(kMidRow);
    QVERIFY(!address.empty());

    // Expand through the view, which is what realizes the children.
    f.view.expand(parent);
    QVERIFY(f.view.isExpanded(parent));
    QVERIFY(f.model.rowCount(parent) > 0);

    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    settle();

    const QModelIndex moved = f.model.index(0, 0, QModelIndex());
    QCOMPARE(f.model.addressAt(moved).toStdString(), address);
    QVERIFY2(f.view.isExpanded(moved),
             "the re-slotted branch came back collapsed -- nobody collapsed it");
    QVERIFY2(f.model.rowCount(moved) > 0,
             "the branch is marked expanded but its children were never realized: "
             "the expand ran while the model could not fetch, and nothing retries it");
}

void CoinSelectionViewTests::reslotOfACollapsedBranchStaysCollapsed()
{
    Fixture f;
    const std::string address = f.addressAtRow(kMidRow);

    // Never expanded: a re-slot must not expand it on the user's behalf.
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    settle();

    const QModelIndex moved = f.model.index(0, 0, QModelIndex());
    QCOMPARE(f.model.addressAt(moved).toStdString(), address);
    QVERIFY(!f.view.isExpanded(moved));
    QCOMPARE(f.model.rowCount(moved), 0);
}

void CoinSelectionViewTests::userCollapseIsNotUndoneByAReslot()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.addressAtRow(kMidRow);

    f.view.expand(parent);
    QVERIFY(f.model.rowCount(parent) > 0);

    // The view un-realizes on a deferred turn, so let the collapse land before
    // the re-slot arrives -- otherwise this would pass for the wrong reason.
    f.view.collapse(parent);
    settle();
    QVERIFY(!f.view.isExpanded(parent));

    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    settle();

    const QModelIndex moved = f.model.index(0, 0, QModelIndex());
    QCOMPARE(f.model.addressAt(moved).toStdString(), address);
    QVERIFY2(!f.view.isExpanded(moved),
             "a re-slot resurrected an expansion the user had closed");
}

void CoinSelectionViewTests::reslotHoldsTheScrollOffset()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.addressAtRow(kMidRow);

    f.view.expand(parent);
    settle();

    QScrollBar* const bar = f.view.verticalScrollBar();
    QVERIFY2(bar->maximum() > 0,
             "the viewport is not scrollable, so this test could not observe a jump");

    // Scroll to the BOTTOM, not to the middle. The re-slot removes the group
    // row and the realized children under it; from mid-range the scrollbar
    // absorbs that without clamping and the offset survives on its own, so the
    // assertion below would hold with or without the restore. At the maximum
    // the shrunken range has to clamp the value, which is what makes the
    // restore observable -- verified by mutation: with the wiring severed this
    // fails, from the middle it did not.
    bar->setValue(bar->maximum());
    const int before = bar->value();
    QVERIFY2(before > 0, "scrolled to the top, where a jump to the top is invisible");

    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    settle();

    QCOMPARE(f.view.verticalScrollBar()->value(), before);
}

void CoinSelectionViewTests::swappingTheModelDropsTheOldConnection()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.addressAtRow(kMidRow);
    f.view.expand(parent);
    QVERIFY(f.view.isExpanded(parent));

    auto second_source = interfaces::MakeSyntheticCoinSource(kCoins, kGroups);
    interfaces::WalletCoinControl second_control;
    CoinSelectionModel second{*second_source, &second_control, GRC::VIEW_COIN_CONTROL};

    f.view.setModel(&second);

    // The old model is still alive and still emits. setModel must have dropped
    // the connection: reaching restoreReslottedBranches now would resolve ids
    // against the NEW model and expand an unrelated row.
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    settle();

    QCOMPARE(f.view.model(), static_cast<QAbstractItemModel*>(&second));
    for (int row = 0; row < second.rowCount(QModelIndex()); ++row) {
        QVERIFY2(!f.view.isExpanded(second.index(row, 0, QModelIndex())),
                 "a stale connection expanded a row in the replacement model");
    }

    // Detach before the locals above go out of scope.
    f.view.setModel(nullptr);
}
