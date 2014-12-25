#ifndef UPGRADEDIALOG_H
#define UPGRADEDIALOG_H

#include <QDialog>
#include "../upgrader.h"

namespace Ui {
    class UpgradeDialog;
}

/** "Upgrade" dialog box */
class UpgradeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpgradeDialog(QWidget *parent = 0);
    ~UpgradeDialog();
    Upgrader *upgrader;
    // void connectR(Upgrader *upgrader);
    void getPerc(Upgrader* upgrader)
	{
    	upgrader->getFileDone();
	}

private:
    Ui::UpgradeDialog *ui;
    void setPerc(long int percentage);

private slots:
    void on_buttonBox_accepted();

public slots:
	// void setPerc(long int percentage);
};

#endif // UPGRADEDIALOG_H
