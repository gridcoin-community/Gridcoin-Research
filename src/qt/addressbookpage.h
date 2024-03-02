#ifndef BITCOIN_QT_ADDRESSBOOKPAGE_H
#define BITCOIN_QT_ADDRESSBOOKPAGE_H

#include <QDialog>

namespace Ui {
    class AddressBookPage;
}
class AddressTableModel;
class OptionsModel;

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

/** Widget that shows a list of sending or receiving addresses.
  */
class AddressBookPage : public QDialog
{
    Q_OBJECT

public:
    enum Tabs {
        SendingTab = 0,
        ReceivingTab = 1
    };

    enum Mode {
        ForSending, /**< Open address book to pick address for sending */
        ForEditing  /**< Open address book for editing */
    };

    explicit AddressBookPage(Mode mode, Tabs tab, QWidget* parent = nullptr);
    ~AddressBookPage();

    void setModel(AddressTableModel *model);
    void setOptionsModel(OptionsModel *optionsModel);
    const QString &getReturnValue() const { return returnValue; }

public slots:
    void done(int retval) override;
    void exportClicked();
    void changeFilter(const QString& needle);
    void resizeTableColumns(const bool& neighbor_pair_adjust = false, const int& index = 0,
                            const int& old_size = 0, const int& new_size = 0);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::AddressBookPage *ui;
    AddressTableModel *model;
    OptionsModel *optionsModel;
    Mode mode;
    Tabs tab;
    QString returnValue;
    QSortFilterProxyModel *proxyModel;
    QSortFilterProxyModel *filterProxyModel;
    QMenu *contextMenu;
    QAction *deleteAction;
    QString newAddressToSelect;

    std::vector<int> m_table_column_sizes;
    bool m_init_column_sizes_set;
    bool m_resize_columns_in_progress;

private slots:
    void on_deleteButton_clicked();
    void on_newAddressButton_clicked();
    /** Copy address of currently selected address entry to clipboard */
    void on_copyToClipboardButton_clicked();
    void on_signMessageButton_clicked();
    void on_verifyMessageButton_clicked();
    void selectionChanged();
    void on_showQRCodeButton_clicked();
    /** Spawn contextual menu (right mouse menu) for address book entry */
    void contextualMenu(const QPoint &point);

    /** Copy label of currently selected address entry to clipboard */
    void onCopyLabelAction();
    /** Edit currently selected address entry */
    void onEditAction();

    /** New entry/entries were added to address table */
    void selectNewAddress(const QModelIndex &parent, int begin, int end);

    /** Resize address book table columns based on incoming signal */
    void addressBookSectionResized(int index, int old_size, int new_size);

signals:
    void signMessage(QString addr);
    void verifyMessage(QString addr);
};

#endif // BITCOIN_QT_ADDRESSBOOKPAGE_H
