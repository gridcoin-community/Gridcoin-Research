#include "coincontroldialog.h"
#include "qt/guilog.h"
#include "consolidateunspentwizardselectinputspage.h"
#include "ui_consolidateunspentwizardselectinputspage.h"

#include "bitcoinunits.h"
#include "addresstablemodel.h"
#include "key_io.h"
#include "optionsmodel.h"
#include "consolidateunspentdialog.h"

#include <set>

using namespace std;

ConsolidateUnspentWizardSelectInputsPage::ConsolidateUnspentWizardSelectInputsPage(QWidget *parent) :
    QWizardPage(parent),
    ui(new Ui::ConsolidateUnspentWizardSelectInputsPage)
{
    ui->setupUi(this);

    // toggle tree/list mode
    connect(ui->treeModeRadioButton, &QRadioButton::toggled, this, &ConsolidateUnspentWizardSelectInputsPage::treeModeRadioButton);
    connect(ui->listModeRadioButton, &QRadioButton::toggled, this, &ConsolidateUnspentWizardSelectInputsPage::listModeRadioButton);

    // click on checkbox
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &ConsolidateUnspentWizardSelectInputsPage::viewItemChanged);

    // click on header
    ui->treeWidget->header()->setSectionsClickable(true);
    connect(ui->treeWidget->header(), &QHeaderView::sectionClicked, this, &ConsolidateUnspentWizardSelectInputsPage::headerSectionClicked);

    // (un)select all
    connect(ui->selectAllPushButton, &QPushButton::clicked, this, &ConsolidateUnspentWizardSelectInputsPage::buttonSelectAllClicked);

    // filter/consolidate button interaction
    connect(ui->maxMinOutputValue, &BitcoinAmountField::textChanged, this, &ConsolidateUnspentWizardSelectInputsPage::maxMinOutputValueChanged);

    // filter mode
    connect(ui->filterModePushButton, &QPushButton::clicked, this, &ConsolidateUnspentWizardSelectInputsPage::buttonFilterModeClicked);

    // filter
    connect(ui->filterPushButton, &QPushButton::clicked, this, &ConsolidateUnspentWizardSelectInputsPage::buttonFilterClicked);

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

    // This is to provide a convenient way to populate the fields shown on the last page ("send" screen).
    registerField("quantityField", ui->quantityLabel, "text", "updateFieldsSignal()");
    registerField("feeField", ui->feeLabel, "text", "updateFieldsSignal()");
    registerField("afterFeeAmountField", ui->afterFeeLabel, "text", "updateFieldsSignal()");

    //This is used to control the disable/enable of the next button on this page.
    registerField("isCompleteSelectInputs*", ui->isCompleteCheckBox);

    // default view is sorted by amount desc
    sortView(COLUMN_AMOUNT_INT64, Qt::DescendingOrder);

    // The tooltips that show m_InputSelectionLimit are set in setModel(), once
    // the value is available from the wallet interface (no model at construction).

    ui->outputLimitWarningIconLabel->setVisible(false);
    ui->outputLimitStopIconLabel->setVisible(false);

    ui->isCompleteCheckBox->hide();
}

ConsolidateUnspentWizardSelectInputsPage::~ConsolidateUnspentWizardSelectInputsPage()
{
    delete ui;
}

void ConsolidateUnspentWizardSelectInputsPage::setModel(WalletModel *model)
{
    this->model = model;

    if (model && model->getOptionsModel() && model->getAddressTableModel() && coinControl != nullptr)
    {
        // The consolidation input cap is a policy value from the wallet
        // interface; set it and the tooltips that show it here (no model exists
        // at construction).
        m_InputSelectionLimit = model->wallet().getMaxConsolidationInputs();
        ui->outputLimitWarningIconLabel->setToolTip(tr("Note: The number of inputs selected for consolidation has been "
                                                     "limited to %1 to prevent a transaction failure due to too many "
                                                     "inputs.").arg(m_InputSelectionLimit));
        ui->outputLimitStopIconLabel->setToolTip(tr("Note: The number of inputs selected for consolidation is currently more "
                                                     "than the limit of %1. Please use the filter or manual selection to reduce "
                                                    "the number of inputs to %1 or less to prevent a transaction failure due to "
                                                    "too many inputs.").arg(m_InputSelectionLimit));

        updateView();
        updateLabels();
    }
}

void ConsolidateUnspentWizardSelectInputsPage::setCoinControl(interfaces::WalletCoinControl *coinControl)
{
    this->coinControl = coinControl;
}

void ConsolidateUnspentWizardSelectInputsPage::setPayAmounts(QList<qint64> *payAmounts)
{
    this->payAmounts = payAmounts;
}

// helper function str_pad
QString ConsolidateUnspentWizardSelectInputsPage::strPad(QString s, int nPadLength, QString sPadding)
{
    while (s.length() < nPadLength)
        s = sPadding + s;

    return s;
}

