#include "aboutdialog.h"
#include "qt/decoration.h"
#include "ui_aboutdialog.h"
#include "clientmodel.h"
#include "updatedialog.h"
#include "util.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->copyrightLabel->setText("Copyright 2009-2024 The Bitcoin/Peercoin/Black-Coin/Gridcoin developers");

    resize(GRC::ScaleSize(this, width(), height()));

    if (!fTestNet) {
        connect(ui->versionInfoButton, &QAbstractButton::pressed, this, [this]() { handlePressVersionInfoButton(); });
    } else {
        ui->versionInfoButton->setDisabled(true);
        ui->versionInfoButton->setToolTip(tr("Version information is not available on testnet."));
    }
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

void AboutDialog::handlePressVersionInfoButton()
{
    std::string client_message_out;
    std::string change_log;
    GRC::Upgrade::UpgradeType upgrade_type = GRC::Upgrade::UpgradeType::Unknown;


    GRC::Upgrade::CheckForLatestUpdate(client_message_out, change_log, upgrade_type, false, false);

    if (client_message_out == std::string {}) {
        client_message_out = "No response from GitHub - check network connectivity.";
        change_log = " ";
    }

    UpdateDialog update_dialog;

    update_dialog.setWindowTitle("Gridcoin Version Information");
    update_dialog.setVersion(QString().fromStdString(client_message_out));
    update_dialog.setUpgradeType(static_cast<GRC::Upgrade::UpgradeType>(upgrade_type));
    update_dialog.setDetails(QString().fromStdString(change_log));
    update_dialog.setModal(false);

    update_dialog.exec();
}
