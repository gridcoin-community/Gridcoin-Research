#include "boinc.h"
#include <boost/filesystem.hpp>
#ifdef WIN32
#include <windows.h>
#define KEY_WOW64_64KEY 0x0100
#endif

extern std::string GetArgument(std::string arg, std::string defaultvalue);

std::string GetBoincDataDir(){

    std::string path = GetArgument("boincdatadir", "");
    if (!path.empty()){
        return path;
    }

    #ifdef WIN32
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Space Sciences Laboratory, U.C. Berkeley\\BOINC Setup\\",
                     0,
                     KEY_READ|KEY_WOW64_64KEY,
                     &hKey) == ERROR_SUCCESS){

        wchar_t szPath[MAX_PATH];
        DWORD dwSize = sizeof(szPath);

        if (RegQueryValueEx(hKey,
                        L"DATADIR",
                        NULL,
                        NULL,
                        (LPBYTE)&szPath,
                        &dwSize) == ERROR_SUCCESS){
            RegCloseKey(hKey);
            std::wstring wsPath = szPath;
            std::string path(wsPath.begin(),wsPath.end());
            if (boost::filesystem::exists(path)){
                return path;
            } else {
                printf("Cannot find BOINC data dir %s.\n", path.c_str());
            }
        }
        RegCloseKey(hKey);
    }

    if (boost::filesystem::exists("C:\\ProgramData\\BOINC\\")){
        return "C:\\ProgramData\\BOINC\\";
    }
    else if(boost::filesystem::exists("C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\")){
        return "C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\";
    }
    #endif

    #ifdef __linux__
    if (boost::filesystem::exists("/var/lib/boinc-client/")){
        return "/var/lib/boinc-client/";
    }
    #endif

    #ifdef __APPLE__
    if (boost::filesystem::exists("/Library/Application Support/BOINC Data/")){
        return "/Library/Application Support/BOINC Data/";
    }
    #endif

    printf("ERROR: Cannot find BOINC data dir\n");
    return "";
}
