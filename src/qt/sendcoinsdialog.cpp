#include "sendcoinsdialog.h"
#include "ui_sendcoinsdialog.h"

#include "init.h"
#include "addresstablemodel.h"
#include "addressbookpage.h"

#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"
#include "guiutil.h"
#include "askpassphrasedialog.h"

#include "wallet/coincontrol.h"
#include "policy/policy.h"
#include "coincontroldialog.h"
#include "consolidateunspentdialog.h"
#include "consolidateunspentwizard.h"
#include "qt/decoration.h"

#include <QMessageBox>
#include <QLocale>
#include <QTextDocument>
#include <QScrollBar>
#include <QClipboard>

SendCoinsDialog::SendCoinsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCoinsDialog),
    coinControl(new CCoinControl),
    payAmounts(new QList<qint64>),
    model(0)
{
    ui->setupUi(this);

    GRC::ScaleFontPointSize(ui->headerTitleLabel, 15);
    GRC::ScaleFontPointSize(ui->headerBalanceLabel, 14);
    GRC::ScaleFontPointSize(ui->headerBalanceCaptionLabel, 8);

    addEntry();

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));

    // Coin Control
    ui->coinControlChangeEdit->setFont(GUIUtil::bitcoinAddressFont());
    connect(ui->coinControlFeaturesButton, SIGNAL(clicked()), this, SLOT(toggleCoinControl()));
    connect(ui->coinControlPushButton, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
    connect(ui->coinControlConsolidateWizardPushButton, SIGNAL(clicked()), this, SLOT(coinControlConsolidateWizardButtonClicked()));
    connect(ui->coinControlResetPushButton, SIGNAL(clicked()), this, SLOT(coinControlResetButtonClicked()));
    connect(ui->coinControlChangeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(coinControlChangeChecked(int)));
    connect(ui->coinControlChangeEdit, SIGNAL(textEdited(const QString &)), this, SLOT(coinControlChangeEdited(const QString &)));

    // Coin Control: clipboard actions
    QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction *clipboardPriorityAction = new QAction(tr("Copy priority"), this);
    QAction *clipboardLowOutputAction = new QAction(tr("Copy low output"), this);
    QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
    connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardPriority()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
    ui->coinControlQuantityLabel->addAction(clipboardQuantityAction);
    ui->coinControlAmountLabel->addAction(clipboardAmountAction);
    ui->coinControlFeeLabel->addAction(clipboardFeeAction);
    ui->coinControlAfterFeeLabel->addAction(clipboardAfterFeeAction);
    ui->coinControlBytesLabel->addAction(clipboardBytesAction);
    ui->coinControlPriorityLabel->addAction(clipboardPriorityAction);
    ui->coinControlLowOutputLabel->addAction(clipboardLowOutputAction);
    ui->coinControlChangeLabel->addAction(clipboardChangeAction);

    fNewRecipientAllowed = true;
}

void SendCoinsDialog::setModel(WalletModel *model)
{
    this->model = model;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            entry->setModel(model);
        }
    }
    if(model && model->getOptionsModel())
    {
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        // Update icons according to the stylesheet
        connect(model->getOptionsModel(),SIGNAL(walletStylesheetChanged(QString)), this, SLOT(updateIcons()));

        // Coin Control
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
        ui->coinControlContentWidget->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        coinControlUpdateLabels();

        // set the icons according to the style options
        updateIcons();
    }
}

SendCoinsDialog::~SendCoinsDialog()
{
    delete ui;
    delete coinControl;
    delete payAmounts;
}

