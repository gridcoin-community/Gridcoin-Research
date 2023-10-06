#include "optionsdialog.h"
#include "qevent.h"
#include "ui_optionsdialog.h"

#include "netbase.h"
#include "bitcoinunits.h"
#include "monitoreddatamapper.h"
#include "optionsmodel.h"
#include "qt/decoration.h"
#include "init.h"
#include "miner.h"
#include "sidestaketablemodel.h"
#include "editsidestakedialog.h"

#include <QSortFilterProxyModel>
#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QSystemTrayIcon>

OptionsDialog::OptionsDialog(QWidget* parent)
    : QDialog(parent)
      , ui(new Ui::OptionsDialog)
      , model(nullptr)
      , mapper(nullptr)
      , fRestartWarningDisplayed_Proxy(false)
      , fRestartWarningDisplayed_Lang(false)
      , fProxyIpValid(true)
      , fStakingEfficiencyValid(true)
      , fMinStakeSplitValueValid(true)
      , fPollExpireNotifyValid(true)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    connect(ui->connectSocks, &QPushButton::toggled, ui->proxyIp, &QWidget::setEnabled);
    connect(ui->connectSocks, &QPushButton::toggled, ui->proxyPort, &QWidget::setEnabled);
    connect(ui->connectSocks, &QPushButton::clicked, this, &OptionsDialog::showRestartWarning_Proxy);

    ui->proxyIp->installEventFilter(this);
    ui->stakingEfficiency->installEventFilter(this);
    ui->minPostSplitOutputValue->installEventFilter(this);
    ui->pollExpireNotifyLineEdit->installEventFilter(this);

    /* Window elements init */
#ifdef Q_OS_MAC
    /* hide launch at startup option on macOS */
    ui->gridcoinAtStartup->setVisible(false);
    ui->gridcoinAtStartupMinimised->setVisible(false);
    ui->verticalLayout_Main->removeWidget(ui->gridcoinAtStartup);
    ui->verticalLayout_Main->removeWidget(ui->gridcoinAtStartupMinimised);
    ui->verticalLayout_Main->removeItem(ui->horizontalLayoutGridcoinStartup);

    /* disable close confirmation on macOS */
    ui->confirmOnClose->setChecked(false);
    ui->confirmOnClose->setEnabled(false);
