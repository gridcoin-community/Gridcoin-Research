#ifndef GRIDCOIN_BOINC_H
#define GRIDCOIN_BOINC_H

#include <string>

class CBoinc
{
private:
    std::string strBoincDataPath;
    #ifdef _WIN32
    std::string GetDataPathFromRegistry();
    #endif // _WIN32
public:
    CBoinc();
    std::string GetBoincDataPath();
    bool DefaultBoincDataPathExists();
};

#endif
