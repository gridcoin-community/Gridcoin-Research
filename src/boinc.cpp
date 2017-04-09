#include "boinc.h"
#include <boost/filesystem.hpp>
#ifdef _WIN32
#include <windows.h>
#define KEY_WOW64_64KEY 0x0100
#define KEY_WOW64_32KEY 0x0200
#endif // _WIN32

CBoinc::CBoinc(){
    strBoincDataPath = "";

    #ifdef _WIN32
    strBoincDataPath = GetDataPathFromRegistry();
    if (strBoincDataPath == ""){
        if (boost::filesystem::exists("C:\\ProgramData\\BOINC\\")){
            strBoincDataPath = "C:\\ProgramData\\BOINC\\";
        }
        else if(boost::filesystem::exists("C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\")){
            strBoincDataPath = "C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\";
        }
    }
    #endif // _WIN32

    #ifdef __linux__
    if (boost::filesystem::exists("/var/lib/boinc-client/")){
        strBoincDataPath = "/var/lib/boinc-client/";
    }
    #endif // __linux__

    #ifdef __APPLE__
    if (boost::filesystem::exists("/Library/Application Support/BOINC Data/")){
        strBoincDataPath = "/Library/Application Support/BOINC Data/";
    }
    #endif // __APPLE__
}

std::string CBoinc::GetBoincDataPath(){
    return strBoincDataPath;
}

bool CBoinc::DefaultBoincDataPathExists(){
    if(strBoincDataPath == "")
        return false;

    return true;
}

#ifdef _WIN32
std::string CBoinc::GetDataPathFromRegistry()
{
    std::string path = "";
    HKEY key;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Space Sciences Laboratory, U.C. Berkeley\\BOINC Setup\\"),0,KEY_READ|KEY_WOW64_64KEY,&key) != ERROR_SUCCESS)
    {
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Space Sciences Laboratory, U.C. Berkeley\\BOINC Setup\\"),0,KEY_READ|KEY_WOW64_32KEY,&key) != ERROR_SUCCESS)
        {
            return path;
        }
    }

    DWORD dwType = REG_SZ;
    char value[1024];
    DWORD length = 1024;

    if (RegQueryValueEx(key,TEXT("DATADIR"),NULL,&dwType,(LPBYTE)&value,&length) == ERROR_SUCCESS)
    {
        path = value;
    }

    RegCloseKey(key);

    return path;
}
#endif
