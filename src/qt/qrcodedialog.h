#ifndef BITCOIN_QT_QRCODEDIALOG_H
#define BITCOIN_QT_QRCODEDIALOG_H

#include <QDialog>
#include <QImage>

namespace Ui {
    class QRCodeDialog;
}
class OptionsModel;

class QRCodeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QRCodeDialog(const QString& addr, const QString& label, bool enableReq, QWidget* parent = nullptr);
    ~QRCodeDialog();

    void setModel(OptionsModel *model);

private slots:
    void on_lnReqAmount_textChanged();
    void on_labelEdit_textChanged();
    void on_messageEdit_textChanged();
    void on_saveAsButton_clicked();
    void on_requestPaymentCheckBox_toggled(bool fChecked);

    void updateDisplayUnit();

private:
    Ui::QRCodeDialog *ui;
    OptionsModel *model;
    QString address;
    QImage myImage;

    void genCode();
    QString getURI();
};

#endif // BITCOIN_QT_QRCODEDIALOG_H
