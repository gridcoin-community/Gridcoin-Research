// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <config/gridcoin-config.h>

#include <util/proc_hardening.h>

#include <logging.h>
#include <util/strencodings.h>

#include <fstream>
#include <istream>
#include <string>

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

namespace {
//! errno of the last SetProcessNonDumpable() call; 0 on success or when not attempted.
int g_non_dumpable_errno = 0;
bool g_non_dumpable_attempted = false;
} // namespace

bool SetProcessNonDumpable()
{
#if defined(__linux__)
    g_non_dumpable_attempted = true;
    if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0) {
        g_non_dumpable_errno = errno;
        return false;
    }
    g_non_dumpable_errno = 0;
#endif
    return true;
}

void LogNonDumpableStatus()
{
    if (!g_non_dumpable_attempted) return;

    if (g_non_dumpable_errno == 0) {
        LogPrintf("Process hardening: non-dumpable (core dumps and same-user memory access blocked)");
    } else {
        LogPrintf("Process hardening: WARNING: could not mark the process non-dumpable: %s",
                  std::strerror(g_non_dumpable_errno));
    }
}

bool SetProcessDumpableForDebugging()
{
#if defined(__linux__)
    return prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) == 0;
#else
    return true;
#endif
}

std::optional<int> ParseTracerPid(std::istream& status)
{
    static const std::string key = "TracerPid:";

    std::string line;
    while (std::getline(status, line)) {
        if (line.compare(0, key.size(), key) != 0) continue;

        // ParseInt32 rejects leading and trailing whitespace, so trim the field first.
        std::string value = line.substr(key.size());
        const size_t first = value.find_first_not_of(" \t");
        if (first == std::string::npos) return std::nullopt;
        value = value.substr(first, value.find_last_not_of(" \t\r") - first + 1);

        int32_t pid = 0;
        if (!ParseInt32(value, &pid) || pid < 0) return std::nullopt;
        return pid;
    }

    return std::nullopt;
}

std::optional<int> ReadOwnTracerPid()
{
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    if (!status) return std::nullopt;
    return ParseTracerPid(status);
#else
    return std::nullopt;
#endif
}

TracerVerdict DecideTracer(std::optional<int> tracer_pid, bool override_set)
{
    if (override_set) return TracerVerdict::StartDumpableForDebugging;
    if (tracer_pid && *tracer_pid > 0) return TracerVerdict::Refuse;
    return TracerVerdict::Start;
}
