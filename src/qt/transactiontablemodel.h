#ifndef BITCOIN_QT_TRANSACTIONTABLEMODEL_H
#define BITCOIN_QT_TRANSACTIONTABLEMODEL_H

#include <QObject>
#include <QModelIndex>
#include <QStringList>
#include <QVariant>

class TransactionRecord;
class WalletModel;

//! Shared transaction-row formatter for the wallet's windowed transaction views.
//!
//! This is no longer a table model over a full transaction replica. The windowed
//! consumers (OverviewTxModel / VIEW_OVERVIEW and DetailedTxModel / VIEW_DETAILED)
//! hold only the served/viewport slice they display and fetch it through the
//! producer's cursor + getRows; they render each of their own records through
//! formatRole() here so the column/role formatting lives in exactly one place.
//! Because no view holds the full history any more, nothing ships the whole
//! wallet across the interface boundary (the O(wallet) reloadAndSnapshot bootstrap
//! is gone).
//!
//! It keeps the ColumnIndex / RoleIndex enums the consumers key on, and the
//! stateless formatRole() + its helpers; it holds no per-row state.
class TransactionTableModel : public QObject
{
    Q_OBJECT
public:
    explicit TransactionTableModel(WalletModel* parent = nullptr);
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

    //! Render \p role for \p rec at \p column (a ColumnIndex). The windowed
    //! consumers call it for their own served records; it reads \p rec only and
    //! keeps no state, so it is safe to call from any consumer for any record.
    QVariant formatRole(const TransactionRecord *rec, int column, int role) const;

    //! The shared column set for the windowed views: DetailedTxModel and the CSV
    //! export delegate their columnCount()/headerData() here so the column
    //! definitions live in one place.
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private:
    WalletModel *walletModel;
    QStringList columns;

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
};

#endif // BITCOIN_QT_TRANSACTIONTABLEMODEL_H
