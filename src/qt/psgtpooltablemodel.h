// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PSGTPOOLTABLEMODEL_H
#define BITCOIN_QT_PSGTPOOLTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>

#include <string>

class WalletModel;

namespace interfaces {
class PSGTPoolContext;
struct PSGTPoolRow;
} // namespace interfaces

//!
//! \brief Table model of the local PSGT pool (#2910) for the PSGT Pool page.
//!
//! Snapshots g_psgt_pool.GetAll() on the GUI thread (the pool takes only its
//! own leaf lock, never cs_main, so this is cheap and deadlock-free) and
//! refreshes when the core PSGTPoolChanged signal fires. Keeps a bounded
//! "recently completed" history so an entry that leaves the pool stays visible
//! briefly before disappearing -- the pool itself is history-free.
//!
class PSGTPoolTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    PSGTPoolTableModel(interfaces::PSGTPoolContext& psgt_context, WalletModel* wallet_model,
                       QObject* parent = nullptr);

    enum ColumnIndex {
        Status = 0,
        Destination,
        Amount,
        Signatures,
        Age,
        Image,
        COLUMN_COUNT
    };

    //! Per-row status, derived from wallet relevance and signing progress.
    enum class RowStatus {
        AwaitingYourSignature,
        AwaitingOthers,
        // History-only terminal states, mapped from the pool's removal reason.
        Expired,
        Completed,
        Conflicted,
        Removed,
    };

    struct Row {
        std::string image_hex;     //!< The pool key / command id (raw Hash160 hex).
        std::string image_address; //!< The arrangement's P2SH address, for the Image column.
        std::string revision_hex;  //!< The revision hash, matched against the removal signal.
        RowStatus status = RowStatus::AwaitingOthers;
        QString destination;
        qint64 amount = 0;
        int sigs_valid = 0;
        int sigs_required = 0;
        int sigs_total = 0;
        qint64 time_received = 0;
        bool is_mine = false;
        bool in_pool = true; //!< false for a history (recently-completed) row.
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    //! The row backing a model index (nullptr if out of range).
    const Row* rowAt(int row) const;

public Q_SLOTS:
    //! Rebuild the row set from the pool. Also called on numBlocksChanged so
    //! ages and the pool-active banner stay current.
    void refresh();

    //! Slot bound to the core PSGTPoolChanged signal via QueuedConnection.
    //! On a removal that this wallet had a stake in, promote the row identified
    //! by revision_hash to the recently-completed history before it vanishes.
    void handlePoolChanged(const QString& revision_hash, quint8 change_type, int reason);

private:
    interfaces::PSGTPoolContext& m_psgt_context;
    WalletModel* m_wallet_model;
    QList<Row> m_rows;

    //! Recently-completed/removed rows this wallet cared about, newest first,
    //! bounded. Merged after the live pool rows in refresh().
    QList<Row> m_history;

    Row MapRow(const interfaces::PSGTPoolRow& src) const;
};

#endif // BITCOIN_QT_PSGTPOOLTABLEMODEL_H
