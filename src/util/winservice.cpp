// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/winservice.h"

#include <atomic>

namespace util::winsvc {
namespace {
//! Whether this process runs as a Windows service. Atomic, because it is read from
//! whichever thread needs to know.
std::atomic<bool> g_running_as_service{false};
} // namespace

bool IsRunningAsWindowsService()
{
    return g_running_as_service.load();
}

void SetRunningAsWindowsServiceForTesting(bool value)
{
    g_running_as_service.store(value);
}

} // namespace util::winsvc
