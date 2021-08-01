#include "consolidateunspentwizardsendpage.h"
#include "ui_consolidateunspentwizardsendpage.h"

#include "util.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"

ConsolidateUnspentWizardSendPage::ConsolidateUnspentWizardSendPage(QWidget *parent) :
    QWizardPage(parent),
    ui(new Ui::ConsolidateUnspentWizardSendPage)
{
    ui->setupUi(this);
}

ConsolidateUnspentWizardSendPage::~ConsolidateUnspentWizardSendPage()
{
    delete ui;
}

void ConsolidateUnspentWizardSendPage::initializePage()
{
    ui->InputQuantityLabel->setText(field("quantityField").toString());
    ui->feeLabel->setText(field("feeField").toString());
    ui->afterFeeAmountLabel->setText(field("afterFeeAmountField").toString());
    ui->destinationAddressLabelLabel->setText(field("selectedAddressLabelField").toString());
    ui->destinationAddressLabel->setText(field("selectedAddressField").toString());

    LogPrint(BCLog::LogFlags::QT, "INFO: %s: destinationAddress = %s",
             __func__, field("selectedAddressField").toString().toStdString());

    qint64 amount = 0;
    bool parse_status = false;

    m_recipient.label = ui->destinationAddressLabelLabel->text();
    m_recipient.address = ui->destinationAddressLabel->text();

    parse_status = BitcoinUnits::parse(model->getOptionsModel()->getDisplayUnit(),
                                       ui->afterFeeAmountLabel->text()
                                            .left(ui->afterFeeAmountLabel->text().indexOf(" ")),
                                       &amount);

    if (parse_status) m_recipient.amount = amount;
}

void ConsolidateUnspentWizardSendPage::setModel(WalletModel *model)
{
    this->model = model;
}

void ConsolidateUnspentWizardSendPage::onFinishButtonClicked()
{
    emit selectedConsolidationRecipientSignal(m_recipient);
}
