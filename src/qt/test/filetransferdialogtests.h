// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_FILETRANSFERDIALOGTESTS_H
#define BITCOIN_QT_TEST_FILETRANSFERDIALOGTESTS_H

#include <QObject>
#include <QTest>

//! The GUI's transfer worker and progress dialog (qt/filetransferdialog.h), driven by in-process
//! sources and sinks: no core and no IPC. The cases observe the dialog from the GUI thread while
//! the transfer runs on the worker.
class FileTransferDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void workerRunsJobsInOrderOnOneThread();
    void workerRunsQueuedJobsBeforeItGoes();
    void downloadWithoutWorkerIsALocalError();
    void downloadShowsProgressAndSavesTheFile();
    void cancelStopsTheDownloadAndLeavesNothing();
    void uploadSendsTheFile();
    void guardFilesComeFromTheCorePaths();
    void backupResultText();
    // Last: it tells the application to quit, then clears that again.
    void loopExitCancelsAndWaitsForTheTransfer();
};

#endif // BITCOIN_QT_TEST_FILETRANSFERDIALOGTESTS_H
