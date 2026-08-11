// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_BOINC_H
#define GRIDCOIN_BOINC_H

#include <fs.h>
#include <vector>

namespace GRC {
//! Resolve the BOINC data directory, memoized. See the definition for why this is
//! cached rather than probed per call, and ResetBoincDataDirCache() for when the
//! cached answer is discarded.
fs::path GetBoincDataDir();

//! Discard the cached BOINC data directory so the next GetBoincDataDir() re-probes.
//! Called from the no-arg Researcher::Reload(), which is the funnel for every user
//! action that can change the answer (startup, the researcher wizard, resetcpids,
//! the interface reload()).
void ResetBoincDataDirCache();
fs::path ResolveBoincDataDir(const std::vector<fs::path>& candidates);
}

#endif // GRIDCOIN_BOINC_H
