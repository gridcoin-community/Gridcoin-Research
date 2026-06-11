// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/detailedtxmodel.h"

#include "qt/transactiontablemodel.h"
#include "qt/walletmodel.h"
#include "qt/wallettxstore.h"
#include "util/system.h"

#include <algorithm>
#include <type_traits>
#include <variant>

// Guard the standalone (Qt-free) mirrors in txfilter.h against the authoritative
// Qt-side enums so they can never silently drift. Relocated here from the retired
// transactionfilterproxy.cpp — this is now the primary detailed-view consumer.
static_assert(GRC::TXSTATUS_CONFLICTED  == TransactionStatus::Conflicted,
              "TXSTATUS_CONFLICTED mirror out of sync with TransactionStatus::Conflicted");
static_assert(GRC::TXSTATUS_NOTACCEPTED == TransactionStatus::NotAccepted,
              "TXSTATUS_NOTACCEPTED mirror out of sync with TransactionStatus::NotAccepted");
static_assert(static_cast<int>(GRC::TXCOL_STATUS)  == static_cast<int>(TransactionTableModel::Status),    "TXCOL_STATUS mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_DATE)    == static_cast<int>(TransactionTableModel::Date),      "TXCOL_DATE mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_TYPE)    == static_cast<int>(TransactionTableModel::Type),      "TXCOL_TYPE mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_ADDRESS) == static_cast<int>(TransactionTableModel::ToAddress), "TXCOL_ADDRESS mirror drift");
static_assert(static_cast<int>(GRC::TXCOL_AMOUNT)  == static_cast<int>(TransactionTableModel::Amount),    "TXCOL_AMOUNT mirror drift");

DetailedTxModel::DetailedTxModel(WalletModel* walletModel, QObject* parent)
    : QAbstractTableModel(parent)
    , m_walletModel(walletModel)
    , m_ttm(walletModel->getTransactionTableModel())
{
    // Register the server-side view: the full history sorted by Date DESC, the
    // TransactionView's default ordering. The default FilterSpec is the
    // "show everything" initial state of the unfiltered detailed view (all dates,
    // ALL_TYPES, no address filter, min_amount 0, show_inactive true). The one
    // runtime input is -showorphans, read once here — exactly mirroring the old
    // TransactionFilterProxy ctor (a launch-time arg with no runtime mutation
    // path, so reading it once is behavior-identical). limit_rows defaults to
    // unlimited, so the served window is the full filtered+sorted set (PR4); the
    // viewport-slice cap is PR5.
    GRC::FilterSpec spec;
    spec.show_orphans = gArgs.GetBoolArg("-showorphans", false);
    GRC::WalletTxStore& store = m_walletModel->getTxStore();
    store.registerView(GRC::VIEW_DETAILED, spec, GRC::TXCOL_DATE, GRC::TXSORT_DESC);

    // registerView pushed a RowsReset that the next drain will deliver, but also
    // seed synchronously so the table is populated before the first drain tick.
    // Capture the high-water (PR4-fix B): the registration Reset that arrives on
    // the first drain carries this same seqno and is skipped as already reflected.
    // count = -1 ("all served") reads the rows, the served count AND the high-water
    // in one cs_store hold — atomic, so a concurrent worker insert can't drop a row
    // that the seqno skip would then suppress.
    m_rows = store.getRows(GRC::VIEW_DETAILED, 0, -1, &m_applied_seqno);

    connect(m_walletModel, &WalletModel::walletEventsDrained,
            this, &DetailedTxModel::applyEventBatch);
}

int DetailedTxModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int DetailedTxModel::columnCount(const QModelIndex& parent) const
{
    // Forward to the formatter model so the column set stays single-sourced.
    return parent.isValid() ? 0 : m_ttm->columnCount(QModelIndex());
}

QVariant DetailedTxModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0
            || static_cast<std::size_t>(index.row()) >= m_rows.size()) {
        return QVariant();
    }
    // Reuse the TransactionTableModel formatters for whichever column the table
    // asks for. const_cast: formatRole takes a non-const record (legacy
    // getTxID()/describe() are non-const) but does not mutate it.
    return m_ttm->formatRole(const_cast<TransactionRecord*>(&m_rows[index.row()]),
                             index.column(), role);
}

