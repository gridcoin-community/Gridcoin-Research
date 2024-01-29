#include "transactionview.h"

#include "transactionfilterproxy.h"
#include "transactionrecord.h"
#include "walletmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"
#include "bitcoinunits.h"
#include "csvmodelwriter.h"
#include "transactiondescdialog.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "qt/decoration.h"

#include <QScrollBar>
#include <QComboBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLatin1String>
#include <QLineEdit>
#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QPoint>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QDateTimeEdit>

TransactionView::TransactionView(QWidget *parent)
    : QFrame(parent)
    , model(nullptr)
    , transactionProxyModel(nullptr)
    , transactionView(nullptr)
    , searchWidgetIconAction(new QAction())
    , m_table_column_sizes({23, 120, 120, 400, 100})
    , m_init_column_sizes_set(false)
    , m_resize_columns_in_progress(false)
{
    setContentsMargins(0, 0, 0, 0);

    // Build header
    QHBoxLayout *headerFrameLayout = new QHBoxLayout();
    headerFrameLayout->setContentsMargins(0, 0, 0, 0);
    headerFrameLayout->setSpacing(15);

    QLabel *headerTitleLabel = new QLabel(tr("Transaction History"));
    headerTitleLabel->setObjectName("headerTitleLabel");
    GRC::ScaleFontPointSize(headerTitleLabel, 15);
    headerFrameLayout->addWidget(headerTitleLabel);

    headerFrameLayout->addStretch();

    searchWidget = new QLineEdit(this);
    searchWidget->setPlaceholderText(tr("Search by address or label"));
    searchWidget->addAction(searchWidgetIconAction, QLineEdit::LeadingPosition);
    searchWidget->setClearButtonEnabled(true);
    headerFrameLayout->addWidget(searchWidget);

    QFrame *headerFrame = new QFrame(this);
    headerFrame->setObjectName("headerFrame");
    headerFrame->setLayout(headerFrameLayout);

    // Build filter row
    QHBoxLayout *filterFrameLayout = new QHBoxLayout();
    filterFrameLayout->setContentsMargins(0, 0, 0, 0);
    filterFrameLayout->setSpacing(6);

    dateWidget = new QComboBox(this);
    dateWidget->addItem(tr("All Time"), All);
    dateWidget->addItem(tr("Today"), Today);
    dateWidget->addItem(tr("This week"), ThisWeek);
    dateWidget->addItem(tr("This month"), ThisMonth);
    dateWidget->addItem(tr("Last month"), LastMonth);
    dateWidget->addItem(tr("This year"), ThisYear);
    dateWidget->addItem(tr("Range..."), Range);
    filterFrameLayout->addWidget(dateWidget);

    typeWidget = new QComboBox(this);

    // Add catch-all
    typeWidget->addItem(tr("All Types"), TransactionFilterProxy::ALL_TYPES);

    // Add types from TransactionRecord Type enum.
    for (const auto& iter : TransactionRecord::TYPES) {
        typeWidget->addItem(TransactionRecord::TypeToString(iter), TransactionFilterProxy::TYPE(iter));
    }

    filterFrameLayout->addWidget(typeWidget);

    filterFrameLayout->addStretch();

    amountWidget = new QLineEdit(this);
    amountWidget->setPlaceholderText(tr("Min amount"));
    amountWidget->setValidator(new QDoubleValidator(0, 1e20, 8, this));
    QSizePolicy amountWidgetSizePolicy = amountWidget->sizePolicy();
    amountWidgetSizePolicy.setHorizontalPolicy(QSizePolicy::Minimum);
    amountWidget->setSizePolicy(amountWidgetSizePolicy);
    filterFrameLayout->addWidget(amountWidget);

    QFrame *filterFrame = new QFrame(this);
    filterFrame->setObjectName("filterFrame");
    filterFrame->setLayout(filterFrameLayout);

    QTableView *view = new QTableView(this);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setShowGrid(false);
    view->horizontalHeader()->setHighlightSections(false);
    transactionView = view;

    QVBoxLayout *tableViewLayout = new QVBoxLayout();
    tableViewLayout->setContentsMargins(9, 9, 9, 9);
    QFrame *tableViewFrame = new QFrame(this);
    tableViewFrame->setObjectName("historyTableFrame");
    tableViewFrame->setLayout(new QVBoxLayout());
    tableViewFrame->layout()->setContentsMargins(0, 0, 0, 0);
    tableViewFrame->layout()->addWidget(view);
    tableViewLayout->addWidget(tableViewFrame);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->setSpacing(0);
    vlayout->addWidget(headerFrame);
    vlayout->addWidget(filterFrame);
    vlayout->addWidget(createDateRangeWidget());
    vlayout->addLayout(tableViewLayout);

    // Actions
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
    QAction *copyTxIDAction = new QAction(tr("Copy transaction ID"), this);
    QAction *editLabelAction = new QAction(tr("Edit label"), this);
    QAction *showDetailsAction = new QAction(tr("Show transaction details"), this);

    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTxIDAction);
    contextMenu->addAction(editLabelAction);
    contextMenu->addAction(showDetailsAction);

    // Connect actions
    connect(dateWidget, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &TransactionView::chooseDate);
    connect(typeWidget, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &TransactionView::chooseType);
    connect(searchWidget, &QLineEdit::textChanged, this, &TransactionView::changedPrefix);
    connect(amountWidget, &QLineEdit::textChanged, this, &TransactionView::changedAmount);

    connect(view, &QTableView::doubleClicked, this, &TransactionView::doubleClicked);
    connect(view, &QWidget::customContextMenuRequested, this, &TransactionView::contextualMenu);

    connect(copyAddressAction, &QAction::triggered, this, &TransactionView::copyAddress);
    connect(copyLabelAction, &QAction::triggered, this, &TransactionView::copyLabel);
    connect(copyAmountAction, &QAction::triggered, this, &TransactionView::copyAmount);
    connect(copyTxIDAction, &QAction::triggered, this, &TransactionView::copyTxID);
    connect(editLabelAction, &QAction::triggered, this, &TransactionView::editLabel);
    connect(showDetailsAction, &QAction::triggered, this, &TransactionView::showDetails);
}

