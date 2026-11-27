#ifndef BITCOIN_QT_ADDRESSTABLEMODEL_H
#define BITCOIN_QT_ADDRESSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

#include <future>

class AddressTablePriv;
class WalletModel;

/**
   Qt model of the address book in the core. This allows views to access and modify the address book.
 */
class AddressTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // parent is required: the model reaches the wallet through parent->wallet(),
    // so it is dereferenced unconditionally (no default null).
    explicit AddressTableModel(WalletModel* parent);
    ~AddressTableModel();

    enum ColumnIndex {
        Label = 0,   /**< User specified label */
        Address = 1  /**< Bitcoin address */
    };

    static constexpr std::initializer_list<ColumnIndex> all_ColumnIndex = {Label, Address};

    enum RoleIndex {
        TypeRole = Qt::UserRole /**< Type of address (#Send or #Receive) */
    };

    /** Return status of edit/insert operation */
    enum EditStatus {
        OK,                     /**< Everything ok */
        NO_CHANGES,             /**< No changes were made during edit operation */
        INVALID_ADDRESS,        /**< Unparseable address */
        DUPLICATE_ADDRESS,      /**< Address already in address book */
        WALLET_UNLOCK_FAILURE,  /**< Wallet could not be unlocked to create new receiving address */
        KEY_GENERATION_FAILURE, /**< Generating a new public key for a receiving address failed */
        NOT_MINE                /**< Address is not owned by this wallet */
    };

    static const QString Send;              /**< Specifies send address */
    static const QString Receive;           /**< Specifies receive address */
    static const QString ReceiveExisting;   /**< Specifies existing receive address */

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex &index) const;
    /*@}*/

    /* Add an address to the model.
       Returns the added address on success, and an empty string otherwise.
     */
    QString addRow(const QString &type, const QString &label, const QString &address);

    /* Ask for the owned addresses not yet in the address book. They arrive,
       sorted, in unbookedReceiveAddressesReady. The wallet reads them under
       cs_main, so they are read on the modal worker, not on the GUI thread.
     */
    void requestUnbookedReceiveAddresses();

    /* Look up label for address in address book, if not found return empty string.
     */
    QString labelForAddress(const QString &address) const;

    /* Look up row index of an address in the model.
       Return -1 if not found.
     */
    int lookupAddress(const QString &address) const;

    EditStatus getEditStatus() const { return editStatus; }

private:
    WalletModel *walletModel;
    AddressTablePriv *priv;
    QStringList columns;
    EditStatus editStatus;
    //! The last request for the unbooked addresses. It reports to this model,
    //! so the destructor waits for it. The requests run one at a time, in
    //! order, so an earlier one has ended by then too.
    std::future<void> m_unbooked_done;

    /** Notify listeners that data changed. */
    void emitDataChanged(int index);

signals:
    void defaultAddressChanged(const QString &address);
    //! The answer to requestUnbookedReceiveAddresses(), on the GUI thread. Empty
    //! when the addresses could not be read.
    void unbookedReceiveAddressesReady(const QStringList& addresses);

public slots:
    /* Update address list from core.
     */
    void updateEntry(const QString &address, const QString &label, bool isMine, int status);

    friend class AddressTablePriv;
};

#endif // BITCOIN_QT_ADDRESSTABLEMODEL_H
