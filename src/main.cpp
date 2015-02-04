// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "alert.h"
#include "checkpoints.h"
#include "db.h"
#include "txdb.h"
#include "net.h"
#include "init.h"
#include "ui_interface.h"
#include "kernel.h"
#include "zerocoin/Zerocoin.h"
#include <math.h>       /* pow */

#include "scrypt.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <ctime>
#include <openssl/md5.h>
#include <boost/lexical_cast.hpp>
#include "global_objects_noui.hpp"
#include "bitcoinrpc.h"
#include "util.h"

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/algorithm/string/join.hpp>
#include "cpid.h"

//Resend Unsent Tx

extern void EraseOrphans();
int DownloadBlocks();

std::string GetCommandNonce(std::string command);
std::string DefaultBlockKey(int key_length);


using namespace std;
using namespace boost;
std::string DefaultBoincHashArgs();

//
// Global state
//

CCriticalSection cs_setpwalletRegistered;
set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;

extern std::string NodeAddress(CNode* pfrom);

CTxMemPool mempool;
unsigned int nTransactionsUpdated = 0;
unsigned int REORGANIZE_FAILED = 0;
extern void RemoveNetworkMagnitude(double LockTime, std::string cpid, MiningCPID bb, double mint, bool IsStake);

extern double GetChainDailyAvgEarnedByCPID(std::string cpid, int64_t locktime, double& out_payments, double& out_daily_avg_payments);

extern bool ChainPaymentViolation(std::string cpid, int64_t locktime, double Proposed_Subsidy);



unsigned int WHITELISTED_PROJECTS = 0;

unsigned int CHECKPOINT_VIOLATIONS = 0;
int64_t nLastTallied = 0;
int64_t nCPIDsLoaded = 0;
extern bool IsCPIDValidv3(std::string cpidv2, bool allow_investor);

std::string DefaultOrg();
std::string DefaultOrgKey(int key_length);

extern std::string boinc_hash(const std::string str);

double MintLimiter(double PORDiff,int64_t RSA_WEIGHT,std::string cpid,int64_t locktime);

extern std::string ComputeCPIDv2(std::string email, std::string bpk, uint256 blockhash);
extern double GetBlockDifficulty(unsigned int nBits);
double GetLastPaymentTimeByCPID(std::string cpid);
extern bool Contains(std::string data, std::string instring);
extern uint256 GetBlockHash256(const CBlockIndex* pindex_hash);
extern bool LockTimeRecent(double locktime);
extern double CoinToDouble(double surrogate);
extern double coalesce(double mag1, double mag2);
extern bool AmIGeneratingBackToBackBlocks();
extern int64_t Floor(int64_t iAmt1, int64_t iAmt2);
extern double PreviousBlockAge();
void CheckForUpgrade();

int AddressUser();

int64_t GetRSAWeightByCPID(std::string cpid);

extern MiningCPID GetMiningCPID();
extern StructCPID GetStructCPID();

extern std::string GetArgument(std::string arg, std::string defaultvalue);


extern void SetAdvisory();
extern bool InAdvisory();

json_spirit::Array MagnitudeReportCSV(bool detail);

bool bNewUserWizardNotified = false;
int64_t nLastBlockSolved = 0;  //Future timestamp
int64_t nLastBlockSubmitted = 0;

uint256 muGlobalCheckpointHash = 0;
uint256 muGlobalCheckpointHashRelayed = 0;
int muGlobalCheckpointHashCounter = 0;
///////////////////////MINOR VERSION////////////////////////////////
int MINOR_VERSION = 195;

			
bool IsUserQualifiedToSendCheckpoint();

std::string BackupGridcoinWallet();


extern double GetPoSKernelPS2();
extern void TallyInBackground();
extern std::string GetBoincDataDir2();


double GetUntrustedMagnitude(std::string cpid, double& out_owed);




extern uint256 GridcoinMultipleAlgoHash(std::string t1);
extern bool OutOfSyncByAgeWithChanceOfMining();

int RebootClient();

std::string YesNo(bool bin);

extern  double GetGridcoinBalance(std::string SendersGRCAddress);


int64_t GetMaximumBoincSubsidy(int64_t nTime);

extern bool IsLockTimeWithinMinutes(int64_t locktime, int minutes);

extern bool IsLockTimeWithinMinutes(double locktime, int minutes);


double GetNetworkProjectCountWithRAC();

extern double CalculatedMagnitude(int64_t locktime);


extern int64_t GetCoinYearReward(int64_t nTime);


extern bool Resuscitate();


extern bool CreditCheckOnline(std::string cpid, double purported_magnitude, double mint, uint64_t nCoinAge, uint64_t nFees, int64_t locktime, double RSAWeight);

extern void AddNetworkMagnitude(double LockTime, std::string cpid, MiningCPID bb, double mint, bool IsStake);

extern double GetMagnitude(std::string cpid, double purported, bool UseNetSoft);





map<uint256, CBlockIndex*> mapBlockIndex;
set<pair<COutPoint, unsigned int> > setStakeSeen;
libzerocoin::Params* ZCParams;

CBigNum bnProofOfWorkLimit(~uint256(0) >> 20); // "standard" scrypt target limit for proof of work, results with 0,000244140625 proof-of-work difficulty
CBigNum bnProofOfStakeLimit(~uint256(0) >> 20);
CBigNum bnProofOfStakeLimitV2(~uint256(0) >> 20);
CBigNum bnProofOfWorkLimitTestNet(~uint256(0) >> 16);

//Gridcoin Minimum Stake Age (16 Hours)
unsigned int nStakeMinAge = 16 * 60 * 60; // 16 hours
unsigned int nStakeMaxAge = -1; // unlimited
unsigned int nModifierInterval = 10 * 60; // time to elapse before new modifier is computed

// Gridcoin:
int nCoinbaseMaturity = 100;
CBlockIndex* pindexGenesisBlock = NULL;
int nBestHeight = -1;

uint256 nBestChainTrust = 0;
uint256 nBestInvalidTrust = 0;

uint256 hashBestChain = 0;
CBlockIndex* pindexBest = NULL;
int64_t nTimeBestReceived = 0;

CMedianFilter<int> cPeerBlockCounts(5, 0); // Amount of blocks that other nodes claim to have

map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*> mapOrphanBlocksByPrev;
set<pair<COutPoint, unsigned int> > setStakeSeenOrphan;

map<uint256, CTransaction> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "Gridcoin Signed Message:\n";

// Settings
int64_t nTransactionFee = MIN_TX_FEE;
int64_t nReserveBalance = 0;
int64_t nMinimumInputValue = 0;


extern enum Checkpoints::CPMode CheckpointsMode;



//leveldb::DB *txdb; // global pointer for LevelDB object instance


// Gridcoin - Rob Halford

extern std::string GetHttpPage(std::string cpid, bool usedns, bool clearcache);

extern std::string RetrieveMd5(std::string s1);
extern std::string RacStringFromDiff(double RAC, unsigned int diffbytes);
extern std::string aes_complex_hash(uint256 scrypt_hash);
extern  bool TallyNetworkAverages(bool ColdBoot);
bool FindRAC(bool CheckingWork,std::string TargetCPID, std::string TargetProjectName, double pobdiff,
	bool bCreditNodeVerification, std::string& out_errors, int& out_position);
volatile bool bNetAveragesLoaded = false;
volatile bool bRestartGridcoinMiner = false;
volatile bool bForceUpdate = false;
volatile bool bExecuteCode = false;
volatile bool bAddressUser = false;
volatile bool bCheckedForUpgrade = false;
volatile bool bCheckedForUpgradeLive = false;

volatile bool bGlobalcomInitialized = false;

extern void PobSleep(int milliseconds);
extern bool CheckWorkCPU(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey);

extern double LederstrumpfMagnitude2(double Magnitude, int64_t locktime);


extern double cdbl(std::string s, int place);
extern double GetBlockValueByHash(uint256 hash);
extern void WriteAppCache(std::string key, std::string value);

extern std::string AppCache(std::string key);

void StartPostOnBackgroundThread(int height, MiningCPID miningcpid, uint256 hashmerkleroot, double nNonce, double subsidy, unsigned int nVersion, std::string message);
extern void LoadCPIDsInBackground();
bool SubmitGridcoinCPUWork(CBlock* pblock, CReserveKey& reservekey, double nonce);
CBlock* getwork_cpu(MiningCPID miningcpid, bool& succeeded,CReserveKey& reservekey);
extern int GetBlockType(uint256 prevblockhash);
extern void PoBGPUMiner(CBlock* pblock, MiningCPID& miningcpid);
extern bool GetTransactionFromMemPool(const uint256 &hash, CTransaction &txOut);
extern unsigned int DiffBytes(double PoBDiff);
extern int Races(int iMax1000);
int ReindexWallet();

std::string cached_getblocks_args = "";
extern bool AESSkeinHash(unsigned int diffbytes, double rac, uint256 scrypthash, std::string& out_skein, std::string& out_aes512);
std::string DefaultGetblocksCommand();
extern int TestAESHash(double rac, unsigned int diffbytes, uint256 scrypt_hash, std::string aeshash);
CClientUIInterface uiDog;
void ExecuteCode();


extern double CreditCheck(std::string cpid, std::string projectname);

extern void CreditCheck(std::string cpid, bool clearcache);



extern void ThreadCPIDs();
extern std::string GetGlobalStatus();
CBlockIndex* GetBlockIndex2(uint256 blockhash, int& out_height);

extern void printbool(std::string comment, bool boo);
extern void PobSleep(int milliseconds);
extern bool OutOfSyncByAge();
extern std::vector<std::string> split(std::string s, std::string delim);
extern bool ProjectIsValid(std::string project);

extern std::string SerializeBoincBlock(MiningCPID mcpid);
extern MiningCPID DeserializeBoincBlock(std::string block);
extern void InitializeCPIDs();
extern void ResendWalletTransactions2();
double GetPoBDifficulty();
double GetNetworkAvgByProject(std::string projectname);
extern bool IsCPIDValid_Retired(std::string cpid, std::string ENCboincpubkey);


extern bool IsCPIDValidv2(MiningCPID& mc, int height);

extern void FindMultiAlgorithmSolution(CBlock* pblock, uint256 hash, uint256 hashTaget, double miningrac);

extern std::string getfilecontents(std::string filename);
extern std::string ToOfficialName(std::string proj);
extern bool LessVerbose(int iMax1000);
extern bool GetBlockNew(uint256 blockhash, int& out_height, CBlock& blk, bool bForceDiskRead);
extern std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);

extern void ShutdownGridcoinMiner();
extern bool OutOfSync();
extern MiningCPID GetNextProject(bool bForce);
extern void HarvestCPIDs(bool cleardata);
extern  bool TallyNetworkAverages(bool ColdBoot);
bool FindRAC(bool CheckingWork,std::string TargetCPID, std::string TargetProjectName, double pobdiff,
	bool bCreditNodeVerification, std::string& out_errors, int& out_position);
bool FindTransactionSlow(uint256 txhashin, CTransaction& txout,  std::string& out_errors);
std::string msCurrentRAC = "";

//static boost::thread_group* minerThreads = NULL;
static boost::thread_group* cpidThreads = NULL;
static boost::thread_group* tallyThreads = NULL;
extern void FlushGridcoinBlockFile(bool fFinalize);






///////////////////////////////
// Standard Boinc Projects ////
///////////////////////////////



 //CPU Projects:
 std::string 	msMiningProject = "";
 std::string 	msMiningCPID = "";
 std::string    msENCboincpublickey = "";
 double      	mdMiningRAC =0;
 double         mdMiningNetworkRAC = 0;
 double			mdPORNonce = 0;
 double         mdPORNonceSolved = 0;
 double         mdLastPorNonce = 0;
 double         mdMachineTimer = 0;
 double         mdMachineTimerLast = 0;

 bool           mbBlocksDownloaded = false;

 std::string    msHashBoinc    = "";
 std::string    msHashBoincTxId= "";
 std::string    msMiningErrors = "";
 std::string    msMiningErrors2 = "";
 std::string    msMiningErrors3 = "";
 std::string    msMiningErrors4 = "";
 std::string    msMiningErrors5 = "";
 std::string    msMiningErrors6 = "";
 std::string    msMiningErrors7 = "";
 std::string    Organization = "";
 std::string    OrganizationKey = "";

 int nGrandfather = 131764;

 //GPU Projects:
 std::string 	msGPUMiningProject = "";
 std::string 	msGPUMiningCPID = "";
 std::string    msGPUENCboincpublickey = "";
 std::string    msGPUboinckey = "";
 double    	    mdGPUMiningRAC = 0;
 double         mdGPUMiningNetworkRAC = 0;
 // Stats for Main Screen:
 double         mdLastPoBDifficulty = 0;
 double         mdLastDifficulty = 0;
 std::string    msGlobalStatus = "";
 std::string    msMyCPID = "";
 double         mdOwed = 0;
 // CPU Miner threads global vars
 
 volatile double nGlobalNonce = 0;
 volatile double nGlobalHashCounter = 0;
 volatile double nGlobalSolutionNonce = 0;


bool fImporting = false;
bool fReindex = false;
bool fBenchmark = false;
bool fTxIndex = false;



int nBestAccepted = -1;

uint256 nBestChainWork = 0;
uint256 nBestInvalidWork = 0;
//Optimizing internal cpu miner:
uint256 GlobalhashMerkleRoot = 0;
uint256 GlobalSolutionPowHash = 0;

				

// Gridcoin status    *************
MiningCPID GlobalCPUMiningCPID = GetMiningCPID();
int nBoincUtilization = 0;
double nMinerPaymentCount = 0;
int nPrint = 0;
std::string sBoincMD5 = "";
std::string sBoincBA = "";
std::string sRegVer = "";
std::string sBoincDeltaOverTime = "";
std::string sMinedHash = "";
std::string sSourceBlock = "";
std::string sDefaultWalletAddress = "";


//std::map<std::string, MiningEntry> minerpayments;
//std::map<std::string, MiningEntry> cpuminerpayments;
//std::map<std::string, MiningEntry> cpupow;
//std::map<std::string, MiningEntry> cpuminerpaymentsconsolidated;

std::map<std::string, StructCPID> mvCPIDs;        //Contains the project stats at the user level
std::map<std::string, StructCPID> mvCreditNode;   //Contains the verified stats at the user level

std::map<std::string, StructCPID> mvNetwork;      //Contains the project stats at the network level
std::map<std::string, StructCPID> mvNetworkCPIDs; //Contains CPID+Projects at the network level
std::map<std::string, StructCPID> mvCreditNodeCPIDProject; //Contains verified CPID+Projects;
std::map<std::string, StructCPID> mvCreditNodeCPID;        // Contains verified CPID Magnitudes;
std::map<std::string, StructCPIDCache> mvCPIDCache; //Contains cached blocknumbers for CPID+Projects;
std::map<std::string, StructCPIDCache> mvAppCache; //Contains cached blocknumbers for CPID+Projects;
std::map<std::string, StructBlockCache> mvBlockCache;  //Contains Cached Blocks
std::map<std::string, StructCPID> mvBoincProjects; // Contains all of the allowed boinc projects;
std::map<std::string, StructCPID> mvMagnitudes; // Contains Magnitudes by CPID & Outstanding Payments Owed per CPID

std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded


// End of Gridcoin Global vars


std::map<int, int> blockcache;


bool bDebugMode = false;

bool bPoolMiningMode = false;
bool bBoincSubsidyEligible = false;
bool bCPUMiningMode = false;






//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

// These functions dispatch to one or all registered wallets


bool GetBlockNew(uint256 blockhash, int& out_height, CBlock& blk, bool bForceDiskRead)
{
    try 
	{
			//First check the cache:
			StructBlockCache cache;
			cache = mvBlockCache[blockhash.GetHex()]; 
			if (cache.initialized)
			{
				 CTransaction txNew;
				 txNew.vin.resize(1);
				 txNew.vin[0].prevout.SetNull();
				 txNew.vout.resize(1);
				 txNew.hashBoinc = cache.hashBoinc;
				 blk.vtx.resize(1);
				 blk.vtx[0] = txNew;
				 blk.nVersion  = cache.nVersion;
				 if (!bForceDiskRead) return true;
			}
		
			CBlockIndex* pblockindex = mapBlockIndex[blockhash];
			bool result = blk.ReadFromDisk(pblockindex);
			if (!result) return false;
			//Cache the block
			cache.initialized = true;
			cache.hashBoinc = blk.vtx[0].hashBoinc;
			cache.hash = blockhash.GetHex();
			cache.nVersion = blk.nVersion;
			mvBlockCache[blockhash.GetHex()] = cache;
			out_height = pblockindex->nHeight;
			return true;
	}
	
	catch (std::exception &e) 
	{
		printf("Catastrophic error retrieving GetBlockNew\r\n");
		return false;
	}
	catch(...)
	{
		printf("Catastrophic error retrieving block in GetBlockNew (06182014) \r\n");
		return false;
	}


}





  double GetGridcoinBalance(std::string SendersGRCAddress)
  {
    int nMinDepth = 1;
    int nMaxDepth = 9999999;
	if (SendersGRCAddress=="") return 0;
    set<CBitcoinAddress> setAddress;
    CBitcoinAddress address(SendersGRCAddress);
	if (!address.IsValid())
	{
		printf("Checkpoints::GetGridcoinBalance::InvalidAddress");
        return 0;
	}
	setAddress.insert(address);
    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, false);
	double global_total = 0;
    BOOST_FOREACH(const COutput& out, vecOutputs)
    {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;
        if(setAddress.size())
        {
            CTxDestination address;
            if(!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
                continue;
            if (!setAddress.count(address))
                continue;
        }
        int64_t nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        CTxDestination address;
		global_total += nValue;
    }

    return CoinToDouble(global_total);
}

  




bool TimerMain(std::string timer_name, int max_ms)
{
	mvTimers[timer_name] = mvTimers[timer_name] + 1;
	if (mvTimers[timer_name] > max_ms)
	{
		mvTimers[timer_name]=0;
		return true;
	}
	return false;
}



double GetPoSKernelPS2()
{
    int nPoSInterval = 72;
    double dStakeKernelsTriedAvg = 0;
    int nStakesHandled = 0, nStakesTime = 0;

    CBlockIndex* pindex = pindexBest;;
    CBlockIndex* pindexPrevStake = NULL;

    while (pindex && nStakesHandled < nPoSInterval)
    {
        if (pindex->IsProofOfStake())
        {
            dStakeKernelsTriedAvg += GetDifficulty(pindex) * 4294967296.0;
            nStakesTime += pindexPrevStake ? (pindexPrevStake->nTime - pindex->nTime) : 0;
            pindexPrevStake = pindex;
            nStakesHandled++;
        }

        pindex = pindex->pprev;
    }

    double result = 0;

    if (nStakesTime)
        result = dStakeKernelsTriedAvg / nStakesTime;

    if (IsProtocolV2(nBestHeight))
        result *= STAKE_TIMESTAMP_MASK + 1;

    return result/100;
}


std::string GetGlobalStatus()
{
	
	try
	{
		std::string status = "";
		double boincmagnitude = CalculatedMagnitude( GetAdjustedTime());
		uint64_t nWeight = 0;
		pwalletMain->GetStakeWeight(nWeight);
		nBoincUtilization = boincmagnitude; //Legacy Support for the about screen
		//Vlad : Request to make overview page magnitude consistent:
		double out_magnitude = 0;
		double out_owed = 0;
		out_magnitude = GetUntrustedMagnitude(GlobalCPUMiningCPID.cpid,out_owed);
		// End of Boinc Magnitude update
		double weight = nWeight/COIN;
		double PORDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
		std::string boost_version = "";
		std::ostringstream sBoost;
		sBoost << boost_version  << "Using Boost "     
			  << BOOST_VERSION / 100000     << "."  // major version
			  << BOOST_VERSION / 100 % 1000 << "."  // minior version
			  << BOOST_VERSION % 100                // patch level
			  << "";
		std::string sWeight = RoundToString((double)weight,0);
		if ((double)weight > 100000000000000) 
		{
				sWeight = sWeight.substr(0,13) + "E" + RoundToString((double)sWeight.length()-13,0);
		}
		status = "&nbsp;<br>Blocks: " + RoundToString((double)nBestHeight,0) + "; PoR Difficulty: " 
			+ RoundToString(PORDiff,3) + "; Net Weight: " + RoundToString(GetPoSKernelPS2(),2)  
			+ "<br>Boinc Weight: " +  sWeight + "; Status: " + msMiningErrors 
			+ "<br>Magnitude: " + RoundToString(out_magnitude,3) + "; Project: " + msMiningProject
			+ "<br>" + msMiningErrors2 + " " + msMiningErrors3 + " " + msMiningErrors4 + 
			+ "<br>" + msMiningErrors5 + " " + msMiningErrors6 + " " + msMiningErrors7 +
			+ "<br>" + sBoost.str();


		//The last line break is for Windows 8.1 Huge Toolbar
		msGlobalStatus = status;
		return status;
	}
	catch (std::exception& e)
	{
			msMiningErrors = "Error obtaining status.";

			printf("Error obtaining status\r\n");
			return "";
		}
		catch(...)
		{
			msMiningErrors = "Error obtaining status (08-18-2014).";
			return "";
		}
	
}



std::string AppCache(std::string key)
{

	StructCPIDCache setting = mvAppCache["cache"+key];
	if (!setting.initialized)
	{
		setting.initialized=true;
		setting.xml = "";
		mvAppCache.insert(map<string,StructCPIDCache>::value_type("cache"+key,setting));
	    mvAppCache["cache"+key]=setting;
	}
	if (setting.xml != "") 
	{
			//printf("AppCachehit on %s",setting.xml.c_str());
	}
	return setting.xml;
}



bool Timer_Main(std::string timer_name, int max_ms)
{
	mvTimers[timer_name] = mvTimers[timer_name] + 1;
	if (mvTimers[timer_name] > max_ms)
	{
		mvTimers[timer_name]=0;
		return true;
	}
	return false;
}



void WriteAppCache(std::string key, std::string value)
{
	StructCPIDCache setting = mvAppCache["cache"+key];
	if (!setting.initialized)
	{
		setting.initialized=true;
		setting.xml = "";
		mvAppCache.insert(map<string,StructCPIDCache>::value_type("cache"+key,setting));
	    mvAppCache["cache"+key]=setting;
	}
	setting.xml = value;
	mvAppCache["cache"+key]=setting;
}



void RegisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.insert(pwalletIn);
    }
}

void UnregisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.erase(pwalletIn);
    }
}



unsigned int DiffBytes(double PoBDiff)
{
	std::string pob = RoundToString(PoBDiff,2);
    double newpob   = cdbl(pob,2);
	unsigned int bytes = 7;
	if (newpob <= .16)                  bytes = 7;
	if (newpob > .16 && newpob <= .5)   bytes = 8;
	if (newpob >  .5 && newpob < 1)     bytes = 9;
	if (newpob >=  1 && newpob <= 1.5)  bytes = 10;
	if (newpob > 1.5 && newpob <= 2)    bytes = 11;
	if (newpob > 2   && newpob <= 5)    bytes = 12;
	if (newpob > 5   && newpob <= 10)   bytes = 13;
	if (newpob > 10  && newpob <= 11)   bytes = 14;
	if (newpob > 11) bytes = newpob+5;
	return bytes;
}



MiningCPID GetNextProject(bool bForce)
{

	if (GlobalCPUMiningCPID.projectname.length() > 3 && !bForce)
	{
	
				if (!Timer_Main("globalcpuminingcpid",20))
				{
					//Prevent Thrashing
					return GlobalCPUMiningCPID;
				}
		
	}
	msMiningProject = "";
	msMiningCPID = "";
	mdMiningRAC = 0;
	msENCboincpublickey = "";

	GlobalCPUMiningCPID.cpid="";
	GlobalCPUMiningCPID.cpidv2 = "";
	GlobalCPUMiningCPID.projectname ="";
	GlobalCPUMiningCPID.rac=0;
	GlobalCPUMiningCPID.encboincpublickey = "";
	GlobalCPUMiningCPID.pobdifficulty = 0;
	GlobalCPUMiningCPID.diffbytes = 0;
	GlobalCPUMiningCPID.lastblockhash = "0";

	if (fDebug) printf("qq0.");

	if ( (IsInitialBlockDownload() || !bCPIDsLoaded) && !bForce) 
	{
		    printf("CPUMiner: Gridcoin is downloading blocks Or CPIDs are not yet loaded...");
			MilliSleep(200);
			return GlobalCPUMiningCPID;
	}
		
	
	try 
	{
	
		if (mvCPIDs.size() < 1)
		{
			printf("Gridcoin has no CPIDs...");
			//Let control reach the investor area

		}

		int iValidProjects=0;
		//Count valid projects:
		for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii) 
		{
				StructCPID structcpid = GetStructCPID();
				structcpid = mvCPIDs[(*ii).first];
				if (structcpid.initialized) 
				{ 
					if (structcpid.Iscpidvalid && structcpid.verifiedrac > 100)
					{
						iValidProjects++;
					}
				}
		}
		

		// Find next available CPU project:
		int iDistributedProject = 0;
		int iRow = 0;

		if (iValidProjects > 0)
		{
		for (int i = 0; i <= 3;i++)
		{
			iRow=0;
			iDistributedProject = (rand() % iValidProjects)+1;

		
			for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii) 
			{

				StructCPID structcpid = GetStructCPID();
				structcpid = mvCPIDs[(*ii).first];
				if (structcpid.initialized) 
				{ 
					if (structcpid.Iscpidvalid && structcpid.projectname.length() > 2 && structcpid.verifiedrac > 100)
					{
							iRow++;
							if (i==3 || iDistributedProject == iRow)
							{
								//Only used for global status:
								msMiningProject = structcpid.projectname;
								msMiningCPID = structcpid.cpid;
								mdMiningRAC = structcpid.verifiedrac;
								msENCboincpublickey = structcpid.boincpublickey;
								if (LessVerbose(5)) printf("Ready to CPU Mine project %s     RAC(%f)  enc %s\r\n",	structcpid.projectname.c_str(),structcpid.rac, msENCboincpublickey.c_str());
								//Required for project to be mined in a block:
								GlobalCPUMiningCPID.cpid=structcpid.cpid;
								GlobalCPUMiningCPID.projectname = structcpid.projectname;
								GlobalCPUMiningCPID.rac=structcpid.verifiedrac;
								GlobalCPUMiningCPID.encboincpublickey = structcpid.boincpublickey;
								GlobalCPUMiningCPID.enccpid = structcpid.boincpublickey;
								GlobalCPUMiningCPID.encaes = structcpid.boincpublickey;
								GlobalCPUMiningCPID.pobdifficulty = GetPoBDifficulty();
		    					double ProjectRAC = GetNetworkAvgByProject(GlobalCPUMiningCPID.projectname);
								GlobalCPUMiningCPID.NetworkRAC = ProjectRAC;
								mdMiningNetworkRAC = GlobalCPUMiningCPID.NetworkRAC;
								double purported = 0;
								if (GlobalCPUMiningCPID.rac > 0) purported=1;
								GlobalCPUMiningCPID.Magnitude = GetMagnitude(GlobalCPUMiningCPID.cpid,purported,true);
								if (fDebug) printf("For CPID %s Verified Magnitude = %f",GlobalCPUMiningCPID.cpid.c_str(),GlobalCPUMiningCPID.Magnitude);
								//Reserved for GRC Speech Synthesis
								msMiningErrors = "Boinc Mining";
								GlobalCPUMiningCPID.RSAWeight = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
								GlobalCPUMiningCPID.LastPaymentTime = GetLastPaymentTimeByCPID(GlobalCPUMiningCPID.cpid);
								return GlobalCPUMiningCPID;
							}
						
					}

				}
			}

		}
		}

		msMiningErrors = "All BOINC projects exhausted.";
		msMiningProject = "INVESTOR";
		msMiningCPID = "INVESTOR";
		mdMiningRAC = 0;
		msENCboincpublickey = "";
		GlobalCPUMiningCPID.initialized = true;
		GlobalCPUMiningCPID.cpid="INVESTOR";
		GlobalCPUMiningCPID.projectname = "INVESTOR";
		GlobalCPUMiningCPID.cpidv2="INVESTOR";
		GlobalCPUMiningCPID.cpidhash = "";
		GlobalCPUMiningCPID.email = "INVESTOR";
		GlobalCPUMiningCPID.boincruntimepublickey = "INVESTOR";
		GlobalCPUMiningCPID.rac=0;
		GlobalCPUMiningCPID.encboincpublickey = "";
		GlobalCPUMiningCPID.enccpid = "";
		GlobalCPUMiningCPID.NetworkRAC = 0;
		GlobalCPUMiningCPID.Magnitude = 0;
        GlobalCPUMiningCPID.clientversion = "";
		GlobalCPUMiningCPID.RSAWeight = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
		GlobalCPUMiningCPID.LastPaymentTime = nLastBlockSolved;
		mdMiningNetworkRAC = 0;
	  	}
		catch (std::exception& e)
		{
			msMiningErrors = "Error obtaining next project.  Error 16172014.";

			printf("Error obtaining next project\r\n");
		}
		catch(...)
		{
			msMiningErrors = "Error obtaining next project.  Error 06172014.";
			printf("Error obtaining next project 2.\r\n");
		}
		return GlobalCPUMiningCPID;

}