void TransactionView::setModel(WalletModel *model)
{
    this->model = model;
    if(model)
    {
        transactionProxyModel = new TransactionFilterProxy(this);
        transactionProxyModel->setSourceModel(model->getTransactionTableModel());
        transactionProxyModel->setDynamicSortFilter(true);
        transactionProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        transactionProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

        transactionProxyModel->setSortRole(Qt::EditRole);

        transactionView->setModel(transactionProxyModel);
        transactionView->setAlternatingRowColors(true);
        transactionView->setSelectionBehavior(QAbstractItemView::SelectRows);
        transactionView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        transactionView->setSortingEnabled(true);
        transactionView->sortByColumn(TransactionTableModel::Date, Qt::DescendingOrder);
        transactionView->verticalHeader()->hide();

        connect(transactionView->horizontalHeader(), &QHeaderView::sectionResized,
                this, &TransactionView::txnViewSectionResized);
    }

    if (model && model->getOptionsModel()) {
        connect(
            model->getOptionsModel(), &OptionsModel::walletStylesheetChanged,
            this, &TransactionView::updateIcons);
        updateIcons(model->getOptionsModel()->getCurrentStyle());
    }
}

void TransactionView::chooseDate(int idx)
{
    if(!transactionProxyModel)
        return;
    QDate current = QDate::currentDate();
    dateRangeWidget->setVisible(false);
    switch(dateWidget->itemData(idx).toInt())
    {
    case All:
        transactionProxyModel->setDateRange(
                TransactionFilterProxy::MIN_DATE,
                TransactionFilterProxy::MAX_DATE);
        break;
    case Today:
        transactionProxyModel->setDateRange(
                GUIUtil::StartOfDay(current),
                TransactionFilterProxy::MAX_DATE);
        break;
    case ThisWeek: {
        // Find last Monday
        QDate startOfWeek = current.addDays(-(current.dayOfWeek()-1));
        transactionProxyModel->setDateRange(
                GUIUtil::StartOfDay(startOfWeek),
                TransactionFilterProxy::MAX_DATE);

        } break;
    case ThisMonth:
        transactionProxyModel->setDateRange(
                GUIUtil::StartOfDay(QDate(current.year(), current.month(), 1)),
                TransactionFilterProxy::MAX_DATE);
        break;
    case LastMonth:
        transactionProxyModel->setDateRange(
                GUIUtil::StartOfDay(QDate(current.year(), current.month(), 1).addMonths(-1)),
                GUIUtil::StartOfDay(QDate(current.year(), current.month(), 1)));
        break;
    case ThisYear:
        transactionProxyModel->setDateRange(
                GUIUtil::StartOfDay(QDate(current.year(), 1, 1)),
                TransactionFilterProxy::MAX_DATE);
        break;
    case Range:
        dateRangeWidget->setVisible(true);
        dateRangeChanged();
        break;
    }
}

