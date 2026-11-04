// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_FILETRANSFERDIALOG_H
#define GRIDCOIN_QT_FILETRANSFERDIALOG_H

#include "fs.h"
#include "interfaces/file_transfer.h"
#include "sync.h"

#include <QString>

#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>
#include <vector>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace interfaces {
struct CorePaths;
} // namespace interfaces

//! The one thread on which this process runs file transfers (RunDownloadWithProgress and
//! RunUploadWithProgress).
//!
//! In the split build a transfer's calls cross IPC, and libmultiprocess keeps state for each
//! thread that makes them: an entry per connection in that thread's request_threads and
//! callback_threads maps. Two paths destroy those entries, and they do not synchronise with each
//! other: the event loop, when it destroys the connection, and the thread's own exit. So a thread
//! that has made calls must not exit while its connection may be destroyed. A transfer that ends
//! because the core dropped the connection would do exactly that on a thread of its own, since the
//! failed call wakes it just as the loop deletes the connection. This thread is started by the
//! first job and joined only when this object is destroyed, which main() does after the
//! connection is gone, as with the GUI thread itself.
class FileTransferWorker
{
public:
    //! Becomes the process's worker (Current()) until it is destroyed. One at a time.
    FileTransferWorker();

    //! Runs the jobs still queued, then joins the thread.
    ~FileTransferWorker();

    FileTransferWorker(const FileTransferWorker&) = delete;
    FileTransferWorker& operator=(const FileTransferWorker&) = delete;

    //! The process's worker, or null outside the lifetime of one.
    static FileTransferWorker* Current();

    //! Run \p job on the worker thread, after the jobs posted before it. False, and the job is not
    //! run, when the thread cannot be started. A job that throws is logged and the thread carries
    //! on.
    bool Post(std::function<void()> job) LOCKS_EXCLUDED(m_mutex);

private:
    void Run() LOCKS_EXCLUDED(m_mutex);

    Mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_jobs GUARDED_BY(m_mutex){};
    bool m_stop GUARDED_BY(m_mutex){false};
    //! Started by the first Post().
    std::thread m_thread GUARDED_BY(m_mutex){};
};

//! Runs DownloadEntry on a worker std::thread with a modal QProgressDialog (bytes done / total, Cancel). Returns the outcome.
//!
//! The worker is FileTransferWorker::Current(); without one the outcome is a LocalError. Cancel
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
