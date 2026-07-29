#include "aboutdialog.h"
#include "qt/decoration.h"
#include "ui_aboutdialog.h"
#include "clientmodel.h"
#include "updatedialog.h"
#include "interfaces/node.h"
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
}

void AboutDialog::setModel(ClientModel *model)
{
    // Set unconditionally so a teardown setModel(nullptr) clears the pointer
    // rather than leaving it dangling at the previous model's node.
    m_node = model ? &model->node() : nullptr;

    if(model)
    {
        ui->versionLabel->setText(model->formatFullVersion());

        // Wire the version-info / update-check button. Testnet status is read
        // through the client model's node interface (ClientModel::isTestNet ->
        // interfaces::Node), never by calling chainparams (OnTestnet()) from GUI
        // code; done here rather than in the constructor because the model is
        // only available once set. -disableupdatecheck is a config read.
        if (!model->isTestNet() && !gArgs.GetBoolArg("-disableupdatecheck", false)) {
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
}

void AboutDialog::setIpcConnectionInfo(const GuiIpcInfo& info)
{
    // In the monolithic build the GUI and node share one process, so there is no
    // IPC connection to describe -- hide the section.
    if (!info.active) {
        ui->ipcInfoLabel->hide();
        return;
    }

    // The label is Qt::RichText, so every interpolated value must be HTML-escaped
    // before insertion: the node-sourced strings (versions, build date, identity,
    // network) arrive over IPC from the daemon and must not be able to inject
    // markup, and a benign '&' or '<' in a local value (e.g. a data-dir path in
    // the socket) would otherwise break rendering. Captions are our own literals.
    const QString identity = info.node_identity.isEmpty()
        ? tr("unavailable") : info.node_identity.toHtmlEscaped();

    const QString text =
        tr("<b>Multiprocess connection</b>") + QStringLiteral("<br>") +
        tr("GUI version: %1").arg(info.gui_version.toHtmlEscaped()) + QStringLiteral("<br>") +
        tr("Node version: %1").arg(info.node_version.toHtmlEscaped()) + QStringLiteral("<br>") +
        tr("Node built: %1").arg(info.node_built_at.toHtmlEscaped()) + QStringLiteral("<br>") +
        tr("IPC schema %1, protocol %2").arg(info.ipc_schema.toHtmlEscaped(), info.ipc_protocol.toHtmlEscaped()) + QStringLiteral("<br>") +
        tr("Node identity: %1 (%2)").arg(identity, info.network.toHtmlEscaped()) + QStringLiteral("<br>") +
        tr("Socket: %1").arg(info.socket_path.toHtmlEscaped());

    ui->ipcInfoLabel->setText(text);
    ui->ipcInfoLabel->show();
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
    // No node (dialog shown before setModel(), or cleared at teardown): nothing
    // to query. The captured pointer below must be non-null off the GUI thread.
    if (!m_node) {
        return;
    }

    // CheckForLatestUpdate does a blocking libcurl GET of the GitHub release
    // JSON (up to a 10 s connect timeout). Run it off the GUI thread so a click
    // never freezes the UI. Guard against overlapping checks: a second click
    // while one is in flight would spawn another worker (this is the read-only
    // half of GRC::Upgrade; it must not touch the download/reset write side).
    if (m_version_check_watcher.isRunning()) {
        return;
    }

    ui->versionInfoButton->setDisabled(true);

    m_version_check_watcher.setFuture(QtConcurrent::run([node = m_node]() {
        interfaces::LatestVersionInfo latest = node->checkForLatestUpdate();
        return AboutVersionInfo{latest.version, latest.details, latest.upgrade_type};
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
    // info.upgrade_type is an int carrying a GRC::Upgrade::UpgradeType value
    // (from interfaces::Node::checkForLatestUpdate, which static_asserts the
    // mapping node-side); UpdateDialog consumes it as its own UpdateType mirror.
    update_dialog.setUpgradeType(info.upgrade_type);
    update_dialog.setDetails(QString::fromStdString(info.details));
    update_dialog.setModal(false);

    update_dialog.exec();
}
