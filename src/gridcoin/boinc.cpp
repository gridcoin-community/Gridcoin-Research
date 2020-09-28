// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/boinc.h"
#include "util.h"

fs::path GRC::GetBoincDataDir()
{
    std::string path = GetArgument("boincdatadir", "");

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

            if (fs::exists(path)){
                return path;
            } else {
                LogPrintf("Cannot find BOINC data dir %s.", path.string());
            }
        }

        RegCloseKey(hKey);
    }

    if (fs::exists("C:\\ProgramData\\BOINC\\")){
        return "C:\\ProgramData\\BOINC\\";
    } else if(fs::exists("C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\")) {
        return "C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\";
    }
    #endif

    #ifdef __linux__
    if (fs::exists("/var/lib/boinc-client/")) {
        return "/var/lib/boinc-client/";
    } else if (fs::exists("/var/lib/boinc/")) {
        return "/var/lib/boinc/";
    }
    #endif

    #ifdef __APPLE__
    if (fs::exists("/Library/Application Support/BOINC Data/")) {
        return "/Library/Application Support/BOINC Data/";
    }
    #endif

    LogPrintf("ERROR: Cannot find BOINC data dir");
    return "";
}