void TransactionView::chooseType(int idx)
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setTypeFilter(
        typeWidget->itemData(idx).toInt());
}

void TransactionView::changedPrefix(const QString &prefix)
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setAddressPrefix(prefix);
}

void TransactionView::changedAmount(const QString &amount)
{
    if(!transactionProxyModel)
        return;
    qint64 amount_parsed = 0;
    if(BitcoinUnits::parse(model->getOptionsModel()->getDisplayUnit(), amount, &amount_parsed))
    {
        transactionProxyModel->setMinAmount(amount_parsed);
    }
    else
    {
        transactionProxyModel->setMinAmount(0);
    }
}

void TransactionView::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Transaction Data"), QString(),
            tr("Comma separated file", "Name of CSV file format") + QLatin1String(" (*.csv)"), nullptr);

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(transactionProxyModel);
    writer.addColumn(tr("Confirmed"), 0, TransactionTableModel::ConfirmedRole);
    writer.addColumn(tr("Date"), 0, TransactionTableModel::DateRole);
    writer.addColumn(tr("Type"), TransactionTableModel::Type, Qt::EditRole);
    writer.addColumn(tr("Label"), 0, TransactionTableModel::LabelRole);
    writer.addColumn(tr("Address"), 0, TransactionTableModel::AddressRole);
    writer.addColumn(tr("Amount"), 0, TransactionTableModel::FormattedAmountRole);
    writer.addColumn(tr("ID"), 0, TransactionTableModel::TxIDRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}

void TransactionView::contextualMenu(const QPoint &point)
{
    QModelIndex index = transactionView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void TransactionView::copyAddress()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::AddressRole);
}

void TransactionView::copyLabel()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::LabelRole);
}

void TransactionView::copyAmount()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::FormattedAmountRole);
}

void TransactionView::copyTxID()
{
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::TxIDRole);
}

void TransactionView::editLabel()
{
    if(!transactionView->selectionModel() ||!model)
        return;
    QModelIndexList selection = transactionView->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        AddressTableModel *addressBook = model->getAddressTableModel();
        if(!addressBook)
            return;
        QString address = selection.at(0).data(TransactionTableModel::AddressRole).toString();
        if(address.isEmpty())
        {
            // If this transaction has no associated address, exit
            return;
        }
        // Is address in address book? Address book can miss address when a transaction is
        // sent from outside the UI.
        int idx = addressBook->lookupAddress(address);
        if(idx != -1)
        {
            // Edit sending / receiving address
            QModelIndex modelIdx = addressBook->index(idx, 0, QModelIndex());
            // Determine type of address, launch appropriate editor dialog type
            QString type = modelIdx.data(AddressTableModel::TypeRole).toString();

            EditAddressDialog dlg(type==AddressTableModel::Receive
                                         ? EditAddressDialog::EditReceivingAddress
                                         : EditAddressDialog::EditSendingAddress,
                                  this);
            dlg.setModel(addressBook);
            dlg.loadRow(idx);
            dlg.exec();
        }
        else
        {
            // Add sending address
            EditAddressDialog dlg(EditAddressDialog::NewSendingAddress,
                                  this);
            dlg.setModel(addressBook);
            dlg.setAddress(address);
            dlg.exec();
        }
    }
}

void TransactionView::showDetails()
{
    if(!transactionView->selectionModel())
        return;
    QModelIndexList selection = transactionView->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        TransactionDescDialog dlg(selection.at(0));
        dlg.exec();
    }
}

