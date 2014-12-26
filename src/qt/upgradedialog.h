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
    void getPerc(Upgrader* upgrader)
	{
    	upgrader->getFileDone();
	}
	bool performUpgrade = false;

private:
    Ui::UpgradeDialog *ui;
    ClientModel *clientModel;
    Upgrader upgrader;
    int historyPtr;

private slots:
    void on_buttonBox_accepted();

public slots:
	void upgrade();
	void setPerc(int percentage);

signals:
	void check(Upgrader *upgrader, UpgradeDialog *UpgradeDialog);
};

#endif // UPGRADEDIALOG_H