// check whether the passed transaction is from us
bool static IsFromMe(CTransaction& tx)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        if (pwallet->IsFromMe(tx))
            return true;
    return false;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx,wtx))
            return true;
    return false;
}

// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->EraseFromWallet(hash);
}

// make sure all wallets know about the given transaction, in the given block
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fConnect)
{
    if (!fConnect)
    {
        // ppcoin: wallets need to refund inputs when disconnecting coinstake
        if (tx.IsCoinStake())
        {
            BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
                if (pwallet->IsFromMe(tx))
                    pwallet->DisableTransaction(tx);
        }
        return;
    }

    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->AddToWalletIfInvolvingMe(tx, pblock, fUpdate);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

// notify wallets about an updated transaction
void static UpdatedTransaction(const uint256& hashTx)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->Inventory(hash);
}

// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
{
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
        pwallet->ResendWalletTransactions(fForce);
}


double CoinToDouble(double surrogate)
{
	
	double coin = (double)surrogate/(double)COIN;
	return coin;
}


double GetTotalBalance()
{
	double total = 0;
    BOOST_FOREACH(CWallet* pwallet, setpwalletRegistered)
	{
        total = total + pwallet->GetBalance();
		std::string audit = RoundToString(total,8);
		//printf("MyUserBalance: %s",audit.c_str());
	}
	return total/COIN;
}
//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:

    size_t nSize = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);

    if (nSize > 5000)
    {
        printf("ignoring large orphan tx (size: %"PRIszu", hash: %s)\n", nSize, hash.ToString().substr(0,10).c_str());
        return false;
    }

    mapOrphanTransactions[hash] = tx;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    printf("stored orphan tx %s (mapsz %"PRIszu")\n", hash.ToString().substr(0,10).c_str(),
        mapOrphanTransactions.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CTransaction& tx = mapOrphanTransactions[hash];
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        mapOrphanTransactionsByPrev[txin.prevout.hash].erase(hash);
        if (mapOrphanTransactionsByPrev[txin.prevout.hash].empty())
            mapOrphanTransactionsByPrev.erase(txin.prevout.hash);
    }
    mapOrphanTransactions.erase(hash);
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans)
    {
        // Evict a random orphan:
        uint256 randomhash = GetRandHash();
        map<uint256, CTransaction>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}




std::string DefaultWalletAddress() 
{
	try {
	//Gridcoin - Find the default public GRC address
	if (sDefaultWalletAddress.length() > 0) return sDefaultWalletAddress;

    string strAccount;
	BOOST_FOREACH(const PAIRTYPE(CTxDestination, string)& item, pwalletMain->mapAddressBook)
    {
    	 const CBitcoinAddress& address = item.first;
		 const std::string& strName = item.second;
		 bool fMine = IsMine(*pwalletMain, address.Get());
		 if (fMine && strName == "Default") {
			 sDefaultWalletAddress=CBitcoinAddress(address).ToString();
			 return sDefaultWalletAddress;
		 }
    }


    //Cant Find

	BOOST_FOREACH(const PAIRTYPE(CTxDestination, string)& item, pwalletMain->mapAddressBook)
    {
    	 const CBitcoinAddress& address = item.first;
		 //const std::string& strName = item.second;
		 bool fMine = IsMine(*pwalletMain, address.Get());
		 if (fMine) {
			 sDefaultWalletAddress=CBitcoinAddress(address).ToString();
			 return sDefaultWalletAddress;
		 }
    }
	}
	 catch (std::exception& e)
	 {
	 }
       

	return "NA";
}








//////////////////////////////////////////////////////////////////////////////
//
// CTransaction and CTxIndex
//

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
{
    SetNull();
    if (!txdb.ReadTxIndex(prevout.hash, txindexRet))
        return false;
    if (!ReadFromDisk(txindexRet.pos))
        return false;
    if (prevout.n >= vout.size())
    {
        SetNull();
        return false;
    }
    return true;
}

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout)
{
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool CTransaction::ReadFromDisk(COutPoint prevout)
{
    CTxDB txdb("r");
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool IsStandardTx(const CTransaction& tx)
{
    if (tx.nVersion > CTransaction::CURRENT_VERSION)
        return false;

    // Treat non-final transactions as non-standard to prevent a specific type
    // of double-spend attack, as well as DoS attacks. (if the transaction
    // can't be mined, the attacker isn't expending resources broadcasting it)
    // Basically we don't want to propagate transactions that can't included in
    // the next block.
    //
    // However, IsFinalTx() is confusing... Without arguments, it uses
    // chainActive.Height() to evaluate nLockTime; when a block is accepted, chainActive.Height()
    // is set to the value of nHeight in the block. However, when IsFinalTx()
    // is called within CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a transaction can
    // be part of the *next* block, we need to call IsFinalTx() with one more
    // than chainActive.Height().
    //
    // Timestamps on the other hand don't get any special treatment, because we
    // can't know what timestamp the next block will have, and there aren't
    // timestamp applications where it matters.
    if (!IsFinalTx(tx, nBestHeight + 1)) {
        return false;
    }
    // nTime has different purpose from nLockTime but can be used in similar attacks
    if (tx.nTime > FutureDrift(GetAdjustedTime(), nBestHeight + 1)) {
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE)
        return false;

    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        // Biggest 'standard' txin is a 3-signature 3-of-3 CHECKMULTISIG
        // pay-to-script-hash, which is 3 ~80-byte signatures, 3
        // ~65-byte public keys, plus a few script ops.
        if (txin.scriptSig.size() > 500)
            return false;
        if (!txin.scriptSig.IsPushOnly())
            return false;
        if (fEnforceCanonical && !txin.scriptSig.HasCanonicalPushes()) {
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut& txout, tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType))
            return false;
        if (whichType == TX_NULL_DATA)
            nDataOut++;
        if (txout.nValue == 0)
            return false;
        if (fEnforceCanonical && !txout.scriptPubKey.HasCanonicalPushes()) {
            return false;
        }
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
        return false;
    }

    return true;
}

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = nBestHeight;
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

//
// Check transaction inputs, and make sure any
// pay-to-script-hash transactions are evaluating IsStandard scripts
//
// Why bother? To avoid denial-of-service attacks; an attacker
// can submit a standard HASH... OP_EQUAL transaction,
// which will get accepted into blocks. The redemption
// script can be anything; an attacker could use a very
// expensive-to-check-upon-redemption script like:
//   DUP CHECKSIG DROP ... repeated 100 times... OP_1
//
bool CTransaction::AreInputsStandard(const MapPrevTx& mapInputs) const
{
    if (IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < vin.size(); i++)
    {
        const CTxOut& prev = GetOutputFor(vin[i], mapInputs);

        vector<vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig the
        // IsStandard() call returns false
        vector<vector<unsigned char> > stack;
        if (!EvalScript(stack, vin[i].scriptSig, *this, i, 0))
            return false;

        if (whichType == TX_SCRIPTHASH)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (!Solver(subscript, whichType2, vSolutions2))
                return false;
            if (whichType2 == TX_SCRIPTHASH)
                return false;

            int tmpExpected;
            tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
            if (tmpExpected < 0)
                return false;
            nArgsExpected += tmpExpected;
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int
CTransaction::GetLegacySigOpCount() const
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}


int CMerkleTx::SetMerkleBranch(const CBlock* pblock)
{
    AssertLockHeld(cs_main);

    CBlock blockTmp;
    if (pblock == NULL)
    {
        // Load the block this tx is in
        CTxIndex txindex;
        if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
            return 0;
        if (!blockTmp.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos))
            return 0;
        pblock = &blockTmp;
    }

    // Update the tx's hashBlock
    hashBlock = pblock->GetHash();

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)pblock->vtx.size(); nIndex++)
        if (pblock->vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)pblock->vtx.size())
    {
        vMerkleBranch.clear();
        nIndex = -1;
        printf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
        return 0;
    }

    // Fill in merkle branch
    vMerkleBranch = pblock->GetMerkleBranch(nIndex);

    // Is the tx in a block that's in the main chain
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    return pindexBest->nHeight - pindex->nHeight + 1;
}







bool CTransaction::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vin empty"));
    if (vout.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vout empty"));
    // Size limits
    if (::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return DoS(100, error("CTransaction::CheckTransaction() : size limits failed"));

    // Check for negative or overflow output values
    int64_t nValueOut = 0;
    for (unsigned int i = 0; i < vout.size(); i++)
    {
        const CTxOut& txout = vout[i];
        if (txout.IsEmpty() && !IsCoinBase() && !IsCoinStake())
            return DoS(100, error("CTransaction::CheckTransaction() : txout empty for user transaction"));
        if (txout.nValue < 0)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue negative"));
        if (txout.nValue > MAX_MONEY)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue too high"));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return DoS(100, error("CTransaction::CheckTransaction() : txout total out of range"));
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase())
    {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return DoS(100, error("CTransaction::CheckTransaction() : coinbase script size is invalid"));
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
            if (txin.prevout.IsNull())
                return DoS(10, error("CTransaction::CheckTransaction() : prevout is null"));
    }

    return true;
}

int64_t CTransaction::GetMinFee(unsigned int nBlockSize, enum GetMinFee_mode mode, unsigned int nBytes) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

    unsigned int nNewBlockSize = nBlockSize + nBytes;
    int64_t nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee)
    {
        BOOST_FOREACH(const CTxOut& txout, vout)
            if (txout.nValue < CENT)
                nMinFee = nBaseFee;
    }

    // Raise the price as the block approaches full
    if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2)
    {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return MAX_MONEY;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}


bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx,
                        bool* pfMissingInputs)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!tx.CheckTransaction())
        return error("AcceptToMemoryPool : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return tx.DoS(100, error("AcceptToMemoryPool : coinbase as individual tx"));

    // ppcoin: coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return tx.DoS(100, error("AcceptToMemoryPool : coinstake as individual tx"));

    // Rather not work on nonstandard transactions (unless -testnet)
    if (!fTestNet && !IsStandardTx(tx))
        return error("AcceptToMemoryPool : nonstandard transaction type");

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = NULL;
    {
    LOCK(pool.cs); // protect pool.mapNextTx
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint outpoint = tx.vin[i].prevout;
        if (pool.mapNextTx.count(outpoint))
        {
            // Disable replacement feature for now
            return false;

            // Allow replacing with a newer version of the same transaction
            if (i != 0)
                return false;
            ptxOld = pool.mapNextTx[outpoint].ptx;
            if (IsFinalTx(*ptxOld))
                return false;
            if (!tx.IsNewerThan(*ptxOld))
                return false;
            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                COutPoint outpoint = tx.vin[i].prevout;
                if (!pool.mapNextTx.count(outpoint) || pool.mapNextTx[outpoint].ptx != ptxOld)
                    return false;
            }
            break;
        }
    }
    }

    {
        CTxDB txdb("r");

        // do we already have it?
        if (txdb.ContainsTx(hash))
            return false;

        MapPrevTx mapInputs;
        map<uint256, CTxIndex> mapUnused;
        bool fInvalid = false;
        if (!tx.FetchInputs(txdb, mapUnused, false, false, mapInputs, fInvalid))
        {
            if (fInvalid)
                return error("AcceptToMemoryPool : FetchInputs found invalid tx %s", hash.ToString().substr(0,10).c_str());
            if (pfMissingInputs)
                *pfMissingInputs = true;
            return false;
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (!tx.AreInputsStandard(mapInputs) && !fTestNet)
            return error("AcceptToMemoryPool : nonstandard transaction input");

        // Note: if you modify this code to accept non-standard transactions, then
        // you should add code here to check that the transaction does a
        // reasonable number of ECDSA signature verifications.

        int64_t nFees = tx.GetValueIn(mapInputs)-tx.GetValueOut();
        unsigned int nSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

        // Don't accept it if it can't get into a block
        int64_t txMinFee = tx.GetMinFee(1000, GMF_RELAY, nSize);
        if (nFees < txMinFee)
            return error("AcceptToMemoryPool : not enough fees %s, %"PRId64" < %"PRId64,
                         hash.ToString().c_str(),
                         nFees, txMinFee);

        // Continuously rate-limit free transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (nFees < MIN_RELAY_TX_FEE)
        {
            static CCriticalSection cs;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow =  GetAdjustedTime();

            {
                LOCK(pool.cs);
                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount > GetArg("-limitfreerelay", 15)*10*1000 && !IsFromMe(tx))
                    return error("AcceptToMemoryPool : free transaction rejected by rate limiter");
                if (fDebug)
                    printf("Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
                dFreeCount += nSize;
            }
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!tx.ConnectInputs(txdb, mapInputs, mapUnused, CDiskTxPos(1,1,1), pindexBest, false, false))
        {
			if (fDebug) printf("AcceptToMemoryPool : ConnectInputs failed %s", hash.ToString().c_str());
			return false;

        }
    }

    // Store transaction in memory
    {
        LOCK(pool.cs);
        if (ptxOld)
        {
            printf("AcceptToMemoryPool : replacing tx %s with new version\n", ptxOld->GetHash().ToString().c_str());
            pool.remove(*ptxOld);
        }
        pool.addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());
	if (fDebug)     printf("AcceptToMemoryPool : accepted %s (poolsz %"PRIszu")\n",           hash.ToString().c_str(),           pool.mapTx.size());
    return true;
}

bool CTxMemPool::addUnchecked(const uint256& hash, CTransaction &tx)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptToMemoryPool to properly check the transaction first.
    {
        mapTx[hash] = tx;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
            mapNextTx[tx.vin[i].prevout] = CInPoint(&mapTx[hash], i);
        nTransactionsUpdated++;
    }
    return true;
}


bool CTxMemPool::remove(const CTransaction &tx, bool fRecursive)
{
    // Remove transaction from memory pool
    {
        LOCK(cs);
        uint256 hash = tx.GetHash();
        if (mapTx.count(hash))
        {
            if (fRecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                        remove(*it->second.ptx, true);
                }
            }
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
            nTransactionsUpdated++;
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction &tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    BOOST_FOREACH(const CTxIn &txin, tx.vin) {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction &txConflict = *it->second.ptx;
            if (txConflict != tx)
                remove(txConflict, true);
        }
    }
    return true;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
    ++nTransactionsUpdated;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (map<uint256, CTransaction>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back((*mi).first);
}




int CMerkleTx::GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;
    AssertLockHeld(cs_main);

    // Find the block it claims to be in
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    // Make sure the merkle branch connects to this block
    if (!fMerkleVerified)
    {
        if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
            return 0;
        fMerkleVerified = true;
    }

    pindexRet = pindex;
    return pindexBest->nHeight - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(CBlockIndex* &pindexRet) const
{
    AssertLockHeld(cs_main);
    int nResult = GetDepthInMainChainINTERNAL(pindexRet);
    if (nResult == 0 && !mempool.exists(GetHash()))
        return -1; // Not in chain, not in mempool

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    return max(0, (nCoinbaseMaturity+10) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool()
{
    return ::AcceptToMemoryPool(mempool, *this, NULL);
}



bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb)
{

    {
        // Add previous supporting transactions first
        BOOST_FOREACH(CMerkleTx& tx, vtxPrev)
        {
            if (!(tx.IsCoinBase() || tx.IsCoinStake()))
            {
                uint256 hash = tx.GetHash();
                if (!mempool.exists(hash) && !txdb.ContainsTx(hash))
                    tx.AcceptToMemoryPool();
            }
        }
        return AcceptToMemoryPool();
    }
    return false;
}

bool CWalletTx::AcceptWalletTransaction()
{
    CTxDB txdb("r");
    return AcceptWalletTransaction(txdb);
}

int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nFile, pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}

// Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock
bool GetTransaction(const uint256 &hash, CTransaction &tx, uint256 &hashBlock)
{
    {
        LOCK(cs_main);
        {
            if (mempool.lookup(hash, tx))
            {
                return true;
            }
        }
        CTxDB txdb("r");
        CTxIndex txindex;
        if (tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex))
        {
            CBlock block;
            if (block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                hashBlock = block.GetHash();
            return true;
        }
    }
    return false;
}








//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

static CBlockIndex* pblockindexFBBHLast;
CBlockIndex* FindBlockByHeight(int nHeight)
{
    CBlockIndex *pblockindex;
    if (nHeight < nBestHeight / 2)
        pblockindex = pindexGenesisBlock;
    else
        pblockindex = pindexBest;
    if (pblockindexFBBHLast && abs(nHeight - pblockindex->nHeight) > abs(nHeight - pblockindexFBBHLast->nHeight))
        pblockindex = pblockindexFBBHLast;
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;
    while (pblockindex->nHeight < nHeight)
        pblockindex = pblockindex->pnext;
    pblockindexFBBHLast = pblockindex;
    return pblockindex;
}


CBlockIndex* RPCFindBlockByHeight(int nHeight)
{
    CBlockIndex *RPCpblockindex;
    if (nHeight < nBestHeight / 2)
        RPCpblockindex = pindexGenesisBlock;
    else
        RPCpblockindex = pindexBest;
    while (RPCpblockindex->nHeight > nHeight)
	{
        RPCpblockindex = RPCpblockindex->pprev;
	}
    while (RPCpblockindex->nHeight < nHeight)
	{
        RPCpblockindex = RPCpblockindex->pnext;
	}
    return RPCpblockindex;
}

CBlockIndex* MainFindBlockByHeight(int nHeight)
{
    CBlockIndex *Mainpblockindex;
    if (nHeight < nBestHeight / 2)
        Mainpblockindex = pindexGenesisBlock;
    else
        Mainpblockindex = pindexBest;
    while (Mainpblockindex->nHeight > nHeight)
	{
        Mainpblockindex = Mainpblockindex->pprev;
	}
    while (Mainpblockindex->nHeight < nHeight)
	{
        Mainpblockindex = Mainpblockindex->pnext;
	}
    return Mainpblockindex;
}



bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
    if (!fReadTransactions)
    {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->nFile, pindex->nBlockPos, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

uint256 static GetOrphanRoot(const CBlock* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock->GetHash();
}

// ppcoin: find block wanted by given orphan block
uint256 WantedByOrphan(const CBlock* pblockOrphan)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblockOrphan->hashPrevBlock))
        pblockOrphan = mapOrphanBlocks[pblockOrphan->hashPrevBlock];
    return pblockOrphan->hashPrevBlock;
}

static CBigNum GetProofOfStakeLimit(int nHeight)
{
    if (IsProtocolV2(nHeight))
        return bnProofOfStakeLimitV2;
    else
        return bnProofOfStakeLimit;
}


double CalculatedMagnitude(int64_t locktime)
{
	
	StructCPID mag = GetStructCPID();
	mag = mvCreditNodeCPID[GlobalCPUMiningCPID.cpid];
	if (GlobalCPUMiningCPID.cpid != "INVESTOR" && mag.ConsensusMagnitude==0) 
	{
		CreditCheck(GlobalCPUMiningCPID.cpid,false);
		mag = mvCreditNodeCPID[GlobalCPUMiningCPID.cpid];
	}
	double calcmag = mag.Magnitude;
	if (calcmag < 0) calcmag=0;
	if (calcmag > GetMaximumBoincSubsidy(locktime)) calcmag=GetMaximumBoincSubsidy(locktime);
	return calcmag;
}

// miner's coin base reward
int64_t GetProofOfWorkReward(int64_t nFees, int64_t locktime, int64_t height)
{
    int64_t nSubsidy = CalculatedMagnitude(locktime) * COIN;
    if (fDebug && GetBoolArg("-printcreation"))
        printf("GetProofOfWorkReward() : create=%s nSubsidy=%"PRId64"\n", FormatMoney(nSubsidy).c_str(), nSubsidy);
	if (nSubsidy < (30*COIN)) nSubsidy=30*COIN;
	//Gridcoin Foundation Block:
	if (height==10)
	{
		nSubsidy =340569880 * COIN;
	}
    return nSubsidy + nFees;
}


int64_t GetProofOfWorkMaxReward(int64_t nFees, int64_t locktime, int64_t height)
{
	int64_t nSubsidy = (GetMaximumBoincSubsidy(locktime)+1) * COIN;
	if (height==10) 
	{
		//R.Halford: 10-11-2014: Gridcoin Foundation Block:
		nSubsidy = 340569880 * COIN;
	}
    return nSubsidy + nFees;
}

//Survey Results: Start inflation rate: 9%, end=1%, 30 day steps, 9 steps, mag multiplier start: 2, mag end .3, 9 steps


int64_t GetMaximumBoincSubsidy(int64_t nTime)
{
	// Gridcoin Global Daily Maximum Researcher Subsidy Schedule
	int MaxSubsidy = 500;
    if (nTime >= 1410393600 && nTime <= 1417305600) MaxSubsidy = 	500; // between inception  and 11-30-2014
	if (nTime >= 1417305600 && nTime <= 1419897600) MaxSubsidy = 	400; // between 11-30-2014 and 12-30-2014
	if (nTime >= 1419897600 && nTime <= 1422576000) MaxSubsidy = 	400; // between 12-30-2014 and 01-30-2015
	if (nTime >= 1422576000 && nTime <= 1425254400) MaxSubsidy = 	300; // between 01-30-2015 and 02-28-2015
	if (nTime >= 1425254400 && nTime <= 1427673600) MaxSubsidy = 	250; // between 02-28-2015 and 03-30-2015
	if (nTime >= 1427673600 && nTime <= 1430352000) MaxSubsidy = 	200; // between 03-30-2015 and 04-30-2015
	if (nTime >= 1430352000 && nTime <= 1438310876) MaxSubsidy = 	150; // between 05-01-2015 and 07-31-2015
	if (nTime >= 1438310876 && nTime <= 1445309276) MaxSubsidy = 	100; // between 08-01-2015 and 10-20-2015
	if (nTime >= 1445309276 && nTime <= 1447977700) MaxSubsidy = 	 75; // between 10-20-2015 and 11-20-2015
	if (nTime > 1447977700)                         MaxSubsidy =   	 50; // from  11-20-2015 forever
    return MaxSubsidy+.5;  //The .5 allows for fractional amounts after the 4th decimal place (used to store the POR indicator)
}

int64_t GetCoinYearReward(int64_t nTime)
{
	// Gridcoin Global Interest Rate Schedule
	int64_t INTEREST = 9;
   	if (nTime >= 1410393600 && nTime <= 1417305600) INTEREST = 	 9 * CENT; // 09% between inception  and 11-30-2014
	if (nTime >= 1417305600 && nTime <= 1419897600) INTEREST = 	 8 * CENT; // 08% between 11-30-2014 and 12-30-2014
	if (nTime >= 1419897600 && nTime <= 1422576000) INTEREST = 	 8 * CENT; // 08% between 12-30-2014 and 01-30-2015
	if (nTime >= 1422576000 && nTime <= 1425254400) INTEREST = 	 7 * CENT; // 07% between 01-30-2015 and 02-30-2015
	if (nTime >= 1425254400 && nTime <= 1427673600) INTEREST = 	 6 * CENT; // 06% between 02-30-2015 and 03-30-2015
	if (nTime >= 1427673600 && nTime <= 1430352000) INTEREST = 	 5 * CENT; // 05% between 03-30-2015 and 04-30-2015
	if (nTime >= 1430352000 && nTime <= 1438310876) INTEREST =   4 * CENT; // 04% between 05-01-2015 and 07-31-2015
	if (nTime >= 1438310876 && nTime <= 1447977700) INTEREST =   3 * CENT; // 03% between 08-01-2015 and 11-20-2015
	if (nTime > 1447977700)                         INTEREST = 1.5 * CENT; //1.5% from 11-21-2015 forever
	return INTEREST;
}

double GetMagnitudeMultiplier(int64_t nTime)
{
	// Gridcoin Global Resarch Subsidy Multiplier Schedule
	double magnitude_multiplier = 2; 
	if (nTime >= 1410393600 && nTime <= 1417305600) magnitude_multiplier =    2;  // between inception and 11-30-2014
	if (nTime >= 1417305600 && nTime <= 1419897600) magnitude_multiplier =  1.5;  // between 11-30-2014 and 12-30-2014
	if (nTime >= 1419897600 && nTime <= 1422576000) magnitude_multiplier =  1.5;  // between 12-30-2014 and 01-30-2015
	if (nTime >= 1422576000 && nTime <= 1425254400) magnitude_multiplier =    1;  // between 01-30-2015 and 02-30-2015
	if (nTime >= 1425254400 && nTime <= 1427673600) magnitude_multiplier =   .9;  // between 02-30-2015 and 03-30-2015
	if (nTime >= 1427673600 && nTime <= 1430352000) magnitude_multiplier =   .8;  // between 03-30-2015 and 04-30-2015
    if (nTime >= 1430352000 && nTime <= 1438310876) magnitude_multiplier =   .7;  // between 05-01-2015 and 07-31-2015
	if (nTime >= 1438310876 && nTime <= 1447977700) magnitude_multiplier =  .60;  // between 08-01-2015 and 11-20-2015
	if (nTime > 1447977700)                         magnitude_multiplier =  .50;  // from 11-21-2015  forever
	return magnitude_multiplier;
}



int64_t GetProofOfStakeMaxReward(int64_t nCoinAge, int64_t nFees, int64_t locktime)
{
	int64_t nInterest = nCoinAge * GetCoinYearReward(locktime) * 33 / (365 * 33 + 8);
	nInterest += 1*COIN;
	int64_t nBoinc    = (GetMaximumBoincSubsidy(locktime)+1) * COIN;
	int64_t nSubsidy  = nInterest + nBoinc;
    return nSubsidy + nFees;
}

double GetProofOfResearchReward(std::string cpid, bool VerifyingBlock)
{
		StructCPID mag = GetStructCPID();
		mag = mvMagnitudes[cpid];
		if (!mag.initialized) return 0;

		//Help prevent sync problems by assessing owed @ 90%
		double owed = (mag.owed*1.0);
		if (owed < 0) owed = 0;
		// Coarse Payment Rule (helps prevent sync problems):
		if (!VerifyingBlock)
		{
			//If owed less than 15% of max subsidy, assess at 0:
			if (owed < (GetMaximumBoincSubsidy(GetAdjustedTime())/15)) 
			{
				if (fDebug3) printf("Owed < 15pc of max %f \r\n",(double)owed);

				owed = 0;
			}

			//Coarse payment rule:
			if (mag.totalowed > (GetMaximumBoincSubsidy(GetAdjustedTime())*2))
			{
				//If owed more than 2* Max Block, pay normal amount
	            owed = (owed*1);
			}
			else
			{
				owed = owed/2;
			}

			if (owed > (GetMaximumBoincSubsidy(GetAdjustedTime()))) owed = GetMaximumBoincSubsidy(GetAdjustedTime()); 

			if (!ChainPaymentViolation(cpid,GetAdjustedTime(),owed)) 
			{
					printf("Chain Payment Violation! cpid %s \r\n",cpid.c_str());
					owed = 0;

			}


			//Halford - Ensure researcher was not paid in the last 2 hours:
			if (IsLockTimeWithinMinutes(mag.LastPaymentTime,120))
			{
				if (fDebug3) printf("Last Payment Time too recent %f \r\n",(double)mag.LastPaymentTime);
				owed = 0;
			}


		}
		//End of Coarse Payment Rule

		return owed * COIN;	
}


