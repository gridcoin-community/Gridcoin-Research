#include "coincontroldialog.h"
#include "ui_coincontroldialog.h"

#include "init.h"
#include "bitcoinunits.h"
#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "policy/policy.h"
#include "policy/fees.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "consolidateunspentdialog.h"

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

CoinControlDialog::CoinControlDialog(QWidget *parent, CCoinControl *coinControl, QList<qint64> *payAmounts) :
    QDialog(parent),
    m_inputSelectionLimit(GetMaxInputsForConsolidationTxn()),
    ui(new Ui::CoinControlDialog),
    coinControl(coinControl),
    payAmounts(payAmounts),
    model(0)
{
    assert(coinControl != nullptr && payAmounts != nullptr);

    ui->setupUi(this);

    // context menu actions
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
             copyTransactionHashAction = new QAction(tr("Copy transaction ID"), this);  // we need to enable/disable this
             //lockAction = new QAction(tr("Lock unspent"), this);                        // we need to enable/disable this
             //unlockAction = new QAction(tr("Unlock unspent"), this);                    // we need to enable/disable this

    // context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTransactionHashAction);
    //contextMenu->addSeparator();
    //contextMenu->addAction(lockAction);
    //contextMenu->addAction(unlockAction);

    // context menu signals
    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(copyTransactionHashAction, SIGNAL(triggered()), this, SLOT(copyTransactionHash()));
    //connect(lockAction, SIGNAL(triggered()), this, SLOT(lockCoin()));
    //connect(unlockAction, SIGNAL(triggered()), this, SLOT(unlockCoin()));

    // clipboard actions
    QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction *clipboardPriorityAction = new QAction(tr("Copy priority"), this);
    QAction *clipboardLowOutputAction = new QAction(tr("Copy low output"), this);
    QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);

    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(clipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(clipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(clipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(clipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(clipboardBytes()));
    connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(clipboardPriority()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(clipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(clipboardChange()));

    ui->coinControlQuantityLabel->addAction(clipboardQuantityAction);
    ui->coinControlAmountLabel->addAction(clipboardAmountAction);
    ui->coinControlFeeLabel->addAction(clipboardFeeAction);
    ui->coinControlAfterFeeLabel->addAction(clipboardAfterFeeAction);
    ui->coinControlBytesLabel->addAction(clipboardBytesAction);
    ui->coinControlPriorityLabel->addAction(clipboardPriorityAction);
    ui->coinControlLowOutputLabel->addAction(clipboardLowOutputAction);
    ui->coinControlChangeLabel->addAction(clipboardChangeAction);

    // toggle tree/list mode
    connect(ui->treeModeRadioButton, SIGNAL(toggled(bool)), this, SLOT(treeModeRadioButton(bool)));
    connect(ui->listModeRadioButton, SIGNAL(toggled(bool)), this, SLOT(listModeRadioButton(bool)));

    // click on checkbox
    connect(ui->treeWidget, SIGNAL(itemChanged( QTreeWidgetItem*, int)), this, SLOT(viewItemChanged( QTreeWidgetItem*, int)));

    // click on header
    ui->treeWidget->header()->setSectionsClickable(true);
    connect(ui->treeWidget->header(), SIGNAL(sectionClicked(int)), this, SLOT(headerSectionClicked(int)));

    // ok button
    connect(ui->buttonBox, SIGNAL(clicked( QAbstractButton*)), this, SLOT(buttonBoxClicked(QAbstractButton*)));

    // (un)select all
    connect(ui->selectAllPushButton, SIGNAL(clicked()), this, SLOT(buttonSelectAllClicked()));

    // filter/consolidate button interaction
    connect(ui->maxMinOutputValue, SIGNAL(textChanged()), this, SLOT(maxMinOutputValueChanged()));

    // filter mode
    connect(ui->filterModePushButton, SIGNAL(clicked()), this, SLOT(buttonFilterModeClicked()));

    // filter
    connect(ui->filterPushButton, SIGNAL(clicked()), this, SLOT(buttonFilterClicked()));

    // consolidate
    connect(ui->consolidateButton, SIGNAL(clicked()), this, SLOT(buttonConsolidateClicked()));

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

    ui->filterModePushButton->setToolTip(tr("Flips the filter mode between selecting inputs less than or equal to the "
                                            "provided value (<=) and greater than or equal to the provided value (>=). "
                                            "The filter also automatically limits the number of inputs to %1, in "
                                            "ascending order for <= and descending order for >=."
                                            ).arg(m_inputSelectionLimit));

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
        updateView();
        //updateLabelLocked();
        CoinControlDialog::updateLabels(model, coinControl, payAmounts, this);
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
       ui->selectAllPushButton->setText("Select All");
    }
    else
    {
       ui->selectAllPushButton->setText("Select None");
    }

    CoinControlDialog::updateLabels(model, coinControl, payAmounts, this);
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

    // Reenable update signals.
    ui->treeWidget->setEnabled(true);

    CoinControlDialog::updateLabels(model, coinControl, payAmounts, this);

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

    connect(&consolidateUnspentDialog, SIGNAL(selectedConsolidationAddressSignal(std::pair<QString, QString>)),
            this, SLOT(selectedConsolidationAddressSlot(std::pair<QString, QString>)));

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
        if (item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
        {
            copyTransactionHashAction->setEnabled(true);
            //if (model->isLockedCoin(uint256(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt()))
            //{
            //    lockAction->setEnabled(false);
            //    unlockAction->setEnabled(true);
            //}
            //else
            //{
            //    lockAction->setEnabled(true);
            //    unlockAction->setEnabled(false);
            //}
        }
        else // this means click on parent node in tree mode -> disable all
        {
            copyTransactionHashAction->setEnabled(false);
            //lockAction->setEnabled(false);
            //unlockAction->setEnabled(false);
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

// context menu action: lock coin
/*void CoinControlDialog::lockCoin()
{
    if (contextMenuItem->checkState(COLUMN_CHECKBOX) == Qt::Checked)
        contextMenuItem->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

    COutPoint outpt(uint256(contextMenuItem->text(COLUMN_TXHASH).toStdString()), contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
    model->lockCoin(outpt);
    contextMenuItem->setDisabled(true);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, QIcon(":/icons/lock_closed"));
    updateLabelLocked();
}*/

// context menu action: unlock coin
/*void CoinControlDialog::unlockCoin()
{
    COutPoint outpt(uint256(contextMenuItem->text(COLUMN_TXHASH).toStdString()), contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
    model->unlockCoin(outpt);
    contextMenuItem->setDisabled(false);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, QIcon());
    updateLabelLocked();
}*/

// copy label "Quantity" to clipboard
void CoinControlDialog::clipboardQuantity()
{
    QApplication::clipboard()->setText(ui->coinControlQuantityLabel->text());
}

// copy label "Amount" to clipboard
void CoinControlDialog::clipboardAmount()
{
    QApplication::clipboard()->setText(ui->coinControlAmountLabel->text().left(ui->coinControlAmountLabel->text().indexOf(" ")));
}

// copy label "Fee" to clipboard
void CoinControlDialog::clipboardFee()
{
    QApplication::clipboard()->setText(ui->coinControlFeeLabel->text().left(ui->coinControlFeeLabel->text().indexOf(" ")));
}

// copy label "After fee" to clipboard
void CoinControlDialog::clipboardAfterFee()
{
    QApplication::clipboard()->setText(ui->coinControlAfterFeeLabel->text().left(ui->coinControlAfterFeeLabel->text().indexOf(" ")));
}

// copy label "Bytes" to clipboard
void CoinControlDialog::clipboardBytes()
{
    QApplication::clipboard()->setText(ui->coinControlBytesLabel->text());
}

// copy label "Priority" to clipboard
void CoinControlDialog::clipboardPriority()
{
    QApplication::clipboard()->setText(ui->coinControlPriorityLabel->text());
}

// copy label "Low output" to clipboard
void CoinControlDialog::clipboardLowOutput()
{
    QApplication::clipboard()->setText(ui->coinControlLowOutputLabel->text());
}

// copy label "Change" to clipboard
void CoinControlDialog::clipboardChange()
{
    QApplication::clipboard()->setText(ui->coinControlChangeLabel->text().left(ui->coinControlChangeLabel->text().indexOf(" ")));
}

// treeview: sort
void CoinControlDialog::sortView(int column, Qt::SortOrder order)
{
    sortColumn = column;
    sortOrder = order;
    ui->treeWidget->sortItems(column, order);
    ui->treeWidget->header()->setSortIndicator((sortColumn == COLUMN_AMOUNT_INT64 ? COLUMN_AMOUNT : (sortColumn == COLUMN_PRIORITY_INT64 ? COLUMN_PRIORITY : sortColumn)), sortOrder);
}

// treeview: clicked on header
void CoinControlDialog::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeWidget->header()->setSortIndicator((sortColumn == COLUMN_AMOUNT_INT64 ? COLUMN_AMOUNT : (sortColumn == COLUMN_PRIORITY_INT64 ? COLUMN_PRIORITY : sortColumn)), sortOrder);
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
            sortOrder = ((sortColumn == COLUMN_AMOUNT_INT64 || sortColumn == COLUMN_PRIORITY_INT64 || sortColumn == COLUMN_DATE || sortColumn == COLUMN_CONFIRMATIONS) ? Qt::DescendingOrder : Qt::AscendingOrder); // if amount,date,conf,priority then default => desc, else default => asc
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
    if (column == COLUMN_CHECKBOX && item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
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
            CoinControlDialog::updateLabels(model, coinControl, payAmounts, this);
        }
    }

    showHideConsolidationReadyToSend();
}

// helper function, return human readable label for priority number
QString CoinControlDialog::getPriorityLabel(double dPriority)
{
    if (dPriority > 576000ULL) // at least medium, this number is from AllowFree(), the other thresholds are kinda random
    {
        if      (dPriority > 5760000000ULL)   return tr("highest");
        else if (dPriority > 576000000ULL)    return tr("high");
        else if (dPriority > 57600000ULL)     return tr("medium-high");
        else                                    return tr("medium");
    }
    else
    {
        if      (dPriority > 5760ULL) return tr("low-medium");
        else if (dPriority > 58ULL)   return tr("low");
        else                            return tr("lowest");
    }
}

// shows count of locked unspent outputs
/*void CoinControlDialog::updateLabelLocked()
{
    vector<COutPoint> vOutpts;
    model->listLockedCoins(vOutpts);
    if (vOutpts.size() > 0)
    {
       ui->labelLocked->setText(tr("(%1 locked)").arg(vOutpts.size()));
       ui->labelLocked->setVisible(true);
    }
    else ui->labelLocked->setVisible(false);
}*/

void CoinControlDialog::updateLabels(WalletModel *model,
                                     CCoinControl *coinControl,
                                     QList<qint64>* payAmounts,
                                     QDialog* dialog)
{
    if (!model) return;

    // nPayAmount
    qint64 nPayAmount = 0;
    bool fLowOutput = false;
    bool fDust = false;
    CTransaction txDummy;
    foreach(const qint64 &amount, *payAmounts)
    {
        nPayAmount += amount;

        if (amount > 0)
        {
            if (amount < CENT)
                fLowOutput = true;

            CTxOut txout(amount, (CScript)vector<unsigned char>(24, 0));
            txDummy.vout.push_back(txout);
        }
    }

    QString sPriorityLabel      = "";
    int64_t nAmount             = 0;
    int64_t nPayFee             = 0;
    int64_t nAfterFee           = 0;
    int64_t nChange             = 0;
    unsigned int nBytes         = 0;
    unsigned int nBytesInputs   = 0;
    double dPriority            = 0;
    double dPriorityInputs      = 0;
    unsigned int nQuantity      = 0;

    vector<COutPoint> vCoinControl;
    vector<COutput>   vOutputs;
    coinControl->ListSelected(vCoinControl);
    model->getOutputs(vCoinControl, vOutputs);

    for (auto const& out : vOutputs)
    {
        // Quantity
        nQuantity++;

        // Amount
        nAmount += out.tx->vout[out.i].nValue;

        // Priority
        dPriorityInputs += (double)out.tx->vout[out.i].nValue * (out.nDepth+1);

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
        // Bytes
        nBytes = nBytesInputs + ((payAmounts->size() > 0 ? payAmounts->size() + 1 : 2) * 34) + 10; // always assume +1 output for change here

        // Priority
        dPriority = dPriorityInputs / nBytes;
        sPriorityLabel = CoinControlDialog::getPriorityLabel(dPriority);

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

            if (nChange == 0)
                nBytes -= 34;
        }

        // after fee
        nAfterFee = nAmount - nPayFee;
        if (nAfterFee < 0)
            nAfterFee = 0;
    }

    // actually update labels
    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel())
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    QLabel *l1 = dialog->findChild<QLabel *>("coinControlQuantityLabel");
    QLabel *l2 = dialog->findChild<QLabel *>("coinControlAmountLabel");
    QLabel *l3 = dialog->findChild<QLabel *>("coinControlFeeLabel");
    QLabel *l4 = dialog->findChild<QLabel *>("coinControlAfterFeeLabel");
    QLabel *l5 = dialog->findChild<QLabel *>("coinControlBytesLabel");
    QLabel *l6 = dialog->findChild<QLabel *>("coinControlPriorityLabel");
    QLabel *l7 = dialog->findChild<QLabel *>("coinControlLowOutputLabel");
    QLabel *l8 = dialog->findChild<QLabel *>("coinControlChangeLabel");

    // enable/disable "low output" and "change"
    dialog->findChild<QLabel *>("coinControlLowOutputTextLabel")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel *>("coinControlLowOutputLabel")    ->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel *>("coinControlChangeTextLabel")   ->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel *>("coinControlChangeLabel")       ->setEnabled(nPayAmount > 0);

    // stats
    l1->setText(QString::number(nQuantity));                                 // Quantity
    l2->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAmount));        // Amount
    l3->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nPayFee));        // Fee
    l4->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAfterFee));      // After Fee
    l5->setText(((nBytes > 0) ? "~" : "") + QString::number(nBytes));                                    // Bytes
    l6->setText(sPriorityLabel);                                             // Priority
    l7->setText((fLowOutput ? (fDust ? tr("DUST") : tr("yes")) : tr("no"))); // Low Output / Dust
    l8->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nChange));        // Change

    // turn labels "red"
    l5->setStyleSheet((nBytes >= 10000) ? "color:red;" : "");               // Bytes >= 10000
    l6->setStyleSheet((dPriority <= 576000) ? "color:red;" : "");         // Priority < "medium"
    l7->setStyleSheet((fLowOutput) ? "color:red;" : "");                    // Low Output = "yes"
    l8->setStyleSheet((nChange > 0 && nChange < CENT) ? "color:red;" : ""); // Change < 0.01BTC

    // tool tips
    l5->setToolTip(tr("This label turns red, if the transaction size is bigger than 10000 bytes.\n\n This means a fee of at least %1 per kb is required.\n\n Can vary +/- 1 Byte per input.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)));
    l6->setToolTip(tr("Transactions with higher priority get more likely into a block.\n\nThis label turns red, if the priority is smaller than \"medium\".\n\n This means a fee of at least %1 per kb is required.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)));
    l7->setToolTip(tr("This label turns red, if any recipient receives an amount smaller than %1.\n\n This means a fee of at least %2 is required. \n\n Amounts below 0.546 times the minimum relay fee are shown as DUST.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)).arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)));
    l8->setToolTip(tr("This label turns red, if the change is smaller than %1.\n\n This means a fee of at least %2 is required.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)).arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CENT)));
    dialog->findChild<QLabel *>("coinControlBytesTextLabel")    ->setToolTip(l5->toolTip());
    dialog->findChild<QLabel *>("coinControlPriorityTextLabel") ->setToolTip(l6->toolTip());
    dialog->findChild<QLabel *>("coinControlLowOutputTextLabel")->setToolTip(l7->toolTip());
    dialog->findChild<QLabel *>("coinControlChangeTextLabel")   ->setToolTip(l8->toolTip());

    // Insufficient funds
    QLabel *label = dialog->findChild<QLabel *>("coinControlInsuffFundsLabel");
    if (label)
        label->setVisible(nChange < 0);
}

void CoinControlDialog::updateView()
{
    bool treeMode = ui->treeModeRadioButton->isChecked();

    ui->treeWidget->clear();
    ui->treeWidget->setEnabled(false); // performance, otherwise updateLabels would be called for every checked checkbox
    ui->treeWidget->setAlternatingRowColors(!treeMode);
    QFlags<Qt::ItemFlag> flgCheckbox=Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    QFlags<Qt::ItemFlag> flgTristate=Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;

    int nDisplayUnit = BitcoinUnits::BTC;
    if (model && model->getOptionsModel())
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    map<QString, vector<COutput> > mapCoins;
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

            // disable locked coins
            /*if (model->isLockedCoin(txhash, out.i))
            {
                COutPoint outpt(txhash, out.i);
                coinControl->UnSelect(outpt); // just to be sure
                itemOutput->setDisabled(true);
                itemOutput->setIcon(COLUMN_CHECKBOX, QIcon(":/icons/lock_closed"));
            }*/

            // set checkbox
            if (coinControl->IsSelected(txhash, out.i))
                itemOutput->setCheckState(COLUMN_CHECKBOX,Qt::Checked);
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
        std::vector<COutPoint> selectionList;

        coinControl->ListSelected(selectionList);

        if (selectionList.size() <= m_inputSelectionLimit)
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
