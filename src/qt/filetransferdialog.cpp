// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/filetransferdialog.h"

#include "interfaces/node.h"
#include "qt/guiipcworker.h"
#include "qt/guiutil.h"

#include <QCoreApplication>
#include <QDialog>
#include <QEventLoop>
#include <QObject>
#include <QProgressDialog>
#include <QPushButton>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>

namespace {
//! How often the GUI thread looks at a transfer in flight.
constexpr std::chrono::milliseconds TRANSFER_POLL{50};

//! What the GUI thread and the worker share while one transfer runs. Both hold it, so it
//! outlives whichever lets go last.
template <typename Result>
struct TransferJob
{
    std::atomic<uint64_t> done{0};
    std::atomic<int64_t> total{-1};
    //! Set once the job has reported its bytes. A job that begins its transfer first (a console
    //! file command) has none to report until then.
    std::atomic<bool> reported{false};
    std::atomic<bool> cancel{false};
    //! Written by the job, and read only once the job has finished. Left as it is (a failure
    //! with no message) if even the failure could not be made.
    Result result{};
};

QString CountsText(uint64_t done, int64_t total)
{
    if (total < 0) return QObject::tr("%1 bytes").arg(QString::number(static_cast<qulonglong>(done)));

    return QObject::tr("%1 of %2 bytes")
        .arg(QString::number(static_cast<qulonglong>(done)), QString::number(static_cast<qlonglong>(total)));
}

//! Run \p transfer on the process's worker while a modal dialog shows its progress, and
//! return its result once it has finished. The dialog shows \p what until the transfer
//! reports bytes, then what \p reported says, read on the GUI thread at each update, with them.
//! \p failed makes the result for a transfer that could not run, or that threw.
template <typename Result>
Result RunWithProgress(QWidget* parent, const QString& title, const QString& what,
                       const std::function<QString()>& reported,
                       std::function<Result(const interfaces::ProgressFn&, const std::atomic<bool>&)> transfer,
                       std::function<Result(const std::string&)> failed)
{
    GuiIpcWorker* const worker = GuiIpcWorker::Current(GuiIpcWorker::Role::Modal);
    if (!worker) return failed("no file transfer thread is running");

    const auto job = std::make_shared<TransferJob<Result>>();

    // Ready once the job has returned, however it ends, so after it has written the result.
    std::future<void> finished = worker->Post([job, transfer = std::move(transfer), failed] {
        try {
            job->result = transfer(
                [job](uint64_t done, int64_t total) {
                    job->done = done;
                    job->total = total;
                    job->reported = true;
                },
                job->cancel);
        } catch (const std::exception& e) {
            job->result = failed(e.what());
        } catch (...) {
            job->result = failed("the transfer failed");
        }
    });

    if (!finished.valid()) return failed("the file transfer thread could not be started");

    // No cancel button from the constructor: the dialog's own is set below, so it can be
    // disabled while cancelling.
    QProgressDialog dialog(what, QString(), 0, 0, parent);
    auto* const cancel_button = new QPushButton(QObject::tr("Cancel"));
    dialog.setCancelButton(cancel_button);
    dialog.setWindowTitle(title);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setMinimumDuration(0);
    dialog.setAutoReset(false);
    dialog.setAutoClose(false);

    QEventLoop loop;
    // Set when this function ends the loop itself; otherwise it was told to quit from
    // outside.
    bool ended_here = false;
    bool cancelling = false;

    const auto is_finished = [&finished] {
        return finished.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
    };

    const auto show_cancelling = [&] {
        dialog.setLabelText(QObject::tr("Cancelling..."));
        dialog.setRange(0, 0);
        cancel_button->setEnabled(false);
    };

    const auto on_cancel = [&] {
        if (!cancelling) {
            cancelling = true;
            job->cancel = true;
            show_cancelling();
        }

        // QProgressDialog hides itself when it is cancelled, or closed. Show it again once
        // that has happened, until the transfer has stopped.
        QTimer::singleShot(0, &dialog, [&] {
            if (!is_finished()) dialog.show();
        });
    };

    QObject::connect(&dialog, &QProgressDialog::canceled, &loop, on_cancel);
    // Escape rejects the dialog, which under Qt 5 hides it without canceled().
    QObject::connect(&dialog, &QDialog::rejected, &loop, on_cancel);

    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(static_cast<int>(TRANSFER_POLL.count()));

    const auto tick = [&] {
        // A timer event can still be delivered after the loop was told to quit.
        if (!loop.isRunning()) return;

        if (is_finished()) {
            ended_here = true;
            loop.quit();
            return;
        }

        if (!cancelling && job->reported) {
            const uint64_t done = job->done;
            const int64_t total = job->total;

            dialog.setLabelText(reported() + "\n" + CountsText(done, total));

            if (total > 0) {
                const uint64_t permille = std::min<uint64_t>(done, static_cast<uint64_t>(total)) * 1000 /
                                          static_cast<uint64_t>(total);
                dialog.setRange(0, 1000);
                dialog.setValue(static_cast<int>(permille));
            }
        }

        // Started again only once this tick is over: setValue() on a modal dialog processes
        // events, and a timer that fired meanwhile would run this inside itself.
        timer.start();
    };

    QObject::connect(&timer, &QTimer::timeout, &loop, tick);

    dialog.show();
    timer.start();
    loop.exec();
    timer.stop();

    if (!ended_here) {
        // Told to quit from outside (the application is quitting): stop the transfer, and
        // wait for it with events still processed. A nested event loop would not run: once
        // the application has been told to quit, QEventLoop::exec() returns at once.
        job->cancel = true;
        cancelling = true;
        show_cancelling();
        dialog.show();

        while (finished.wait_for(TRANSFER_POLL) != std::future_status::ready) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, static_cast<int>(TRANSFER_POLL.count()));
        }
    }

    dialog.hide();
    finished.get();

    return std::move(job->result);
}
} // namespace

