#ifndef UPGRADEDIALOG_H
#define UPGRADEDIALOG_H

#include <QDialog>
#include "../upgrader.h"

namespace Ui {
    class UpgradeDialog;
}
class ClientModel;

/** "Upgrade" dialog box */
class UpgradeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpgradeDialog(QWidget *parent = 0);
    ~UpgradeDialog();

    void setClientModel(ClientModel *model);
    bool initialized = false;

private:
    Ui::UpgradeDialog *ui;
    ClientModel *clientModel;
    Upgrader upgrader;
    int target;
    void cancelDownload();

private slots:
    void on_retryDownloadButton_clicked();
    void on_upgradeButton_clicked();
    void on_hideButton_clicked();


public slots:
    void upgrade();
    void blocks();
    void initialize(int target);
    void setPerc(int percentage);
    void enableUpgradeButton(bool state);
    void enableretryDownloadButton(bool state);

signals:
    void check(Upgrader *upgrader, UpgradeDialog *UpgradeDialog);
};

#endif // UPGRADEDIALOG_H
