// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_FILETRANSFERDIALOG_H
#define GRIDCOIN_QT_FILETRANSFERDIALOG_H

#include "fs.h"
#include "interfaces/file_transfer.h"

#include <QString>

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

//! The core's files a backup must not be written over (CheckDestination's core_files): its wallet
//! file, configuration file, settings file and debug log. A path the core did not report is left
//! out.
std::vector<fs::path> BackupGuardFiles(const interfaces::CorePaths& paths);

//! The message a backup ends with, a line per entry: "Saved <n> bytes to <path>." for an entry that
//! arrived, the note of one the core had nothing to send for, and the message of the one that did
//! not arrive, after its name. \p failed is set when an entry failed; a cancel is not a failure.
QString BackupResultText(const std::vector<interfaces::BackupEntryResult>& results, bool& failed);

#endif // GRIDCOIN_QT_FILETRANSFERDIALOG_H