// (un)select all
void ConsolidateUnspentWizardSelectInputsPage::buttonSelectAllClicked()
{
    m_InputSelectionLimitedByFilter = false;

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

    updateLabels();
}

void ConsolidateUnspentWizardSelectInputsPage::maxMinOutputValueChanged()
{
    ui->maxMinOutputValue->value(&m_FilterValueValid);
}

void ConsolidateUnspentWizardSelectInputsPage::buttonFilterModeClicked()
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

void ConsolidateUnspentWizardSelectInputsPage::buttonFilterClicked()
{
    m_ViewItemsChangedViaFilter = true;

    m_InputSelectionLimitedByFilter = filterInputsByValue(m_FilterMode, ui->maxMinOutputValue->value(), m_InputSelectionLimit);

    updateLabels();

    m_ViewItemsChangedViaFilter = false;
}

bool ConsolidateUnspentWizardSelectInputsPage::filterInputsByValue(const bool& less, const CAmount& inputFilterValue,
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
            GUILogPrint(GUILogCategory::QT, "INFO: %s: Culled input %u with value %f.",
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

    // If the number of inputs selected was limited, then true is returned.
    return culled_inputs;
}

// treeview: sort
void ConsolidateUnspentWizardSelectInputsPage::sortView(int column, Qt::SortOrder order)
{
    sortColumn = column;
    sortOrder = order;
    ui->treeWidget->sortItems(column, order);
    ui->treeWidget->header()->setSortIndicator((sortColumn == COLUMN_AMOUNT_INT64 ?
                                                    COLUMN_AMOUNT : sortColumn),
                                               sortOrder);
}

// treeview: clicked on header
void ConsolidateUnspentWizardSelectInputsPage::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeWidget->header()->setSortIndicator((sortColumn == COLUMN_AMOUNT_INT64 ?
                                                        COLUMN_AMOUNT : sortColumn),
                                                   sortOrder);
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

            // if amount,date,conf then default => desc, else default => asc
            sortOrder = ((sortColumn == COLUMN_AMOUNT_INT64 || sortColumn == COLUMN_DATE || sortColumn == COLUMN_CONFIRMATIONS) ?
                             Qt::DescendingOrder : Qt::AscendingOrder);
        }

        sortView(sortColumn, sortOrder);
    }
}


// toggle tree mode
void ConsolidateUnspentWizardSelectInputsPage::treeModeRadioButton(bool checked)
{
    if (checked && model)
        updateView();
}

// toggle list mode
void ConsolidateUnspentWizardSelectInputsPage::listModeRadioButton(bool checked)
{
    if (checked && model)
        updateView();
}