// miner's coin stake reward based on coin age spent (coin-days)
int64_t GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees, std::string cpid, 
	bool VerifyingBlock, int64_t locktime, double& OUT_POR, double& OUT_INTEREST, double RSA_WEIGHT)
{
    
	int64_t nInterest = nCoinAge * GetCoinYearReward(locktime) * 33 / (365 * 33 + 8);
	if (RSA_WEIGHT > 24000) nInterest += 1*COIN;
	int64_t nBoinc    = GetProofOfResearchReward(cpid,VerifyingBlock);
	int64_t nSubsidy  = nInterest + nBoinc;
	
    if (fDebug || GetBoolArg("-printcreation"))
	{
        printf("GetProofOfStakeReward(): create=%s nCoinAge=%"PRId64" nBoinc=%"PRId64"   \n",
		FormatMoney(nSubsidy).c_str(), nCoinAge, nBoinc);
	}

	int64_t maxStakeReward1 = GetProofOfStakeMaxReward(nCoinAge, nFees, locktime);
	int64_t maxStakeReward2 = GetProofOfStakeMaxReward(nCoinAge, nFees, GetAdjustedTime());
	int64_t maxStakeReward = Floor(maxStakeReward1,maxStakeReward2);
	if ((nSubsidy+nFees) > maxStakeReward) nSubsidy = maxStakeReward-nFees;
	int64_t nTotalSubsidy = nSubsidy + nFees;
	
	if (nBoinc > 1)
	{
		std::string sTotalSubsidy = RoundToString(CoinToDouble(nTotalSubsidy)+.00000123,8);
		if (sTotalSubsidy.length() > 7)
		{
			sTotalSubsidy = sTotalSubsidy.substr(0,sTotalSubsidy.length()-4) + "0124";
			nTotalSubsidy = cdbl(sTotalSubsidy,8)*COIN;
			//if (fDebug3) printf("Total calculated subsidy %s Or %f or %f \r\n",sTotalSubsidy.c_str(),(double)nTotalSubsidy,(double)CoinToDouble(nTotalSubsidy));
		}
	}

	OUT_POR = CoinToDouble(nBoinc);
	OUT_INTEREST = CoinToDouble(nInterest);
    return nTotalSubsidy;
}



static const int64_t nTargetTimespan = 16 * 60;  // 16 mins

//
// maximum nBits value could possible be required nTime after
//
unsigned int ComputeMaxBits(CBigNum bnTargetLimit, unsigned int nBase, int64_t nTime)
{
    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    bnResult *= 2;
    while (nTime > 0 && bnResult < bnTargetLimit)
    {
        // Maximum 200% adjustment per day...
        bnResult *= 2;
        nTime -= 24 * 60 * 60;
    }
    if (bnResult > bnTargetLimit)
        bnResult = bnTargetLimit;
    return bnResult.GetCompact();
}

//
// minimum amount of work that could possibly be required nTime after
// minimum proof-of-work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    return ComputeMaxBits(bnProofOfWorkLimit, nBase, nTime);
}

//
// minimum amount of stake that could possibly be required nTime after
// minimum proof-of-stake required was nBase
//
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int nBlockTime)
{
    return ComputeMaxBits(bnProofOfStakeLimit, nBase, nTime);
}


// ppcoin: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = pindex->pprev;
    return pindex;
}


//Find prior block hash
uint256 GetBlockHash256(const CBlockIndex* pindex_hash)
{
	if (pindex_hash == NULL) return 0;
	if (!pindex_hash) return 0;
	return pindex_hash->GetBlockHash();
}

static unsigned int GetNextTargetRequiredV1(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? bnProofOfStakeLimit : bnProofOfWorkLimit;

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, fProofOfStake);
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nTargetSpacing = GetTargetSpacing(pindexLast->nHeight);
    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
	

    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    if (bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

static unsigned int GetNextTargetRequiredV2(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? GetProofOfStakeLimit(pindexLast->nHeight) : bnProofOfWorkLimit;

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, fProofOfStake);
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nTargetSpacing = GetTargetSpacing(pindexLast->nHeight);
    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    if (nActualSpacing < 0)
        nActualSpacing = nTargetSpacing;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->nBits);

	//Gridcoin - Reset Diff to 1 on 12-21-2014 (R Halford) - Diff sticking at 2065 due to many incompatible features
	if (pindexLast->nHeight >= 91387 && pindexLast->nHeight <= 91500)
	{
		    return bnTargetLimit.GetCompact(); 
	}

	//1-14-2015 R Halford - Make diff reset to zero after periods of exploding diff:
	double PORDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
	if (PORDiff > 900000)
	{
		    return bnTargetLimit.GetCompact(); 
	}
		

	//nTargetTimespan = 16 * 60;  // 16 mins  (TargetSpacing = 64);  nInterval = 15 min

    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
	{
		printf("@v2.2");
        bnNew = bnTargetLimit;
	}

    return bnNew.GetCompact();
}

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake)
{
	//After block 89600, new diff algorithm is used
    if (pindexLast->nHeight < 89600)
        return GetNextTargetRequiredV1(pindexLast, fProofOfStake);
    else
        return GetNextTargetRequiredV2(pindexLast, fProofOfStake);
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > bnProofOfWorkLimit)
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    return std::max(cPeerBlockCounts.median(), Checkpoints::GetTotalBlocksEstimate());
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    if (pindexBest == NULL || nBestHeight < GetNumBlocksOfPeers())
        return true;
    static int64_t nLastUpdate;
    static CBlockIndex* pindexLastBest;
    if (pindexBest != pindexLastBest)
    {
        pindexLastBest = pindexBest;
        nLastUpdate =  GetAdjustedTime();
    }
    return ( GetAdjustedTime() - nLastUpdate < 15 &&
            pindexBest->GetBlockTime() <  GetAdjustedTime() - 8 * 60 * 60);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (pindexNew->nChainTrust > nBestInvalidTrust)
    {
        nBestInvalidTrust = pindexNew->nChainTrust;
        CTxDB().WriteBestInvalidTrust(CBigNum(nBestInvalidTrust));
        uiInterface.NotifyBlocksChanged();
    }

    uint256 nBestInvalidBlockTrust = pindexNew->nChainTrust - pindexNew->pprev->nChainTrust;
    uint256 nBestBlockTrust = pindexBest->nHeight != 0 ? (pindexBest->nChainTrust - pindexBest->pprev->nChainTrust) : pindexBest->nChainTrust;

    printf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  blocktrust=%"PRId64"  date=%s\n",
      pindexNew->GetBlockHash().ToString().substr(0,20).c_str(), pindexNew->nHeight,
      CBigNum(pindexNew->nChainTrust).ToString().c_str(), nBestInvalidBlockTrust.Get64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexNew->GetBlockTime()).c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  trust=%s  blocktrust=%"PRId64"  date=%s\n",
      hashBestChain.ToString().substr(0,20).c_str(), nBestHeight,
      CBigNum(pindexBest->nChainTrust).ToString().c_str(),
      nBestBlockTrust.Get64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());
}


void CBlock::UpdateTime(const CBlockIndex* pindexPrev)
{
    nTime = max(GetBlockTime(), GetAdjustedTime());
}







bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!IsCoinBase())
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
        {
            COutPoint prevout = txin.prevout;

            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            if (!txdb.UpdateTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : UpdateTxIndex failed");
        }
    }

    // Remove transaction from index
    // This can fail if a duplicate of this transaction was in a chain that got
    // reorganized away. This is only possible if this transaction was completely
    // spent, so erasing it would be a no-op anyway.
    txdb.EraseTxIndex(*this);

    return true;
}


bool CTransaction::FetchInputs(CTxDB& txdb, const map<uint256, CTxIndex>& mapTestPool,
                               bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
    // FetchInputs can return false either because we just haven't seen some inputs
    // (in which case the transaction should be stored as an orphan)
    // or because the transaction is malformed (in which case the transaction should
    // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
    fInvalid = false;

    if (IsCoinBase())
        return true; // Coinbase transactions have no inputs to fetch.

    for (unsigned int i = 0; i < vin.size(); i++)
    {
        COutPoint prevout = vin[i].prevout;
        if (inputsRet.count(prevout.hash))
            continue; // Got it already

        // Read txindex
        CTxIndex& txindex = inputsRet[prevout.hash].first;
        bool fFound = true;
        if ((fBlock || fMiner) && mapTestPool.count(prevout.hash))
        {
            // Get txindex from current proposed changes
            txindex = mapTestPool.find(prevout.hash)->second;
        }
        else
        {
            // Read txindex from txdb
            fFound = txdb.ReadTxIndex(prevout.hash, txindex);
        }
        if (!fFound && (fBlock || fMiner))
            return fMiner ? false : error("FetchInputs() : %s prev tx %s index entry not found", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());

        // Read txPrev
        CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (!fFound || txindex.pos == CDiskTxPos(1,1,1))
        {
            // Get prev tx from single transactions in memory
            if (!mempool.lookup(prevout.hash, txPrev))
                return error("FetchInputs() : %s mempool Tx prev not found %s", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());
            if (!fFound)
                txindex.vSpent.resize(txPrev.vout.size());
        }
        else
        {
            // Get prev tx from disk
            if (!txPrev.ReadFromDisk(txindex.pos))
                return error("FetchInputs() : %s ReadFromDisk prev tx %s failed", GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());
        }
    }

    // Make sure all prevout.n indexes are valid:
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        const COutPoint prevout = vin[i].prevout;
        assert(inputsRet.count(prevout.hash) != 0);
        const CTxIndex& txindex = inputsRet[prevout.hash].first;
        const CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
        {
            // Revisit this if/when transaction replacement is implemented and allows
            // adding inputs:
            fInvalid = true;
            return DoS(100, error("FetchInputs() : %s prevout.n out of range %d %"PRIszu" %"PRIszu" prev tx %s\n%s", GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));
        }
    }

    return true;
}

const CTxOut& CTransaction::GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const
{
    MapPrevTx::const_iterator mi = inputs.find(input.prevout.hash);
    if (mi == inputs.end())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.hash not found");

    const CTransaction& txPrev = (mi->second).second;
    if (input.prevout.n >= txPrev.vout.size())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.n out of range");

    return txPrev.vout[input.prevout.n];
}





void WriteStringToFile(const boost::filesystem::path &path, std::string sOut)
{
    FILE* file = fopen(path.string().c_str(), "w");
    if (file)
    {
        fprintf(file, "%s\r\n", sOut.c_str());
        fclose(file);
    }
}




std::vector<std::string> &split_bychar(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split_bychar(const std::string &s, char delim) 
{
    std::vector<std::string> elems;
    split_bychar(s, delim, elems);
    return elems;
}


std::vector<std::string> split(std::string s, std::string delim)
{
	size_t pos = 0;
	std::string token;
	std::vector<std::string> elems;
    
	while ((pos = s.find(delim)) != std::string::npos) 
	{
		token = s.substr(0, pos);
		elems.push_back(token);
		s.erase(0, pos + delim.length());
	}
	elems.push_back(s);
	return elems;

}





int64_t CTransaction::GetValueIn(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    int64_t nResult = 0;
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        nResult += GetOutputFor(vin[i], inputs).nValue;
    }
    return nResult;

}



double PreviousBlockAge()
{
		if (nBestHeight < 30) return 99999;
		double nTime = max(pindexBest->GetMedianTimePast()+1, GetAdjustedTime());
		double nActualTimespan = nTime - pindexBest->pprev->GetBlockTime();
		return nActualTimespan;
}






bool ClientOutOfSync() 
{
	double lastblockage = PreviousBlockAge();
	if (lastblockage > (30*60)) return true;
	if (fReindex || fImporting ) return true;
	if (pindexBest == NULL || nBestHeight < GetNumBlocksOfPeers()-30) return true;
	return false;
}




bool OutOfSyncByAge() 
{
	double lastblockage = PreviousBlockAge();
	if (lastblockage > (60*30)) return true;
	if (fReindex || fImporting ) return true;
	return false;
}


bool LessVerbose(int iMax1000)
{
	 //Returns True when RND() level is lower than the number presented
	 int iVerbosityLevel = rand() % 1000; 
	 if (iVerbosityLevel < iMax1000) return true;
	 return false;
}


bool KeyEnabled(std::string key)
{
	if (mapArgs.count("-" + key))
	{
			std::string sBool = GetArg("-" + key, "false");
			if (sBool == "true") return true;
	}
	return false;
		
}


bool OutOfSyncByAgeWithChanceOfMining()
{
	try
	{
		    //R Halford - Refactor - Reported by Pallas
		    if (KeyEnabled("overrideoutofsyncrule")) return false;
			bool oosbyage = OutOfSyncByAge();
			//Rule 1: If  Last Block Out of sync by Age - Return Out of Sync 95% of the time:
			if (oosbyage) if (LessVerbose(900)) return true;
			// Rule 2 : Dont mine on Fork Rule:
	     	//If the diff is < .00015 in Prod, Most likely the client is mining on a fork: (Make it exceedingly hard):
			double PORDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
			if (!fTestNet && PORDiff < .00010)
			{
				printf("Most likely you are mining on a fork! Diff %f",PORDiff);
				if (LessVerbose(950)) return true;
			}
			return false;
	}
	catch (std::exception &e) 
	{
				printf("Error while assessing Sync Condition\r\n");
				return true;
	}
	catch(...)
	{
				printf("Error while assessing Sync Condition[2].\r\n");
				return true;
	}
	return true;


}


int Races(int iMax1000)
{
	 int i = rand() % iMax1000;
	 if (i < 1) i = 1;
	 return i;
}




unsigned int CTransaction::GetP2SHSigOpCount(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        const CTxOut& prevout = GetOutputFor(vin[i], inputs);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(vin[i].scriptSig);
    }
    return nSigOps;
}

bool CTransaction::ConnectInputs(CTxDB& txdb, MapPrevTx inputs, map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx,
    const CBlockIndex* pindexBlock, bool fBlock, bool fMiner)
{
    // Take over previous transactions' spent pointers
    // fBlock is true when this is called from AcceptBlock when a new best-block is added to the blockchain
    // fMiner is true when called from the internal bitcoin miner
    // ... both are false when called from CTransaction::AcceptToMemoryPool
    if (!IsCoinBase())
    {
        int64_t nValueIn = 0;
        int64_t nFees = 0;
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex& txindex = inputs[prevout.hash].first;
            CTransaction& txPrev = inputs[prevout.hash].second;

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
                return DoS(100, error("ConnectInputs() : %s prevout.n out of range %d %"PRIszu" %"PRIszu" prev tx %s\n%s", GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));

            // If prev is coinbase or coinstake, check that it's matured
            if (txPrev.IsCoinBase() || txPrev.IsCoinStake())
                for (const CBlockIndex* pindex = pindexBlock; pindex && pindexBlock->nHeight - pindex->nHeight < nCoinbaseMaturity; pindex = pindex->pprev)
                    if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
                        return error("ConnectInputs() : tried to spend %s at depth %d", txPrev.IsCoinBase() ? "coinbase" : "coinstake", pindexBlock->nHeight - pindex->nHeight);

            // ppcoin: check transaction timestamp
            if (txPrev.nTime > nTime)
                return DoS(100, error("ConnectInputs() : transaction timestamp earlier than input transaction"));

            // Check for negative or overflow input values
            nValueIn += txPrev.vout[prevout.n].nValue;
            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return DoS(100, error("ConnectInputs() : txin values out of range"));

        }
        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex& txindex = inputs[prevout.hash].first;
            CTransaction& txPrev = inputs[prevout.hash].second;

            // Check for conflicts (double-spend)
            // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
            // for an attacker to attempt to split the network.
            if (!txindex.vSpent[prevout.n].IsNull())
			{
				if (fMiner) return false;
				//This could be due to mismatched coins in wallet: 12-28-2014
				int nMismatchSpent;
				int64_t nBalanceInQuestion;
				pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion);
    
				//return error("ConnectInputs() : %s prev tx already used at %s", GetHash().ToString().substr(0,10).c_str(), txindex.vSpent[prevout.n].ToString().c_str());

			}

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.
			
			//if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate())))
            
            if (!(fBlock && (nBestHeight < GetNumBlocksOfPeers() ) ))
            {
                // Verify signature
                if (!VerifySignature(txPrev, *this, i, 0))
                {
                    return DoS(100,error("ConnectInputs() : %s VerifySignature failed", GetHash().ToString().substr(0,10).c_str()));
                }
            }

            // Mark outpoints as spent
            txindex.vSpent[prevout.n] = posThisTx;

            // Write back
            if (fBlock || fMiner)
            {
                mapTestPool[prevout.hash] = txindex;
            }
        }

        if (!IsCoinStake())
        {
            if (nValueIn < GetValueOut())
                return DoS(100, error("ConnectInputs() : %s value in < value out", GetHash().ToString().substr(0,10).c_str()));

            // Tally transaction fees
            int64_t nTxFee = nValueIn - GetValueOut();
            if (nTxFee < 0)
                return DoS(100, error("ConnectInputs() : %s nTxFee < 0", GetHash().ToString().substr(0,10).c_str()));

            // enforce transaction fees for every block
            if (nTxFee < GetMinFee())
                return fBlock? DoS(100, error("ConnectInputs() : %s not paying required fee=%s, paid=%s", GetHash().ToString().substr(0,10).c_str(), FormatMoney(GetMinFee()).c_str(), FormatMoney(nTxFee).c_str())) : false;

            nFees += nTxFee;
            if (!MoneyRange(nFees))
                return DoS(100, error("ConnectInputs() : nFees out of range"));
        }
    }

    return true;
}

bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    
	//Gridcoin - 1-9-2015 - Halford - Remove Weighted Magnitude and payments from Global Magnitude vectors
	//if (vtx.size() > 0) hb = vtx[0].hashBoinc;
	//MiningCPID bb = DeserializeBoincBlock(hashboinc);
    //double mint = CoinToDouble(pindex->nMint);
    //RemoveNetworkMagnitude(nTime,bb.cpid,bb,mint,true);

	// Disconnect in reverse order
    for (int i = vtx.size()-1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

	

    // ppcoin: clean up wallet after disconnecting coinstake
    BOOST_FOREACH(CTransaction& tx, vtx)
        SyncWithWallets(tx, this, false, false);

	//Retally Network Averages - Halford - 1-11-2015
	TallyNetworkAverages(true);
		

    return true;
}



double BlockVersion(std::string v)
{
	if (v.length() < 10) return 0;
	std::string vIn = v.substr(1,7);
	boost::replace_all(vIn, ".", "");
	double ver1 = cdbl(vIn,0);
	return ver1;
}



double ClientVersionNew()
{
	double cv = BlockVersion(FormatFullVersion());
	return cv;
}

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(pindex->pprev->nHeight, 395*COIN, !fJustCheck, !fJustCheck, false,false))
	{
        printf("ConnectBlock::Failed - \r\n");
		return false;
	}

    //// issue here: it doesn't know the version
    unsigned int nTxPos;
    if (fJustCheck)
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    else
        nTxPos = pindex->nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION) - (2 * GetSizeOfCompactSize(0)) + GetSizeOfCompactSize(vtx.size());

    map<uint256, CTxIndex> mapQueuedChanges;
    int64_t nFees = 0;
    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    int64_t nStakeReward = 0;
    unsigned int nSigOps = 0;
    BOOST_FOREACH(CTransaction& tx, vtx)
    {
        uint256 hashTx = tx.GetHash();

        // Do not allow blocks that contain transactions which 'overwrite' older transactions,
        // unless those are already completely spent.
        // If such overwrites are allowed, coinbases and transactions depending upon those
        // can be duplicated to remove the ability to spend the first instance -- even after
        // being sent to another address.
        // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
        // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
        // already refuses previously-known transaction ids entirely.
        // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
        // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
        // two in the chain that violate it. This prevents exploiting the issue against nodes in their
        // initial block download.
        CTxIndex txindexOld;
        if (txdb.ReadTxIndex(hashTx, txindexOld)) {
            BOOST_FOREACH(CDiskTxPos &pos, txindexOld.vSpent)
                if (pos.IsNull())
                    return false;
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock() : too many sigops"));

        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
            nValueOut += tx.GetValueOut();
        else
        {
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
                return false;

            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nSigOps > MAX_BLOCK_SIGOPS)
                return DoS(100, error("ConnectBlock() : too many sigops"));

            int64_t nTxValueIn = tx.GetValueIn(mapInputs);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
                nStakeReward = nTxValueOut - nTxValueIn;

            if (!tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false))
                return false;
        }

        mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());
    }

    if (IsProofOfWork() && pindex->nHeight > nGrandfather)
    {
        int64_t nReward = GetProofOfWorkMaxReward(nFees,nTime,pindex->nHeight);
        // Check coinbase reward
        if (vtx[0].GetValueOut() > nReward)
            return DoS(50, error("ConnectBlock[] : coinbase reward exceeded (actual=%"PRId64" vs calculated=%"PRId64")",
                   vtx[0].GetValueOut(),
                   nReward));
    }

	MiningCPID bb = DeserializeBoincBlock(vtx[0].hashBoinc);

	uint64_t nCoinAge = 0;
        
    if (IsProofOfStake() && pindex->nHeight > nGrandfather)
    {
        // ppcoin: coin stake tx earns reward instead of paying fee
        if (!vtx[1].GetCoinAge(txdb, nCoinAge))
            return error("ConnectBlock[] : %s unable to get coin age for coinstake", vtx[1].GetHash().ToString().substr(0,10).c_str());

        int64_t nCalculatedStakeReward = GetProofOfStakeMaxReward(nCoinAge, nFees, nTime);

		if (nStakeReward > nCalculatedStakeReward)
            return DoS(1, error("ConnectBlock[] : coinstake pays above maximum (actual=%"PRId64" vs calculated=%"PRId64")", nStakeReward, nCalculatedStakeReward));
		
		if (!ClientOutOfSync() && bb.cpid=="INVESTOR" && nStakeReward > 1)
		{
			double OUT_POR = 0;
			double OUT_INTEREST = 0;
			int64_t nCalculatedResearchReward = GetProofOfStakeReward(nCoinAge, nFees, bb.cpid, true, nTime, OUT_POR, OUT_INTEREST,bb.RSAWeight);
			if (nStakeReward > nCalculatedResearchReward*TOLERANCE_PERCENT)
			{
				   
							return DoS(1, error("ConnectBlock[] : Investor Reward pays too much : cpid %s (actual=%"PRId64" vs calculated=%"PRId64")",
							bb.cpid.c_str(), nStakeReward, nCalculatedResearchReward));
			}
		}
    }

	

    // ppcoin: track money supply and mint amount info
    pindex->nMint = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = (pindex->pprev? pindex->pprev->nMoneySupply : 0) + nValueOut - nValueIn;

	//Gridcoin: Maintain network consensus for Magnitude & Outstanding Amount Owed by CPID  
		
	double mint = CoinToDouble(pindex->nMint);

	// Block Spamming (Halford) 12-23-2014
    double PORDiff = GetBlockDifficulty(nBits);
		
	if (pindex->nHeight > nGrandfather)
	{

		// Block Spamming (Halford) 12-23-2014
		if (mint < MintLimiter(PORDiff,bb.RSAWeight,bb.cpid,GetBlockTime())) 
		{
			return error("CheckProofOfStake[] : Mint too Small, %f",(double)mint);
		}

		
		if (mint == 0) return error("CheckProofOfStake[] : Mint is ZERO! %f",(double)mint);
	
		if (!ClientOutOfSync())
		{

			//12-26-2014 Halford - Orphan Flood Attack
			if (IsLockTimeWithinMinutes(GetBlockTime(),15))
			{
				double bv = BlockVersion(bb.clientversion);
				double cvn = ClientVersionNew();
				//if (fDebug) printf("BV %f, CV %f   ",bv,cvn);
				if (bv+10 < cvn) return error("ConnectBlock[]: Old client version after mandatory upgrade - block rejected\r\n");
			}

			//Block being accepted within the last hour: Check with Netsoft - AND Verify User will not be overpaid:
			bool outcome = CreditCheckOnline(bb.cpid,bb.Magnitude,mint,nCoinAge,nFees,nTime,bb.RSAWeight);
			if (!outcome) return DoS(1,error("ConnectBlock[]: Netsoft online check failed\r\n"));
			double OUT_POR = 0;
			double OUT_INTEREST = 0;

			int64_t nCalculatedResearch = GetProofOfStakeReward(nCoinAge, nFees, bb.cpid, true, nTime, OUT_POR, OUT_INTEREST, bb.RSAWeight);
			
			if (bb.cpid != "INVESTOR" && mint > 1)
			{
				if ((bb.ResearchSubsidy+OUT_INTEREST)*TOLERANCE_PERCENT < mint)
				{
						return error("ConnectBlock[] : Researchers Interest %f and Research %f and Mint %f for CPID %s does not match calculated research subsidy",
							(double)bb.InterestSubsidy,(double)bb.ResearchSubsidy,(double)mint,bb.cpid.c_str());
				
				}
			}

			//1-17-2015 R Halford - Use Hard Chain Payment Calculated Daily Average Paid to approve block
			if (IsLockTimeWithinMinutes(GetBlockTime(),30) && bb.cpid != "INVESTOR")
			{

				bool ChainPaymentApproved = ChainPaymentViolation(bb.cpid,GetBlockTime(),bb.ResearchSubsidy);
				if (!ChainPaymentApproved)
				{
						TallyNetworkAverages(false); //Re-Tally using cache
						ChainPaymentApproved = ChainPaymentViolation(bb.cpid,GetBlockTime(),bb.ResearchSubsidy);
				}
				if (!ChainPaymentApproved) 	
				{
						TallyNetworkAverages(true); //Erase Cache & Re-Tally 
						ChainPaymentApproved = ChainPaymentViolation(bb.cpid,GetBlockTime(),bb.ResearchSubsidy);
				}
				if (!ChainPaymentApproved) 
				{
											
						return DoS(20, error("ConnectBlock[] : Researchers Reward for CPID %s pays too much - (Submitted Research Subsidy %f vs calculated=%f) Hash: %s",
										bb.cpid.c_str(), (double)bb.ResearchSubsidy, 
										(double)OUT_POR, vtx[0].hashBoinc.c_str()));
		
				}
		
			}

		 }
	 }
	
	
	AddNetworkMagnitude(nTime, bb.cpid, bb, mint, IsProofOfStake()); //Updates Total payments and Actual Magnitude per CPID
	
	//  End of Network Consensus

    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("Connect() : WriteBlockIndex for pindex failed");

    if (fJustCheck)
        return true;

    // Write queued txindex changes
    for (map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin(); mi != mapQueuedChanges.end(); ++mi)
    {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock[] : UpdateTxIndex failed");
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock[] : WriteBlockIndex failed");
    }

    // Watch for transactions paying to me
    BOOST_FOREACH(CTransaction& tx, vtx)
        SyncWithWallets(tx, this, true);

    return true;
}


