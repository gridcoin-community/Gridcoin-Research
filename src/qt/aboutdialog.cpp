#include "aboutdialog.h"
#include "qt/decoration.h"
#include "ui_aboutdialog.h"
#include "clientmodel.h"
#include "updatedialog.h"
#include "util.h"

#include <QtConcurrent>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    QString copyrightText = "Copyright 2009-";
    std::variant<int, QString> copyright_year = COPYRIGHT_YEAR;

    try {
        copyrightText += QString::number(std::get<int>(copyright_year));
    } catch (const std::bad_variant_access& e) {
        try {
            copyrightText += std::get<QString>(copyright_year);
        } catch (const std::bad_variant_access& e) {
            copyrightText += "Present";
        }
    }

    copyrightText +=  " The Bitcoin/Peercoin/Black-Coin/Gridcoin developers";

    ui->copyrightLabel->setText(copyrightText);

    resize(GRC::ScaleSize(this, width(), height()));

    connect(&m_version_check_watcher, &QFutureWatcher<AboutVersionInfo>::finished,
            this, &AboutDialog::versionCheckFinished);

    if (!fTestNet && !gArgs.GetBoolArg("-disableupdatecheck", false)) {
        connect(ui->versionInfoButton, &QAbstractButton::pressed, this, [this]() { handlePressVersionInfoButton(); });
    } else if (gArgs.GetBoolArg("-disableupdatecheck", false)) {
        ui->versionInfoButton->setDisabled(true);
        ui->versionInfoButton->setToolTip(tr("Version information and update check has been disabled "
                                             "by config or startup parameter."));
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
    // CheckForLatestUpdate does a blocking libcurl GET of the GitHub release
    // JSON (up to a 10 s connect timeout). Run it off the GUI thread so a click
    // never freezes the UI. Guard against overlapping checks: a second click
    // while one is in flight would spawn another worker (this is the read-only
    // half of GRC::Upgrade; it must not touch the download/reset write side).
    if (m_version_check_watcher.isRunning()) {
        return;
    }

    ui->versionInfoButton->setDisabled(true);

    m_version_check_watcher.setFuture(QtConcurrent::run([]() {
        AboutVersionInfo info;
        GRC::Upgrade::UpgradeType upgrade_type = GRC::Upgrade::UpgradeType::Unknown;
        GRC::Upgrade::CheckForLatestUpdate(info.version, info.details, upgrade_type, false, false);
        info.upgrade_type = static_cast<int>(upgrade_type);
        return info;
    }));
}

void AboutDialog::versionCheckFinished()
{
    ui->versionInfoButton->setDisabled(false);

    AboutVersionInfo info = m_version_check_watcher.result();

    if (info.version.empty()) {
        info.version = "No response from GitHub - check network connectivity.";
        info.details = " ";
    }

    UpdateDialog update_dialog;

    update_dialog.setWindowTitle("Gridcoin Version Information");
    update_dialog.setVersion(QString::fromStdString(info.version));
    update_dialog.setUpgradeType(static_cast<GRC::Upgrade::UpgradeType>(info.upgrade_type));
    update_dialog.setDetails(QString::fromStdString(info.details));
    update_dialog.setModal(false);

    update_dialog.exec();
}