#endif

    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        ui->minimizeToTray->setChecked(false);
        ui->minimizeToTray->setEnabled(false);
        ui->minimizeOnClose->setChecked(false);
        ui->minimizeOnClose->setEnabled(false);
    }

    /* Display elements init */
    QDir translations(":translations");
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for (const QString& langStr : translations.entryList()) {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
        else
        {
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
    }

    ui->unit->setModel(new BitcoinUnits(this));
    ui->styleComboBox->addItem(tr("Dark"), QVariant("dark"));
    ui->styleComboBox->addItem(tr("Light"), QVariant("light"));

    /* Widget-to-option mapper */
    mapper = new MonitoredDataMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* enable apply button when data modified */
    connect(mapper, &MonitoredDataMapper::viewModified, this, &OptionsDialog::enableApplyButton);
    /* disable apply button when new data loaded */
    connect(mapper, &MonitoredDataMapper::currentIndexChanged, this, &OptionsDialog::disableApplyButton);
    /* setup/change UI elements when proxy IP, stakingEfficiency, or minStakeSplitValue is invalid/valid */
    connect(this, &OptionsDialog::proxyIpValid, this, &OptionsDialog::handleProxyIpValid);
    connect(this, &OptionsDialog::stakingEfficiencyValid, this, &OptionsDialog::handleStakingEfficiencyValid);
    connect(this, &OptionsDialog::minStakeSplitValueValid, this, &OptionsDialog::handleMinStakeSplitValueValid);
    /** setup/change UI elements when poll expiry notification time window is valid/invalid */
    connect(this, &OptionsDialog::pollExpireNotifyValid, this, &OptionsDialog::handlePollExpireNotifyValid);

    if (fTestNet) ui->disableUpdateCheck->setHidden(true);

    ui->gridcoinAtStartupMinimised->setHidden(!ui->gridcoinAtStartup->isChecked());
    ui->limitTxnDisplayDateEdit->setHidden(!ui->limitTxnDisplayCheckBox->isChecked());

    ui->pollExpireNotifyLabel->setHidden(ui->disablePollNotifications->isChecked());
    ui->pollExpireNotifyLineEdit->setHidden(ui->disablePollNotifications->isChecked());

    connect(ui->gridcoinAtStartup, &QCheckBox::toggled, this, &OptionsDialog::hideStartMinimized);
    connect(ui->gridcoinAtStartupMinimised, &QCheckBox::toggled, this, &OptionsDialog::hideStartMinimized);

    connect(ui->limitTxnDisplayCheckBox, &QCheckBox::toggled, this, &OptionsDialog::hideLimitTxnDisplayDate);

    connect(ui->disablePollNotifications, &QCheckBox::toggled, this , &OptionsDialog::hidePollExpireNotify);

    bool stake_split_enabled = ui->enableStakeSplit->isChecked();

    ui->stakingEfficiencyLabel->setHidden(!stake_split_enabled);
    ui->stakingEfficiency->setHidden(!stake_split_enabled);
    ui->minPostSplitOutputValueLabel->setHidden(!stake_split_enabled);
    ui->minPostSplitOutputValue->setHidden(!stake_split_enabled);

    connect(ui->enableStakeSplit, &QCheckBox::toggled, this, &OptionsDialog::hideStakeSplitting);
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setModel(OptionsModel *model)
{
    this->model = model;

    if(model)
    {
        connect(model, &OptionsModel::displayUnitChanged, this, &OptionsDialog::updateDisplayUnit);

        mapper->setModel(model);
        setMapper();
        mapper->toFirst();

        SideStakeTableModel* sidestake_model = model->getSideStakeTableModel();

        sidestake_model->refresh();

        ui->sidestakingTableView->setModel(sidestake_model);
        ui->sidestakingTableView->verticalHeader()->hide();
        ui->sidestakingTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->sidestakingTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        ui->sidestakingTableView->setContextMenuPolicy(Qt::CustomContextMenu);

        // Scale column widths by the logical DPI over 96.0 to deal with hires displays.
        ui->sidestakingTableView->setColumnWidth(SideStakeTableModel::Address, GRC::ScalePx(this, ADDRESS_COLUMN_WIDTH));
        ui->sidestakingTableView->setColumnWidth(SideStakeTableModel::Allocation, GRC::ScalePx(this, ALLOCATION_COLUMN_WIDTH));
        ui->sidestakingTableView->setColumnWidth(SideStakeTableModel::Description, GRC::ScalePx(this, DESCRIPTION_COLUMN_WIDTH));
        ui->sidestakingTableView->setColumnWidth(SideStakeTableModel::Status, GRC::ScalePx(this, STATUS_COLUMN_WIDTH));
        ui->sidestakingTableView->horizontalHeader()->setStretchLastSection(true);
        ui->sidestakingTableView->setShowGrid(true);

        ui->sidestakingTableView->sortByColumn(0, Qt::AscendingOrder);

        connect(ui->enableSideStaking, &QCheckBox::toggled, this, &OptionsDialog::hideSideStakeEdit);
        connect(ui->enableSideStaking, &QCheckBox::toggled, this, &OptionsDialog::refreshSideStakeTableModel);

        connect(ui->pushButtonNewSideStake, &QPushButton::clicked, this, &OptionsDialog::newSideStakeButton_clicked);
        connect(ui->pushButtonEditSideStake, &QPushButton::clicked, this, &OptionsDialog::editSideStakeButton_clicked);
        connect(ui->pushButtonDeleteSideStake, &QPushButton::clicked, this, &OptionsDialog::deleteSideStakeButton_clicked);

        connect(ui->sidestakingTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &OptionsDialog::sidestakeSelectionChanged);

        ui->sidestakingTableView->installEventFilter(this);

        connect(this, &OptionsDialog::sidestakeAllocationInvalid, this, &OptionsDialog::handleSideStakeAllocationInvalid);

    }

    /* update the display unit, to not use the default ("BTC") */
    updateDisplayUnit();

    updateStyle();

    /* warn only when language selection changes by user action (placed here so init via mapper doesn't trigger this) */
    connect(ui->lang, static_cast<void (QValueComboBox::*)()>(&QValueComboBox::valueChanged),
            this, &OptionsDialog::showRestartWarning_Lang);

    /* disable apply button after settings are loaded as there is nothing to save */
    disableApplyButton();
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->reserveBalance, OptionsModel::ReserveBalance);
    mapper->addMapping(ui->gridcoinAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->gridcoinAtStartupMinimised, OptionsModel::StartMin);
    mapper->addMapping(ui->disableUpdateCheck, OptionsModel::DisableUpdateCheck);
    mapper->addMapping(ui->returnChangeToInputAddressForContracts, OptionsModel::ContractChangeToInput);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    /* Staking */
    mapper->addMapping(ui->enableStaking, OptionsModel::EnableStaking);
    mapper->addMapping(ui->enableStakeSplit, OptionsModel::EnableStakeSplit);
    mapper->addMapping(ui->stakingEfficiency, OptionsModel::StakingEfficiency);
    mapper->addMapping(ui->minPostSplitOutputValue, OptionsModel::MinStakeSplitValue);
    mapper->addMapping(ui->enableSideStaking, OptionsModel::EnableSideStaking);

    /* Window */
    mapper->addMapping(ui->disableTransactionNotifications, OptionsModel::DisableTrxNotifications);
    mapper->addMapping(ui->disablePollNotifications, OptionsModel::DisablePollNotifications);
    mapper->addMapping(ui->pollExpireNotifyLineEdit, OptionsModel::PollExpireNotification);
#ifndef Q_OS_MAC
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
        mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
    }
    mapper->addMapping(ui->confirmOnClose, OptionsModel::ConfirmOnClose);