/*
bool static Reorganize_testnet(CTxDB& txdb)
{
    printf("REORGANIZE testnet \n");
    // Find the fork
	int nMaxDepth = nBestHeight;
	int nLookback = 200; 
	int nMinDepth = nMaxDepth - nLookback;
	if (nMinDepth < 2) nMinDepth = 2;
	CBlock block;
	int iRow = 0;
	for (int ii = nMaxDepth; ii > nMinDepth; ii--)
	{
				CBlockIndex* pblockindex = MainFindBlockByHeight(ii);
			    if (!block.ReadFromDisk(pblockindex))	return error("ReorganizeTestnet() : ReadFromDisk for disconnect failed");
                if (!block.DcnectBlock(txdb, pblockindex))
							return error("ReorganizeTestnet() : DisconnectBlock %s failed", pblockindex->GetBlockHash().ToString().c_str());
	}
	return true;
}
*/





bool static Reorganize(CTxDB& txdb, CBlockIndex* pindexNew)
{
    printf("REORGANIZE\n");

    // Find the fork
    CBlockIndex* pfork = pindexBest;
    CBlockIndex* plonger = pindexNew;
    while (pfork != plonger)
    {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->pprev))
                return error("Reorganize() : plonger->pprev is null");
        if (pfork == plonger)
            break;
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
    }

    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = pindexBest; pindex != pfork; pindex = pindex->pprev)
        vDisconnect.push_back(pindex);

    // List of what to connect
    vector<CBlockIndex*> vConnect;
    for (CBlockIndex* pindex = pindexNew; pindex != pfork; pindex = pindex->pprev)
        vConnect.push_back(pindex);
    reverse(vConnect.begin(), vConnect.end());

    printf("REORGANIZE: Disconnect %"PRIszu" blocks; %s..%s\n", vDisconnect.size(), pfork->GetBlockHash().ToString().substr(0,20).c_str(), pindexBest->GetBlockHash().ToString().substr(0,20).c_str());
    printf("REORGANIZE: Connect %"PRIszu" blocks; %s..%s\n", vConnect.size(), pfork->GetBlockHash().ToString().substr(0,20).c_str(), pindexNew->GetBlockHash().ToString().substr(0,20).c_str());
	
	if (vDisconnect.size() > 0)
	{
		//1-9-2015 - Re-eligibile for staking
		StructCPID sMag = mvMagnitudes[GlobalCPUMiningCPID.cpid];
		if (sMag.initialized)
		{
			sMag.LastPaymentTime = 0;
			mvMagnitudes[GlobalCPUMiningCPID.cpid]=sMag;
		}
		nLastBlockSolved = 0;
	
	}

    // Disconnect shorter branch
    list<CTransaction> vResurrect;
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, pindex))
            return error("Reorganize() : DisconnectBlock %s failed", pindex->GetBlockHash().ToString().substr(0,20).c_str());

        // Queue memory transactions to resurrect.
        // We only do this for blocks after the last checkpoint (reorganisation before that
        // point should only happen with -reindex/-loadblock, or a misbehaving peer.
        BOOST_REVERSE_FOREACH(const CTransaction& tx, block.vtx)
            if (!(tx.IsCoinBase() || tx.IsCoinStake()) && pindex->nHeight > Checkpoints::GetTotalBlocksEstimate())
                vResurrect.push_front(tx);
    }

    // Connect longer branch
    vector<CTransaction> vDelete;
    for (unsigned int i = 0; i < vConnect.size(); i++)
    {
        CBlockIndex* pindex = vConnect[i];
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.ConnectBlock(txdb, pindex))
        {
            // Invalid block
            return error("Reorganize() : ConnectBlock %s failed", pindex->GetBlockHash().ToString().substr(0,20).c_str());
        }

        // Queue memory transactions to delete
        BOOST_FOREACH(const CTransaction& tx, block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");

    // Make sure it's successfully written to disk before changing memory structure
    if (!txdb.TxnCommit())
        return error("Reorganize() : TxnCommit failed");

    // Disconnect shorter branch
    BOOST_FOREACH(CBlockIndex* pindex, vDisconnect)
        if (pindex->pprev)
            pindex->pprev->pnext = NULL;

    // Connect longer branch
    BOOST_FOREACH(CBlockIndex* pindex, vConnect)
        if (pindex->pprev)
            pindex->pprev->pnext = pindex;

    // Resurrect memory transactions that were in the disconnected branch
    BOOST_FOREACH(CTransaction& tx, vResurrect)
        AcceptToMemoryPool(mempool, tx, NULL);

    // Delete redundant memory transactions that are in the connected branch
    BOOST_FOREACH(CTransaction& tx, vDelete) {
        mempool.remove(tx);
        mempool.removeConflicts(tx);
    }

    printf("REORGANIZE: done\n");
	//12-9-2014  - Halford - Reload network averages after a reorg since user now has orphans
	//TallyNetworkAverages(true);
	//printf("Finished Retally");		
    return true;
}


void SetAdvisory()
{
	CheckpointsMode = Checkpoints::ADVISORY;
			
}

bool InAdvisory()
{
	return (CheckpointsMode == Checkpoints::ADVISORY);
}

// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew)
{
    uint256 hash = GetHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash))
    {
        txdb.TxnAbort();
        InvalidChainFound(pindexNew);
        return false;
    }
    if (!txdb.TxnCommit())
        return error("SetBestChain() : TxnCommit failed");

    // Add to current best branch
    pindexNew->pprev->pnext = pindexNew;

    // Delete redundant memory transactions
    BOOST_FOREACH(CTransaction& tx, vtx)
        mempool.remove(tx);

    return true;
}

bool CBlock::SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew)
{
    uint256 hash = GetHash();

    if (!txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == NULL && hash == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
    {
        txdb.WriteHashBestChain(hash);
        if (!txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    }
    else if (hashPrevBlock == hashBestChain)
    {
        if (!SetBestChainInner(txdb, pindexNew))
		{
			int nResult = 0;
	    	#if defined(WIN32) && defined(QT_GUI)
		    //nResult = RebootClient();
			#endif
			
            return error("SetBestChain() : SetBestChainInner failed");

		}
    }
    else
    {
        // the first block in the new chain that will cause it to become the new best chain
        CBlockIndex *pindexIntermediate = pindexNew;

        // list of blocks that need to be connected afterwards
        std::vector<CBlockIndex*> vpindexSecondary;

        // Reorganize is costly in terms of db load, as it works in a single db transaction.
        // Try to limit how much needs to be done inside
        while (pindexIntermediate->pprev && pindexIntermediate->pprev->nChainTrust > pindexBest->nChainTrust)
        {
            vpindexSecondary.push_back(pindexIntermediate);
            pindexIntermediate = pindexIntermediate->pprev;
        }

        if (!vpindexSecondary.empty())
            printf("Postponing %"PRIszu" reconnects\n", vpindexSecondary.size());

        // Switch to new best branch
        if (!Reorganize(txdb, pindexIntermediate))
        {
					//10-30-2014 - Halford - Reboot when reorganize fails 3 times
					REORGANIZE_FAILED++;

					std::string suppressreboot = GetArg("-suppressreboot", "true");
					if (suppressreboot!="true")
					{
						if (REORGANIZE_FAILED==100)
						{
							int nResult = 0;
							#if defined(WIN32) && defined(QT_GUI)
								nResult = RebootClient();
							#endif
							printf("Rebooting... %f",(double)nResult);
						}
					}
				txdb.TxnAbort();
				InvalidChainFound(pindexNew);
				return error("SetBestChain() : Reorganize failed");
		}
		REORGANIZE_FAILED=0;
			
        // Connect further blocks
        BOOST_REVERSE_FOREACH(CBlockIndex *pindex, vpindexSecondary)
        {
            CBlock block;
            if (!block.ReadFromDisk(pindex))
            {
                printf("SetBestChain() : ReadFromDisk failed\n");
                break;
            }
            if (!txdb.TxnBegin()) {
                printf("SetBestChain() : TxnBegin 2 failed\n");
                break;
            }
            // errors now are not fatal, we still did a reorganisation to a new chain in a valid way
            if (!block.SetBestChainInner(txdb, pindex))
                break;
        }
    }

    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload)
    {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }

    // New best block
    hashBestChain = hash;
    pindexBest = pindexNew;
    pblockindexFBBHLast = NULL;
    nBestHeight = pindexBest->nHeight;
    nBestChainTrust = pindexNew->nChainTrust;
    nTimeBestReceived =  GetAdjustedTime();
    nTransactionsUpdated++;

    uint256 nBestBlockTrust = pindexBest->nHeight != 0 ? (pindexBest->nChainTrust - pindexBest->pprev->nChainTrust) : pindexBest->nChainTrust;

	if (fDebug)
	{
	    printf("SetBestChain: new best=%s  height=%d  trust=%s  blocktrust=%"PRId64"  date=%s\n",
		  hashBestChain.ToString().substr(0,20).c_str(), nBestHeight,
		  CBigNum(nBestChainTrust).ToString().c_str(),
          nBestBlockTrust.Get64(),
          DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());
	}
	else
	{
		printf("{SBC} new best=%s  height=%d ; ",hashBestChain.ToString().c_str(), nBestHeight);

	}

    // Check the version of the last 100 blocks to see if we need to upgrade:
    if (!fIsInitialDownload)
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = pindexBest;
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            printf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded, CBlock::CURRENT_VERSION);
        if (nUpgraded > 100/2)
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
    }

    std::string strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }
	REORGANIZE_FAILED=0;

    return true;
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are 
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool CTransaction::GetCoinAge(CTxDB& txdb, uint64_t& nCoinAge) const
{
    CBigNum bnCentSecond = 0;  // coin age in the unit of cent-seconds
    nCoinAge = 0;

    if (IsCoinBase())
        return true;

    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        // First try finding the previous transaction in database
        CTransaction txPrev;
        CTxIndex txindex;
        if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
            continue;  // previous transaction not in main chain
        if (nTime < txPrev.nTime)
            return false;  // Transaction timestamp violation

        // Read block header
        CBlock block;
        if (!block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
            return false; // unable to read block of previous transaction
        if (block.GetBlockTime() + nStakeMinAge > nTime)
            continue; // only count coins meeting min age requirement

        int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += CBigNum(nValueIn) * (nTime-txPrev.nTime) / CENT;

        if (fDebug && GetBoolArg("-printcoinage"))
            printf("coin age nValueIn=%"PRId64" nTimeDiff=%d bnCentSecond=%s\n", nValueIn, nTime - txPrev.nTime, bnCentSecond.ToString().c_str());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug && GetBoolArg("-printcoinage"))
        printf("coin age bnCoinDay=%s\n", bnCoinDay.ToString().c_str());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}

// ppcoin: total coin age spent in block, in the unit of coin-days.
bool CBlock::GetCoinAge(uint64_t& nCoinAge) const
{
    nCoinAge = 0;

    CTxDB txdb("r");
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        uint64_t nTxCoinAge;
        if (tx.GetCoinAge(txdb, nTxCoinAge))
            nCoinAge += nTxCoinAge;
        else
            return false;
    }

    if (nCoinAge == 0) // block coin age minimum 1 coin-day
        nCoinAge = 1;
    if (fDebug && GetBoolArg("-printcoinage"))
        printf("block coin age total nCoinDays=%"PRId64"\n", nCoinAge);
    return true;
}

bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos, const uint256& hashProof)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().substr(0,20).c_str());

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    pindexNew->phashBlock = &hash;
    map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // ppcoin: compute chain trust score
    pindexNew->nChainTrust = (pindexNew->pprev ? pindexNew->pprev->nChainTrust : 0) + pindexNew->GetBlockTrust();

    // ppcoin: compute stake entropy bit for stake modifier
    if (!pindexNew->SetStakeEntropyBit(GetStakeEntropyBit()))
        return error("AddToBlockIndex() : SetStakeEntropyBit() failed");

    // Record proof hash value
    pindexNew->hashProof = hashProof;

    // ppcoin: compute stake modifier
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier))
	{
        printf("AddToBlockIndex() : ComputeNextStakeModifier() failed");
	}
    pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);

    
    // Add to mapBlockIndex
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    pindexNew->phashBlock = &((*mi).first);

    // Write to disk block index
    CTxDB txdb;
    if (!txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));
    if (!txdb.TxnCommit())
        return false;

    LOCK(cs_main);

    // New best
    if (pindexNew->nChainTrust > nBestChainTrust)
        if (!SetBestChain(txdb, pindexNew))
            return false;

    if (pindexNew == pindexBest)
    {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }

    uiInterface.NotifyBlocksChanged();
    return true;
}



bool Resuscitate()
{
    LOCK(cs_main);
    CTxDB txdb;
    int nMaxDepth = nBestHeight;
	if (nMaxDepth < 100) return false;
	CBlock block;
	int ii = 0;
	ii = nMaxDepth-50;
	CBlockIndex* pblockindex = MainFindBlockByHeight(ii);
	block.ReadFromDisk(pblockindex);
	pindexBest = pblockindex;
	InvalidChainFound(pindexBest);
	uiInterface.NotifyBlocksChanged();
    return true;
}


int BlockHeight(uint256 bh)
{
	int nGridHeight=0;
	map<uint256, CBlockIndex*>::iterator iMapIndex = mapBlockIndex.find(bh);
    if (iMapIndex != mapBlockIndex.end())
	{
		 CBlockIndex* pPrev = (*iMapIndex).second;
		 nGridHeight = pPrev->nHeight+1;
	}
	return nGridHeight;
}
	

bool CBlock::CheckBlock(int height1, int64_t Mint, bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig, bool fLoadingIndex) const
{
    
	//1-18-2015

	if (GetHash()==hashGenesisBlock || GetHash()==hashGenesisBlockTestNet) return true;
	
	// These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty() || vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return DoS(100, error("CheckBlock[] : size limits failed"));

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(GetPoWHash(), nBits))
        return DoS(50, error("CheckBlock[] : proof of work failed"));

	//Reject blocks with diff > 10000000000000000
	double blockdiff = GetBlockDifficulty(nBits);
	if (height1 > nGrandfather && blockdiff > 10000000000000000)
	{
		   return DoS(1, error("CheckBlock[] : Block Bits larger than 10000000000000000.\r\n"));
	}

	

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return DoS(100, error("CheckBlock[] : first tx is not coinbase"));
    for (unsigned int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return DoS(100, error("CheckBlock[] : more than one coinbase"));

 
	//ProofOfResearch
	if (vtx.size() > 0)
	{
			MiningCPID boincblock = DeserializeBoincBlock(vtx[0].hashBoinc);
			if (boincblock.cpid != "INVESTOR")
			{
    			if (boincblock.projectname == "") 	return DoS(1,error("PoR Project Name invalid"));
	    		if (boincblock.rac < 100) 			return DoS(1,error("RAC too low"));
				
				//Block CPID 12-26-2014 hashPrevBlock->nHeight
				if (!IsCPIDValidv2(boincblock,height1))
				{
						return DoS(100,error("Bad CPID : height %f, bad hashboinc %s",(double)height1,vtx[0].hashBoinc.c_str()));
				}

			}

		
		    // Gridcoin (NovaCoin's): check proof-of-stake block signature
			if (IsProofOfStake() && height1 > nGrandfather)
			{
		
				//Mint limiter checks 1-20-2015
				double PORDiff = GetBlockDifficulty(nBits);
				double mint1 = CoinToDouble(Mint);
				double total_subsidy = boincblock.ResearchSubsidy + boincblock.InterestSubsidy;
    
				if (fDebug) printf("CheckBlock[]: TotalSubsidy %f, Height %f, %s, %f, Res %f, Interest %f, hb: %s \r\n",
					(double)total_subsidy,(double)height1,    boincblock.cpid.c_str(),
						(double)mint1,boincblock.ResearchSubsidy,boincblock.InterestSubsidy,vtx[0].hashBoinc.c_str());
			
				if (total_subsidy < MintLimiter(PORDiff,boincblock.RSAWeight,boincblock.cpid,GetBlockTime()))
				{
					printf("****CheckBlock[]: Total Mint too Small %s, mint %f, Res %f, Interest %f, hash %s \r\n",boincblock.cpid.c_str(),
						(double)mint1,boincblock.ResearchSubsidy,boincblock.InterestSubsidy,vtx[0].hashBoinc.c_str());
					//1-21-2015 - Prevent Hackers from spamming the network with small blocks
					return DoS(30, 	error("****CheckBlock[]: Total Mint too Small %s, mint %f, Res %f, Interest %f, hash %s \r\n",boincblock.cpid.c_str(),
							(double)mint1,boincblock.ResearchSubsidy,boincblock.InterestSubsidy,vtx[0].hashBoinc.c_str()));
				}
			

				//12-26-2014 Halford - Orphan Flood Attack
				if (IsLockTimeWithinMinutes(GetBlockTime(),15))
				{
					double bv = BlockVersion(boincblock.clientversion);
					double cvn = ClientVersionNew();
					if (fDebug) printf("BV %f, CV %f   ",bv,cvn);
					if (bv+10 < cvn) return error("ConnectBlock(): Old client version after mandatory upgrade - block rejected\r\n");
					if (bv < 3373) return error("CheckBlock[]:  Old client spamming new blocks after mandatory upgrade \r\n");
				}



	    		if (fCheckSig && !CheckBlockSignature())
					return DoS(100, error("CheckBlock[] : bad proof-of-stake block signature"));
			}


		}
		else
		{ 
			printf("VX100-");
			return false;
		}

	// End of Proof Of Research


    if (IsProofOfStake())
    {
        // Coinbase output should be empty if proof-of-stake block
        if (vtx[0].vout.size() != 1 || !vtx[0].vout[0].IsEmpty())
            return DoS(100, error("CheckBlock[] : coinbase output not empty for proof-of-stake block"));

        // Second transaction must be coinstake, the rest must not be
        if (vtx.empty() || !vtx[1].IsCoinStake())
            return DoS(100, error("CheckBlock[] : second tx is not coinstake"));
        for (unsigned int i = 2; i < vtx.size(); i++)
            if (vtx[i].IsCoinStake())
                return DoS(100, error("CheckBlock[] : more than one coinstake"));

    }

    // Check transactions
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        if (!tx.CheckTransaction())
            return DoS(tx.nDoS, error("CheckBlock[] : CheckTransaction failed"));

        // ppcoin: check transaction timestamp
        if (GetBlockTime() < (int64_t)tx.nTime)
            return DoS(50, error("CheckBlock[] : block timestamp earlier than transaction timestamp"));
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    set<uint256> uniqueTx;
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock[] : duplicate transaction"));

    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        nSigOps += tx.GetLegacySigOpCount();
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock[] : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock[] : hashMerkleRoot mismatch"));


    return true;
}

bool CBlock::AcceptBlock(bool generated_by_me)
{
    AssertLockHeld(cs_main);
	
    if (nVersion > CURRENT_VERSION)
        return DoS(100, error("AcceptBlock() : reject unknown block version %d", nVersion));

    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found"));
    CBlockIndex* pindexPrev = (*mi).second;
    int nHeight = pindexPrev->nHeight+1;

    if (IsProtocolV2(nHeight) && nVersion < 7)
        return DoS(100, error("AcceptBlock() : reject too old nVersion = %d", nVersion));
    else if (!IsProtocolV2(nHeight) && nVersion > 6)
        return DoS(100, error("AcceptBlock() : reject too new nVersion = %d", nVersion));

    if (IsProofOfWork() && nHeight > LAST_POW_BLOCK)
        return DoS(100, error("AcceptBlock() : reject proof-of-work at height %d", nHeight));

	if (nHeight > nGrandfather)
	{
	  		// Check timestamp

			/*
			if (GetBlockTime() > FutureDrift(GetAdjustedTime()+10, nHeight))
				return DoS(80,error("AcceptBlock() : block timestamp too far in the future"));
			//Halford 1-16-2015 - Block Timestamp too Early

			if (!ClientOutOfSync())
			{
				double nLastTime = max(pindexBest->GetMedianTimePast()+1, GetAdjustedTime());
				if (GetBlockTime() < nLastTime)
				{
					return error("AcceptBlock(2) : block timestamp too far in the past");
				}
			}
			*/

			// Check coinbase timestamp
			if (GetBlockTime() > FutureDrift((int64_t)vtx[0].nTime, nHeight))
			{
				return DoS(80, error("AcceptBlock() : coinbase timestamp is too early"));
			}
			// Check coinstake timestamp
			//if (IsProofOfStake() && !CheckCoinStakeTimestamp(nHeight, GetBlockTime(), (int64_t)vtx[1].nTime))				return DoS(60, error("AcceptBlock() : coinstake timestamp violation nTimeBlock=%"PRId64" nTimeTx=%u", GetBlockTime(), vtx[1].nTime));

			// Check timestamp against prev
			if (GetBlockTime() <= pindexPrev->GetPastTimeLimit() || FutureDrift(GetBlockTime(), nHeight) < pindexPrev->GetBlockTime())
				return DoS(60, error("AcceptBlock() : block's timestamp is too early"));
		// Check proof-of-work or proof-of-stake
		if (nBits != GetNextTargetRequired(pindexPrev, IsProofOfStake()))
				return DoS(100, error("AcceptBlock() : incorrect %s", IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));

	}


    
    // Check that all transactions are finalized
    BOOST_FOREACH(const CTransaction& tx, vtx)
        if (!IsFinalTx(tx, nHeight, GetBlockTime()))
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

    uint256 hashProof;

    // Verify hash target and signature of coinstake tx
	if (nHeight > nGrandfather)
	{
				if (IsProofOfStake())
				{
					uint256 targetProofOfStake;
					
					if (!CheckProofOfStake(pindexPrev, vtx[1], nBits, hashProof, targetProofOfStake, vtx[0].hashBoinc, generated_by_me, nNonce))
					{
						//if (!generated_by_me) printf("WARNING: AcceptBlock[]: check proof-of-stake failed for block %s, nonce %f    \n", hash.ToString().c_str(),(double)nNonce);
						//return false; // do not error here as we expect this during initial block download
						return error("WARNING: AcceptBlock(): check proof-of-stake failed for block %s, nonce %f    \n", hash.ToString().c_str(),(double)nNonce);
						//return false; // do not error here as we expect this during initial block download

					}
					
				}
	}



    // PoW is checked in CheckBlock[]
    if (IsProofOfWork())
    {
        hashProof = GetPoWHash();
    }

   
	//Grandfather
	if (nHeight > nGrandfather)
	{
		 bool cpSatisfies = Checkpoints::CheckSync(hash, pindexPrev);

		// Check that the block satisfies synchronized checkpoint
		if (CheckpointsMode == Checkpoints::STRICT && !cpSatisfies)
		{
			if (CHECKPOINT_DISTRIBUTED_MODE==1)
			{
				CHECKPOINT_VIOLATIONS++;
				if (CHECKPOINT_VIOLATIONS > 3)
				{
					//For stability, move the client into ADVISORY MODE:
					printf("Moving Gridcoin into Checkpoint ADVISORY mode.\r\n");
					CheckpointsMode = Checkpoints::ADVISORY;
				}
			}
			return error("AcceptBlock() : rejected by synchronized checkpoint");
		}
		
		if (CheckpointsMode == Checkpoints::ADVISORY && !cpSatisfies)
			strMiscWarning = _("WARNING: synchronized checkpoint violation detected, but skipped!");
		
		if (CheckpointsMode == Checkpoints::ADVISORY && cpSatisfies && CHECKPOINT_DISTRIBUTED_MODE==1)
		{
			///Move the client back into STRICT mode
			CHECKPOINT_VIOLATIONS = 0;
			printf("Moving Gridcoin into Checkpoint STRICT mode.\r\n");
			strMiscWarning = "";
			CheckpointsMode = Checkpoints::STRICT;
		}

		// Enforce rule that the coinbase starts with serialized block height
		CScript expect = CScript() << nHeight;
		if (vtx[0].vin[0].scriptSig.size() < expect.size() ||
			!std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
			return DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));
	}

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    if (!WriteToDisk(nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos, hashProof))
        return error("AcceptBlock() : AddToBlockIndex failed");

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    // ppcoin: check pending sync-checkpoint
    Checkpoints::AcceptPendingSyncCheckpoint();

    return true;
}



uint256 CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    if (bnTarget <= 0)
        return 0;
	// R Halford - Add Magnitude to Block Trust - Subtract 1 million from chain trust for each magnitude (higher mag = more trust = lower bits = higher total chain trust)
			
	int64_t block_mag = 0;

	
	uint256 chaintrust = (((CBigNum(1)<<256) / (bnTarget+1)) - (block_mag)).getuint256();

	return chaintrust;
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired, unsigned int nToCheck)
{
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}


void GridcoinServices()
{
	
	
	if (nBestHeight > 100 && nBestHeight < 1000)
	{
		//12-28-2014 Suggested by SeP
		if (GetArg("-suppressdownloadblocks", "true") == "false")
		{

			std::string email = GetArgument("email", "NA");
			if (email.length() > 5 && !mbBlocksDownloaded)
			{
				#if defined(WIN32) && defined(QT_GUI)
					mbBlocksDownloaded=true;
					DownloadBlocks();
				#endif
			}
		}
	}
	//Called once for every block accepted
	if (OutOfSyncByAge()) return;


	//Backup the wallet once per 900 blocks:
	if (TimerMain("backupwallet", 900))
	{
		std::string backup_results = BackupGridcoinWallet();
		printf("Daily backup results: %s\r\n",backup_results.c_str());
	}
	//Every 30 blocks, tally consensus mags:
	if (TimerMain("update_boinc_magnitude", 30))
	{
			    TallyInBackground();
	}

	



	if (KeyEnabled("exportmagnitude"))
	{
		if (TimerMain("export_magnitude",900))
		{
			json_spirit::Array results;
		    results = MagnitudeReportCSV(true);

		}
	}

	if (TimerMain("gather_cpids",45))
	{
			if (fDebug) printf("\r\nReharvesting cpids in background thread...\r\n");
			LoadCPIDsInBackground();
			printf(" {CPIDs Re-Loaded} ");
	}

	if (TimerMain("check_for_autoupgrade",60))
	{
		bCheckedForUpgradeLive = true;
	}


	//Dont do this on headless-SePulcher 12-4-2014 (Halford)
	#if defined(QT_GUI)
	GetGlobalStatus();
	bForceUpdate=true;
	uiInterface.NotifyBlocksChanged();
    #endif

	#if defined(WIN32) && defined(QT_GUI)
	if (bCheckedForUpgradeLive == true && !fTestNet && bProjectsInitialized && bGlobalcomInitialized)
	{
		bCheckedForUpgradeLive=false;
		printf("{Checking for Upgrade} ");
		CheckForUpgrade();
	}
	#endif


}

