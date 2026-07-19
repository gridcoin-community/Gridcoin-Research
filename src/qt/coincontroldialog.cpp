#include "coincontroldialog.h"
#include "ui_coincontroldialog.h"

#include "bitcoinunits.h"
#include "addresstablemodel.h"
#include "key_io.h"
#include "optionsmodel.h"
#include "consolidateunspentdialog.h"
#include "qt/decoration.h"

#include <set>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QCursor>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFlags>
#include <QIcon>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>

using namespace std;

CoinControlDialog::CoinControlDialog(QWidget* parent, interfaces::WalletCoinControl* coinControl, QList<qint64>* payAmounts,
                                     bool fSubtractFeeFromAmount)
               : QDialog(parent)
               , ui(new Ui::CoinControlDialog)
               , coinControl(coinControl)
               , payAmounts(payAmounts)
               , model(nullptr)
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
    connect(ui->treeWidget, &QWidget::customContextMenuRequested, this, &CoinControlDialog::showMenu);
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

    // click on checkbox
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &CoinControlDialog::viewItemChanged);

    // click on header
    ui->treeWidget->header()->setSectionsClickable(true);
    connect(ui->treeWidget->header(), &QHeaderView::sectionClicked, this, &CoinControlDialog::headerSectionClicked);

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

    ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, 150);
    ui->treeWidget->setColumnWidth(COLUMN_AMOUNT, 170);
    ui->treeWidget->setColumnWidth(COLUMN_LABEL, 200);
    ui->treeWidget->setColumnWidth(COLUMN_ADDRESS, 290);
    ui->treeWidget->setColumnWidth(COLUMN_DATE, 110);
    ui->treeWidget->setColumnWidth(COLUMN_CONFIRMATIONS, 100);
    ui->treeWidget->setColumnHidden(COLUMN_TXHASH, true);         // store transacton hash in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_VOUT_INDEX, true);     // store vout index in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_AMOUNT_INT64, true);   // store amount int64_t in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_CHANGE_BOOL, true);    // store change flag but don't show it

    ui->consolidateSendReadyLabel->hide();

    // default view is sorted by amount desc
    sortView(COLUMN_AMOUNT_INT64, Qt::DescendingOrder);
}

CoinControlDialog::~CoinControlDialog()
{
    delete ui;
}

void CoinControlDialog::setModel(WalletModel *model)
{
    this->model = model;

    if(model && model->getOptionsModel() && model->getAddressTableModel())
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

        updateView();
        CoinControlDialog::updateLabels(model, coinControl, payAmounts, this, m_fSubtractFeeFromAmount);
    }
}

