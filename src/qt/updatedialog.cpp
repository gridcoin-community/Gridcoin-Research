// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "updatedialog.h"
#include "qicon.h"
#include "qstyle.h"
#include "qt/decoration.h"

#include "ui_updatedialog.h"

UpdateDialog::UpdateDialog(QWidget* parent)
    : QDialog(parent)
      , ui(new Ui::UpdateDialog)
{
    ui->setupUi(this);

    resize(GRC::ScaleSize(this, width(), height()));
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}

void UpdateDialog::setVersion(QString version)
{
    ui->versionData->setText(version);
}

void UpdateDialog::setDetails(QString message)
{
    ui->versionDetails->setText(message);
}

void UpdateDialog::setUpgradeType(GRC::Upgrade::UpgradeType upgrade_type)
{
    QIcon info_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
    QIcon warning_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    QIcon unknown_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion);

    switch (upgrade_type) {
    case GRC::Upgrade::UpgradeType::Mandatory:
        [[fallthrough]];
    case GRC::Upgrade::UpgradeType::Unsupported:
        ui->infoIcon->setPixmap(GRC::ScaleIcon(this, warning_icon, 48));
        break;
    case GRC::Upgrade::UpgradeType::Leisure:
        ui->infoIcon->setPixmap(GRC::ScaleIcon(this, info_icon, 48));
        break;
    case GRC::Upgrade::Unknown:
        ui->infoIcon->setPixmap(GRC::ScaleIcon(this, unknown_icon, 48));
        break;
    }
}