void EraseOrphans()
{

	//12-26-2014
	mapOrphanBlocks.clear();
	setStakeSeenOrphan.clear();
	setStakeSeen.clear();
       
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock, bool generated_by_me)
{
    AssertLockHeld(cs_main);
	printf("+");

    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock[]: already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().substr(0,20).c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock[]: already have block (orphan) %s", hash.ToString().substr(0,20).c_str());

    // ppcoin: check proof-of-stake
    // Limited duplicity on stake: prevents block flood attack
    // Duplicate stake allowed only when there is orphan child block
	if (pblock->IsProofOfStake() && setStakeSeen.count(pblock->GetProofOfStake()) && !mapOrphanBlocksByPrev.count(hash) && !Checkpoints::WantedByPendingSyncCheckpoint(hash))
	{
		//MilliSleep(1);
        //return error("ProcessBlock[] : duplicate proof-of-stake (%s, %d) for block %s", pblock->GetProofOfStake().first.ToString().c_str(), pblock->GetProofOfStake().second, hash.ToString().c_str());
	}

    CBlockIndex* pcheckpoint = Checkpoints::GetLastSyncCheckpoint();
    if (pcheckpoint && pblock->hashPrevBlock != hashBestChain && !Checkpoints::WantedByPendingSyncCheckpoint(hash))
    {
        // Extra checks to prevent "fill up memory by spamming with bogus blocks"
		//HALFORD: (1): CHECKPOINTS_OUT
        int64_t deltaTime = pblock->GetBlockTime() - pcheckpoint->nTime;
        if (deltaTime < -10*60)
        {
            if (pfrom)                 pfrom->Misbehaving(1);
            return error("ProcessBlock[] : block with timestamp before last checkpoint");

        }

/* FIXME
        CBigNum bnNewBlock;
        bnNewBlock.SetCompact(pblock->nBits);
        CBigNum bnRequired;

        if (pblock->IsProofOfStake())
            bnRequired.SetCompact(ComputeMinStake(GetLastBlockIndex(pcheckpoint, true)->nBits, deltaTime, pblock->nTime));
        else
            bnRequired.SetCompact(ComputeMinWork(GetLastBlockIndex(pcheckpoint, false)->nBits, deltaTime));

        if (bnNewBlock > bnRequired)
        {
            if (pfrom)
                pfrom->Misbehaving(100);
            return error("ProcessBlock[] : block with too little %s", pblock->IsProofOfStake()? "proof-of-stake" : "proof-of-work");
        }
*/
    }

    // Preliminary checks 1-19-2015 ** Note: Mint is zero before block is signed
    if (!pblock->CheckBlock(pindexBest->nHeight, 100*COIN))
        return error("ProcessBlock[] : CheckBlock FAILED");

    // ppcoin: ask for pending sync-checkpoint if any
    if (!IsInitialBlockDownload())
        Checkpoints::AskForPendingSyncCheckpoint(pfrom);

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock))
    {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->hashPrevBlock.ToString().c_str());
		//12-26-2014 - Halford - Orphan Block Flood Attack

		double orphan_diff = GetAdjustedTime() - pfrom->nLastOrphan;
		if (orphan_diff < 60) pfrom->nOrphanCountViolations++;
		if (orphan_diff > 60*1)
		{
				pfrom->nOrphanCountViolations--;
				pfrom->nOrphanCount--;
		}

		if (orphan_diff > 60*10)
		{
				pfrom->nOrphanCountViolations=0;
				pfrom->nOrphanCount=0;
		}
	
		double orphan_punishment = GetArg("-orphanpunishment", 2);

		if (orphan_punishment > 0 && !ClientOutOfSync() )
		{
			if (pfrom->nOrphanCount > 75) 
			{
				if (fDebug2) printf("Orphan punishment enabled. %f    ",(double)pfrom->nOrphanCount);
				pfrom->Misbehaving(2);
			}
			if (pfrom->nOrphanCountViolations < 0) pfrom->nOrphanCountViolations=0;
			if (pfrom->nOrphanCount < 0)           pfrom->nOrphanCount=0;
			if (pfrom->nOrphanCountViolations > 65) pfrom->Misbehaving(2);
			pfrom->nLastOrphan=GetAdjustedTime();
			pfrom->nOrphanCount++;
		}

        // ppcoin: check proof-of-stake
        if (pblock->IsProofOfStake())
        {
            // Limited duplicity on stake: prevents block flood attack
            // Duplicate stake allowed only when there is orphan child block
            if (setStakeSeenOrphan.count(pblock->GetProofOfStake()) && !mapOrphanBlocksByPrev.count(hash) && !Checkpoints::WantedByPendingSyncCheckpoint(hash))
			{
				//MilliSleep(1);
                //return error("ProcessBlock[] : duplicate proof-of-stake (%s, %d) for orphan block %s", pblock->GetProofOfStake().first.ToString().c_str(), pblock->GetProofOfStake().second, hash.ToString().c_str());
			}
            else
			{
                setStakeSeenOrphan.insert(pblock->GetProofOfStake());
			}
        }
        CBlock* pblock2 = new CBlock(*pblock);
        mapOrphanBlocks.insert(make_pair(hash, pblock2));
        mapOrphanBlocksByPrev.insert(make_pair(pblock2->hashPrevBlock, pblock2));

        // Ask this guy to fill in what we're missing
        if (pfrom)
        {
            pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(pblock2));
            // ppcoin: getblocks may not obtain the ancestor block rejected
            // earlier by duplicate-stake check so we ask for it again directly
            if (!IsInitialBlockDownload())
                pfrom->AskFor(CInv(MSG_BLOCK, WantedByOrphan(pblock2)));
        }
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock(generated_by_me))
        return error("ProcessBlock[] : AcceptBlock FAILED");

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            CBlock* pblockOrphan = (*mi).second;
            if (pblockOrphan->AcceptBlock(generated_by_me))
                vWorkQueue.push_back(pblockOrphan->GetHash());
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            setStakeSeenOrphan.erase(pblockOrphan->GetProofOfStake());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }
	double nBalance = GetTotalBalance();
	std::string SendingWalletAddress = DefaultWalletAddress();
	printf("{PB}: ACC; \r\n");
	//	double mint = mapBlockIndex[hash]->nMint/COIN;
	
	if (CHECKPOINT_DISTRIBUTED_MODE==0)
	{
		// If we are in centralized mode, follow peercoins model: if responsible for sync-checkpoint send it
		if (pfrom && !CSyncCheckpoint::strMasterPrivKey.empty())        
		{
			printf("Sending sync checkpoint...\r\n");
				Checkpoints::SendSyncCheckpoint(Checkpoints::AutoSelectSyncCheckpoint());
		}
	}
	else if (CHECKPOINT_DISTRIBUTED_MODE==1)
	{
		//If we are in decentralized mode, follow Gridcoins model:
		//Include node balance and GRC Sending Address in sync checkpoint
		bool bUserQualified = IsUserQualifiedToSendCheckpoint();
		//printf("Checkpoint Staking amount %f \r\n",mint);
		if (pfrom && (nBalance > MINIMUM_CHECKPOINT_TRANSMISSION_BALANCE) && bUserQualified)
		{
			Checkpoints::SendSyncCheckpointWithBalance(Checkpoints::AutoSelectSyncCheckpoint(),nBalance,SendingWalletAddress);
		}
	}
	else if (CHECKPOINT_DISTRIBUTED_MODE==2)
	{
		//11-23-2014: If we are in decentralized individual hash checkpoint mode - send a hash checkpoint
		printf("Broadcasting hash Checkpoint for block %s \r\n",pblock->hashPrevBlock.GetHex().c_str());
		if (pfrom)
		{
			Checkpoints::SendSyncHashCheckpoint(pblock->hashPrevBlock,SendingWalletAddress);
		}

	}
	
	//Gridcoin - R Halford - 11-2-2014 - Initiative to move critical processes from Timer to main:
	GridcoinServices();
    return true;
}

// GridCoin: (previously NovaCoin) : Attempt to generate suitable proof-of-stake
bool CBlock::SignBlock(CWallet& wallet, int64_t nFees)
{
    // if we are trying to sign
    //    something except proof-of-stake block template
    if (!vtx[0].vout[0].IsEmpty())
        return false;

    // if we are trying to sign
    //    a complete proof-of-stake block
    if (IsProofOfStake())
        return true;

    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime(); // startup timestamp

    CKey key;
    CTransaction txCoinStake;
    if (IsProtocolV2(nBestHeight+1))
        txCoinStake.nTime &= ~STAKE_TIMESTAMP_MASK;


	
    int64_t nSearchTime = txCoinStake.nTime; // search to current time
	int64_t out_gridreward = 0;

    if (nSearchTime > nLastCoinStakeSearchTime)
    {
        int64_t nSearchInterval = IsProtocolV2(nBestHeight+1) ? 1 : nSearchTime - nLastCoinStakeSearchTime;
		std::string out_hashboinc = "";
        if (wallet.CreateCoinStake(wallet, nBits, nSearchInterval, nFees, txCoinStake, key, out_gridreward, out_hashboinc))
        {
			//1-8-2015 Extract solved Key
			double solvedNonce = cdbl(AppCache(pindexBest->GetBlockHash().GetHex()),0);
			nNonce=solvedNonce;
			if (fDebug3) printf("7. Nonce %f, SNonce %f, StakeTime %f, MaxHistTD %f, BBPTL %f, PDBTm %f \r\n",
				(double)nNonce,(double)solvedNonce,(double)txCoinStake.nTime,
				(double)max(pindexBest->GetPastTimeLimit()+1, PastDrift(pindexBest->GetBlockTime(), pindexBest->nHeight+1)),
				(double)pindexBest->GetPastTimeLimit(), (double)PastDrift(pindexBest->GetBlockTime(), pindexBest->nHeight+1)	);
		    if (txCoinStake.nTime >= max(pindexBest->GetPastTimeLimit()+1, PastDrift(pindexBest->GetBlockTime(), pindexBest->nHeight+1)))
			{
                // make sure coinstake would meet timestamp protocol
                //    as it would be the same as the block timestamp
                vtx[0].nTime = nTime = txCoinStake.nTime;
                nTime = max(pindexBest->GetPastTimeLimit()+1, GetMaxTransactionTime());
                nTime = max(GetBlockTime(), PastDrift(pindexBest->GetBlockTime(), pindexBest->nHeight+1));
	
			
                // we have to make sure that we have no future timestamps in
                //    our transactions set
                for (vector<CTransaction>::iterator it = vtx.begin(); it != vtx.end();)
                    if (it->nTime > nTime) { it = vtx.erase(it); } else { ++it; }
                vtx.insert(vtx.begin() + 1, txCoinStake);
				vtx[0].hashBoinc= out_hashboinc;

                hashMerkleRoot = BuildMerkleTree();
				return key.Sign(GetHash(), vchBlockSig);
			}
        }
        nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
        nLastCoinStakeSearchTime = nSearchTime;
    }

    return false;
}

bool CBlock::CheckBlockSignature() const
{
    if (IsProofOfWork())
        return vchBlockSig.empty();

    vector<valtype> vSolutions;
    txnouttype whichType;

    const CTxOut& txout = vtx[1].vout[1];

    if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        valtype& vchPubKey = vSolutions[0];
        CKey key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        if (vchBlockSig.empty())
            return false;
        return key.Verify(GetHash(), vchBlockSig);
    }

    return false;
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
    {
        fShutdown = true;
        string strMessage = _("Warning: Disk space is low!");
        strMiscWarning = strMessage;
        printf("*** %s\n", strMessage.c_str());
        uiInterface.ThreadSafeMessageBox(strMessage, "GridCoin", CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
        StartShutdown();
        return false;
    }
    return true;
}

static filesystem::path BlockFilePath(unsigned int nFile)
{
    string strBlockFn = strprintf("blk%04u.dat", nFile);
    return GetDataDir() / strBlockFn;
}

FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if ((nFile < 1) || (nFile == (unsigned int) -1))
        return NULL;
    FILE* file = fopen(BlockFilePath(nFile).string().c_str(), pszMode);
    if (!file)
        return NULL;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return NULL;
        }
    }
    return file;
}

static unsigned int nCurrentBlockFile = 1;

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    while (true)
    {
        FILE* file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
        if (!file)
            return NULL;
        if (fseek(file, 0, SEEK_END) != 0)
            return NULL;
        // FAT32 file size max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < (long)(0x7F000000 - MAX_SIZE))
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        fclose(file);
        nCurrentBlockFile++;
    }
}

bool LoadBlockIndex(bool fAllowNew)
{
    LOCK(cs_main);

    CBigNum bnTrustedModulus;

    if (fTestNet)
    {
        pchMessageStart[0] = 0xcd;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xc0;
        pchMessageStart[3] = 0xef;

        bnTrustedModulus.SetHex("f0d14cf72623dacfe738d0892b599be0f31052239cddd95a3f25101c801dc990453b38c9434efe3f372db39a32c2bb44cbaea72d62c8931fa785b0ec44531308df3e46069be5573e49bb29f4d479bfc3d162f57a5965db03810be7636da265bfced9c01a6b0296c77910ebdc8016f70174f0f18a57b3b971ac43a934c6aedbc5c866764a3622b5b7e3f9832b8b3f133c849dbcc0396588abcd1e41048555746e4823fb8aba5b3d23692c6857fccce733d6bb6ec1d5ea0afafecea14a0f6f798b6b27f77dc989c557795cc39a0940ef6bb29a7fc84135193a55bcfc2f01dd73efad1b69f45a55198bd0e6bef4d338e452f6a420f1ae2b1167b923f76633ab6e55");
        bnProofOfWorkLimit = bnProofOfWorkLimitTestNet; // 16 bits PoW target limit for testnet
        nStakeMinAge = 1 * 60 * 60; // test net min age is 1 hour
        nCoinbaseMaturity = 10; // test maturity is 10 blocks
    }
    else
    {
        bnTrustedModulus.SetHex("d01f952e1090a5a72a3eda261083256596ccc192935ae1454c2bafd03b09e6ed11811be9f3a69f5783bbbced8c6a0c56621f42c2d19087416facf2f13cc7ed7159d1c5253119612b8449f0c7f54248e382d30ecab1928dbf075c5425dcaee1a819aa13550e0f3227b8c685b14e0eae094d65d8a610a6f49fff8145259d1187e4c6a472fa5868b2b67f957cb74b787f4311dbc13c97a2ca13acdb876ff506ebecbb904548c267d68868e07a32cd9ed461fbc2f920e9940e7788fed2e4817f274df5839c2196c80abe5c486df39795186d7bc86314ae1e8342f3c884b158b4b05b4302754bf351477d35370bad6639b2195d30006b77bf3dbb28b848fd9ecff5662bf39dde0c974e83af51b0d3d642d43834827b8c3b189065514636b8f2a59c42ba9b4fc4975d4827a5d89617a3873e4b377b4d559ad165748632bd928439cfbc5a8ef49bc2220e0b15fb0aa302367d5e99e379a961c1bc8cf89825da5525e3c8f14d7d8acca2fa9c133a2176ae69874d8b1d38b26b9c694e211018005a97b40848681b9dd38feb2de141626fb82591aad20dc629b2b6421cef1227809551a0e4e943ab99841939877f18f2d9c0addc93cf672e26b02ed94da3e6d329e8ac8f3736eebbf37bb1a21e5aadf04ee8e3b542f876aa88b2adf2608bd86329b7f7a56fd0dc1c40b48188731d11082aea360c62a0840c2db3dad7178fd7e359317ae081");
    }

#if 0
    // Set up the Zerocoin Params object
    ZCParams = new libzerocoin::Params(bnTrustedModulus);
#endif

    //
    // Load block index
    //
    CTxDB txdb("cr+");
    if (!txdb.LoadBlockIndex())
        return false;

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty())
    {
        if (!fAllowNew)
            return false;

        // Genesis block - Genesis2
        // MainNet - Official New Genesis Block:
		////////////////////////////////////////
		/*
	 21:58:24 block.nTime = 1413149999 
	10/12/14 21:58:24 block.nNonce = 1572771 
	10/12/14 21:58:24 block.GetHash = 00000f762f698b5962aa81e38926c3a3f1f03e0b384850caed34cd9164b7f990
	10/12/14 21:58:24 CBlock(hash=00000f762f698b5962aa81e38926c3a3f1f03e0b384850caed34cd9164b7f990, ver=1, 
	hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000,
	hashMerkleRoot=0bd65ac9501e8079a38b5c6f558a99aea0c1bcff478b8b3023d09451948fe841, nTime=1413149999, nBits=1e0fffff, nNonce=1572771, vtx=1, vchBlockSig=)
	10/12/14 21:58:24   Coinbase(hash=0bd65ac950, nTime=1413149999, ver=1, vin.size=1, vout.size=1, nLockTime=0)
    CTxIn(COutPoint(0000000000, 4294967295), coinbase 00012a4531302f31312f313420416e6472656120526f73736920496e647573747269616c20486561742076696e646963617465642077697468204c454e522076616c69646174696f6e)
    CTxOut(empty)
	vMerkleTree: 0bd65ac950 

		*/

		const char* pszTimestamp = "10/11/14 Andrea Rossi Industrial Heat vindicated with LENR validation";

        CTransaction txNew;
		//GENESIS TIME

		txNew.nTime = 1413033777;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 0 << CBigNum(42) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock = 0;
        block.hashMerkleRoot = block.BuildMerkleTree();
        block.nVersion = 1;
		//R&D - Testers Wanted Thread:
		block.nTime    = !fTestNet ? 1413033777 : 1406674534;
		//Official Launch time:
        block.nBits    = bnProofOfWorkLimit.GetCompact();
		block.nNonce = !fTestNet ? 130208 : 1;
    	printf("starting Genesis Check...");
	    // If genesis block hash does not match, then generate new genesis hash.
        if (block.GetHash() != hashGenesisBlock)  
        {
            printf("Searching for genesis block...\n");
            // This will figure out a valid hash and Nonce if you're
            // creating a different genesis block: 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000xFFF
            
			uint256 hashTarget = CBigNum().SetCompact(block.nBits).getuint256();
            uint256 thash;
			
            while (true)
            {
            	thash = block.GetHash();
			
				if (thash <= hashTarget)
                    break;
                if ((block.nNonce & 0xFFF) == 0)
                {
                    printf("nonce %08X: hash = %s (target = %s)\n", block.nNonce, thash.ToString().c_str(), hashTarget.ToString().c_str());
                }
                ++block.nNonce;
                if (block.nNonce == 0)
                {
                    printf("NONCE WRAPPED, incrementing time\n");
                    ++block.nTime;
                }
            }
            printf("block.nTime = %u \n", block.nTime);
            printf("block.nNonce = %u \n", block.nNonce);
            printf("block.GetHash = %s\n", block.GetHash().ToString().c_str());
        }


        block.print();
		
	    //// debug print
	
		//GENESIS3: Official Merkle Root
		uint256 merkle_root = uint256("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f");
		assert(block.hashMerkleRoot == merkle_root);
        assert(block.GetHash() == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
        assert(block.CheckBlock(1,10*COIN));

        // Start new block file
        unsigned int nFile;
        unsigned int nBlockPos;
        if (!block.WriteToDisk(nFile, nBlockPos))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        if (!block.AddToBlockIndex(nFile, nBlockPos, hashGenesisBlock))
            return error("LoadBlockIndex() : genesis block not accepted");

        // ppcoin: initialize synchronized checkpoint
        if (!Checkpoints::WriteSyncCheckpoint((!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet)))
            return error("LoadBlockIndex() : failed to init sync checkpoint");
    }

    string strPubKey = "";

    // if checkpoint master key changed must reset sync-checkpoint
    if (!txdb.ReadCheckpointPubKey(strPubKey) || strPubKey != CSyncCheckpoint::strMasterPubKey)
    {
        // write checkpoint master key to db
        txdb.TxnBegin();
        if (!txdb.WriteCheckpointPubKey(CSyncCheckpoint::strMasterPubKey))
            return error("LoadBlockIndex() : failed to write new checkpoint master key to db");
        if (!txdb.TxnCommit())
            return error("LoadBlockIndex() : failed to commit new checkpoint master key to db");
        if ((!fTestNet) && !Checkpoints::ResetSyncCheckpoint())
            return error("LoadBlockIndex() : failed to reset sync-checkpoint");
    }

    return true;
}


vector<unsigned char> StringToVector(std::string sData)
{
        vector<unsigned char> v(sData.begin(), sData.end());
		return v;
}

std::string VectorToString(vector<unsigned char> v)
{
        std::string s(v.begin(), v.end());
        return s;
}


std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end)
{

	std::string extraction = "";
	string::size_type loc = XMLdata.find( key, 0 );
	if( loc != string::npos ) 
	{
		string::size_type loc_end = XMLdata.find( key_end, loc+3);
		if (loc_end != string::npos )
		{
			extraction = XMLdata.substr(loc+(key.length()),loc_end-loc-(key.length()));

		}
	}

	return extraction;
}



std::string RetrieveMd5(std::string s1)
{
	try 
	{
		const char* chIn = s1.c_str();
		unsigned char digest2[16];
		//const unsigned char * pszBlah = reinterpret_cast<const unsigned char *> (s1.c_str());
		MD5((unsigned char*)chIn, strlen(chIn), (unsigned char*)&digest2);    
		char mdString2[33];
		for(int i = 0; i < 16; i++) sprintf(&mdString2[i*2], "%02x", (unsigned int)digest2[i]);
 		std::string xmd5(mdString2);
		return xmd5;
	}
    catch (std::exception &e) 
	{
		printf("MD5 INVALID!");
		return "";
	}

	
}



double Round(double d, int place)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(place) << d ;
	double r = lexical_cast<double>(ss.str());
	return r;
}

double cdbl(std::string s, int place)
{
	if (s=="") s="0";

    double r = lexical_cast<double>(s);
	double d = Round(r,place);
	return d;
}


std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
	  if (fDebug)  printf("loading file to string %s","test");

    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}

std::ifstream::pos_type filesize2(const char* filename)
{
    std::ifstream in(filename, std::ifstream::in | std::ifstream::binary);
    in.seekg(0, std::ifstream::end);
    return in.tellg(); 
}



std::string deletefile(std::string filename)
{
	std::string buffer;
	std::string line;
	ifstream myfile;   
    if (fDebug) printf("loading file to string %s",filename.c_str());

	filesystem::path path = filename;

	if (!filesystem::exists(path)) {
		printf("the file does not exist %s",path.string().c_str());	
		return "-1";
	}


	 int deleted = remove(filename.c_str());
	 if (deleted != 0) return "Error deleting.";
	 return "";
	
}


int GetFilesize(FILE* file)
{
    int nSavePos = ftell(file);
    int nFilesize = -1;
    if (fseek(file, 0, SEEK_END) == 0)
        nFilesize = ftell(file);
    fseek(file, nSavePos, SEEK_SET);
    return nFilesize;
}



std::string getfilecontents(std::string filename)
{
	std::string buffer;
	std::string line;
	ifstream myfile;   
    if (fDebug) printf("loading file to string %s",filename.c_str());

	filesystem::path path = filename;

	if (!filesystem::exists(path)) {
		printf("the file does not exist %s",path.string().c_str());	
		return "-1";
	}

	
	if (false) {
		std::string destname = filename+".copy";
	
		ifstream source(filename.c_str(), ios::binary);
		ofstream dest(destname.c_str(), ios::binary);
		istreambuf_iterator<char> begin_source(source);
		istreambuf_iterator<char> end_source;
		ostreambuf_iterator<char> begin_dest(dest); 
		copy(begin_source, end_source, begin_dest);
		source.close();
		dest.close();
	}

	 FILE *file = fopen(filename.c_str(), "rb");
     CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
	 int fileSize = GetFilesize(filein);
     filein.fclose();

	 myfile.open(filename.c_str()); 
		
    buffer.reserve(fileSize);
    if (fDebug) printf("opening file %s",filename.c_str());
	
	if(myfile) 
	{
	  while(getline(myfile, line))
	  {
			buffer = buffer + line + "\r\n";
	  }
	}   
	myfile.close();
	return buffer;
}


bool IsCPIDValidv3(std::string cpidv2, bool allow_investor)
{
	bool result=false;
	if (allow_investor) if (cpidv2 == "INVESTOR" || cpidv2=="investor") return true;
	if (cpidv2.length() < 34) return false;
	result = CPID_IsCPIDValid(cpidv2.substr(0,32),cpidv2,0);
	return result;
}

bool IsCPIDValidv2(MiningCPID& mc, int height)
{
	//12-24-2014 Halford - Transition to CPIDV2
	if (height < nGrandfather) return true;

	bool result = false;
	if (height < 97000)
	{
			result = IsCPIDValid_Retired(mc.cpid,mc.enccpid);
	}
	else
	{
		    if (mc.cpid == "INVESTOR" || mc.cpid=="investor") return true;
	        result = CPID_IsCPIDValid(mc.cpid, mc.cpidv2, (uint256)mc.lastblockhash);
	}

	return result;
}


bool IsLocalCPIDValid(StructCPID& structcpid)
{

	bool new_result = IsCPIDValidv3(structcpid.cpidv2,true);
	return new_result;

}



bool IsCPIDValid_Retired(std::string cpid, std::string ENCboincpubkey)
{

	try
	{
			if(cpid=="" || cpid.length() < 5) 
			{
				printf("CPID length empty.");
				return false;
			}
			if (cpid=="INVESTOR") return true;
			if (ENCboincpubkey == "" || ENCboincpubkey.length() < 5) 
			{
					if (fDebug) printf("ENCBpk length empty.");
					return false;
			}
			std::string bpk = AdvancedDecrypt(ENCboincpubkey);
			std::string bpmd5 = RetrieveMd5(bpk);
			if (bpmd5==cpid) return true;
			if (fDebug) printf("Md5<>cpid, md5 %s cpid %s  root bpk %s \r\n     ",bpmd5.c_str(), cpid.c_str(),bpk.c_str());

			return false;
	}
	catch (std::exception &e) 
	{
				printf("Error while resolving IsCpidValid\r\n");
				return false;
	}
	catch(...)
	{
				printf("Error while Resolving IsCpidValid 2.\r\n");
				return false;
	}
	return false;

}

double Cap(double dAmt, double Ceiling)
{
	if (dAmt > Ceiling) dAmt = Ceiling;
	return dAmt;
}

int64_t Floor(int64_t iAmt1, int64_t iAmt2)
{
	int64_t iOut = 0;
	if (iAmt1 <= iAmt2) 
	{
		iOut = iAmt1;
	}
	else
	{
		iOut = iAmt2;
	}
	return iOut;
	
}

double coalesce(double mag1, double mag2)
{
	if (mag1 > 0) return mag1;
	return mag2;
}

double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime, double& total_owed, double block_magnitude)
{

	
	//Gridcoin - payment range is stored in HighLockTime-LowLockTime
	// If newbie has not participated for 14 days, use earliest payment in chain to assess payment window
	// (Important to prevent e-mail change attacks) - Calculate payment timespan window in days
	double payment_timespan = (GetAdjustedTime() - mag.EarliestPaymentTime)/38400;
	if (payment_timespan < 2) payment_timespan =  2;
	if (payment_timespan > 10) payment_timespan = 14;
	mag.PaymentTimespan = payment_timespan;
	double research_magnitude = LederstrumpfMagnitude2(coalesce(mag.ConsensusMagnitude,block_magnitude),locktime);
	double owed = payment_timespan * Cap(research_magnitude*GetMagnitudeMultiplier(locktime), GetMaximumBoincSubsidy(locktime)*5);
	double paid = mag.payments;
	double outstanding = Cap(owed-paid, GetMaximumBoincSubsidy(locktime)*5);
	//Calculate long term totals:
	total_owed = payment_timespan * Cap(research_magnitude*GetMagnitudeMultiplier(locktime), GetMaximumBoincSubsidy(locktime)*5);
	//printf("Getting payment_timespan %f for outstanding amount for %s; owed %f paid %f Research Magnitude %f \r\n",		payment_timespan,cpid.c_str(),owed,paid,mag.ConsensusMagnitude);
	if (outstanding < 0) outstanding=0;
	return outstanding;
}

bool IsLockTimeWithin14days(double locktime)
{
	//Within 14 days
	double nCutoff =  GetAdjustedTime() - (60*60*24*14);
	if (locktime < nCutoff) return false;
	return true;
}

bool LockTimeRecent(double locktime)
{
	//Returns true if adjusted time is within 45 minutes
	double nCutoff =  GetAdjustedTime() - (60*45);
	if (locktime < nCutoff) return false;
	return true;
}

bool IsLockTimeWithinMinutes(double locktime, int minutes)
{
	double nCutoff =  GetAdjustedTime() - (60*minutes);
	if (locktime < nCutoff) return false;
	return true;
}

bool IsLockTimeWithinMinutes(int64_t locktime, int minutes)
{
	double nCutoff = GetAdjustedTime() - (60*minutes);
	if (locktime < nCutoff) return false;
	return true;
}


double GetMagnitudeWeight(double LockTime)
{
	double age = ( GetAdjustedTime() - LockTime)/86400;
	double inverse = 14-age;
	if (inverse < 1) inverse=1;
	return inverse*inverse;
}



