#ifndef GRIDCOIN_BOINC_H
#define GRIDCOIN_BOINC_H

#include <string>

class CBoinc
{
private:
    std::string strBoincDataPath;
public:
    CBoinc();
    std::string GetBoincDataPath();
    bool DefaultBoincDataPathExists();
};

#endif
