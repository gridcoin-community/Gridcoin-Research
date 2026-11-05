// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_WINSERVICE_H
#define GRIDCOIN_UTIL_WINSERVICE_H

//! gridcoinresearchd under the Windows service control manager.
namespace util::winsvc {

//! Whether this process runs as a Windows service. False unless the service entry point
//! has said so, and always false on other platforms. Safe to call from any thread.
bool IsRunningAsWindowsService();

//! Make IsRunningAsWindowsService() answer \p value, for tests.
void SetRunningAsWindowsServiceForTesting(bool value);

} // namespace util::winsvc

#endif // GRIDCOIN_UTIL_WINSERVICE_H
