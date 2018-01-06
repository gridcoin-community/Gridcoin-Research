#include "aboutdialog.h"
#include "ui_aboutdialog.h"
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



AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // Set current copyright year and boinc utilization
    QString cr = "Copyright 2009-2018 The Bitcoin/Peercoin/Black-Coin/Gridcoin developers";
    std::string sBoincUtilization="";
    sBoincUtilization = strprintf("%d",nBoincUtilization);
	QString qsUtilization = QString::fromUtf8(sBoincUtilization.c_str());
	QString qsRegVersion  = QString::fromUtf8(sRegVer.c_str());
	ui->copyrightLabel->setText("Boinc Magnitude: " + qsUtilization + "              " + ", Registered Version: " + qsRegVersion + "             " + cr);
   // setTheme(this, THEME_ABOUTDIALOG);

}


void AboutDialog::setModel(ClientModel *model)
{
    if(model)
    {
        ui->versionLabel->setText(model->formatFullVersion());
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_buttonBox_accepted()
{
    close();
}