// helper function str_pad
QString CoinControlDialog::strPad(QString s, int nPadLength, QString sPadding)
{
    while (s.length() < nPadLength)
        s = sPadding + s;

    return s;
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
    ui->treeWidget->setEnabled(false);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != m_ToState)
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, m_ToState);
    ui->treeWidget->setEnabled(true);

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

    CoinControlDialog::updateLabels(model, coinControl, payAmounts, this, m_fSubtractFeeFromAmount);
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
    // Disable generating update signals unnecessarily during this filter operation.
    ui->treeWidget->setEnabled(false);

    QTreeWidgetItemIterator iter(ui->treeWidget);

    // If less is true, then we are choosing the smallest inputs upward, and so the map comparator needs to be "less than".
    // If less is false, then we are choosing the largest inputs downward, and so the map comparator needs to be "greater
    // than".
    auto comp = [less](CAmount a, CAmount b)
    {
        if (less)
        {
            return (a < b);
        }
        else
        {
            return (a > b);
        }
    };

    std::multimap<CAmount, std::pair<QTreeWidgetItem*, COutPoint>, decltype(comp)> input_map(comp);

    bool culled_inputs = false;

    while (*iter)
    {
        CAmount input_value = (*iter)->text(COLUMN_AMOUNT_INT64).toLongLong();
        COutPoint outpoint(uint256S((*iter)->text(COLUMN_TXHASH).toStdString()), (*iter)->text(COLUMN_VOUT_INDEX).toUInt());

        if ((*iter)->checkState(COLUMN_CHECKBOX) == Qt::Checked)
        {
            if ((*iter)->text(COLUMN_TXHASH).length() == 64)
            {
                if ((less && input_value <= inputFilterValue) || (!less && input_value >= inputFilterValue))
                {
                    input_map.insert(std::make_pair(input_value, std::make_pair(*iter, outpoint)));
                }
                else
                {
                    (*iter)->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
                    coinControl->UnSelect(outpoint);
                }
            }
        }

        ++iter;
    }

    // The second loop is to limit the number of selected outputs to the inputCountLimit.
    unsigned int input_count = 0;

    for (auto& input : input_map)
    {
        if (input_count >= inputSelectionLimit)
        {
            LogPrint(BCLog::LogFlags::MISC, "INFO: %s: Culled input %u with value %f.",
                     __func__, input_count, (double) input.first / COIN);

            if (coinControl->IsSelected(input.second.second.hash, input.second.second.n))
            {
                input.second.first->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

                culled_inputs = true;
                coinControl->UnSelect(input.second.second);
            }
        }

        ++input_count;
    }

    // Re-enable update signals.
    ui->treeWidget->setEnabled(true);

    CoinControlDialog::updateLabels(model, coinControl, payAmounts, this, m_fSubtractFeeFromAmount);

    // If the number of inputs selected was limited, then true is returned.
    return culled_inputs;
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

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
    {
        QString label = ui->treeWidget->topLevelItem(i)->text(COLUMN_LABEL);
        QString address = ui->treeWidget->topLevelItem(i)->text(COLUMN_ADDRESS);
        QString change = ui->treeWidget-> topLevelItem(i)->text(COLUMN_CHANGE_BOOL);

        if (!change.toInt()) addressList[address] = label;
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
    QTreeWidgetItem *item = ui->treeWidget->itemAt(point);
    if(item)
    {
        contextMenuItem = item;

        // disable some items (like Copy Transaction ID, lock, unlock) for tree roots in context menu
        if (item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means it is a child node, so it is not a parent node in tree mode)
        {
            copyTransactionHashAction->setEnabled(true);
        }
        else // this means click on parent node in tree mode -> disable all
        {
            copyTransactionHashAction->setEnabled(false);
        }

        // show context menu
        contextMenu->exec(QCursor::pos());
    }
}

// context menu action: copy amount
void CoinControlDialog::copyAmount()
{
    QApplication::clipboard()->setText(contextMenuItem->text(COLUMN_AMOUNT));
}

// context menu action: copy label
void CoinControlDialog::copyLabel()
{
    if (ui->treeModeRadioButton->isChecked() && contextMenuItem->text(COLUMN_LABEL).length() == 0 && contextMenuItem->parent())
        QApplication::clipboard()->setText(contextMenuItem->parent()->text(COLUMN_LABEL));
    else
        QApplication::clipboard()->setText(contextMenuItem->text(COLUMN_LABEL));
}

// context menu action: copy address
void CoinControlDialog::copyAddress()
{
    if (ui->treeModeRadioButton->isChecked() && contextMenuItem->text(COLUMN_ADDRESS).length() == 0 && contextMenuItem->parent())
        QApplication::clipboard()->setText(contextMenuItem->parent()->text(COLUMN_ADDRESS));
    else
        QApplication::clipboard()->setText(contextMenuItem->text(COLUMN_ADDRESS));
}

