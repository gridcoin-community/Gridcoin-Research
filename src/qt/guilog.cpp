// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/guilog.h"

#include "logging.h"

#include <cassert>

namespace {
//! Map a GUI log category onto its core BCLog counterpart. Confined to this
//! translation unit -- the rest of the Qt layer never sees BCLog. No default
//! case, so -Wswitch flags a new GUILogCategory that is missing a mapping; the
//! post-switch assert catches it at runtime as a backstop.
BCLog::LogFlags ToCoreFlag(GUILogCategory category)
{
    switch (category) {
    case GUILogCategory::QT:      return BCLog::LogFlags::QT;
    case GUILogCategory::VOTE:    return BCLog::LogFlags::VOTE;
    case GUILogCategory::SCRAPER: return BCLog::LogFlags::SCRAPER;
    case GUILogCategory::MISC:    return BCLog::LogFlags::MISC;
    case GUILogCategory::VERBOSE: return BCLog::LogFlags::VERBOSE;
    case GUILogCategory::NOISY:   return BCLog::LogFlags::NOISY;
    }

    assert(false && "unhandled GUILogCategory");
    return BCLog::LogFlags::QT;
}
} // namespace

namespace GUILog {

bool Enabled()
{
    // Monolithic sink: defer to the core logger. A multiprocess split GUI will
    // consult its own GUI-local logging state here.
    return LogInstance().Enabled();
}

bool WillLogCategory(GUILogCategory category)
{
    // Monolithic sink: defer to the core logger's -debug state. A multiprocess
    // split GUI will consult its own GUI-local logging configuration here.
    return LogInstance().WillLogCategory(ToCoreFlag(category));
}

void LogPrintStr(const std::string& line)
{
    // Monolithic sink: forward to the core logger (shared debug.log). Matches a
    // core LogPrintf -- the newline is appended here and the logger adds any
    // timestamp/thread prefix. A multiprocess split GUI repoints this at a
    // GUI-local file. (Callers gate on Enabled()/WillLogCategory first.)
    LogInstance().LogPrintStr(line + "\n");
}

} // namespace GUILog
