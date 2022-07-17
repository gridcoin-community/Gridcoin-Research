#include "addressbookpage.h"
#include "ui_addressbookpage.h"

#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"
#include "qt/decoration.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QLatin1String>
#include <QMessageBox>
#include <QMenu>

#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

AddressBookPage::AddressBookPage(Mode mode, Tabs tab, QWidget* parent)
             : QDialog(parent)
             , ui(new Ui::AddressBookPage)
             , model(nullptr)
             , optionsModel(nullptr)
             , mode(mode)
             , tab(tab)
             , m_table_column_sizes({150, 100})
             , m_init_column_sizes_set(false)
             , m_resize_columns_in_progress(false)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->newAddressButton->setIcon(QIcon());
    ui->copyToClipboardButton->setIcon(QIcon());
    ui->deleteButton->setIcon(QIcon());
#endif

#ifndef USE_QRCODE
    ui->showQRCodeButton->setVisible(false);
#endif

    switch(mode)
    {
    case ForSending:
        connect(ui->tableView, &QTableView::doubleClicked, this, &QDialog::accept);
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->tableView->setFocus();
        break;
    case ForEditing:
        ui->okayButtonBox->setVisible(false);
        break;
    }
    switch(tab)
    {
    case SendingTab:
        ui->explanationLabel->setVisible(false);
        ui->deleteButton->setVisible(true);
        ui->signMessageButton->setVisible(false);
        break;
    case ReceivingTab:
        ui->deleteButton->setVisible(false);
        ui->verifyMessageButton->setVisible(false);
        ui->signMessageButton->setVisible(true);
        break;
    }

    // Context menu actions
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *copyAddressAction = new QAction(ui->copyToClipboardButton->text(), this);
    QAction *editAction = new QAction(tr("&Edit"), this);
    QAction *showQRCodeAction = new QAction(ui->showQRCodeButton->text(), this);
    QAction *signMessageAction = new QAction(ui->signMessageButton->text(), this);
    QAction *verifyMessageAction = new QAction(ui->verifyMessageButton->text(), this);
    deleteAction = new QAction(ui->deleteButton->text(), this);

    // Build context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(editAction);
    if(tab == SendingTab)
        contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(showQRCodeAction);
    if(tab == ReceivingTab)
        contextMenu->addAction(signMessageAction);
    else if(tab == SendingTab)
        contextMenu->addAction(verifyMessageAction);

    // Connect signals for context menu actions
    connect(copyAddressAction, &QAction::triggered, this, &AddressBookPage::on_copyToClipboardButton_clicked);
    connect(copyLabelAction, &QAction::triggered, this, &AddressBookPage::onCopyLabelAction);
    connect(editAction, &QAction::triggered, this, &AddressBookPage::onEditAction);
    connect(deleteAction, &QAction::triggered, this, &AddressBookPage::on_deleteButton_clicked);
    connect(showQRCodeAction, &QAction::triggered, this, &AddressBookPage::on_showQRCodeButton_clicked);
    connect(signMessageAction, &QAction::triggered, this, &AddressBookPage::on_signMessageButton_clicked);
    connect(verifyMessageAction, &QAction::triggered, this, &AddressBookPage::on_verifyMessageButton_clicked);

    connect(ui->tableView, &QWidget::customContextMenuRequested, this, &AddressBookPage::contextualMenu);

    // Pass through accept action from button box
    connect(ui->okayButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    connect(ui->tableView->horizontalHeader(), &QHeaderView::sectionResized,
                    this, &AddressBookPage::addressBookSectionResized);
}

AddressBookPage::~AddressBookPage()
{
    delete ui;
}

