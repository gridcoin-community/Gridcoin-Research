// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "coinselectionmodeltests.h"

#include "interfaces/wallet.h"
#include "interfaces/wallet_coin_channel.h"
#include "interfaces/wallet_coin_source.h"
#include "qt/coinselectionmodel.h"

#include <memory>
#include <string>
#include <vector>

namespace {

//! 200 coins over 20 distinct groups: enough directory rows for a group to be
//! moved from the middle to the top, small enough to stay instant.
constexpr int kCoins = 200;
constexpr int kGroups = 20;
constexpr int kMidRow = 5;

//! The model under test plus everything it borrows. The synthetic source seeds
//! synchronously in its constructor, so the directory is populated by the time
//! the model's test-seam constructor reseeds from it.
struct Fixture {
    std::shared_ptr<interfaces::WalletCoinSource> source{
        interfaces::MakeSyntheticCoinSource(kCoins, kGroups)};
    interfaces::WalletCoinControl coin_control;
    CoinSelectionModel model{*source, &coin_control, GRC::VIEW_COIN_CONTROL};
};

//! A re-slot as CoinViews publishes it: the row leaves \p from and comes back
//! at \p to, carrying its identity in the inserted group's address.
std::vector<GRC::WalletCoinEvent> reslotBatch(const std::string& address, int from, int to)
{
    GRC::CoinGroupInfo moved;
    moved.address = address;
    moved.total_amount = 1'000'000;
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

std::vector<GRC::WalletCoinEvent> removalBatch(int position)
{
    std::vector<GRC::WalletCoinEvent> batch;
    batch.push_back({/*seqno=*/1, /*emit_time_us=*/0,
                     GRC::CoinGroupsRemovedPayload{GRC::VIEW_COIN_CONTROL, /*epoch=*/0, position,
                                                   /*count=*/1}});
    return batch;
}

} // namespace

void CoinSelectionModelTests::reslotOfAnExpandedGroupIsReported()
{
    Fixture f;
    QCOMPARE(f.model.rowCount(QModelIndex()), kGroups);

    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.model.addressAt(parent).toStdString();
    QVERIFY(!address.empty());

    // Expand it, the way the view does.
    QVERIFY(f.model.canFetchMore(parent));
    f.model.fetchMore(parent);
    QVERIFY(f.model.rowCount(parent) > 0);

    QList<int> reported;
    QObject::connect(&f.model, &CoinSelectionModel::groupsReslotted,
                     [&reported](const QList<int>& ids) { reported = ids; });

    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));

    // The branch came back collapsed (the removal took the realized cache),
    // and the model reported exactly it for restoration.
    QCOMPARE(reported.size(), 1);
    QCOMPARE(f.model.addressAt(f.model.groupIndexForId(reported.first())).toStdString(), address);
}

void CoinSelectionModelTests::reslotOfACollapsedGroupIsNotReported()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.model.addressAt(parent).toStdString();

    bool emitted = false;
    QObject::connect(&f.model, &CoinSelectionModel::groupsReslotted,
                     [&emitted](const QList<int>&) { emitted = true; });

    // Never expanded, so there is nothing to restore.
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    QVERIFY(!emitted);
}

void CoinSelectionModelTests::realRemovalIsNotReported()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    QVERIFY(f.model.canFetchMore(parent));
    f.model.fetchMore(parent);

    bool emitted = false;
    QObject::connect(&f.model, &CoinSelectionModel::groupsReslotted,
                     [&emitted](const QList<int>&) { emitted = true; });

    // A group that emptied out is removed and does NOT come back in the batch:
    // nothing to re-expand, and no phantom index handed to the view.
    f.model.applyCoinEventBatch(removalBatch(kMidRow));
    QVERIFY(!emitted);
    QCOMPARE(f.model.rowCount(QModelIndex()), kGroups - 1);
}

void CoinSelectionModelTests::userCollapseForgetsTheExpansion()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.model.addressAt(parent).toStdString();

    QVERIFY(f.model.canFetchMore(parent));
    f.model.fetchMore(parent);
    // The user collapses it (the view's deferred un-realize).
    f.model.releaseGroup(parent);

    bool emitted = false;
    QObject::connect(&f.model, &CoinSelectionModel::groupsReslotted,
                     [&emitted](const QList<int>&) { emitted = true; });

    // A later re-slot must NOT resurrect an expansion the user closed.
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    QVERIFY(!emitted);
}

void CoinSelectionModelTests::reportedIdResolvesToTheMovedRow()
{
    Fixture f;
    const QModelIndex parent = f.model.index(kMidRow, 0, QModelIndex());
    const std::string address = f.model.addressAt(parent).toStdString();
    QVERIFY(f.model.canFetchMore(parent));
    f.model.fetchMore(parent);

    QList<int> reported;
    QObject::connect(&f.model, &CoinSelectionModel::groupsReslotted,
                     [&reported](const QList<int>& ids) { reported = ids; });

    // Move it to the head of the directory. The id is what the view resolves
    // against, precisely because the row number moved.
    f.model.applyCoinEventBatch(reslotBatch(address, kMidRow, 0));
    QCOMPARE(reported.size(), 1);

    const QModelIndex moved = f.model.groupIndexForId(reported.first());
    QVERIFY(moved.isValid());
    QCOMPARE(moved.row(), 0);
    QCOMPARE(f.model.rowCount(QModelIndex()), kGroups);
}
