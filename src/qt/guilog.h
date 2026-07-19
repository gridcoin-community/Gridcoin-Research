// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_GUILOG_H
#define GRIDCOIN_QT_GUILOG_H

#include <string>

#include <tinyformat.h>

//! GUI logging shim.
//!
//! The Qt layer logs through this shim instead of calling the core logger
//! (LogPrint/LogPrintf) directly. Logging is a stateless local utility, so this
//! is deliberately not an interfaces:: boundary type -- it is a thin swap point
//! that turns the core logger into just one implementation of the GUI's logging
//! semantic:
//!
//!  - Monolithic build (today): the sink forwards to the core logger, so GUI
//!    lines land in the shared debug.log exactly as before, honouring -debug
//!    category filtering.
//!  - Multiprocess split (later): the GUI process has no access to the node's
//!    debug.log, so the sink is repointed at a GUI-local file. Because logging
//!    must never depend on the IPC channel (the GUI has to be able to log a
//!    failed/absent connection), log lines are never shipped over the wire.
//!
//! Only qt/guilog.cpp includes the core logging header; every other Qt
//! translation unit uses these macros and stays off the core logger.

//! GUI log categories, mirroring the BCLog categories the Qt layer uses. Kept
//! as a small independent enum so no GUI translation unit needs logging.h.
enum class GUILogCategory {
    QT,
    VOTE,
    SCRAPER,
    MISC,
    VERBOSE,
    NOISY,
};

namespace GUILog {

//! Whether logging is enabled at all (mirrors the core logger's Enabled()).
bool Enabled();

//! Whether the given category is currently enabled (mirrors LogAcceptCategory).
bool WillLogCategory(GUILogCategory category);

//! Emit one already-formatted line to the active sink (adds the newline and any
//! timestamp/thread prefixing, matching a core LogPrintf).
void LogPrintStr(const std::string& line);

//! Format and emit an unconditional GUI log line (mirror of LogPrintf). Kept a
//! template (not a tfm::format macro) so the format-string lint can parse it,
//! exactly as the core LogPrintf is. The enabled check comes BEFORE tfm::format
//! so disabled logging skips the string-formatting work (the arguments
//! themselves are evaluated at the call site regardless). tfm::format is wrapped
//! so a malformed format string logs an error rather than aborting the GUI --
//! both matching the core LogPrintf.
template <typename... Args>
void LogPrintf(const char* fmt, const Args&... args)
{
    if (!Enabled()) {
        return;
    }

    std::string log_msg;
    try {
        log_msg = tfm::format(fmt, args...);
    } catch (tinyformat::format_error& e) {
        log_msg = "Error \"" + std::string(e.what()) + "\" while formatting GUI log message: " + fmt;
    }

    LogPrintStr(log_msg);
}

} // namespace GUILog

//! Unconditional GUI log line (mirror of LogPrintf).
#define GUILogPrintf(...) GUILog::LogPrintf(__VA_ARGS__)

//! Category-gated GUI log line (mirror of LogPrint).
#define GUILogPrint(category, ...)                       \
    do {                                                 \
        if (GUILog::WillLogCategory(category)) {         \
            GUILog::LogPrintf(__VA_ARGS__);              \
        }                                                \
    } while (0)

#endif // GRIDCOIN_QT_GUILOG_H
