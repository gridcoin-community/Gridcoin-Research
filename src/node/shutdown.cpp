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
} // namespace

void SetShutdownRequested()
{
    g_shutdown_requested = true;
}

bool ShutdownRequested()
{
    return g_shutdown_requested;
}

void SetShutdownInProgress(bool in_progress)
{
    g_shutdown_in_progress = in_progress;
}

bool ShutdownInProgress()
{
    return g_shutdown_in_progress;
}
