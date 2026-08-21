#include "coincontroldialog.h"
#include "qt/guilog.h"
#include "ui_coincontroldialog.h"

#include "bitcoinunits.h"
#include "key_io.h"
#include "optionsmodel.h"
#include "consolidateunspentdialog.h"
#include "qt/coinselectionmodel.h"
#include "qt/coinselectionview.h"
#include "qt/decoration.h"

#include <set>

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QString>

#ifdef COINSELECTION_MODEL_TESTER
#include <QAbstractItemModelTester>
#endif

using namespace std;

namespace {

//! Auto-expand budget for partially-selected groups (legacy parity, bounded):
//! stop once this many child rows would be realized, so a pathological wallet
//! cannot materialize half a million view items in one shot.
constexpr int kAutoExpandRowBudget = 50000;

} // anonymous namespace

CoinControlDialog::CoinControlDialog(QWidget* parent, interfaces::WalletCoinControl* coinControl, QList<qint64>* payAmounts,
                                     bool fSubtractFeeFromAmount)
               : QDialog(parent)
               , ui(new Ui::CoinControlDialog)
               , coinControl(coinControl)
               , payAmounts(payAmounts)
               , model(nullptr)
               , sortColumn(CoinSelectionModel::COLUMN_AMOUNT)
               , sortOrder(Qt::DescendingOrder)
               , m_fSubtractFeeFromAmount(fSubtractFeeFromAmount)
{
    assert(coinControl != nullptr && payAmounts != nullptr);

    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    // context menu actions
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
             copyTransactionHashAction = new QAction(tr("Copy transaction ID"), this);  // we need to enable/disable this

    // context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTransactionHashAction);

    // context menu signals
    connect(ui->treeView, &QWidget::customContextMenuRequested, this, &CoinControlDialog::showMenu);
    connect(copyAddressAction, &QAction::triggered, this, &CoinControlDialog::copyAddress);
    connect(copyLabelAction, &QAction::triggered, this, &CoinControlDialog::copyLabel);
    connect(copyAmountAction, &QAction::triggered, this, &CoinControlDialog::copyAmount);
    connect(copyTransactionHashAction, &QAction::triggered, this, &CoinControlDialog::copyTransactionHash);

    // clipboard actions
    QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction *clipboardLowOutputAction = new QAction(tr("Copy low output"), this);
    QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);

    connect(clipboardQuantityAction, &QAction::triggered, this, &CoinControlDialog::clipboardQuantity);
    connect(clipboardAmountAction, &QAction::triggered, this, &CoinControlDialog::clipboardAmount);
    connect(clipboardFeeAction, &QAction::triggered, this, &CoinControlDialog::clipboardFee);
    connect(clipboardAfterFeeAction, &QAction::triggered, this, &CoinControlDialog::clipboardAfterFee);
    connect(clipboardBytesAction, &QAction::triggered, this, &CoinControlDialog::clipboardBytes);
    connect(clipboardLowOutputAction, &QAction::triggered, this, &CoinControlDialog::clipboardLowOutput);
    connect(clipboardChangeAction, &QAction::triggered, this, &CoinControlDialog::clipboardChange);

    ui->coinControlQuantityLabel->addAction(clipboardQuantityAction);
    ui->coinControlAmountLabel->addAction(clipboardAmountAction);
    ui->coinControlFeeLabel->addAction(clipboardFeeAction);
    ui->coinControlAfterFeeLabel->addAction(clipboardAfterFeeAction);
    ui->coinControlBytesLabel->addAction(clipboardBytesAction);
    ui->coinControlLowOutputLabel->addAction(clipboardLowOutputAction);
    ui->coinControlChangeLabel->addAction(clipboardChangeAction);

    // toggle tree/list mode
    connect(ui->treeModeRadioButton, &QRadioButton::toggled, this, &CoinControlDialog::treeModeRadioButton);
    connect(ui->listModeRadioButton, &QRadioButton::toggled, this, &CoinControlDialog::listModeRadioButton);

    // click on header
    ui->treeView->header()->setSectionsClickable(true);
    connect(ui->treeView->header(), &QHeaderView::sectionClicked, this, &CoinControlDialog::headerSectionClicked);

    // ok button
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &CoinControlDialog::buttonBoxClicked);

    // (un)select all
    connect(ui->selectAllPushButton, &QPushButton::clicked, this, &CoinControlDialog::buttonSelectAllClicked);

    // filter/consolidate button interaction
    connect(ui->maxMinOutputValue, &BitcoinAmountField::textChanged, this, &CoinControlDialog::maxMinOutputValueChanged);

    // filter mode
    connect(ui->filterModePushButton, &QPushButton::clicked, this, &CoinControlDialog::buttonFilterModeClicked);

    // filter
    connect(ui->filterPushButton, &QPushButton::clicked, this, &CoinControlDialog::buttonFilterClicked);

    // consolidate
    connect(ui->consolidateButton, &QPushButton::clicked, this, &CoinControlDialog::buttonConsolidateClicked);

    ui->consolidateSendReadyLabel->hide();
}

