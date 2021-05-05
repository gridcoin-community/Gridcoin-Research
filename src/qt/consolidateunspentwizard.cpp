#include "coincontroldialog.h"
#include "consolidateunspentwizard.h"
#include "consolidateunspentdialog.h"
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
    this->setStartId(SelectInputsPage);

    ui->selectInputsPage->setCoinControl(coinControl);
    ui->selectInputsPage->setPayAmounts(payAmounts);

    connect(ui->selectInputsPage, SIGNAL(setAddressListSignal(std::map<QString, QString>)),
            ui->selectDestinationPage, SLOT(SetAddressList(const std::map<QString, QString>)));

    connect(ui->selectInputsPage, SIGNAL(setDefaultAddressSignal(QString)),
            ui->selectDestinationPage, SLOT(setDefaultAddressSelection(QString)));

    connect(this->button(QWizard::FinishButton), SIGNAL(clicked()), ui->sendPage, SLOT(onFinishButtonClicked()));
    connect(ui->sendPage, SIGNAL(selectedConsolidationRecipientSignal(SendCoinsRecipient)),
            this, SIGNAL(selectedConsolidationRecipientSignal(SendCoinsRecipient)));
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
