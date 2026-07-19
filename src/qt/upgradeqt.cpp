// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "upgradeqt.h"
#include "gridcoin/upgrade.h"
#include "util.h"

#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QMenuBar>
#include <QProgressDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QIcon>
#include <QString>
#include <boost/thread.hpp>

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

using namespace GRC;

UpgradeQt::UpgradeQt() {}

QString UpgradeQt::ToQString(const std::string& string)
{
    return QString::fromStdString(string);
}

void UpgradeQt::ErrorMsg(const std::string& text, const std::string& informativetext)
{
    QMessageBox ErrorMsg;

    ErrorMsg.setIcon(QMessageBox::Critical);
    ErrorMsg.setText(QString::fromStdString(text));
    ErrorMsg.setInformativeText(QString::fromStdString(informativetext));
    ErrorMsg.setStandardButtons(QMessageBox::Ok);
    ErrorMsg.setDefaultButton(QMessageBox::Ok);

    ErrorMsg.exec();
}

int UpgradeQt::Msg(const std::string& text, const std::string& informativetext, bool question)
{
    QMessageBox Msg;

    Msg.setIcon(QMessageBox::Question);
    Msg.setText(ToQString(text));
    Msg.setInformativeText(ToQString(informativetext));

    if (question)
    {
        Msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        Msg.setDefaultButton(QMessageBox::No);
    }

    else
    {
        Msg.setStandardButtons(QMessageBox::Ok);
        Msg.setDefaultButton(QMessageBox::Ok);
    }

    int result = Msg.exec();

    switch (result)
    {
        case QMessageBox::Yes    :    return 1;
        case QMessageBox::No     :    return 0;
        case QMessageBox::Ok     :    return -1;
        // Should never be reached
        default                  :    return -1;
    }
}

bool UpgradeQt::ResetBlockchain(QApplication& ResetBlockchainApp)
{
    ResetBlockchainApp.processEvents();
    ResetBlockchainApp.setWindowIcon(QPixmap(":/images/gridcoin"));

    Upgrade resetblockchain;

    resetblockchain.CleanupBlockchainData();

    bool fSuccess = (DownloadStatus.GetCleanupBlockchainDataComplete() && !DownloadStatus.GetCleanupBlockchainDataFailed());

    if (fSuccess)
        Msg(_("Reset Blockchain Data: Blockchain data removal was a success"),
            _("The wallet will now shutdown. Please start your wallet to begin sync from zero"), false);

    else
    {
        std::string inftext = resetblockchain.ResetBlockchainMessages(Upgrade::CleanUp);

        ErrorMsg(_("Reset Blockchain Data: Blockchain data removal failed."), inftext);
    }

    return fSuccess;
}
