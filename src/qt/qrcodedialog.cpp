#include "qrcodedialog.h"
#include "ui_qrcodedialog.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include <QPixmap>
#include <QUrl>

#include <qrencode.h>

QRCodeDialog::QRCodeDialog(const QString &addr, const QString &label, bool enableReq, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QRCodeDialog),
    model(0),
    address(addr)
{
    ui->setupUi(this);

    setWindowTitle(QString("%1").arg(address));

    ui->requestPaymentCheckBox->setVisible(enableReq);
    ui->amountTextLabel->setVisible(enableReq);
    ui->lnReqAmount->setVisible(enableReq);

    ui->labelEdit->setText(label);

    ui->saveAsButton->setEnabled(false);

    genCode();
}

QRCodeDialog::~QRCodeDialog()
{
    delete ui;
}

void QRCodeDialog::setModel(OptionsModel *model)
{
    this->model = model;

    if (model)
        connect(model, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void QRCodeDialog::genCode()
{
    QString uri = getURI();

    if (uri != "")
    {
        ui->qrCodeLabel->setText("");

        QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
        if (!code)
        {
            ui->qrCodeLabel->setText(tr("Error encoding URI into QR Code."));
            return;
        }
        myImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
        myImage.fill(0xffffff);
        unsigned char *p = code->data;
        for (int y = 0; y < code->width; y++)
        {
            for (int x = 0; x < code->width; x++)
            {
                myImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                p++;
            }
        }
        QRcode_free(code);

        ui->qrCodeLabel->setPixmap(QPixmap::fromImage(myImage).scaled(300, 300));

        ui->outUriEdit->setPlainText(uri);
    }
}

QString QRCodeDialog::getURI()
{
    QString ret = QString("gridcoin:%1").arg(address);
    int paramCount = 0;

    ui->outUriEdit->clear();

    if (ui->requestPaymentCheckBox->isChecked())
    {
        if (ui->lnReqAmount->validate())
        {
            // even if we allow a non BTC unit input in lnReqAmount, we generate the URI with BTC as unit (as defined in BIP21)
            ret += QString("?amount=%1").arg(BitcoinUnits::format(BitcoinUnits::BTC, ui->lnReqAmount->value()));
            paramCount++;
        }
        else
        {
            ui->saveAsButton->setEnabled(false);
            ui->qrCodeLabel->setText(tr("The entered amount is invalid, please check."));
            return QString("");
        }
    }

    if (!ui->labelEdit->text().isEmpty())
    {
        QString lbl(QUrl::toPercentEncoding(ui->labelEdit->text()));
        ret += QString("%1label=%2").arg(paramCount == 0 ? "?" : "&").arg(lbl);
        paramCount++;
    }

    if (!ui->messageEdit->text().isEmpty())
    {
        QString msg(QUrl::toPercentEncoding(ui->messageEdit->text()));
        ret += QString("%1message=%2").arg(paramCount == 0 ? "?" : "&").arg(msg);
        paramCount++;
    }

    // limit URI length to prevent a DoS against the QR-Code dialog
    if (ret.length() > MAX_URI_LENGTH)
    {
        ui->saveAsButton->setEnabled(false);
        ui->qrCodeLabel->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        return QString("");
    }

    ui->saveAsButton->setEnabled(true);
    return ret;
}

void QRCodeDialog::on_lnReqAmount_textChanged()
{
    genCode();
}

void QRCodeDialog::on_labelEdit_textChanged()
{
    genCode();
}

void QRCodeDialog::on_messageEdit_textChanged()
{
    genCode();
}

void QRCodeDialog::on_saveAsButton_clicked()
{
    QString fn = GUIUtil::getSaveFileName(this, tr("Save QR Code"), QString(), tr("PNG Images (*.png)"));
    if (!fn.isEmpty())
        myImage.scaled(EXPORT_IMAGE_SIZE, EXPORT_IMAGE_SIZE).save(fn);
}

void QRCodeDialog::on_requestPaymentCheckBox_toggled(bool fChecked)
{
    if (!fChecked)
        // if requestPaymentCheckBox is not active, don't display lnReqAmount as invalid
        ui->lnReqAmount->setValid(true);

    genCode();
}

void QRCodeDialog::updateDisplayUnit()
{
    if (model)
    {
        // Update lnReqAmount with the current unit
        ui->lnReqAmount->setDisplayUnit(model->getDisplayUnit());
    }
}