void SendCoinsDialog::on_sendButton_clicked()
{
    QList<SendCoinsRecipient> recipients;
    bool valid = true;

    if(!model)
        return;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            if(entry->validate())
            {
                recipients.append(entry->getValue());
            }
            else
            {
                valid = false;
            }
        }
    }

    if(!valid || recipients.isEmpty())
    {
        return;
    }

    // Format confirmation message
    QStringList formatted;
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        formatted.append(tr("<b>%1</b> to %2 (%3)").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, rcp.amount),
              rcp.label.toHtmlEscaped(), rcp.address));
    }

    fNewRecipientAllowed = false;

    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm send coins"),
                          tr("Are you sure you want to send %1?").arg(formatted.join(tr(" and "))),
          QMessageBox::Yes|QMessageBox::Cancel,
          QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        fNewRecipientAllowed = true;
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        fNewRecipientAllowed = true;
        return;
    }

    WalletModel::SendCoinsReturn sendstatus;

    if (!model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
        sendstatus = model->sendCoins(recipients);
    else
        sendstatus = model->sendCoins(recipients, coinControl);

    switch(sendstatus.status)
    {
    case WalletModel::InvalidAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The recipient address is not valid, please recheck."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::InvalidAmount:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount to pay must be larger than 0."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount exceeds your balance."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The total exceeds your balance when the %1 transaction fee is included.").
            arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, sendstatus.fee)),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::DuplicateAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Duplicate address found, can only send to each address once per send operation."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCreationFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: Transaction creation failed."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCommitFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::Aborted: // User aborted, nothing to do
        break;
    case WalletModel::OK:
        accept();
        coinControl->UnSelectAll();
        coinControlUpdateLabels();
        break;
    }
    fNewRecipientAllowed = true;
}

void SendCoinsDialog::clear()
{
    // Remove entries until only one left
    while(ui->entries->count())
    {
        delete ui->entries->takeAt(0)->widget();
    }
    addEntry();

    updateRemoveEnabled();

    ui->sendButton->setDefault(true);
}

void SendCoinsDialog::reject()
{
    clear();
}

void SendCoinsDialog::accept()
{
    clear();
}

SendCoinsEntry *SendCoinsDialog::addEntry()
{
    SendCoinsEntry *entry = new SendCoinsEntry(this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendCoinsEntry*)), this, SLOT(removeEntry(SendCoinsEntry*)));
    connect(entry, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    updateRemoveEnabled();

    // Focus the field, so that entry can start immediately
    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    QCoreApplication::instance()->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if(bar)
        bar->setSliderPosition(bar->maximum());
    return entry;
}

void SendCoinsDialog::updateRemoveEnabled()
{
    // Remove buttons are enabled as soon as there is more than one send-entry
    bool enabled = (ui->entries->count() > 1);
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            entry->setRemoveEnabled(enabled);

            // Transactions can only contain one message. Hide the message field
            // for all but the first output entry.
            //
            // TODO: separate the message field from the context of each output.
            // Leaving this field in the first output group gives the impression
            // that only the first recipient can see the message.
            //
            entry->setMessageEnabled(i == 0);
        }
    }
    setupTabChain(0);
    coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    delete entry;
    updateRemoveEnabled();
}

QWidget *SendCoinsDialog::setupTabChain(QWidget *prev)
{
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->addButton);
    QWidget::setTabOrder(ui->addButton, ui->sendButton);
    return ui->sendButton;
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient &rv)
{
    if(!fNewRecipientAllowed)
        return;

    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setValue(rv);
}

bool SendCoinsDialog::handleURI(const QString &uri)
{
    SendCoinsRecipient rv;
    // URI has to be valid
    if (GUIUtil::parseBitcoinURI(uri, &rv))
    {
        CBitcoinAddress address(rv.address.toStdString());
        if (!address.IsValid())
            return false;
        pasteEntry(rv);
        return true;
    }

    return false;
}

void SendCoinsDialog::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    Q_UNUSED(stake);
    Q_UNUSED(unconfirmedBalance);
    Q_UNUSED(immatureBalance);
    if(!model || !model->getOptionsModel())
        return;

    int unit = model->getOptionsModel()->getDisplayUnit();

    ui->headerBalanceLabel->setText(BitcoinUnits::format(unit, balance));
    ui->headerBalanceCaptionLabel->setText(tr("Available (%1)").arg(BitcoinUnits::name(unit)));
}

void SendCoinsDialog::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        // Update headerBalanceLabel with the current balance and the current unit
        int unit = model->getOptionsModel()->getDisplayUnit();

        ui->headerBalanceLabel->setText(BitcoinUnits::format(unit, model->getBalance()));
        ui->headerBalanceCaptionLabel->setText(tr("Available (%1)").arg(BitcoinUnits::name(unit)));
    }
}

void SendCoinsDialog::toggleCoinControl()
{
    if (model && model->getOptionsModel()) {
        model->getOptionsModel()->toggleCoinControlFeatures();
    }
}