CoinControlDialog::~CoinControlDialog()
{
    delete ui;
}

void CoinControlDialog::setModel(WalletModel *model)
{
    this->model = model;

    if(model && model->getOptionsModel())
    {
        // The consolidation input cap is a policy value fetched through the
        // wallet interface (no model exists at construction, so this and the
        // tooltip that shows it are set here rather than in the constructor).
        m_inputSelectionLimit = model->wallet().getMaxConsolidationInputs();
        ui->filterModePushButton->setToolTip(tr("Flips the filter mode between selecting inputs less than or equal to the "
                                                "provided value (<=) and greater than or equal to the provided value (>=). "
                                                "The filter also automatically limits the number of inputs to %1, in "
                                                "ascending order for <= and descending order for >=."
                                                ).arg(m_inputSelectionLimit));

        // The windowed selection model: registers its view, reconciles the
        // selection against the store, and seeds from the (possibly still
        // loading) snapshot. The tree renders disabled until the first
        // snapshot lands (loadingFinished).
        m_selection_model = new CoinSelectionModel(model, coinControl, GRC::VIEW_COIN_CONTROL, this);
#ifdef COINSELECTION_MODEL_TESTER
        // Dev-loop only: exercises the tree-model invariants (index/parent
        // round-trips, bracket consistency) on every mutation.
        new QAbstractItemModelTester(m_selection_model,
                                     QAbstractItemModelTester::FailureReportingMode::Fatal,
                                     m_selection_model);
#endif
        ui->treeView->setModel(m_selection_model);
        ui->treeView->setAlternatingRowColors(m_selection_model->displayMode() == GRC::CoinViewMode::Flat);

        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_CHECKBOX, 150);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_AMOUNT, 170);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_LABEL, 200);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_ADDRESS, 290);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_DATE, 110);
        ui->treeView->setColumnWidth(CoinSelectionModel::COLUMN_CONFIRMATIONS, 100);

        connect(ui->treeView, &CoinSelectionView::visibleIndexesChanged,
                m_selection_model, &CoinSelectionModel::onVisibleIndexes);
        connect(m_selection_model, &CoinSelectionModel::selectionChanged,
                this, &CoinControlDialog::modelSelectionChanged);
        connect(m_selection_model, &CoinSelectionModel::loadingFinished,
                this, &CoinControlDialog::modelLoadingFinished);

        // default view is sorted by amount desc
        ui->treeView->header()->setSortIndicator(CoinSelectionModel::COLUMN_AMOUNT, Qt::DescendingOrder);

        if (m_selection_model->isLoading()) {
            ui->treeView->setEnabled(false);
        } else {
            modelLoadingFinished();
        }

        CoinControlDialog::updateLabels(model, coinControl, payAmounts, this, m_fSubtractFeeFromAmount);
    }
}

void CoinControlDialog::modelLoadingFinished()
{
    ui->treeView->setEnabled(true);
    expandPartiallySelected();
    CoinControlDialog::updateLabels(model, coinControl, payAmounts, this, m_fSubtractFeeFromAmount);
}

void CoinControlDialog::modelSelectionChanged()
{
    CoinControlDialog::updateLabels(model, coinControl, payAmounts, this, m_fSubtractFeeFromAmount);
    showHideConsolidationReadyToSend();
}