// context menu action: copy transaction id
void CoinControlDialog::copyTransactionHash()
{
    QApplication::clipboard()->setText(contextMenuItem->text(COLUMN_TXHASH));
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

// treeview: sort
void CoinControlDialog::sortView(int column, Qt::SortOrder order)
{
    sortColumn = column;
    sortOrder = order;
    ui->treeWidget->sortItems(column, order);
    ui->treeWidget->header()->setSortIndicator((sortColumn == COLUMN_AMOUNT_INT64 ? COLUMN_AMOUNT : sortColumn), sortOrder);
}

// treeview: clicked on header
void CoinControlDialog::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeWidget->header()->setSortIndicator((sortColumn == COLUMN_AMOUNT_INT64 ? COLUMN_AMOUNT : sortColumn), sortOrder);
    }
    else
    {
        if (logicalIndex == COLUMN_AMOUNT) // sort by amount
            logicalIndex = COLUMN_AMOUNT_INT64;

        if (sortColumn == logicalIndex)
            sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder);
        else
        {
            sortColumn = logicalIndex;
            sortOrder = ((sortColumn == COLUMN_AMOUNT_INT64 || sortColumn == COLUMN_DATE || sortColumn == COLUMN_CONFIRMATIONS) ? Qt::DescendingOrder : Qt::AscendingOrder); // if amount,date,conf then default => desc, else default => asc
        }

        sortView(sortColumn, sortOrder);
    }
}

// toggle tree mode
void CoinControlDialog::treeModeRadioButton(bool checked)
{
    if (checked && model)
        updateView();
}

// toggle list mode
void CoinControlDialog::listModeRadioButton(bool checked)
{
    if (checked && model)
        updateView();
}