void AddressBookPage::setModel(AddressTableModel *model)
{
    this->model = model;
    if(!model)
        return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    switch(tab)
    {
    case ReceivingTab:
        // Receive filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Receive);
        break;
    case SendingTab:
        // Send filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Send);
        break;
    }

    filterProxyModel = new QSortFilterProxyModel(this);
    filterProxyModel->setSourceModel(proxyModel);
    filterProxyModel->setDynamicSortFilter(true);
    filterProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterProxyModel->setFilterKeyColumn(-1); // All columns

    ui->tableView->setModel(filterProxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    connect(ui->tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AddressBookPage::selectionChanged);

    // Select row for newly created address
    connect(model, &AddressTableModel::rowsInserted,
            this, &AddressBookPage::selectNewAddress);

    selectionChanged();
}

void AddressBookPage::setOptionsModel(OptionsModel *optionsModel)
{
    this->optionsModel = optionsModel;
}

void AddressBookPage::on_copyToClipboardButton_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Address);
}

void AddressBookPage::onCopyLabelAction()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Label);
}

void AddressBookPage::onEditAction()
{
    if(!ui->tableView->selectionModel())
        return;
    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows();
    if(indexes.isEmpty())
        return;

    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::EditSendingAddress :
            EditAddressDialog::EditReceivingAddress);

    QModelIndex origIndex = filterProxyModel->mapToSource(indexes.at(0));
    origIndex = proxyModel->mapToSource(origIndex);

    dlg.setModel(model);
    dlg.loadRow(origIndex.row());
    dlg.exec();
}

void AddressBookPage::on_signMessageButton_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    for (QModelIndex index : indexes) {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit signMessage(addr);
}

void AddressBookPage::on_verifyMessageButton_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    for (QModelIndex index : indexes) {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit verifyMessage(addr);
}

void AddressBookPage::on_newAddressButton_clicked()
{
    if(!model)
        return;
    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::NewSendingAddress :
            EditAddressDialog::NewReceivingAddress, this);
    dlg.setModel(model);
    if(dlg.exec())
    {
        newAddressToSelect = dlg.getAddress();
    }
}

void AddressBookPage::on_deleteButton_clicked()
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;
    QModelIndexList indexes = table->selectionModel()->selectedRows();
    if(!indexes.isEmpty())
    {
        table->model()->removeRow(indexes.at(0).row());
    }
}

void AddressBookPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        switch(tab)
        {
        case SendingTab:
            // In sending tab, allow deletion of selection
            ui->deleteButton->setEnabled(true);
            ui->deleteButton->setVisible(true);
            deleteAction->setEnabled(true);
            ui->signMessageButton->setEnabled(false);
            ui->signMessageButton->setVisible(false);
            ui->verifyMessageButton->setEnabled(true);
            ui->verifyMessageButton->setVisible(true);
            break;
        case ReceivingTab:
            // Deleting receiving addresses, however, is not allowed
            ui->deleteButton->setEnabled(false);
            ui->deleteButton->setVisible(false);
            deleteAction->setEnabled(false);
            ui->signMessageButton->setEnabled(true);
            ui->signMessageButton->setVisible(true);
            ui->verifyMessageButton->setEnabled(false);
            ui->verifyMessageButton->setVisible(false);
            break;
        }
        ui->copyToClipboardButton->setEnabled(true);
        ui->showQRCodeButton->setEnabled(true);
    }
    else
    {
        ui->deleteButton->setEnabled(false);
        ui->showQRCodeButton->setEnabled(false);
        ui->copyToClipboardButton->setEnabled(false);
        ui->signMessageButton->setEnabled(false);
        ui->verifyMessageButton->setEnabled(false);
    }
}

void AddressBookPage::done(int retval)
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel() || !table->model())
        return;
    // When this is a tab/widget and not a model dialog, ignore "done"
    if(mode == ForEditing)
        return;

    // Figure out which address was selected, and return it
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);

    for (QModelIndex index : indexes) {
        QVariant address = table->model()->data(index);
        returnValue = address.toString();
    }

    if(returnValue.isEmpty())
    {
        // If no address entry selected, return rejected
        retval = Rejected;
    }

    QDialog::done(retval);
}

void AddressBookPage::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Address Book Data"), QString(),
            tr("Comma separated file", "Name of CSV file format") + QLatin1String(" (*.csv)"), nullptr);

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
    writer.addColumn("Address", AddressTableModel::Address, Qt::EditRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}