void CoinControlDialog::expandPartiallySelected()
{
    if (!m_selection_model || m_selection_model->displayMode() != GRC::CoinViewMode::Tree) {
        return;
    }

    int budget = kAutoExpandRowBudget;
    const int rows = m_selection_model->rowCount();
    for (int i = 0; i < rows && budget > 0; ++i) {
        const QModelIndex idx = m_selection_model->index(i, CoinSelectionModel::COLUMN_CHECKBOX);
        const Qt::CheckState state = idx.data(Qt::CheckStateRole).value<Qt::CheckState>();
        if (state != Qt::PartiallyChecked) continue;

        // Charge the budget BEFORE expanding: expand() realizes the whole
        // group in one call, so checking afterwards would let a single
        // half-million-child group blow the cap it exists to enforce. The
        // group's child count is the server-side aggregate — no realization
        // needed to read it.
        const int children = m_selection_model->groupOutputCount(i);
        if (children > budget) break;
        budget -= children;

        ui->treeView->expand(idx.siblingAtColumn(0));
    }
}

// ok button
void CoinControlDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
        done(QDialog::Accepted); // closes the dialog

    if (m_consolidationAddress.second.size())
    {
        SendCoinsRecipient consolidationRecipient;

        qint64 amount = 0;
        bool parse_status = false;

        consolidationRecipient.label = m_consolidationAddress.first;
        consolidationRecipient.address = m_consolidationAddress.second;
        parse_status = BitcoinUnits::parse(model->getOptionsModel()->getDisplayUnit(),
                                           ui->coinControlAfterFeeLabel->text()
                                                .left(ui->coinControlAfterFeeLabel->text().indexOf(" ")),
                                           &amount);

        if (parse_status) consolidationRecipient.amount = amount;

        emit selectedConsolidationRecipientSignal(consolidationRecipient);
    }

    showHideConsolidationReadyToSend();
}

// (un)select all
void CoinControlDialog::buttonSelectAllClicked()
{
    if (!m_selection_model) return;

    // The legacy state machine, over the server-side bulk operation.
    m_selection_model->selectAll(m_ToState == Qt::Checked);

    if (m_ToState == Qt::Checked)
    {
        m_ToState = Qt::Unchecked;
    }
    else
    {
        m_ToState = Qt::Checked;
    }

    if (m_ToState == Qt::Checked)
    {
       ui->selectAllPushButton->setText(tr("Select All"));
    }
    else
    {
       ui->selectAllPushButton->setText(tr("Select None"));
    }

    // selectAll emitted selectionChanged -> labels refreshed there.
    showHideConsolidationReadyToSend();
}

void CoinControlDialog::maxMinOutputValueChanged()
{

    bool maxMinOutputValueValid = false;

    ui->maxMinOutputValue->value(&maxMinOutputValueValid);

    // If someone has put a value in the filter amount field, then consolidate should be disabled until the
    // filter button is pressed to apply the filter. If the field is empty, then the consolidation can work
    // without the filter application first, (i.e. consolidation is enabled), because the idea is to select
    // up to the m_inputSelectionLimit number of inputs either from smallest upward or largest downward by
    // following the <= or >= filter mode button. This shortcut is mainly for convenience.
    if (maxMinOutputValueValid)
    {
        ui->consolidateButton->setEnabled(false);
    }
    else
    {
        ui->consolidateButton->setEnabled(true);
    }

    showHideConsolidationReadyToSend();
}

void CoinControlDialog::buttonFilterModeClicked()
{
    if (m_FilterMode)
    {
        m_FilterMode = false;
        ui->filterModePushButton->setText(">=");
    }
    else
    {
        m_FilterMode = true;
        ui->filterModePushButton->setText("<=");
    }
}

void CoinControlDialog::buttonFilterClicked()
{
    // Don't limit the number of outputs for the filter only operation.
    filterInputsByValue(m_FilterMode, ui->maxMinOutputValue->value(), std::numeric_limits<unsigned int>::max());

    ui->consolidateButton->setEnabled(true);
    showHideConsolidationReadyToSend();
}