QWidget *TransactionView::createDateRangeWidget()
{
    dateRangeWidget = new QFrame();
    dateRangeWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    dateRangeWidget->setContentsMargins(1,1,1,1);
    QHBoxLayout *layout = new QHBoxLayout(dateRangeWidget);
    layout->setContentsMargins(0,0,0,0);
    layout->addSpacing(23);
    layout->addWidget(new QLabel(tr("Range:")));

    dateFrom = new QDateTimeEdit(this);
    dateFrom->setDisplayFormat("dd/MM/yy");
    dateFrom->setCalendarPopup(true);
    dateFrom->setMinimumWidth(100);
    dateFrom->setDate(QDate::currentDate().addDays(-7));
    layout->addWidget(dateFrom);
    layout->addWidget(new QLabel(tr("to")));

    dateTo = new QDateTimeEdit(this);
    dateTo->setDisplayFormat("dd/MM/yy");
    dateTo->setCalendarPopup(true);
    dateTo->setMinimumWidth(100);
    dateTo->setDate(QDate::currentDate());
    layout->addWidget(dateTo);
    layout->addStretch();

    // Hide by default
    dateRangeWidget->setVisible(false);

    // Notify on change
    connect(dateFrom, &QDateTimeEdit::dateChanged, this, &TransactionView::dateRangeChanged);
    connect(dateTo, &QDateTimeEdit::dateChanged, this, &TransactionView::dateRangeChanged);

    return dateRangeWidget;
}

void TransactionView::dateRangeChanged()
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setDateRange(
            GUIUtil::StartOfDay(dateFrom->date()),
            GUIUtil::StartOfDay(dateTo->date()).addDays(1));
}

void TransactionView::focusTransaction(const QModelIndex &idx)
{
    if(!transactionProxyModel)
        return;
    QModelIndex targetIdx = transactionProxyModel->mapFromSource(idx);
    transactionView->scrollTo(targetIdx);
    transactionView->setCurrentIndex(targetIdx);
    transactionView->setFocus();
}

void TransactionView::updateIcons(const QString& theme)
{
    searchWidgetIconAction->setIcon(QIcon(":/icons/" + theme + "_search"));
}

void TransactionView::resizeTableColumns(const bool& neighbor_pair_adjust, const int& index,
                                         const int& old_size, const int& new_size)
{
    // This prevents unwanted recursion to here from txnViewSectionResized.
    m_resize_columns_in_progress = true;

    if (!model) {
        m_resize_columns_in_progress = false;

        return;
    }

    if (!m_init_column_sizes_set) {
        for (int i = 0; i < (int) m_table_column_sizes.size(); ++i) {
            transactionView->horizontalHeader()->resizeSection(i, m_table_column_sizes[i]);
        }

        m_init_column_sizes_set = true;
        m_resize_columns_in_progress = false;

        return;
    }

    if (neighbor_pair_adjust) {
        if (index != TransactionTableModel::all_ColumnIndex.size() - 1) {
            int new_neighbor_section_size = transactionView->horizontalHeader()->sectionSize(index + 1)
                    + old_size - new_size;

            transactionView->horizontalHeader()->resizeSection(
                        index + 1, new_neighbor_section_size);

            // This detects and deals with the case where the resize of a column tries to force the neighbor
            // to a size below its minimum, in which case we have to reverse out the attempt.
            if (transactionView->horizontalHeader()->sectionSize(index + 1)
                    != new_neighbor_section_size) {
                transactionView->horizontalHeader()->resizeSection(
                            index,
                            transactionView->horizontalHeader()->sectionSize(index)
                            + new_neighbor_section_size
                            - transactionView->horizontalHeader()->sectionSize(index + 1));
            }
        } else {
            // Do not allow the last column to be resized because there is no adjoining neighbor to the right
            // and we are maintaining the total width fixed to the size of the containing frame.
            transactionView->horizontalHeader()->resizeSection(index, old_size);
        }

        m_resize_columns_in_progress = false;

        return;
    }

    // This is the proportional resize case when the window is resized or the history icon button is pressed.
    const int width = transactionView->horizontalHeader()->width() - 5;

    int orig_header_width = 0;

    for (const auto& iter : TransactionTableModel::all_ColumnIndex) {
        orig_header_width += transactionView->horizontalHeader()->sectionSize(iter);
    }

    if (!width || !orig_header_width) return;

    for (const auto& iter : TransactionTableModel::all_ColumnIndex) {
        int section_size = transactionView->horizontalHeader()->sectionSize(iter);

        transactionView->horizontalHeader()->resizeSection(
                    iter, section_size * width / orig_header_width);
    }

    m_resize_columns_in_progress = false;
}

void TransactionView::resizeEvent(QResizeEvent *event)
{
    resizeTableColumns();

    QWidget::resizeEvent(event);
}

void TransactionView::txnViewSectionResized(int index, int old_size, int new_size)
{
    // Avoid implicit recursion between resizeTableColumns and txnViewSectionResized
    if (m_resize_columns_in_progress) return;

    resizeTableColumns(true, index, old_size, new_size);
}
