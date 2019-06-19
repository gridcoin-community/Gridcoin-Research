#ifndef GLOBAL_OBJECTS_NOUI_HPP
#define GLOBAL_OBJECTS_NOUI_HPP

#include "fwd.h"
#include <string>
#include <map>
#include <set>

extern int nBoincUtilization;
extern std::string sRegVer;
extern int nRegVersion;
extern bool bNetAveragesLoaded;
extern bool bForceUpdate;
extern bool fQtActive;
extern bool bGridcoinGUILoaded;

struct StructCPID
{
    bool initialized;
    bool Iscpidvalid;

    double utc;
    double rectime;
    double age;
    double verifiedutc;
    double verifiedage;
    uint32_t entries;
    double NetworkProjects;
    double Magnitude;
    double PaymentMagnitude;
    double owed;
    double payments;
    double interestPayments;
    double verifiedMagnitude;
    double TotalMagnitude;
    uint32_t LowLockTime;
    uint32_t HighLockTime;
    double Accuracy;
    double totalowed;
    double LastPaymentTime;
    double ResearchSubsidy;
    /* double ResearchAverageMagnitude; TotalMagnitude / (Accuracy+0.01) */
    double EarliestPaymentTime;
    double InterestSubsidy;
    double PaymentTimespan;
    int32_t LastBlock;
    double NetworkMagnitude;
    double NetworkAvgMagnitude;
    std::string cpid;
    std::string cpidhash;
    std::string projectname;
    std::string team;
    std::string boincpublickey;
    std::string errors;
    std::string cpidv2;
    std::string BlockHash;
    std::set<const CBlockIndex*> rewardBlocks;
};

struct MiningCPID
{
    bool initialized;
    double Magnitude;
    double RSAWeight;
    double LastPaymentTime;
    double ResearchSubsidy;
    double ResearchSubsidy2;
    double ResearchAge;
    double ResearchMagnitudeUnit;
    double ResearchAverageMagnitude;
    double InterestSubsidy;

    std::string projectname;
    std::string encboincpublickey;
    std::string cpid;
    std::string cpidhash;
    std::string enccpid;
    std::string aesskein;
    std::string encaes;
    std::string clientversion;
    std::string cpidv2;
    std::string email;
    std::string boincruntimepublickey;
    std::string GRCAddress;
    std::string lastblockhash;
    std::string Organization;
    std::string OrganizationKey;
    std::string NeuralHash;
    std::string superblock;
    std::string LastPORBlockHash;
    std::string CurrentNeuralHash;
    std::string BoincPublicKey;
    std::string BoincSignature;
};

//User CPIDs
extern std::map<std::string, StructCPID> mvCPIDs;
//Network Averages
extern std::map<std::string, StructCPID> mvNetwork;
extern std::map<std::string, StructCPID> mvNetworkCopy;
extern std::map<std::string, StructCPID> mvMagnitudes;
extern std::map<std::string, StructCPID> mvMagnitudesCopy;

//Global CPU Mining CPID:
extern MiningCPID GlobalCPUMiningCPID;

// Timers
extern std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded

#endif /* GLOBAL_OBJECTS_NOUI_HPP */