// checkbox clicked by user
void CoinControlDialog::viewItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == COLUMN_CHECKBOX && item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means it is a child node, so it is not a parent node in tree mode)
    {
        COutPoint outpt(uint256S(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt());

        if (item->checkState(COLUMN_CHECKBOX) == Qt::Unchecked)
            coinControl->UnSelect(outpt);
        else if (item->isDisabled()) // locked (this happens if "check all" through parent node)
            item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
        else
            coinControl->Select(outpt);

        // selection changed -> update labels
        if (ui->treeWidget->isEnabled()) // do not update on every click for (un)select all
        {
            CoinControlDialog::updateLabels(model, coinControl, payAmounts, this, m_fSubtractFeeFromAmount);
        }
    }

    showHideConsolidationReadyToSend();
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

void CoinControlDialog::updateView()
{
    bool treeMode = ui->treeModeRadioButton->isChecked();

    ui->treeWidget->clear();
    ui->treeWidget->setEnabled(false); // performance, otherwise updateLabels would be called for every checked checkbox
    ui->treeWidget->setAlternatingRowColors(!treeMode);
    QFlags<Qt::ItemFlag> flgCheckbox=Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    QFlags<Qt::ItemFlag> flgTristate=Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate;

    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel())
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    const std::map<std::string, std::vector<interfaces::WalletOutput>> mapCoins = model->listCoins();

    // Reconcile the selection against the currently-available coins: prune any
    // selected outpoint that no longer exists (e.g. a coin the wallet staked
    // out from under the selection). Keeps the fee/quantity display honest; the
    // send path re-validates node-side regardless.
    {
        std::set<COutPoint> available;
        for (auto const& coins : mapCoins)
            for (auto const& out : coins.second)
                available.insert(out.outpoint);

        for (auto it = coinControl->selected.begin(); it != coinControl->selected.end(); )
        {
            if (available.count(*it) == 0)
                it = coinControl->selected.erase(it);
            else
                ++it;
        }
    }

    for (auto const& coins : mapCoins)
    {
        QTreeWidgetItem *itemWalletAddress = new QTreeWidgetItem();
        QString sWalletAddress = QString::fromStdString(coins.first);
        QString sWalletLabel = "";
        if (model->getAddressTableModel())
            sWalletLabel = model->getAddressTableModel()->labelForAddress(sWalletAddress);
        if (sWalletLabel.length() == 0)
            sWalletLabel = tr("(no label)");

        if (treeMode)
        {
            // wallet address
            ui->treeWidget->addTopLevelItem(itemWalletAddress);

            itemWalletAddress->setFlags(flgTristate);
            itemWalletAddress->setCheckState(COLUMN_CHECKBOX,Qt::Unchecked);

            // label
            itemWalletAddress->setText(COLUMN_LABEL, sWalletLabel);

            // address
            itemWalletAddress->setText(COLUMN_ADDRESS, sWalletAddress);
        }

        int64_t nSum = 0;
        int nChildren = 0;
        for (auto const& out : coins.second)
        {
            nSum += out.amount;
            nChildren++;

            QTreeWidgetItem *itemOutput;
            if (treeMode)    itemOutput = new QTreeWidgetItem(itemWalletAddress);
            else             itemOutput = new QTreeWidgetItem(ui->treeWidget);
            itemOutput->setFlags(flgCheckbox);
            itemOutput->setCheckState(COLUMN_CHECKBOX,Qt::Unchecked);

            // address
            QString sAddress = QString::fromStdString(out.address);
            if (!sAddress.isEmpty())
            {
                // if listMode or change => show bitcoin address. In tree mode, address is not shown again for direct wallet address outputs
                if (!treeMode || (!(sAddress == sWalletAddress)))
                    itemOutput->setText(COLUMN_ADDRESS, sAddress);
            }

            // label
            if (!(sAddress == sWalletAddress)) // change
            {
                // tooltip from where the change comes from
                itemOutput->setToolTip(COLUMN_LABEL, tr("change from %1 (%2)").arg(sWalletLabel, sWalletAddress));
                itemOutput->setText(COLUMN_LABEL, tr("(change)"));
                itemOutput->setText(COLUMN_CHANGE_BOOL, QString::number(1));
            }
            else if (!treeMode)
            {
                QString sLabel = "";
                if (model->getAddressTableModel())
                    sLabel = model->getAddressTableModel()->labelForAddress(sAddress);
                if (sLabel.length() == 0)
                    sLabel = tr("(no label)");
                itemOutput->setText(COLUMN_LABEL, sLabel);
            }

            // amount
            itemOutput->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, out.amount));
            itemOutput->setText(COLUMN_AMOUNT_INT64, strPad(QString::number(out.amount), 15, " ")); // padding so that sorting works correctly

            // date
            itemOutput->setText(COLUMN_DATE, QDateTime::fromSecsSinceEpoch(out.time).toUTC().toString("yy-MM-dd hh:mm"));

            // immature PoS reward — flagged node-side (the maturity check
            // needs cs_main, which no longer belongs on the GUI thread)
            if (out.immature) {
                itemOutput->setBackground(COLUMN_CONFIRMATIONS, Qt::red);
                itemOutput->setDisabled(true);
            }

            // confirmations
            itemOutput->setText(COLUMN_CONFIRMATIONS, strPad(QString::number(out.depth), 8, " "));

            // transaction hash
            uint256 txhash = out.outpoint.hash;
            itemOutput->setText(COLUMN_TXHASH, txhash.GetHex().c_str());

            // vout index
            itemOutput->setText(COLUMN_VOUT_INDEX, QString::number(out.outpoint.n));

            // set checkbox
            if (coinControl->IsSelected(txhash, out.outpoint.n))
                itemOutput->setCheckState(COLUMN_CHECKBOX,Qt::Checked);
        }

        // amount
        if (treeMode)
        {
            itemWalletAddress->setText(COLUMN_CHECKBOX, "(" + QString::number(nChildren) + ")");
            itemWalletAddress->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, nSum));
            itemWalletAddress->setText(COLUMN_AMOUNT_INT64, strPad(QString::number(nSum), 15, " "));
        }
    }

    // expand all partially selected
    if (treeMode)
    {
        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked)
                ui->treeWidget->topLevelItem(i)->setExpanded(true);
    }

    // sort view
    sortView(sortColumn, sortOrder);
    ui->treeWidget->setEnabled(true);
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