// Coin Control: copy label "Quantity" to clipboard
void SendCoinsDialog::coinControlClipboardQuantity()
{
    QApplication::clipboard()->setText(ui->coinControlQuantityLabel->text());
}

// Coin Control: copy label "Amount" to clipboard
void SendCoinsDialog::coinControlClipboardAmount()
{
    QApplication::clipboard()->setText(ui->coinControlAmountLabel->text().left(ui->coinControlAmountLabel->text().indexOf(" ")));
}

// Coin Control: copy label "Fee" to clipboard
void SendCoinsDialog::coinControlClipboardFee()
{
    QApplication::clipboard()->setText(ui->coinControlFeeLabel->text().left(ui->coinControlFeeLabel->text().indexOf(" ")));
}

// Coin Control: copy label "After fee" to clipboard
void SendCoinsDialog::coinControlClipboardAfterFee()
{
    QApplication::clipboard()->setText(ui->coinControlAfterFeeLabel->text().left(ui->coinControlAfterFeeLabel->text().indexOf(" ")));
}

// Coin Control: copy label "Bytes" to clipboard
void SendCoinsDialog::coinControlClipboardBytes()
{
    QApplication::clipboard()->setText(ui->coinControlBytesLabel->text());
}

// Coin Control: copy label "Priority" to clipboard
void SendCoinsDialog::coinControlClipboardPriority()
{
    QApplication::clipboard()->setText(ui->coinControlPriorityLabel->text());
}

// Coin Control: copy label "Low output" to clipboard
void SendCoinsDialog::coinControlClipboardLowOutput()
{
    QApplication::clipboard()->setText(ui->coinControlLowOutputLabel->text());
}

// Coin Control: copy label "Change" to clipboard
void SendCoinsDialog::coinControlClipboardChange()
{
    QApplication::clipboard()->setText(ui->coinControlChangeLabel->text().left(ui->coinControlChangeLabel->text().indexOf(" ")));
}

// Coin Control: settings menu - coin control enabled/disabled by user
void SendCoinsDialog::coinControlFeatureChanged(bool checked)
{
    ui->coinControlContentWidget->setVisible(checked);
    updateCoinControlIcon();
    coinControlUpdateLabels();
}

// Coin Control: button inputs -> show actual coin control dialog
void SendCoinsDialog::coinControlButtonClicked()
{
    CoinControlDialog dlg(this, coinControl, payAmounts);
    dlg.setModel(model);

    connect(&dlg, SIGNAL(selectedConsolidationRecipientSignal(SendCoinsRecipient)),
            this, SLOT(selectedConsolidationRecipient(SendCoinsRecipient)));

    dlg.exec();
    coinControlUpdateLabels();
}

void SendCoinsDialog::coinControlResetButtonClicked()
{
    coinControl->SetNull();
    coinControlUpdateLabels();
}

void SendCoinsDialog::coinControlConsolidateWizardButtonClicked()
{
    CoinControlDialog dlg(this, coinControl, payAmounts);
    dlg.setModel(model);

    connect(&dlg, SIGNAL(selectedConsolidationRecipientSignal(SendCoinsRecipient)),
            this, SLOT(selectedConsolidationRecipient(SendCoinsRecipient)));

    ConsolidateUnspentWizard wizard(this, coinControl, payAmounts);
    wizard.setModel(model);

    connect(&wizard, SIGNAL(selectedConsolidationRecipientSignal(SendCoinsRecipient)),
            this, SLOT(selectedConsolidationRecipient(SendCoinsRecipient)));

    wizard.exec();
    coinControlUpdateLabels();
}

void SendCoinsDialog::selectedConsolidationRecipient(SendCoinsRecipient consolidationRecipient)
{
    LogPrintf("INFO: %s: SLOT called.", __func__);

    ui->coinControlChangeCheckBox->setChecked(true);
    ui->coinControlChangeEdit->setText(consolidationRecipient.address);

    for (int i = ui->entries->count() - 1; i >= 0; --i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());

        if (entry)
        {
            removeEntry(entry);
        }
    }

    pasteEntry(consolidationRecipient);
}

