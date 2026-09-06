// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/boinc.h"
#include "util.h"

#include <mutex>
#include <optional>
#include <vector>

namespace {
//! \brief fs::exists() that reports an unprobeable path as absent.
//!
//! The throwing overload raises filesystem_error when the path cannot be
//! stat()ed at all -- most commonly EACCES on a directory owned by another
//! user. Probing for BOINC is a best-effort guess among candidates, and every
//! caller here wants "no" for a candidate it is not allowed to look at, not an
//! exception unwinding out of startup.
bool PathExists(const fs::path& path)
{
    boost::system::error_code ec;

    return fs::exists(path, ec) && !ec;
}
} // anonymous namespace

fs::path GRC::ResolveBoincDataDir(const std::vector<fs::path>& candidates)
{
    // Pass 1: Prefer a directory with client_state.xml (active BOINC installation).
    for (const auto& candidate : candidates) {
        if (PathExists(candidate / "client_state.xml")) {
            return candidate;
        }
    }

    // Pass 2: Fall back to any directory that exists (installed but not yet run).
    for (const auto& candidate : candidates) {
        if (PathExists(candidate)) {
            return candidate;
        }
    }

    return "";
}

namespace {
//! Guards the resolved-path cache below. A leaf lock: nothing is taken while it is
//! held except the filesystem/registry probe itself, which is exactly what the cache
//! exists to stop repeating.
std::mutex g_boinc_data_dir_mutex;

//! Empty until the first probe. Holds the NEGATIVE result too -- caching only
//! successes would leave the failing case probing forever, and the failing case is
//! precisely the expensive one (on Windows: a registry open/query, then fs::exists on
//! C:\ProgramData\BOINC\, then on the legacy C:\Documents and Settings\All Users\...
//! junction, then an error() log).
std::optional<fs::path> g_boinc_data_dir_cache;
} // anonymous namespace

//! Resolve the BOINC data directory from scratch. Callers want GetBoincDataDir(),
//! which memoizes this.
static fs::path FindBoincDataDir()
{
    std::string path = gArgs.GetArg("-boincdatadir", "");

    if (!path.empty()) {
        return fs::path(path);
    }

    #ifdef WIN32
    HKEY hKey;
    if (RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Space Sciences Laboratory, U.C. Berkeley\\BOINC Setup\\",
        0,
        KEY_READ|KEY_WOW64_64KEY,
        &hKey) == ERROR_SUCCESS)
    {
        wchar_t szPath[MAX_PATH];
        DWORD dwSize = sizeof(szPath);

        if (RegQueryValueEx(
            hKey,
            L"DATADIR",
            nullptr,
            nullptr,
            (LPBYTE)&szPath,
            &dwSize) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);

            fs::path path = std::wstring(szPath);

            if (PathExists(path)){
                return path;
            } else {
                LogPrintf("Cannot find BOINC data dir %s.", path.string());
            }
        }

        RegCloseKey(hKey);
    }

    if (PathExists("C:\\ProgramData\\BOINC\\")){
        return "C:\\ProgramData\\BOINC\\";
    } else if(PathExists("C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\")) {
        return "C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\";
    }
    #endif

    #ifdef __linux__
    std::vector<fs::path> linux_candidates = {
        "/var/lib/boinc-client/",
        "/var/lib/boinc/",
    };

    char* pszHome = getenv("HOME");

    if (pszHome && strlen(pszHome) > 0) {
        linux_candidates.push_back(fs::path(pszHome) / ".var/app/edu.berkeley.BOINC/");
    }

    fs::path linux_result = GRC::ResolveBoincDataDir(linux_candidates);

    if (!linux_result.empty()) {
        return linux_result;
    }
    #endif

    #ifdef __APPLE__
    if (PathExists("/Library/Application Support/BOINC Data/")) {
        return "/Library/Application Support/BOINC Data/";
    }
    #endif

    error("%s: Cannot find BOINC data directory. You may need to manually specify in the gridcoinresearch.conf file "
          "the data directory location by using boincdatadir=<data directory location>.", __func__);

    return "";
}

fs::path GRC::GetBoincDataDir()
{
    std::lock_guard<std::mutex> lock(g_boinc_data_dir_mutex);

    // Probing per call was costing a registry query and two filesystem probes on
    // Windows every time the GUI rebuilt its researcher snapshot -- which was once
    // per block. Worse, it is guaranteed futile for a whole class of user: someone
    // who installs Gridcoin before BOINC has no data directory, and the beacon
    // wizard that would let them fix it is gated on the wallet being synced. So for
    // the entire initial sync the answer cannot change and every probe fails, while
    // error() logs unconditionally -- default logging, no -debug needed.
    //
    // Cache it instead. The answer only changes when the user tells the wallet
    // something changed, and every one of those routes through the no-arg
    // Researcher::Reload() -- startup, the wizard (switchMode -> ChangeMode ->
    // Reload), resetcpids, and the interface reload() -- which calls
    // ResetBoincDataDirCache(). A time-based cache would be wrong here: nothing
    // expires on a schedule, it changes on an action.
    if (!g_boinc_data_dir_cache) {
        g_boinc_data_dir_cache = FindBoincDataDir();
    }

    return *g_boinc_data_dir_cache;
}

void GRC::ResetBoincDataDirCache()
{
    std::lock_guard<std::mutex> lock(g_boinc_data_dir_mutex);

    g_boinc_data_dir_cache.reset();
}