double GetChainDailyAvgEarnedByCPID(std::string cpid, int64_t locktime, double& out_payments, double& out_daily_avg_payments)
{
	// Returns the average daily payment amount per CPID
	out_payments=0;
	out_daily_avg_payments=0;
	if (cpid=="INVESTOR") return 0;
	StructCPID structMag = GetStructCPID();
	//CPID is verified at the block level
	structMag = mvMagnitudes[cpid];

	if (structMag.initialized)
	{
				double AvgDailyPayments = structMag.payments/14;
				double DailyOwed = (structMag.PaymentTimespan * Cap(structMag.PaymentMagnitude*GetMagnitudeMultiplier(locktime), GetMaximumBoincSubsidy(locktime)*5)/14);
				out_payments=structMag.payments;
				out_daily_avg_payments = AvgDailyPayments;
				return DailyOwed;
	}
	else
	{
		//Uninitialized Newbie... 
		double NewbieCredit = GetMaximumBoincSubsidy(locktime);
		return NewbieCredit;
	}
			
}

bool ChainPaymentViolation(std::string cpid, int64_t locktime_future, double Proposed_Subsidy)
{
	double DailyOwed = 0;
	double Payments = 0;
	double AvgDailyPayments = 0;
	int64_t locktime = locktime_future - (60*60*24*14);
	double BlockMax = GetMaximumBoincSubsidy(locktime);
	//2-2-2015, R Halford: To help smooth out cutover periods, Assess mag multiplier and max boinc subsidies as of T-14:
	if (cpid=="INVESTOR" || cpid=="investor") return true;
	if (Proposed_Subsidy > BlockMax) 
	{
		if (fDebug3 || LessVerbose(100)) printf("Chain payment violation - proposed subsidy greater than Block Max %s Proposed Subsidy %f",cpid.c_str(),Proposed_Subsidy);
		return false;
	}
	DailyOwed = GetChainDailyAvgEarnedByCPID(cpid,locktime,Payments,AvgDailyPayments);
	bool bApproved = ( (Proposed_Subsidy+Payments)/14  <  DailyOwed*TOLERANCE_PERCENT);
	if (!bApproved) 
	{
		printf("Chain payment violation for CPID %s, proposed_subsidy %f, Payments %f, DailyOwed %f, AvgDailyPayments %f",
			   cpid.c_str(),Proposed_Subsidy,Payments,DailyOwed,AvgDailyPayments);
	}
	return bApproved;
}


void AddWeightedMagnitude(double LockTime,StructCPID &structMagnitude,double magnitude_level)
{
	double weight = GetMagnitudeWeight(LockTime);
	structMagnitude.ConsensusTotalMagnitude = structMagnitude.ConsensusTotalMagnitude + (magnitude_level*weight);
	structMagnitude.ConsensusMagnitudeCount=structMagnitude.ConsensusMagnitudeCount + weight;
	structMagnitude.Accuracy++;
	structMagnitude.ConsensusMagnitude = (structMagnitude.ConsensusTotalMagnitude/(structMagnitude.ConsensusMagnitudeCount+.01));
	structMagnitude.Magnitude = structMagnitude.ConsensusMagnitude;
	structMagnitude.PaymentMagnitude = LederstrumpfMagnitude2(structMagnitude.Magnitude,LockTime);
}



void RemoveNetworkMagnitude(double LockTime, std::string cpid, MiningCPID bb, double mint, bool IsStake)
{
        if (!IsLockTimeWithin14days(LockTime)) return;
		StructCPID structMagnitude = GetStructCPID();
		structMagnitude = mvMagnitudes[cpid];
		if (!structMagnitude.initialized)
		{
			structMagnitude = GetStructCPID();
			structMagnitude.initialized=true;
			structMagnitude.LastPaymentTime = 0;
			structMagnitude.EarliestPaymentTime = 99999999999;
			mvMagnitudes.insert(map<string,StructCPID>::value_type(cpid,structMagnitude));
		}
		structMagnitude.projectname = bb.projectname;
		structMagnitude.rac = structMagnitude.rac - bb.rac;
		structMagnitude.entries--;
		if (IsStake)
		{
			double interest = (double)mint - (double)bb.ResearchSubsidy;
			structMagnitude.payments -= bb.ResearchSubsidy;
			structMagnitude.interestPayments = structMagnitude.interestPayments - interest;
		    structMagnitude.LastPaymentTime = 0;
		}
		structMagnitude.cpid = cpid;
		double total_owed = 0;
		mvMagnitudes[cpid] = structMagnitude;
		structMagnitude.owed = GetOutstandingAmountOwed(structMagnitude,cpid,LockTime,total_owed,bb.Magnitude);
		structMagnitude.totalowed = total_owed;
		mvMagnitudes[cpid] = structMagnitude;
}





void AddNetworkMagnitude(double LockTime, std::string cpid, MiningCPID bb, double mint, bool IsStake)
{

	    if (!IsLockTimeWithin14days(LockTime)) return;

		StructCPID globalMag = GetStructCPID();
		globalMag = mvMagnitudes["global"];
		if (!globalMag.initialized)
		{
				globalMag = GetStructCPID();
				globalMag.initialized=true;
				globalMag.LowLockTime = 99999999999;
				globalMag.HighLockTime = 0;
				mvMagnitudes.insert(map<string,StructCPID>::value_type(cpid,globalMag));
		}

		
		StructCPID structMagnitude = GetStructCPID();

		structMagnitude = mvMagnitudes[cpid];

		if (!structMagnitude.initialized)
		{
			structMagnitude = GetStructCPID();
			structMagnitude.initialized=true;
			structMagnitude.LastPaymentTime = 0;
			structMagnitude.EarliestPaymentTime = 99999999999;
			mvMagnitudes.insert(map<string,StructCPID>::value_type(cpid,structMagnitude));
								
		}

		structMagnitude.projectname = bb.projectname;
		structMagnitude.rac = structMagnitude.rac + bb.rac;
		structMagnitude.entries++;

		if (IsStake)
		{
			double interest = (double)mint - (double)bb.ResearchSubsidy;

			//double interest = bb.InterestSubsidy;
			structMagnitude.payments += bb.ResearchSubsidy;
			structMagnitude.interestPayments=structMagnitude.interestPayments + interest;
			
			if (LockTime > structMagnitude.LastPaymentTime && bb.ResearchSubsidy > 0) structMagnitude.LastPaymentTime = LockTime;
			if (LockTime < structMagnitude.EarliestPaymentTime) structMagnitude.EarliestPaymentTime = LockTime;
			// Per RTM 12-23-2014 (Halford) Track detailed payments made to each CPID
			structMagnitude.PaymentTimestamps += RoundToString(LockTime,0)+",";
			structMagnitude.PaymentAmountsResearch    += RoundToString(bb.ResearchSubsidy,2) + ",";
			structMagnitude.PaymentAmountsInterest    += RoundToString(interest,2) + ",";
		}
		structMagnitude.cpid = cpid;
		//Since this function can be called more than once per block (once for the solver, once for the voucher), the avg changes here:
		if (bb.Magnitude > 0)
		{
			//printf("Adding magnitude %f for %s",bb.Magnitude,bb.cpid.c_str());
			AddWeightedMagnitude(LockTime,structMagnitude,bb.Magnitude);
		}

		structMagnitude.AverageRAC = structMagnitude.rac / (structMagnitude.entries+.01);
		if (LockTime < globalMag.LowLockTime)  globalMag.LowLockTime=LockTime;
		if (LockTime > globalMag.HighLockTime) globalMag.HighLockTime = LockTime;
		double total_owed = 0;
		mvMagnitudes[cpid] = structMagnitude;
		structMagnitude.owed = GetOutstandingAmountOwed(structMagnitude,cpid,LockTime,total_owed,bb.Magnitude);
		structMagnitude.totalowed = total_owed;
		mvMagnitudes[cpid] = structMagnitude;
		mvMagnitudes["global"] = globalMag;
}




bool TallyNetworkAverages(bool ColdBoot)
{
	//Iterate throught last 14 days, tally network averages
    if (nBestHeight < 15) 
	{
		bNetAveragesLoaded = true;
		return true;
	}
	if (fDebug) printf("Gathering network avgs (begin)\r\n");
	//If we did this within the last 5 mins, we are fine:
	if (IsLockTimeWithinMinutes(nLastTallied,10)) return true;
	nLastTallied = GetAdjustedTime();

	bNetAveragesLoaded = false;
	
	LOCK(cs_main);
	try 
	{
					int nMaxDepth = nBestHeight;
					int nLookback = 1000*14; //Daily block count * Lookback in days
					int nMinDepth = nMaxDepth - nLookback;
					if (nMinDepth < 2) nMinDepth = 2;
					if (mvNetwork.size() > 1) mvNetwork.clear();
					if (mvNetworkCPIDs.size() > 1)  mvNetworkCPIDs.clear();
					if (mvMagnitudes.size() > 1) 	mvMagnitudes.clear();
						
					CBlock block;
					printf("Gathering network avgs [2]\r\n");

					int iRow = 0;
					double NetworkRAC = 0;
					double NetworkProjects = 0;
					//Adding support for Magnitudes With Consensus by CPID
					for (int ii = nMaxDepth; ii > nMinDepth; ii--)
					{
     					CBlockIndex* pblockindex = FindBlockByHeight(ii);
						block.ReadFromDisk(pblockindex);
						std::string hashboinc = "";
						double mint = CoinToDouble(pblockindex->nMint);

						if (block.vtx.size() > 0) hashboinc = block.vtx[0].hashBoinc;
						MiningCPID bb = DeserializeBoincBlock(hashboinc);

						//Verify validity of project
						bool piv = ProjectIsValid(bb.projectname);
						if ( (piv && bb.rac > 100 && bb.projectname.length() > 2 && bb.cpid.length() > 5)  || bb.cpid=="INVESTOR" ) 
						{

							std::string proj= bb.projectname;
							std::string projcpid = bb.cpid + ":" + bb.projectname;
							std::string cpid = bb.cpid;
							std::string cpidv2 = bb.cpidv2;
							std::string lbh = bb.lastblockhash;

							StructCPID structcpid = GetStructCPID();
							structcpid = mvNetwork[proj];
							iRow++;
							if (!structcpid.initialized) 
							{
								structcpid = GetStructCPID();
								structcpid.initialized = true;
								mvNetwork.insert(map<string,StructCPID>::value_type(proj,structcpid));
							} 

							//Insert Global Network Project Stats:
							structcpid.projectname = proj;
							structcpid.rac = structcpid.rac + bb.rac;
							NetworkRAC = NetworkRAC + bb.rac;
							structcpid.TotalRAC = NetworkRAC;
							structcpid.entries++;
							if (structcpid.entries > 0)
							{
									structcpid.AverageRAC = structcpid.rac / (structcpid.entries+.01);
							}
							mvNetwork[proj] = structcpid;

							//Insert Global Project+CPID Stats:
							StructCPID structnetcpidproject = GetStructCPID();
							structnetcpidproject = mvNetworkCPIDs[projcpid];

							if (!structnetcpidproject.initialized)
							{
								structnetcpidproject = GetStructCPID();
								structnetcpidproject.initialized = true;
								mvNetworkCPIDs.insert(map<string,StructCPID>::value_type(projcpid,structnetcpidproject));
								NetworkProjects++;

							}
							structnetcpidproject.projectname = proj;
							structnetcpidproject.cpid = cpid;
							structnetcpidproject.cpidv2 = cpidv2;
							structnetcpidproject.rac = structnetcpidproject.rac + bb.rac;
							structnetcpidproject.entries++;
							if (structnetcpidproject.entries > 0)
							{
								structnetcpidproject.AverageRAC = structnetcpidproject.rac/(structnetcpidproject.entries+.01);
							}
							mvNetworkCPIDs[projcpid] = structnetcpidproject;
							// Insert CPID, Magnitude, Payments
							AddNetworkMagnitude(block.nTime,cpid,bb,mint,pblockindex->IsProofOfStake());

					    	//	printf("Adding mint %f for %s\r\n",mint,cpid.c_str());

						}
					}
					double NetworkAvg = 0;
					if (iRow > 0)
					{
						NetworkAvg = NetworkRAC/(iRow+.01);
					}
	
					if (fDebug) printf("Assimilating network consensus..");

					StructCPID structcpid = GetStructCPID();
					structcpid = mvNetwork["NETWORK"];
					structcpid.projectname="NETWORK";
					structcpid.rac = NetworkRAC;
					structcpid.entries = 1;
					structcpid.cpidv2 = "";
					structcpid.initialized = true;
					structcpid.AverageRAC = NetworkAvg;
					structcpid.NetworkProjects = NetworkProjects;

					mvNetwork["NETWORK"] = structcpid;
					// Tally total magnitudes for each cpid:

					bNetAveragesLoaded = true;
					if (fDebug) printf("Done gathering\r\n");
					return true;
		
	}
	catch (std::exception &e) 
	{
	    printf("Error while tallying network averages.\r\n");
		bNetAveragesLoaded=true;
	}
    catch(...)
	{
		printf("Error while tallying network averages. [1]\r\n");
		bNetAveragesLoaded=true;
	}
	bNetAveragesLoaded=true;
	return false;

}








void PrintBlockTree()
{
    AssertLockHeld(cs_main);
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = (*mi).second;
        mapNext[pindex->pprev].push_back(pindex);
        //while (rand() % 3 == 0)
        //    mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++)
                printf("| ");
            printf("|\\\n");
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++)
                printf("| ");
            printf("|\n");
       }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            printf("| ");

        // print item
        CBlock block;
        block.ReadFromDisk(pindex);
        printf("%d (%u,%u) %s  %08x  %s  mint %7s  tx %"PRIszu"",
            pindex->nHeight,
            pindex->nFile,
            pindex->nBlockPos,
            block.GetHash().ToString().c_str(),
            block.nBits,
            DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str(),
            FormatMoney(pindex->nMint).c_str(),
            block.vtx.size());

        PrintWallets(block);

        // put the main time-chain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (unsigned int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
    }
}

bool LoadExternalBlockFile(FILE* fileIn)
{
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    {
        LOCK(cs_main);
        try {
            CAutoFile blkdat(fileIn, SER_DISK, CLIENT_VERSION);
            unsigned int nPos = 0;
            while (nPos != (unsigned int)-1 && blkdat.good() && !fRequestShutdown)
            {
                unsigned char pchData[65536];
                do {
                    fseek(blkdat, nPos, SEEK_SET);
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat);
                    if (nRead <= 8)
                    {
                        nPos = (unsigned int)-1;
                        break;
                    }
                    void* nFind = memchr(pchData, pchMessageStart[0], nRead+1-sizeof(pchMessageStart));
                    if (nFind)
                    {
                        if (memcmp(nFind, pchMessageStart, sizeof(pchMessageStart))==0)
                        {
                            nPos += ((unsigned char*)nFind - pchData) + sizeof(pchMessageStart);
                            break;
                        }
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                    }
                    else
                        nPos += sizeof(pchData) - sizeof(pchMessageStart) + 1;
                } while(!fRequestShutdown);
                if (nPos == (unsigned int)-1)
                    break;
                fseek(blkdat, nPos, SEEK_SET);
                unsigned int nSize;
                blkdat >> nSize;
                if (nSize > 0 && nSize <= MAX_BLOCK_SIZE)
                {
                    CBlock block;
                    blkdat >> block;
                    if (ProcessBlock(NULL,&block,false))
                    {
                        nLoaded++;
                        nPos += 4 + nSize;
                    }
                }
            }
        }
        catch (std::exception &e) {
            printf("%s() : Deserialize or I/O error caught during load\n",
                   __PRETTY_FUNCTION__);
        }
    }
    printf("Loaded %i blocks from external file in %"PRId64"ms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

extern map<uint256, CAlert> mapAlerts;
extern CCriticalSection cs_mapAlerts;

string GetWarnings(string strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (GetBoolArg("-testsafemode"))
        strRPC = "test";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    // if detected invalid checkpoint enter safe mode
    if (Checkpoints::hashInvalidCheckpoint != 0)
    {
			

		if (CHECKPOINT_DISTRIBUTED_MODE==1)
		{	
			//10-18-2014-Halford- If invalid checkpoint found, reboot the node:
			//printf("Rebooting...");
			printf("Moving Gridcoin into Checkpoint ADVISORY mode.\r\n");
			CheckpointsMode = Checkpoints::ADVISORY;
	
		}
		else
		{

			#if defined(WIN32) && defined(QT_GUI)
				int nResult = 0;
	    		std::string rebootme = "";
				if (mapArgs.count("-reboot"))
				{
					rebootme = GetArg("-reboot", "false");
				}
				if (rebootme == "true")
				{
					nResult = RebootClient();
					printf("Rebooting %u",nResult);
				}
			#endif
	
			nPriority = 3000;
			strStatusBar = strRPC = _("WARNING: Invalid checkpoint found! Displayed transactions may not be correct! You may need to upgrade, or notify developers.");
			printf("WARNING: Invalid checkpoint found! Displayed transactions may not be correct! You may need to upgrade, or notify developers.");
		}
		

    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority)
            {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
                if (nPriority > 1000)
                    strRPC = strStatusBar;
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}








//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool static AlreadyHave(CTxDB& txdb, const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:
        {
        bool txInMap = false;
        txInMap = mempool.exists(inv.hash);
        return txInMap ||
               mapOrphanTransactions.count(inv.hash) ||
               txdb.ContainsTx(inv.hash);
        }

    case MSG_BLOCK:
        return mapBlockIndex.count(inv.hash) ||
               mapOrphanBlocks.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}



std::string GetLastBlockGRCAddress()
{
    CBlock block;
    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexBest, true);
	block.ReadFromDisk(pindexPrev);
	std::string hb = "";
	if (block.vtx.size() > 0) hb = block.vtx[0].hashBoinc;
	MiningCPID bb = DeserializeBoincBlock(hb);
	return bb.GRCAddress;
}

bool AmIGeneratingBackToBackBlocks()
{
	std::string wallet1 = GetLastBlockGRCAddress();
	std::string mywallet = DefaultWalletAddress();
	if (wallet1.length() > 3 && mywallet.length() > 3)
	{
			if (wallet1==mywallet) return true;
	}
	return false;
}

bool AcidTest(std::string precommand, std::string acid, CNode* pfrom)
{
	std::vector<std::string> vCommand = split(acid,",");
	if (vCommand.size() >= 6)
	{
		std::string sboinchashargs = DefaultOrgKey(12);  //Use 12 characters for inter-client communication
		std::string nonce =          vCommand[0];
		std::string command =        vCommand[1];
		std::string hash =           vCommand[2]; 
		std::string org =            vCommand[3];
		std::string pub_key_prefix = vCommand[4];
		std::string bhrn =           vCommand[5]; 
		std::string grid_pass =      vCommand[6];
		std::string grid_pass_decrypted = AdvancedDecryptWithSalt(grid_pass,sboinchashargs);
		
		if (LessVerbose(100)) printf("*Org:%s; ",pub_key_prefix.c_str());
		if (fDebug)	printf("*Org:%s;K:%s ",org.c_str(),pub_key_prefix.c_str());

		if (grid_pass_decrypted != bhrn+nonce+org+pub_key_prefix) 
		{
			if (fDebug) printf("Decrypted gridpass %s <> hashed message",grid_pass_decrypted.c_str());
			nonce="";
			command="";
		}
		
		std::string pw1 = RetrieveMd5(nonce+","+command+","+org+","+pub_key_prefix+","+sboinchashargs);

		if (precommand=="aries") 
		{
			//pfrom->securityversion = pw1;
		}
		if (fDebug) printf(" Nonce %s,comm %s,hash %s,pw1 %s \r\n",nonce.c_str(),command.c_str(),hash.c_str(),pw1.c_str());
		//If timestamp too old; disconnect
		double timediff = std::abs(GetAdjustedTime() - cdbl(nonce,0));
		if (timediff > 12*60 && GetAdjustedTime() > 1410913403)
		{
			printf("Network time attack [2] timediff %f  nonce %f    localtime %f",timediff,(double)cdbl(nonce,0),(double)GetAdjustedTime());
			pfrom->Misbehaving(25);
			pfrom->fDisconnect = true;
		}

		if (hash != pw1)
		{
			if (fDebug2) printf("Acid test failed for %s %s.",NodeAddress(pfrom).c_str(),acid.c_str());
			double punishment = GetArg("-punishment", 10);
			pfrom->Misbehaving(punishment);
			return false;
		}
		return true;
	}
	else
	{
		if (fDebug2) printf("Message corrupted. Node %s partially banned.",NodeAddress(pfrom).c_str());
		pfrom->Misbehaving(10);
		return false;
	}
	return true;
}




// The message start string is designed to be unlikely to occur in normal data.
// The characters are rarely used upper ASCII, not valid as UTF-8, and produce
// a large 4-byte int at any alignment.
unsigned char pchMessageStart[4] = { 0x70, 0x35, 0x22, 0x05 };


std::string NodeAddress(CNode* pfrom)
{
    CAddress addrThem = GetLocalAddress(&pfrom->addr);
	std::string ip = addrThem.ToString();
	return ip;
}



bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv, int64_t nTimeReceived)
{
    static map<CService, CPubKey> mapReuseKey;
    RandAddSeedPerfmon();
    if (fDebug)
        printf("received: %s (%"PRIszu" bytes)\n", strCommand.c_str(), vRecv.size());
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        printf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }

	//12-26-2014 Halford : Message Attacks ////////////////////////////////////
	//boost::replace_all(strCommand, " ", "");

	std::string precommand = strCommand;

	///////////////////////////////////////////////////////////////////////////


    if (strCommand == "aries")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->Misbehaving(10);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
		std::string acid = "";
		vRecv >> pfrom->nVersion >> pfrom->boinchashnonce >> pfrom->boinchashpw >> pfrom->cpid >> pfrom->enccpid >> acid >> pfrom->nServices >> nTime >> addrMe;

		//Halford - 12-26-2014 - Thwart Hackers
		bool ver_valid = AcidTest(strCommand,acid,pfrom);
        if (fDebug) printf("Ver Acid %s, Validity %s ",acid.c_str(),YesNo(ver_valid).c_str());
		if (!ver_valid)
		{
			//1-2-2015
			//double punishment = GetArg("-punishment", 10);
		    pfrom->Misbehaving(100);
		    pfrom->fDisconnect = true;
            return false;
		}

		bool unauthorized = false;
		double timedrift = std::abs(GetAdjustedTime() - nTime);
		
		if (true)
		{
			if (timedrift > (8*60))
			{
				printf("Disconnecting unauthorized peer with Network Time so far off by %f seconds!\r\n",(double)timedrift);
				unauthorized = true;
			}
		}
		else
		{
			if (timedrift > (10*60) && LessVerbose(500))
			{
				printf("Disconnecting authorized peer with Network Time so far off by %f seconds!\r\n",(double)timedrift);
				unauthorized = true;
			}
		}

		if (unauthorized)
		{
			printf("Disconnected unauthorized peer.         ");
			
			//double punishment1 = GetArg("-punishment", 20);
			
            pfrom->Misbehaving(100);
		    pfrom->fDisconnect = true;
            return false;
        }


        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION)
        {
            // disconnect from peers older than this proto version
            if (fDebug) printf("partner %s using obsolete version %i; disconnecting\n", pfrom->addr.ToString().c_str(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

		

        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty())
            vRecv >> pfrom->strSubVer;

		
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;

	
	 		if (GetArgument("autoban2","false") == "true")
			{
				if (pfrom->nStartingHeight < 1000 && LessVerbose(500)) 
				{
					if (fDebug) printf("Node with low height");
					pfrom->Misbehaving(10);
					pfrom->fDisconnect=true;
					return false;
				}
	
				if (pfrom->nStartingHeight < 1 && LessVerbose(700)) 
				{
					pfrom->Misbehaving(10);
			    	pfrom->fDisconnect=true;
					return false;
				}

				if (pfrom->nStartingHeight < 1 && pfrom->nServices == 0)
				{
					pfrom->Misbehaving(10);
			    	pfrom->fDisconnect=true;
					return false;
				}
			}
		
		
		if (pfrom->fInbound && addrMe.IsRoutable())
        {
            pfrom->addrLocal = addrMe;
            SeenLocal(addrMe);
        }

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {
            if (fDebug) printf("connected to self at %s, disconnecting\n", pfrom->addr.ToString().c_str());
            pfrom->fDisconnect = true;
            return true;
        }

        // record my external IP reported by peer
        if (addrFrom.IsRoutable() && addrMe.IsRoutable())
            addrSeenByPeer = addrMe;

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        if (GetBoolArg("-synctime", true))
            AddTimeData(pfrom->addr, nTime);

        // Change version
        pfrom->PushMessage("verack");
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound)
        {
            // Advertise our address
            if (!fNoListen && !IsInitialBlockDownload())
            {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable())
                    pfrom->PushAddress(addr);
            }

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000)
            {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            addrman.Good(pfrom->addr);
        } else {
            if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom)
            {
                addrman.Add(addrFrom, addrFrom);
                addrman.Good(addrFrom);
            }
        }

        // Ask the first connected node for block updates
        static int nAskedForBlocks = 0;
        if (!pfrom->fClient && !pfrom->fOneShot &&
            (pfrom->nStartingHeight > (nBestHeight - 144)) &&
            (pfrom->nVersion < NOBLKS_VERSION_START ||
             pfrom->nVersion >= NOBLKS_VERSION_END) &&
             (nAskedForBlocks < 1 || vNodes.size() <= 1))
        {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(pindexBest, uint256(0));
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
                item.second.RelayTo(pfrom);
        }

        // Relay sync-checkpoint
        {
            LOCK(Checkpoints::cs_hashSyncCheckpoint);
            if (!Checkpoints::checkpointMessage.IsNull())
                Checkpoints::checkpointMessage.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        if (fDebug) printf("receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s\n", pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString().c_str(), addrFrom.ToString().c_str(), pfrom->addr.ToString().c_str());

        cPeerBlockCounts.input(pfrom->nStartingHeight);

        // ppcoin: ask for pending sync-checkpoint if any
        if (!IsInitialBlockDownload())
            Checkpoints::AskForPendingSyncCheckpoint(pfrom);
    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else 1-10-2015 Halford
		printf("Hack attempt from %s - %s (banned) \r\n",pfrom->addrName.c_str(),NodeAddress(pfrom).c_str());
        pfrom->Misbehaving(100);
		pfrom->fDisconnect=true;
        return false;
    }


    else if (strCommand == "verack")
    {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
    }


    else if (strCommand == "gridaddr")
    { 
		//addr->gridaddr
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000)
        {
            pfrom->Misbehaving(10);
            return error("message addr size() = %"PRIszu"", vAddr.size());
        }


		if (pfrom->nStartingHeight < 1 && LessVerbose(500)) return true;

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        BOOST_FOREACH(CAddress& addr, vAddr)
        {
            if (fShutdown)
                return true;
            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);

			bool bad_node = (pfrom->nStartingHeight < 1 && LessVerbose(700));

			
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable() && !bad_node)
            {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ (( GetAdjustedTime() +hashAddr)/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }

    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(10);
            return error("message inv size() = %"PRIszu"", vInv.size());
        }

        // find last block in inv vector
        unsigned int nLastBlock = (unsigned int)(-1);
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK) {
                nLastBlock = vInv.size() - 1 - nInv;
                break;
            }
        }
        CTxDB txdb("r");
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(txdb, inv);
            if (fDebug)
                printf("  got inventory: %s  %s\n", inv.ToString().c_str(), fAlreadyHave ? "have" : "new");

            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]));
            } else if (nInv == nLastBlock) {
                // In case we are on a very long side-chain, it is possible that we already have
                // the last block in an inv bundle sent in response to getblocks. Try to detect
                // this situation and push another getblocks to continue.
                pfrom->PushGetBlocks(mapBlockIndex[inv.hash], uint256(0));
                if (fDebug)
                    printf("force request: %s\n", inv.ToString().c_str());
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(10);
            return error("message getdata size() = %"PRIszu"", vInv.size());
        }

        if (fDebugNet || (vInv.size() != 1))
		{
            if (fDebug)  printf("received getdata (%"PRIszu" invsz)\n", vInv.size());
		}

        BOOST_FOREACH(const CInv& inv, vInv)
        {
            if (fShutdown)
                return true;
            if (fDebugNet || (vInv.size() == 1))
			{
              if (fDebug)   printf("received getdata for: %s\n", inv.ToString().c_str());
			}

            if (inv.type == MSG_BLOCK)
            {
                // Send block from disk
                map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    CBlock block;
                    block.ReadFromDisk((*mi).second);
					//HALFORD 12-26-2014
					std::string acid = GetCommandNonce("encrypt");
                    pfrom->PushMessage("encrypt", block, acid);

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue)
                    {
                        // ppcoin: send latest proof-of-work block to allow the
                        // download node to accept as orphan (proof-of-stake 
                        // block might be rejected by stake connection check)
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, GetLastBlockIndex(pindexBest, false)->GetBlockHash()));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue = 0;
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                    CTransaction tx;
                    if (mempool.lookup(inv.hash, tx)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                    }
                }
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }


    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = locator.GetBlockIndex();

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        int nLimit = 2000;
		

        if (fDebug)    printf("getblocks %d to %s limit %d\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20).c_str(), nLimit);

        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                if (fDebug) printf("getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20).c_str());
                // ppcoin: tell downloading node about the latest block if it's
                // without risk being rejected due to stake connection check
                if (hashStop != hashBestChain && pindex->GetBlockTime() + nStakeMinAge > pindexBest->GetBlockTime())
                    pfrom->PushInventory(CInv(MSG_BLOCK, hashBestChain));
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0)
            {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                if (fDebug) printf("  getblocks stopping at limit %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20).c_str());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }
    else if (strCommand == "checkpoint")
    {
		//11-23-2014 - Receive Checkpoint from other nodes:
        CSyncCheckpoint checkpoint;
        vRecv >> checkpoint;
		//Checkpoint received from node with more than 1 Million GRC:
		
		if (CHECKPOINT_DISTRIBUTED_MODE==0 || CHECKPOINT_DISTRIBUTED_MODE==1)
		{
			if (checkpoint.ProcessSyncCheckpoint(pfrom))
			{
				// Relay
				pfrom->hashCheckpointKnown = checkpoint.hashCheckpoint;
				LOCK(cs_vNodes);
				BOOST_FOREACH(CNode* pnode, vNodes)
					checkpoint.RelayTo(pnode);
			}
		}
		else if (CHECKPOINT_DISTRIBUTED_MODE == 2)
		{
			// R HALFORD: One of our global GRC nodes solved a PoR block, store the last blockhash in memory
			muGlobalCheckpointHash = checkpoint.hashCheckpointGlobal;
			muGlobalCheckpointHashCounter=0;
			// Relay
			pfrom->hashCheckpointKnown = checkpoint.hashCheckpointGlobal;
			//Prevent broadcast storm: If not broadcast yet, relay the checkpoint globally:
			if (muGlobalCheckpointHashRelayed != checkpoint.hashCheckpointGlobal && checkpoint.hashCheckpointGlobal != 0)
			{
				LOCK(cs_vNodes);
				BOOST_FOREACH(CNode* pnode, vNodes)
				{
					checkpoint.RelayTo(pnode);
				}
				if (LessVerbose(50))
				{
						printf("RX-GlobCk:LBH %s %s\r\n",	checkpoint.SendingWalletAddress.c_str(), checkpoint.hashCheckpointGlobal.GetHex().c_str());
				}
			}
		}
    }

    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = (*mi).second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = locator.GetBlockIndex();
            if (pindex)
                pindex = pindex->pnext;
        }

        vector<CBlock> vHeaders;
        int nLimit = 2000;
        printf("getheaders %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20).c_str());
        for (; pindex; pindex = pindex->pnext)
        {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        pfrom->PushMessage("headers", vHeaders);
    }


    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        bool fMissingInputs = false;
        if (AcceptToMemoryPool(mempool, tx, &fMissingInputs))
        {
            SyncWithWallets(tx, NULL, true);
            RelayTransaction(tx, inv.hash);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);
		

            // Recursively process any orphan transactions that depended on this one
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (set<uint256>::iterator mi = mapOrphanTransactionsByPrev[hashPrev].begin();
                     mi != mapOrphanTransactionsByPrev[hashPrev].end();
                     ++mi)
                {
                    const uint256& orphanTxHash = *mi;
                    CTransaction& orphanTx = mapOrphanTransactions[orphanTxHash];
                    bool fMissingInputs2 = false;

                    if (AcceptToMemoryPool(mempool, orphanTx, &fMissingInputs2))
                    {
                        printf("   accepted orphan tx %s\n", orphanTxHash.ToString().substr(0,10).c_str());
                        SyncWithWallets(tx, NULL, true);
                        RelayTransaction(orphanTx, orphanTxHash);
                        mapAlreadyAskedFor.erase(CInv(MSG_TX, orphanTxHash));
                        vWorkQueue.push_back(orphanTxHash);
                        vEraseQueue.push_back(orphanTxHash);
                    }
                    else if (!fMissingInputs2)
                    {
                        // invalid orphan
                        vEraseQueue.push_back(orphanTxHash);
                        printf("   removed invalid orphan tx %s\n", orphanTxHash.ToString().substr(0,10).c_str());
                    }
                }
            }

            BOOST_FOREACH(uint256 hash, vEraseQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            AddOrphanTx(tx);

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nEvicted = LimitOrphanTxSize(MAX_ORPHAN_TRANSACTIONS);
            if (nEvicted > 0)
                printf("mapOrphan overflow, removed %u tx\n", nEvicted);
        }
        if (tx.nDoS) pfrom->Misbehaving(tx.nDoS);
    }


    else if (strCommand == "encrypt")
    {
		//12-26-2014 HALFORD

        CBlock block;
		std::string acid = "";
        vRecv >> block >> acid;

		bool block_valid = AcidTest(strCommand,acid,pfrom);
		if (!block_valid) return false;
		
        uint256 hashBlock = block.GetHash();
		if (fDebug) printf("Acid %s, Validity %s ",acid.c_str(),YesNo(block_valid).c_str());

        printf("Received block %s ; ", hashBlock.ToString().c_str());
        if (fDebug) block.print();

        CInv inv(MSG_BLOCK, hashBlock);
        pfrom->AddInventoryKnown(inv);

        if (ProcessBlock(pfrom, &block, false))
            mapAlreadyAskedFor.erase(inv);
        if (block.nDoS) pfrom->Misbehaving(block.nDoS);
    }


    else if (strCommand == "getaddr")
    {
        // Don't return addresses older than nCutOff timestamp
        int64_t nCutOff =  GetAdjustedTime() - (nNodeLifespan * 24 * 60 * 60);
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        BOOST_FOREACH(const CAddress &addr, vAddr)
            if(addr.nTime > nCutOff)
                pfrom->PushAddress(addr);
    }


    else if (strCommand == "mempool")
    {
        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        for (unsigned int i = 0; i < vtxid.size(); i++) {
            CInv inv(MSG_TX, vtxid[i]);
            vInv.push_back(inv);
            if (i == (MAX_INV_SZ - 1))
                    break;
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }


    else if (strCommand == "checkorder")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        if (!GetBoolArg("-allowreceivebyip"))
        {
            pfrom->PushMessage("reply", hashReply, (int)2, string(""));
            return true;
        }

        CWalletTx order;
        vRecv >> order;

        /// we have a chance to check the order here

        // Keep giving the same key to the same ip until they use it
        if (!mapReuseKey.count(pfrom->addr))
            pwalletMain->GetKeyFromPool(mapReuseKey[pfrom->addr], true);

        // Send back approval of order and pubkey to use
        CScript scriptPubKey;
        scriptPubKey << mapReuseKey[pfrom->addr] << OP_CHECKSIG;
        pfrom->PushMessage("reply", hashReply, (int)0, scriptPubKey);
    }


    else if (strCommand == "reply")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        CRequestTracker tracker;
        {
            LOCK(pfrom->cs_mapRequests);
            map<uint256, CRequestTracker>::iterator mi = pfrom->mapRequests.find(hashReply);
            if (mi != pfrom->mapRequests.end())
            {
                tracker = (*mi).second;
                pfrom->mapRequests.erase(mi);
            }
        }
        if (!tracker.IsNull())
            tracker.fn(tracker.param1, vRecv);
    }


    else if (strCommand == "ping")
    {
		std::string acid = "";
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce >> acid;
			bool pong_valid = AcidTest(strCommand,acid,pfrom);
			if (!pong_valid) return false;
			if (fDebug) printf("pong valid %s",YesNo(pong_valid).c_str());

            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }


    else if (strCommand == "pong")
    {
        int64_t pingUsecEnd = nTimeReceived;
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;
	    if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime >= -1) {
                        // Successful ping time measurement, replace previous
                        pfrom->nPingUsecTime = pingUsecTime;
                    } else {
                        // This should never happen
                        sProblem = "Timing mishap";
                    }
                } else {
                    // Nonce mismatches are normal when pings are overlapping
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
                        // This is most likely a bug in another implementation somewhere, cancel this ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
            // This is most likely a bug in another implementation somewhere, cancel this ping
            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            printf("pong %s %s: %s, %"PRIx64" expected, %"PRIx64" received, %u bytes\n"
                , pfrom->addr.ToString().c_str()
                , pfrom->strSubVer.c_str()
                , sProblem.c_str()
                , pfrom->nPingNonceSent
                , nonce
                , nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }


    else if (strCommand == "alert")
    {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0)
        {
            if (alert.ProcessAlert())
            {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                        alert.RelayTo(pnode);
                }
            }
            else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                pfrom->Misbehaving(10);
            }
        }
    }


    else
    {
        // Ignore unknown commands for extensibility
    }

	//12-26-2014 Halford (grid-version,grid-addr)

    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "aries" || strCommand == "gridaddr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
            AddressCurrentlyConnected(pfrom->addr);


    return true;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
    //if (fDebug)
    //    printf("ProcessMessages(%zu messages)\n", pfrom->vRecvMsg.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        //if (fDebug)
        //    printf("ProcessMessages(message %u msgsz, %zu bytes, complete:%s)\n",
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, pchMessageStart, sizeof(pchMessageStart)) != 0) {
            printf("\n\nPROCESSMESSAGE: INVALID MESSAGESTART\n\n");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid())
        {
            printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum)
        {
            printf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
               strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try
        {
            {
                LOCK(cs_main);
                fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime);
            }
            if (fShutdown)
                break;
        }
        catch (std::ios_base::failure& e)
        {
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
		{
           if (fDebug)   printf("ProcessMessage(%s, %u bytes) FAILED\n", strCommand.c_str(), nMessageSize);
		}
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}



double checksum(std::string s) 
{
	char ch;
	double chk = 0;
	std::string sOut = "";
	for (unsigned int i=0;i < s.length(); i++) 
	{
		ch = s.at(i);
		int ascii = ch;
		chk=chk+ascii;
	}
	return chk;
}






 uint256 GetScryptHashString(std::string s)
 {
        uint256 thash = 0;
        //scrypt_1024_1_1_256(BEGIN(s), END(s));
		thash = Hash(BEGIN(s),END(s));
        return thash;
 }




uint256 GridcoinMultipleAlgoHash(std::string t1)
{
        uint256 thash = 0;
    	std::string& t2 = t1;
		thash = Hash(t2.begin(),t2.begin()+t2.length());
		return thash;
}




std::string aes_complex_hash(uint256 scrypt_hash)
{
	  if (scrypt_hash==0) return "0";
	  
			std::string	sScryptHash = scrypt_hash.GetHex();
			std::string	sENCAES512 = AdvancedCrypt(sScryptHash);
			double chk = checksum(sENCAES512);
			uint256     hashSkein = GridcoinMultipleAlgoHash(sScryptHash);
			hashSkein = hashSkein + chk;
			std::string sSkeinAES512 = hashSkein.GetHex();
			return sSkeinAES512;
}

int TestAESHash(double rac, unsigned int diffbytes, uint256 scrypt_hash, std::string aeshash)
{
		std::size_t fAES = 0;
		std::string ractarget = RacStringFromDiff(rac,diffbytes);
		std::string skeinhash = "";
		if (aeshash=="") 
			{
				printf("TestAESHash::AESHash is empty\r\n");
				return -3;
		}

		if (scrypt_hash > 0) 
		{
			skeinhash=aes_complex_hash(scrypt_hash);
			if (aeshash != skeinhash) 
			{
				//printf("\r\nTestAESHash: Failure:  RacTarget %s purported aeshash %s, calculated aeshash %s", ractarget.c_str(), 					aeshash.c_str(), skeinhash.c_str());
				return -1;
			}
		}
		fAES = aeshash.find(ractarget);
		if (fAES == std::string::npos) return -2;
		return 0;
}


double LederstrumpfMagnitude_Retired(double Magnitude, int64_t locktime)
{
   // Establish constants
    double e = 2.718;
	double v = 4;
	double r = 0.915;
	double x = 0;
	double new_magnitude = 0;
	double user_magnitude_cap = 3000; //This is where the full 500 magnitude is achieved
	//Function returns a new magnitude between Magnitudes > 80% of Cap and <= Cap;
	double MagnitudeCap_LowSide = GetMaximumBoincSubsidy(locktime)*.80;
	if (Magnitude < MagnitudeCap_LowSide) return Magnitude;
	x = Magnitude / (user_magnitude_cap);
	new_magnitude = ((MagnitudeCap_LowSide/2) / (1 + pow((double)e, (double)(-v * (x - r))))) + MagnitudeCap_LowSide;
	//Debug.Print "With RAC of " + Trim(Rac) + ", NetRac of " + Trim(NetworkRac) + ", Subsidy = " + Trim(Subsidy)
	if (new_magnitude < MagnitudeCap_LowSide) new_magnitude=MagnitudeCap_LowSide;
	if (new_magnitude > GetMaximumBoincSubsidy(locktime)) new_magnitude=GetMaximumBoincSubsidy(locktime);
	return new_magnitude;
}


double LederstrumpfMagnitude2(double Magnitude, int64_t locktime)
{
	//2-1-2015 - Halford - Set MagCap to 2000
	double MagCap = 2000;
	double out_mag = Magnitude;
	if (Magnitude >= MagCap*.90 && Magnitude <= MagCap*1.0) out_mag = MagCap*.90;
	if (Magnitude >= MagCap*1.0 && Magnitude <= MagCap*1.1) out_mag = MagCap*.91;
	if (Magnitude >= MagCap*1.1 && Magnitude <= MagCap*1.2) out_mag = MagCap*.92;
	if (Magnitude >= MagCap*1.2 && Magnitude <= MagCap*1.3) out_mag = MagCap*.93;
	if (Magnitude >= MagCap*1.3 && Magnitude <= MagCap*1.4) out_mag = MagCap*.94;
	if (Magnitude >= MagCap*1.4 && Magnitude <= MagCap*1.5) out_mag = MagCap*.95;
	if (Magnitude >= MagCap*1.5 && Magnitude <= MagCap*1.6) out_mag = MagCap*.96;
	if (Magnitude >= MagCap*1.6 && Magnitude <= MagCap*1.7) out_mag = MagCap*.97;
	if (Magnitude >= MagCap*1.7 && Magnitude <= MagCap*1.8) out_mag = MagCap*.98;
	if (Magnitude >= MagCap*1.8 && Magnitude <= MagCap*1.9) out_mag = MagCap*.99;
	if (Magnitude >  MagCap*2.0)						    out_mag = MagCap*1.0;
	return out_mag;

}



bool Contains(std::string data, std::string instring)
{
	std::size_t found = 0;
	found = data.find(instring);
	if (found != std::string::npos) return true;
	return false;
}


std::string NN(std::string value)
{
	return value.empty() ? "" : value;
}

std::string SerializeBoincBlock(MiningCPID mcpid)
{
	std::string delim = "<|>";
	std::string version = FormatFullVersion();
	mcpid.GRCAddress = DefaultWalletAddress();
	mcpid.Organization = DefaultOrg();
	mcpid.OrganizationKey = DefaultBlockKey(8); //Only reveal 8 characters of the key in the block (should be enough to identify hackers)
	
	if (mcpid.lastblockhash.empty()) mcpid.lastblockhash = "0";
	std::string bb = mcpid.cpid + delim + mcpid.projectname + delim + mcpid.aesskein + delim + RoundToString(mcpid.rac,0)
					+ delim + RoundToString(mcpid.pobdifficulty,5) + delim + RoundToString((double)mcpid.diffbytes,0) 
					+ delim + NN(mcpid.enccpid) 
					+ delim + NN(mcpid.encaes) + delim + RoundToString(mcpid.nonce,0) + delim + RoundToString(mcpid.NetworkRAC,0) 
					+ delim + NN(version)
					+ delim + RoundToString(mcpid.ResearchSubsidy,2) + delim 
					+ RoundToString(mcpid.LastPaymentTime,0) + 
					delim + RoundToString(mcpid.RSAWeight,0) 
					+ delim + NN(mcpid.cpidv2)
					+ delim + RoundToString(mcpid.Magnitude,0)
					+ delim + NN(mcpid.GRCAddress) + delim + NN(mcpid.lastblockhash)
					+ delim + RoundToString(mcpid.InterestSubsidy,2) + delim + NN(mcpid.Organization) + delim + NN(mcpid.OrganizationKey);
	return bb;
}


std::string SerializeBoincBlock_Old(std::string cpid, std::string projectname, std::string AESSkein, double RAC,
	double PoBDifficulty, unsigned int diffbytes, std::string enccpid, std::string encaes, double nonce, double NetworkRAC, 
	double ResearchSubsidy, double vouched_magnitude, double vouched_rac, double vouched_network_rac)
{
	std::string delim = "<|>";

	if (PoBDifficulty > 99) PoBDifficulty=99;
	std::string version = FormatFullVersion();
	std::string bb = cpid + delim + projectname + delim + AESSkein + delim + RoundToString(RAC,0)
					+ delim + RoundToString(PoBDifficulty,5) + delim + RoundToString((double)diffbytes,0) + delim + enccpid 
					+ delim + encaes + delim + RoundToString(nonce,0) + delim + RoundToString(NetworkRAC,0) + delim + version 
					+ delim + RoundToString(ResearchSubsidy,2) + delim + RoundToString(vouched_magnitude,0) + delim + RoundToString(vouched_rac,0) 
					+ delim + RoundToString(vouched_network_rac,0);

	return bb;
}

MiningCPID DeserializeBoincBlock(std::string block)
{
	MiningCPID surrogate = GetMiningCPID();
	try 
	{

	std::vector<std::string> s = split(block,"<|>");
	if (s.size() > 7)
	{
		surrogate.cpid = s[0];
		surrogate.projectname = s[1];
		boost::to_lower(surrogate.projectname);
      	surrogate.aesskein = s[2];
		surrogate.rac = cdbl(s[3],0);
		surrogate.pobdifficulty = cdbl(s[4],6);
		surrogate.diffbytes = (unsigned int)cdbl(s[5],0);
		surrogate.enccpid = s[6];
		surrogate.encboincpublickey = s[6];
		surrogate.encaes = s[7];
		surrogate.nonce = cdbl(s[8],0);
		if (s.size() > 9) 
		{
			surrogate.NetworkRAC = cdbl(s[9],0);
		}
		if (s.size() > 10)
		{
			surrogate.clientversion = s[10];
		}
		if (s.size() > 11)
		{
			surrogate.ResearchSubsidy = cdbl(s[11],2);
		}
		if (s.size() > 12)
		{
			surrogate.LastPaymentTime = cdbl(s[12],0);
		}
		if (s.size() > 13)
		{
			surrogate.RSAWeight = cdbl(s[13],0);
		}
		if (s.size() > 14)
		{
			surrogate.cpidv2 = s[14];
		}
		if (s.size() > 15)
		{
			surrogate.Magnitude = cdbl(s[15],0);
		}
		if (s.size() > 16)
		{
			surrogate.GRCAddress = s[16];
		}
		if (s.size() > 17)
		{
			surrogate.lastblockhash = s[17];
		}
		if (s.size() > 18)
		{
			surrogate.InterestSubsidy = cdbl(s[18],2);
		}
		if (s.size() > 19)
		{
			surrogate.Organization = s[19];
		}
		if (s.size() > 20)
		{
			surrogate.OrganizationKey = s[20];
		}
		
	}
	}
	catch (...)
	{
		    printf("Deserialize ended with an error (06182014) \r\n");
	}
	return surrogate;


}




void printbool(std::string comment, bool boo)
{
	if (boo)
	{

		printf("%s : TRUE",comment.c_str());
	}
	else
	{
		printf("%s : FALSE",comment.c_str());

	}

}

double GetMagnitude(std::string cpid, double purported, bool UseNetSoft)
{
	if (!UseNetSoft)
	{
		//First try the consensus:
		CreditCheck(cpid,false);
		StructCPID UntrustedHost = GetStructCPID();
		UntrustedHost = mvCreditNodeCPID[cpid]; //Contains Mag across entire CPID
		double magnitude_consensus = UntrustedHost.ConsensusMagnitude;
		if (magnitude_consensus >= purported) 
		{
				if (fDebug) printf("For cpid %s, using Consensus mag of %f\r\n",cpid.c_str(),magnitude_consensus);
				return magnitude_consensus;
		}
		//Try clearing the cache; call Netsoft
		CreditCheck(cpid,true);
		UntrustedHost = mvCreditNodeCPID[cpid]; //Contains Mag across entire CPID
		magnitude_consensus = UntrustedHost.Magnitude;
	    if (fDebug) printf("For cpid %s, using Netsoft mag of %f\r\n",cpid.c_str(),magnitude_consensus);
		return magnitude_consensus;
	}
	else
	{
		CreditCheck(cpid,false);
		StructCPID UntrustedHost = GetStructCPID();
		UntrustedHost = mvCreditNodeCPID[cpid]; //Contains Mag across entire CPID
		double mag = UntrustedHost.Magnitude;
		if (mag*TOLERANCE_PERCENT >= purported) 
		{
			if (fDebug) printf("For cpid %s, using NetSoft cached mag of %f\r\n",cpid.c_str(),mag);
			return mag;
		}
		//Try clearing the cache; call Netsoft
		for (int i = 0; i <= 3;i++)
		{
			CreditCheck(cpid,true);
			UntrustedHost = mvCreditNodeCPID[cpid]; //Contains Mag across entire CPID
			mag = UntrustedHost.Magnitude;
			printf("Attempt #%f ; For cpid %s, using NetSoft actual mag of %f\r\n",(double)i,cpid.c_str(),mag);
			if (mag*TOLERANCE_PERCENT >= purported) return mag;
		}
		return mag;
	
	}
			
}


bool IsUserQualifiedToSendCheckpoint()
{
	std::string cpid = GlobalCPUMiningCPID.cpid;
	//The User must have a boinc mining record with an accuracy > 20 to send checkpoints:
	if (cpid=="INVESTOR") return false;
	// Check the magnitude report
	StructCPID mag = GetStructCPID();
	mag = mvMagnitudes[cpid];
	if (mag.initialized) 
	{
			if (mag.Accuracy > 20)
			{
				if (mag.ConsensusMagnitude > 50)
				{
					return true;
				}
	
			}
	}
	return false;
}

bool CreditCheckOnline(std::string cpid, double purported_magnitude, double mint, uint64_t nCoinAge, 
	uint64_t nFees, int64_t locktime, double RSAWeight)
{
	if (cpid=="INVESTOR" && purported_magnitude==0) return true;
	// First, check the magnitude report
	StructCPID mag = GetStructCPID();
	mag = mvMagnitudes[cpid];
	bool consensus_passed = false;
	if (mag.initialized && purported_magnitude > 0) 
	{
			if (mag.Accuracy > 10)
			{
				double consensus_magnitude = mag.ConsensusMagnitude;
				if (purported_magnitude <= (consensus_magnitude*TOLERANCE_PERCENT)) 
				{
					consensus_passed = true;
				}
	
			}
	}
	if (!consensus_passed && purported_magnitude==0) consensus_passed=true;

	if (!consensus_passed)
	{
		    double actual_magnitude = 0;

		    for (int i = 0; i <= 3; i++)
			{
				actual_magnitude = GetMagnitude(cpid,purported_magnitude,true);
				printf("CreditCheckOnline Try# %f: CPID: %s  PurportedMag %f, NetsoftMag %f",(double)i,cpid.c_str(),purported_magnitude,actual_magnitude);
				if (purported_magnitude <= (actual_magnitude*TOLERANCE_PERCENT)) break;
			}
			if (purported_magnitude > (actual_magnitude*TOLERANCE_PERCENT)) 
			{
					printf("Credit check online failed\r\n");
					return false;
			}
	}

	// If block is PoR, Resulting block must not exceed outstanding amount owed for this CPID:
	if (purported_magnitude > 0)
	{
		//TODO: Ensure Inflation portion of mint is compared properly (	COIN_YEAR_REWARD = APR )
		double OUT_POR = 0;
		double OUT_INTEREST=0;
		double owed = GetProofOfStakeReward(nCoinAge,nFees,cpid,true,locktime,OUT_POR,OUT_INTEREST,RSAWeight);
		if (mint > (owed*TOLERANCE_PERCENT))
		{
			printf("Credit Check Online failed:  Mint results in a payment > outstanding owed; owed %f - mint %f.",owed,mint);
			return false;
		}
	}
	return true;

}

void ClearCPID(std::string cpid)
{
	mvCPIDCache["cache"+cpid].initialized=false;
    mvCreditNodeCPID[cpid].initialized=false;
							
}

  

std::string ComputeCPIDv2(std::string email, std::string bpk, uint256 blockhash)
{
		//if (GetBoolArg("-disablecpidv2")) return "";
		CPID c = CPID();
		std::string cpid_non = bpk+email;
		std::string digest = c.CPID_V2(email,bpk,blockhash);
		return digest;
}



std::string boinc_hash(const std::string str)
{
	// Return the boinc hash of a string:
    CPID c = CPID(str);
    return c.hexdigest();
}




void InitializeProjectStruct(StructCPID& project)
{
	std::string email = GetArgument("email", "NA");
	boost::to_lower(email);

	project.email = email;
	std::string cpid_non = project.cpidhash+email;
	project.boincruntimepublickey = project.cpidhash;
	project.cpid = boinc_hash(cpid_non);

	std::string ENCbpk = AdvancedCrypt(cpid_non);
	project.boincpublickey = ENCbpk;
	project.cpidv2 = ComputeCPIDv2(email, project.cpidhash, 0);
	project.link = "http://boinc.netsoft-online.com/get_user.php?cpid=" + project.cpid;
	//Local CPID with struct
	//Must contain cpidv2, cpid, boincpublickey
	project.Iscpidvalid = false;
	project.Iscpidvalid = IsLocalCPIDValid(project);
 	if (project.team != "gridcoin") 
	{
			project.Iscpidvalid = false;
			project.errors = "Team invalid";
	}
	if (fDebug) printf("Assimilating local project %s, Valid %s",project.projectname.c_str(),YesNo(project.Iscpidvalid).c_str());


}

void AddProjectFromNetSoft(StructCPID& netsoft)
{
	if (netsoft.cpid != GlobalCPUMiningCPID.cpid && GlobalCPUMiningCPID.cpid.length() > 3 && GlobalCPUMiningCPID.cpid != "INVESTOR") 
	{
		//Dont add it
		return;
	}

	
	StructCPID NewProject = GetStructCPID();
	NewProject.cpid = netsoft.cpid;
	NewProject.projectname = netsoft.projectname;
	NewProject.verifiedutc = netsoft.verifiedutc;
	NewProject.verifiedrac = netsoft.verifiedrac;
	NewProject.rac = netsoft.rac;
	NewProject.team = netsoft.verifiedteam;
	NewProject.verifiedteam = netsoft.verifiedteam;
	NewProject.initialized = true;
	NewProject.verifiedrectime = netsoft.verifiedrectime;
	NewProject.verifiedage = netsoft.verifiedage;
	NewProject.emailhash=netsoft.emailhash;
	NewProject.cpidhash = GlobalCPUMiningCPID.cpidhash;
	if (NewProject.verifiedteam != "gridcoin") NewProject.verifiedrac = -1;
	
	InitializeProjectStruct(NewProject);
		
	mvCPIDs.insert(map<string,StructCPID>::value_type(NewProject.projectname,NewProject));
	if (NewProject.rac > 100 && NewProject.Iscpidvalid)
	{
		mvCPIDs[netsoft.projectname] = NewProject;
		printf("Adding local project missing - from Netsoft %s %s\r\n",NewProject.projectname.c_str(),NewProject.cpid.c_str());
	}
    

}