// Coin Control: checkbox custom change address
void SendCoinsDialog::coinControlChangeChecked(int state)
{
    if (model)
    {
        if (state == Qt::Checked)
            coinControl->destChange = CBitcoinAddress(ui->coinControlChangeEdit->text().toStdString()).Get();
        else
            coinControl->destChange = CNoDestination();
    }

    ui->coinControlChangeEdit->setEnabled((state == Qt::Checked));
    ui->coinControlChangeAddressLabel->setEnabled((state == Qt::Checked));

    coinControlUpdateStatus();
}

// Coin Control: custom change address changed
void SendCoinsDialog::coinControlChangeEdited(const QString & text)
{
    if (model)
    {
        coinControl->destChange = CBitcoinAddress(text.toStdString()).Get();

        // label for the change address
        ui->coinControlChangeAddressLabel->setStyleSheet(QString());
        if (text.isEmpty())
            ui->coinControlChangeAddressLabel->setText(QString());
        else if (!CBitcoinAddress(text.toStdString()).IsValid())
        {
            ui->coinControlChangeAddressLabel->setStyleSheet("QLabel{color:red;}");
            ui->coinControlChangeAddressLabel->setText(tr("WARNING: Invalid Gridcoin address"));
        }
        else
        {
            QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
            if (!associatedLabel.isEmpty())
                ui->coinControlChangeAddressLabel->setText(associatedLabel);
            else
            {
                CPubKey pubkey;
                CKeyID keyid;
                CBitcoinAddress(text.toStdString()).GetKeyID(keyid);
                if (model->getPubKey(keyid, pubkey))
                    ui->coinControlChangeAddressLabel->setText(tr("(no label)"));
                else
                {
                    ui->coinControlChangeAddressLabel->setStyleSheet("QLabel{color:red;}");
                    ui->coinControlChangeAddressLabel->setText(tr("WARNING: unknown change address"));
                }
            }
        }
    }
}

// Coin Control: update labels
void SendCoinsDialog::coinControlUpdateLabels()
{
    coinControlUpdateStatus();

    if (!model || !model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
        return;

    // set pay amounts
    payAmounts->clear();
    for (int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) payAmounts->append(entry->getValue().amount);
    }

    if (coinControl->HasSelected())
    {
        // actual coin control calculation
        CoinControlDialog::updateLabels(model, coinControl, payAmounts, this);

        // show coin control stats
        ui->coinControlAutomaticallySelectedLabel->hide();
        ui->widgetCoinControl->show();
    }
    else
    {
        // hide coin control stats
        ui->coinControlAutomaticallySelectedLabel->show();
        ui->widgetCoinControl->hide();
        ui->coinControlInsuffFundsLabel->hide();
    }
}

void SendCoinsDialog::coinControlUpdateStatus()
{
    if (model
        && model->getOptionsModel()
        && model->getOptionsModel()->getCoinControlFeatures()
        && (coinControl->HasSelected() || ui->coinControlChangeCheckBox->isChecked()))
    {
        ui->coinControlStatusLabel->setText(tr("Active"));
        ui->coinControlStatusIconLabel->setPixmap(GRC::ScaleIcon(this, ":/icons/round_green_check", 16));
        return;
    }

    ui->coinControlStatusLabel->setText(tr("Inactive"));
    ui->coinControlStatusIconLabel->setPixmap(GRC::ScaleIcon(this, ":/icons/round_gray_x", 16));
}

void SendCoinsDialog::updateIcons()
{
#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->addButton->setIcon(QIcon());
    ui->clearButton->setIcon(QIcon());
    ui->sendButton->setIcon(QIcon());
#else
    if(model && model->getOptionsModel())
    {
        ui->sendButton->setIcon(QIcon(":/icons/send_"+model->getOptionsModel()->getCurrentStyle()));
    }
#endif

    updateCoinControlIcon();
}

void SendCoinsDialog::updateCoinControlIcon()
{
    if (!model || !model->getOptionsModel()) {
        return;
    }

    const QString theme = model->getOptionsModel()->getCurrentStyle();

    if (model->getOptionsModel()->getCoinControlFeatures()) {
        ui->coinControlFeaturesButton->setIcon(QIcon(":/icons/" + theme + "_chevron_down"));
    } else {
        ui->coinControlFeaturesButton->setIcon(QIcon(":/icons/" + theme + "_chevron_right"));
    }
}
