#include "coincontroldialog.h"
#include "consolidateunspentwizardselectinputspage.h"
#include "ui_consolidateunspentwizardselectinputspage.h"

#include "init.h"
#include "bitcoinunits.h"
#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "policy/policy.h"
#include "policy/fees.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "consolidateunspentdialog.h"

using namespace std;

ConsolidateUnspentWizardSelectInputsPage::ConsolidateUnspentWizardSelectInputsPage(QWidget *parent) :
    QWizardPage(parent),
    ui(new Ui::ConsolidateUnspentWizardSelectInputsPage)
{
    m_InputSelectionLimit = GetMaxInputsForConsolidationTxn();

    ui->setupUi(this);

    // toggle tree/list mode
    connect(ui->treeModeRadioButton, SIGNAL(toggled(bool)), this, SLOT(treeModeRadioButton(bool)));
    connect(ui->listModeRadioButton, SIGNAL(toggled(bool)), this, SLOT(listModeRadioButton(bool)));

    // click on checkbox
    connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(viewItemChanged(QTreeWidgetItem*, int)));

    // click on header
    ui->treeWidget->header()->setSectionsClickable(true);
    connect(ui->treeWidget->header(), SIGNAL(sectionClicked(int)), this, SLOT(headerSectionClicked(int)));

    // (un)select all
    connect(ui->selectAllPushButton, SIGNAL(clicked()), this, SLOT(buttonSelectAllClicked()));

    // filter/consolidate button interaction
    connect(ui->maxMinOutputValue, SIGNAL(textChanged()), this, SLOT(maxMinOutputValueChanged()));

    // filter mode
    connect(ui->filterModePushButton, SIGNAL(clicked()), this, SLOT(buttonFilterModeClicked()));

    // filter
    connect(ui->filterPushButton, SIGNAL(clicked()), this, SLOT(buttonFilterClicked()));

    ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, 150);
    ui->treeWidget->setColumnWidth(COLUMN_AMOUNT, 170);
    ui->treeWidget->setColumnWidth(COLUMN_LABEL, 200);
    ui->treeWidget->setColumnWidth(COLUMN_ADDRESS, 290);
    ui->treeWidget->setColumnWidth(COLUMN_DATE, 110);
    ui->treeWidget->setColumnWidth(COLUMN_CONFIRMATIONS, 100);
    ui->treeWidget->setColumnWidth(COLUMN_PRIORITY, 100);
    ui->treeWidget->setColumnHidden(COLUMN_TXHASH, true);         // store transacton hash in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_VOUT_INDEX, true);     // store vout index in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_AMOUNT_INT64, true);   // store amount int64_t in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_PRIORITY_INT64, true); // store priority int64_t in this column, but don't show it
    ui->treeWidget->setColumnHidden(COLUMN_CHANGE_BOOL, true);    // store change flag but don't show it

    // This is to provide a convenient way to populate the fields shown on the last page ("send" screen).
    registerField("quantityField", ui->quantityLabel, "text", "updateFieldsSignal()");
    registerField("feeField", ui->feeLabel, "text", "updateFieldsSignal()");
    registerField("afterFeeAmountField", ui->afterFeeLabel, "text", "updateFieldsSignal()");

    //This is used to control the disable/enable of the next button on this page.
    registerField("isCompleteSelectInputs*", ui->isCompleteCheckBox);

    // default view is sorted by amount desc
    sortView(COLUMN_AMOUNT_INT64, Qt::DescendingOrder);

    ui->outputLimitWarningIconLabel->setToolTip(tr("Note: The number of inputs selected for consolidation has been "
                                                 "limited to %1 to prevent a transaction failure due to too many "
                                                 "inputs.").arg(m_InputSelectionLimit));
    ui->outputLimitStopIconLabel->setToolTip(tr("Note: The number of inputs selected for consolidation is currently more "
                                                 "than the limit of %1. Please use the filter or manual selection to reduce "
                                                "the number of inputs to %1 or less to prevent a transaction failure due to "
                                                "too many inputs.").arg(m_InputSelectionLimit));

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
        updateView();
        updateLabels();
    }
}

void ConsolidateUnspentWizardSelectInputsPage::setCoinControl(CCoinControl *coinControl)
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
       ui->selectAllPushButton->setText("Select All");
    }
    else
    {
       ui->selectAllPushButton->setText("Select None");
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
            LogPrint(BCLog::LogFlags::QT, "INFO: %s: Culled input %u with value %f.",
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

    // Reenable update signals.
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
                                                    COLUMN_AMOUNT : (sortColumn == COLUMN_PRIORITY_INT64 ?
                                                                         COLUMN_PRIORITY : sortColumn)),
                                               sortOrder);
}

