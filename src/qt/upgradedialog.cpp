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



UpgradeDialog::UpgradeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpgradeDialog)
{
    ui->setupUi(this);

    // retrieve information about download
    long int size = 0;
    int perc = 0;
    
    // while (perc < 50)
    // {
    size = upgrader->getFileDone();
    // setPerc(upgrader->getFilePerc(size));
    // }
}

// void UpgradeDialog::connectR(Upgrader *upgrader)
// {
//     // connect(upgrader,SIGNAL(updatePerc(percentage)),this,SLOT(setPerc(percentage)));
// }

void UpgradeDialog::setPerc(long int percentage)
{
    ui->progressBar->setValue((int)percentage);
}

UpgradeDialog::~UpgradeDialog()
{
    delete ui;
}

void UpgradeDialog::on_buttonBox_accepted()
{
    close();
}
