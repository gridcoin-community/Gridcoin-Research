#include "upgradeqt.h"
#include "upgrade.h"
#include "util.h"
#include "db.h"
#include "txdb-leveldb.h"
#include <QtWidgets>
#include <QProgressDialog>
#include <QMessageBox>
#include <QApplication>
#include <QPixmap>
#include <QIcon>
#include <QString>
#include <boost/thread.hpp>

UpgradeQt::UpgradeQt() {}

QString UpgradeQt::ToQString(const std::string& string)
{
    return QString::fromStdString(string);
}

bool UpgradeQt::SnapshotMain()
{
    // Keep this seperate from the main application
    int xargc = 1;
    char *xargv[] = {(char*)"gridcoinresearch"};

    QApplication SnapshotApp(xargc, xargv);

    SnapshotApp.setOrganizationName("Gridcoin");
    SnapshotApp.setApplicationName("Gridcoin-Qt");
    SnapshotApp.processEvents();
    SnapshotApp.setWindowIcon(QPixmap(":/images/gridcoin"));

    Upgrade UpgradeMain;

    QProgressDialog Progress("", ToQString(_("Cancel")), 0, 100);
    Progress.setWindowModality(Qt::WindowModal);

    Progress.setMinimumDuration(0);
    Progress.setAutoClose(false);
    Progress.setAutoReset(false);
    Progress.setValue(0);
    Progress.show();

    SnapshotApp.processEvents();

    // Create a thread for snapshot to be downloaded
    boost::thread SnapshotDownloadThread(std::bind(&UpgradeQt::DownloadSnapshot, this)); // thread runs free

    std::string BaseProgressString = _("Stage (1/4): Downloading snapshot.zip: Speed ");

    QString OutputText;

    while (!DownloadStatus.SnapshotDownloadComplete)
    {
        if (DownloadStatus.SnapshotDownloadFailed)
        {
            ErrorMsg(_("Failed to download snapshot.zip; See debug.log"), _("Wallet will now shutdown"));

            return false;
        }

        if (DownloadStatus.SnapshotDownloadSpeed < 1000000 && DownloadStatus.SnapshotDownloadSpeed > 0)
            OutputText = ToQString(BaseProgressString + RoundToString((DownloadStatus.SnapshotDownloadSpeed / (double)1000), 1) + _(" KB/s"));

        else if (DownloadStatus.SnapshotDownloadSpeed > 1000000)
            OutputText = ToQString(BaseProgressString + RoundToString((DownloadStatus.SnapshotDownloadSpeed / (double)1000000), 1) + _(" MB/s"));

        // Not supported
        else
            OutputText = ToQString(BaseProgressString + _(" N/A"));

        Progress.setLabelText(OutputText);
        Progress.setValue(DownloadStatus.SnapshotDownloadProgress);

        SnapshotApp.processEvents();

        if (Progress.wasCanceled())
        {
            if (CancelOperation())
            {
                fCancelOperation = true;

                SnapshotDownloadThread.interrupt();
                SnapshotDownloadThread.join();

                Msg(_("Snapshot operation canceled."), _("The wallet will not shutdown."));

                return false;
            }

            // Avoid the window disappearing for 1 second after a reset
            else
            {
                Progress.reset();

                continue;
            }
        }

        MilliSleep(1000);
    }

    Progress.reset();
    Progress.setValue(0);
    Progress.setLabelText(ToQString(_("Stage (2/4): Verify SHA256SUM of snapshot.zip")));

    SnapshotApp.processEvents();

    // Get the snapshot.zip Sha256sum from webserver
    if (UpgradeMain.VerifySHA256SUM())
    {
        Progress.setValue(100);

        SnapshotApp.processEvents();
    }

    else
    {
        ErrorMsg(_("Could not verify SHA256SUM of file against Servers SHA256SUM of snapshot.zip."), _("The wallet will now shutdown."));

        return false;
    }

    if (Progress.wasCanceled())
    {
        if (CancelOperation())
        {
            fCancelOperation = true;

            Msg(_("Snapshot operation canceled."), _("The wallet will not shutdown."));

            return false;
        }       
    }

    // Make it seen
    MilliSleep(3000);

    Progress.reset();
    Progress.setValue(0);
    Progress.setLabelText(ToQString(_("Stage (3/4): Cleanup blockchain data")));

    SnapshotApp.processEvents();

    // Clean up the blockchain data
    if (UpgradeMain.CleanupBlockchainData())
    {
        Progress.setValue(100);

        SnapshotApp.processEvents();
    }

    else
    {
        ErrorMsg(_("Could not cleanup previous blockchain data."), _("The wallet will now shutdown."));

        return false;
    }

    if (Progress.wasCanceled())
    {
        if (CancelOperation())
        {
            fCancelOperation = true;

            Msg(_("Snapshot operation canceled."), _("The wallet will not shutdown."));

            return false;
        }
    }

    // Make it seen
    MilliSleep(3000);

    Progress.reset();
    Progress.setValue(0);

    Progress.setLabelText(ToQString(_("Stage (4/4): Extracting snapshot.zip")));

    SnapshotApp.processEvents();

    // Extract Snapshot
    // Create a thread for snapshot to be extracted
    boost::thread SnapshotExtractThread(std::bind(&UpgradeQt::ExtractSnapshot, this));

    while (!ExtractStatus.SnapshotExtractComplete)
    {
        if (Progress.wasCanceled())
        {
            if (CancelOperation())
            {
                fCancelOperation = true;

                SnapshotDownloadThread.interrupt();
                SnapshotDownloadThread.join();

                Msg(_("Snapshot operation canceled."), _("The wallet will not shutdown."));

                return false;
            }

            // Avoid the window disappearing for 1 second after a reset
            else
            {
                Progress.reset();

                continue;
            }

        }

        if (ExtractStatus.SnapshotExtractFailed)
        {
            ErrorMsg(_("Snapshot extraction failed! Cleaning up any extracted data"), _("The wallet will now shutdown."));

            // Do this without checking on sucess, If it passed in stage 3 it will pass here.
            UpgradeMain.CleanupBlockchainData();

            return false;
        }

        Progress.setValue(ExtractStatus.SnapshotExtractProgress);

        SnapshotApp.processEvents();

        MilliSleep(1000);
    }

    Progress.setValue(100);

    SnapshotApp.processEvents();

    Msg(_("Snapshot Operation successful!"), _("The wallet is now shutting down. Please restart your wallet."));

    return true;
}

void UpgradeQt::DownloadSnapshot()
{
   RenameThread("grc-snapshotdl");

   Upgrade upgrade;

   upgrade.DownloadSnapshot();
}

void UpgradeQt::ExtractSnapshot()
{
    RenameThread("grc-snapshotex");

    Upgrade upgrade;

    upgrade.ExtractSnapshot();
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
