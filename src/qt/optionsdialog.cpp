#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "netbase.h"
#include "bitcoinunits.h"
#include "monitoreddatamapper.h"
#include "optionsmodel.h"
#include "qt/decoration.h"
#include "init.h"
#include "miner.h"

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

    if (fTestNet) ui->disableUpdateCheck->setHidden(true);

    ui->gridcoinAtStartupMinimised->setHidden(!ui->gridcoinAtStartup->isChecked());
    ui->limitTxnDisplayDateEdit->setHidden(!ui->limitTxnDisplayCheckBox->isChecked());

    connect(ui->gridcoinAtStartup, &QCheckBox::toggled, this, &OptionsDialog::hideStartMinimized);
    connect(ui->gridcoinAtStartupMinimised, &QCheckBox::toggled, this, &OptionsDialog::hideStartMinimized);

    connect(ui->limitTxnDisplayCheckBox, &QCheckBox::toggled, this, &OptionsDialog::hideLimitTxnDisplayDate);

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

    /* Window */
    mapper->addMapping(ui->disableTransactionNotifications, OptionsModel::DisableTrxNotifications);
    mapper->addMapping(ui->disablePollNotifications, OptionsModel::DisablePollNotifications);
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
    mapper->submit();
    accept();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_applyButton_clicked()
{
    mapper->submit();
    disableApplyButton();
}

void OptionsDialog::showRestartWarning_Proxy()
{
    if(!fRestartWarningDisplayed_Proxy)
    {
        QMessageBox::warning(this, tr("Warning"), tr("This setting will take effect after restarting Gridcoin."), QMessageBox::Ok);
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

bool OptionsDialog::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusOut)
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
    }
    return QDialog::eventFilter(object, event);
}
