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

void CoinSelectionViewTests::userCollapseSurvivesAReslotInTheSameTurn()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.addressAtRow(kMidRow);

    f.view.expand(parent);
    QVERIFY(f.model.rowCount(parent) > 0);

    // The ORDERING userCollapseIsNotUndoneByAReslot deliberately avoids: no
    // settle() between the collapse and the re-slot, so the deferred
    // un-realize has not run yet. The removal half invalidates the persistent
    // index the continuation is holding, so it returns early -- and if the
    // collapse were recorded only there, the insert half would still see the
    // address expanded and queue a re-expansion.
    f.view.collapse(parent);
    QVERIFY(!f.view.isExpanded(parent));
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    settle();

    const QModelIndex moved = f.model.index(0, 0, QModelIndex());
    QCOMPARE(f.model.addressAt(moved).toStdString(), address);
    QVERIFY2(!f.view.isExpanded(moved),
             "a re-slot arriving on the collapse turn resurrected the branch: "
             "the collapse was recorded only by the deferred releaseGroup, "
             "which the removal had already made unreachable");
    QCOMPARE(f.model.rowCount(moved), 0);
}


void CoinSelectionViewTests::reExpandBeforeTheDeferredReleaseKeepsTheBranch()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.addressAtRow(kMidRow);

    f.view.expand(parent);
    QVERIFY(f.model.rowCount(parent) > 0);

    // Collapse and re-expand in the SAME turn, so the deferred un-realize
    // fires after the re-expand. The slot is still warm, so the re-expand
    // never reaches fetchMore(): only the expanded() signal sees it. Without
    // the fire-time isExpanded() re-validation the continuation rips the
    // children out of a branch the view is showing as open.
    f.view.collapse(parent);
    f.view.expand(parent);
    QVERIFY(f.view.isExpanded(parent));
    settle();

    QVERIFY(f.view.isExpanded(parent));
    QVERIFY2(f.model.rowCount(parent) > 0,
             "the deferred release un-realized a branch the user had re-opened");

    // And the branch is still owned by the restore machinery afterwards: the
    // same-turn collapse/re-expand must not have desynchronized m_expanded.
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    settle();
    const QModelIndex moved = f.model.index(0, 0, QModelIndex());
    QCOMPARE(f.model.addressAt(moved).toStdString(), address);
    QVERIFY2(f.view.isExpanded(moved),
             "the same-turn collapse/re-expand left the branch outside "
             "m_expanded: the re-slot restore no longer fires for it");
    QVERIFY(f.model.rowCount(moved) > 0);
}

void CoinSelectionViewTests::resetBetweenEmissionAndContinuationDoesNotReExpand()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.addressAtRow(kMidRow);

    f.view.expand(parent);
    QVERIFY(f.model.rowCount(parent) > 0);

    // Queue the restore, then reset before it runs. The Reset has to be its
    // OWN batch: in the same batch, reseedFromSource clears m_pending_reexpand
    // before the end-of-batch emission, groupsReslotted never fires, and this
    // would pass without testing anything.
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));

    std::vector<GRC::WalletCoinEvent> reset;
    reset.push_back({/*seqno=*/3, /*emit_time_us=*/0,
                     GRC::CoinResetPayload{GRC::VIEW_COIN_CONTROL, /*epoch=*/0}});
    f.model.applyCoinEventBatch(reset);
    settle();

    // A Reset collapses every branch and clears m_expanded, so the user is
    // looking at a fully collapsed tree. The queued continuation must not
    // re-open one behind them: it holds ids issued by a registry that has
    // since been renumbered, and nothing about a queued singleShot is
    // cancelled by the reseed.
    for (int row = 0; row < f.model.rowCount(QModelIndex()); ++row) {
        const QModelIndex idx = f.model.index(row, 0, QModelIndex());
        QVERIFY2(!f.view.isExpanded(idx),
                 "a continuation queued before the Reset re-expanded a branch "
                 "the Reset had closed");
        QCOMPARE(f.model.rowCount(idx), 0);
    }
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

void CoinSelectionViewTests::modelSwapBeforeTheDeferredReleaseIsIgnored()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    f.view.expand(parent);
    QVERIFY(f.model.rowCount(parent) > 0);

    auto second_source = interfaces::MakeSyntheticCoinSource(kCoins, kGroups);
    interfaces::WalletCoinControl second_control;
    CoinSelectionModel second{*second_source, &second_control, GRC::VIEW_COIN_CONTROL};

    // Collapse and swap the model in the SAME turn, so the deferred release
    // fires against the replacement. The pinned-model guard must drop the
    // continuation: pidx belongs to the OLD model, and releaseGroup() on the
    // new one would resolve its row number against an unrelated directory.
    f.view.collapse(parent);
    f.view.setModel(&second);
    settle();

    // The old model keeps its realized (though collapsed) slot -- the
    // continuation must not have run against either model.
    QVERIFY2(f.model.rowCount(parent) > 0,
             "the deferred release fired across a model swap");
    for (int row = 0; row < second.rowCount(QModelIndex()); ++row) {
        QCOMPARE(second.rowCount(second.index(row, 0, QModelIndex())), 0);
    }

    // Detach before the locals above go out of scope.
    f.view.setModel(nullptr);
}