// checkbox clicked by user
void ConsolidateUnspentWizardSelectInputsPage::viewItemChanged(QTreeWidgetItem* item, int column)
{
    if (!m_ViewItemsChangedViaFilter) m_InputSelectionLimitedByFilter = false;

    if (column == COLUMN_CHECKBOX)
    {
        // transaction hash is 64 characters (this means it is a child node, so it is not a parent node in tree mode)
        if (item->text(COLUMN_TXHASH).length() == 64)
        {
            COutPoint outpt(uint256S(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt());

            if (item->checkState(COLUMN_CHECKBOX) == Qt::Unchecked)
            {
                coinControl->UnSelect(outpt);
            }
            else if (item->isDisabled()) // locked (this happens if "check all" through parent node)
            {
                item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
            }
            else
            {
                coinControl->Select(outpt);
            }
        }

        // selection changed -> update labels
        if (ui->treeWidget->isEnabled())
        {
            // do not update on every click for (un)select all
            updateLabels();
        }
    }
}

void ConsolidateUnspentWizardSelectInputsPage::updateLabels()
{
    if (!model) return;

    // Gather the recipient amounts; all fee/quantity math (byte sizing via
    // pubkey compression, nTransactionFee/GetMinFee, sub-CENT change absorption)
    // runs node-side in one call, so the wizard does no fee math and holds no
    // policy/consensus headers.
    std::vector<int64_t> recipientAmounts;
    recipientAmounts.reserve(payAmounts->size());
    for (const auto& amount : std::as_const(*payAmounts))
    {
        recipientAmounts.push_back(amount);
    }

    const interfaces::CoinControlSummary summary =
        model->wallet().computeCoinControlSummary(*coinControl, recipientAmounts, /*subtract_fee_from_amount=*/false);

    const unsigned int nQuantity = static_cast<unsigned int>(summary.quantity);

    // actually update labels
    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel()) nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    // stats
    ui->quantityLabel->setText(QString::number(summary.quantity));                        // Quantity
    ui->feeLabel->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.fee));       // Fee
    ui->afterFeeLabel->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, summary.after_fee)); // After Fee

    std::map<QString, QString> addressList;
    QString defaultAddress;
    unsigned int numberAddressesWhereOutputsChecked = 0;

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
    {
        QString label = ui->treeWidget->topLevelItem(i)->text(COLUMN_LABEL);
        QString address = ui->treeWidget->topLevelItem(i)->text(COLUMN_ADDRESS);
        QString change = ui->treeWidget-> topLevelItem(i)->text(COLUMN_CHANGE_BOOL);

        Qt::CheckState state = ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX);

        // If a not unchecked top level item is not a change address and it results in an insert into the m_AddressList
        if (!change.toInt() && addressList.insert(std::make_pair(address, label)).second)
        {
            if (state == Qt::Checked || state == Qt::PartiallyChecked)
            {
                defaultAddress = label;

                ++numberAddressesWhereOutputsChecked;
            }
        }
    }

    if (!addressList.empty()) emit setAddressListSignal(addressList);

    // This covers the 0 case too, where the default address will be an empty QString.
    if (numberAddressesWhereOutputsChecked < 2)
    {
        // This will be an empty QString if the numberAddressesWhereOutputsChecked equals 0. It will be
        // the above defaultAddress if numberAddressesWhereOutputsChecked equals 1.
        emit setDefaultAddressSignal(defaultAddress);
    }
    else
    {
        // If numberAddressesWhereOutputsChecked is 2 or greater, then clear the default address (i.e. set to
        // empty QString.
        emit setDefaultAddressSignal(QString());
    }

    // This provides the trigger to update the fields from the labels, since they are QLabels and don't have appropriate
    // internal signals.
    emit updateFieldsSignal();

    if (nQuantity < 2)
    {
        SetOutputWarningStop(InputStatus::INSUFFICIENT_OUTPUTS);
    }
    else if (nQuantity < m_InputSelectionLimit
             || (nQuantity == m_InputSelectionLimit && !m_InputSelectionLimitedByFilter))
    {
        SetOutputWarningStop(InputStatus::NORMAL);
    }
    else if (nQuantity == m_InputSelectionLimit && m_InputSelectionLimitedByFilter)
    {
        SetOutputWarningStop(InputStatus::WARNING);
    }
    else if (nQuantity > m_InputSelectionLimit)
    {
        SetOutputWarningStop(InputStatus::STOP);
    }
}

void ConsolidateUnspentWizardSelectInputsPage::updateView()
{
    bool treeMode = ui->treeModeRadioButton->isChecked();

    ui->treeWidget->clear();
    ui->treeWidget->setEnabled(false); // performance, otherwise updateLabels would be called for every checked checkbox
    ui->treeWidget->setAlternatingRowColors(!treeMode);
    QFlags<Qt::ItemFlag> flgCheckbox=Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    QFlags<Qt::ItemFlag> flgTristate=Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate;

    int nDisplayUnit = BitcoinUnits::BTC;

    if (model && model->getOptionsModel())
    {
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();
    }

    const std::map<std::string, std::vector<interfaces::WalletOutput>> mapCoins = model->listCoins();

    // Reconcile the selection against the currently-available coins: keep only
    // the selected outpoints that still exist (e.g. drop a coin the wallet
    // staked out from under the selection). Scan the already-fetched coin list
    // once, testing membership against the usually-small selection, so we never
    // materialize a set of every outpoint (important on large wallets). The
    // send path re-validates node-side regardless.
    {
        std::set<COutPoint> still_selected;
        for (auto const& coins : mapCoins)
            for (auto const& out : coins.second)
                if (coinControl->selected.count(out.outpoint))
                    still_selected.insert(out.outpoint);

        coinControl->selected.swap(still_selected);
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
            {
                itemOutput->setCheckState(COLUMN_CHECKBOX,Qt::Checked);
            }
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

void ConsolidateUnspentWizardSelectInputsPage::SetOutputWarningStop(InputStatus input_status)
{
    switch (input_status)
    {
    case InputStatus::INSUFFICIENT_OUTPUTS:
        ui->outputLimitWarningIconLabel->setVisible(false);
        ui->outputLimitStopIconLabel->setVisible(false);
        ui->isCompleteCheckBox->setChecked(false);
        break;
    case InputStatus::NORMAL:
        ui->outputLimitWarningIconLabel->setVisible(false);
        ui->outputLimitStopIconLabel->setVisible(false);
        ui->isCompleteCheckBox->setChecked(true);
        break;
    case InputStatus::WARNING:
        ui->outputLimitWarningIconLabel->setVisible(true);
        ui->outputLimitStopIconLabel->setVisible(false);
        ui->isCompleteCheckBox->setChecked(true);
        break;
    case InputStatus::STOP:
        ui->outputLimitWarningIconLabel->setVisible(false);
        ui->outputLimitStopIconLabel->setVisible(true);
        ui->isCompleteCheckBox->setChecked(false);
    }
}
