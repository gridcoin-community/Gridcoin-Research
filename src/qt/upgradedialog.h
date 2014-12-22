#ifndef UPGRADEDIALOG_H
#define UPGRADEDIALOG_H

#include <QDialog>

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
    // void connectR(Upgrader *upgrader);

private:
    Ui::UpgradeDialog *ui;

private slots:
    void on_buttonBox_accepted();

public slots:
	void setPerc(int percentage);
};

#endif // UPGRADEDIALOG_H
