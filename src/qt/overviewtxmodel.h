// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OVERVIEWTXMODEL_H
#define BITCOIN_QT_OVERVIEWTXMODEL_H

#include "qt/transactionrecord.h"
#include "qt/wallet_event_queue.h"
#include "uint256.h"

#include <QAbstractListModel>

#include <vector>

class WalletModel;
class TransactionTableModel;

//!
//! \brief Mini windowed-model consumer for OverviewPage's recent-transactions list.
//!
//! The windowed-model PR3 testbed. Instead of a client-side QSortFilterProxyModel
//! over the full TransactionTableModel replica, OverviewPage drives a VIEW_OVERVIEW
//! cursor in the producer GRC::WalletTxStore (server-side filter+sort, maintained
//! off cs_main/cs_wallet on the store-worker). This model holds ONLY the cursor's
//! served-window slice of records — refilled on RowsReset, patched on
//! RowsInserted/Removed/Changed — so the count/limit/resize mechanics are proven
//! here before the detailed transaction table migrates to the same path (PR5).
//!
//! It is a single-column list whose column renders the ToAddress column's roles
//! (the recent-tx delegate composes several of them — the type icon, address,
//! amount, date — exactly as the old proxy did with modelColumn = ToAddress).
//! Role values are produced by TransactionTableModel::formatRole, so the
//! formatters live in one place and the delegate reads identical data.
//!
class OverviewTxModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit OverviewTxModel(WalletModel* walletModel, int initialLimit, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    //! Set the served-row cap (OverviewPage's stairstep resize); forwards to the
    //! producer cursor. The resulting Insert/Remove events arrive via the drain.
    void setLimit(int limit);
    int limit() const { return m_limit; }

    //! The tx hash at served row \p row (for click-through to the detailed view),
    //! or a null hash if out of range.
    uint256 txidAt(int row) const;

private slots:
    //! Apply a drained batch of wallet events; ignores everything not VIEW_OVERVIEW.
    void applyEventBatch(const std::vector<GRC::WalletEvent>& events);

private:
    WalletModel* m_walletModel;
    TransactionTableModel* m_ttm;            //!< formatter source (formatRole)
    int m_limit;
    std::vector<TransactionRecord> m_rows;   //!< served-window replica (VIEW_OVERVIEW)
    //! Highest store seqno already reflected in m_rows (PR4-fix B): an event whose
    //! seqno is <= this is already in a getRows refetch and is skipped, so a worker
    //! delta landing between a Reset emission and the refetch is not double-applied.
    uint64_t m_applied_seqno = 0;
};

#endif // BITCOIN_QT_OVERVIEWTXMODEL_H
