#include "transactionview.h"

#include "detailedtxmodel.h"
#include "transactionrecord.h"
#include "walletmodel.h"
#include "qt/wallettxstore.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"
#include "bitcoinunits.h"
#include "csvmodelwriter.h"
#include "transactiondescdialog.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "qt/decoration.h"
#include "util/system.h"

#include <algorithm>
#include <limits>

#include <QAbstractTableModel>
#include <QItemSelectionModel>
#include <QScrollBar>
#include <QShowEvent>
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
    , m_detailedModel(nullptr)
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
    typeWidget->addItem(tr("All Types"), GRC::ALL_TYPES);

    // Add types from TransactionRecord Type enum.
    for (const auto& iter : TransactionRecord::TYPES) {
        typeWidget->addItem(TransactionRecord::TypeToString(iter), GRC::TypeBit(iter));
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
    view->setAccessibleName(tr("Transaction history"));
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
        // The cursor-backed model registers a VIEW_DETAILED view in the producer
        // WalletTxStore (server-side filter+sort off cs_main) and is the table's
        // direct model — no proxy. Sorting and filtering are pushed to the cursor
        // (sort() / setFilter()) instead of recomputed Qt-side per row. Seed the
        // local filter spec's -showorphans gate to match the model's registered
        // spec so the first widget-driven applyFilter() does not drop it.
        m_filterSpec = GRC::FilterSpec();
        m_filterSpec.show_orphans = gArgs.GetBoolArg("-showorphans", false);

        m_detailedModel = new DetailedTxModel(model, this);

        transactionView->setModel(m_detailedModel);
        transactionView->setAlternatingRowColors(true);
        transactionView->setSelectionBehavior(QAbstractItemView::SelectRows);
        transactionView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        transactionView->setSortingEnabled(true);
        transactionView->sortByColumn(TransactionTableModel::Date, Qt::DescendingOrder);
        transactionView->verticalHeader()->hide();

        connect(transactionView->horizontalHeader(), &QHeaderView::sectionResized,
                this, &TransactionView::txnViewSectionResized);

        // Windowed model (PR5-B): drive the viewport-slice content fetch from scroll
        // and resize, and capture/restore the resort anchor around a sort/filter
        // Reset. The selected row is kept cached by the synchronous ensure in the
        // context actions (copy/Show details) and by the scroll-driven fetch that
        // follows focusTransaction/restoreAnchor's scrollTo — so there is deliberately
        // NO currentChanged hook (it would call drainEventQueue from inside a
        // selection-changed handler, resetting the model mid-notification; PR5-B
        // review #12).
        connect(transactionView->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &TransactionView::reportViewport);
        connect(transactionView->horizontalHeader(), &QHeaderView::sectionClicked,
                this, &TransactionView::captureAnchor);
        connect(m_detailedModel, &DetailedTxModel::viewReset,
                this, &TransactionView::restoreAnchor);
    }

    if (model && model->getOptionsModel()) {
        connect(
            model->getOptionsModel(), &OptionsModel::walletStylesheetChanged,
            this, &TransactionView::updateIcons);
        updateIcons(model->getOptionsModel()->getCurrentStyle());
    }
}

// The cursor's date bounds are seconds since epoch (FilterSpec defaults
// 0 == MIN_DATE, 0xFFFFFFFF == MAX_DATE), comparing the same instants the old
// proxy compared as QDateTime — GUIUtil::StartOfDay()'s QDateTime carries its own
// zone, so toSecsSinceEpoch() yields the identical UTC boundary.
namespace {
constexpr int64_t MIN_DATE_SECS = 0;
constexpr int64_t MAX_DATE_SECS = 0xFFFFFFFF;
int64_t StartOfDaySecs(const QDate& date) { return GUIUtil::StartOfDay(date).toSecsSinceEpoch(); }

//! Transient read-only model over a full snapshot of the detailed view's rows, used
//! ONLY for CSV export (PR5-B). The live windowed DetailedTxModel caches just the
//! viewport, so exporting through it would blank every off-window row; this wraps
//! the producer's full filtered+sorted set (getAllRows) and reuses
//! TransactionTableModel::formatRole so the exported values are identical. The
//! snapshot vector lives only for the export and is freed at scope exit. No Q_OBJECT
//! needed — CSVModelWriter drives it polymorphically through QAbstractItemModel.
class ExportRowsModel : public QAbstractTableModel
{
public:
    ExportRowsModel(std::vector<TransactionRecord> rows, TransactionTableModel* ttm)
        : m_rows(std::move(rows)), m_ttm(ttm) {}
    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid()) return 0;
        // Saturate the size_t->int cast (consistent with getRows/getAllRows): a CSV
        // export can never exceed 2^31 rows, but a saturating cast keeps the count
        // well-defined for CSVModelWriter rather than wrapping negative (Copilot PR5-B).
        return static_cast<int>(std::min<std::size_t>(
            m_rows.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_ttm->columnCount(QModelIndex());
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0
                || static_cast<std::size_t>(index.row()) >= m_rows.size()) {
            return QVariant();
        }
        return m_ttm->formatRole(const_cast<TransactionRecord*>(&m_rows[index.row()]),
                                 index.column(), role);
    }
private:
    std::vector<TransactionRecord> m_rows;
    TransactionTableModel* m_ttm;
};
} // namespace

