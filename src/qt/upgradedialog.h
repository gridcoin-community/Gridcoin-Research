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
	bool performUpgrade = false;
	bool downloading = false;
	bool initialized = false;

private:
    Ui::UpgradeDialog *ui;
    ClientModel *clientModel;
    Upgrader upgrader;

private slots:
    void on_retryDownloadButton_clicked();
    void on_upgradeButton_clicked();
    void on_hideButton_clicked();


public slots:
	void upgrade();
	void setPerc(int percentage);
	bool requestRestart();
	void enableUpgradeButton(bool state);
	void enableretryDownloadButton(bool state);

signals:
	void check(Upgrader *upgrader, UpgradeDialog *UpgradeDialog);
};

#endif // UPGRADEDIALOG_H
