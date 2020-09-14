#include "signverifymessagedialog.h"
#include "ui_signverifymessagedialog.h"

#include "addressbookpage.h"
#include "base58.h"
#include "guiutil.h"
#include "init.h"
#include "main.h"
#include "optionsmodel.h"
#include "streams.h"
#include "walletmodel.h"
#include "wallet/wallet.h"

#include <string>
#include <vector>

#include <QClipboard>

SignVerifyMessageDialog::SignVerifyMessageDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignVerifyMessageDialog),
    model(0)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->addressInEdit_SM, this);
    GUIUtil::setupAddressWidget(ui->addressInEdit_VM, this);

    ui->addressInEdit_SM->installEventFilter(this);
    ui->messageInEdit_SM->installEventFilter(this);
    ui->signatureOutEdit_SM->installEventFilter(this);
    ui->addressInEdit_VM->installEventFilter(this);
    ui->messageInEdit_VM->installEventFilter(this);
    ui->signatureInEdit_VM->installEventFilter(this);

    ui->signatureOutEdit_SM->setFont(GUIUtil::bitcoinAddressFont());
    ui->signatureInEdit_VM->setFont(GUIUtil::bitcoinAddressFont());
}

SignVerifyMessageDialog::~SignVerifyMessageDialog()
{
    delete ui;
}

void SignVerifyMessageDialog::setModel(WalletModel *model)
{
    this->model = model;
}

void SignVerifyMessageDialog::setAddress_SM(QString address)
{
    ui->addressInEdit_SM->setText(address);
    ui->messageInEdit_SM->setFocus();
}

void SignVerifyMessageDialog::setAddress_VM(QString address)
{
    ui->addressInEdit_VM->setText(address);
    ui->messageInEdit_VM->setFocus();
}

void SignVerifyMessageDialog::showTab_SM(bool fShow)
{
    ui->tabWidget->setCurrentIndex(0);

    if (fShow)
        this->show();
}

void SignVerifyMessageDialog::showTab_VM(bool fShow)
{
    ui->tabWidget->setCurrentIndex(1);
    if (fShow)
        this->show();
}

void SignVerifyMessageDialog::on_addressBookButton_SM_clicked()
{
    if (model && model->getAddressTableModel())
    {
        AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec())
        {
            setAddress_SM(dlg.getReturnValue());
        }
    }
}

void SignVerifyMessageDialog::on_pasteButton_SM_clicked()
{
    setAddress_SM(QApplication::clipboard()->text());
}

void SignVerifyMessageDialog::on_signMessageButton_SM_clicked()
{
    /* Clear old signature to ensure users don't get confused on error with an old signature displayed */
    ui->signatureOutEdit_SM->clear();

    CBitcoinAddress addr(ui->addressInEdit_SM->text().toStdString());
    if (!addr.IsValid())
    {
        ui->addressInEdit_SM->setValid(false);
        ui->statusLabel_SM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_SM->setText(tr("The entered address is invalid.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
    {
        ui->addressInEdit_SM->setValid(false);
        ui->statusLabel_SM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_SM->setText(tr("The entered address does not refer to a key.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid())
    {
        ui->statusLabel_SM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_SM->setText(tr("Wallet unlock was cancelled."));
        return;
    }

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
    {
        ui->statusLabel_SM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_SM->setText(tr("Private key for the entered address is not available."));
        return;
    }

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << ui->messageInEdit_SM->document()->toPlainText().toStdString();

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(Hash(ss.begin(), ss.end()), vchSig))
    {
        ui->statusLabel_SM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_SM->setText(QString("<nobr>") + tr("Message signing failed.") + QString("</nobr>"));
        return;
    }

    ui->statusLabel_SM->setStyleSheet("QLabel { color: green; }");
    ui->statusLabel_SM->setText(QString("<nobr>") + tr("Message signed.") + QString("</nobr>"));

    ui->signatureOutEdit_SM->setText(QString::fromStdString(EncodeBase64(&vchSig[0], vchSig.size())));
}

void SignVerifyMessageDialog::on_copySignatureButton_SM_clicked()
{
    QApplication::clipboard()->setText(ui->signatureOutEdit_SM->text());
}

void SignVerifyMessageDialog::on_clearButton_SM_clicked()
{
    ui->addressInEdit_SM->clear();
    ui->messageInEdit_SM->clear();
    ui->signatureOutEdit_SM->clear();
    ui->statusLabel_SM->clear();

    ui->addressInEdit_SM->setFocus();
}

void SignVerifyMessageDialog::on_addressBookButton_VM_clicked()
{
    if (model && model->getAddressTableModel())
    {
        AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::SendingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec())
        {
            setAddress_VM(dlg.getReturnValue());
        }
    }
}

void SignVerifyMessageDialog::on_verifyMessageButton_VM_clicked()
{
    CBitcoinAddress addr(ui->addressInEdit_VM->text().toStdString());
    if (!addr.IsValid())
    {
        ui->addressInEdit_VM->setValid(false);
        ui->statusLabel_VM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_VM->setText(tr("The entered address is invalid.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
    {
        ui->addressInEdit_VM->setValid(false);
        ui->statusLabel_VM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_VM->setText(tr("The entered address does not refer to a key.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(ui->signatureInEdit_VM->text().toStdString().c_str(), &fInvalid);

    if (fInvalid)
    {
        ui->signatureInEdit_VM->setValid(false);
        ui->statusLabel_VM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_VM->setText(tr("The signature could not be decoded.") + QString(" ") + tr("Please check the signature and try again."));
        return;
    }

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << ui->messageInEdit_VM->document()->toPlainText().toStdString();

    CKey key;
    if (!key.SetCompactSignature(Hash(ss.begin(), ss.end()), vchSig))
    {
        ui->signatureInEdit_VM->setValid(false);
        ui->statusLabel_VM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_VM->setText(tr("The signature did not match the message digest.") + QString(" ") + tr("Please check the signature and try again."));
        return;
    }

    if (!(CBitcoinAddress(key.GetPubKey().GetID()) == addr))
    {
        ui->statusLabel_VM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_VM->setText(QString("<nobr>") + tr("Message verification failed.") + QString("</nobr>"));
        return;
    }

    ui->statusLabel_VM->setStyleSheet("QLabel { color: green; }");
    ui->statusLabel_VM->setText(QString("<nobr>") + tr("Message verified.") + QString("</nobr>"));
}

void SignVerifyMessageDialog::on_clearButton_VM_clicked()
{
    ui->addressInEdit_VM->clear();
    ui->signatureInEdit_VM->clear();
    ui->messageInEdit_VM->clear();
    ui->statusLabel_VM->clear();

    ui->addressInEdit_VM->setFocus();
}

bool SignVerifyMessageDialog::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn)
    {
        if (ui->tabWidget->currentIndex() == 0)
        {
            /* Clear status message on focus change */
            ui->statusLabel_SM->clear();

            /* Select generated signature */
            if (object == ui->signatureOutEdit_SM)
            {
                ui->signatureOutEdit_SM->selectAll();
                return true;
            }
        }
        else if (ui->tabWidget->currentIndex() == 1)
        {
            /* Clear status message on focus change */
            ui->statusLabel_VM->clear();
        }
    }
    return QDialog::eventFilter(object, event);
}