void CreditCheck(std::string cpid, bool clearcache)
{
	try {

			std::string cc = GetHttpPage(cpid,true,clearcache);
			if (cc.length() < 50) 
			{
				if (fDebug) printf("Note: HTTP Page returned blank from netsoft for %s\r\n",cpid.c_str());
				return;
			}

			double projavg = 0;
						
			int iRow = 0;
			std::vector<std::string> vCC = split(cc.c_str(),"<project>");

			//printf("Performing credit check for %s",cpid.c_str());


			//Clear current magnitude
			StructCPID structMag = GetStructCPID();
			structMag = mvCreditNodeCPID[cpid];
			if (structMag.initialized)
			{
				structMag.TotalMagnitude = 0;
				structMag.MagnitudeCount=0;
				structMag.Magnitude = 0;
				structMag.verifiedTotalRAC = 0;
					
				mvCreditNodeCPID[cpid]=structMag;
			}

			if (vCC.size() > 1)
			{
				for (unsigned int i = 0; i < vCC.size(); i++)
				{
					std::string sProj  = ExtractXML(vCC[i],"<name>","</name>");
					std::string utc    = ExtractXML(vCC[i],"<total_credit>","</total_credit>");
					std::string rac    = ExtractXML(vCC[i],"<expavg_credit>","</expavg_credit>");
					std::string team   = ExtractXML(vCC[i],"<team_name>","</team_name>");
					std::string rectime= ExtractXML(vCC[i],"<expavg_time>","</expavg_time>");
					boost::to_lower(sProj);
					sProj = ToOfficialName(sProj);
					boost::to_lower(sProj);
					//1-11-2015 Rob Halford - List of Exceptions to Map Netsoft Name -> Boinc Client Name

					//6-21-2014 (R Halford) : In this convoluted situation, we found MindModeling@beta in the Boinc client, and MindModeling@Home in the CreditCheckOnline XML;
					if (sProj == "mindmodeling@home") sProj = "mindmodeling@beta";
					if (sProj == "Quake Catcher Network") sProj = "Quake-Catcher Network";

					//Is project Valid
					bool projectvalid = ProjectIsValid(sProj);
					if (!projectvalid) sProj = "";
					//printf("CreditCheck:Enumerating %s;",sProj.c_str());

					if (sProj.length() > 3) 
					{


						StructCPID structcc = GetStructCPID();
						structcc = mvCreditNode[sProj];
						iRow++;
						if (!structcc.initialized) 
						{
							structcc = GetStructCPID();
							structcc.initialized = true;
							mvCreditNode.insert(map<string,StructCPID>::value_type(sProj,structcc));
						} 
						structcc.cpid = cpid;
						structcc.projectname = sProj;
						structcc.verifiedutc = cdbl(utc,0);
						structcc.verifiedrac = cdbl(rac,0);
						boost::to_lower(team);
						structcc.verifiedteam = team;
						if (structcc.verifiedteam != "gridcoin") structcc.verifiedrac = -1;
						structcc.verifiedrectime = cdbl(rectime,0);
						double currenttime =  GetAdjustedTime();
						double nActualTimespan = currenttime - structcc.verifiedrectime;
						structcc.verifiedage = nActualTimespan;
						mvCreditNode[sProj] = structcc;	
						// Halford 8-17-2014
						// We found a project can exist in Netsoft and not in the client - ensure client has them all:
						StructCPID structClientProject = GetStructCPID();
						structClientProject = mvCPIDs[sProj];
						if (!structClientProject.initialized)
						{
							structClientProject = GetStructCPID();
							if (fDebug) printf("Calling netsoft for %s",sProj.c_str());
							AddProjectFromNetSoft(structcc);
						}
						//End of Adding to Client Project List
					

						//////////////////////////// Store this information by CPID+Project also:

						StructCPID structverify = GetStructCPID();

						std::string sKey = cpid + ":" + sProj;
						structverify = mvCreditNodeCPIDProject[sKey]; //Contains verified CPID+Projects;
						if (!structverify.initialized)
						{
							structverify = GetStructCPID();
							structverify.initialized = true;
							if (fDebug) printf("inserting Credit Node CPID Project %s",sKey.c_str());
							mvCreditNodeCPIDProject.insert(map<string,StructCPID>::value_type(sKey,structverify));
						}
						structverify.cpid = cpid;
						structverify.projectname = sProj;
						structverify.verifiedutc = cdbl(utc,0);
						structverify.verifiedrac = cdbl(rac,0);
						structverify.verifiedteam = team;
						structverify.verifiedrectime = cdbl(rectime,0);
						structverify.verifiedage = nActualTimespan;
						mvCreditNodeCPIDProject[sKey]=structverify;
						//Store this information by CPID also:
						StructCPID structc = GetStructCPID();
						structc = mvCreditNodeCPID[cpid]; //Contains verified total RAC for the entire CPID
						if (!structc.initialized)
						{
							structc = GetStructCPID();
							structc.initialized = true;
							structc.TotalMagnitude = 0;
							mvCreditNodeCPID.insert(map<string,StructCPID>::value_type(cpid,structc));
						}
						structc.cpid = cpid;
						structc.projectname = sProj;
						structc.verifiedutc = cdbl(utc,0);
						structc.verifiedrac = cdbl(rac,0);
						boost::to_lower(team);
						structc.team = team;
						structc.verifiedteam = team;
						structc.verifiedrectime = cdbl(rectime,0);
						structc.verifiedage = nActualTimespan;
						if (cdbl(rac,0) > 0)
						{
							structc.verifiedTotalRAC = structc.verifiedTotalRAC + cdbl(rac,0);
						}
						projavg=GetNetworkAvgByProject(sProj);

						//	bool including = (ProjectRAC > 1 && structcpid.rac > 100 && structcpid.Iscpidvalid && structcpid.verifiedrac > 100);
			
						if (projavg > 100 && structc.verifiedrac > 100 && structc.team == "gridcoin")
						{
							structc.verifiedTotalNetworkRAC = structc.verifiedTotalNetworkRAC + projavg;
							double project_magnitude = 0;
							double UserVerifiedRAC = structc.verifiedrac;
							if (UserVerifiedRAC < 0) UserVerifiedRAC = 0;
							project_magnitude = UserVerifiedRAC/(projavg+.01) * 100;
							structc.TotalMagnitude = structc.TotalMagnitude + project_magnitude;
							structc.MagnitudeCount++;
							structc.Magnitude = (structc.TotalMagnitude/(structc.MagnitudeCount+.01));
							//Halford 9-28-2014: Per Survey results, use Magnitude Calculation v2: Assess Magnitude based on all whitelisted projects
							double WhitelistedWithRAC = GetNetworkProjectCountWithRAC();
							structc.Magnitude = (structc.TotalMagnitude/WHITELISTED_PROJECTS) * WhitelistedWithRAC;

			


							mvCreditNodeCPID[cpid]=structc;
							if (fDebug) printf("Adding magnitude for project %s : ProjectAvgRAC %f, User RAC %f, new Magnitude %f\r\n",
								sProj.c_str(),
								projavg,
								structc.verifiedrac,
								structc.Magnitude);

						}
						mvCreditNodeCPID[cpid]=structc;
						
						/////////////////////////////
						//printf("Adding Credit Node Result %s",cpid.c_str());
					}
				}
			}
			
	}
	catch (std::exception &e) 
	{
			 printf("error while accessing credit check online.\r\n");
	}
    catch(...)
	{
			printf("Error While accessing credit check online (2).\r\n");
	}

	
}



double CreditCheck(std::string cpid, std::string projectname)
{
	std::string sKey = cpid + ":" + projectname;
	StructCPID structverify = GetStructCPID();
	structverify = mvCreditNodeCPIDProject[sKey]; //Contains verified CPID+Projects;
	if (!structverify.initialized)
	{
		structverify = GetStructCPID();
		CreditCheck(cpid,false);
	}
	structverify = mvCreditNodeCPIDProject[sKey]; //Contains verified CPID+Projects;
	if (!structverify.initialized) 
	{
		structverify = GetStructCPID();
		//Defer to the chain, as internet may be down:
		printf("Cannot reach credit check node... Checking main chain\r\n");
		std::string out_errors = "";
		int out_position = 0;
		bool InChain = FindRAC(false,cpid, projectname, 14, true, out_errors, out_position);
		if (InChain) return 99000;
		return 0;
	}
	else
	{
		double rac = structverify.verifiedrac;
		return rac;
	}
				
}



bool ProjectIsValid(std::string project)
{
	StructCPID structcpid = GetStructCPID();
	boost::to_lower(project);
	structcpid = mvBoincProjects[project];
	return structcpid.initialized;
			
}
  
std::string ToOfficialName(std::string proj)
{
			boost::to_lower(proj);
			//Convert local XML project name [On the Left] to official [Netsoft] projectname:
			if (proj=="boincsimap")             proj = "simap";
			if (proj=="pogs")                   proj = "theskynet pogs";
			if (proj=="convector.fsv.cvut.cz")  proj = "convector";
			if (proj=="distributeddatamining")  proj = "distributed data mining";
			if (proj=="distrrtgen")             proj = "distributed rainbow table generator";
			if (proj=="eon2")                   proj = "eon";
			if (proj=="test4theory@home")       proj = "test4theory";
			if (proj=="lhc@home")               proj = "lhc@home 1.0";
			if (proj=="mindmodeling@beta")      proj = "mindmodeling@beta";
			if (proj=="volpex@uh")              proj = "volpex";
			if (proj=="oproject")               proj = "oproject@home";

			return proj; 
}







std::string RacStringFromDiff(double RAC,unsigned int diffbytes)
{
     std::string rac1 = "00000000000000000000000000000000" + RoundToString(RAC,0);
	 if (diffbytes > rac1.length()) diffbytes=1;
	 std::string rac2 = rac1.substr(rac1.length()-diffbytes, diffbytes);
	 //printf("RacTarget: RAC %f Target: %s\r\n",RAC,rac2.c_str());
	 return rac2;
}


std::string GetBoincDataDir2()
{
	std::string path = "";
	/*       Default setting: boincdatadir=c:\\programdata\\boinc\\   */


    if (mapArgs.count("-boincdatadir")) 
	{
        path = mapArgs["-boincdatadir"];
		if (path.length() > 0) return path;
    } 

    #ifndef WIN32
    #ifdef __linux__
        path = "/var/lib/boinc-client/"; // Linux
    #else
        path = "/Library/Application Support/BOINC Data/"; // Mac OSX
    #endif
    #elif WINVER < 0x0600
        path = "c:\\documents and settings\\all users\\application data\\boinc\\"; // Windows XP
    #else
        path = "c:\\programdata\\boinc\\"; // Windows Vista and up
    #endif

    return path;
}


std::string GetArgument(std::string arg, std::string defaultvalue)
{
	std::string result = defaultvalue;
	if (mapArgs.count("-" + arg))
	{
		result = GetArg("-" + arg, defaultvalue);
	}
	return result;

}
	   



void HarvestCPIDs(bool cleardata)
{
	
	if (fDebug) printf("loading BOINC cpids ...\r\n");

	//Remote Boinc Feature - R Halford
	
	std::string sBoincKey = GetArgument("boinckey","");
   
	if (!sBoincKey.empty())
	{
		//Deserialize key into Global CPU Mining CPID
		printf("Using key %s \r\n",sBoincKey.c_str());
	
		std::string sDec=DecodeBase64(sBoincKey);
		printf("Using key %s \r\n",sDec.c_str());
	
	    if (sDec.empty()) printf("Error while deserializing boinc key!  Please use execute genboinckey to generate a boinc key from the host with boinc installed.\r\n");
		GlobalCPUMiningCPID = DeserializeBoincBlock(sDec);
		GlobalCPUMiningCPID.initialized = true;

		if (GlobalCPUMiningCPID.cpid.empty()) 
		{
				 printf("Error while deserializing boinc key!  Please use execute genboinckey to generate a boinc key from the host with boinc installed.\r\n");
		}
		else
		{
			printf("CPUMiningCPID Initialized.\r\n");
		}
		printf("Using Serialized Boinc CPID %s",GlobalCPUMiningCPID.cpid.c_str());

			StructCPID structcpid = GetStructCPID();
			structcpid.initialized = true;
			structcpid.cpidhash = GlobalCPUMiningCPID.cpidhash;
			structcpid.projectname = GlobalCPUMiningCPID.projectname;
			structcpid.team = "gridcoin"; //Will be verified later during Netsoft Call
			structcpid.verifiedteam = "gridcoin";
			structcpid.rac = GlobalCPUMiningCPID.rac;
			structcpid.cpid = GlobalCPUMiningCPID.cpid;
			structcpid.verifiedrac = GlobalCPUMiningCPID.rac; //Will be re-verified later
			structcpid.boincpublickey = GlobalCPUMiningCPID.encboincpublickey;
			structcpid.boincruntimepublickey = structcpid.cpidhash;
			structcpid.initialized = true;
			structcpid.NetworkRAC = GlobalCPUMiningCPID.NetworkRAC;
			
			structcpid.email = GlobalCPUMiningCPID.email;
			structcpid.cpidv2 = structcpid.cpid;

			structcpid.link = "http://boinc.netsoft-online.com/get_user.php?cpid=" + structcpid.cpid;
			structcpid.Iscpidvalid = true;
			mvCPIDs.insert(map<string,StructCPID>::value_type(structcpid.projectname,structcpid));

						   
			StructCPID structverify = GetStructCPID();
			std::string sKey = structcpid.cpid + ":" + structcpid.projectname;
			structverify = GetStructCPID();
			CreditCheck(structcpid.cpid,false);
			
	
			GetNextProject(false);
			if (fDebug) printf("GCMCPI %s",GlobalCPUMiningCPID.cpid.c_str());
			if (fDebug) 			printf("Finished getting first remote boinc project\r\n");
		return;
  }
       
 try 
 {

	std::string sourcefile = GetBoincDataDir2() + "client_state.xml";
    std::string sout = "";
    sout = getfilecontents(sourcefile);
	if (sout == "-1") 
	{
		printf("Unable to obtain Boinc CPIDs \r\n");

		if (mapArgs.count("-boincdatadir") && mapArgs["-boincdatadir"].length() > 0)
		{
			printf("Boinc data directory set in gridcoinresearch.conf has been incorrectly specified \r\n");
		}

		else printf("Boinc data directory is not in the operating system's default location \r\nPlease move it there or specify its current location in gridcoinresearch.conf \r\n");

		return;
	}

	if (cleardata)
	{
		mvCPIDs.clear();
		mvCreditNode.clear();
		mvCreditNodeCPID.clear();
		mvCreditNodeCPIDProject.clear();
		mvCPIDCache.clear();
	}
	std::string email = GetArgument("email","");
    boost::to_lower(email);

	int iRow = 0;
	std::vector<std::string> vCPID = split(sout.c_str(),"<project>");
	if (vCPID.size() > 0)
	{
	
		for (unsigned int i = 0; i < vCPID.size(); i++)
		{
			std::string email_hash = ExtractXML(vCPID[i],"<email_hash>","</email_hash>");
			std::string cpidhash = ExtractXML(vCPID[i],"<cross_project_id>","</cross_project_id>");
			std::string utc=ExtractXML(vCPID[i],"<user_total_credit>","</user_total_credit>");
			std::string rac=ExtractXML(vCPID[i],"<user_expavg_credit>","</user_expavg_credit>");
			std::string proj=ExtractXML(vCPID[i],"<project_name>","</project_name>");
			std::string team=ExtractXML(vCPID[i],"<team_name>","</team_name>");
			std::string rectime = ExtractXML(vCPID[i],"<rec_time>","</rec_time>");
			
			//Is project Valid
		
			boost::to_lower(proj);
            proj = ToOfficialName(proj);

			bool projectvalid = ProjectIsValid(proj);
			
			if (cpidhash.length() > 5 && proj.length() > 3) 
			{
				std::string cpid_non = cpidhash+email;
				to_lower(cpid_non);
								
				StructCPID structcpid = GetStructCPID();
				structcpid = mvCPIDs[proj];
				iRow++;
				if (!structcpid.initialized) 
				{
					structcpid = GetStructCPID();
					structcpid.initialized = true;
					mvCPIDs.insert(map<string,StructCPID>::value_type(proj,structcpid));
				} 
				
				structcpid.cpidhash = cpidhash;
				structcpid.projectname = proj;
				boost::to_lower(team);
				structcpid.team = team;
				InitializeProjectStruct(structcpid);
				if (fDebug) printf("Harv new project %s cpid %s valid %s",structcpid.projectname.c_str(),structcpid.cpid.c_str(),YesNo(structcpid.Iscpidvalid).c_str());

				if (!structcpid.Iscpidvalid)
				{
					structcpid.errors = "CPID calculation invalid.  Check e-mail + reset project.";
				}
				else
				{
						GlobalCPUMiningCPID.cpidhash = cpidhash;
						GlobalCPUMiningCPID.email = email;
						GlobalCPUMiningCPID.boincruntimepublickey = cpidhash;
				}
				structcpid.utc = cdbl(utc,0);
				structcpid.rac = cdbl(rac,0);
			
				if (structcpid.team != "gridcoin") 
				{
						structcpid.Iscpidvalid = false;
						structcpid.errors = "Team invalid";
				}

				structcpid.rectime = cdbl(rectime,0);

				double currenttime =  GetAdjustedTime();
				double nActualTimespan = currenttime - structcpid.rectime;
				structcpid.age = nActualTimespan;

				//Have credits been verified yet?
				//If not, Call out to credit check node:
	            StructCPID structverify = GetStructCPID();
				std::string sKey = structcpid.cpid + ":" + proj;
							
				structverify = mvCreditNodeCPIDProject[sKey]; //Contains verified CPID+Projects;
				if (!structverify.initialized)
				{
					structverify = GetStructCPID();
				}
				if (projectvalid)
 				{
					CreditCheck(structcpid.cpid,cleardata);
					structverify=mvCreditNodeCPIDProject[sKey];
				}
				
				if (structverify.initialized) 
				{
					structcpid.verifiedutc     = structverify.verifiedutc;
					structcpid.verifiedrac     = structverify.verifiedrac;
					structcpid.verifiedteam    = structverify.verifiedteam;
					structcpid.verifiedrectime = structverify.verifiedrectime;
					structcpid.verifiedage     = structverify.verifiedage;
				}
				if (structcpid.verifiedteam != "gridcoin") 
				{	
						structcpid.Iscpidvalid = false;
						structcpid.errors = "Team invalid";
				}

				
				if (!structcpid.Iscpidvalid)
				{
					structcpid.Iscpidvalid = false;
					structcpid.errors = "CPID invalid.  Check E-mail address.";
				}

				if (!structverify.initialized && structcpid.Iscpidvalid)
				{
					structcpid.Iscpidvalid = false;
					structcpid.errors = "Project missing in credit verification node.";
				}

				if (structcpid.rac < 100)         
				{
					structcpid.Iscpidvalid = false;
					structcpid.errors = "RAC too low";
				}

				if (structverify.initialized)
				{
					if (structcpid.verifiedrac < 100 && structcpid.verifiedrac > 0)
					{
						structcpid.errors = "Verified RAC too low.";

					}
				}

				if (structverify.initialized)
				{
					if (structcpid.verifiedrac == 0 && structcpid.rac > 100)
					{
							
								structcpid.errors = "Unable to verify project RAC online: Project missing in credit verification node.";
					}

					if (structverify.initialized && structcpid.projectname != structverify.projectname)
					{
							structcpid.errors = "Project name does not match client project name.";
			
					}
				

			   }
               if (!projectvalid) 
			   {

				   structcpid.Iscpidvalid = false;
				   structcpid.errors = "Not an official boinc whitelisted project.  Please see 'list projects'.";
			   }

    	  	   mvCPIDs[proj] = structcpid;						
    	  	   if (fDebug) printf("Adding %s",structcpid.cpid.c_str());

			}

		}

	}

	}
	
	catch (std::exception &e) 
	{
			 printf("Error while harvesting CPIDs.\r\n");
	}
    catch(...)
	{
		     printf("Error while harvesting CPIDs 2.\r\n");
	}
	


}



void ThreadTally()
{
	printf("Tallying..");
	//Erase miners last payment time 12-10-2014
	if (false)
	{
	if (GlobalCPUMiningCPID.cpid != "INVESTOR")
	{
		StructCPID Host = mvMagnitudes[GlobalCPUMiningCPID.cpid]; 
		if (Host.initialized)
		{
				//ToDo: Should we set this to zero where orphans occur?
				//Host.Magnitude = 0;
				mvMagnitudes[GlobalCPUMiningCPID.cpid]=Host;
		}	
	}
	}


	printf(".T2.");
	TallyNetworkAverages(false);
	GetNextProject(false);
	printf(".T3.");
	if (fDebug) printf("Completed with Tallying()");

}


void ThreadCPIDs()
{
	//SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("grc-cpids");
    bCPIDsLoaded = false;
	
	HarvestCPIDs(true);
	bCPIDsLoaded = true;
	//Reloads maglevel:
	printf("Performing 1st credit check (%s)",GlobalCPUMiningCPID.cpid.c_str());
	CreditCheck(GlobalCPUMiningCPID.cpid,true);
	printf("Getting first project");
	GetNextProject(false);
	printf("Finished getting first project");
	bProjectsInitialized = true;
}


void LoadCPIDsInBackground()
{
	  if (IsLockTimeWithinMinutes(nCPIDsLoaded,10)) return;
	  nCPIDsLoaded = GetAdjustedTime();
	  cpidThreads = new boost::thread_group();
	  cpidThreads->create_thread(boost::bind(&ThreadCPIDs));
}

void TallyInBackground()
{
	tallyThreads = new boost::thread_group();
	tallyThreads->create_thread(boost::bind(&ThreadTally));
}

StructCPID GetStructCPID()
{
	StructCPID c;
	c.cpid = "";
	c.emailhash="";
	c.cpidhash="";
	c.projectname="";
	c.initialized=false;
	c.isvoucher=false;
	c.rac = 0;
	c.utc=0;
	c.rectime=0;
	c.age = 0;
	c.team="";
	c.activeproject=false;
	c.verifiedrac=0;
	c.verifiedutc=0;
	c.verifiedteam="";
	c.verifiedrectime=0;
	c.verifiedage=0;
	c.entries=0;
	c.AverageRAC=0;
	c.NetworkProjects=0;
	c.boincpublickey="";
	c.Iscpidvalid=false;
	c.link="";
	c.errors="";
	c.email="";
	c.boincruntimepublickey="";
	c.cpidv2="";
	c.NetworkRAC=0;
	c.TotalRAC=0;
	c.TotalNetworkRAC=0;
	c.Magnitude=0;
	c.LastMagnitude=0;
	c.PaymentMagnitude=0;
	c.owed=0;
	c.payments=0;
	c.outstanding=0;
	c.verifiedTotalRAC=0;
	c.verifiedTotalNetworkRAC=0;
	c.verifiedMagnitude=0;
	c.TotalMagnitude=0;
	c.MagnitudeCount=0;
	c.LowLockTime=0;
	c.HighLockTime=0;
	c.ConsensusTotalMagnitude=0;
	c.ConsensusMagnitudeCount=0;
	c.Accuracy=0;
	c.ConsensusMagnitude=0;
	c.totalowed=0;
	c.longtermtotalowed=0;
	c.longtermowed=0;
	c.LastPaymentTime=0;
	c.EarliestPaymentTime=0;
	c.RSAWeight=0;
	c.PaymentTimespan=0;
	c.ResearchSubsidy = 0;
	c.InterestSubsidy = 0;
	c.Canary = 0;
	c.interestPayments = 0;
	c.payments = 0;
	c.PaymentTimestamps = "";
	c.PaymentAmountsResearch = "";
	c.PaymentAmountsInterest = "";

	return c;

}

MiningCPID GetMiningCPID()
{
	MiningCPID mc;
	mc.projectname = "";
	mc.rac = 0;
	mc.encboincpublickey = "";
	mc.cpid = "";
	mc.cpidhash = "";
	mc.pobdifficulty = 0;
	mc.diffbytes = 0;
	mc.initialized = false;
	mc.enccpid = "";
	mc.aesskein = "";
	mc.encaes = "";
	mc.nonce = 0;
	mc.NetworkRAC=0;
	mc.prevBlockType = 0;
	mc.clientversion = "";
	mc.VouchedCPID = "";
	mc.cpidv2 = "";
	mc.email = "";
	mc.boincruntimepublickey = "";
	mc.GRCAddress = "";
	mc.lastblockhash = "0";
	mc.VouchedRAC = 0;
	mc.VouchedNetworkRAC  = 0;
	mc.Magnitude = 0;
	mc.ConsensusTotalMagnitude = 0;
	mc.ConsensusMagnitudeCount  = 0;
	mc.Accuracy = 0;
	mc.ConsensusMagnitude = 0;
	mc.RSAWeight = 0;
	mc.LastPaymentTime=0;
	mc.ResearchSubsidy = 0;
	mc.InterestSubsidy = 0;
	mc.Canary = 0;

	mc.Organization = "";
	mc.OrganizationKey = "";

	return mc;
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
    TRY_LOCK(cs_main, lockMain);
    if (lockMain) {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        //
        // Message: ping
        //
        bool pingSend = false;
        if (pto->fPingQueued) {
            // RPC ping request by user
            pingSend = true;
        }
        if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) {
            // Ping automatically sent as a latency probe & keepalive.
            pingSend = true;
        }
        if (pingSend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                RAND_bytes((unsigned char*)&nonce, sizeof(nonce));
            }
            pto->fPingQueued = false;
            pto->nPingUsecStart = GetTimeMicros();
            if (pto->nVersion > BIP0031_VERSION) {
                pto->nPingNonceSent = nonce;
				std::string acid = GetCommandNonce("ping");
            
                pto->PushMessage("ping", nonce, acid);
            } else {
                // Peer is too old to support ping command with nonce, pong will never arrive.
                pto->nPingNonceSent = 0;
                pto->PushMessage("ping");
            }
        }

        // Resend wallet transactions that haven't gotten in a block yet
        ResendWalletTransactions();

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && ( GetAdjustedTime() - nLastRebroadcast > 24 * 60 * 60))
        {
            {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    if (nLastRebroadcast)
                        pnode->setAddrKnown.clear();

                    // Rebroadcast our address
                    if (!fNoListen)
                    {
                        CAddress addr = GetLocalAddress(&pnode->addr);
                        if (addr.IsRoutable())
                            pnode->PushAddress(addr);
                    }
                }
            }
            nLastRebroadcast =  GetAdjustedTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH(const CAddress& addr, pto->vAddrToSend)
            {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second)
                {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000)
                    {
                        pto->PushMessage("gridaddr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("gridaddr", vAddr);
        }


        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH(const CInv& inv, pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint256 hashRand = inv.hash ^ hashSalt;
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    // always trickle our own transactions
                    if (!fTrickleWait)
                    {
                        CWalletTx wtx;
                        if (GetTransaction(inv.hash, wtx))
                            if (wtx.fFromMe)
                                fTrickleWait = true;
                    }

                    if (fTrickleWait)
                    {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);


        //
        // Message: getdata
        //
        vector<CInv> vGetData;
        int64_t nNow =  GetAdjustedTime() * 1000000;
        CTxDB txdb("r");
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(txdb, inv))
            {
                if (fDebugNet)
                    printf("sending getdata: %s\n", inv.ToString().c_str());
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
                mapAlreadyAskedFor[inv] = nNow;
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);

    }
    return true;
}