bool CoinControlDialog::filterInputsByValue(const bool& less, const CAmount& inputFilterValue,
                                            const unsigned int& inputSelectionLimit)
{
    if (!m_selection_model) return false;

    // The legacy prune-only semantics, relocated server-side: deselect
    // members failing the predicate, then keep only the inputSelectionLimit
    // smallest (less) / largest survivors. The model applies the returned
    // outpoint delta to coinControl and emits selectionChanged (which
    // refreshes the labels).
    return m_selection_model->applyValueFilter(less, inputFilterValue, inputSelectionLimit);
}

void CoinControlDialog::buttonConsolidateClicked()
{
    ConsolidateUnspentDialog consolidateUnspentDialog(this, m_inputSelectionLimit);

    std::map<QString, QString> addressList;

    bool culled_inputs = false;

    // Note that we are applying the filter here to limit the number of inputs only to ensure the m_inputSelectionLimit
    // input maximum is not exceeded for the purpose of consolidation.
    CAmount outputFilterValue = 0;

    outputFilterValue = m_FilterMode ? MAX_MONEY: 0;

    culled_inputs = filterInputsByValue(m_FilterMode, outputFilterValue, m_inputSelectionLimit);

    // The candidate destinations: groups with at least one direct (non-change)
    // output. This unifies the legacy tree/list-mode discrepancy to the
    // defensible semantics — a group consisting entirely of change walked
    // back to a spent address is not offered as a consolidation destination.
    for (const GRC::CoinGroupInfo& group : m_selection_model->groupDirectory())
    {
        if (group.direct_output_count <= 0) continue;
        const QString label = group.label.empty() ? tr("(no label)")
                                                  : QString::fromStdString(group.label);
        addressList[QString::fromStdString(group.address)] = label;
    }

    if (!addressList.empty()) consolidateUnspentDialog.SetAddressList(addressList);

    if (culled_inputs) consolidateUnspentDialog.SetOutputWarningVisible(true);

    connect(&consolidateUnspentDialog, &ConsolidateUnspentDialog::selectedConsolidationAddressSignal,
            this, &CoinControlDialog::selectedConsolidationAddressSlot);

    consolidateUnspentDialog.exec();
}

// context menu
void CoinControlDialog::showMenu(const QPoint &point)
{
    if (!m_selection_model) return;

    const QModelIndex index = ui->treeView->indexAt(point);
    if (index.isValid())
    {
        contextMenuIndex = index;

        // Copy Transaction ID applies only to output rows with a cached
        // record (group rows and placeholders have no single txid).
        copyTransactionHashAction->setEnabled(
            !m_selection_model->txHashTextAt(index).isEmpty());

        // show context menu
        contextMenu->exec(QCursor::pos());
    }
}

// The row the context menu was opened on, or an invalid index if the model
// dropped it while the menu's nested event loop ran (a drain can reset or
// remove rows under an open menu). Every copy action goes through this.
QModelIndex CoinControlDialog::contextMenuTarget() const
{
    if (!m_selection_model || !contextMenuIndex.isValid()) return QModelIndex();
    return QModelIndex(contextMenuIndex);
}

// context menu action: copy amount
void CoinControlDialog::copyAmount()
{
    const QModelIndex index = contextMenuTarget();
    if (!index.isValid()) return;
    QApplication::clipboard()->setText(m_selection_model->amountTextAt(index));
}

// context menu action: copy label
void CoinControlDialog::copyLabel()
{
    const QModelIndex index = contextMenuTarget();
    if (!index.isValid()) return;
    QApplication::clipboard()->setText(m_selection_model->labelAt(index));
}

// context menu action: copy address
void CoinControlDialog::copyAddress()
{
    const QModelIndex index = contextMenuTarget();
    if (!index.isValid()) return;
    QApplication::clipboard()->setText(m_selection_model->addressAt(index));
}

// context menu action: copy transaction id
void CoinControlDialog::copyTransactionHash()
{
    const QModelIndex index = contextMenuTarget();
    if (!index.isValid()) return;
    QApplication::clipboard()->setText(m_selection_model->txHashTextAt(index));
}