void TransactionView::chooseDate(int idx)
{
    if(!m_detailedModel)
        return;
    QDate current = QDate::currentDate();
    dateRangeWidget->setVisible(false);
    switch(dateWidget->itemData(idx).toInt())
    {
    case All:
        m_filterSpec.date_from = MIN_DATE_SECS;
        m_filterSpec.date_to = MAX_DATE_SECS;
        break;
    case Today:
        m_filterSpec.date_from = StartOfDaySecs(current);
        m_filterSpec.date_to = MAX_DATE_SECS;
        break;
    case ThisWeek: {
        // Find last Monday
        QDate startOfWeek = current.addDays(-(current.dayOfWeek()-1));
        m_filterSpec.date_from = StartOfDaySecs(startOfWeek);
        m_filterSpec.date_to = MAX_DATE_SECS;
        } break;
    case ThisMonth:
        m_filterSpec.date_from = StartOfDaySecs(QDate(current.year(), current.month(), 1));
        m_filterSpec.date_to = MAX_DATE_SECS;
        break;
    case LastMonth:
        m_filterSpec.date_from = StartOfDaySecs(QDate(current.year(), current.month(), 1).addMonths(-1));
        m_filterSpec.date_to = StartOfDaySecs(QDate(current.year(), current.month(), 1));
        break;
    case ThisYear:
        m_filterSpec.date_from = StartOfDaySecs(QDate(current.year(), 1, 1));
        m_filterSpec.date_to = MAX_DATE_SECS;
        break;
    case Range:
        dateRangeWidget->setVisible(true);
        dateRangeChanged();
        return;  // dateRangeChanged() applies the filter
    }
    applyFilter();
}

void TransactionView::chooseType(int idx)
{
    if(!m_detailedModel)
        return;
    m_filterSpec.type_mask = typeWidget->itemData(idx).toUInt();
    applyFilter();
}

void TransactionView::changedPrefix(const QString &prefix)
{
    if(!m_detailedModel)
        return;
    m_filterSpec.address_substr = prefix.toStdString();
    applyFilter();
}

void TransactionView::changedAmount(const QString &amount)
{
    if(!m_detailedModel)
        return;
    qint64 amount_parsed = 0;
    if(BitcoinUnits::parse(model->getOptionsModel()->getDisplayUnit(), amount, &amount_parsed))
    {
        m_filterSpec.min_amount = amount_parsed;
    }
    else
    {
        m_filterSpec.min_amount = 0;
    }
    applyFilter();
}

void TransactionView::applyFilter()
{
    if(!m_detailedModel)
        return;
    captureAnchor();   // preserve the user's row across the filter Reset (PR5-B)
    m_detailedModel->setFilter(m_filterSpec);
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

    // Export the FULL filtered+sorted set, not the live windowed model (which caches
    // only the viewport slice — exporting through it would blank every off-window
    // row). The snapshot vector is freed at scope exit (windowed-model PR5-B).
    ExportRowsModel exportModel(m_detailedModel->getAllRows(),
                                model->getTransactionTableModel());
    writer.setModel(&exportModel);
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
    ensureSelectedRowCached();   // the copied row must not be a placeholder (PR5-B)
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::AddressRole);
}

void TransactionView::copyLabel()
{
    ensureSelectedRowCached();
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::LabelRole);
}

void TransactionView::copyAmount()
{
    ensureSelectedRowCached();
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::FormattedAmountRole);
}

void TransactionView::copyTxID()
{
    ensureSelectedRowCached();
    GUIUtil::copyEntryData(transactionView, 0, TransactionTableModel::TxIDRole);
}

