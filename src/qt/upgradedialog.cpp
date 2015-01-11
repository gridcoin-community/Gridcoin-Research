#include "upgradedialog.h"
#include "ui_upgradedialog.h"
#include "clientmodel.h"
#include "version.h"
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

extern void Imker(void *kippel);
extern Upgrader upgrader;

UpgradeDialog::UpgradeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpgradeDialog)
{
    ui->setupUi(this);
}

class Checker: public QObject
{
    Q_OBJECT
public slots:
    void start();
    void check(Upgrader *upgrader, UpgradeDialog *upgradeDialog);

signals:
    void change(int percentage);
    void enableUpgradeButton(bool state);
    void enableretryDownloadButton(bool state);
};

#include "upgradedialog.moc"

void Checker::start()
{
    
}

void Checker::check(Upgrader *upgraders, UpgradeDialog *upgradeDialog)
{
    printf("Checker initialized\n");
    while(upgrader.downloading())
    {
        emit(change(upgrader.getFilePerc(upgrader.getFileDone())));
        // printf("Delta: %i\n", upgrader.getFilePerc(upgrader.getFileDone()));
        #ifdef WIN32
        Sleep(1000);
        #else
        usleep(1000);
        #endif
    }
    connect(this, SIGNAL(enableretryDownloadButton(bool)), upgradeDialog, SLOT(enableretryDownloadButton(bool)));
    if (upgrader.downloadSuccess())
    {
        emit(change(100)); // 99 is filthy
        connect(this, SIGNAL(enableUpgradeButton(bool)), upgradeDialog, SLOT(enableUpgradeButton(bool)));
        emit(enableUpgradeButton(true));
        emit(enableretryDownloadButton(false));
    }
    else
    {
        emit(enableretryDownloadButton(true));
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
    if((initialized) && (target!=targo))
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
                cancelDownload();
                initialized = false; 
            }
        // delete horizontalSpacer;
    }
    if (!initialized)
    {
    // re-instantiate Upgrade, in case download was broken off previously
    target = targo;
    if (!upgrader.setTarget(target)) 
        {
            printf("Upgrader already busy 2\n");
            return;
        }
    initialized = true;

    boost::thread(Imker, &upgrader);
    enableUpgradeButton(false);
    enableretryDownloadButton(true);
    ui->retryDownloadButton->setText("Cancel Download");
    QThread *thread = new QThread;
    Checker *checker = new Checker();
    connect(this, SIGNAL(check(Upgrader*, UpgradeDialog*)), checker, SLOT(check(Upgrader*, UpgradeDialog*)));
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
    if (initialized)
    {
        cancelDownload();
        initialized = false;
        ui->retryDownloadButton->setText("Retry Download");
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

void UpgradeDialog::cancelDownload()
{
    upgrader.cancelDownload(true);
}