QVariant DetailedTxModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return m_ttm->headerData(section, orientation, role);
}

void DetailedTxModel::sort(int column, Qt::SortOrder order)
{
    // Forward to the producer cursor; the resulting RowsReset refills this model
    // on the next drain. QTableView owns the header sort-indicator display. Kick
    // an immediate drain so the reorder lands now, not up to 500ms later (PR4-D).
    m_walletModel->getTxStore().setViewSort(
        GRC::VIEW_DETAILED, column,
        order == Qt::AscendingOrder ? GRC::TXSORT_ASC : GRC::TXSORT_DESC);
    m_walletModel->requestEventDrainSoon();
}

void DetailedTxModel::setFilter(const GRC::FilterSpec& spec)
{
    m_walletModel->getTxStore().setViewFilter(GRC::VIEW_DETAILED, spec);
    m_walletModel->requestEventDrainSoon();   // reflect the filter immediately (PR4-D)
}

QModelIndex DetailedTxModel::indexForTxid(const uint256& hash) const
{
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].hash == hash) {
            return index(static_cast<int>(i), 0);
        }
    }
    return QModelIndex();
}

void DetailedTxModel::applyEventBatch(const std::vector<GRC::WalletEvent>& events)
{
    GRC::WalletTxStore& store = m_walletModel->getTxStore();
    const int lastColumn = columnCount(QModelIndex()) - 1;
    for (const GRC::WalletEvent& ev : events) {
        const uint64_t seqno = ev.seqno;
        std::visit([&](auto&& payload) {
            using P = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<P, GRC::RowsResetPayload>) {
                if (payload.viewId != GRC::VIEW_DETAILED) return;
                if (seqno <= m_applied_seqno) return;   // already reflected (PR4-fix B)
                // Refetch the FULL current served set and the store's high-water in
                // one cs_store hold. Using the live totalAccepted (not the possibly
                // stale payload.total) plus the high-water means any worker insert/
                // remove that landed after this Reset was emitted is captured here
                // exactly once and will be skipped when its own event arrives.
                beginResetModel();
                uint64_t hw = m_applied_seqno;
                // count = -1: rows + served count + high-water in ONE cs_store hold
                // (atomic — a separate totalAccepted() could under-fetch a row the
                // high-water already covered, dropping it permanently). (PR4-fix B)
                m_rows = store.getRows(GRC::VIEW_DETAILED, 0, -1, &hw);
                endResetModel();
                m_applied_seqno = std::max(hw, seqno);
            } else if constexpr (std::is_same_v<P, GRC::RowsInsertedPayload>) {
                if (payload.viewId != GRC::VIEW_DETAILED) return;
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
                if (payload.viewId != GRC::VIEW_DETAILED) return;
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
                if (payload.viewId != GRC::VIEW_DETAILED) return;
                if (seqno <= m_applied_seqno) return;
                // The payload carries no records (a Change does not move the row),
                // so re-fetch the changed slice from the cursor and refresh. A
                // row spans every column, so the dataChanged span is full-width.
                const std::vector<TransactionRecord> fresh =
                    store.getRows(GRC::VIEW_DETAILED, payload.first, payload.count);
                for (std::size_t i = 0; i < fresh.size()
                        && static_cast<std::size_t>(payload.first) + i < m_rows.size(); ++i) {
                    m_rows[static_cast<std::size_t>(payload.first) + i] = fresh[i];
                }
                if (!fresh.empty()) {
                    emit dataChanged(index(payload.first, 0),
                                     index(payload.first + static_cast<int>(fresh.size()) - 1,
                                           lastColumn));
                }
                m_applied_seqno = seqno;
            }
            // VIEW_FULL/VIEW_OVERVIEW Rows* events and RowCountChanged are not
            // consumed here. RowCountChanged signals a served-count change under a
            // finite cap; PR4 holds the full set (unlimited cap) so it never fires.
            // PR5's windowed cap will consume it to drive the scrollbar extent.
        }, ev.payload);
    }
}
