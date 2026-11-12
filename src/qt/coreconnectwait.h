// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_CORECONNECTWAIT_H
#define GRIDCOIN_QT_CORECONNECTWAIT_H

#include "qt/guiipcworker.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QProgressDialog>
#include <QPushButton>
#include <QString>
#include <QTimer>

#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <string>

//! How WaitForCore paces itself.
struct CoreWaitTimings
{
    //! How often the GUI thread looks at the attempt in flight.
    std::chrono::milliseconds poll{50};
    //! The least time from the end of one attempt to the start of the next,
    //! and how long an attempt may be in flight before the dialog is shown.
    std::chrono::milliseconds retry_interval{2000};
};

//! How WaitForCore ended.
template <typename Result>
struct CoreWaitOutcome
{
    //! The last attempt that finished. Default-constructed when that attempt
    //! threw (see error).
    Result last{};
    //! The wait was canceled: by the user, or by its event loop being told to
    //! quit from outside (the application quitting).
    bool canceled{false};
    //! The waiting dialog was shown.
    bool dialog_shown{false};
    //! What the last attempt threw, if it threw; the wait then ends as for a
    //! failure that is not retried.
    std::string error;
};

//! Connect to a core that may still be starting, without blocking the GUI
//! thread.
//!
//! Each \p attempt runs on the process's modal GuiIpcWorker, while the GUI
//! thread runs an event loop and looks at it every timings.poll. An attempt
//! makes IPC calls, and the worker outlives the connection it hands over:
//! main() makes the worker before it connects (see GuiIpcWorker). Without that
//! worker no attempt is made, and the wait ends at once as for an attempt that
//! threw. An attempt that \p connected
//! ends the wait. After one that did not, \p retry, given the time since the
//! first attempt started, says whether to try again, and the next attempt
//! starts timings.retry_interval after that one ended. A busy dialog, "Waiting
//! for the Gridcoin core to start...", with Cancel, is shown after the first
//! failure that is retried, or once an attempt has been in flight for
//! timings.retry_interval, whichever comes first.
//!
//! Cancel ends the wait, and no attempt starts after it. An attempt still in
//! flight is waited for with the dialog up, saying "Canceling..." with Cancel
//! disabled, while the GUI thread keeps processing events; that attempt's
//! result is then the last one. The same holds when the event loop is told to
//! quit from outside. The attempt in flight is always finished here, so no
//! attempt is running when this returns.
template <typename Result>
CoreWaitOutcome<Result> WaitForCore(std::function<Result()> attempt,
                                    std::function<bool(const Result&)> connected,
                                    std::function<bool(const Result&, std::chrono::seconds)> retry,
                                    const CoreWaitTimings& timings, const QString& title)
{
    using Clock = std::chrono::steady_clock;
    const Clock::time_point first_start = Clock::now();

    //! What one attempt hands over: its result, or what it threw. Shared with
    //! the job that runs it, and read only once the job has finished.
    struct Slot
    {
        Result result{};
        bool threw{false};
        std::string error{};
    };

    CoreWaitOutcome<Result> outcome;
    std::shared_ptr<Slot> slot;
    // Ready once the attempt has returned, or thrown, and its job is gone.
    std::future<void> pending;
    Clock::time_point pending_start = first_start;
    Clock::time_point next_start = first_start;
    std::unique_ptr<QProgressDialog> dialog;
    QPushButton* cancel_button = nullptr;
    QEventLoop loop;
    // Set when this function ends the loop itself; otherwise it was told to
    // quit from outside.
    bool ended_here = false;

    const auto end_loop = [&] {
        ended_here = true;
        loop.quit();
    };

    // Take the finished attempt's result, as the last one. Returns false when
    // the attempt threw.
    const auto take_result = [&] {
        pending.get();
        if (slot->threw) {
            outcome.last = Result{};
            outcome.error = slot->error;
            return false;
        }
        outcome.last = std::move(slot->result);
        return true;
    };

    const auto on_canceled = [&] {
        if (!outcome.canceled) {
            outcome.canceled = true;
            dialog->setLabelText(QObject::tr("Canceling..."));
            cancel_button->setEnabled(false);
        }
        if (!pending.valid()) {
            end_loop();
            return;
        }
        // QProgressDialog hides itself when it is canceled, or closed. Show it
        // again once that has happened, until the attempt in flight finishes.
        QTimer::singleShot(0, dialog.get(), [&] {
            if (pending.valid()) dialog->show();
        });
    };

    const auto show_dialog = [&] {
        if (dialog) return;
        // No cancel button from the constructor: the dialog's own is set below,
        // so it can be disabled while canceling.
        dialog = std::make_unique<QProgressDialog>(QObject::tr("Waiting for the Gridcoin core to start..."),
                                                   QString(), 0, 0);
        cancel_button = new QPushButton(QObject::tr("Cancel"));
        dialog->setCancelButton(cancel_button);
        dialog->setWindowTitle(title);
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->setMinimumDuration(0);
        dialog->setAutoReset(false);
        dialog->setAutoClose(false);
        QObject::connect(dialog.get(), &QProgressDialog::canceled, &loop, on_canceled);
        dialog->show();
        outcome.dialog_shown = true;
    };

    const auto start_attempt = [&] {
        pending_start = Clock::now();
        slot = std::make_shared<Slot>();

        GuiIpcWorker* const worker = GuiIpcWorker::Current(GuiIpcWorker::Role::Modal);
        if (worker) {
            pending = worker->Post([job_slot = slot, attempt] {
                try {
                    job_slot->result = attempt();
                } catch (const std::exception& e) {
                    job_slot->threw = true;
                    job_slot->error = e.what();
                } catch (...) {
                    job_slot->threw = true;
                    job_slot->error = "unknown exception";
                }
            });
        }

        if (!pending.valid()) {
            // Nothing ran: taken as an attempt that threw.
            slot->threw = true;
            slot->error = worker ? "the thread the connect attempts run on could not be started"
                                 : "no thread to run the connect attempts on";
            std::promise<void> none;
            none.set_value();
            pending = none.get_future();
        }
    };

    const auto tick = [&] {
        // A timer event can still be delivered after the loop was told to
        // quit; it must not start an attempt then, nor take a result that the
        // code after the loop takes.
        if (!loop.isRunning()) return;
        const Clock::time_point now = Clock::now();
        if (pending.valid()) {
            if (pending.wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
                if (!outcome.canceled && now - pending_start >= timings.retry_interval) show_dialog();
                return;
            }
            if (!take_result() || outcome.canceled || connected(outcome.last)) {
                end_loop();
                return;
            }
            if (!retry(outcome.last, std::chrono::duration_cast<std::chrono::seconds>(now - first_start))) {
                end_loop();
                return;
            }
            show_dialog();
            next_start = now + timings.retry_interval;
        }
        if (outcome.canceled || now < next_start) return;
        start_attempt();
    };

    QTimer timer;
    timer.setInterval(static_cast<int>(timings.poll.count()));
    QObject::connect(&timer, &QTimer::timeout, &loop, tick);
    timer.start();
    start_attempt();
    loop.exec();
    timer.stop();

    if (!ended_here) outcome.canceled = true;
    if (pending.valid()) {
        // The loop was told to quit from outside with an attempt in flight.
        // Finish it with events still processed, so the GUI thread is not
        // stuck waiting for it. A nested event loop would not run: once the
        // application has been told to quit, QEventLoop::exec() returns at
        // once.
        show_dialog();
        dialog->setLabelText(QObject::tr("Canceling..."));
        cancel_button->setEnabled(false);
        while (pending.wait_for(timings.poll) != std::future_status::ready) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, static_cast<int>(timings.poll.count()));
        }
        take_result();
    }
    if (dialog) dialog->hide();
    return outcome;
}

#endif // GRIDCOIN_QT_CORECONNECTWAIT_H
