// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/overviewtxmodel.h"

#include "qt/txfilter.h"
#include "qt/transactiontablemodel.h"
#include "qt/walletmodel.h"
#include "qt/wallettxstore.h"

#include <algorithm>
#include <type_traits>
#include <variant>

OverviewTxModel::OverviewTxModel(WalletModel* walletModel, int initialLimit, QObject* parent)
    : QAbstractListModel(parent)
    , m_walletModel(walletModel)
    , m_ttm(walletModel->getTransactionTableModel())
    , m_limit(initialLimit)
{
    // Register the server-side view: recent transactions sorted by Status DESC,
    // inactive (Conflicted/NotAccepted) masked, capped at the resize-derived
    // limit — matching the old TransactionFilterProxy configuration in
    // OverviewPage::setWalletModel.
    GRC::FilterSpec spec;
    spec.show_inactive = false;
    spec.limit_rows = m_limit;
    GRC::WalletTxStore& store = m_walletModel->getTxStore();
    store.registerView(GRC::VIEW_OVERVIEW, spec, GRC::TXCOL_STATUS, GRC::TXSORT_DESC);

    // registerView pushed a RowsReset that the next drain will deliver, but also
    // seed synchronously so the list is populated before the first drain tick.
    // Capture the high-water (PR4-fix B): the registration Reset arriving on the
    // first drain carries this seqno and is skipped as already reflected. count = -1
    // ("all served") reads rows + high-water (RowsResult) in one cs_store hold —
    // atomic, so a concurrent worker insert can't drop a row the skip would suppress.
    GRC::RowsResult seed = store.getRows(GRC::VIEW_OVERVIEW, 0, -1);
    m_rows = std::move(seed.records);
    m_applied_seqno = seed.high_water;

    connect(m_walletModel, &WalletModel::walletEventsDrained,
            this, &OverviewTxModel::applyEventBatch);
}

int OverviewTxModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant OverviewTxModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0
            || static_cast<std::size_t>(index.row()) >= m_rows.size()) {
        return QVariant();
    }
    // Render the ToAddress column's roles — the single visual column the recent-tx
    // delegate composes (its DecorationRole is the transaction-TYPE icon
    // txAddressDecoration, and its DisplayRole is the address), matching the old
    // proxy list which set modelColumn = ToAddress. Reuse the TransactionTableModel
    // formatters. const_cast: formatRole takes a non-const record (legacy
    // getTxID()/describe() are non-const) but does not mutate it.
    return m_ttm->formatRole(const_cast<TransactionRecord*>(&m_rows[index.row()]),
                             TransactionTableModel::ToAddress, role);
}

void OverviewTxModel::setLimit(int limit)
{
    if (limit == m_limit) {
        return;
    }
    m_limit = limit;
    // The cursor recomputes its served window and emits the boundary Insert/Remove
    // events, which arrive on the next drain.
    m_walletModel->getTxStore().setViewLimit(GRC::VIEW_OVERVIEW, limit);
}

uint256 OverviewTxModel::txidAt(int row) const
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_rows.size()) {
        return uint256();
    }
    return m_rows[row].hash;
}

void OverviewTxModel::applyEventBatch(const std::vector<GRC::WalletEvent>& events)
{
    GRC::WalletTxStore& store = m_walletModel->getTxStore();
    for (const GRC::WalletEvent& ev : events) {
        const uint64_t seqno = ev.seqno;
        std::visit([&](auto&& payload) {
            using P = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<P, GRC::RowsResetPayload>) {
                if (payload.viewId != GRC::VIEW_OVERVIEW) return;
                if (seqno <= m_applied_seqno) return;   // already reflected (PR4-fix B)
                // Refetch the live served window + the store's high-water in one
                // cs_store hold (getRows clamps the count to the served cap), so any
                // worker delta that landed after this Reset was emitted is captured
                // here exactly once and skipped when its own event arrives.
                beginResetModel();
                // count = -1: rows + high-water in ONE cs_store hold (RowsResult,
                // atomic — getRows clamps to the live served window, capped; a
                // separately-locked count could under-fetch a row the high-water
                // already covered, dropping it permanently below cap). (PR4-fix B)
                GRC::RowsResult r = store.getRows(GRC::VIEW_OVERVIEW, 0, -1);
                m_rows = std::move(r.records);
                endResetModel();
                m_applied_seqno = std::max(r.high_water, seqno);
            } else if constexpr (std::is_same_v<P, GRC::RowsInsertedPayload>) {
                if (payload.viewId != GRC::VIEW_OVERVIEW) return;
                if (seqno <= m_applied_seqno) return;   // already reflected in a refetch
                if (payload.records.empty()) return;  // empty insert → invalid beginInsertRows range
                const int pos = payload.position;
                if (pos < 0 || static_cast<std::size_t>(pos) > m_rows.size()) return;
                beginInsertRows(QModelIndex(), pos,
                                pos + static_cast<int>(payload.records.size()) - 1);
                m_rows.insert(m_rows.begin() + pos,
                              payload.records.begin(), payload.records.end());
                endInsertRows();
                m_applied_seqno = seqno;
            } else if constexpr (std::is_same_v<P, GRC::RowsRemovedPayload>) {
                if (payload.viewId != GRC::VIEW_OVERVIEW) return;
                if (seqno <= m_applied_seqno) return;
                const int pos = payload.position;
                if (pos < 0 || payload.count <= 0
                        || static_cast<std::size_t>(pos) + static_cast<std::size_t>(payload.count)
                               > m_rows.size()) return;
                beginRemoveRows(QModelIndex(), pos, pos + payload.count - 1);
                m_rows.erase(m_rows.begin() + pos, m_rows.begin() + pos + payload.count);
                endRemoveRows();
                m_applied_seqno = seqno;
            } else if constexpr (std::is_same_v<P, GRC::RowsChangedPayload>) {
                if (payload.viewId != GRC::VIEW_OVERVIEW) return;
                if (seqno <= m_applied_seqno) return;
                // The payload carries no records (a Change does not move the row),
                // so re-fetch the changed slice from the cursor and refresh.
                const std::vector<TransactionRecord> fresh =
                    store.getRows(GRC::VIEW_OVERVIEW, payload.first, payload.count).records;
                for (std::size_t i = 0; i < fresh.size()
                        && static_cast<std::size_t>(payload.first) + i < m_rows.size(); ++i) {
                    m_rows[static_cast<std::size_t>(payload.first) + i] = fresh[i];
                }
                if (!fresh.empty()) {
                    emit dataChanged(index(payload.first, 0),
                                     index(payload.first + static_cast<int>(fresh.size()) - 1, 0));
                }
                m_applied_seqno = seqno;
            }
            // VIEW_FULL Rows* events, RowCountChanged, and ChainTipChanged are
            // not consumed here (RowCountChanged is for the scrolled detailed
            // view; the Overview list shows only its served window).
        }, ev.payload);
    }
}
