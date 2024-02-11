#ifndef BITCOIN_QT_ABOUTDIALOG_H
#define BITCOIN_QT_ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
    class AboutDialog;
}
class ClientModel;

/** "About" dialog box */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();

    void setModel(ClientModel *model);
private:
    Ui::AboutDialog *ui;

private slots:
    void on_buttonBox_accepted();
    void handlePressVersionInfoButton();
};

#endif // BITCOIN_QT_ABOUTDIALOG_H
