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
}

// void UpgradeDialog::connectR(Upgrader *upgrader)
// {
//     // connect(upgrader,SIGNAL(updatePerc(percentage)),this,SLOT(setPerc(percentage)));
// }

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