// copy label "Quantity" to clipboard
void CoinControlDialog::clipboardQuantity()
{
    QApplication::clipboard()->setText(ui->coinControlQuantityLabel->text());
}

// copy label "Amount" to clipboard
void CoinControlDialog::clipboardAmount()
{
    QString text = ui->coinControlAmountLabel->text().left(ui->coinControlAmountLabel->text().indexOf(" "));
    text.remove(BitcoinUnits::THIN_SPACE);
    QApplication::clipboard()->setText(text);
}

// copy label "Fee" to clipboard
void CoinControlDialog::clipboardFee()
{
    QString text = ui->coinControlFeeLabel->text().left(ui->coinControlFeeLabel->text().indexOf(" "));
    text.remove(BitcoinUnits::THIN_SPACE);
    QApplication::clipboard()->setText(text);
}

// copy label "After fee" to clipboard
void CoinControlDialog::clipboardAfterFee()
{
    QString text = ui->coinControlAfterFeeLabel->text().left(ui->coinControlAfterFeeLabel->text().indexOf(" "));
    text.remove(BitcoinUnits::THIN_SPACE);
    QApplication::clipboard()->setText(text);
}

// copy label "Bytes" to clipboard
void CoinControlDialog::clipboardBytes()
{
    QApplication::clipboard()->setText(ui->coinControlBytesLabel->text());
}

// copy label "Low output" to clipboard
void CoinControlDialog::clipboardLowOutput()
{
    QApplication::clipboard()->setText(ui->coinControlLowOutputLabel->text());
}

// copy label "Change" to clipboard
void CoinControlDialog::clipboardChange()
{
    QString text = ui->coinControlChangeLabel->text().left(ui->coinControlChangeLabel->text().indexOf(" "));
    text.remove(BitcoinUnits::THIN_SPACE);
    QApplication::clipboard()->setText(text);
}

// treeview: clicked on header
void CoinControlDialog::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == CoinSelectionModel::COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeView->header()->setSortIndicator(sortColumn, sortOrder);
        return;
    }

    if (sortColumn == logicalIndex)
    {
        sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder);
    }
    else
    {
        sortColumn = logicalIndex;
        // if amount,date,conf then default => desc, else default => asc
        sortOrder = ((sortColumn == CoinSelectionModel::COLUMN_AMOUNT
                      || sortColumn == CoinSelectionModel::COLUMN_DATE
                      || sortColumn == CoinSelectionModel::COLUMN_CONFIRMATIONS)
                         ? Qt::DescendingOrder : Qt::AscendingOrder);
    }

    m_selection_model->sort(sortColumn, sortOrder);
    ui->treeView->header()->setSortIndicator(sortColumn, sortOrder);
    expandPartiallySelected();
}

// toggle tree mode
void CoinControlDialog::treeModeRadioButton(bool checked)
{
    if (checked && m_selection_model)
    {
        ui->treeView->setAlternatingRowColors(false);
        m_selection_model->setDisplayMode(GRC::CoinViewMode::Tree);
        expandPartiallySelected();
    }
}

// toggle list mode
void CoinControlDialog::listModeRadioButton(bool checked)
{
    if (checked && m_selection_model)
    {
        ui->treeView->setAlternatingRowColors(true);
        m_selection_model->setDisplayMode(GRC::CoinViewMode::Flat);
    }
}

