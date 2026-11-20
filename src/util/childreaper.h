// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_CHILDREAPER_H
#define GRIDCOIN_UTIL_CHILDREAPER_H

#include <string>

//! Running commands for the notify options, and reaping the processes they leave
//! behind when the core is process 1 of a PID namespace. POSIX only. On Windows
//! the functions do not exist.
namespace util {

#ifndef WIN32

//! Runs command with "/bin/sh -c" and the current environment, waits for the shell,
//! and returns its waitpid status, or -1 when the shell could not be started (the
//! errno is logged). The shell is known to the orphan reaper, which therefore never
//! takes its exit status.
//!
//! Every child process the core creates must go through this function, or be
//! registered in the same way, or the reaper may reap it first.
int RunShellCommand(const std::string& command);

//! Starts the orphan reaper thread when this process is process 1, or when force is
//! set. The reaper waits for any child that is not being waited for by its
//! spawner, which is what an init process does with orphans. Does nothing when it
//! is already running.
void StartOrphanReaper(bool force = false);

//! Stops and joins the reaper thread. Safe to call repeatedly and when the reaper
//! was never started.
void StopOrphanReaper();

#endif // !WIN32

} // namespace util

#endif // GRIDCOIN_UTIL_CHILDREAPER_H
