#ifndef BITCOIN_QT_TRANSACTIONTABLEMODEL_H
#define BITCOIN_QT_TRANSACTIONTABLEMODEL_H

#include "qt/wallet_event_queue.h"

#include <QAbstractTableModel>
#include <QStringList>

#include <vector>

class CWallet;
class TransactionTablePriv;
class TransactionRecord;
class WalletModel;

/** UI model for the transaction table of a wallet.
 */
class TransactionTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit TransactionTableModel(CWallet* wallet, WalletModel* parent = nullptr);
    ~TransactionTableModel();

    enum ColumnIndex {
        Status = 0,
        Date = 1,
        Type = 2,
        ToAddress = 3,
        Amount = 4
    };

    static constexpr std::initializer_list<ColumnIndex> all_ColumnIndex = {Status, Date, Type, ToAddress, Amount};

    /** Roles to get specific information from a transaction row.
        These are independent of column.
    */
    enum RoleIndex {
        /** Type of transaction */
        TypeRole = Qt::UserRole,
        /** Date and time this transaction was created */
        DateRole,
        /** Long description (HTML format) */
        LongDescriptionRole,
        /** Address of transaction */
        AddressRole,
        /** Label of address related to transaction */
        LabelRole,
        /** Net amount of transaction */
        AmountRole,
        /** Unique identifier */
        TxIDRole,
        /** Is transaction confirmed? */
        ConfirmedRole,
        /** Formatted amount, without brackets when unconfirmed */
        FormattedAmountRole,
        /** Transaction status (TransactionRecord::Status) */
        StatusRole
    };

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    //! Role-formatting core (factored out of data()): render \p role for an
    //! arbitrary \p rec at \p column (a ColumnIndex). data() calls it for this
    //! model's rows; the per-view windowed consumers (OverviewTxModel, PR3) call
    //! it for their own served records, so the formatters live in exactly one
    //! place. \p rec may be any record, not necessarily in this model's replica.
    QVariant formatRole(TransactionRecord *rec, int column, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;

    //! First replica row matching tx \p hash, as a model index, or an invalid
    //! index. Maps a per-view consumer's clicked row (OverviewTxModel, PR3) back
    //! to a TransactionTableModel index for the detailed-view click-through.
    QModelIndex indexForTxid(const uint256& hash) const;

    //!
    //! \brief Consume a batch of producer-side wallet events drained from
    //! WalletModel's WalletEventQueue. Inserts/removes rows from the cached
    //! list without taking cs_main or cs_wallet — payloads are already
    //! decomposed at the producer side under the locks that were held there.
    //!
    //! Status updates (TxUpdated) do not mutate rows directly; status fields
    //! refresh lazily on read via TransactionTableModel::data() / the existing
    //! updateConfirmations() flow.
    //!
    void applyEventBatch(const std::vector<GRC::WalletEvent>& events);

private:
    CWallet* wallet;
    WalletModel *walletModel;
    QStringList columns;
    TransactionTablePriv *priv;

    QString lookupAddress(const std::string &address, bool tooltip) const;
    QVariant addressColor(const TransactionRecord *wtx) const;
    QString formatTxStatus(const TransactionRecord *wtx) const;
    QString formatTxDate(const TransactionRecord *wtx) const;
    QString formatTxType(const TransactionRecord *wtx) const;
    QString formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const;
    QString formatTxAmount(const TransactionRecord *wtx, bool showUnconfirmed=true) const;
    QString formatTooltip(const TransactionRecord *rec) const;
    QString formatTxTypeExplanation(const TransactionRecord *rec) const;
    QVariant txStatusDecoration(const TransactionRecord *wtx) const;
    QVariant txAddressDecoration(const TransactionRecord *wtx) const;

public slots:
    void refreshWallet();
    void updateConfirmations();
    void updateDisplayUnit();

    friend class TransactionTablePriv;
};

#endif // BITCOIN_QT_TRANSACTIONTABLEMODEL_H