#endif    

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->styleComboBox, OptionsModel::WalletStylesheet,"currentData");
    mapper->addMapping(ui->limitTxnDisplayCheckBox, OptionsModel::LimitTxnDisplay);
    mapper->addMapping(ui->limitTxnDisplayDateEdit, OptionsModel::LimitTxnDate);
    mapper->addMapping(ui->displayAddresses, OptionsModel::DisplayAddresses);
}

void OptionsDialog::enableApplyButton()
{
    ui->applyButton->setEnabled(true);
}

void OptionsDialog::disableApplyButton()
{
    ui->applyButton->setEnabled(false);
}

void OptionsDialog::enableSaveButtons()
{
    /* prevent enabling of the save buttons when data modified, if there is an invalid proxy address present */
    if(fProxyIpValid)
        setSaveButtonState(true);
}

void OptionsDialog::disableSaveButtons()
{
    setSaveButtonState(false);
}

void OptionsDialog::setSaveButtonState(bool fState)
{
    ui->applyButton->setEnabled(fState);
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_okButton_clicked()
{
    refreshSideStakeTableModel();

    accept();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_applyButton_clicked()
{
    refreshSideStakeTableModel();

    disableApplyButton();
}

void OptionsDialog::newSideStakeButton_clicked()
{
    if (!model) {
        return;
    }

    EditSideStakeDialog dialog(EditSideStakeDialog::NewSideStake, this);

    dialog.setModel(model->getSideStakeTableModel());

    dialog.exec();
}

void OptionsDialog::editSideStakeButton_clicked()
{
    if (!model || !ui->sidestakingTableView->selectionModel()) {
        return;
    }

    QModelIndexList indexes = ui->sidestakingTableView->selectionModel()->selectedRows();

    if (indexes.isEmpty()) {
        return;
    }

    if (indexes.size() > 1) {
        QMessageBox::warning(this, tr("Error"), tr("You can only edit one sidestake at a time."),  QMessageBox::Ok);
    }

    EditSideStakeDialog dialog(EditSideStakeDialog::EditSideStake, this);

    dialog.setModel(model->getSideStakeTableModel());
    dialog.loadRow(indexes.at(0).row());
    dialog.exec();
}

void OptionsDialog::deleteSideStakeButton_clicked()
{
    if (!model || !ui->sidestakingTableView->selectionModel()) {
        return;
    }

    QModelIndexList indexes = ui->sidestakingTableView->selectionModel()->selectedRows();

    if (indexes.isEmpty()) {
        return;
    }

    if (indexes.size() > 1) {
        QMessageBox::warning(this, tr("Error"), tr("You can only delete one sidestake at a time."),  QMessageBox::Ok);
    }

    model->getSideStakeTableModel()->removeRows(indexes.at(0).row(), 1);
}

void OptionsDialog::showRestartWarning_Proxy()
{
    if(!fRestartWarningDisplayed_Proxy)
    {
        QMessageBox::warning(this, tr("Warning"), tr("This setting will take effect"
                                                     " after restarting Gridcoin."), QMessageBox::Ok);
        fRestartWarningDisplayed_Proxy = true;
    }
}

void OptionsDialog::showRestartWarning_Lang()
{
    if(!fRestartWarningDisplayed_Lang)
    {
        QMessageBox::warning(this, tr("Warning"), tr("This setting will take effect after restarting Gridcoin."), QMessageBox::Ok);
        fRestartWarningDisplayed_Lang = true;
    }
}

void OptionsDialog::updateDisplayUnit()
{
    if(model)
    {
        /* Update reserveBalance with the current unit */
        ui->reserveBalance->setDisplayUnit(model->getDisplayUnit());
    }
}

void OptionsDialog::updateStyle()
{
    if(model)
    {
        /* Update style with the current stylesheet */
        QString value=model->getCurrentStyle();
        int index = ui->styleComboBox->findData(value);
        if ( index != -1 ) { // -1 for not found
           ui->styleComboBox->setCurrentIndex(index);
        }
    }
}

void OptionsDialog::hideStartMinimized()
{
    if (model)
    {
        ui->gridcoinAtStartupMinimised->setHidden(!ui->gridcoinAtStartup->isChecked());
    }
}

void OptionsDialog::hideLimitTxnDisplayDate()
{
    if (model)
    {
        ui->limitTxnDisplayDateEdit->setHidden(!ui->limitTxnDisplayCheckBox->isChecked());
    }
}

void OptionsDialog::hidePollExpireNotify()
{
    if (model) {
        ui->pollExpireNotifyLabel->setHidden(ui->disablePollNotifications->isChecked());
        ui->pollExpireNotifyLineEdit->setHidden(ui->disablePollNotifications->isChecked());
    }
}

void OptionsDialog::hideStakeSplitting()
{
    if (model)
    {
        bool stake_split_enabled = ui->enableStakeSplit->isChecked();

        ui->stakingEfficiencyLabel->setHidden(!stake_split_enabled);
        ui->stakingEfficiency->setHidden(!stake_split_enabled);
        ui->minPostSplitOutputValueLabel->setHidden(!stake_split_enabled);
        ui->minPostSplitOutputValue->setHidden(!stake_split_enabled);
    }
}

void OptionsDialog::hideSideStakeEdit()
{
    if (model) {
        bool local_side_staking_enabled = ui->enableSideStaking->isChecked();

        ui->pushButtonNewSideStake->setHidden(!local_side_staking_enabled);
        ui->pushButtonEditSideStake->setHidden(!local_side_staking_enabled);
    }
}

void OptionsDialog::handleProxyIpValid(QValidatedLineEdit *object, bool fState)
{
    // this is used in a check before re-enabling the save buttons
    fProxyIpValid = fState;

    if (fProxyIpValid)
    {
        enableSaveButtons();
        ui->statusLabel->clear();
    }
    else
    {
        disableSaveButtons();
        object->setValid(fProxyIpValid);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

void OptionsDialog::handleStakingEfficiencyValid(QValidatedLineEdit *object, bool fState)
{
    // this is used in a check before re-enabling the save buttons
    fStakingEfficiencyValid = fState;

    if (fStakingEfficiencyValid)
    {
        enableSaveButtons();
        ui->statusLabel->clear();
    }
    else
    {
        disableSaveButtons();
        object->setValid(fStakingEfficiencyValid);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied target staking efficiency is invalid."));
    }
}

void OptionsDialog::handleMinStakeSplitValueValid(QValidatedLineEdit *object, bool fState)
{
    // this is used in a check before re-enabling the save buttons
    fMinStakeSplitValueValid = fState;

    if (fMinStakeSplitValueValid)
    {
        enableSaveButtons();
        ui->statusLabel->clear();
    }
    else
    {
        disableSaveButtons();
        object->setValid(fMinStakeSplitValueValid);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied minimum post stake-split UTXO size is invalid."));
    }
}

void OptionsDialog::handlePollExpireNotifyValid(QValidatedLineEdit *object, bool fState)
{
    // this is used in a check before re-enabling the save buttons
    fPollExpireNotifyValid = fState;

    if (fPollExpireNotifyValid) {
        enableSaveButtons();
        ui->statusLabel->clear();
    } else {
        disableSaveButtons();
        object->setValid(fPollExpireNotifyValid);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied time for notification before poll expires must "
                                    "be between 0.25 and 24 hours."));
    }
}

void OptionsDialog::refreshSideStakeTableModel()
{
    if (!mapper->submit()
        && model->getSideStakeTableModel()->getEditStatus() == SideStakeTableModel::INVALID_ALLOCATION) {
        emit sidestakeAllocationInvalid();
    } else {
        model->getSideStakeTableModel()->refresh();
    }
}

bool OptionsDialog::eventFilter(QObject *object, QEvent *event)
{
    bool filter_event = false;

    if (event->type() == QEvent::FocusOut) {
        filter_event = true;
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
           filter_event = true;
        }
    }

    if (filter_event)
    {
        if (object == ui->proxyIp)
        {
            CService serv(LookupNumeric(ui->proxyIp->text().toStdString().c_str(), 9050));
            proxyType addrProxy = proxyType(serv, true);
            /* Check proxyIp for a valid IPv4/IPv6 address and emit the proxyIpValid signal */
            emit proxyIpValid(ui->proxyIp, addrProxy.IsValid());
        }

        if (object == ui->stakingEfficiency)
        {
            bool ok = false;
            double efficiency = ui->stakingEfficiency->text().toDouble(&ok);

            if (!ok)
            {
                emit stakingEfficiencyValid(ui->stakingEfficiency, false);
            }
            else
            {
                if (efficiency < 75.0 || efficiency > 98.0)
                {
                    emit stakingEfficiencyValid(ui->stakingEfficiency, false);
                }
                else
                {
                    emit stakingEfficiencyValid(ui->stakingEfficiency, true);
                }
            }
        }

        if (object == ui->minPostSplitOutputValue)
        {
            bool ok = false;
            CAmount post_split_min_value = (CAmount) ui->minPostSplitOutputValue->text().toULong(&ok) * COIN;

            if (!ok)
            {
                emit minStakeSplitValueValid(ui->minPostSplitOutputValue, false);
            }
            else
            {
                if (post_split_min_value < MIN_STAKE_SPLIT_VALUE_GRC * COIN || post_split_min_value > MAX_MONEY)
                {
                    emit minStakeSplitValueValid(ui->minPostSplitOutputValue, false);
                }
                else
                {
                    emit minStakeSplitValueValid(ui->minPostSplitOutputValue, true);
                }
            }
        }

        if (object == ui->pollExpireNotifyLineEdit) {
            bool ok = false;
            double hours = ui->pollExpireNotifyLineEdit->text().toDouble(&ok);

            if (!ok) {
                emit pollExpireNotifyValid(ui->pollExpireNotifyLineEdit, false);
            } else {
                if (hours >= 0.25 && hours <= 24.0 * 7.0) {
                    emit pollExpireNotifyValid(ui->pollExpireNotifyLineEdit, true);
                } else {
                    emit pollExpireNotifyValid(ui->pollExpireNotifyLineEdit, false);
                }
            }
        }
    }

<<<<<<< HEAD
=======
    // This is required to provide immediate feedback on invalid allocation entries on in place editing.
    if (object == ui->sidestakingTableView)
    {
        if (model->getSideStakeTableModel()->getEditStatus() == SideStakeTableModel::INVALID_ALLOCATION) {
            LogPrint(BCLog::LogFlags::VERBOSE, "INFO %s: event type = %i",
                     __func__,
                     (int) event->type());

            emit sidestakeAllocationInvalid();
        }
    }

>>>>>>> 71b6deb3e (Implementation of EditSideStakeDialog)
    return QDialog::eventFilter(object, event);
}

void OptionsDialog::sidestakeSelectionChanged()
{
    QTableView *table = ui->sidestakingTableView;

    if (table->selectionModel()->hasSelection()) {
        QModelIndexList indexes = ui->sidestakingTableView->selectionModel()->selectedRows();

        if (indexes.size() > 1) {
            ui->pushButtonEditSideStake->setEnabled(false);
            ui->pushButtonDeleteSideStake->setEnabled(false);
        } else if (static_cast<GRC::SideStake*>(indexes.at(0).internalPointer())->m_status
                   == GRC::SideStakeStatus::MANDATORY) {
            ui->pushButtonEditSideStake->setEnabled(false);
            ui->pushButtonDeleteSideStake->setEnabled(false);
        } else {
            ui->pushButtonEditSideStake->setEnabled(true);
            ui->pushButtonDeleteSideStake->setEnabled(true);
        }
    }
}

void OptionsDialog::handleSideStakeAllocationInvalid()
{
    model->getSideStakeTableModel()->refresh();

    QMessageBox::warning(this, windowTitle(),
                         tr("The entered allocation is not valid and is reverted. Check to make sure "
                            "that the allocation is greater than or equal to zero and when added to the other "
                            "allocations totals less than 100."),
                         QMessageBox::Ok, QMessageBox::Ok);
}

void OptionsDialog::updateSideStakeTableView()
{
    ui->sidestakingTableView->update();
}
