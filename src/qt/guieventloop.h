// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUIEVENTLOOP_H
#define BITCOIN_QT_GUIEVENTLOOP_H

#include <QThreadPool>

#include <utility>

//! Waits for every job on QThreadPool::globalInstance() when it goes out of
//! scope, on every exit path, exception unwinding included. Declare it AFTER
//! the objects that pooled jobs dereference: locals are destroyed in reverse
//! order of declaration, so its destructor then runs before any of theirs.
//! A catch clause cannot do this job, because the unwind has already destroyed
//! the try block's locals by the time the handler runs.
//!
//! Waiting on an idle pool returns at once, so a later waitForDone() on the
//! same path, such as the explicit one in StartGridcoinQt after
//! RunGuiEventLoop returns, finds nothing left to join.
//!
//! Destroy it while the QCoreApplication is alive: ~QCoreApplication deletes
//! the global pool, and globalInstance() returns null from then on.
class GlobalPoolReapGuard
{
public:
    GlobalPoolReapGuard() = default;
    ~GlobalPoolReapGuard() { QThreadPool::globalInstance()->waitForDone(); }

    GlobalPoolReapGuard(const GlobalPoolReapGuard&) = delete;
    GlobalPoolReapGuard& operator=(const GlobalPoolReapGuard&) = delete;
};

//! Runs the GUI's event loop and the teardown after it, passed in as `run`.
//! Returns, or lets an exception from `run` propagate, only once every job on
//! the global QThreadPool has finished.
//!
//! Pooled jobs start inside the event loop and may use objects the caller
//! declared before this call; the About dialog's version check uses
//! interfaces::Node. If `run` throws, the caller's unwind destroys those
//! objects before any catch clause runs. The guard is armed before `run`, so
//! it reaps as the exception leaves this function, before it reaches them.
//!
//! A function rather than a guard written into StartGridcoinQt, so that
//! test_gridcoin-qt runs this exact code (GuiEventLoopTests).
template <typename Fn>
void RunGuiEventLoop(Fn&& run)
{
    GlobalPoolReapGuard pool_reap_guard;
    std::forward<Fn>(run)();
}

#endif // BITCOIN_QT_GUIEVENTLOOP_H
