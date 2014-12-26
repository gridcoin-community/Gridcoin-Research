#include "upgradedialog.h"
#include "ui_upgradedialog.h"
#include "clientmodel.h"
#include "version.h"
#ifdef WIN32
#include <QAxObject>
#endif
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "uint256.h"
#include "base58.h"
#include "../global_objects.hpp"
#include "../global_objects_noui.hpp"
#include <QThread>

bool downloadDone;
bool downloading = false;

UpgradeDialog::UpgradeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpgradeDialog),
    historyPtr(0)
{
    ui->setupUi(this);
}

void Imker(void *kippel)
{
    std::string target = "snapshot.zip";
    std::string source = "signed/snapshot.zip";
    Upgrader *upgrader = (Upgrader*)kippel;
    upgrader->downloader(target, DATA, source);
    downloadDone = true;
    // upgrader->unzipper(target, DATA);
}

/* Object for executing console RPC commands in a separate thread.
*/
class Checker: public QObject
{
    Q_OBJECT
public slots:
    void start();
    void check(Upgrader *upgrader, UpgradeDialog *upgradeDialog);

signals:
    void change(int percentage);
};

#include "upgradedialog.moc"

void Checker::start()
{
    
}

void Checker::check(Upgrader *upgrader, UpgradeDialog *upgradeDialog)
{
    while(!downloadDone)
    {
        emit(change((int)upgrader->getFilePerc(upgrader->getFileDone())));
        usleep(1000*800);
    }
    downloading = false;    
}

void UpgradeDialog::upgrade()
{
    if (!downloading)
    {

    // re-instantiate Upgrade, in case download was broken off previously
    Upgrader jim;
    upgrader = jim;
    
    void *alf = &upgrader;
    downloadDone = false;
    downloading = true;
    boost::thread j(Imker, alf);
    // j.join();
    // boost::thread h(checkOnUpgrade, this);
    QThread* thread = new QThread;
    Checker *executor = new Checker();
    connect(this, SIGNAL(check(Upgrader*, UpgradeDialog*)), executor, SLOT(check(Upgrader*, UpgradeDialog*)));
    connect(executor, SIGNAL(change(int)), this, SLOT(setPerc(int)));
    executor->moveToThread(thread);
    thread->start();
    emit(check(&upgrader, this));    
    }
}

void UpgradeDialog::setPerc(int percentage)
{
    ui->progressBar->setValue(percentage);
}

UpgradeDialog::~UpgradeDialog()
{
    delete ui;
}

void UpgradeDialog::on_buttonBox_accepted()
{
    close();
}
