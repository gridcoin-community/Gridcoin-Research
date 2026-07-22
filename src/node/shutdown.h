// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SHUTDOWN_H
#define BITCOIN_NODE_SHUTDOWN_H

//! Node shutdown state, extracted from the former util.h fRequestShutdown /
//! fShutdown globals (multiprocess Phase 1e) so the GUI can no longer read core
//! lifecycle state as a process global. Core code queries and mutates the
//! shutdown state exclusively through these functions; the GUI observes a
//! core-initiated shutdown through interfaces:: (uiInterface.QueueShutdown
//! today), never these.
//!
//! Two distinct concepts are tracked:
//!  - "requested": someone asked the node to stop -- the RPC `stop` command (via
//!    StartShutdown), SIGTERM, the console control handler, or an init failure.
//!    The daemon main loop polls ShutdownRequested() and then runs Shutdown().
//!  - "in progress": Shutdown() has begun tearing the node down; worker threads
//!    observe ShutdownInProgress() to break their loops.

//! Request that the node begin shutting down (idempotent). Unlike
//! StartShutdown() (init.h), this does not perform the Qt marshalling that makes
//! the GUI leave its event loop; it only sets the request flag. Used by the
//! POSIX/Windows signal handlers, the init-failure paths, and StartShutdown()'s
//! non-GUI (daemon) branch.
void SetShutdownRequested();

//! Whether a shutdown has been requested.
bool ShutdownRequested();

//! Set whether node teardown is in progress. Passed \c true at the top of
//! Shutdown() (and by the low-disk abort in CheckDiskSpace); reset to \c false
//! by CConnman::Start(), which re-arms the flag for an in-process restart of
//! the connection manager (e.g. the blockchain-reset path). Defaults to \c true
//! so the common "begin teardown" call sites read cleanly.
void SetShutdownInProgress(bool in_progress = true);

//! Whether node teardown is in progress. Worker threads use this as their
//! loop-exit condition.
bool ShutdownInProgress();

#endif // BITCOIN_NODE_SHUTDOWN_H
