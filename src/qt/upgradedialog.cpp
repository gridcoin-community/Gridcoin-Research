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
#include <QMessageBox>

bool downloadDone;

UpgradeDialog::UpgradeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpgradeDialog)
{
    ui->setupUi(this);
}

// Placed in a dedicated thread to run downloader from upgrader

void Imker(void *kippel)
{
    std::string target = "snapshot.zip";
    std::string source = "snapshot.zip";
    Upgrader *upgrader = (Upgrader*)kippel;
    upgrader->downloader(target, DATA, source);
    downloadDone = true;
    // upgrader->unzipper(target, DATA);
}

class Checker: public QObject
{
    Q_OBJECT
public slots:
    void start();
    void check(Upgrader *upgrader, UpgradeDialog *upgradeDialog);

signals:
    void change(int percentage);
    void requestRestart();
    void enableUpgradeButton(bool state);
    void enableretryDownloadButton(bool state);
};

#include "upgradedialog.moc"

void Checker::start()
{
    
}

void Checker::check(Upgrader *upgrader, UpgradeDialog *upgradeDialog)
{
    while(!downloadDone)
    {
        emit(change(upgrader->getFilePerc(upgrader->getFileDone())));
        usleep(1000*800);
    }
    emit(change(100)); // 99 is filthy
    upgradeDialog->downloading = false;
    emit(requestRestart());
    connect(this, SIGNAL(enableUpgradeButton(bool)), upgradeDialog, SLOT(enableUpgradeButton(bool)));
    connect(this, SIGNAL(enableretryDownloadButton(bool)), upgradeDialog, SLOT(enableretryDownloadButton(bool)));
    emit(enableUpgradeButton(true));
    emit(enableretryDownloadButton(true));
    
}

bool UpgradeDialog::requestRestart()
{
    // QMessageBox msgBox;
    // msgBox.setText("The document has been modified.");
    // msgBox.exec();
    // return true;
}

void UpgradeDialog::upgrade()
{
    if (!initialized)
    {
    // re-instantiate Upgrade, in case download was broken off previously
    Upgrader jim;
    upgrader = jim;
    initialized = true;
    
    void *alf = &upgrader;
    downloadDone = false;
    downloading = true;
    boost::thread(Imker, alf);
    // enableUpgradeButton(false);
    enableUpgradeButton(true); // TEMP
    enableretryDownloadButton(false);
    QThread* thread = new QThread;
    Checker *checker = new Checker();
    connect(this, SIGNAL(check(Upgrader*, UpgradeDialog*)), checker, SLOT(check(Upgrader*, UpgradeDialog*)));
    connect(checker, SIGNAL(requestRestart()), this, SLOT(requestRestart()));
    connect(checker, SIGNAL(change(int)), this, SLOT(setPerc(int)));
    checker->moveToThread(thread);
    thread->start();
    emit(check(&upgrader, this));    
    }
}

void UpgradeDialog::setPerc(int percentage)
{
    ui->progressBar->setValue(percentage);
}

void UpgradeDialog::enableUpgradeButton(bool state)
{
    ui->upgradeButton->setEnabled(state);
}

void UpgradeDialog::enableretryDownloadButton(bool state)
{
    ui->retryDownloadButton->setEnabled(state);
}

UpgradeDialog::~UpgradeDialog()
{
    delete ui;
}

void UpgradeDialog::on_retryDownloadButton_clicked()
{
    initialized = false;
    upgrade();
}

void UpgradeDialog::on_upgradeButton_clicked()
{
    upgrader.upgrade();
}

void UpgradeDialog::on_hideButton_clicked()
{
    close();
}


