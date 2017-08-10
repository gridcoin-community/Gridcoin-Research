#include "util.h"
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
#include "global_objects.hpp"
#include "global_objects_noui.hpp"
#include <QThread>
#include <QMessageBox>

extern void Imker(void *kippel);
extern Upgrader upgrader;

Checker checker;

enum DOWNLOADSTATE
{
    DOWNLOADING,
    CANCELLED,
    FINISHED
};

UpgradeDialog::UpgradeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpgradeDialog)
{
    ui->setupUi(this);
    QThread *thread = new QThread;
    connect(this, SIGNAL(check()), &checker, SLOT(check()));
    connect(&checker, SIGNAL(change(int)), this, SLOT(setPerc(int)));
    connect(&checker, SIGNAL(enableUpgradeButton(bool)), this, SLOT(enableUpgradeButton(bool)));
    connect(&checker, SIGNAL(enableretryDownloadButton(bool)), this, SLOT(enableretryDownloadButton(bool)));
    connect(&checker, SIGNAL(setDownloadState(int)), this, SLOT(setDownloadState(int)));
    checker.moveToThread(thread);
    thread->start();
}

//#include "upgradedialog.moc"

void Checker::start()
{
    
}

void Checker::check()
{
    if (upgrader.downloading())
    {
        emit(setDownloadState(DOWNLOADING));
    }
    printf("Checker initialized\n");
    while(upgrader.downloading())
    {
        // emit(setDownloadState(DOWNLOADING));
        emit(change(upgrader.getFilePerc(upgrader.getFileDone())));
        #ifdef WIN32
        Sleep(1000);
        #else
        usleep(1000*1000);
        #endif
    }
    if (upgrader.downloadSuccess())
    {
        emit(change(100)); // 99 is filthy
        emit(setDownloadState(FINISHED));
    }
    else
    {
        emit(change(0));
        emit(setDownloadState(CANCELLED));
    }
}

void UpgradeDialog::upgrade()
{
    initialize(QT);
}

void UpgradeDialog::blocks()
{
    initialize(BLOCKS);
}

void UpgradeDialog::initialize(int targo)
{
    if(upgrader.getTarget()!=-1 && upgrader.getTarget()!=targo && (upgrader.downloadSuccess() || upgrader.downloading()))
    {
        QMessageBox changeDownload;
        changeDownload.setWindowTitle((targo == QT)? "Already downloading blocks" : "Already upgrading client");
        changeDownload.setText((targo == QT)? "Cancel and upgrade client instead" : "Cancel and download blocks instead?");
        changeDownload.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        changeDownload.setDefaultButton(QMessageBox::No);
        QFont font = QApplication::font("QWorkspaceTitleBar");
        QFontMetrics metric(font);
        QSpacerItem* horizontalSpacer = new QSpacerItem(metric.width(windowTitle()) + 150, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = (QGridLayout*)changeDownload.layout();
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        if (changeDownload.exec() == QMessageBox::Yes) 
            {
                upgrader.cancelDownload(true);
                downloadThread.join();
            }
        // delete horizontalSpacer;
    }
    if (!upgrader.downloading() && (!upgrader.downloadSuccess() || (upgrader.getTarget()!=-1 && upgrader.getTarget()!=targo)))
    {
    // re-instantiate Upgrade, in case download was broken off previously
    target = targo;
    if (!upgrader.setTarget(target)) 
        {
            printf("Upgrader already busy\n");
            return;
        }
    downloadThread = boost::thread(Imker, &upgrader);
    emit(check());    
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

void UpgradeDialog::setDownloadState(int state)
{

    switch (state)
    {
        case DOWNLOADING:
        {
            enableUpgradeButton(false);
            enableretryDownloadButton(true);
            ui->retryDownloadButton->setText("Cancel Download");
            ui->hideButton->setText("Hide");
            break;
        }
        case FINISHED:
        {
            enableUpgradeButton(true);
            enableretryDownloadButton(false);
            ui->hideButton->setText("Hide");
            break;
        }
        case CANCELLED:
        {
            enableUpgradeButton(false);
            enableretryDownloadButton(true);
            ui->retryDownloadButton->setText("Retry Downloads");
            ui->hideButton->setText("Close");
            break;
        }
    }
}

UpgradeDialog::~UpgradeDialog()
{
    downloadThread.interrupt();
    downloadThread.join();
    delete ui;
}

void UpgradeDialog::on_retryDownloadButton_clicked()
{
    if (upgrader.downloading())
    {
        upgrader.cancelDownload(true);
        setDownloadState(CANCELLED);
    }
    else
    {
        int c = 0;
        while (upgrader.downloading() && c < 20)
        {
            #ifdef WIN32
            Sleep(500);
            #else
            usleep(1000*500);
            #endif
            c++;
            printf("Waiting for curl\n");
        }
        setDownloadState(DOWNLOADING);
        initialize(target);
    }
}

void UpgradeDialog::on_upgradeButton_clicked()
{
    upgrader.launcher(UPGRADER, target);
}

void UpgradeDialog::on_hideButton_clicked()
{
    close();
}
