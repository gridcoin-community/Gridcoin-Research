// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "upgradeqt.h"
#include "gridcoin/upgrade.h"
#include "util.h"

#include <QtWidgets>
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

bool UpgradeQt::SnapshotMain(QApplication& SnapshotApp)
{
    // This governs the sleep in milliseconds between iterations of the progress polling while loops below.
    unsigned int poll_delay = 1000;

    SnapshotApp.processEvents();
    SnapshotApp.setWindowIcon(QPixmap(":/images/gridcoin"));

    // We use the functions from the core-side Upgrade class for the worker thread and heavy lifting, but the "main" for
    // the Qt side is here rather than the SnapshotMain().
    Upgrade UpgradeMain;

    // Verify a mandatory release is not available before we continue to snapshot download.
    std::string VersionResponse = "";

    if (UpgradeMain.CheckForLatestUpdate(VersionResponse, false, true))
    {
        ErrorMsg(UpgradeMain.ResetBlockchainMessages(Upgrade::UpdateAvailable),
                 UpgradeMain.ResetBlockchainMessages(Upgrade::GithubResponse) + "\r\n" + VersionResponse);

        return false;
    }

    m_Progress = new QProgressDialog("", ToQString(_("Cancel")), 0, 100);
    m_Progress->setWindowModality(Qt::WindowModal);

    m_Progress->setMinimumDuration(0);
    m_Progress->setAutoClose(false);
    m_Progress->setAutoReset(false);
    m_Progress->setValue(0);
    m_Progress->show();

#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);

    m_quitAction = new QAction(tr("E&xit"), this);
    m_quitAction->setToolTip(tr("Quit application"));
    m_quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    m_quitAction->setMenuRole(QAction::QuitRole);

    m_appMenuBar = new QMenuBar();
    QMenu *file = m_appMenuBar->addMenu(tr("&File"));
    file->addAction(m_quitAction);

    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *) m_Progress);
    dockIconHandler->setIcon(QPixmap(":/images/gridcoin"));
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // When doing this for the Qt side, we are only going to use this for the SetType to drive the workflow.
    GRC::Progress worker_progress;

    SnapshotApp.processEvents();

    // Create a thread for snapshot to be downloaded
    boost::thread WorkerMainThread(Upgrade::WorkerMain, boost::ref(worker_progress));

    QString OutputText;

    worker_progress.SetType(Progress::Type::SnapshotDownload);

    std::string BaseProgressString = _("Stage (1/4): Downloading snapshot.zip: Speed ");

    while (!DownloadStatus.GetSnapshotDownloadComplete())
    {
        if (DownloadStatus.GetSnapshotDownloadFailed())
        {
            ErrorMsg(_("Failed to download snapshot.zip; See debug.log"), _("The wallet will now shutdown."));

            return false;
        }

        if (DownloadStatus.GetSnapshotDownloadSpeed() < 1000000 && DownloadStatus.GetSnapshotDownloadSpeed() > 0)
            OutputText = ToQString(BaseProgressString + RoundToString((DownloadStatus.GetSnapshotDownloadSpeed() / (double)1000), 1) + " " + _("KB/s")
                                   + " (" + RoundToString(DownloadStatus.GetSnapshotDownloadAmount() / (double)(1024 * 1024 * 1024), 2) + _("GB/")
                                   + RoundToString(DownloadStatus.GetSnapshotDownloadSize() / (double)(1024 * 1024 * 1024), 2) + _("GB)"));

        else if (DownloadStatus.GetSnapshotDownloadSpeed() > 1000000)
            OutputText = ToQString(BaseProgressString + RoundToString((DownloadStatus.GetSnapshotDownloadSpeed() / (double)1000000), 1) + " " + _("MB/s")
                                   + " (" + RoundToString(DownloadStatus.GetSnapshotDownloadAmount() / (double)(1024 * 1024 * 1024), 2) + _("GB/")
                                   + RoundToString(DownloadStatus.GetSnapshotDownloadSize() / (double)(1024 * 1024 * 1024), 2) + _("GB)"));

        // Not supported
        else
            OutputText = ToQString(BaseProgressString + " " + _("N/A"));

        m_Progress->setLabelText(OutputText);
        m_Progress->setValue(DownloadStatus.GetSnapshotDownloadProgress());

        SnapshotApp.processEvents();

        if (m_Progress->wasCanceled())
        {
            if (CancelOperation())
            {
                fCancelOperation = true;

                WorkerMainThread.interrupt();
                WorkerMainThread.join();

                Msg(_("Snapshot operation canceled."), _("The wallet will now shutdown."));

                return false;
            }
            // Avoid the window disappearing for 1 second after a reset
            else
            {
                m_Progress->reset();

                continue;
            }
        }

        UninterruptibleSleep(std::chrono::milliseconds{poll_delay});
    }

    m_Progress->reset();
    m_Progress->setValue(0);
    m_Progress->setLabelText(ToQString(_("Stage (2/4): Verify SHA256SUM of snapshot.zip")));

    SnapshotApp.processEvents();

    worker_progress.SetType(Progress::Type::SHA256SumVerification);

    while (!DownloadStatus.GetSHA256SUMComplete())
    {
        if (DownloadStatus.GetSHA256SUMFailed())
        {
            ErrorMsg(_("Failed to download snapshot.zip; See debug.log"), _("The wallet will now shutdown."));

            return false;
        }

        m_Progress->setValue(DownloadStatus.GetSHA256SUMProgress());

        SnapshotApp.processEvents();

        if (m_Progress->wasCanceled())
        {
            if (CancelOperation())
            {
                fCancelOperation = true;

                WorkerMainThread.interrupt();
                WorkerMainThread.join();

                Msg(_("Snapshot operation canceled."), _("The wallet will now shutdown."));

                return false;
            }
            // Avoid the window disappearing for 1 second after a reset
            else
            {
                m_Progress->reset();

                continue;
            }
        }

        UninterruptibleSleep(std::chrono::milliseconds{poll_delay});
    }

    m_Progress->reset();
    m_Progress->setValue(0);
    m_Progress->setLabelText(ToQString(_("Stage (3/4): Cleanup blockchain data")));

    SnapshotApp.processEvents();

    worker_progress.SetType(Progress::Type::CleanupBlockchainData);

    while (!DownloadStatus.GetCleanupBlockchainDataComplete())
    {
        if (DownloadStatus.GetCleanupBlockchainDataFailed())
        {
            ErrorMsg(_("Failed to download snapshot.zip; See debug.log"), _("The wallet will now shutdown."));

            return false;
        }

        m_Progress->setValue(DownloadStatus.GetCleanupBlockchainDataProgress());

        SnapshotApp.processEvents();

        if (m_Progress->wasCanceled())
        {
            if (CancelOperation())
            {
                fCancelOperation = true;

                WorkerMainThread.interrupt();
                WorkerMainThread.join();

                Msg(_("Snapshot operation canceled."), _("The wallet will now shutdown."));

                return false;
            }
            // Avoid the window disappearing for 1 second after a reset
            else
            {
                m_Progress->reset();

                continue;
            }
        }

        UninterruptibleSleep(std::chrono::milliseconds{poll_delay});
    }

    m_Progress->reset();
    m_Progress->setValue(0);
    m_Progress->setLabelText(ToQString(_("Stage (4/4): Extracting snapshot.zip")));

    SnapshotApp.processEvents();

    worker_progress.SetType(Progress::Type::SnapshotExtraction);

    while (!ExtractStatus.GetSnapshotExtractComplete())
    {
        if (m_Progress->wasCanceled())
        {
            if (CancelOperation())
            {
                fCancelOperation = true;

                WorkerMainThread.interrupt();
                WorkerMainThread.join();

                Msg(_("Snapshot operation canceled."), _("The wallet will now shutdown."));

                return false;
            }
            // Avoid the window disappearing for 1 second after a reset
            else
            {
                m_Progress->reset();

                continue;
            }

        }

        if (ExtractStatus.GetSnapshotZipInvalid())
        {
            fCancelOperation = true;

            WorkerMainThread.interrupt();
            WorkerMainThread.join();

            Msg(_("Snapshot operation canceled due to an invalid snapshot zip."), _("The wallet will now shutdown."));

            return false;
        }

        if (ExtractStatus.GetSnapshotExtractFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();

            ErrorMsg(_("Snapshot extraction failed! Cleaning up any extracted data"), _("The wallet will now shutdown."));

            // Do this without checking on success, If it passed in stage 3 it will pass here.
            UpgradeMain.CleanupBlockchainData();

            return false;
        }

        m_Progress->setValue(ExtractStatus.GetSnapshotExtractProgress());

        SnapshotApp.processEvents();

        UninterruptibleSleep(std::chrono::milliseconds{poll_delay});
    }

    m_Progress->setValue(100);

    WorkerMainThread.interrupt();
    WorkerMainThread.join();

    SnapshotApp.processEvents();

