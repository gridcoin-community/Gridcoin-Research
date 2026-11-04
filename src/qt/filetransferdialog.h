// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_FILETRANSFERDIALOG_H
#define GRIDCOIN_QT_FILETRANSFERDIALOG_H

#include "fs.h"
#include "interfaces/file_transfer.h"

#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace interfaces {
struct CorePaths;
} // namespace interfaces

//! Runs DownloadEntry on the process's modal GuiIpcWorker with a modal QProgressDialog (bytes done /
//! total, Cancel). Returns the outcome.
//!
//! Without that worker the outcome is a LocalError. Cancel
//! stops the transfer, and the dialog stays up, saying so, until it has stopped. The transfer has
//! always finished when this returns, also when the event loop is told to quit meanwhile, which
//! cancels it.
interfaces::TransferOutcome RunDownloadWithProgress(QWidget* parent, const QString& title, interfaces::FileSource& src,
                                                    uint32_t entry, const fs::path& dest);

//! UploadFile on the worker, with the same dialog as RunDownloadWithProgress.
interfaces::FileSinkResult RunUploadWithProgress(QWidget* parent, const QString& title, interfaces::FileSink& sink,
                                                 const fs::path& local);

//! The work of a transfer and what comes with it, run on the modal worker: \p progress is told the
//! bytes done of the total, and \p cancel is set when the user cancels.
using TransferJobFn = std::function<void(const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel)>;

//! Runs \p job on the modal GuiIpcWorker with the dialog of RunDownloadWithProgress, for work that is
//! more than one transfer: a console file command also begins its transfer, which can run the
//! command in the core first, and ends it. The dialog shows \p what, and the bytes once the job
//! reports them. Cancel sets the job's flag, and the dialog stays up, saying so, until the job has
//! returned. The job has always finished when this returns. False, with \p error set, when there is
//! no worker or the job threw.
bool RunJobWithProgress(QWidget* parent, const QString& title, const QString& what, TransferJobFn job,
                        std::string& error);

//! Makes the core begin a backup, and returns the source of its copies, or null with \p error set.
using BackupBeginFn = std::function<std::unique_ptr<interfaces::FileSource>(std::string& error)>;

//! Runs a whole backup on the modal GuiIpcWorker under one dialog of RunJobWithProgress: \p begin,
//! then each entry saved to the destination at the same index of \p dests (DownloadBackupEntries),
//! then the source's destruction, which deletes what the core has not sent. The begin makes the
//! core copy the items, which takes as long as the wallet is large; the dialog says "Preparing the
//! backup..." meanwhile, and shows the bytes of each entry once they arrive. A cancel while the
//! copies are made takes effect when the begin returns: nothing is written, and the first entry's
//! outcome is Cancelled. The backup has always finished when this returns.
//!
//! False, with \p error set and \p results empty, when the begin returned no source, there is no
//! worker, or the job threw (a lost connection).
bool RunBackupWithProgress(QWidget* parent, const QString& title, BackupBeginFn begin,
                           const std::vector<fs::path>& dests, std::vector<interfaces::BackupEntryResult>& results,
                           std::string& error);

//! The core's files a backup must not be written over (CheckDestination's core_files): its wallet
//! file, configuration file, settings file and debug log. A path the core did not report is left
//! out.
std::vector<fs::path> BackupGuardFiles(const interfaces::CorePaths& paths);

//! The message a backup ends with, a line per entry: "Saved <n> bytes to <path>." for an entry that
//! arrived, the note of one the core had nothing to send for, and the message of the one that did
//! not arrive, after its name. \p failed is set when an entry failed; a cancel is not a failure.
QString BackupResultText(const std::vector<interfaces::BackupEntryResult>& results, bool& failed);

#endif // GRIDCOIN_QT_FILETRANSFERDIALOG_H
