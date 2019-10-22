#ifndef GLOBAL_OBJECTS_NOUI_HPP
#define GLOBAL_OBJECTS_NOUI_HPP

#include "fwd.h"
#include <string>
#include <map>
#include <set>

extern int nBoincUtilization;
extern std::string sRegVer;
extern bool bNetAveragesLoaded;
extern bool bForceUpdate;
extern bool fQtActive;
extern bool bGridcoinGUILoaded;

struct StructCPID
{
    bool initialized;

    double Magnitude;
    double payments;
    double TotalMagnitude;
    uint32_t LowLockTime;
    double Accuracy;
    double ResearchSubsidy;
    /* double ResearchAverageMagnitude; TotalMagnitude / (Accuracy+0.01) */
    double EarliestPaymentTime;
    double InterestSubsidy;
    int32_t LastBlock;
    double NetworkMagnitude;
    double NetworkAvgMagnitude;
    std::string cpid;
    std::string BlockHash;
    std::set<const CBlockIndex*> rewardBlocks;
};

//Network Averages
extern std::map<std::string, StructCPID> mvNetwork;
extern std::map<std::string, StructCPID> mvNetworkCopy;
extern std::map<std::string, StructCPID> mvMagnitudes;
extern std::map<std::string, StructCPID> mvMagnitudesCopy;

// Timers
extern std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded

#endif /* GLOBAL_OBJECTS_NOUI_HPP */
