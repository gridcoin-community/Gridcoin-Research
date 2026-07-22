// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/shutdown.h"

#include <atomic>

namespace {
//! Set when a shutdown is requested; polled by the daemon main loop. Written
//! from POSIX/Windows signal handlers, so it must be a lock-free atomic (which
//! is async-signal-safe); this replaces the former plain-bool fRequestShutdown.
std::atomic<bool> g_shutdown_requested{false};

//! Set once node teardown begins; observed by worker threads as their loop-exit
//! condition. Replaces the former std::atomic<bool> fShutdown.
std::atomic<bool> g_shutdown_in_progress{false};

// SetShutdownRequested() is called from the POSIX/Windows signal handlers, where
// only lock-free atomic operations are async-signal-safe. Enforce that the
// platform actually provides a lock-free bool atomic at compile time rather than
// relying on the assumption silently. std::atomic<bool> is always lock-free on
// every platform this project targets, so this never fires in practice.
static_assert(std::atomic<bool>::is_always_lock_free,
              "shutdown flags must be lock-free: they are written from signal handlers");
} // namespace

void SetShutdownRequested()
{
    // Relaxed: this flag carries no data dependency with other memory; the
    // daemon main loop and worker threads only need to observe the value change
    // eventually, and a relaxed store is what keeps the signal-handler path
    // async-signal-safe.
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}

bool ShutdownRequested()
{
    return g_shutdown_requested.load(std::memory_order_relaxed);
}

void SetShutdownInProgress(bool in_progress)
{
    g_shutdown_in_progress.store(in_progress, std::memory_order_relaxed);
}

bool ShutdownInProgress()
{
    return g_shutdown_in_progress.load(std::memory_order_relaxed);
}
