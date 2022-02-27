#include "coincontroldialog.h"
#include "consolidateunspentwizard.h"
#include "consolidateunspentdialog.h"
#include "qt/decoration.h"
#include "ui_consolidateunspentwizard.h"

#include "util.h"


ConsolidateUnspentWizard::ConsolidateUnspentWizard(QWidget *parent,
                                                   CCoinControl *coinControl,
                                                   QList<qint64> *payAmounts) :
    QWizard(parent),
    ui(new Ui::ConsolidateUnspentWizard),
    coinControl(coinControl),
    payAmounts(payAmounts)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));
    this->setStartId(SelectInputsPage);

    ui->selectInputsPage->setCoinControl(coinControl);
    ui->selectInputsPage->setPayAmounts(payAmounts);

    connect(ui->selectInputsPage, &ConsolidateUnspentWizardSelectInputsPage::setAddressListSignal,
            ui->selectDestinationPage, &ConsolidateUnspentWizardSelectDestinationPage::SetAddressList);

    connect(ui->selectInputsPage, &ConsolidateUnspentWizardSelectInputsPage::setDefaultAddressSignal,
            ui->selectDestinationPage, &ConsolidateUnspentWizardSelectDestinationPage::setDefaultAddressSelection);

    connect(this->button(QWizard::FinishButton), &QAbstractButton::clicked,
            ui->sendPage, &ConsolidateUnspentWizardSendPage::onFinishButtonClicked);
    connect(ui->sendPage, &ConsolidateUnspentWizardSendPage::selectedConsolidationRecipientSignal,
            this, &ConsolidateUnspentWizard::selectedConsolidationRecipientSignal);
}

ConsolidateUnspentWizard::~ConsolidateUnspentWizard()
{
    delete ui;
}

void ConsolidateUnspentWizard::accept()
{
    QDialog::accept();
}

void ConsolidateUnspentWizard::setModel(WalletModel *model)
{
    this->model = model;

    ui->selectInputsPage->setModel(model);
    ui->sendPage->setModel(model);
}
