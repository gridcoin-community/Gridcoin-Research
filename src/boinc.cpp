#include "boinc.h"
#include <boost/filesystem.hpp>

CBoinc::CBoinc(){
    strBoincDataPath = "";

    #ifdef _WIN32
    if (boost::filesystem::exists("C:\\ProgramData\\BOINC\\")){
        strBoincDataPath = "C:\\ProgramData\\BOINC\\";
    }
    else if(boost::filesystem::exists("C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\")){
        strBoincDataPath = "C:\\Documents and Settings\\All Users\\Application Data\\BOINC\\";
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
