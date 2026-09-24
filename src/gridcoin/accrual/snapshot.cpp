// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/accrual/snapshot.h"

#include "tinyformat.h"
#include "util/system.h"

namespace GRC {

fs::path SnapshotDirectory()
{
    return GetDataDir() / "accrual";
}

fs::path SnapshotPath(const uint64_t height)
{
    return SnapshotDirectory() / strprintf("%" PRIu64 ".dat", height);
}

} // namespace GRC
