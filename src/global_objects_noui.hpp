#ifndef GLOBAL_OBJECTS_NOUI_HPP
#define GLOBAL_OBJECTS_NOUI_HPP

extern int nBoincUtilization;
extern std::string sBoincMD5;
extern std::string sBoincBA;
extern std::string sRegVer;
extern std::string sBoincDeltaOverTime;
extern std::string sMinedHash;
extern std::string sSourceBlock;
extern int nRegVersion;
extern bool bDebugMode;
extern bool bBoincSubsidyEligible;
extern bool bPoolMiningMode;
extern bool bCPUMiningMode;
extern bool bRestartGridcoinMiner;
extern bool bCPIDsLoaded;
extern bool bProjectsInitialized;
extern int  iCriticalThreadDelay;
extern bool CreatingNewBlock;
extern bool bNetAveragesLoaded;
extern bool bForceUpdate;
extern bool bExecuteCode;
extern bool bAddressUser;
extern bool bCheckedForUpgrade;
extern bool bCheckedForUpgradeLive;
extern bool bGlobalcomInitialized;
extern bool bAllowBackToBack;
extern bool CreatingCPUBlock;
extern bool bStakeMinerOutOfSyncWithNetwork;
extern bool bDoTally;
extern bool bExecuteGridcoinServices;
extern bool bTallyFinished;
extern bool bGridcoinGUILoaded;
extern bool bRequestFromHonestNode;
extern double nGlobalHashCounter;

extern int miningthreadcount;
	
struct StructCPID 
{
    bool initialized;
    bool isvoucher;
    bool activeproject;
    bool Iscpidvalid;
    
    double rac;
    double utc;
    double rectime;
    double age;
    double verifiedrac;
    double verifiedutc;
    double verifiedrectime;
    double verifiedage;
    double entries;
    double AverageRAC;
    double NetworkProjects;
    double NetworkRAC;
    double TotalRAC;
    double TotalNetworkRAC;
    double Magnitude;
    double LastMagnitude;
    double PaymentMagnitude;
    double owed;
    double payments;
    double interestPayments;
    double outstanding;
    double verifiedTotalRAC;
    double verifiedMagnitude;
    double TotalMagnitude;
    double MagnitudeCount;
    double LowLockTime;
    double HighLockTime;
    double Accuracy;
    double totalowed;
    double longtermtotalowed;
    double longtermowed;
    double LastPaymentTime;
    double ResearchSubsidy;
    double ResearchSubsidy2;
    double ResearchAge;
    double ResearchMagnitudeUnit;
    double ResearchAverageMagnitude;
    double EarliestPaymentTime;
    double RSAWeight;
    double InterestSubsidy;
    double PaymentTimespan;
    double verifiedTotalNetworkRAC;
    double LastBlock;
    double NetworkMagnitude;
    double NetworkAvgMagnitude;
    double NetsoftRAC;
    double GRCQuote;
    double BTCQuote;
    double Canary;
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
    std::string PaymentTimestamps;
    std::string PaymentAmountsResearch;
    std::string PaymentAmountsInterest;
    std::string PaymentAmountsBlocks;
    std::string BlockHash;
    std::string GRCAddress;
    std::string LastPORBlockHash;
    std::string CurrentNeuralHash;
    std::string BoincPublicKey;
    std::string BoincSignature;
};



struct StructCPIDCache 
{
    std::string cpid;
    std::string cpidproject;
    std::string xml;
    bool initialized;
    double blocknumber;
};



struct StructBlockCache 
{
    std::string cpid;
    std::string project;
    double rac;
    std::string hashBoinc;
    bool initialized;
    int nHeight;
    std::string hash;
    int BlockType;
    int nVersion;
};



struct MiningCPID
{
    double rac;
    double pobdifficulty;
    unsigned int diffbytes;
    bool initialized;	
    double nonce;
    double NetworkRAC;
    double VouchedRAC;
    double VouchedNetworkRAC;
    double Magnitude;
    double Accuracy;
    double RSAWeight;
    double LastPaymentTime;
    double ResearchSubsidy;
    double ResearchSubsidy2;
    double ResearchAge;
    double ResearchMagnitudeUnit;
    double ResearchAverageMagnitude;
    double InterestSubsidy;
    double GRCQuote;
    double BTCQuote;
    int prevBlockType;
    double Canary;
    
    std::string projectname;
    std::string encboincpublickey;
    std::string cpid;
    std::string cpidhash;
    std::string enccpid;
    std::string aesskein;
    std::string encaes;
    std::string clientversion;
    std::string VouchedCPID;
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

extern std::map<int, int> blockcache;

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

//Boinc Valid Projects
extern std::map<std::string, StructCPID> mvBoincProjects; // Contains all of the allowed boinc projects;
// Timers
extern std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded
extern double nMinerPaymentCount;


#endif /* GLOBAL_OBJECTS_NOUI_HPP */



