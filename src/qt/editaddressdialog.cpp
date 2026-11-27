#include "editaddressdialog.h"
#include "ui_editaddressdialog.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "qt/decoration.h"

#include <QComboBox>
#include <QDataWidgetMapper>
#include <QMessageBox>

EditAddressDialog::EditAddressDialog(Mode mode, QWidget* parent)
               : QDialog(parent)
               , ui(new Ui::EditAddressDialog)
               , addressCombo(nullptr)
               , mapper(nullptr)
               , mode(mode)
               , model(nullptr)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));

    GUIUtil::setupAddressWidget(ui->addressEdit, this);

    switch(mode)
    {
    case NewReceivingAddress:
        setWindowTitle(tr("New receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case NewSendingAddress:
        setWindowTitle(tr("New sending address"));
        break;
    case EditReceivingAddress:
        setWindowTitle(tr("Edit receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case EditSendingAddress:
        setWindowTitle(tr("Edit sending address"));
        break;
    case AddExistingReceivingAddress:
        setWindowTitle(tr("Add existing receiving address"));
        // Replace the address line edit with a combo box of unbooked addresses.
        // The caller owns the layout item that replaceWidget() returns.
        addressCombo = new QComboBox(this);
        addressCombo->setFont(GUIUtil::bitcoinAddressFont());
        delete ui->formLayout->replaceWidget(ui->addressEdit, addressCombo);
        ui->addressEdit->hide();
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
}

EditAddressDialog::~EditAddressDialog()
{
    delete ui;
}

void EditAddressDialog::setModel(AddressTableModel *model)
{
    this->model = model;
    if(!model)
        return;

    mapper->setModel(model);
    mapper->addMapping(ui->labelEdit, AddressTableModel::Label);
    mapper->addMapping(ui->addressEdit, AddressTableModel::Address);

    if(mode == AddExistingReceivingAddress && addressCombo)
    {
        // The list is read off the GUI thread. Until it arrives the box is
        // empty and disabled.
        addressCombo->setEnabled(false);
        connect(model, &AddressTableModel::unbookedReceiveAddressesReady, this, [this](const QStringList& addresses) {
            addressCombo->clear();
            addressCombo->addItems(addresses);
            addressCombo->setEnabled(true);
        });
        model->requestUnbookedReceiveAddresses();
    }
}

void EditAddressDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditAddressDialog::saveCurrentRow()
{
    if(!model)
        return false;

    switch(mode)
    {
    case NewReceivingAddress:
    case NewSendingAddress:
        address = model->addRow(
                mode == NewSendingAddress ? AddressTableModel::Send : AddressTableModel::Receive,
                ui->labelEdit->text(),
                ui->addressEdit->text());
        break;
    case AddExistingReceivingAddress:
        address = model->addRow(
                AddressTableModel::ReceiveExisting,
                ui->labelEdit->text(),
                addressCombo ? addressCombo->currentText() : QString());
        break;
    case EditReceivingAddress:
    case EditSendingAddress:
        if(mapper->submit())
        {
            address = ui->addressEdit->text();
        }
        break;
    }
    return !address.isEmpty();
}

void EditAddressDialog::accept()
{
    if(!model)
        return;

    if(!saveCurrentRow())
    {
        switch(model->getEditStatus())
        {
        case AddressTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case AddressTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case AddressTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not a valid Gridcoin address.").arg(getDisplayAddress()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is already in the address book.").arg(getDisplayAddress()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("Could not unlock wallet."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::KEY_GENERATION_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("New key generation failed."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::NOT_MINE:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not owned by this wallet.").arg(getDisplayAddress()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        }
        return;
    }
    QDialog::accept();
}

QString EditAddressDialog::getDisplayAddress() const
{
    if(addressCombo)
        return addressCombo->currentText();
    return ui->addressEdit->text();
}

QString EditAddressDialog::getAddress() const
{
    return address;
}

void EditAddressDialog::setAddress(const QString &address)
{
    this->address = address;
    ui->addressEdit->setText(address);
}