interfaces::TransferOutcome RunDownloadWithProgress(QWidget* parent, const QString& title, interfaces::FileSource& src,
                                                    uint32_t entry, const fs::path& dest)
{
    const QString what = QObject::tr("Saving %1").arg(GUIUtil::boostPathToQString(dest));

    return RunWithProgress<interfaces::TransferOutcome>(
        parent, title, what, [what] { return what; },
        [&src, entry, dest](const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel) {
            return interfaces::DownloadEntry(src, entry, dest, progress, cancel);
        },
        [](const std::string& message) {
            interfaces::TransferOutcome out;
            out.status = interfaces::TransferStatus::LocalError;
            out.message = message;
            return out;
        });
}

interfaces::FileSinkResult RunUploadWithProgress(QWidget* parent, const QString& title, interfaces::FileSink& sink,
                                                 const fs::path& local)
{
    const QString what = QObject::tr("Sending %1").arg(GUIUtil::boostPathToQString(local));

    return RunWithProgress<interfaces::FileSinkResult>(
        parent, title, what, [what] { return what; },
        [&sink, local](const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel) {
            return interfaces::UploadFile(sink, local, progress, cancel);
        },
        [](const std::string& message) {
            interfaces::FileSinkResult out;
            out.ok = false;
            out.error = message;
            return out;
        });
}

bool RunJobWithProgress(QWidget* parent, const QString& title, const QString& what, TransferJobFn job,
                        std::string& error)
{
    struct JobResult
    {
        bool ok{false};
        std::string error{};
    };

    const JobResult result = RunWithProgress<JobResult>(
        parent, title, what, [what] { return what; },
        [job = std::move(job)](const interfaces::ProgressFn& progress, const std::atomic<bool>& cancel) {
            job(progress, cancel);

            JobResult ran;
            ran.ok = true;
            return ran;
        },
        [](const std::string& message) {
            JobResult failed;
            failed.error = message;
            return failed;
        });

    error = result.error;
    return result.ok;
}

