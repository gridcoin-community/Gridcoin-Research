// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_PROC_HARDENING_H
#define GRIDCOIN_UTIL_PROC_HARDENING_H

#include <iosfwd>
#include <optional>

//! Default for the -nonewprivs startup option (on).
static const bool DEFAULT_NO_NEW_PRIVS = true;

//! Best-effort, in-process privilege hardening for the daemon.
//!
//! On Linux this sets \c PR_SET_NO_NEW_PRIVS -- which blocks this process and
//! all of its children from gaining privileges through \c execve() of a setuid /
//! file-capability binary -- and then drops the capability bounding set so those
//! capabilities can never be acquired, even by executing a setuid-root helper.
//!
//! \c NO_NEW_PRIVS works for any process; the bounding-set drop needs
//! \c CAP_SETPCAP, so it only takes effect when the daemon was started
//! privileged (e.g. by root before a privilege drop). For an already-unprivileged
//! launch each drop returns EPERM and is a harmless no-op -- the systemd unit's
//! \c CapabilityBoundingSet= is the authoritative drop for packaged installs.
//!
//! This is defence-in-depth that complements, and is secondary to, the hardened
//! systemd unit (contrib/init/gridcoinresearchd.service.in). It is deliberately NOT
//! a syscall sandbox: an in-process seccomp-bpf filter was considered and
//! rejected (Bitcoin Core added then removed theirs as unmaintainable; the
//! declarative systemd SystemCallFilter= is the maintained equivalent).
//!
//! No-op on non-Linux platforms. Logs via LogPrintf, so call after
//! InitLogging(). The caller gates this on the -nonewprivs option.
void HardenProcess();

//! Linux: prctl(PR_SET_DUMPABLE, 0); records the outcome for LogNonDumpableStatus(). No-op (true) elsewhere.
//! Call as the first statement of main, before any fork or daemonisation, so other processes of the same user
//! cannot ptrace the process or read /proc/<pid>/mem, environ or fd, and no core dump is written.
bool SetProcessNonDumpable();

//! Logs "Process hardening: non-dumpable (core dumps and same-user memory access blocked)" or the failure with
//! strerror. Call after InitLogging().
void LogNonDumpableStatus();

//! The TracerPid field of a /proc/<pid>/status stream; nullopt when absent or not a number.
std::optional<int> ParseTracerPid(std::istream& status);

//! ParseTracerPid of /proc/self/status (Linux); nullopt elsewhere or when unreadable.
std::optional<int> ReadOwnTracerPid();

enum class TracerVerdict { Start, StartDumpableForDebugging, Refuse };

//! override set -> StartDumpableForDebugging; tracer_pid > 0 -> Refuse; otherwise Start.
TracerVerdict DecideTracer(std::optional<int> tracer_pid, bool override_set);

//! Linux: prctl(PR_SET_DUMPABLE, 1), for the -debugallowtracer override. A non-root tracer loses memory access
//! after the first non-dumpable prctl, so the override has to restore it. No-op (true) elsewhere.
bool SetProcessDumpableForDebugging();

#endif // GRIDCOIN_UTIL_PROC_HARDENING_H