void AddressBookPage::changeFilter(const QString& needle)
{
    filterProxyModel->setFilterFixedString(needle);
}

void AddressBookPage::on_showQRCodeButton_clicked()
{
#ifdef USE_QRCODE
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);

    for (QModelIndex index : indexes) {
        QString address = index.data().toString(), label = index.sibling(index.row(), 0).data(Qt::EditRole).toString();

        QRCodeDialog *dialog = new QRCodeDialog(address, label, tab == ReceivingTab, this);
        if(optionsModel)
            dialog->setModel(optionsModel);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
#endif
}

void AddressBookPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void AddressBookPage::selectNewAddress(const QModelIndex &parent, int begin, int end)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, AddressTableModel::Address, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAddressToSelect))
    {
        // Select row of newly created address, once
        ui->tableView->setFocus();
        ui->tableView->selectRow(idx.row());
        newAddressToSelect.clear();
    }
}

void AddressBookPage::resizeTableColumns(const bool& neighbor_pair_adjust, const int& index,
                                         const int& old_size, const int& new_size)
{
    // This prevents unwanted recursion to here from addressBookSectionResized.
    m_resize_columns_in_progress = true;

    if (!model) {
        m_resize_columns_in_progress = false;

        return;
    }

    if (!m_init_column_sizes_set) {
        for (int i = 0; i < (int) m_table_column_sizes.size(); ++i) {
            ui->tableView->horizontalHeader()->resizeSection(i, m_table_column_sizes[i]);
        }

        m_init_column_sizes_set = true;
        m_resize_columns_in_progress = false;

        return;
    }

    if (neighbor_pair_adjust) {
        if (index != AddressTableModel::all_ColumnIndex.size() - 1) {
            int new_neighbor_section_size = ui->tableView->horizontalHeader()->sectionSize(index + 1)
                    + old_size - new_size;

            ui->tableView->horizontalHeader()->resizeSection(
                        index + 1, new_neighbor_section_size);

            // This detects and deals with the case where the resize of a column tries to force the neighbor
            // to a size below its minimum, in which case we have to reverse out the attempt.
            if (ui->tableView->horizontalHeader()->sectionSize(index + 1)
                    != new_neighbor_section_size) {
                ui->tableView->horizontalHeader()->resizeSection(
                            index,
                            ui->tableView->horizontalHeader()->sectionSize(index)
                            + new_neighbor_section_size
                            - ui->tableView->horizontalHeader()->sectionSize(index + 1));
            }
        } else {
            // Do not allow the last column to be resized because there is no adjoining neighbor to the right
            // and we are maintaining the total width fixed to the size of the containing frame.
            ui->tableView->horizontalHeader()->resizeSection(index, old_size);
        }

        m_resize_columns_in_progress = false;

        return;
    }

    // This is the proportional resize case when the window is resized or the receive or favorites icon button is pressed.
    const int width = ui->tableView->horizontalHeader()->width() - 5;

    int orig_header_width = 0;

    for (const auto& iter : AddressTableModel::all_ColumnIndex) {
        orig_header_width += ui->tableView->horizontalHeader()->sectionSize(iter);
    }

    if (!width || !orig_header_width) return;

    for (const auto& iter : AddressTableModel::all_ColumnIndex) {
        int section_size = ui->tableView->horizontalHeader()->sectionSize(iter);

        ui->tableView->horizontalHeader()->resizeSection(
                    iter, section_size * width / orig_header_width);
    }

    m_resize_columns_in_progress = false;
}

void AddressBookPage::resizeEvent(QResizeEvent *event)
{
    resizeTableColumns();

    QWidget::resizeEvent(event);
}

void AddressBookPage::addressBookSectionResized(int index, int old_size, int new_size)
{
    // Avoid implicit recursion between resizeTableColumns and addressBookSectionResized
    if (m_resize_columns_in_progress) return;

    resizeTableColumns(true, index, old_size, new_size);
}
