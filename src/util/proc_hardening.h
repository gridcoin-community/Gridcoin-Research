// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_PROC_HARDENING_H
#define GRIDCOIN_UTIL_PROC_HARDENING_H

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
//! systemd unit (contrib/init/gridcoinresearchd.service). It is deliberately NOT
//! a syscall sandbox: an in-process seccomp-bpf filter was considered and
//! rejected (Bitcoin Core added then removed theirs as unmaintainable; the
//! declarative systemd SystemCallFilter= is the maintained equivalent).
//!
//! No-op on non-Linux platforms. Logs via LogPrintf, so call after
//! InitLogging(). The caller gates this on the -nonewprivs option.
void HardenProcess();

#endif // GRIDCOIN_UTIL_PROC_HARDENING_H