bool RunBackupWithProgress(QWidget* parent, const QString& title, BackupBeginFn begin,
                           const std::vector<fs::path>& dests, std::vector<interfaces::BackupEntryResult>& results,
                           std::string& error)
{
    struct BackupResult
    {
        bool begun{false};
        std::string error{};
        std::vector<interfaces::BackupEntryResult> entries{};
    };

    results.clear();
    error.clear();

    // The entry being saved, which the job sets as each download starts and the dialog reads.
    const auto saving = std::make_shared<std::atomic<size_t>>(0);

    const auto saving_text = [saving, dests] {
        const size_t entry = saving->load();
        if (entry >= dests.size()) return QObject::tr("Saving the backup");

        return QObject::tr("Saving %1 (%2 of %3)")
            .arg(GUIUtil::boostPathToQString(dests[entry]), QString::number(static_cast<qulonglong>(entry + 1)),
                 QString::number(static_cast<qulonglong>(dests.size())));
    };

    BackupResult result = RunWithProgress<BackupResult>(
        parent, title, QObject::tr("Preparing the backup..."), saving_text,
        [begin = std::move(begin), dests, saving](const interfaces::ProgressFn& progress,
                                                  const std::atomic<bool>& cancel) {
            BackupResult ran;

            // The source goes as this returns, on this thread: the core deletes what it has not
            // sent, and another transfer can begin.
            const std::unique_ptr<interfaces::FileSource> source = begin(ran.error);

            if (!source) {
                if (ran.error.empty()) ran.error = QObject::tr("The backup could not begin.").toStdString();
                return ran;
            }

            ran.begun = true;
            ran.entries = interfaces::DownloadBackupEntries(
                *source, dests,
                [&progress, &cancel, &saving](interfaces::FileSource& src, uint32_t entry,
                                              const interfaces::FileEntryInfo& info, const fs::path& dest) {
                    // A cancel while the core made its copies takes effect here, before anything
                    // is written.
                    if (cancel.load()) {
                        interfaces::TransferOutcome cancelled;
                        cancelled.status = interfaces::TransferStatus::Cancelled;
                        cancelled.message = "cancelled";
                        return cancelled;
                    }

                    // The dialog names this entry from now on, with its own counts, not the ones
                    // the entry before it left.
                    *saving = entry;
                    progress(0, info.size);
                    return interfaces::DownloadEntry(src, entry, dest, progress, cancel);
                });

            return ran;
        },
        [](const std::string& message) {
            BackupResult failed;
            failed.error = message;
            return failed;
        });

    if (!result.begun) {
        error = std::move(result.error);
        return false;
    }

    results = std::move(result.entries);
    return true;
}

std::vector<fs::path> BackupGuardFiles(const interfaces::CorePaths& paths)
{
    std::vector<fs::path> files;

    for (const std::string* path : {&paths.wallet_file, &paths.config_file, &paths.settings_file, &paths.debug_log}) {
        if (!path->empty()) files.emplace_back(*path);
    }

    return files;
}

QString BackupResultText(const std::vector<interfaces::BackupEntryResult>& results, bool& failed)
{
    failed = false;
    QStringList lines;

    for (const interfaces::BackupEntryResult& result : results) {
        const QString dest = GUIUtil::boostPathToQString(result.dest);

        if (result.skipped) {
            lines << QObject::tr("%1 was not backed up: %2")
                         .arg(QString::fromStdString(result.name), QString::fromStdString(result.note));
        } else if (result.outcome.status == interfaces::TransferStatus::Ok) {
            lines << QObject::tr("Saved %1 bytes to %2.")
                         .arg(QString::number(static_cast<qulonglong>(result.outcome.bytes)), dest);
        } else if (result.outcome.status == interfaces::TransferStatus::Cancelled) {
            lines << QObject::tr("The backup was cancelled; nothing was saved to %1.").arg(dest);
        } else {
            failed = true;

            // A source that could not list its files names none.
            const QString message = QString::fromStdString(result.outcome.message);
            lines << (result.name.empty() ? message
                                          : QObject::tr("%1 was not saved: %2")
                                                .arg(QString::fromStdString(result.name), message));
        }
    }

    return lines.join("\n");
}
