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
extern volatile bool bRestartGridcoinMiner;
extern volatile bool bCPIDsLoaded;
extern volatile bool bProjectsInitialized;
extern volatile int  iCriticalThreadDelay;
extern volatile bool CreatingNewBlock;
extern volatile bool bNetAveragesLoaded;
extern volatile bool bForceUpdate;
extern volatile bool bExecuteCode;
extern volatile bool bAddressUser;
extern volatile bool bCheckedForUpgrade;
extern volatile bool bCheckedForUpgradeLive;

extern volatile bool bGlobalcomInitialized;


extern volatile bool bAllowBackToBack;
extern volatile bool CreatingCPUBlock;

extern volatile double nGlobalNonce;
extern volatile double nGlobalHashCounter;
extern volatile double nGlobalSolutionNonce;


extern int miningthreadcount;


/*
struct MiningEntry {
        double shares;
		std::string strComment;
        std::string strAccount;
		double payout;
		double rbpps;
		double totalutilization;
		double totalrows;
		double avgboinc;
		bool   paid;
		double payments;
		double lookback;
		double difficulty;
		int blockhour;
		int wallethour;
		std::string boinchash;
		int projectid;
		std::string projectuserid;
		std::string transactionid;
		double blocknumber;
		double locktime;
		std::string projectaddress;
		double cpupowverificationtime;
		double cpupowverificationresult;
		double cpupowverificationtries;
		std::string homogenizedkey;
		std::string cpupowhash;
		double credits;
		double lastpaid;
		double lastpaiddate;
		double totalpayments;
		double networkcredits;
		double compensation;
	    double owed;
	    double approvedtransactions;
	    double nextpaymentamount;
		double cputotalpayments;
		double dummy1;
		double dummy2;
		double dummy3;
		bool approved;


    };
	*/



	
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
		double outstanding;
		double verifiedTotalRAC;
		double verifiedTotalNetworkRAC;
		double verifiedMagnitude;
        double TotalMagnitude;
		double MagnitudeCount;
		double LowLockTime;
		double HighLockTime;
		double ConsensusTotalMagnitude;
		double ConsensusMagnitudeCount;
		double Accuracy;
		double ConsensusMagnitude;
		double totalowed;
		double longtermtotalowed;
		double longtermowed;
		double LastPaymentTime;
		double EarliestPaymentTime;
		double RSAWeight;
		double PaymentTimespan;
		double ResearchSubsidy;

	

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
		
		
	};



    struct StructCPIDCache 
	{
		std::string cpid;
		std::string cpidproject;
		std::string xml;
		bool initialized;
		double blocknumber;
	};



	struct StructBlockCache {
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
		std::string projectname;
		double rac;
		std::string encboincpublickey;
		std::string cpid;
		std::string cpidhash;
		double pobdifficulty;
		unsigned int diffbytes;
		bool initialized;	
		std::string enccpid;
		std::string aesskein;
		std::string encaes;
		double nonce;
		double NetworkRAC;
		int prevBlockType;
		std::string clientversion;
		std::string VouchedCPID;
		std::string cpidv2;
		std::string email;
		std::string boincruntimepublickey;
		std::string GRCAddress;
		std::string lastblockhash;
		double VouchedRAC;
		double VouchedNetworkRAC;
		double Magnitude;
		double ConsensusTotalMagnitude;
		double ConsensusMagnitudeCount;
		double Accuracy;
		double ConsensusMagnitude;
		double RSAWeight;
		double LastPaymentTime;
		double ResearchSubsidy;

	};

	
        






extern std::map<int, int> blockcache;

//User CPIDs 
extern std::map<std::string, StructCPID> mvCPIDs;
//Verified CPIDs
extern std::map<std::string, StructCPID> mvCreditNode;
//Network Averages
extern std::map<std::string, StructCPID> mvNetwork;
extern std::map<std::string, StructCPID> mvNetworkCPIDs;
extern std::map<std::string, StructCPID> mvMagnitudes;



extern std::map<std::string, StructCPID> mvCreditNodeCPIDProject; //Contains verified CPID+Projects;
extern std::map<std::string, StructCPID> mvCreditNodeCPID;  //Contains verified CPID total Magnitude;
//Caches
extern std::map<std::string, StructCPIDCache> mvCPIDCache; //Contains cached blocknumbers for CPID+Projects;

extern std::map<std::string, StructBlockCache> mvBlockCache;  //Contains Cached Blocks
extern std::map<std::string, StructCPIDCache> mvAppCache; //Contains cached blocknumbers for CPID+Projects;

//Global CPU Mining CPID:
extern MiningCPID GlobalCPUMiningCPID;


//Boinc Valid Projects
extern std::map<std::string, StructCPID> mvBoincProjects; // Contains all of the allowed boinc projects;
// Timers
extern std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded


extern double nMinerPaymentCount;


#endif /* GLOBAL_OBJECTS_NOUI_HPP */



