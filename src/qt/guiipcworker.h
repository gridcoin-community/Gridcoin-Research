// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_GUIIPCWORKER_H
#define GRIDCOIN_QT_GUIIPCWORKER_H

#include "sync.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <thread>

//! A long-lived thread on which the GUI calls into the core off the GUI thread.
//!
//! In the split build those calls cross IPC, and libmultiprocess keeps state for each thread that
//! makes them: an entry per connection in that thread's request_threads and callback_threads maps.
//! Two paths destroy those entries, and they do not synchronise with each other: the event loop,
//! when it destroys the connection, and the thread's own exit. So a thread that has made calls must
//! not exit while its connection may be destroyed. A short-lived thread whose call ends because the
//! core dropped the connection does exactly that, since the failed call wakes it just as the loop
//! deletes the connection.
//!
//! A worker's thread is started by its first job and joined only when the worker is destroyed.
//! main() makes one worker for each role before the connection, so each is destroyed after it, as
//! the GUI thread itself outlives it: the loop erases a worker's entries when it destroys the
//! connection, and the worker's exit then finds none. In the monolithic build no call crosses IPC,
//! and a worker is a thread that is started when first needed.
class GuiIpcWorker
{
public:
    //! What a worker is for. Each role has a thread of its own, so the work of one never waits
    //! behind the work of another.
    enum class Role {
        //! Work a modal dialog waits for, such as a file transfer or a passphrase check.
        Modal,
        //! The coin list's wallet scan, which takes as long as the wallet is large, so a prompt
        //! must never wait behind it.
        CoinLoad,
    };

    //! Becomes the process's worker for \p role (Current()) until it is destroyed. One for each
    //! role at a time: a second one is logged and not used.
    explicit GuiIpcWorker(Role role);

    //! Runs the jobs still queued, then joins the thread.
    ~GuiIpcWorker();

    GuiIpcWorker(const GuiIpcWorker&) = delete;
    GuiIpcWorker& operator=(const GuiIpcWorker&) = delete;

    //! The process's worker for \p role, or null outside the lifetime of one.
    static GuiIpcWorker* Current(Role role);

    //! Run \p job on the worker thread, after the jobs posted before it. The future becomes ready
    //! once the job has returned, or thrown, and has been destroyed with everything it captured. A
    //! job that throws is logged and the thread carries on. The future is invalid, and the job is
    //! not run, when the thread cannot be started or the worker is being destroyed.
    //!
    //! A job must not wait for a job posted after it: they run one at a time.
    std::future<void> Post(std::function<void()> job) LOCKS_EXCLUDED(m_mutex);

private:
    struct Job
    {
        std::function<void()> run;
        std::promise<void> done;
    };

    void Run() LOCKS_EXCLUDED(m_mutex);

    const Role m_role;

    //! A leaf: nothing else is locked under it, and no job runs while it is held.
    Mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Job> m_jobs GUARDED_BY(m_mutex){};
    bool m_stop GUARDED_BY(m_mutex){false};
    //! Started by the first Post().
    std::thread m_thread GUARDED_BY(m_mutex){};
};

#endif // GRIDCOIN_QT_GUIIPCWORKER_H