void TransactionView::editLabel()
{
    if(!transactionView->selectionModel() ||!model)
        return;
    QModelIndexList selection = transactionView->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        // Ensure the row is cached so AddressRole is not an off-window placeholder (PR5-B).
        if (m_detailedModel) m_detailedModel->ensureRowCached(selection.at(0).row());
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
    if(!selection.isEmpty() && model && m_detailedModel)
    {
        const int row = selection.at(0).row();
        // Sync the consumer cache to the producer cursor, then read the clicked
        // row's identity (hash, idx) from our OWN cache — the user-visible truth.
        // Resolving detail by identity (not by view-relative row) keeps it
        // drift-free: a row coordinate handed to the producer could map to a
        // neighbouring tx if our drain queue lagged the cursor, and same-hash
        // parts scatter under the Amount/Address sorts (windowed-model PR5-C).
        m_detailedModel->ensureRowCached(row);
        uint256 hash;
        int idx = -1;
        if (m_detailedModel->keyAt(row, hash, idx))
        {
            const QString html = model->getTxStore().getRowDetail(hash, idx);
            TransactionDescDialog dlg(html, this);
            dlg.exec();
        }
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
    if(!m_detailedModel)
        return;
    m_filterSpec.date_from = StartOfDaySecs(dateFrom->date());
    m_filterSpec.date_to = GUIUtil::StartOfDay(dateTo->date()).addDays(1).toSecsSinceEpoch();
    applyFilter();
}

void TransactionView::focusTransaction(const QModelIndex &idx)
{
    if(!m_detailedModel)
        return;
    // The incoming index belongs to the TransactionTableModel (OverviewPage emits
    // one mapped through indexForTxid). There is no proxy source-mapping anymore;
    // bridge to this view by transaction id, the identity shared across cursors.
    uint256 hash;
    hash.SetHex(idx.data(TransactionTableModel::TxIDRole).toString().toStdString());
    const QModelIndex targetIdx = m_detailedModel->indexForTxid(hash);
    if(!targetIdx.isValid())
        return;
    transactionView->scrollTo(targetIdx);
    transactionView->setCurrentIndex(targetIdx);
    transactionView->setFocus();
}

void TransactionView::reportViewport()
{
    if (!m_detailedModel || !transactionView->model()) return;
    // rowAt() returns -1 before first layout and for the empty strip below the last
    // row. Bail when the top is not laid out yet — the bounded ctor seed covers the
    // pre-layout window, and a later scroll/resize reports once geometry exists.
    // NEVER derive `last` from rowCount() unless the top is valid, or a pre-layout
    // report would request the whole table (PR5-B review GAP #1).
    const int first = transactionView->rowAt(0);
    if (first < 0) return;
    int last = transactionView->rowAt(transactionView->viewport()->height() - 1);
    if (last < 0) {
        // Content shorter than the viewport: safe to clamp to the last row.
        last = m_detailedModel->rowCount() - 1;
    }
    if (last < first) last = first;
    m_detailedModel->onViewportChanged(first, last);
}

void TransactionView::captureAnchor()
{
    m_have_anchor = false;
    if (!m_detailedModel || !transactionView->selectionModel()) return;
    // Prefer the current index; fall back to the topmost visible row.
    QModelIndex idx = transactionView->selectionModel()->currentIndex();
    if (!idx.isValid()) {
        const int top = transactionView->rowAt(0);
        if (top < 0) return;
        idx = m_detailedModel->index(top, 0);
    }
    // The anchor row may be a placeholder (just scrolled/clicked, not yet fetched);
    // ensure it synchronously so its record is readable — otherwise a resort right
    // after a click-through / scroll would capture nothing and fail to restore the
    // selection (PR5-B review). captureAnchor runs in a non-drain user-action context
    // (header click / filter widget), so the synchronous ensure is safe.
    m_detailedModel->ensureRowCached(idx.row());
    // Capture the EXACT (hash, idx) of the selected part, not just the hash: a tx's
    // decomposed parts share the hash but scatter under an Amount/Address sort, so a
    // hash-only anchor (rowForKey idx=-1 -> the min-position part) would restore to
    // the wrong row (PR5-B soak finding).
    uint256 anchor_hash;
    int anchor_idx = -1;
    if (!m_detailedModel->keyAt(idx.row(), anchor_hash, anchor_idx)) {
        return;   // still a placeholder (filtered/empty) — no anchor
    }
    m_anchor_hash = anchor_hash;
    m_anchor_idx = anchor_idx;
    m_have_anchor = true;
}

void TransactionView::restoreAnchor()
{
    if (!m_have_anchor || !m_detailedModel) return;
    m_have_anchor = false;
    const int row = m_detailedModel->rowForKey(m_anchor_hash, m_anchor_idx);
    if (row < 0) {
        // Anchor filtered out / gone after the resort: keep the current scroll, but
        // refresh the model's pending viewport from the post-Reset geometry so its
        // re-armed fetch targets the right window instead of a stale pre-Reset range
        // (PR5-B review cluster A).
        reportViewport();
        return;
    }
    const QModelIndex idx = m_detailedModel->index(row, 0);
    // scrollTo moves the viewport to the anchor; setCurrentIndex re-selects it.
    transactionView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    transactionView->setCurrentIndex(idx);
    // Explicitly refresh the pending viewport from the post-scroll geometry: scrollTo
    // usually fires valueChanged -> reportViewport, but NOT if the scroll position is
    // unchanged (anchor already centred) — so call it directly to guarantee the
    // re-armed fetch targets the anchor region, not the post-Reset top (PR5-B review).
    // (A copy / Show details on the anchor re-ensures synchronously.)
    reportViewport();
}

void TransactionView::ensureSelectedRowCached()
{
    if (!m_detailedModel || !transactionView->selectionModel()) return;
    const QModelIndexList sel = transactionView->selectionModel()->selectedRows();
    if (!sel.isEmpty()) m_detailedModel->ensureRowCached(sel.at(0).row());
}

void TransactionView::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    // First real geometry after layout: fetch the actually-visible window (the
    // bounded ctor seed only covers a pre-layout guess).
    reportViewport();
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

    reportViewport();   // the visible row count changed; refetch the window (PR5-B)
}

void TransactionView::txnViewSectionResized(int index, int old_size, int new_size)
{
    // Avoid implicit recursion between resizeTableColumns and txnViewSectionResized
    if (m_resize_columns_in_progress) return;

    resizeTableColumns(true, index, old_size, new_size);
}
