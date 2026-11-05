// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "test/wine.h"

#ifdef WIN32
#include <windows.h>
#endif

namespace grc_test {

bool RunningUnderWine()
{
#ifdef WIN32
    // Wine's ntdll exports wine_get_version; Windows' does not.
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");

    return ntdll != nullptr && ::GetProcAddress(ntdll, "wine_get_version") != nullptr;
#else
    return false;
#endif
}

} // namespace grc_test
