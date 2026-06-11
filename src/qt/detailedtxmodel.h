// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DETAILEDTXMODEL_H
#define BITCOIN_QT_DETAILEDTXMODEL_H

#include "qt/transactionrecord.h"
#include "qt/txfilter.h"
#include "qt/wallet_event_queue.h"
#include "uint256.h"

#include <QAbstractTableModel>

#include <vector>

class WalletModel;
class TransactionTableModel;

//!
//! \brief Windowed-model consumer for the detailed transaction-history view (PR4).
//!
//! Replaces the client-side TransactionFilterProxy: it registers a VIEW_DETAILED
//! cursor in the producer GRC::WalletTxStore (server-side filter + sort, maintained
//! off cs_main/cs_wallet on the store-worker), holds the cursor's served rows, and
//! renders through TransactionTableModel::formatRole — so the detailed table reads
//! identical role values with no formatter duplication. It is the TransactionView's
//! direct model (no proxy layer): a header click drives sort() → the producer's
//! setViewSort; the filter UI drives setFilter() → setViewFilter.
//!
//! PR4 uses an UNLIMITED cursor cap, so this holds the full filtered+sorted replica
//! (servedCount == totalAccepted). The viewport-slice windowing — where it would
//! hold only the visible rows — is deferred to PR5; this model's count/limit
//! mechanics are the same ones OverviewPage proved in PR3.
//!
class DetailedTxModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit DetailedTxModel(WalletModel* walletModel, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    //! Header-click sort: forwards to the producer cursor's setViewSort; the
    //! resulting RowsReset refills this model (QTableView manages the indicator).
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    //! Push a new filter spec to the producer cursor (the filter UI calls this).
    void setFilter(const GRC::FilterSpec& spec);

    //! Model index of the first served row whose tx hash is \p hash, or an invalid
    //! index — maps a click-through (e.g. from OverviewPage) to a row here.
    QModelIndex indexForTxid(const uint256& hash) const;

private slots:
    //! Apply a drained wallet-event batch; ignores everything not VIEW_DETAILED.
    void applyEventBatch(const std::vector<GRC::WalletEvent>& events);

private:
    WalletModel* m_walletModel;
    TransactionTableModel* m_ttm;            //!< formatter source (formatRole/headerData)
    std::vector<TransactionRecord> m_rows;   //!< served-window replica (full set in PR4)
    //! Highest store seqno already reflected in m_rows (PR4-fix B). A getRows
    //! refetch (initial seed or a Reset) reads the store's high-water; any later
    //! event whose seqno is <= this is already in m_rows and must be skipped, so a
    //! worker insert/remove that landed between a Reset emission and the refetch is
    //! never double-applied.
    uint64_t m_applied_seqno = 0;
};

#endif // BITCOIN_QT_DETAILEDTXMODEL_H
