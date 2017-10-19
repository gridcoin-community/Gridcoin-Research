#ifndef UPGRADEDIALOG_H
#define UPGRADEDIALOG_H

#include <QDialog>
#ifndef Q_MOC_RUN
// #include "../upgrader.h"
#endif

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

private:
    Ui::UpgradeDialog *ui;
    ClientModel *clientModel;
    int target;
    boost::thread downloadThread;

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
    void setDownloadState(int state);

signals:
    void check();
};

class Checker: public QObject
{
    Q_OBJECT
public slots:
    void start();
    void check();

signals:
    void setDownloadState(int state);
    void change(int percentage);
    void enableUpgradeButton(bool state);
    void enableretryDownloadButton(bool state);
};

#endif // UPGRADEDIALOG_H