#ifdef Q_OS_MAC
    delete m_appMenuBar;
#endif

    Msg(_("Snapshot operation successful!"), _("The wallet is now shutting down. Please restart your wallet."));

    return true;
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

bool UpgradeQt::CancelOperation()
{
    // It'll only be 1 or 0
    return Msg(_("Cancel snapshot operation?"), _("Are you sure you want to cancel the snapshot operation?"), true);
}

void UpgradeQt::DeleteSnapshot()
{
    // File is out of scope now check if it exists and if so delete it.
    // This covers partial downloaded files or a http response downloaded into file.
    std::string snapshotfile = gArgs.GetArg("-snapshoturl",
                                            "https://download.gridcoin.us/download/downloadstake/signed/snapshot.zip");

    size_t pos = snapshotfile.find_last_of("/");

    snapshotfile = snapshotfile.substr(pos + 1, (snapshotfile.length() - pos - 1));

    try
    {
        fs::path snapshotpath = GetDataDir() / snapshotfile;

        if (fs::exists(snapshotpath))
            if (fs::is_regular_file(snapshotpath))
                fs::remove(snapshotpath);
    }

    catch (fs::filesystem_error& e)
    {
        LogPrintf("Snapshot Downloader: Exception occurred while attempting to delete snapshot (%s)", e.code().message());
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
