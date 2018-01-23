#ifndef GLOBAL_OBJECTS_NOUI_HPP
#define GLOBAL_OBJECTS_NOUI_HPP

#include <string>
#include <map>

extern int nBoincUtilization;
extern std::string sRegVer;
extern int nRegVersion;
extern bool bNetAveragesLoaded;
extern bool bForceUpdate;
extern bool bGlobalcomInitialized;
extern bool bGridcoinGUILoaded;

struct StructCPID
{
    bool initialized;
    bool Iscpidvalid;

    double rac;
    double utc;
    double rectime;
    double age;
    double verifiedrac;
    double verifiedutc;
    double verifiedrectime;
    double verifiedage;
    uint32_t entries;
    double AverageRAC;
    double NetworkProjects;
    double NetworkRAC;
    double TotalRAC;
    double TotalNetworkRAC;
    double Magnitude;
    double PaymentMagnitude;
    double owed;
    double payments;
    double interestPayments;
    double verifiedTotalRAC;
    double verifiedMagnitude;
    double TotalMagnitude;
    uint32_t LowLockTime;
    uint32_t HighLockTime;
    double Accuracy;
    double totalowed;
    double LastPaymentTime;
    double ResearchSubsidy;
    double ResearchAverageMagnitude;
    double EarliestPaymentTime;
    double InterestSubsidy;
    double PaymentTimespan;
    int32_t LastBlock;
    double NetworkMagnitude;
    double NetworkAvgMagnitude;
    std::string cpid;
    std::string emailhash;
    std::string cpidhash;
    std::string projectname;
    std::string team;
    std::string verifiedteam;
    std::string boincpublickey;
    std::string link;
    std::string errors;
    std::string email;
    std::string boincruntimepublickey;
    std::string cpidv2;
    std::string BlockHash;
};



struct StructCPIDCache
{
    std::string xml;
    bool initialized;
};



struct MiningCPID
{
    double rac;
    double pobdifficulty;
    unsigned int diffbytes;
    bool initialized;
    double nonce;
    double NetworkRAC;
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
//Verified CPIDs
extern std::map<std::string, StructCPID> mvCreditNode;
//Network Averages
extern std::map<std::string, StructCPID> mvNetwork;
extern std::map<std::string, StructCPID> mvNetworkCopy;
extern std::map<std::string, StructCPID> mvMagnitudes;
extern std::map<std::string, StructCPID> mvMagnitudesCopy;


extern std::map<std::string, StructCPID> mvCreditNodeCPIDProject; //Contains verified CPID+Projects;
extern std::map<std::string, StructCPID> mvCreditNodeCPID;  //Contains verified CPID total Magnitude;
//Caches
extern std::map<std::string, StructCPIDCache> mvCPIDCache; //Contains cached blocknumbers for CPID+Projects;
extern std::map<std::string, StructCPIDCache> mvAppCache; //Contains cached blocknumbers for CPID+Projects;

//Global CPU Mining CPID:
extern MiningCPID GlobalCPUMiningCPID;

// Timers
extern std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded

#endif /* GLOBAL_OBJECTS_NOUI_HPP */