void CoinControlDialog::updateLabels(WalletModel *model,
                                     interfaces::WalletCoinControl *coinControl,
                                     QList<qint64>* payAmounts,
                                     QDialog* dialog,
                                     bool fSubtractFeeFromAmount)
{
    if (!model) return;

    // Gather the recipient amounts (the full list, including any zero entries,
    // which the byte estimate counts) and the pay total. All the fee/quantity
    // math -- byte sizing via pubkey compression, nTransactionFee/GetMinFee,
    // and the sub-CENT change absorption -- runs node-side in one call, so the
    // GUI holds no policy/consensus headers and just renders the summary.
    qint64 nPayAmount = 0;
    std::vector<int64_t> recipientAmounts;
    recipientAmounts.reserve(payAmounts->size());
    for (const qint64& amount : std::as_const(*payAmounts)) {
        nPayAmount += amount;
        recipientAmounts.push_back(amount);
    }

    const interfaces::CoinControlSummary summary =
        model->wallet().computeCoinControlSummary(*coinControl, recipientAmounts, fSubtractFeeFromAmount);

    // actually update labels
    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel())
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    QLabel *l1 = dialog->findChild<QLabel *>("coinControlQuantityLabel");
    QLabel *l2 = dialog->findChild<QLabel *>("coinControlAmountLabel");
    QLabel *l3 = dialog->findChild<QLabel *>("coinControlFeeLabel");
    QLabel *l4 = dialog->findChild<QLabel *>("coinControlAfterFeeLabel");
    QLabel *l5 = dialog->findChild<QLabel *>("coinControlBytesLabel");
    QLabel *l7 = dialog->findChild<QLabel *>("coinControlLowOutputLabel");
    QLabel *l8 = dialog->findChild<QLabel *>("coinControlChangeLabel");

    // enable/disable "low output" and "change"
    dialog->findChild<QLabel *>("coinControlLowOutputTextLabel")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel *>("coinControlLowOutputLabel")    ->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel *>("coinControlChangeTextLabel")   ->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel *>("coinControlChangeLabel")       ->setEnabled(nPayAmount > 0);

    // stats
    l1->setText(QString::number(summary.quantity));                                    // Quantity
    l2->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.amount));           // Amount
    l3->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.fee));              // Fee
    l4->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.after_fee));        // After Fee
    l5->setText(((summary.bytes > 0) ? "~" : "") + QString::number(summary.bytes));    // Bytes
    l7->setText((summary.low_output ? (summary.dust ? tr("DUST") : tr("yes")) : tr("no"))); // Low Output / Dust
    l8->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.change));           // Change

    // turn labels "red"
    l5->setStyleSheet((summary.bytes >= 10000) ? "color:red;" : "");                // Bytes >= 10000
    l7->setStyleSheet((summary.low_output) ? "color:red;" : "");                    // Low Output = "yes"
    l8->setStyleSheet((summary.change > 0 && summary.change < CENT) ? "color:red;" : ""); // Change < 0.01BTC

    // tool tips
    l5->setToolTip(tr("This label turns red, if the transaction size is bigger than 10000 bytes.\n\n This means a fee of at least %1 per kb is required.\n\n Can vary +/- 1 Byte per input.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)));
    l7->setToolTip(tr("This label turns red, if any recipient receives an amount smaller than %1.\n\n This means a fee of at least %2 is required. \n\n Amounts below 0.546 times the minimum relay fee are shown as DUST.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT), BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)));
    l8->setToolTip(tr("This label turns red, if the change is smaller than %1.\n\n This means a fee of at least %2 is required.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT), BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)));
    dialog->findChild<QLabel *>("coinControlBytesTextLabel")    ->setToolTip(l5->toolTip());
    dialog->findChild<QLabel *>("coinControlLowOutputTextLabel")->setToolTip(l7->toolTip());
    dialog->findChild<QLabel *>("coinControlChangeTextLabel")   ->setToolTip(l8->toolTip());

    // Insufficient funds
    QLabel *label = dialog->findChild<QLabel *>("coinControlInsuffFundsLabel");
    if (label)
        label->setVisible(summary.change < 0);
}

void CoinControlDialog::selectedConsolidationAddressSlot(std::pair<QString, QString> address)
{
    m_consolidationAddress = address;
    showHideConsolidationReadyToSend();
}

void CoinControlDialog::showHideConsolidationReadyToSend()
{
    if (m_consolidationAddress.second.size() && coinControl->HasSelected() && ui->consolidateButton->isEnabled())
    {
        // This is more expensive. Only do if it passes the first two conditions above. We want to check
        // and make sure that the number of inputs is less than m_inputSelectionLimit for consolidation purposes.
        if (coinControl->selected.size() <= m_inputSelectionLimit)
        {
            ui->consolidateSendReadyLabel->show();
        }
        else
        {
            ui->consolidateSendReadyLabel->hide();
        }
    }
    else
    {
        ui->consolidateSendReadyLabel->hide();
    }
}
