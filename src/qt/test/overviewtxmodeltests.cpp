// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/test/overviewtxmodeltests.h"
#include "interfaces/wallet_coin_source.h"
#include "interfaces/wallet_tx_channel.h"
#include "interfaces/wallet_tx_record.h"
#include "qt/optionsmodel.h"
#include "qt/overviewtxmodel.h"
#include "qt/test/interfacefakes.h"
#include "qt/walletmodel.h"

#include <QTest>

#include <vector>

namespace {
//! A transaction source whose first drain delivers one batch: the rows the
//! overview's cursor gains after its limit is raised, as an insert. The seed
//! read at construction is empty, as it is when the overview registers its
//! cursor before its first layout.
class OneBatchTxSource : public qt_test::FakeWalletTxSource
{
public:
    std::vector<GRC::WalletEvent> m_batch;

    std::vector<GRC::WalletEvent> drainEvents(std::size_t) override
    {
        std::vector<GRC::WalletEvent> batch;
        batch.swap(m_batch);
        return batch;
    }
};
} // anonymous namespace

void OverviewTxModelTests::transaction_updated_follows_the_applied_rows()
{
    // The overview refreshes its list (and shows or hides it by its row count)
    // on WalletModel::transactionUpdated. The rows a drain delivers must already
    // be in the overview's model when that signal fires; otherwise the list is
    // judged empty and stays hidden until something else refreshes it.
    qt_test::FakeNode node;
    qt_test::FakeSideStakeManager sidestakes;
    qt_test::FakeWallet wallet;
    OneBatchTxSource tx_source;
    auto coin_source = interfaces::MakeSyntheticCoinSource(0, 0);
    OptionsModel options(node, sidestakes);
    WalletModel wallet_model(wallet, tx_source, *coin_source, &options);

    OverviewTxModel overview(&wallet_model, 10);
    QCOMPARE(overview.rowCount(), 0);

    GRC::RowsInsertedPayload insert;
    insert.position = 0;
    insert.records = {TransactionRecord(uint256S("01"), 1000), TransactionRecord(uint256S("02"), 1001)};
    insert.viewId = GRC::VIEW_OVERVIEW;
    tx_source.m_batch.push_back(GRC::WalletEvent{1, 0, insert});

    std::vector<int> rows_seen;
    QObject::connect(&wallet_model, &WalletModel::transactionUpdated,
                     [&] { rows_seen.push_back(overview.rowCount()); });

    // The drain timer delivers the batch.
    QTRY_VERIFY_WITH_TIMEOUT(!rows_seen.empty(), 5000);

    QCOMPARE(rows_seen.front(), 2);
    QCOMPARE(overview.rowCount(), 2);
}
