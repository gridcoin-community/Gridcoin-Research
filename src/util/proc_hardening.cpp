// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <config/gridcoin-config.h>

#include <util/proc_hardening.h>

#include <logging.h>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <sys/prctl.h>

// Fallbacks in case the libc headers predate these prctl operations (all have
// been in the kernel ABI for years; the numbers are stable).
#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif
#ifndef PR_CAPBSET_READ
#define PR_CAPBSET_READ 23
#endif
#ifndef PR_CAPBSET_DROP
#define PR_CAPBSET_DROP 24
#endif
#endif // __linux__

void HardenProcess()
{
#if defined(__linux__)
    // PR_SET_NO_NEW_PRIVS: any process may set this. Once set, execve() can no
    // longer grant privileges via setuid/setgid bits or file capabilities, for
    // this process and every descendant. The daemon never needs to gain
    // privileges, so this closes a whole class of local escalation.
    const bool no_new_privs = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0;
    if (!no_new_privs) {
        LogPrintf("HardenProcess: warning: PR_SET_NO_NEW_PRIVS failed: %s", std::strerror(errno));
    }

    // Drop the capability bounding set so neither this process nor its children
    // can ever acquire these capabilities, even by executing a setuid-root
    // helper. Dropping needs CAP_SETPCAP, so it only bites when the daemon was
    // started privileged; an already-unprivileged launch gets EPERM on the first
    // real drop and we stop there (the systemd unit's CapabilityBoundingSet= is
    // the authoritative drop in that case). PR_CAPBSET_READ returns 1 if the cap
    // is in the set, 0 if already absent, and -1 (EINVAL) once cap exceeds the
    // highest capability the running kernel knows -- which ends the loop without
    // needing CAP_LAST_CAP at build time. Only count caps we actually removed
    // (drop also "succeeds" for an already-absent cap, which would over-report).
    int dropped = 0;
    for (int cap = 0;; ++cap) {
        const int present = prctl(PR_CAPBSET_READ, cap, 0, 0, 0);
        if (present < 0) break;      // past the last capability the kernel knows
        if (present == 0) continue;  // already not in the bounding set
        if (prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) == 0) {
            ++dropped;
        } else if (errno == EPERM) {
            break;                   // no CAP_SETPCAP -> cannot drop any; stop
        }
    }
    LogPrintf("HardenProcess: NO_NEW_PRIVS %s; dropped %d bounding-set capabilities "
              "(0 is expected for an unprivileged launch).",
              no_new_privs ? "set" : "NOT set (see warning above)", dropped);
#endif // __linux__
}