// treeview: clicked on header
void ConsolidateUnspentWizardSelectInputsPage::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeWidget->header()->setSortIndicator((sortColumn == COLUMN_AMOUNT_INT64 ?
                                                        COLUMN_AMOUNT : (sortColumn == COLUMN_PRIORITY_INT64 ?
                                                                             COLUMN_PRIORITY : sortColumn)),
                                                   sortOrder);
    }
    else
    {
        if (logicalIndex == COLUMN_AMOUNT) // sort by amount
            logicalIndex = COLUMN_AMOUNT_INT64;

        if (logicalIndex == COLUMN_PRIORITY) // sort by priority
            logicalIndex = COLUMN_PRIORITY_INT64;

        if (sortColumn == logicalIndex)
            sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder);
        else
        {
            sortColumn = logicalIndex;

            // if amount,date,conf,priority then default => desc, else default => asc
            sortOrder = ((sortColumn == COLUMN_AMOUNT_INT64 || sortColumn == COLUMN_PRIORITY_INT64
                          || sortColumn == COLUMN_DATE || sortColumn == COLUMN_CONFIRMATIONS) ?
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
        // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
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

    // nPayAmount
    qint64 nPayAmount = 0;
    CTransaction txDummy;
    for (const auto& amount: *payAmounts)
    {
        nPayAmount += amount;

        if (amount > 0)
        {
            CTxOut txout(amount, (CScript)vector<unsigned char>(24, 0));
            txDummy.vout.push_back(txout);
        }
    }

    QString sPriorityLabel = QString();
    int64_t nAmount = 0;
    int64_t nPayFee = 0;
    int64_t nAfterFee = 0;
    int64_t nChange = 0;
    unsigned int nBytes = 0;
    unsigned int nBytesInputs = 0;
    unsigned int nQuantity = 0;

    vector<COutPoint> vCoinControl;
    vector<COutput> vOutputs;
    coinControl->ListSelected(vCoinControl);
    model->getOutputs(vCoinControl, vOutputs);

    for (const auto& out : vOutputs)
    {
        // Quantity
        nQuantity++;

        // Amount
        nAmount += out.tx->vout[out.i].nValue;

        // Bytes
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
        {
            CPubKey pubkey;
            try {
                if (model->getPubKey(std::get<CKeyID>(address), pubkey))
                    nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
                else
                    nBytesInputs += 148; // in all error cases, simply assume 148 here
            } catch (const std::bad_variant_access&) {
                nBytesInputs += 148;
            }
        }
        else nBytesInputs += 148;
    }

    // calculation
    if (nQuantity > 0)
    {
        // Bytes - always assume +1 output for change here
        nBytes = nBytesInputs + ((payAmounts->size() > 0 ? payAmounts->size() + 1 : 2) * 34) + 10;

        // Fee
        int64_t nFee = nTransactionFee * (1 + (int64_t)nBytes / 1000);

        // Min Fee
        int64_t nMinFee = GetMinFee(txDummy, 1000, GMF_SEND, nBytes);

        nPayFee = max(nFee, nMinFee);

        if (nPayAmount > 0)
        {
            nChange = nAmount - nPayFee - nPayAmount;

            // if sub-cent change is required, the fee must be raised to at least CTransaction::nMinTxFee
            if (nPayFee < CENT && nChange > 0 && nChange < CENT)
            {
                if (nChange < CENT) // change < 0.01 => simply move all change to fees
                {
                    nPayFee = nChange;
                    nChange = 0;
                }
                else
                {
                    nChange = nChange + nPayFee - CENT;
                    nPayFee = CENT;
                }
            }

            if (nChange == 0) nBytes -= 34;
        }

        // after fee
        nAfterFee = nAmount - nPayFee;
        if (nAfterFee < 0) nAfterFee = 0;
    }

    // actually update labels
    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel()) nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    // stats
    ui->quantityLabel->setText(QString::number(nQuantity));                            // Quantity
    ui->feeLabel->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nPayFee));        // Fee
    ui->afterFeeLabel->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAfterFee)); // After Fee

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

    // This provids the trigger to update the fields from the labels, since they are QLabels and don't have appropriate
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
    QFlags<Qt::ItemFlag> flgTristate=Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;

    int nDisplayUnit = BitcoinUnits::BTC;

    if (model && model->getOptionsModel())
    {
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();
    }

    map<QString, vector<COutput>> mapCoins;
    model->listCoins(mapCoins);

    for (auto const& coins : mapCoins)
    {
        QTreeWidgetItem *itemWalletAddress = new QTreeWidgetItem();
        QString sWalletAddress = coins.first;
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
        double dPrioritySum = 0;
        int nChildren = 0;
        int nInputSum = 0;

        for (auto const& out : coins.second)
        {
            int nInputSize = 148; // 180 if uncompressed public key
            nSum += out.tx->vout[out.i].nValue;
            nChildren++;

            QTreeWidgetItem *itemOutput;
            if (treeMode)    itemOutput = new QTreeWidgetItem(itemWalletAddress);
            else             itemOutput = new QTreeWidgetItem(ui->treeWidget);
            itemOutput->setFlags(flgCheckbox);
            itemOutput->setCheckState(COLUMN_CHECKBOX,Qt::Unchecked);

            // address
            CTxDestination outputAddress;
            QString sAddress = "";
            if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, outputAddress))
            {
                sAddress = CBitcoinAddress(outputAddress).ToString().c_str();

                // if listMode or change => show bitcoin address. In tree mode, address is not shown again for direct wallet address outputs
                if (!treeMode || (!(sAddress == sWalletAddress)))
                    itemOutput->setText(COLUMN_ADDRESS, sAddress);

                CPubKey pubkey;
                try {
                    if (model->getPubKey(std::get<CKeyID>(outputAddress), pubkey) && !pubkey.IsCompressed())
                        nInputSize = 180;
                } catch (const std::bad_variant_access&) {}
            }

            // label
            if (!(sAddress == sWalletAddress)) // change
            {
                // tooltip from where the change comes from
                itemOutput->setToolTip(COLUMN_LABEL, tr("change from %1 (%2)").arg(sWalletLabel).arg(sWalletAddress));
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
            itemOutput->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, out.tx->vout[out.i].nValue));
            itemOutput->setText(COLUMN_AMOUNT_INT64, strPad(QString::number(out.tx->vout[out.i].nValue), 15, " ")); // padding so that sorting works correctly

            // date
            itemOutput->setText(COLUMN_DATE, QDateTime::fromTime_t(out.tx->GetTxTime()).toUTC().toString("yy-MM-dd hh:mm"));

            // immature PoS reward
            {
                // LOCK on cs_main must be taken for depth and maturity.
                LOCK(cs_main);

                if (out.tx->IsCoinStake() && out.tx->GetBlocksToMaturity() > 0 && out.tx->GetDepthInMainChain() > 0) {
                    itemOutput->setBackground(COLUMN_CONFIRMATIONS, Qt::red);
                    itemOutput->setDisabled(true);
                }
            }

            // confirmations
            itemOutput->setText(COLUMN_CONFIRMATIONS, strPad(QString::number(out.nDepth), 8, " "));

            // priority
            double dPriority = ((double)out.tx->vout[out.i].nValue  / (nInputSize + 78)) * (out.nDepth+1); // 78 = 2 * 34 + 10
            itemOutput->setText(COLUMN_PRIORITY, CoinControlDialog::getPriorityLabel(dPriority));
            itemOutput->setText(COLUMN_PRIORITY_INT64, strPad(QString::number((int64_t)dPriority), 20, " "));
            dPrioritySum += (double)out.tx->vout[out.i].nValue  * (out.nDepth+1);
            nInputSum    += nInputSize;

            // transaction hash
            uint256 txhash = out.tx->GetHash();
            itemOutput->setText(COLUMN_TXHASH, txhash.GetHex().c_str());

            // vout index
            itemOutput->setText(COLUMN_VOUT_INDEX, QString::number(out.i));

            // set checkbox
            if (coinControl->IsSelected(txhash, out.i))
            {
                itemOutput->setCheckState(COLUMN_CHECKBOX,Qt::Checked);
            }
        }

        // amount
        if (treeMode)
        {
            dPrioritySum = dPrioritySum / (nInputSum + 78);
            itemWalletAddress->setText(COLUMN_CHECKBOX, "(" + QString::number(nChildren) + ")");
            itemWalletAddress->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, nSum));
            itemWalletAddress->setText(COLUMN_AMOUNT_INT64, strPad(QString::number(nSum), 15, " "));
            itemWalletAddress->setText(COLUMN_PRIORITY, CoinControlDialog::getPriorityLabel(dPrioritySum));
            itemWalletAddress->setText(COLUMN_PRIORITY_INT64, strPad(QString::number((int64_t)dPrioritySum), 20, " "));
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
