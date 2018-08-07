// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"
#include "net.h"
#include "alert.h"
#include "checkpoints.h"
#include "db.h"
#include "txdb.h"
#include "init.h"
#include "ui_interface.h"
#include "kernel.h"
#include "block.h"
#include "scrypt.h"
#include "global_objects_noui.hpp"
#include "cpid.h"
#include "rpcserver.h"
#include "rpcclient.h"
#include "boinc.h"
#include "beacon.h"
#include "miner.h"
#include "neuralnet.h"
#include "grcrestarter.h"
#include "backup.h"
#include "appcache.h"
#include "tally.h"
#include "contract/contract.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/algorithm/string/join.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <openssl/md5.h>
#include <ctime>
#include <math.h>

extern std::string NodeAddress(CNode* pfrom);
extern std::string ConvertBinToHex(std::string a);
extern std::string ConvertHexToBin(std::string a);
extern bool WalletOutOfSync();
extern bool WriteKey(std::string sKey, std::string sValue);
bool AdvertiseBeacon(std::string &sOutPrivKey, std::string &sOutPubKey, std::string &sError, std::string &sMessage);
extern void CleanInboundConnections(bool bClearAll);
bool RequestSupermajorityNeuralData();
extern bool AskForOutstandingBlocks(uint256 hashStart);
extern bool CleanChain();
extern void ResetTimerMain(std::string timer_name);
bool TallyResearchAverages(CBlockIndex* index);
bool TallyResearchAverages_retired(CBlockIndex* index);
bool TallyResearchAverages_v9(CBlockIndex* index);
extern void IncrementCurrentNeuralNetworkSupermajority(std::string NeuralHash, std::string GRCAddress, double distance);
extern MiningCPID GetInitializedMiningCPID(std::string name, std::map<std::string, MiningCPID>& vRef);
extern std::string getHardDriveSerial();
extern double ExtractMagnitudeFromExplainMagnitude();
extern void GridcoinServices();
extern double SnapToGrid(double d);
extern bool StrLessThanReferenceHash(std::string rh);
extern bool IsContract(CBlockIndex* pIndex);
std::string ExtractValue(std::string data, std::string delimiter, int pos);
extern MiningCPID GetBoincBlockByIndex(CBlockIndex* pblockindex);
UniValue MagnitudeReport(std::string cpid);
extern void AddCPIDBlockHash(const std::string& cpid, const uint256& blockhash);
void RemoveCPIDBlockHash(const std::string& cpid, const uint256& blockhash);
extern void ZeroOutResearcherTotals(std::string cpid);
extern StructCPID GetLifetimeCPID(const std::string& cpid, const std::string& sFrom);
extern std::string getCpuHash();
std::string TimestampToHRDate(double dtm);
bool CPIDAcidTest2(std::string bpk, std::string externalcpid);
extern bool BlockNeedsChecked(int64_t BlockTime);
extern void FixInvalidResearchTotals(std::vector<CBlockIndex*> vDisconnect, std::vector<CBlockIndex*> vConnect);
int64_t GetEarliestWalletTransaction();
extern void IncrementVersionCount(const std::string& Version);
double GetSuperblockAvgMag(std::string data,double& out_beacon_count,double& out_participant_count,double& out_avg,bool bIgnoreBeacons, int nHeight);
extern bool LoadAdminMessages(bool bFullTableScan,std::string& out_errors);

extern std::string GetCurrentNeuralNetworkSupermajorityHash(double& out_popularity);

extern double CalculatedMagnitude2(std::string cpid, int64_t locktime,bool bUseLederstrumpf);

double DoubleFromAmount(int64_t amount);

extern bool UpdateNeuralNetworkQuorumData();
bool AsyncNeuralRequest(std::string command_name,std::string cpid,int NodeLimit);
extern bool FullSyncWithDPORNodes();

bool CheckMessageSignature(std::string sMessageAction, std::string sMessageType, std::string sMsg, std::string sSig,std::string opt_pubkey);
extern std::string strReplace(std::string& str, const std::string& oldStr, const std::string& newStr);
extern bool GetEarliestStakeTime(std::string grcaddress, std::string cpid);
extern double GetTotalBalance();
extern std::string PubKeyToAddress(const CScript& scriptPubKey);
extern void IncrementNeuralNetworkSupermajority(const std::string& NeuralHash, const std::string& GRCAddress, double distance, const CBlockIndex* pblockindex);

extern CBlockIndex* GetHistoricalMagnitude(std::string cpid);

extern double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime, double& total_owed, double block_magnitude);

extern double GetOwedAmount(std::string cpid);
bool TallyMagnitudesInSuperblock();
extern std::string GetNeuralNetworkReport();
std::string GetCommandNonce(std::string command);
std::string DefaultBlockKey(int key_length);

extern std::string ToOfficialName(std::string proj);

extern double GRCMagnitudeUnit(int64_t locktime);
unsigned int nNodeLifespan;

using namespace std;
using namespace boost;

//
// Global state
//

CCriticalSection cs_setpwalletRegistered;
set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;

extern std::string NodeAddress(CNode* pfrom);

CTxMemPool mempool;
unsigned int WHITELISTED_PROJECTS = 0;
int64_t nLastPing = 0;
int64_t nLastAskedForBlocks = 0;
int64_t nBootup = 0;
int64_t nLastLoadAdminMessages = 0;
int64_t nLastGRCtallied = 0;
int64_t nLastCleaned = 0;

extern bool IsCPIDValidv3(std::string cpidv2, bool allow_investor);

double MintLimiter(double PORDiff,int64_t RSA_WEIGHT,std::string cpid,int64_t locktime);
double GetLastPaymentTimeByCPID(std::string cpid);
extern double CoinToDouble(double surrogate);
int64_t GetRSAWeightByCPID(std::string cpid);
extern MiningCPID GetMiningCPID();
extern StructCPID GetStructCPID();

int64_t nLastBlockSolved = 0;  //Future timestamp
int64_t nLastBlockSubmitted = 0;
int64_t nLastCheckedForUpdate = 0;

///////////////////////MINOR VERSION////////////////////////////////
std::string msMasterProjectPublicKey  = "049ac003b3318d9fe28b2830f6a95a2624ce2a69fb0c0c7ac0b513efcc1e93a6a6e8eba84481155dd82f2f1104e0ff62c69d662b0094639b7106abc5d84f948c0a";
// The Private Key is revealed by design, for public messages only:
std::string msMasterMessagePrivateKey = "308201130201010420fbd45ffb02ff05a3322c0d77e1e7aea264866c24e81e5ab6a8e150666b4dc6d8a081a53081a2020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a144034200044b2938fbc38071f24bede21e838a0758a52a0085f2e034e7f971df445436a252467f692ec9c5ba7e5eaa898ab99cbd9949496f7e3cafbf56304b1cc2e5bdf06e";
std::string msMasterMessagePublicKey  = "044b2938fbc38071f24bede21e838a0758a52a0085f2e034e7f971df445436a252467f692ec9c5ba7e5eaa898ab99cbd9949496f7e3cafbf56304b1cc2e5bdf06e";

std::string YesNo(bool bin);

int64_t GetMaximumBoincSubsidy(int64_t nTime);
extern double CalculatedMagnitude(int64_t locktime,bool bUseLederstrumpf);
extern int64_t GetCoinYearReward(int64_t nTime);

BlockMap mapBlockIndex;
set<pair<COutPoint, unsigned int> > setStakeSeen;

CBigNum bnProofOfWorkLimit(~uint256(0) >> 20); // "standard" scrypt target limit for proof of work, results with 0,000244140625 proof-of-work difficulty
CBigNum bnProofOfStakeLimit(~uint256(0) >> 20);
CBigNum bnProofOfStakeLimitV2(~uint256(0) >> 20);
CBigNum bnProofOfWorkLimitTestNet(~uint256(0) >> 16);

//Gridcoin Minimum Stake Age (16 Hours)
unsigned int nStakeMinAge = 16 * 60 * 60; // 16 hours
unsigned int nStakeMaxAge = -1; // unlimited
unsigned int nModifierInterval = 10 * 60; // time to elapse before new modifier is computed
bool bOPReturnEnabled = true;

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

std::map<std::string, double> mvNeuralNetworkHash;
std::map<std::string, double> mvCurrentNeuralNetworkHash;

std::map<std::string, double> mvNeuralVersion;

std::map<std::string, StructCPID> mvDPOR;
std::map<std::string, StructCPID> mvDPORCopy;

std::map<std::string, StructCPID> mvResearchAge;
std::map<std::string, HashSet> mvCPIDBlockHashes;

BlockFinder blockFinder;

// Gridcoin - Rob Halford

extern std::string RetrieveMd5(std::string s1);
extern std::string aes_complex_hash(uint256 scrypt_hash);

bool bNetAveragesLoaded = false;
bool bForceUpdate = false;
bool bGlobalcomInitialized = false;
bool bStakeMinerOutOfSyncWithNetwork = false;
bool fQtActive = false;
bool bGridcoinGUILoaded = false;

extern double LederstrumpfMagnitude2(double Magnitude, int64_t locktime);
extern void GetGlobalStatus();
extern bool ProjectIsValid(std::string project);

double GetNetworkAvgByProject(std::string projectname);
extern bool IsCPIDValid_Retired(std::string cpid, std::string ENCboincpubkey);
extern bool IsCPIDValidv2(MiningCPID& mc, int height);
extern std::string getfilecontents(std::string filename);
extern bool LessVerbose(int iMax1000);
extern MiningCPID GetNextProject(bool bForce);
extern void HarvestCPIDs(bool cleardata);

///////////////////////////////
// Standard Boinc Projects ////
///////////////////////////////

//Global variables to display current mined project in various places:
std::string    msMiningProject;
std::string    msMiningCPID;
std::string    msPrimaryCPID;
double         mdPORNonce = 0;
double         mdLastPorNonce = 0;
double         mdMachineTimerLast = 0;
// Mining status variables
std::string    msHashBoinc;
std::string    msMiningErrors;
std::string    msPoll;
std::string    msMiningErrors5;
std::string    msMiningErrors6;
std::string    msMiningErrors7;
std::string    msMiningErrors8;
std::string    msAttachmentGuid;
std::string    msMiningErrorsIncluded;
std::string    msMiningErrorsExcluded;
std::string    msNeuralResponse;
std::string    msHDDSerial;
//When syncing, we grandfather block rejection rules up to this block, as rules became stricter over time and fields changed

int nGrandfather = 1034700;
int nNewIndex = 271625;
int nNewIndex2 = 364500;

int64_t nGenesisSupply = 340569880;

// Stats for Main Screen:
globalStatusType GlobalStatusStruct;

bool fColdBoot = true;
bool fEnforceCanonical = true;
bool fUseFastIndex = false;

// Gridcoin status    *************
MiningCPID GlobalCPUMiningCPID = GetMiningCPID();
int nBoincUtilization = 0;
std::string sRegVer;

std::map<std::string, StructCPID> mvCPIDs;        //Contains the project stats at the user level
std::map<std::string, StructCPID> mvCreditNode;   //Contains the verified stats at the user level
std::map<std::string, StructCPID> mvNetwork;      //Contains the project stats at the network level
std::map<std::string, StructCPID> mvNetworkCopy;      //Contains the project stats at the network level
std::map<std::string, StructCPID> mvCreditNodeCPID;        // Contains verified CPID Magnitudes;
std::map<std::string, StructCPID> mvMagnitudes; // Contains Magnitudes by CPID & Outstanding Payments Owed per CPID
std::map<std::string, StructCPID> mvMagnitudesCopy; // Contains Magnitudes by CPID & Outstanding Payments Owed per CPID

std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded

// End of Gridcoin Global vars

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//
void ResetTimerMain(std::string timer_name)
{
    mvTimers[timer_name] = 0;
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

bool UpdateNeuralNetworkQuorumData()
{
    if (!bGlobalcomInitialized) return false;
    int64_t superblock_time = ReadCache("superblock", "magnitudes").timestamp;
    int64_t superblock_age = GetAdjustedTime() - superblock_time;
    std::string myNeuralHash = "";
    double popularity = 0;
    std::string consensus_hash = GetNeuralNetworkSupermajorityHash(popularity);
    std::string sAge = ToString(superblock_age);
    std::string sBlock = ReadCache("superblock", "block_number").value;
    std::string sTimestamp = TimestampToHRDate(superblock_time);
    std::string data = "<QUORUMDATA><AGE>" + sAge + "</AGE><HASH>" + consensus_hash + "</HASH><BLOCKNUMBER>" + sBlock + "</BLOCKNUMBER><TIMESTAMP>"
                       + sTimestamp + "</TIMESTAMP><PRIMARYCPID>" + msPrimaryCPID + "</PRIMARYCPID></QUORUMDATA>";
    NN::ExecuteDotNetStringFunction("SetQuorumData",data);
    return true;
}

bool FullSyncWithDPORNodes()
{
    if(!NN::IsEnabled())
        return false;
    // 3-30-2016 : First try to get the master database from another neural network node if these conditions occur:
    // The foreign node is fully synced.  The foreign nodes quorum hash matches the supermajority hash.  My hash != supermajority hash.
    double dCurrentPopularity = 0;
    std::string sCurrentNeuralSupermajorityHash = GetCurrentNeuralNetworkSupermajorityHash(dCurrentPopularity);
    std::string sMyNeuralHash = "";
    sMyNeuralHash = NN::GetNeuralHash();
    if (!sMyNeuralHash.empty() && !sCurrentNeuralSupermajorityHash.empty() && sMyNeuralHash != sCurrentNeuralSupermajorityHash)
    {
        bool bNodeOnline = RequestSupermajorityNeuralData();
        if (bNodeOnline) return false;  // Async call to another node will continue after the node responds.
    }
    std::string errors1;
    LoadAdminMessages(false,errors1);

    const int64_t iEndTime= (GetAdjustedTime()-CONSENSUS_LOOKBACK) - ( (GetAdjustedTime()-CONSENSUS_LOOKBACK) % BLOCK_GRANULARITY);
    const int64_t nLookback = 30 * 6 * 86400;
    const int64_t iStartTime = (iEndTime - nLookback) - ( (iEndTime - nLookback) % BLOCK_GRANULARITY);
    std::string cpiddata = GetListOf("beacon", iStartTime, iEndTime);
    std::string sWhitelist = GetListOf("project");
    int64_t superblock_time = ReadCache("superblock", "magnitudes").timestamp;
    int64_t superblock_age = GetAdjustedTime() - superblock_time;
    double popularity = 0;
    std::string consensus_hash = GetNeuralNetworkSupermajorityHash(popularity);
    std::string sAge = ToString(superblock_age);
    std::string sBlock = ReadCache("superblock", "block_number").value;
    std::string sTimestamp = TimestampToHRDate(superblock_time);
    std::string data = "<WHITELIST>" + sWhitelist + "</WHITELIST><CPIDDATA>"
                       + cpiddata + "</CPIDDATA><QUORUMDATA><AGE>" + sAge + "</AGE><HASH>" + consensus_hash + "</HASH><BLOCKNUMBER>" + sBlock + "</BLOCKNUMBER><TIMESTAMP>"
                       + sTimestamp + "</TIMESTAMP><PRIMARYCPID>" + msPrimaryCPID + "</PRIMARYCPID></QUORUMDATA>";
    NN::SynchronizeDPOR(data);
    return true;
}

double GetEstimatedNetworkWeight(unsigned int nPoSInterval)
{
    // The number of stakes to include in the average has been reduced to 40 (default) from 72. 72 stakes represented 1.8 hours at
    // standard spacing. This is too long. 40 blocks is nominally 1 hour.
    double result;

    // The constant below comes from (MaxHash / StandardDifficultyTarget) * 16 sec / 90 sec. If you divide it by 80 to convert to GRC you
    // get the familiar 9544517.40667
    result = 763561392.533 * GetAverageDifficulty(nPoSInterval);
    if (fDebug10) LogPrintf("GetEstimatedNetworkWeight debug: Network Weight = %f", result);
    if (fDebug10) LogPrintf("GetEstimatedNetworkWeight debug: Network Weight in GRC = %f", result / 80.0);

    return result;
}

double GetAverageDifficulty(unsigned int nPoSInterval)
{
    /*
     * Diff is inversely related to Target (without the coinweight multiplier), but proportional to the
     * effective number of coins on the network. This is tricky, if you want to get the average target value
     * used over an interval you should use a harmonic average, since target is inversely related to diff. If
     * on the other hand, you want to average diff in a way to also determine the average coins active in
     * the network, you should simply use an arithmetic average. See the relation between diff and estimated
     * network weight above. We do not need to take into account the actual spacing of the blocks, because this
     * already handled by the retargeting in GetNextTargetRequiredV2, and in fact, given the random distribution
     * of block spacing, it would be harmful to use a spacing correction for small nPoSInterval sizes.
     *
     * Also... The number of stakes to include in the average has been reduced to 40 (default) from 72.
     * 72 stakes represented 1.8 hours at standard spacing. This is too long. 40 blocks is nominally 1 hour.
     */

    double dDiff = 1.0;
    double dDiffSum = 0.0;
    unsigned int nStakesHandled = 0;
    double result;

    CBlockIndex* pindex = pindexBest;

    while (pindex && nStakesHandled < nPoSInterval)
    {
        if (pindex->IsProofOfStake())
        {
            dDiff = GetDifficulty(pindex);
            // dDiff should never be zero, but just in case, skip the block and move to the next one.
            if (dDiff)
            {
                dDiffSum += dDiff;
                nStakesHandled++;
                if (fDebug10) LogPrintf("GetAverageDifficulty debug: dDiff = %f", dDiff);
                if (fDebug10) LogPrintf("GetAverageDifficulty debug: nStakesHandled = %u", nStakesHandled);
            }
        }

        pindex = pindex->pprev;
    }

    result = nStakesHandled ? dDiffSum / nStakesHandled : 0;
    if (fDebug10) LogPrintf("GetAverageDifficulty debug: Average dDiff = %f", result);

    return result;
}

double GetEstimatedTimetoStake(double dDiff, double dConfidence)
{
    /*
     * The algorithm below is an attempt to come up with a more accurate way of estimating Time to Stake (ETTS) based on
     * the actual situation of the miner and UTXO's. A simple equation will not provide good results, because in mainnet,
     * the cooldown period is 16 hours, and depending on how many UTXO's and where they are with respect to getting out of
     * cooldown has a lot to do with the expected time to stake.
     *
     * The way to conceptualize the approach below is to think of the UTXO's as bars on a Gantt Chart. It is a negative Gantt
     * chart, meaning that each UTXO bar is cooldown period long, and while the current time is in that bar, the staking probability
     * for the UTXO is zero, and UnitStakingProbability elsewhere. A timestamp mask of 16x the normal mask is used to reduce
     * the work in the nested loop, so that a 16 hour interval will have a maximum of 225 events, and most likely far less.
     * This is important, because the inner loop will be the number of UTXO's. A future improvement to this algorithm would
     * also be to quantize (group) the UTXO's themselves (the Gantt bars) so that the work would be further reduced.
     * You will see that once the UTXO's are sorted in ascending order based on the time of the end of each of their cooldowns, this
     * becomes a manageable algorithm to piece the probabilities together.
     *
     * You will note that the compound Poisson (geometric) recursive probability relation is used, since you cannot simply add
     * the probabilities due to consideration of high confidence (CDF) values of 80% or more.
     *
     * Thin local data structures are used to hold the UTXO information. This minimizes the amount of time
     * that locks on the wallet need to be held at the expense of a little memory consumption.
     */

    double result = 0.0;

    // dDiff must be >= 0 and dConfidence must lie on the interval [0,1) otherwise this is an error.
    assert(dDiff >= 0 && dConfidence >= 0 && dConfidence < 1);

    // if dConfidence = 0, then the result must be 0.
    if (!dConfidence)
    {
        if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: Confidence of 0 specified: ETTS = %f", result);
        return result;
    }

    bool staking = MinerStatus.nLastCoinStakeSearchInterval && MinerStatus.WeightSum;
    // Get out early if not staking and set return value of 0.
    if (!staking)
    {
        if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: Not staking: ETTS = %f", result);
        return result;
    }

    int64_t nValue = 0;
    int64_t nCurrentTime = GetAdjustedTime();
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: nCurrentTime = %i", nCurrentTime);

    CTxDB txdb("r");

    // Here I am defining a time mask 16 times as long as the normal stake time mask. This is to quantize the UTXO's into a maximum of
    // 16 hours * 3600 / 256 = 225 time bins for evaluation. Otherwise for a large number of UTXO's, this algorithm could become
    // really expensive.
    const int ETTS_TIMESTAMP_MASK = (16 * (STAKE_TIMESTAMP_MASK + 1)) - 1;
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: ETTS_TIMESTAMP_MASK = %x", ETTS_TIMESTAMP_MASK);

    int64_t BalanceAvailForStaking = 0;
    vector<COutput> vCoins;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        BalanceAvailForStaking = pwalletMain->GetBalance() - nReserveBalance;

        if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: BalanceAvailForStaking = %u", BalanceAvailForStaking);

        // Get out early if no balance available and set return value of 0. This should already have happened above, because with no
        // balance left after reserve, staking should be disabled; however, just to be safe...
        if (BalanceAvailForStaking <= 0)
        {
            if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: No balance available: ETTS = %f", result);
            return result;
        }

        //reminder... void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl=NULL, bool fIncludeStakingCoins=false) const;
        pwalletMain->AvailableCoins(vCoins, true, NULL, true);
    }


    // An efficient local structure to store the UTXO's with the bare minimum info we need.
    typedef vector< std::pair<int64_t, int64_t> > vCoinsExt;
    vCoinsExt vUTXO;
    // A local ordered set to store the unique "bins" corresponding to the UTXO transaction times. We are going to use this
    // for the outer loop.
    std::set<int64_t> UniqueUTXOTimes;
    // We want the first "event" to be the CurrentTime. This does not have to be quantized.
    UniqueUTXOTimes.insert(nCurrentTime);

    // Debug output cooldown...
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: nStakeMinAge = %i", nStakeMinAge);

    // If dDiff = 0 from supplied argument (which is also the default), then derive a smoothed difficulty over the default PoSInterval of 40 blocks by calling
    // GetAverageDifficulty(40), otherwise let supplied argument dDiff stand.
    if (!dDiff) dDiff = GetAverageDifficulty(40);
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: dDiff = %f", dDiff);

    // The stake probability per "throw" of 1 weight unit = target value at diff of 1.0 / (maxhash * diff). This happens effectively every STAKE_TIMESTAMP_MASK+1 sec.
    double dUnitStakeProbability = 1 / (4295032833.0 * dDiff);
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: dUnitStakeProbability = %e", dUnitStakeProbability);


    int64_t nTime = 0;
    for (const auto& out : vCoins)
    {
        CTxIndex txindex;
        CBlock CoinBlock; //Block which contains CoinTx
        if (!txdb.ReadTxIndex(out.tx->GetHash(), txindex))
            continue; //error?

        if (!CoinBlock.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
            continue;

        // We are going to store as an event the time that the UTXO matures (is available for staking again.)
        nTime = (CoinBlock.GetBlockTime() & ~ETTS_TIMESTAMP_MASK) + nStakeMinAge;

        nValue = out.tx->vout[out.i].nValue;

        // Only consider UTXO's that are actually stakeable - which means that each one must be less than the available balance
        // subtracting the reserve. Each UTXO also has to be greater than 1/80 GRC to result in a weight greater than zero in the CreateCoinStake loop,
        // so eliminate UTXO's with less than 0.0125 GRC balances right here. The test with Satoshi units for that is
        // nValue >= 1250000.
        if(BalanceAvailForStaking >= nValue && nValue >= 1250000)
        {
        vUTXO.push_back(std::pair<int64_t, int64_t>( nTime, nValue));
        if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: pair (relative to current time: <%i, %i>", nTime - nCurrentTime, nValue);

        // Only record a time below if it is after nCurrentTime, because UTXO's that have matured already are already stakeable and can be grouped (will be found)
        // by the nCurrentTime record that was already injected above.
        if(nTime > nCurrentTime) UniqueUTXOTimes.insert(nTime);
        }
    }


    int64_t nTimePrev = nCurrentTime;
    int64_t nDeltaTime = 0;
    int64_t nThrows = 0;
    int64_t nCoinWeight = 0;
    double dProbAccumulator = 0;
    double dCumulativeProbability = 0;
    // Note: Even though this is a compound Poisson process leading to a compound geometric distribution, and the individual probabilities are
    // small, we are mounting to high CDFs. This means to be reasonably accurate, we cannot just add the probabilities, because the intersections
    // become significant. The CDF of a compound geometric distribution as you do tosses with different probabilities follows the
    // recursion relation... CDF.i = 1 - (1 - CDF.i-1)(1 - p.i). If all probabilities are the same, this reduces to the familiar
    // CDF.k = 1 - (1 - p)^k where ^ is exponentiation.
    for(const auto& itertime : UniqueUTXOTimes)
    {

        nTime = itertime;
        dProbAccumulator = 0;

        for( auto& iterUTXO : vUTXO)
        {

            if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: Unique UTXO Time: %u, vector pair <%u, %u>", nTime, iterUTXO.first, iterUTXO.second);

            // If the "negative Gantt chart bar" is ending or has ended for a UTXO, it now accumulates probability. (I.e. the event time being checked
            // is greater than or equal to the cooldown expiration of the UTXO.)
            // accumulation for that UTXO.
            if(nTime >= iterUTXO.first)
            {
                // The below weight calculation is just like the CalculateStakeWeightV8 in kernel.cpp.
                nCoinWeight = iterUTXO.second / 1250000;

                dProbAccumulator = 1 - ((1 - dProbAccumulator) * (1 - (dUnitStakeProbability * nCoinWeight)));
                if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: dProbAccumulator = %e", dProbAccumulator);
            }

        }
        nDeltaTime = nTime - nTimePrev;
        nThrows = nDeltaTime / (STAKE_TIMESTAMP_MASK + 1);
        if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: nThrows = %i", nThrows);
        dCumulativeProbability = 1 - ((1 - dCumulativeProbability) * pow((1 - dProbAccumulator), nThrows));
        if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: dCumulativeProbability = %e", dCumulativeProbability);

        if(dCumulativeProbability >= dConfidence)
            break;

        nTimePrev = nTime;
    }

    // If (dConfidence - dCumulativeProbability) > 0, it means we exited the negative Gantt chart area and the desired confidence level
    // has not been reached. All of the eligible UTXO's are contributing probability, and this is the final dProbAccumulator value.
    // If the loop above is degenerate (i.e. only the current time pass through), then dCumulativeProbability will be zero.
    // If it was not degenerate and the positive reqions in the Gantt chart area contributed some probability, then dCumulativeProbability will
    // be greater than zero. We must compute the amount of time beyond nTime that is required to bridge the gap between
    // dCumulativeProbability and dConfidence. If (dConfidence - dCumulativeProbability) <= 0 then we overshot during the Gantt chart area,
    // and we will back off by nThrows amount, which will now be negative.
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: dProbAccumulator = %e", dProbAccumulator);

    // Shouldn't happen because if we are down here, we are staking, and there have to be eligible UTXO's, but just in case...
    if (dProbAccumulator == 0.0)
    {
        if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: ERROR in dProbAccumulator calculations");
        return result;
    }

    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: dConfidence = %f", dConfidence);
    // If nThrows is negative, this just means we overshot in the Gantt chart loop and have to backtrack by nThrows.
    nThrows = (int64_t)((log(1 - dConfidence) - log(1 - dCumulativeProbability)) / log(1 - dProbAccumulator));
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: nThrows = %i", nThrows);

    nDeltaTime = nThrows * (STAKE_TIMESTAMP_MASK + 1);
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: nDeltaTime = %i", nDeltaTime);

    // Because we are looking at the delta time required past nTime, which is where we exited the Gantt chart loop.
    result = nDeltaTime + nTime - nCurrentTime;
    if (fDebug10) LogPrintf("GetEstimatedTimetoStake debug: ETTS at %d confidence = %i", dConfidence, result);

    // The old calculation for comparative purposes, only done if fDebug10 set. Note that this is the "fixed" old
    // calculation, because the old network weight calculation was wrong too...
    if (fDebug10)
    {
        double oldETTS = 0;

        oldETTS = GetTargetSpacing(nBestHeight) * GetEstimatedNetworkWeight(40) / MinerStatus.WeightSum;
        LogPrintf("GetEstimatedTimetoStake debug: oldETTS = %f", oldETTS);
    }

    return result;
}


void GetGlobalStatus()
{
    //Populate overview

    try
    {
        double boincmagnitude = CalculatedMagnitude(GetAdjustedTime(),false);
        uint64_t nWeight = 0;
        pwalletMain->GetStakeWeight(nWeight);
        nBoincUtilization = boincmagnitude; //Legacy Support for the about screen
        double weight = nWeight/COIN;
        double PORDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
        std::string sWeight = RoundToString((double)weight,0);

        //9-6-2015 Add RSA fields to overview
        if ((double)weight > 100000000000000)
        {
            sWeight = sWeight.substr(0,13) + "E" + RoundToString((double)sWeight.length()-13,0);
        }

        // It is necessary to assign a local variable for ETTS to avoid an occasional deadlock between the lock below,
        // the lock on cs_main in GetEstimateTimetoStake(), and the corresponding lock in the stakeminer.
        double dETTS = GetEstimatedTimetoStake()/86400.0;
        LOCK(GlobalStatusStruct.lock);
        { LOCK(MinerStatus.lock);
        GlobalStatusStruct.blocks = ToString(nBestHeight);
        GlobalStatusStruct.difficulty = RoundToString(PORDiff,3);
        GlobalStatusStruct.netWeight = RoundToString(GetEstimatedNetworkWeight() / 80.0,2);
        //todo: use the real weight from miner status (requires scaling)
        GlobalStatusStruct.coinWeight = sWeight;
        GlobalStatusStruct.magnitude = RoundToString(boincmagnitude,2);
        GlobalStatusStruct.ETTS = RoundToString(dETTS,3);
        GlobalStatusStruct.ERRperday = RoundToString(boincmagnitude * GRCMagnitudeUnit(GetAdjustedTime()),2);
        GlobalStatusStruct.project = msMiningProject;
        GlobalStatusStruct.cpid = GlobalCPUMiningCPID.cpid;
        GlobalStatusStruct.poll = msPoll;

        GlobalStatusStruct.status.clear();

        if(MinerStatus.WeightSum)
            GlobalStatusStruct.coinWeight = RoundToString(MinerStatus.WeightSum / 80.0,2);

        GlobalStatusStruct.errors.clear();
        std::string Alerts = GetWarnings("statusbar");
        if(!Alerts.empty())
            GlobalStatusStruct.errors += _("Alert: ") + Alerts + "; ";

        if (PORDiff < 0.1)
            GlobalStatusStruct.errors +=  _("Low difficulty!; ");

        if(!MinerStatus.ReasonNotStaking.empty())
            GlobalStatusStruct.errors +=  _("Miner: ") + MinerStatus.ReasonNotStaking;

        unsigned long stk_dropped = MinerStatus.KernelsFound - MinerStatus.AcceptedCnt;
        if(stk_dropped)
            GlobalStatusStruct.errors += "Rejected " + ToString(stk_dropped) + " stakes;";

        if(!msMiningErrors6.empty())
            GlobalStatusStruct.errors +=msMiningErrors6 + "; ";
        if(!msMiningErrors7.empty())
            GlobalStatusStruct.errors += msMiningErrors7 + "; ";
        if(!msMiningErrors8.empty())
            GlobalStatusStruct.errors += msMiningErrors8 + "; ";

        }
        return;
    }
    catch (std::exception& e)
    {
        msMiningErrors = _("Error obtaining status.");

        LogPrintf("Error obtaining status");
        return;
    }
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


MiningCPID GetInitializedGlobalCPUMiningCPID(std::string cpid)
{

    MiningCPID mc = GetMiningCPID();
    mc.initialized = true;
    mc.cpid=cpid;
    mc.projectname = cpid;
    mc.cpidv2=cpid;
    mc.cpidhash = "";
    mc.email = cpid;
    mc.boincruntimepublickey = cpid;
    mc.rac=0;
    mc.encboincpublickey = "";
    mc.enccpid = "";
    mc.NetworkRAC = 0;
    mc.Magnitude = 0;
    mc.clientversion = "";
    mc.RSAWeight = GetRSAWeightByCPID(cpid);
    mc.LastPaymentTime = nLastBlockSolved;
    mc.diffbytes = 0;
    mc.lastblockhash = "0";
    // Reuse for debugging
    mc.Organization = GetArg("-org", "");
    return mc;
}


MiningCPID GetNextProject(bool bForce)
{



    if (GlobalCPUMiningCPID.projectname.length() > 3   &&  GlobalCPUMiningCPID.projectname != "INVESTOR"  && GlobalCPUMiningCPID.Magnitude >= 1)
    {
                if (!Timer_Main("globalcpuminingcpid",10))
                {
                    //Prevent Thrashing
                    return GlobalCPUMiningCPID;
                }
    }


    std::string sBoincKey = GetArgument("boinckey","");
    if (!sBoincKey.empty())
    {
        if (fDebug3 && LessVerbose(50)) LogPrintf("Using cached boinckey for project %s",GlobalCPUMiningCPID.projectname);
                    msMiningProject = GlobalCPUMiningCPID.projectname;
                    msMiningCPID = GlobalCPUMiningCPID.cpid;
                    if (LessVerbose(5)) LogPrintf("BoincKey - Mining project %s     RAC(%f)",  GlobalCPUMiningCPID.projectname, GlobalCPUMiningCPID.rac);
                    double ProjectRAC = GetNetworkAvgByProject(GlobalCPUMiningCPID.projectname);
                    GlobalCPUMiningCPID.NetworkRAC = ProjectRAC;
                    GlobalCPUMiningCPID.Magnitude = CalculatedMagnitude(GetAdjustedTime(),false);
                    if (fDebug3) LogPrintf("(boinckey) For CPID %s Verified Magnitude = %f",GlobalCPUMiningCPID.cpid,GlobalCPUMiningCPID.Magnitude);
                    msMiningErrors = (msMiningCPID == "INVESTOR" || msPrimaryCPID=="INVESTOR" || msMiningCPID.empty()) ? _("Staking Interest") : _("Mining");
                    GlobalCPUMiningCPID.RSAWeight = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
                    GlobalCPUMiningCPID.LastPaymentTime = GetLastPaymentTimeByCPID(GlobalCPUMiningCPID.cpid);
                    return GlobalCPUMiningCPID;
    }


    msMiningProject = "";
    msMiningCPID = "";
    GlobalCPUMiningCPID = GetInitializedGlobalCPUMiningCPID("");

    std::string email = GetArgument("email", "NA");
    boost::to_lower(email);



    if (IsInitialBlockDownload() && !bForce)
    {
        if (LessVerbose(100)) LogPrintf("CPUMiner: Gridcoin is downloading blocks Or CPIDs are not yet loaded...");
        MilliSleep(1);
        return GlobalCPUMiningCPID;
    }

    try
    {

        if (mvCPIDs.size() < 1)
        {
            if (fDebug && LessVerbose(10)) LogPrintf("Gridcoin has no CPIDs...");
            //Let control reach the investor area
        }

        int iValidProjects=0;
        //Count valid projects:
        for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii)
        {
                StructCPID structcpid = mvCPIDs[(*ii).first];
                if (        msPrimaryCPID == structcpid.cpid &&
                    structcpid.initialized && structcpid.Iscpidvalid)           iValidProjects++;
        }

        // Find next available CPU project:
        int iDistributedProject = 0;
        int iRow = 0;

        if (email=="" || email=="NA") iValidProjects = 0;  //Let control reach investor area


        if (iValidProjects > 0)
        {
        for (int i = 0; i <= 4;i++)
        {
            iRow=0;
            iDistributedProject = (rand() % iValidProjects)+1;

            for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii)
            {
                StructCPID structcpid = mvCPIDs[(*ii).first];

                if (structcpid.initialized)
                {
                    if (msPrimaryCPID == structcpid.cpid &&
                        structcpid.Iscpidvalid && structcpid.projectname.length() > 1)
                    {
                            iRow++;
                            if (i==4 || iDistributedProject == iRow)
                            {
                                if (true)
                                {
                                    GlobalCPUMiningCPID.enccpid = structcpid.boincpublickey;
                                    bool checkcpid = IsCPIDValid_Retired(structcpid.cpid,GlobalCPUMiningCPID.enccpid);
                                    if (!checkcpid)
                                    {
                                        LogPrintf("CPID invalid %s  1.  ",structcpid.cpid);
                                        continue;
                                    }

                                    if (checkcpid)
                                    {

                                        GlobalCPUMiningCPID.email = email;

                                        if (LessVerbose(1) || fDebug || fDebug3) LogPrintf("Ready to CPU Mine project %s with CPID %s, RAC(%f) ",
                                            structcpid.projectname.c_str(),structcpid.cpid.c_str(),
                                            structcpid.rac);
                                        //Required for project to be mined in a block:
                                        GlobalCPUMiningCPID.cpid=structcpid.cpid;
                                        GlobalCPUMiningCPID.projectname = structcpid.projectname;
                                        GlobalCPUMiningCPID.rac=structcpid.rac;
                                        GlobalCPUMiningCPID.encboincpublickey = structcpid.boincpublickey;
                                        GlobalCPUMiningCPID.encaes = structcpid.boincpublickey;


                                        GlobalCPUMiningCPID.boincruntimepublickey = structcpid.cpidhash;
                                        if(fDebug) LogPrintf("GNP: Setting bpk to %s",structcpid.cpidhash);

                                        uint256 pbh = 1;
                                        GlobalCPUMiningCPID.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email,GlobalCPUMiningCPID.boincruntimepublickey, pbh);
                                        GlobalCPUMiningCPID.lastblockhash = "0";
                                        // Sign the block
                                        GlobalCPUMiningCPID.BoincPublicKey = GetBeaconPublicKey(structcpid.cpid, false);
                                        std::string sSignature;
                                        std::string sError;
                                        bool bResult = SignBlockWithCPID(GlobalCPUMiningCPID.cpid, GlobalCPUMiningCPID.lastblockhash, sSignature, sError, true);
#                                       if 0
                                        if (!bResult)
                                        {
                                            LogPrintf("GetNextProject: failed to sign block with cpid -> %s", sError);
                                            continue;
                                        }
                                        GlobalCPUMiningCPID.BoincSignature = sSignature;
                                        if (!IsCPIDValidv2(GlobalCPUMiningCPID,1))
                                        {
                                            LogPrintf("CPID INVALID (GetNextProject) %s, %s  ",GlobalCPUMiningCPID.cpid,GlobalCPUMiningCPID.cpidv2);
                                            continue;
                                        }
#                                       endif


                                        //Only used for global status:
                                        msMiningProject = structcpid.projectname;
                                        msMiningCPID = structcpid.cpid;

                                        double ProjectRAC = GetNetworkAvgByProject(GlobalCPUMiningCPID.projectname);
                                        GlobalCPUMiningCPID.NetworkRAC = ProjectRAC;
                                        GlobalCPUMiningCPID.Magnitude = CalculatedMagnitude(GetAdjustedTime(),false);
                                        if (fDebug && LessVerbose(2)) LogPrintf("For CPID %s Verified Magnitude = %f",GlobalCPUMiningCPID.cpid, GlobalCPUMiningCPID.Magnitude);
                                        //Reserved for GRC Speech Synthesis
                                        msMiningErrors = (msMiningCPID == "INVESTOR" || !IsResearcher(msPrimaryCPID) || msMiningCPID.empty()) ? _("Staking Interest") : _("Boinc Mining");
                                        GlobalCPUMiningCPID.RSAWeight = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
                                        GlobalCPUMiningCPID.LastPaymentTime = GetLastPaymentTimeByCPID(GlobalCPUMiningCPID.cpid);
                                        return GlobalCPUMiningCPID;
                                    }
                                }
                            }

                    }

                }
            }

        }
        }

        msMiningErrors = (IsResearcher(msPrimaryCPID)) ? _("All BOINC projects exhausted.") : "";
        msMiningProject = "INVESTOR";
        msMiningCPID = "INVESTOR";
        GlobalCPUMiningCPID = GetInitializedGlobalCPUMiningCPID("INVESTOR");
        if (fDebug10) LogPrintf("-Investor mode-");

        }
        catch (std::exception& e)
        {
            msMiningErrors = _("Error obtaining next project.  Error 16172014.");

            LogPrintf("Error obtaining next project");
        }
        catch(...)
        {
            msMiningErrors = _("Error obtaining next project.  Error 06172014.");
            LogPrintf("Error obtaining next project 2.");
        }
        return GlobalCPUMiningCPID;

}



// check whether the passed transaction is from us
bool static IsFromMe(CTransaction& tx)
{
    for (auto const& pwallet : setpwalletRegistered)
        if (pwallet->IsFromMe(tx))
            return true;
    return false;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    for (auto const& pwallet : setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx,wtx))
            return true;
    return false;
}

// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
{
    for (auto const& pwallet : setpwalletRegistered)
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
            for (auto const& pwallet : setpwalletRegistered)
                if (pwallet->IsFromMe(tx))
                    pwallet->DisableTransaction(tx);
        }
        return;
    }

    for (auto const& pwallet : setpwalletRegistered)
        pwallet->AddToWalletIfInvolvingMe(tx, pblock, fUpdate);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

// notify wallets about an updated transaction
void static UpdatedTransaction(const uint256& hashTx)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->Inventory(hash);
}

// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
{
    for (auto const& pwallet : setpwalletRegistered)
        pwallet->ResendWalletTransactions(fForce);
}


double CoinToDouble(double surrogate)
{
    //Converts satoshis to a human double amount
    double coin = (double)surrogate/(double)COIN;
    return coin;
}

double GetTotalBalance()
{
    double total = 0;
    for (auto const& pwallet : setpwalletRegistered)
    {
        total = total + pwallet->GetBalance();
        total = total + pwallet->GetStake();
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
        LogPrint("mempool", "ignoring large orphan tx (size: %" PRIszu ", hash: %s)", nSize, hash.ToString().substr(0,10));
        return false;
    }

    mapOrphanTransactions[hash] = tx;
    for (auto const& txin : tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    LogPrint("mempool", "stored orphan tx %s (mapsz %" PRIszu ")", hash.ToString().substr(0,10), mapOrphanTransactions.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CTransaction& tx = mapOrphanTransactions[hash];
    for (auto const& txin : tx.vin)
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
    static std::string sDefaultWalletAddress;
    if (!sDefaultWalletAddress.empty())
        return sDefaultWalletAddress;

    try
    {
        //Gridcoin - Find the default public GRC address (since a user may have many receiving addresses):
        for (auto const& item : pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            const std::string& strName = item.second;
            bool fMine = IsMine(*pwalletMain, address.Get());
            if (fMine && strName == "Default")
            {
                sDefaultWalletAddress=CBitcoinAddress(address).ToString();
                return sDefaultWalletAddress;
            }
        }

        //Cant Find
        for (auto const& item : pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            //const std::string& strName = item.second;
            bool fMine = IsMine(*pwalletMain, address.Get());
            if (fMine)
            {
                sDefaultWalletAddress=CBitcoinAddress(address).ToString();
                return sDefaultWalletAddress;
            }
        }
    }
    catch (std::exception& e)
    {
        return "ERROR";
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
    std::string reason = "";
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

    for (auto const& txin : tx.vin)
    {

        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)=1624
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)

        if (txin.scriptSig.size() > 1650)
            return false;
        if (!txin.scriptSig.IsPushOnly())
            return false;
        if (fEnforceCanonical && !txin.scriptSig.HasCanonicalPushes()) {
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    for (auto const& txout : tx.vout) {
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


    // not more than one data txout per non-data txout is permitted
    // only one data txout is permitted too
    if (nDataOut > 1 && nDataOut > tx.vout.size()/2)
    {
        reason = "multi-op-return";
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
    for (auto const& txin : tx.vin)
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
        if (!EvalScript(stack, vin[i].scriptSig, *this, i, 0))            return false;

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

unsigned int CTransaction::GetLegacySigOpCount() const
{
    unsigned int nSigOps = 0;
    for (auto const& txin : vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (auto const& txout : vout)
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
        LogPrintf("ERROR: SetMerkleBranch() : couldn't find tx in block");
        return 0;
    }

    // Fill in merkle branch
    vMerkleBranch = pblock->GetMerkleBranch(nIndex);

    // Is the tx in a block that's in the main chain
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
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
    for (auto const& txin : vin)
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
        for (auto const& txin : vin)
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
        for (auto const& txout : vout)
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


bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx, bool* pfMissingInputs)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!tx.CheckTransaction())
        return error("AcceptToMemoryPool : CheckTransaction failed");

    // Verify beacon contract in tx if found
    if (!VerifyBeaconContractTx(tx))
        return tx.DoS(25, error("AcceptToMemoryPool : bad beacon contract in tx %s; rejected", tx.GetHash().ToString().c_str()));

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
            return error("AcceptToMemoryPool : not enough fees %s, %" PRId64 " < %" PRId64,
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
                    LogPrint("mempool", "Rate limit dFreeCount: %g => %g", dFreeCount, dFreeCount+nSize);
                dFreeCount += nSize;
            }
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!tx.ConnectInputs(txdb, mapInputs, mapUnused, CDiskTxPos(1,1,1), pindexBest, false, false))
        {
            // If this happens repeatedly, purge peers
            if (TimerMain("AcceptToMemoryPool", 20))
            {
                LogPrint("mempool", "AcceptToMemoryPool::CleaningInboundConnections");
                CleanInboundConnections(true);
            }
            if (fDebug || true)
            {
                return error("AcceptToMemoryPool : Unable to Connect Inputs %s", hash.ToString().c_str());
            }
            else
            {
                return false;
            }
        }
    }

    // Store transaction in memory
    {
        LOCK(pool.cs);
        if (ptxOld)
        {
            LogPrint("mempool", "AcceptToMemoryPool : replacing tx %s with new version", ptxOld->GetHash().ToString());
            pool.remove(*ptxOld);
        }
        pool.addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());
    if (fDebug)     LogPrint("mempool", "AcceptToMemoryPool : accepted %s (poolsz %" PRIszu ")",           hash.ToString(), pool.mapTx.size());
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
            for (auto const& txin : tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction &tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    for (auto const &txin : tx.vin)
    {
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
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
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
        for (auto tx : vtxPrev)
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
    BlockMap::iterator mi = mapBlockIndex.find(block.GetHash());
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


double CalculatedMagnitude(int64_t locktime,bool bUseLederstrumpf)
{
    // Get neural network magnitude:
    std::string cpid = "";
    if (GlobalCPUMiningCPID.initialized && !GlobalCPUMiningCPID.cpid.empty()) cpid = GlobalCPUMiningCPID.cpid;
    StructCPID stDPOR = GetInitializedStructCPID2(cpid,mvDPOR);
    return bUseLederstrumpf ? LederstrumpfMagnitude2(stDPOR.Magnitude,locktime) : stDPOR.Magnitude;
}

double CalculatedMagnitude2(std::string cpid, int64_t locktime,bool bUseLederstrumpf)
{
    // Get neural network magnitude:
    StructCPID stDPOR = GetInitializedStructCPID2(cpid,mvDPOR);
    return bUseLederstrumpf ? LederstrumpfMagnitude2(stDPOR.Magnitude,locktime) : stDPOR.Magnitude;
}



// miner's coin base reward
int64_t GetProofOfWorkReward(int64_t nFees, int64_t locktime, int64_t height)
{
    //NOTE: THIS REWARD IS ONLY USED IN THE POW PHASE (Block < 8000):
    int64_t nSubsidy = CalculatedMagnitude(locktime,true) * COIN;
    if (fDebug && GetBoolArg("-printcreation"))
        LogPrintf("GetProofOfWorkReward() : create=%s nSubsidy=%" PRId64, FormatMoney(nSubsidy), nSubsidy);
    if (nSubsidy < (30*COIN)) nSubsidy=30*COIN;
    //Gridcoin Foundation Block:
    if (height==10)
    {
        nSubsidy = nGenesisSupply * COIN;
    }
    if (fTestNet) nSubsidy += 1000*COIN;

    return nSubsidy + nFees;
}


int64_t GetProofOfWorkMaxReward(int64_t nFees, int64_t locktime, int64_t height)
{
    int64_t nSubsidy = (GetMaximumBoincSubsidy(locktime)+1) * COIN;
    if (height==10)
    {
        //R.Halford: 10-11-2014: Gridcoin Foundation Block:
        //Note: Gridcoin Classic emitted these coins.  So we had to add them to block 10.  The coins were burned then given back to the owners that mined them in classic (as research coins).
        nSubsidy = nGenesisSupply * COIN;
    }

    if (fTestNet) nSubsidy += 1000*COIN;
    return nSubsidy + nFees;
}

//Survey Results: Start inflation rate: 9%, end=1%, 30 day steps, 9 steps, mag multiplier start: 2, mag end .3, 9 steps
int64_t GetMaximumBoincSubsidy(int64_t nTime)
{
    // Gridcoin Global Daily Maximum Researcher Subsidy Schedule
    int MaxSubsidy = 500;
    if (nTime >= 1410393600 && nTime <= 1417305600) MaxSubsidy =    500; // between inception  and 11-30-2014
    if (nTime >= 1417305600 && nTime <= 1419897600) MaxSubsidy =    400; // between 11-30-2014 and 12-30-2014
    if (nTime >= 1419897600 && nTime <= 1422576000) MaxSubsidy =    400; // between 12-30-2014 and 01-30-2015
    if (nTime >= 1422576000 && nTime <= 1425254400) MaxSubsidy =    300; // between 01-30-2015 and 02-28-2015
    if (nTime >= 1425254400 && nTime <= 1427673600) MaxSubsidy =    250; // between 02-28-2015 and 03-30-2015
    if (nTime >= 1427673600 && nTime <= 1430352000) MaxSubsidy =    200; // between 03-30-2015 and 04-30-2015
    if (nTime >= 1430352000 && nTime <= 1438310876) MaxSubsidy =    150; // between 05-01-2015 and 07-31-2015
    if (nTime >= 1438310876 && nTime <= 1445309276) MaxSubsidy =    100; // between 08-01-2015 and 10-20-2015
    if (nTime >= 1445309276 && nTime <= 1447977700) MaxSubsidy =     75; // between 10-20-2015 and 11-20-2015
    if (nTime > 1447977700)                         MaxSubsidy =     50; // from  11-20-2015 forever
    return MaxSubsidy+.5;  //The .5 allows for fractional amounts after the 4th decimal place (used to store the POR indicator)
}

int64_t GetCoinYearReward(int64_t nTime)
{
    // Gridcoin Global Interest Rate Schedule
    int64_t INTEREST = 9;
    if (nTime >= 1410393600 && nTime <= 1417305600) INTEREST =   9 * CENT; // 09% between inception  and 11-30-2014
    if (nTime >= 1417305600 && nTime <= 1419897600) INTEREST =   8 * CENT; // 08% between 11-30-2014 and 12-30-2014
    if (nTime >= 1419897600 && nTime <= 1422576000) INTEREST =   8 * CENT; // 08% between 12-30-2014 and 01-30-2015
    if (nTime >= 1422576000 && nTime <= 1425254400) INTEREST =   7 * CENT; // 07% between 01-30-2015 and 02-30-2015
    if (nTime >= 1425254400 && nTime <= 1427673600) INTEREST =   6 * CENT; // 06% between 02-30-2015 and 03-30-2015
    if (nTime >= 1427673600 && nTime <= 1430352000) INTEREST =   5 * CENT; // 05% between 03-30-2015 and 04-30-2015
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


int64_t GetProofOfStakeMaxReward(uint64_t nCoinAge, int64_t nFees, int64_t locktime)
{
    int64_t nInterest = nCoinAge * GetCoinYearReward(locktime) * 33 / (365 * 33 + 8);
    nInterest += 10*COIN;
    int64_t nBoinc    = (GetMaximumBoincSubsidy(locktime)+1) * COIN;
    int64_t nSubsidy  = nInterest + nBoinc;
    return nSubsidy + nFees;
}

double GetProofOfResearchReward(std::string cpid, bool VerifyingBlock)
{

        StructCPID mag = GetInitializedStructCPID2(cpid,mvMagnitudes);

        if (!mag.initialized) return 0;
        double owed = (mag.owed*1.0);
        if (owed < 0) owed = 0;
        // Coarse Payment Rule (helps prevent sync problems):
        if (!VerifyingBlock)
        {
            //If owed less than 4% of max subsidy, assess at 0:
            if (owed < (GetMaximumBoincSubsidy(GetAdjustedTime())/50))
            {
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


        }
        //End of Coarse Payment Rule
        return owed * COIN;
}


// miner's coin stake reward based on coin age spent (coin-days)

int64_t GetProofOfStakeReward(uint64_t nCoinAge, int64_t nFees, std::string cpid,
    bool VerifyingBlock, int VerificationPhase, int64_t nTime, CBlockIndex* pindexLast, std::string operation,
    double& OUT_POR, double& OUT_INTEREST, double& dAccrualAge, double& dMagnitudeUnit, double& AvgMagnitude)
{

    // Non Research Age - RSA Mode - Legacy (before 10-20-2015)
    if (!IsResearchAgeEnabled(pindexLast->nHeight))
    {
            int64_t nInterest = nCoinAge * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8);
            int64_t nBoinc    = GetProofOfResearchReward(cpid,VerifyingBlock);
            int64_t nSubsidy  = nInterest + nBoinc;
            if (fDebug10 || GetBoolArg("-printcreation"))
            {
                LogPrintf("GetProofOfStakeReward(): create=%s nCoinAge=%" PRIu64 " nBoinc=%" PRId64 "   ",
                FormatMoney(nSubsidy), nCoinAge, nBoinc);
            }
            int64_t maxStakeReward1 = GetProofOfStakeMaxReward(nCoinAge, nFees, nTime);
            int64_t maxStakeReward2 = GetProofOfStakeMaxReward(nCoinAge, nFees, GetAdjustedTime());
            int64_t maxStakeReward = std::min(maxStakeReward1, maxStakeReward2);
            if ((nSubsidy+nFees) > maxStakeReward) nSubsidy = maxStakeReward-nFees;
            int64_t nTotalSubsidy = nSubsidy + nFees;
            if (nBoinc > 1)
            {
                std::string sTotalSubsidy = RoundToString(CoinToDouble(nTotalSubsidy)+.00000123,8);
                if (sTotalSubsidy.length() > 7)
                {
                    sTotalSubsidy = sTotalSubsidy.substr(0,sTotalSubsidy.length()-4) + "0124";
                    nTotalSubsidy = RoundFromString(sTotalSubsidy,8)*COIN;
                }
            }

            OUT_POR = CoinToDouble(nBoinc);
            OUT_INTEREST = CoinToDouble(nInterest);
            return nTotalSubsidy;
    }
    else
    {
            // Research Age Subsidy - PROD
            int64_t nBoinc = ComputeResearchAccrual(nTime, cpid, operation, pindexLast, VerifyingBlock, VerificationPhase, dAccrualAge, dMagnitudeUnit, AvgMagnitude);
            int64_t nInterest = nCoinAge * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8);

            // TestNet: For any subsidy < 30 day duration, ensure 100% that we have a start magnitude and an end magnitude, otherwise make subsidy 0 : PASS
            // TestNet: For any subsidy > 30 day duration, ensure 100% that we have a midpoint magnitude in Every Period, otherwise, make subsidy 0 : In Test as of 09-06-2015
            // TestNet: Ensure no magnitudes are out of bounds to ensure we do not generate an insane payment : PASS (Lifetime PPD takes care of this)
            // TestNet: Any subsidy with a duration wider than 6 months should not be paid : PASS

            int64_t maxStakeReward = GetMaximumBoincSubsidy(nTime) * COIN * 255;

            if (nBoinc > maxStakeReward) nBoinc = maxStakeReward;
            int64_t nSubsidy = nInterest + nBoinc;

            if (fDebug10 || GetBoolArg("-printcreation"))
            {
                LogPrintf("GetProofOfStakeReward(): create=%s nCoinAge=%" PRIu64 " nBoinc=%" PRId64 "   ",
                FormatMoney(nSubsidy), nCoinAge, nBoinc);
            }

            int64_t nTotalSubsidy = nSubsidy + nFees;
            if (nBoinc > 1)
            {
                std::string sTotalSubsidy = RoundToString(CoinToDouble(nTotalSubsidy)+.00000123,8);
                if (sTotalSubsidy.length() > 7)
                {
                    sTotalSubsidy = sTotalSubsidy.substr(0,sTotalSubsidy.length()-4) + "0124";
                    nTotalSubsidy = RoundFromString(sTotalSubsidy,8)*COIN;
                }
            }

            OUT_POR = CoinToDouble(nBoinc);
            OUT_INTEREST = CoinToDouble(nInterest);
            return nTotalSubsidy;

    }
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

    //Gridcoin - Reset Diff to 1 on 12-19-2014 (R Halford) - Diff sticking at 2065 due to many incompatible features
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


    //Since our nTargetTimespan is (16 * 60) or 16 mins and our TargetSpacing = 64, the nInterval = 15 min

    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
    {
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

bool CheckProofOfResearch(
        const CBlockIndex* pindexPrev, //previous block in chain index
        const CBlock &block)     //block to check
{
    if(block.vtx.size() == 0 ||
       !block.IsProofOfStake() ||
       pindexPrev->nHeight <= nGrandfather ||
       !IsResearchAgeEnabled(pindexPrev->nHeight))
        return true;

    MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc, block.nVersion);
    if(!IsResearcher(bb.cpid))
        return true;

    //For higher security, plus lets catch these bad blocks before adding them to the chain to prevent reorgs:
    double OUT_POR = 0;
    double OUT_INTEREST = 0;
    double dAccrualAge = 0;
    double dMagnitudeUnit = 0;
    double dAvgMagnitude = 0;
    int64_t nCoinAge = 0;
    int64_t nFees = 0;

    bool fNeedsChecked = BlockNeedsChecked(block.nTime) || block.nVersion>=9;

    if(!fNeedsChecked)
        return true;

    // 6-4-2017 - Verify researchers stored block magnitude
    double dNeuralNetworkMagnitude = CalculatedMagnitude2(bb.cpid, block.nTime, false);
    if( bb.Magnitude > 0
        && (fTestNet || (!fTestNet && pindexPrev->nHeight > 947000))
        && bb.Magnitude > (dNeuralNetworkMagnitude*1.25) )
    {
        return error("CheckProofOfResearch: Researchers block magnitude > neural network magnitude: Block Magnitude %f, Neural Network Magnitude %f, CPID %s ",
                     bb.Magnitude, dNeuralNetworkMagnitude, bb.cpid.c_str());
    }

    int64_t nCalculatedResearch = GetProofOfStakeReward(nCoinAge, nFees, bb.cpid, true, 1, block.nTime,
                                                        pindexBest, "checkblock_researcher", OUT_POR, OUT_INTEREST, dAccrualAge, dMagnitudeUnit, dAvgMagnitude);

    if(!IsV9Enabled_Tally(pindexPrev->nHeight))
    {
        if (bb.ResearchSubsidy > ((OUT_POR*1.25)+1))
        {
            if (fDebug) LogPrintf("CheckProofOfResearch: Researchers Reward Pays too much : Retallying : "
                                "claimedand %f vs calculated StakeReward %f for CPID %s",
                                bb.ResearchSubsidy, OUT_POR, bb.cpid);

            TallyResearchAverages(pindexBest);
            GetLifetimeCPID(bb.cpid,"CheckProofOfResearch()");
            nCalculatedResearch = GetProofOfStakeReward(nCoinAge, nFees, bb.cpid, true, 2, block.nTime,
                                                        pindexBest, "checkblock_researcher_doublecheck", OUT_POR, OUT_INTEREST, dAccrualAge, dMagnitudeUnit, dAvgMagnitude);
        }
    }
    (void)nCalculatedResearch;

    if (bb.ResearchSubsidy > ((OUT_POR*1.25)+1))
    {
        if(fDebug) LogPrintf("CheckProofOfResearch: pHistorical was %s", GetHistoricalMagnitude(bb.cpid)->GetBlockHash().GetHex());
        return block.DoS(10,error("CheckProofOfResearch: Researchers Reward Pays too much : "
                            "claimed %f vs calculated %f for CPID %s",
                            bb.ResearchSubsidy, OUT_POR, bb.cpid.c_str() ));
    }

    return true;
}

// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    LOCK(cs_main);
    return std::max(cPeerBlockCounts.median(), Checkpoints::GetTotalBlocksEstimate());
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    if ((pindexBest == NULL || nBestHeight < GetNumBlocksOfPeers()) && nBestHeight<1185000)
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

    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s",
      pindexNew->GetBlockHash().ToString().substr(0,20), pindexNew->nHeight,
      CBigNum(pindexNew->nChainTrust).ToString(), nBestInvalidBlockTrust.Get64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexNew->GetBlockTime()));
    LogPrintf("InvalidChainFound:  current best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s",
      hashBestChain.ToString().substr(0,20), nBestHeight,
      CBigNum(pindexBest->nChainTrust).ToString(),
      nBestBlockTrust.Get64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()));
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
        for (auto const& txin : vin)
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
            {
                if (fDebug) LogPrintf("FetchInputs() : %s mempool Tx prev not found %s", GetHash().ToString().substr(0,10),  prevout.hash.ToString().substr(0,10));
                return false;
            }
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
            return DoS(100, error("FetchInputs() : %s prevout.n out of range %d %" PRIszu " %" PRIszu " prev tx %s\n%s", GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));
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


int64_t PreviousBlockAge()
{
    LOCK(cs_main);

    int64_t blockTime = pindexBest && pindexBest->pprev
            ? pindexBest->pprev->GetBlockTime()
            : 0;

    return GetAdjustedTime() - blockTime;
}


bool OutOfSyncByAge()
{
    // Assume we are out of sync if the current block age is 10
    // times older than the target spacing. This is the same
    // rules at Bitcoin uses.
    const int64_t maxAge = GetTargetSpacing(nBestHeight) * 10;
    return PreviousBlockAge() >= maxAge;
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
                return DoS(100, error("ConnectInputs() : %s prevout.n out of range %d %" PRIszu " %" PRIszu " prev tx %s\n%s", GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));

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
                if (fMiner)
                {
                    msMiningErrorsExcluded += " ConnectInputs() : " + GetHash().GetHex() + " used at "
                        + txindex.vSpent[prevout.n].ToString() + ";   ";
                    return false;
                }
                if (!txindex.vSpent[prevout.n].IsNull())
                {
                    if (fTestNet && pindexBlock->nHeight < nGrandfather)
                    {
                        return fMiner ? false : true;
                    }
                    if (!fTestNet && pindexBlock->nHeight < nGrandfather)
                    {
                        return fMiner ? false : true;
                    }
                    if (TimerMain("ConnectInputs", 20))
                    {
                        CleanInboundConnections(false);
                    }

                    if (fMiner) return false;
                    return fDebug ? error("ConnectInputs() : %s prev tx already used at %s", GetHash().ToString().c_str(), txindex.vSpent[prevout.n].ToString().c_str()) : false;
                }

            }

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.

            if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate())))
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
            {
                LogPrintf("ConnectInputs(): VALUE IN < VALUEOUT ");
                return DoS(100, error("ConnectInputs() : %s value in < value out", GetHash().ToString().substr(0,10).c_str()));
            }

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

    // Disconnect in reverse order
    bool bDiscTxFailed = false;
    for (int i = vtx.size()-1; i >= 0; i--)
    {
        if (!vtx[i].DisconnectInputs(txdb))
        {
            bDiscTxFailed = true;
        }

        /* Delete the contract.
         * Previous version will be reloaded in reoranize. */
        {
            std::string sMType = ExtractXML(vtx[i].hashBoinc, "<MT>", "</MT>");
            if(!sMType.empty())
            {
                std::string sMKey = ExtractXML(vtx[i].hashBoinc, "<MK>", "</MK>");
                DeleteCache(sMType, sMKey);
                if(fDebug)
                    LogPrintf("DisconnectBlock: Delete contract %s %s", sMType, sMKey);

                if("beacon"==sMType)
                {
                    sMKey=sMKey+"A";
                    DeleteCache("beaconalt", sMKey+"."+ToString(vtx[i].nTime));
                }
            }
        }

    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    // Brod: I do not like this...
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    for (auto const& tx : vtx)
        SyncWithWallets(tx, this, false, false);

    if (bDiscTxFailed) return error("DisconnectBlock(): Failed");
    return true;
}



double BlockVersion(std::string v)
{
    if (v.length() < 10) return 0;
    std::string vIn = v.substr(1,7);
    boost::replace_all(vIn, ".", "");
    double ver1 = RoundFromString(vIn,0);
    return ver1;
}


std::string PubKeyToAddress(const CScript& scriptPubKey)
{
    //Converts a script Public Key to a Gridcoin wallet address
    txnouttype type;
    vector<CTxDestination> addresses;
    int nRequired;
    if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired))
    {
        return "";
    }
    std::string address = "";
    for (auto const& addr : addresses)
    {
        address = CBitcoinAddress(addr).ToString();
    }
    return address;
}

bool LoadSuperblock(std::string data, int64_t nTime, int height)
{
        WriteCache("superblock","magnitudes",ExtractXML(data,"<MAGNITUDES>","</MAGNITUDES>"),nTime);
        WriteCache("superblock","averages",ExtractXML(data,"<AVERAGES>","</AVERAGES>"),nTime);
        WriteCache("superblock","quotes",ExtractXML(data,"<QUOTES>","</QUOTES>"),nTime);
        WriteCache("superblock","all",data,nTime);
        WriteCache("superblock","block_number",ToString(height),nTime);
        return true;
}

std::string CharToString(char c)
{
    std::stringstream ss;
    std::string sOut = "";
    ss << c;
    ss >> sOut;
    return sOut;
}


template< typename T >
std::string int_to_hex( T i )
{
  std::stringstream stream;
  stream << "0x"
         << std::setfill ('0') << std::setw(sizeof(T)*2)
         << std::hex << i;
  return stream.str();
}

std::string DoubleToHexStr(double d, int iPlaces)
{
    int nMagnitude = atoi(RoundToString(d,0).c_str());
    std::string hex_string = int_to_hex(nMagnitude);
    std::string sOut = "00000000" + hex_string;
    std::string sHex = sOut.substr(sOut.length()-iPlaces,iPlaces);
    return sHex;
}

int HexToInt(std::string sHex)
{
    int x;
    std::stringstream ss;
    ss << std::hex << sHex;
    ss >> x;
    return x;
}
std::string ConvertHexToBin(std::string a)
{
    if (a.empty()) return "";
    std::string sOut = "";
    for (unsigned int x = 1; x <= a.length(); x += 2)
    {
       std::string sChunk = a.substr(x-1,2);
       int i = HexToInt(sChunk);
       char c = (char)i;
       sOut.push_back(c);
    }
    return sOut;
}


double ConvertHexToDouble(std::string hex)
{
    int d = HexToInt(hex);
    double dOut = (double)d;
    return dOut;
}


std::string ConvertBinToHex(std::string a)
{
      if (a.empty()) return "0";
      std::string sOut = "";
      for (unsigned int x = 1; x <= a.length(); x++)
      {
           char c = a[x-1];
           int i = (int)c;
           std::string sHex = DoubleToHexStr((double)i,2);
           sOut += sHex;
      }
      return sOut;
}

std::string UnpackBinarySuperblock(std::string sBlock)
{
    // 12-21-2015: R HALFORD: If the block is not binary, return the legacy format for backward compatibility
    std::string sBinary = ExtractXML(sBlock,"<BINARY>","</BINARY>");
    if (sBinary.empty()) return sBlock;
    std::string sZero = ExtractXML(sBlock,"<ZERO>","</ZERO>");
    double dZero = RoundFromString(sZero,0);
    // Binary data support structure:
    // Each CPID consumes 16 bytes and 2 bytes for magnitude: (Except CPIDs with zero magnitude - the count of those is stored in XML node <ZERO> to save space)
    // 1234567890123456MM
    // MM = Magnitude stored as 2 bytes
    // No delimiter between CPIDs, Step Rate = 18
    std::string sReconstructedMagnitudes = "";
    for (unsigned int x = 0; x < sBinary.length(); x += 18)
    {
        if (sBinary.length() >= x+18)
        {
            std::string bCPID = sBinary.substr(x,16);
            std::string bMagnitude = sBinary.substr(x+16,2);
            std::string sCPID = ConvertBinToHex(bCPID);
            std::string sHexMagnitude = ConvertBinToHex(bMagnitude);
            double dMagnitude = ConvertHexToDouble("0x" + sHexMagnitude);
            std::string sRow = sCPID + "," + RoundToString(dMagnitude,0) + ";";
            sReconstructedMagnitudes += sRow;
        }
    }
    // Append zero magnitude researchers so the beacon count matches
    for (double d0 = 1; d0 <= dZero; d0++)
    {
            std::string sZeroCPID = "0";
            std::string sRow1 = sZeroCPID + ",15;";
            sReconstructedMagnitudes += sRow1;
    }
    std::string sAverages   = ExtractXML(sBlock,"<AVERAGES>","</AVERAGES>");
    std::string sQuotes     = ExtractXML(sBlock,"<QUOTES>","</QUOTES>");
    std::string sReconstructedBlock = "<AVERAGES>" + sAverages + "</AVERAGES><QUOTES>" + sQuotes + "</QUOTES><MAGNITUDES>" + sReconstructedMagnitudes + "</MAGNITUDES>";
    return sReconstructedBlock;
}

std::string PackBinarySuperblock(std::string sBlock)
{
    std::string sMagnitudes = ExtractXML(sBlock,"<MAGNITUDES>","</MAGNITUDES>");
    std::string sAverages   = ExtractXML(sBlock,"<AVERAGES>","</AVERAGES>");
    std::string sQuotes     = ExtractXML(sBlock,"<QUOTES>","</QUOTES>");
    // For each CPID in the superblock, convert data to binary
    std::vector<std::string> vSuperblock = split(sMagnitudes.c_str(),";");
    std::string sBinary = "";
    double dZeroMagCPIDCount = 0;
    for (unsigned int i = 0; i < vSuperblock.size(); i++)
    {
            if (vSuperblock[i].length() > 1)
            {
                std::string sPrefix = "00000000000000000000000000000000000" + ExtractValue(vSuperblock[i],",",0);
                std::string sCPID = sPrefix.substr(sPrefix.length()-32,32);
                double magnitude = RoundFromString(ExtractValue("0"+vSuperblock[i],",",1),0);
                if (magnitude < 0)     magnitude=0;
                if (magnitude > 32767) magnitude = 32767;  // Ensure we do not blow out the binary space (technically we can handle 0-65535)
                std::string sBinaryCPID   = ConvertHexToBin(sCPID);
                std::string sHexMagnitude = DoubleToHexStr(magnitude,4);
                std::string sBinaryMagnitude = ConvertHexToBin(sHexMagnitude);
                std::string sBinaryEntry  = sBinaryCPID+sBinaryMagnitude;
                if (sCPID=="00000000000000000000000000000000")
                {
                    dZeroMagCPIDCount += 1;
                }
                else
                {
                    sBinary += sBinaryEntry;
                }

            }
    }
    std::string sReconstructedBinarySuperblock = "<ZERO>" + RoundToString(dZeroMagCPIDCount,0) + "</ZERO><BINARY>" + sBinary + "</BINARY><AVERAGES>" + sAverages + "</AVERAGES><QUOTES>" + sQuotes + "</QUOTES>";
    return sReconstructedBinarySuperblock;
}




double ClientVersionNew()
{
    double cv = BlockVersion(FormatFullVersion());
    return cv;
}


int64_t ReturnCurrentMoneySupply(CBlockIndex* pindexcurrent)
{
    if (pindexcurrent->pprev)
    {
        // If previous exists, and previous money supply > Genesis, OK to use it:
        if (pindexcurrent->pprev->nHeight > 11 && pindexcurrent->pprev->nMoneySupply > nGenesisSupply)
        {
            return pindexcurrent->pprev->nMoneySupply;
        }
    }
    // Special case where block height < 12, use standard old logic:
    if (pindexcurrent->nHeight < 12)
    {
        return (pindexcurrent->pprev? pindexcurrent->pprev->nMoneySupply : 0);
    }
    // At this point, either the last block pointer was NULL, or the client erased the money supply previously, fix it:
    CBlockIndex* pblockIndex = pindexcurrent;
    CBlockIndex* pblockMemory = pindexcurrent;
    int nMinDepth = (pindexcurrent->nHeight)-140000;
    if (nMinDepth < 12) nMinDepth=12;
    while (pblockIndex->nHeight > nMinDepth)
    {
            pblockIndex = pblockIndex->pprev;
            LogPrintf("Money Supply height %d", pblockIndex->nHeight);

            if (pblockIndex == NULL || !pblockIndex->IsInMainChain()) continue;
            if (pblockIndex == pindexGenesisBlock)
            {
                return nGenesisSupply;
            }
            if (pblockIndex->nMoneySupply > nGenesisSupply)
            {
                //Set index back to original pointer
                pindexcurrent = pblockMemory;
                //Return last valid money supply
                return pblockIndex->nMoneySupply;
            }
    }
    // At this point, we fall back to the old logic with a minimum of the genesis supply (should never happen - if it did, blockchain will need rebuilt anyway due to other fields being invalid):
    pindexcurrent = pblockMemory;
    return (pindexcurrent->pprev? pindexcurrent->pprev->nMoneySupply : nGenesisSupply);
}

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck, bool fReorganizing)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock("ConnectBlock",pindex->pprev->nHeight, 395*COIN, !fJustCheck, !fJustCheck, false,false))
    {
        LogPrintf("ConnectBlock::Failed - ");
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
    double DPOR_Paid = 0;

    bool bIsDPOR = false;

    if(nVersion>=8)
    {
        uint256 tmp_hashProof;
        if(!CheckProofOfStakeV8(pindex->pprev, *this, /*generated_by_me*/ false, tmp_hashProof))
            return error("ConnectBlock(): check proof-of-stake failed");
    }

    for (auto &tx : vtx)
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
            for (auto const& pos : txindexOld.vSpent)
                if (pos.IsNull())
                    return false;
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock[] : too many sigops"));

        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
        {
            nValueOut += tx.GetValueOut();
        }
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
                return DoS(100, error("ConnectBlock[] : too many sigops"));

            int64_t nTxValueIn = tx.GetValueIn(mapInputs);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
            {
                nStakeReward = nTxValueOut - nTxValueIn;
                if (tx.vout.size() > 3 && pindex->nHeight > nGrandfather) bIsDPOR = true;
                // ResearchAge: Verify vouts cannot contain any other payments except coinstake: PASS (GetValueOut returns the sum of all spent coins in the coinstake)
                if (IsResearchAgeEnabled(pindex->nHeight) && fDebug10)
                {
                    int64_t nTotalCoinstake = 0;
                    for (unsigned int i = 0; i < tx.vout.size(); i++)
                    {
                        nTotalCoinstake += tx.vout[i].nValue;
                    }
                    if (fDebug10)   LogPrintf(" nHeight %d; nTCS %f; nTxValueOut %f",
                        pindex->nHeight,CoinToDouble(nTotalCoinstake),CoinToDouble(nTxValueOut));
                }

                // Verify no recipients exist after coinstake (Recipients start at output position 3 (0=Coinstake flag, 1=coinstake amount, 2=splitstake amount)
                if (bIsDPOR && pindex->nHeight > nGrandfather)
                {
                    for (unsigned int i = 3; i < tx.vout.size(); i++)
                    {
                        std::string Recipient = PubKeyToAddress(tx.vout[i].scriptPubKey);
                        double      Amount    = CoinToDouble(tx.vout[i].nValue);
                        if (fDebug10) LogPrintf("Iterating Recipient #%d  %s with Amount %f", i, Recipient, Amount);
                        if (Amount > 0)
                        {
                            if (fDebug3) LogPrintf("Iterating Recipient #%d  %s with Amount %f", i, Recipient, Amount);
                            LogPrintf("POR Payment results in an overpayment; Recipient %s, Amount %f ",Recipient, Amount);
                            return DoS(50,error("POR Payment results in an overpayment; Recipient %s, Amount %f ",
                                                Recipient.c_str(), Amount));
                        }
                    }
                }
            }

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
            return DoS(50, error("ConnectBlock[] : coinbase reward exceeded (actual=%" PRId64 " vs calculated=%" PRId64 ")",
                   vtx[0].GetValueOut(),
                   nReward));
    }

    MiningCPID bb = DeserializeBoincBlock(vtx[0].hashBoinc,nVersion);
    uint64_t nCoinAge = 0;

    double dStakeReward = CoinToDouble(nStakeReward+nFees) - DPOR_Paid; //DPOR Recipients checked above already
    double dStakeRewardWithoutFees = CoinToDouble(nStakeReward) - DPOR_Paid;

    if (fDebug) LogPrintf("Stake Reward of %f B %f I %f F %.f %s %s  ",
        dStakeReward,bb.ResearchSubsidy,bb.InterestSubsidy,(double)nFees,bb.cpid, bb.Organization);

    if (IsProofOfStake() && pindex->nHeight > nGrandfather)
    {
            // ppcoin: coin stake tx earns reward instead of paying fee
        if (!vtx[1].GetCoinAge(txdb, nCoinAge))
            return error("ConnectBlock[] : %s unable to get coin age for coinstake", vtx[1].GetHash().ToString().substr(0,10).c_str());

        double dCalcStakeReward = CoinToDouble(GetProofOfStakeMaxReward(nCoinAge, nFees, nTime));

        if (dStakeReward > dCalcStakeReward+1 && !IsResearchAgeEnabled(pindex->nHeight))
            return DoS(1, error("ConnectBlock[] : coinstake pays above maximum (actual= %f, vs calculated=%f )", dStakeReward, dCalcStakeReward));

        //9-3-2015
        double dMaxResearchAgeReward = CoinToDouble(GetMaximumBoincSubsidy(nTime) * COIN * 255);

        if (bb.ResearchSubsidy > dMaxResearchAgeReward && IsResearchAgeEnabled(pindex->nHeight))
            return DoS(1, error("ConnectBlock[ResearchAge] : Coinstake pays above maximum (actual= %f, vs calculated=%f )", dStakeRewardWithoutFees, dMaxResearchAgeReward));

        if (!IsResearcher(bb.cpid) && dStakeReward > 1)
        {
            double OUT_POR = 0;
            double OUT_INTEREST_OWED = 0;

            double dAccrualAge = 0;
            double dAccrualMagnitudeUnit = 0;
            double dAccrualMagnitude = 0;

            double dCalculatedResearchReward = CoinToDouble(GetProofOfStakeReward(nCoinAge, nFees, bb.cpid, true, 1, nTime,
                    pindex, "connectblock_investor",
                    OUT_POR, OUT_INTEREST_OWED, dAccrualAge, dAccrualMagnitudeUnit, dAccrualMagnitude));
            if (dStakeReward > (OUT_INTEREST_OWED+1+nFees) )
            {
                    return DoS(10, error("ConnectBlock[] : Investor Reward pays too much : cpid %s (actual %f vs calculated %f), dCalcResearchReward %f, Fees %f",
                    bb.cpid.c_str(), dStakeReward, OUT_INTEREST_OWED, dCalculatedResearchReward, (double)nFees));
            }
        }

    }


    AddCPIDBlockHash(bb.cpid, pindex->GetBlockHash());

    // Track money supply and mint amount info
    pindex->nMint = nValueOut - nValueIn + nFees;
    if (fDebug10) LogPrintf (".TMS.");

    pindex->nMoneySupply = ReturnCurrentMoneySupply(pindex) + nValueOut - nValueIn;

    // Gridcoin: Store verified magnitude and CPID in block index (7-11-2015)
    if (pindex->nHeight > nNewIndex2)
    {
        pindex->SetCPID(bb.cpid);
        pindex->nMagnitude = bb.Magnitude;
        pindex->nResearchSubsidy = bb.ResearchSubsidy;
        pindex->nInterestSubsidy = bb.InterestSubsidy;
        pindex->nIsSuperBlock =  (bb.superblock.length() > 20) ? 1 : 0;
        // Must scan transactions after CoinStake to know if this is a contract.
        int iPos = 0;
        pindex->nIsContract = 0;
        for (auto const& tx : vtx)
        {
            if (tx.hashBoinc.length() > 3 && iPos > 0)
            {
                pindex->nIsContract = 1;
                break;
            }
            iPos++;
        }
    }

    double mint = CoinToDouble(pindex->nMint);
    double PORDiff = GetBlockDifficulty(nBits);

    if (pindex->nHeight > nGrandfather && !fReorganizing)
    {
        // Block Spamming
        if (mint < MintLimiter(PORDiff,bb.RSAWeight,bb.cpid,GetBlockTime()))
        {
            return error("CheckProofOfStake[] : Mint too Small, %f",(double)mint);
        }

        if (mint == 0) return error("CheckProofOfStake[] : Mint is ZERO! %f",(double)mint);

        double OUT_POR = 0;
        double OUT_INTEREST = 0;
        double dAccrualAge = 0;
        double dMagnitudeUnit = 0;
        double dAvgMagnitude = 0;

        // ResearchAge 1:
        GetProofOfStakeReward(nCoinAge, nFees, bb.cpid, true, 1, nTime,
            pindex, "connectblock_researcher", OUT_POR, OUT_INTEREST, dAccrualAge, dMagnitudeUnit, dAvgMagnitude);
        if (IsResearcher(bb.cpid))
        {

                //ResearchAge: Since the best block may increment before the RA is connected but After the RA is computed, the ResearchSubsidy can sometimes be slightly smaller than we calculate here due to the RA timespan increasing.  So we will allow for time shift before rejecting the block.
                double dDrift = IsResearchAgeEnabled(pindex->nHeight) ? bb.ResearchSubsidy*.15 : 1;
                if (IsResearchAgeEnabled(pindex->nHeight) && dDrift < 10) dDrift = 10;

                if ((bb.ResearchSubsidy + bb.InterestSubsidy + dDrift) < dStakeRewardWithoutFees)
                {
                        return DoS(20, error("ConnectBlock[] : Researchers Interest %f + Research %f + TimeDrift %f and total Mint %f, [StakeReward] <> %f, with Out_Interest %f, OUT_POR %f, Fees %f, DPOR %f  for CPID %s does not match calculated research subsidy",
                            (double)bb.InterestSubsidy,(double)bb.ResearchSubsidy,dDrift,CoinToDouble(mint),dStakeRewardWithoutFees,
                            (double)OUT_INTEREST,(double)OUT_POR,CoinToDouble(nFees),(double)DPOR_Paid,bb.cpid.c_str()));

                }

                if (bb.lastblockhash != pindex->pprev->GetBlockHash().GetHex())
                {
                            std::string sNarr = "ConnectBlock[ResearchAge] : Historical DPOR Replay attack : lastblockhash != actual last block hash.";
                            LogPrintf("******  %s ***** ",sNarr);
                }

                if (IsResearchAgeEnabled(pindex->nHeight)
                    && (BlockNeedsChecked(nTime) || nVersion>=9))
                {

                        // 6-4-2017 - Verify researchers stored block magnitude
                        // 2018 02 04 - Moved here for better effect.
                        double dNeuralNetworkMagnitude = CalculatedMagnitude2(bb.cpid, nTime, false);
                        if( bb.Magnitude > 0
                            && (fTestNet || (!fTestNet && (pindex->nHeight-1) > 947000))
                            && bb.Magnitude > (dNeuralNetworkMagnitude*1.25) )
                        {
                            return DoS(20, error(
                                "ConnectBlock[ResearchAge]: Researchers block magnitude > neural network magnitude: Block Magnitude %f, Neural Network Magnitude %f, CPID %s ",
                                bb.Magnitude, dNeuralNetworkMagnitude, bb.cpid.c_str()));
                        }

                        // 2018 02 04 - Brod - Move cpid check here for better effect
                        /* Only signature check is sufficient here, but kiss and
                            call the function. The height is of previous block. */
                        if( !IsCPIDValidv2(bb,pindex->nHeight-1) )
                        {
                            /* ignore on bad blocks already in chain */
                            const std::set<uint256> vSkipHashBoincSignCheck =
                            {    uint256("58b2d6d0ff7e3ebcaca1058be7574a87efadd4b7f5c661f9e14255f851a6185e") //P1144550 S
                                ,uint256("471292b59e5f3ad94c39b3784a9a3f7a8324b9b56ff0ad00bd48c31658537c30") //P1146939 S
                                ,uint256("5b63d4edbdec06ddc2182703ce45a3ced70db0d813e329070e83bf37347a6c2c") //P1152917 S
                                ,uint256("e9035d821668a0563b632e9c84bc5af73f53eafcca1e053ac6da53907c7f6940") //P1154121 S
                                ,uint256("1d30c6d4dce377d69c037f1a725aabbc6bafa72a95456dbe2b2538bc1da115bd") //P1168122 S
                                ,uint256("934c6291209d90bb5d3987885b413c18e39f0e28430e8d302f20888d2a35e725") //P1168193 S
                                ,uint256("58282559939ced7ebed7d390559c7ac821932958f8f2399ad40d1188eb0a57f9") //P1170167 S
                                ,uint256("946996f693a33fa1334c1f068574238a463d438b1a3d2cd6d1dd51404a99c73d") //P1176436 S
                            };
                            if( vSkipHashBoincSignCheck.count(pindex->GetBlockHash())==0 )
                                return DoS(20, error(
                                    "ConnectBlock[ResearchAge]: Bad CPID or Block Signature : CPID %s, cpidv2 %s, LBH %s, Bad Hashboinc [%s]",
                                     bb.cpid.c_str(), bb.cpidv2.c_str(),
                                     bb.lastblockhash.c_str(), vtx[0].hashBoinc.c_str()));
                            else LogPrintf("WARNING: ignoring invalid hashBoinc signature on block %s", pindex->GetBlockHash().ToString());
                        }

                        // Mitigate DPOR Relay attack
                        // bb.LastBlockhash should be equal to previous index lastblockhash, in order to check block signature correctly and prevent re-use of lastblockhash
                        if (bb.lastblockhash != pindex->pprev->GetBlockHash().GetHex())
                        {
                            std::string sNarr = "ConnectBlock[ResearchAge] : DPOR Replay attack : lastblockhash != actual last block hash.";
                            LogPrintf("******  %s ***** ", sNarr);
                            if (fTestNet || (pindex->nHeight > 975000)) return DoS(20, error(" %s ",sNarr.c_str()));
                        }

                        const std::set<uint256> vSkipHashdStakeRewardSignCheck =
                        {   uint256("129ae6779d620ec189f8e5148e205efca2dfe31d9f88004b918da3342157b7ff") //T407024
                           ,uint256("c3f85818ba5290aaea1bcbd25b4e136f83acc93999942942bbb25aee2c655f7a") //T407068
                           ,uint256("cf7f1316f92547f611852cf738fc7a4a643a2bb5b9290a33cd2f9425f44cc3f9") //T407099
                           ,uint256("b47085beb075672c6f20d059633d0cad4dba9c5c20f1853d35455b75dc5d54a9") //T407117
                           ,uint256("5a7d437d15bccc41ee8e39143e77960781f3dcf08697a888fa8c4af8a4965682") //T407161
                           ,uint256("aeb3c24277ae1047bda548975a515e9d353d6e12a2952fb733da03f92438fb0f") //T407181
                           ,uint256("fc584b18239f3e3ea78afbbd33af7c6a29bb518b8299f01c1ed4b52d19413d4f") //T407214
                           ,uint256("d5441f7c35eb9ea1b786bbbed820b7f327504301ae70ef2ac3ca3cbc7106236b") //T479114
                        };
                        if( vSkipHashdStakeRewardSignCheck.count(pindex->GetBlockHash())==0 )
                        {
                          if (dStakeReward > ((OUT_POR*1.25)+OUT_INTEREST+1+CoinToDouble(nFees)))
                          {
                            StructCPID st1 = GetLifetimeCPID(pindex->GetCPID(),"ConnectBlock()");
                            GetProofOfStakeReward(nCoinAge, nFees, bb.cpid, true, 2, nTime, pindex, "connectblock_researcher_doublecheck", OUT_POR, OUT_INTEREST, dAccrualAge, dMagnitudeUnit, dAvgMagnitude);
                            if (dStakeReward > ((OUT_POR*1.25)+OUT_INTEREST+1+CoinToDouble(nFees)))
                            {
                                  if (fDebug3) LogPrintf("ConnectBlockError[ResearchAge] : Researchers Reward Pays too much : Interest %f and Research %f and StakeReward %f, OUT_POR %f, with Out_Interest %f for CPID %s ",
                                      (double)bb.InterestSubsidy,(double)bb.ResearchSubsidy,dStakeReward,(double)OUT_POR,(double)OUT_INTEREST,bb.cpid);
                                      return DoS(10,error("ConnectBlock[ResearchAge] : Researchers Reward Pays too much : Interest %f and Research %f and StakeReward %f, OUT_POR %f, with Out_Interest %f for CPID %s ",
                                          (double)bb.InterestSubsidy,(double)bb.ResearchSubsidy,dStakeReward,(double)OUT_POR,(double)OUT_INTEREST,bb.cpid.c_str()));
                            }
                          }
                        }
                      else LogPrintf("WARNING: ignoring invalid hash in dStakeReward check signature on block %s", pindex->GetBlockHash().ToString());
                }
        }

        //Approve first coinstake in DPOR block
        if (IsResearcher(bb.cpid) && IsLockTimeWithinMinutes(GetBlockTime(), GetAdjustedTime(), 15) && !IsResearchAgeEnabled(pindex->nHeight))
        {
            if (bb.ResearchSubsidy > (GetOwedAmount(bb.cpid)+1))
            {
                if (bb.ResearchSubsidy > (GetOwedAmount(bb.cpid)+1))
                {
                    StructCPID strUntrustedHost = GetInitializedStructCPID2(bb.cpid,mvMagnitudes);
                    if (bb.ResearchSubsidy > strUntrustedHost.totalowed)
                    {
                        double deficit = strUntrustedHost.totalowed - bb.ResearchSubsidy;
                        if ( (deficit < -500 && strUntrustedHost.Accuracy > 10) || (deficit < -150 && strUntrustedHost.Accuracy > 5) || deficit < -50)
                        {
                            LogPrintf("ConnectBlock[] : Researchers Reward results in deficit of %f for CPID %s with trust level of %f - (Submitted Research Subsidy %f vs calculated=%f) Hash: %s",
                                   deficit, bb.cpid, (double)strUntrustedHost.Accuracy, bb.ResearchSubsidy,
                                   OUT_POR, vtx[0].hashBoinc.c_str());
                        }
                        else
                        {
                            return error("ConnectBlock[] : Researchers Reward for CPID %s pays too much - (Submitted Research Subsidy %f vs calculated=%f) Hash: %s",
                                         bb.cpid.c_str(), bb.ResearchSubsidy,
                                         OUT_POR, vtx[0].hashBoinc.c_str());
                        }
                    }
                }
            }
        }
    }

    //Gridcoin: Maintain network consensus for Payments and Neural popularity:  (As of 7-5-2015 this is now done exactly every 30 blocks)

    //DPOR - 6/12/2015 - Reject superblocks not hashing to the supermajority:

    if (bb.superblock.length() > 20)
    {
        if(nVersion >= 9)
        {
            // break away from block timing
            if (fDebug) LogPrintf("ConnectBlock: Updating Neural Supermajority (v9 CB) height %d",pindex->nHeight);
            ComputeNeuralNetworkSupermajorityHashes();
            // Prevent duplicate superblocks
            if(nVersion >= 9 && !NeedASuperblock())
                return error(("ConnectBlock: SuperBlock rcvd, but not Needed (too early)"));
        }

        if ((pindex->nHeight > nGrandfather && !fReorganizing) || nVersion >= 9 )
        {
            // 12-20-2015 : Add support for Binary Superblocks
            std::string superblock = UnpackBinarySuperblock(bb.superblock);
            std::string neural_hash = GetQuorumHash(superblock);
            std::string legacy_neural_hash = RetrieveMd5(superblock);
            double popularity = 0;
            std::string consensus_hash = GetNeuralNetworkSupermajorityHash(popularity);
            // Only reject superblock when it is new And when QuorumHash of Block != the Popular Quorum Hash:
            if ((IsLockTimeWithinMinutes(GetBlockTime(), GetAdjustedTime(), 15) || nVersion>=9) && !fColdBoot)
            {
                // Let this take effect together with stakev8
                if (nVersion>=8)
                {
                    try
                    {
                        CBitcoinAddress address;
                        bool validaddressinblock = address.SetString(bb.GRCAddress);
                        validaddressinblock &= address.IsValid();
                        if (!validaddressinblock)
                        {
                            return error("ConnectBlock[] : Superblock staked with invalid GRC address in block");
                        }
                        if (!IsNeuralNodeParticipant(bb.GRCAddress, nTime))
                        {
                            return error("ConnectBlock[] : Superblock staked by ineligible neural node participant");
                        }
                    }
                    catch (...)
                    {
                        return error("ConnectBlock[] : Superblock stake check caused unknown exception with GRC address %s", bb.GRCAddress.c_str());
                    }
                }
                if (!VerifySuperblock(superblock, pindex))
                {
                    return error("ConnectBlock[] : Superblock avg mag below 10; SuperblockHash: %s, Consensus Hash: %s",
                                        neural_hash.c_str(), consensus_hash.c_str());
                }
                if (!IsResearchAgeEnabled(pindex->nHeight))
                {
                    if (consensus_hash != neural_hash && consensus_hash != legacy_neural_hash)
                    {
                        return error("ConnectBlock[] : Superblock hash does not match consensus hash; SuperblockHash: %s, Consensus Hash: %s",
                                        neural_hash.c_str(), consensus_hash.c_str());
                    }
                }
                else
                {
                    if (consensus_hash != neural_hash)
                    {
                        return error("ConnectBlock[] : Superblock hash does not match consensus hash; SuperblockHash: %s, Consensus Hash: %s",
                                        neural_hash.c_str(), consensus_hash.c_str());
                    }
                }

            }
        }

        if(nVersion<9)
        {
            //If we are out of sync, and research age is enabled, and the superblock is valid, load it now, so we can continue checking blocks accurately
            // I would suggest to NOT bother with superblock at all here. It will be loaded in tally.
            if ((OutOfSyncByAge() || fColdBoot || fReorganizing) && IsResearchAgeEnabled(pindex->nHeight) && pindex->nHeight > nGrandfather)
            {
                if (bb.superblock.length() > 20)
                {
                    std::string superblock = UnpackBinarySuperblock(bb.superblock);
                    if (VerifySuperblock(superblock, pindex))
                    {
                        LoadSuperblock(superblock,pindex->nTime,pindex->nHeight);
                        if (fDebug)
                            LogPrintf("ConnectBlock(): Superblock Loaded %d", pindex->nHeight);

                        TallyResearchAverages(pindexBest);
                    }
                    else
                    {
                        if (fDebug3) LogPrintf("ConnectBlock(): Superblock Not Loaded %d", pindex->nHeight);
                    }
                }
            }
            /*
                -- Normal Superblocks are loaded during Tally
            */
        }
    }

    //  End of Network Consensus

    // Gridcoin: Track payments to CPID, and last block paid
    if (pindex->nResearchSubsidy > 0 && IsResearcher(bb.cpid))
    {
        StructCPID stCPID = GetInitializedStructCPID2(bb.cpid,mvResearchAge);

        stCPID.InterestSubsidy += bb.InterestSubsidy;
        stCPID.ResearchSubsidy += bb.ResearchSubsidy;
        if (pindex->nHeight > stCPID.LastBlock)
        {
                stCPID.LastBlock = pindex->nHeight;
                stCPID.BlockHash = pindex->GetBlockHash().GetHex();
        }

        if (pindex->nMagnitude > 0)
        {
                stCPID.Accuracy++;
                stCPID.TotalMagnitude += pindex->nMagnitude;
                stCPID.ResearchAverageMagnitude = stCPID.TotalMagnitude/(stCPID.Accuracy+.01);
        }

        if (pindex->nTime < stCPID.LowLockTime)  stCPID.LowLockTime = pindex->nTime;
        if (pindex->nTime > stCPID.HighLockTime) stCPID.HighLockTime = pindex->nTime;

        mvResearchAge[bb.cpid]=stCPID;
    }

    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("Connect() : WriteBlockIndex for pindex failed");

    if (pindex->nHeight % 5 == 0 && pindex->nHeight > 100)
    {
        std::string errors1 = "";
        LoadAdminMessages(false,errors1);
    }

    // Slow down Retallying when in RA mode so we minimize disruption of the network
    // TODO: Remove this if we can sync to v9 without it.
    if ( (pindex->nHeight % 60 == 0) && IsResearchAgeEnabled(pindex->nHeight) && BlockNeedsChecked(pindex->nTime))
    {
        if(!IsV9Enabled_Tally(pindexBest->nHeight))
            TallyResearchAverages(pindexBest);
    }

    if (IsResearchAgeEnabled(pindex->nHeight) && !OutOfSyncByAge())
    {
        fColdBoot = false;
    }

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
    for (auto const& tx : vtx)
        SyncWithWallets(tx, this, true);

    return true;
}


bool ReorganizeChain(CTxDB& txdb, unsigned &cnt_dis, unsigned &cnt_con, CBlock &blockNew, CBlockIndex* pindexNew);
bool ForceReorganizeToHash(uint256 NewHash)
{
    LOCK(cs_main);
    CTxDB txdb;

    auto mapItem = mapBlockIndex.find(NewHash);
    if(mapItem == mapBlockIndex.end())
        return error("ForceReorganizeToHash: failed to find requested block in block index");

    CBlockIndex* pindexCur = pindexBest;
    CBlockIndex* pindexNew = mapItem->second;
    LogPrintf("** Force Reorganize **");
    LogPrintf(" Current best height %i hash %s", pindexCur->nHeight,pindexCur->GetBlockHash().GetHex());
    LogPrintf(" Target height %i hash %s", pindexNew->nHeight,pindexNew->GetBlockHash().GetHex());

    CBlock blockNew;
    if (!blockNew.ReadFromDisk(pindexNew))
    {
        LogPrintf("ForceReorganizeToHash: Fatal Error while reading new best block.");
        return false;
    }

    unsigned cnt_dis=0;
    unsigned cnt_con=0;
    bool success = false;

    success = ReorganizeChain(txdb, cnt_dis, cnt_con, blockNew, pindexNew);

    if(pindexBest->nChainTrust < pindexCur->nChainTrust)
        LogPrintf("WARNING ForceReorganizeToHash: Chain trust is now less then before!");

    if (!success)
    {
        return error("ForceReorganizeToHash: Fatal Error while setting best chain.");
    }

    AskForOutstandingBlocks(uint256(0));
    LogPrintf("ForceReorganizeToHash: success! height %d hash %s", pindexBest->nHeight,pindexBest->GetBlockHash().GetHex());
    return true;
}

bool DisconnectBlocksBatch(CTxDB& txdb, list<CTransaction>& vResurrect, unsigned& cnt_dis, CBlockIndex* pcommon)
{
    set<string> vRereadCPIDs;
    while(pindexBest != pcommon)
    {
        if(!pindexBest->pprev)
            return error("DisconnectBlocksBatch: attempt to reorganize beyond genesis"); /*fatal*/

        if (fDebug) LogPrintf("DisconnectBlocksBatch: %s",pindexBest->GetBlockHash().GetHex());

        CBlock block;
        if (!block.ReadFromDisk(pindexBest))
            return error("DisconnectBlocksBatch: ReadFromDisk for disconnect failed"); /*fatal*/
        if (!block.DisconnectBlock(txdb, pindexBest))
            return error("DisconnectBlocksBatch: DisconnectBlock %s failed", pindexBest->GetBlockHash().ToString().c_str()); /*fatal*/

        // disconnect from memory
        assert(!pindexBest->pnext);
        if (pindexBest->pprev)
            pindexBest->pprev->pnext = NULL;

        // Queue memory transactions to resurrect.
        // We only do this for blocks after the last checkpoint (reorganisation before that
        // point should only happen with -reindex/-loadblock, or a misbehaving peer.
        for (auto const& tx : boost::adaptors::reverse(block.vtx))
            if (!(tx.IsCoinBase() || tx.IsCoinStake()) && pindexBest->nHeight > Checkpoints::GetTotalBlocksEstimate())
                vResurrect.push_front(tx);

        if(pindexBest->IsUserCPID())
        {
            // remeber the cpid to re-read later
            vRereadCPIDs.insert(pindexBest->GetCPID());
            // The user has no longer staked this block.
            RemoveCPIDBlockHash(pindexBest->GetCPID(), pindexBest->GetBlockHash());
        }

        // New best block
        cnt_dis++;
        pindexBest = pindexBest->pprev;
        hashBestChain = pindexBest->GetBlockHash();
        blockFinder.Reset();
        nBestHeight = pindexBest->nHeight;
        nBestChainTrust = pindexBest->nChainTrust;

        if (!txdb.WriteHashBestChain(pindexBest->GetBlockHash()))
            return error("DisconnectBlocksBatch: WriteHashBestChain failed"); /*fatal*/

    }

    /* fix up after disconnecting, prepare for new blocks */
    if(cnt_dis>0)
    {

        //Block was disconnected - User is Re-eligibile for staking
        StructCPID sMag = GetInitializedStructCPID2(GlobalCPUMiningCPID.cpid,mvMagnitudes);
        nLastBlockSolved = 0;
        if (sMag.initialized)
        {
            sMag.LastPaymentTime = 0;
            mvMagnitudes[GlobalCPUMiningCPID.cpid]=sMag;
        }

        // Resurrect memory transactions that were in the disconnected branch
        for( CTransaction& tx : vResurrect)
            AcceptToMemoryPool(mempool, tx, NULL);

        if (!txdb.TxnCommit())
            return error("DisconnectBlocksBatch: TxnCommit failed"); /*fatal*/

        // Need to reload all contracts
        if (fDebug10) LogPrintf("DisconnectBlocksBatch: LoadAdminMessages");
        std::string admin_messages;
        LoadAdminMessages(true, admin_messages);

        // Tally research averages.
        if(IsV9Enabled_Tally(nBestHeight))
        {
            assert(IsTallyTrigger(pindexBest));
            if (fDebug) LogPrintf("DisconnectBlocksBatch: TallyResearchAverages (v9P %%%d) height %d", TALLY_GRANULARITY, nBestHeight);
            TallyResearchAverages(pindexBest);
        }
        else
        {
            // todo: do something with retired tally? maybe?
        }

        // Re-read researchers history after all blocks disconnected
        if (fDebug10) LogPrintf("DisconnectBlocksBatch: GetLifetimeCPID");
        for( const string& sRereadCPID : vRereadCPIDs )
            GetLifetimeCPID(sRereadCPID,"DisconnectBlocksBatch");

    }
    return true;
}

bool ReorganizeChain(CTxDB& txdb, unsigned &cnt_dis, unsigned &cnt_con, CBlock &blockNew, CBlockIndex* pindexNew)
{
    assert(pindexNew);
    //assert(!pindexNew->pnext);
    //assert(pindexBest || hashBestChain == pindexBest->GetBlockHash());
    //assert(nBestHeight = pindexBest->nHeight && nBestChainTrust == pindexBest->nChainTrust);
    //assert(!pindexBest->pnext);
    assert(pindexNew->GetBlockHash()==blockNew.GetHash());
    /* note: it was already determined that this chain is better than current best */
    /* assert(pindexNew->nChainTrust > nBestChainTrust); but may be overriden by command */
    assert( !pindexGenesisBlock == !pindexBest );

    list<CTransaction> vResurrect;
    list<CBlockIndex*> vConnect;
    set<string> vRereadCPIDs;

    /* find fork point */
    CBlockIndex *pcommon = NULL;
    if(pindexGenesisBlock)
    {
        pcommon = pindexNew;
        while( pcommon->pnext==NULL && pcommon!=pindexBest )
        {
            pcommon = pcommon->pprev;

            if(!pcommon)
                return error("ReorganizeChain: unable to find fork root");
        }

        if(pcommon != pindexBest)
        {
            pcommon = FindTallyTrigger(pcommon);
            if(!pcommon)
                return error("ReorganizeChain: unable to find fork root with tally point");
        }

        if (pcommon!=pindexBest || pindexNew->pprev!=pcommon)
        {
            LogPrintf("ReorganizeChain: from {%s %d}\n"
                     "ReorganizeChain: comm {%s %d}\n"
                     "ReorganizeChain: to   {%s %d}\n"
                     "REORGANIZE: disconnect %d, connect %d blocks"
                ,pindexBest->GetBlockHash().GetHex().c_str(), pindexBest->nHeight
                ,pcommon->GetBlockHash().GetHex().c_str(), pcommon->nHeight
                ,pindexNew->GetBlockHash().GetHex().c_str(), pindexNew->nHeight
                ,pindexBest->nHeight - pcommon->nHeight
                ,pindexNew->nHeight - pcommon->nHeight);
        }
    }

    /* disconnect blocks */
    if(pcommon!=pindexBest)
    {
        if (!txdb.TxnBegin())
            return error("ReorganizeChain: TxnBegin failed");
        if(!DisconnectBlocksBatch(txdb, vResurrect, cnt_dis, pcommon))
        {
            error("ReorganizeChain: DisconnectBlocksBatch() failed");
            LogPrintf("This is fatal error. Chain index may be corrupt. Aborting.\n"
                      "Please Reindex the chain and Restart.");
            exit(1); //todo
        }

        int nMismatchSpent;
        int64_t nBalanceInQuestion;
        pwalletMain->FixSpentCoins(nMismatchSpent, nBalanceInQuestion);
    }

    if (fDebug && cnt_dis>0) LogPrintf("ReorganizeChain: disconnected %d blocks",cnt_dis);

    for(CBlockIndex *p = pindexNew; p != pcommon; p=p->pprev)
        vConnect.push_front(p);

    /* Connect blocks */
    for(auto const pindex : vConnect)
    {
        CBlock block_load;
        CBlock &block = (pindex==pindexNew)? blockNew : block_load;

        if(pindex!=pindexNew)
        {
            if (!block.ReadFromDisk(pindex))
                return error("ReorganizeChain: ReadFromDisk for connect failed");
            assert(pindex->GetBlockHash()==block.GetHash());
        }
        else
        {
            assert(pindex==pindexNew);
            assert(pindexNew->GetBlockHash()==block.GetHash());
            assert(pindexNew->GetBlockHash()==blockNew.GetHash());
        }

        uint256 hash = block.GetHash();
        uint256 nBestBlockTrust;

        if (fDebug) LogPrintf("ReorganizeChain: connect %s",hash.ToString());

        if (!txdb.TxnBegin())
            return error("ReorganizeChain: TxnBegin failed");

        if (pindexGenesisBlock == NULL)
        {
            if(hash != (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
            {
                txdb.TxnAbort();
                return error("ReorganizeChain: genesis block hash does not match");
            }
            pindexGenesisBlock = pindex;
        }
        else
        {
            assert(pindex->GetBlockHash()==block.GetHash());
            assert(pindex->pprev == pindexBest);
            if (!block.ConnectBlock(txdb, pindex, false, false))
            {
                txdb.TxnAbort();
                error("ReorganizeChain: ConnectBlock %s failed", hash.ToString().c_str());
                LogPrintf("Previous block %s",pindex->pprev->GetBlockHash().ToString());
                InvalidChainFound(pindex);
                return false;
            }
        }

        // Delete redundant memory transactions
        for (auto const& tx : block.vtx)
        {
            mempool.remove(tx);
            mempool.removeConflicts(tx);
        }

        if (!txdb.WriteHashBestChain(pindex->GetBlockHash()))
        {
            txdb.TxnAbort();
            return error("ReorganizeChain: WriteHashBestChain failed");
        }

        // Make sure it's successfully written to disk before changing memory structure
        if (!txdb.TxnCommit())
            return error("ReorganizeChain: TxnCommit failed");

        // Add to current best branch
        if(pindex->pprev)
        {
            assert( !pindex->pprev->pnext );
            pindex->pprev->pnext = pindex;
            nBestBlockTrust = pindex->nChainTrust - pindex->pprev->nChainTrust;
        }
        else
            nBestBlockTrust = pindex->nChainTrust;

        // update best block
        hashBestChain = hash;
        pindexBest = pindex;
        blockFinder.Reset();
        nBestHeight = pindexBest->nHeight;
        nBestChainTrust = pindexBest->nChainTrust;
        nTimeBestReceived =  GetAdjustedTime();
        cnt_con++;

        // Load recent contracts
        std::string admin_messages;
        LoadAdminMessages(false, admin_messages);

        if(IsV9Enabled_Tally(nBestHeight))
        {
            // quorum not needed
            // Tally research averages.
            if(IsTallyTrigger(pindexBest))
            {
                if (fDebug) LogPrintf("ReorganizeChain: TallyNetworkAverages (v9N %%%d) height %d",TALLY_GRANULARITY,nBestHeight);
                TallyResearchAverages(pindexBest);
            }
        }
        else
        {
            //TODO: do something with retired tally?
        }

        if(pindex->IsUserCPID()) // is this needed?
            GetLifetimeCPID(pindex->cpid.GetHex(), "ReorganizeChain");
    }

    if (fDebug && (cnt_dis>0 || cnt_con>1))
        LogPrintf("ReorganizeChain: Disconnected %d and Connected %d blocks.",cnt_dis,cnt_con);

    return true;
}

bool SetBestChain(CTxDB& txdb, CBlock &blockNew, CBlockIndex* pindexNew)
{
    unsigned cnt_dis=0;
    unsigned cnt_con=0;
    bool success = false;
    const auto origBestIndex = pindexBest;

    success = ReorganizeChain(txdb, cnt_dis, cnt_con, blockNew, pindexNew);

    if(origBestIndex && origBestIndex->nChainTrust > nBestChainTrust)
    {
        LogPrintf("SetBestChain: Reorganize caused lower chain trust than before. Reorganizing back.");
        CBlock origBlock;
        if (!origBlock.ReadFromDisk(origBestIndex))
            return error("SetBestChain: Fatal Error while reading original best block");
        success = ReorganizeChain(txdb, cnt_dis, cnt_con, origBlock, origBestIndex);
    }

    if(!success)
        return false;

    /* Fix up after block connecting */


    //std::set<uint128> connected_cpids;


    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload)
    {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }

    if(IsV9Enabled_Tally(nBestHeight))
    {
        // Update quorum data.
        if ((nBestHeight % 3) == 0)
        {
            if (fDebug) LogPrintf("SetBestChain: Updating Neural Supermajority (v9 %%3) height %d",nBestHeight);
            ComputeNeuralNetworkSupermajorityHashes();
        }
        // Update quorum data.
        if ((nBestHeight % 10) == 0 && !OutOfSyncByAge() && NeedASuperblock())
        {
            if (fDebug) LogPrintf("SetBestChain: Updating Neural Quorum (v9 M) height %d",nBestHeight);
            UpdateNeuralNetworkQuorumData();
        }
    }
    else if (!fIsInitialDownload)
        // Retally after reorganize to sync up amounts owed.
        TallyResearchAverages(pindexNew);

    if (fDebug)
    {
        LogPrintf("{SBC} {%s %d}  trust=%s  date=%s",
               hashBestChain.ToString(), nBestHeight,
               CBigNum(nBestChainTrust).ToString(),
               DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()));
    }
    else
        LogPrintf("{SBC} new best {%s %d} ; ",hashBestChain.ToString(), nBestHeight);

    std::string strCmd = GetArg("-blocknotify", "");
    if (!fIsInitialDownload && !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    // Perform Gridcoin services now that w have a new head.
    // Remove V9 checks after the V9 switch.
    // TODO: ???
    if(IsV9Enabled(nBestHeight))
        GridcoinServices();

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

    for (auto const& txin : vin)
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
            LogPrintf("coin age nValueIn=%" PRId64 " nTimeDiff=%d bnCentSecond=%s", nValueIn, nTime - txPrev.nTime, bnCentSecond.ToString());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug && GetBoolArg("-printcoinage"))
        LogPrintf("coin age bnCoinDay=%s", bnCoinDay.ToString());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}

// ppcoin: total coin age spent in block, in the unit of coin-days.
bool CBlock::GetCoinAge(uint64_t& nCoinAge) const
{
    nCoinAge = 0;

    CTxDB txdb("r");
    for (auto const& tx : vtx)
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
        LogPrintf("block coin age total nCoinDays=%" PRIu64, nCoinAge);
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
    BlockMap::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
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
        LogPrintf("AddToBlockIndex() : ComputeNextStakeModifier() failed");
    }
    pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);

    // Add to mapBlockIndex
    BlockMap::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
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
        if (!SetBestChain(txdb, *this, pindexNew))
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

bool CBlock::CheckBlock(std::string sCaller, int height1, int64_t Mint, bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig, bool fLoadingIndex) const
{

    if (GetHash()==hashGenesisBlock || GetHash()==hashGenesisBlockTestNet) return true;
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty() || vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return DoS(100, error("CheckBlock[] : size limits failed"));

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(GetPoWHash(), nBits))
        return DoS(50, error("CheckBlock[] : proof of work failed"));

    //Reject blocks with diff that has grown to an extrordinary level (should never happen)
    double blockdiff = GetBlockDifficulty(nBits);
    if (height1 > nGrandfather && blockdiff > 10000000000000000)
    {
       return DoS(1, error("CheckBlock[] : Block Bits larger than 10000000000000000."));
    }

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return DoS(100, error("CheckBlock[] : first tx is not coinbase"));
    for (unsigned int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return DoS(100, error("CheckBlock[] : more than one coinbase"));
    //Research Age
    MiningCPID bb = DeserializeBoincBlock(vtx[0].hashBoinc,nVersion);
    if(nVersion<9)
    {
        //For higher security, plus lets catch these bad blocks before adding them to the chain to prevent reorgs:
        if (IsResearcher(bb.cpid) && IsProofOfStake() && height1 > nGrandfather && IsResearchAgeEnabled(height1) && BlockNeedsChecked(nTime) && !fLoadingIndex)
        {
            double blockVersion = BlockVersion(bb.clientversion);
            double cvn = ClientVersionNew();
            if (fDebug10) LogPrintf("BV %f, CV %f   ",blockVersion,cvn);
            // Enforce Beacon Age
            if (blockVersion < 3588 && height1 > 860500 && !fTestNet)
                return error("CheckBlock[]:  Old client spamming new blocks after mandatory upgrade ");
        }

        //Orphan Flood Attack
        if (height1 > nGrandfather)
        {
            double bv = BlockVersion(bb.clientversion);
            double cvn = ClientVersionNew();
            if (fDebug10) LogPrintf("BV %f, CV %f   ",bv,cvn);
            // Enforce Beacon Age
            if (bv < 3588 && height1 > 860500 && !fTestNet)
                return error("CheckBlock[]:  Old client spamming new blocks after mandatory upgrade ");
        }
    }

    if (IsResearcher(bb.cpid) && height1 > nGrandfather && BlockNeedsChecked(nTime))
    {
        if (bb.projectname.empty() && !IsResearchAgeEnabled(height1))
            return DoS(1,error("CheckBlock::PoR Project Name invalid"));

        if (!fLoadingIndex)
        {
            bool cpidresult = false;
            int cpidV2CutOverHeight = fTestNet ? 0 : 97000;
            int cpidV3CutOverHeight = fTestNet ? 196300 : 725000;
            if (height1 < cpidV2CutOverHeight)
            {
                cpidresult = IsCPIDValid_Retired(bb.cpid,bb.enccpid);
            }
            else if (height1 <= cpidV3CutOverHeight)
            {
                cpidresult = CPID_IsCPIDValid(bb.cpid, bb.cpidv2, (uint256)bb.lastblockhash);
            }
            else
            {

                cpidresult = (bb.lastblockhash.size()==64)
                    && (bb.BoincSignature.size()>=16)
                    && (bb.BoincSignature.find(' ')==std::string::npos);

                /* This is not used anywhere, so let it be.
                cpidresult = cpidresult
                    && (bb.BoincPublicKey.size()==130)
                    && (bb.BoincPublicKey.find(' ')==std::string::npos);
                */

                /* full "v3" signature check is performed in ConnectBlock */
            }

            if(!cpidresult)
                return DoS(20, error(
                            "Bad CPID or Block Signature : height %i, CPID %s, cpidv2 %s, LBH %s, Bad Hashboinc [%s]",
                             height1, bb.cpid.c_str(), bb.cpidv2.c_str(),
                             bb.lastblockhash.c_str(), vtx[0].hashBoinc.c_str()));
        }
    }

    // Gridcoin: check proof-of-stake block signature
    if (IsProofOfStake() && height1 > nGrandfather)
    {
        //Mint limiter checks 1-20-2015
        double PORDiff = GetBlockDifficulty(nBits);
        double mint1 = CoinToDouble(Mint);
        double total_subsidy = bb.ResearchSubsidy + bb.InterestSubsidy;
        double limiter = MintLimiter(PORDiff,bb.RSAWeight,bb.cpid,GetBlockTime());
        if (fDebug10) LogPrintf("CheckBlock[]: TotalSubsidy %f, Height %i, %s, %f, Res %f, Interest %f, hb: %s ",
                             total_subsidy, height1, bb.cpid,
                             mint1,bb.ResearchSubsidy,bb.InterestSubsidy,vtx[0].hashBoinc);
        if (total_subsidy < limiter)
        {
            if (fDebug3) LogPrintf("****CheckBlock[]: Total Mint too Small %s, mint %f, Res %f, Interest %f, hash %s ",bb.cpid,
                                mint1,bb.ResearchSubsidy,bb.InterestSubsidy,vtx[0].hashBoinc);
            //1-21-2015 - Prevent Hackers from spamming the network with small blocks
            return error("****CheckBlock[]: Total Mint too Small %f < %f Research %f Interest %f BOINC %s",
                         total_subsidy,limiter,bb.ResearchSubsidy,bb.InterestSubsidy,vtx[0].hashBoinc);
        }

        if (fCheckSig && !CheckBlockSignature())
            return DoS(100, error("CheckBlock[] : bad proof-of-stake block signature"));
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
        {
            if (vtx[i].IsCoinStake())
            {
                LogPrintf("Found more than one coinstake in coinbase at location %d", i);
                return DoS(100, error("CheckBlock[] : more than one coinstake"));
            }
        }
    }

    // Check transactions
    for (auto const& tx : vtx)
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
    for (auto const& tx : vtx)
    {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock[] : duplicate transaction"));

    unsigned int nSigOps = 0;
    for (auto const& tx : vtx)
    {
        nSigOps += tx.GetLegacySigOpCount();
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock[] : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock[] : hashMerkleRoot mismatch"));

    //if (fDebug3) LogPrintf(".EOCB.");
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
    BlockMap::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found"));
    CBlockIndex* pindexPrev = (*mi).second;
    int nHeight = pindexPrev->nHeight+1;

    // The block height at which point we start rejecting v7 blocks and
    // start accepting v8 blocks.
    if(       (IsProtocolV2(nHeight) && nVersion < 7)
              || (IsV8Enabled(nHeight) && nVersion < 8)
              || (IsV9Enabled(nHeight) && nVersion < 9)
              )
        return DoS(20, error("AcceptBlock() : reject too old nVersion = %d", nVersion));
    else if( (!IsProtocolV2(nHeight) && nVersion >= 7)
             ||(!IsV8Enabled(nHeight) && nVersion >= 8)
             ||(!IsV9Enabled(nHeight) && nVersion >= 9)
             )
        return DoS(100, error("AcceptBlock() : reject too new nVersion = %d", nVersion));

    if (IsProofOfWork() && nHeight > LAST_POW_BLOCK)
        return DoS(100, error("AcceptBlock() : reject proof-of-work at height %d", nHeight));

    if (nHeight > nGrandfather || nHeight >= 999000)
    {
            // Check coinbase timestamp
            if (GetBlockTime() > FutureDrift((int64_t)vtx[0].nTime, nHeight))
            {
                return DoS(80, error("AcceptBlock() : coinbase timestamp is too early"));
            }
            // Check timestamp against prev
            if (GetBlockTime() <= pindexPrev->GetPastTimeLimit() || FutureDrift(GetBlockTime(), nHeight) < pindexPrev->GetBlockTime())
                return DoS(60, error("AcceptBlock() : block's timestamp is too early"));
            // Check proof-of-work or proof-of-stake
            if (nBits != GetNextTargetRequired(pindexPrev, IsProofOfStake()))
                return DoS(100, error("AcceptBlock() : incorrect %s", IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));
    }

    for (auto const& tx : vtx)
    {
        // Check that all transactions are finalized
        if (!IsFinalTx(tx, nHeight, GetBlockTime()))
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));

        // Verify beacon contract if a transaction contains a beacon contract
        // Current bad contracts in chain would cause a fork on sync, skip them
        if (nVersion>=9 && !VerifyBeaconContractTx(tx))
            return DoS(25, error("CheckBlock[] : bad beacon contract found in tx %s contained within block; rejected", tx.GetHash().ToString().c_str()));
    }

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

    uint256 hashProof;

    // Verify hash target and signature of coinstake tx
    if ((nHeight > nGrandfather || nHeight >= 999000) && nVersion <= 7)
    {
                if (IsProofOfStake())
                {
                    uint256 targetProofOfStake;
                    if (!CheckProofOfStake(pindexPrev, vtx[1], nBits, hashProof, targetProofOfStake, vtx[0].hashBoinc, generated_by_me, nNonce) && (IsLockTimeWithinMinutes(GetBlockTime(), GetAdjustedTime(), 600) || nHeight >= 999000))
                    {
                        return error("WARNING: AcceptBlock(): check proof-of-stake failed for block %s, nonce %f    ", hash.ToString().c_str(),(double)nNonce);
                    }

                }
    }
    if (nVersion >= 8)
    {
        //must be proof of stake
        //no grandfather exceptions
        //if (IsProofOfStake())
        if(!CheckProofOfStakeV8(pindexPrev, *this, generated_by_me, hashProof))
        {
            error("WARNING: AcceptBlock(): check proof-of-stake failed for block %s, nonce %f    ", hash.ToString().c_str(),(double)nNonce);
            LogPrintf(" prev %s",pindexPrev->GetBlockHash().ToString());
            return false;
        }
    }

    if(nVersion<9)
    {
        // Verify proof of research.
        if(!CheckProofOfResearch(pindexPrev, *this))
        {
            return error("WARNING: AcceptBlock(): check proof-of-research failed for block %s, nonce %i", hash.ToString().c_str(), nNonce);
        }
    }
    /*else Do not check v9 rewards here as context here is insufficient and it is
      checked again in ConnectBlock */

    // PoW is checked in CheckBlock[]
    if (IsProofOfWork())
    {
        hashProof = GetPoWHash();
    }

    //Grandfather
    if (nHeight > nGrandfather)
    {
        // Check that the block chain matches the known block chain up to a checkpoint
        if (!Checkpoints::CheckHardened(nHeight, hash))
            return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

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
        for (auto const& pnode : vNodes)
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    if (fDebug) LogPrintf("{ACC}");
    nLastAskedForBlocks=GetAdjustedTime();
    ResetTimerMain("OrphanBarrage");
    return true;
}


uint256 CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    if (bnTarget <= 0) return 0;
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

bool ServicesIncludesNN(CNode* pNode)
{
    return (Contains(pNode->strSubVer,"1999")) ? true : false;
}

bool VerifySuperblock(const std::string& superblock, const CBlockIndex* parent)
{
    // Pre-condition checks.
    if(!parent)
    {
        LogPrintf("Invalid block passed to VerifySuperblock");
        return false;
    }

    if(superblock.length() <= 20)
    {
        LogPrintf("Invalid superblock passed to VerifySuperblock");
        return false;
    }

    // Validate superblock contents.
    bool bPassed = true;

    if(parent->nVersion < 8)
    {
        double out_avg = 0;
        double out_beacon_count=0;
        double out_participant_count=0;
        double avg_mag = GetSuperblockAvgMag(superblock,out_beacon_count,out_participant_count,out_avg,false, parent->nHeight);

        // New rules added here:
        // Before block version- and stake engine 8 there used to be a requirement
        // that the average magnitude across all researchers must be at least 10.
        // This is not necessary but cannot be changed until a mandatory is released.

        if (out_avg < 10 && fTestNet)  bPassed = false;
        if (out_avg < 70 && !fTestNet) bPassed = false;

        if (avg_mag < 10 && !fTestNet) bPassed = false;
    }
    else
    {
        // Block version above 8
        // none, as they are easy to work arount and complicate sb production

        // previously:
        // * low/high limit of average of researcher magnitudes
        // * low limit of project avg rac
        // * count of researchers within 10% of their beacon count
        // * count of projects at least half of previous sb project count
    }

    if (!bPassed)
    {
        if (fDebug) LogPrintf(" Verification of Superblock Failed ");
    }

    return bPassed;
}

bool NeedASuperblock()
{
    bool bDireNeedOfSuperblock = false;
    std::string superblock = ReadCache("superblock","all").value;
    if (superblock.length() > 20 && !OutOfSyncByAge())
    {
        if (!VerifySuperblock(superblock, pindexBest))
            bDireNeedOfSuperblock = true;
        /*
         // Check project count in last superblock
         double out_project_count = 0;
         double out_whitelist_count = 0;
         GetSuperblockProjectCount(superblock, out_project_count, out_whitelist_count);
         */
    }

    int64_t superblock_age = GetAdjustedTime() - ReadCache("superblock", "magnitudes").timestamp;
    if (superblock_age > GetSuperblockAgeSpacing(nBestHeight))
        bDireNeedOfSuperblock = true;

    if(bDireNeedOfSuperblock && pindexBest && pindexBest->nVersion>=9)
    {
        // If all the checks indicate true and is v9, look 15 blocks back to
        // prevent duplicate superblocks
        for(CBlockIndex *pindex = pindexBest;
            pindex && pindex->nHeight + 15 > nBestHeight;
            pindex = pindex->pprev)
        {
            if(pindex->nIsSuperBlock)
                return false;
        }
    }

    return bDireNeedOfSuperblock;
}




void GridcoinServices()
{

    //Dont do this on headless - SeP
    if(fQtActive)
    {
       if ((nBestHeight % 125) == 0)
       {
            GetGlobalStatus();
            bForceUpdate=true;
            uiInterface.NotifyBlocksChanged();
       }
    }

    // Services thread activity

    if(IsV9Enabled_Tally(nBestHeight))
    {
        // in SetBestChain
    }
    else
    {
        int64_t superblock_age = GetAdjustedTime() - ReadCache("superblock", "magnitudes").timestamp;
        bool bNeedSuperblock = (superblock_age > (GetSuperblockAgeSpacing(nBestHeight)));
        if ( nBestHeight % 3 == 0 && NeedASuperblock() ) bNeedSuperblock=true;

        if (fDebug10) LogPrintf(" MRSA %" PRId64 ", BH %d", superblock_age, nBestHeight);

        if (bNeedSuperblock)
        {
            if ((nBestHeight % 3) == 0)
            {
                if (fDebug) LogPrintf("SVC: Updating Neural Supermajority (v3 A) height %d",nBestHeight);
                ComputeNeuralNetworkSupermajorityHashes();
            }
            if ((nBestHeight % 3) == 0 && !OutOfSyncByAge())
            {
                if (fDebug) LogPrintf("SVC: Updating Neural Quorum (v3 A) height %d",nBestHeight);
                if (fDebug10) LogPrintf("#CNNSH# ");
                UpdateNeuralNetworkQuorumData();
            }

            // Perform retired tallies between the V9 block switch (1144000) and the
            // V9 tally switch (1144120) or else blocks will be rejected in between.
            if (IsV9Enabled(nBestHeight) && (nBestHeight % 20) == 0)
            {
                if (fDebug) LogPrintf("SVC: set off Tally (v3 B) height %d",nBestHeight);
                if (fDebug10) LogPrintf("#TIB# ");
                TallyResearchAverages(pindexBest);
            }
        }
        else
        {
            // When superblock is not old, Tally every N blocks:
            int nTallyGranularity = fTestNet ? 60 : 20;
            if (IsV9Enabled(nBestHeight) && (nBestHeight % nTallyGranularity) == 0)
            {
                if (fDebug) LogPrintf("SVC: set off Tally (v3 C) height %d",nBestHeight);
                if (fDebug3) LogPrintf("TIB1 ");
                TallyResearchAverages(pindexBest);
            }

            if ((nBestHeight % 5)==0)
            {
                if (fDebug) LogPrintf("SVC: Updating Neural Supermajority (v3 D) height %d",nBestHeight);
                ComputeNeuralNetworkSupermajorityHashes();
            }
            if ((nBestHeight % 5)==0 && !OutOfSyncByAge())
            {
                if (fDebug) LogPrintf("SVC: Updating Neural Quorum (v3 E) height %d",nBestHeight);
                if (fDebug3) LogPrintf("CNNSH2 ");
                UpdateNeuralNetworkQuorumData();
            }
        }
    }

    if (TimerMain("clearcache",1000))
    {
        ClearCache("neural_data");
    }


    //Dont perform the following functions if out of sync
    if (pindexBest->nHeight < nGrandfather || OutOfSyncByAge())
        return;

    if (fDebug) LogPrintf(" {SVC} ");

    //Backup the wallet once per 900 blocks or as specified in config:
    int nWBI = GetArg("-walletbackupinterval", 900);
    if (nWBI && TimerMain("backupwallet", nWBI))
    {
        bool bWalletBackupResults = BackupWallet(*pwalletMain, GetBackupFilename("wallet.dat"));
        bool bConfigBackupResults = BackupConfigFile(GetBackupFilename("gridcoinresearch.conf"));
        LogPrintf("Daily backup results: Wallet -> %s Config -> %s", (bWalletBackupResults ? "true" : "false"), (bConfigBackupResults ? "true" : "false"));
    }

    if (TimerMain("MyNeuralMagnitudeReport",30))
    {
        try
        {
            if (msNeuralResponse.length() < 25 && IsResearcher(msPrimaryCPID))
            {
                AsyncNeuralRequest("explainmag",msPrimaryCPID,5);
                if (fDebug3) LogPrintf("Async explainmag sent for %s.",msPrimaryCPID);
            }
            if (fDebug3) LogPrintf("MR Complete");
        }
        catch (std::exception &e)
        {
            LogPrintf("Error in MyNeuralMagnitudeReport1.");
        }
        catch(...)
        {
            LogPrintf("Error in MyNeuralMagnitudeReport.");
        }
    }

    // Every N blocks as a Synchronized TEAM:
    if ((nBestHeight % 30) == 0)
    {
        //Sync RAC with neural network IF superblock is over 24 hours Old, Or if we have No superblock (in case of the latter, age will be 45 years old)
        // Note that nodes will NOT accept superblocks without a supermajority hash, so the last block will not be in memory unless it is a good superblock.
        // Let's start syncing the neural network as soon as the LAST superblock is over 12 hours old.
        // Also, lets do this as a TEAM exactly every 30 blocks (~30 minutes) to try to reach an EXACT consensus every half hour:
        // For effeciency, the network sleeps for 20 hours after a good superblock is accepted
        if (NeedASuperblock() && IsNeuralNodeParticipant(DefaultWalletAddress(), GetAdjustedTime()))
        {
            if (fDebug3) LogPrintf("FSWDPOR ");
            FullSyncWithDPORNodes();
        }
    }

    if (( (nBestHeight-10) % 30 ) == 0)
    {
            // 10 Blocks after the network started syncing the neural network as a team, ask the neural network to come to a quorum
            if (NeedASuperblock() && IsNeuralNodeParticipant(DefaultWalletAddress(), GetAdjustedTime()))
            {
                // First verify my node has a synced contract
                std::string contract;
                contract = NN::GetNeuralContract();
                if (VerifySuperblock(contract, pindexBest))
                {
                        AsyncNeuralRequest("quorum","gridcoin",25);
                }
            }
    }


    if (TimerMain("send_beacon",180))
    {
        std::string tBeaconPublicKey = GetBeaconPublicKey(GlobalCPUMiningCPID.cpid,true);
        if (tBeaconPublicKey.empty() && IsResearcher(GlobalCPUMiningCPID.cpid))
        {
            std::string sOutPubKey = "";
            std::string sOutPrivKey = "";
            std::string sError = "";
            std::string sMessage = "";
            bool fResult = AdvertiseBeacon(sOutPrivKey,sOutPubKey,sError,sMessage);
            if (!fResult)
            {
                LogPrintf("BEACON ERROR!  Unable to send beacon %s, %s",sError, sMessage);
                LOCK(MinerStatus.lock);
                msMiningErrors6 = _("Unable To Send Beacon! Unlock Wallet!");
            }
        }
    }

    if (TimerMain("gather_cpids",480))
        msNeuralResponse.clear();

/*
    // Check for updates once per day.
    if(GetAdjustedTime() - nLastCheckedForUpdate > 24 * 60 * 60)
    {
        nLastCheckedForUpdate = GetAdjustedTime();

        if (fDebug3) LogPrintf("Checking for upgrade...");
        if(Restarter::IsUpgradeAvailable())
        {
            LogPrintf("Upgrade available.");
            if(GetBoolArg("-autoupgrade", false))
            {
                LogPrintf("Upgrading client.");
                Restarter::UpgradeClient();
            }
        }
    }
*/

    if (fDebug10) LogPrintf(" {/SVC} ");
}



bool AskForOutstandingBlocks(uint256 hashStart)
{
    if (IsLockTimeWithinMinutes(nLastAskedForBlocks, GetAdjustedTime(), 2)) return true;
    nLastAskedForBlocks = GetAdjustedTime();

    int iAsked = 0;
    LOCK(cs_vNodes);
    for (auto const& pNode : vNodes)
    {
                pNode->ClearBanned();
                if (!pNode->fClient && !pNode->fOneShot && (pNode->nStartingHeight > (nBestHeight - 144)) && (pNode->nVersion < NOBLKS_VERSION_START || pNode->nVersion >= NOBLKS_VERSION_END) )
                {
                        if (hashStart==uint256(0))
                        {
                            pNode->PushGetBlocks(pindexBest, uint256(0), true);
                        }
                        else
                        {
                            CBlockIndex* pblockindex = mapBlockIndex[hashStart];
                            if (pblockindex)
                            {
                                pNode->PushGetBlocks(pblockindex, uint256(0), true);
                            }
                            else
                            {
                                return error("Unable to find block index %s",hashStart.ToString().c_str());
                            }
                        }
                        LogPrintf(".B.");
                        iAsked++;
                        if (iAsked > 10) break;
                }
    }
    return true;
}


void ClearOrphanBlocks()
{
    LOCK(cs_main);
    for(auto it = mapOrphanBlocks.begin(); it != mapOrphanBlocks.end(); it++)
    {
        delete it->second;
    }

    mapOrphanBlocks.clear();
    mapOrphanBlocksByPrev.clear();
}

void CleanInboundConnections(bool bClearAll)
{
        if (IsLockTimeWithinMinutes(nLastCleaned, GetAdjustedTime(), 10)) return;
        nLastCleaned = GetAdjustedTime();
        LOCK(cs_vNodes);
        for(CNode* pNode : vNodes)
        {
                pNode->ClearBanned();
                if (pNode->nStartingHeight < (nBestHeight-1000) || bClearAll)
                {
                        pNode->fDisconnect=true;
                }
        }
        LogPrintf("Cleaning inbound connections");
}

bool WalletOutOfSync()
{
    LOCK(cs_main);

    // Only trigger an out of sync condition if the node has synced near the best block prior to going out of sync.
    bool bSyncedCloseToTop = nBestHeight > GetNumBlocksOfPeers() - 1000;
    return OutOfSyncByAge() && bSyncedCloseToTop;
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock, bool generated_by_me)
{
    AssertLockHeld(cs_main);

    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.ToString().c_str());

    // ppcoin: check proof-of-stake
    // Limited duplicity on stake: prevents block flood attack
    // Duplicate stake allowed only when there is orphan child block
    if (pblock->IsProofOfStake() && setStakeSeen.count(pblock->GetProofOfStake()) && !mapOrphanBlocksByPrev.count(hash))
        return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for block %s", pblock->GetProofOfStake().first.ToString().c_str(),
        pblock->GetProofOfStake().second,
        hash.ToString().c_str());

    if (pblock->hashPrevBlock != hashBestChain)
    {
        // Extra checks to prevent "fill up memory by spamming with bogus blocks"
        const CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
        if(pcheckpoint != NULL)
        {
            int64_t deltaTime = pblock->GetBlockTime() - pcheckpoint->nTime;
            if (deltaTime < 0)
            {
                if (pfrom)
                    pfrom->Misbehaving(1);
                return error("ProcessBlock() : block with timestamp before last checkpoint");
            }
        }
    }

    // Preliminary checks
    if (!pblock->CheckBlock("ProcessBlock", pindexBest->nHeight, 100*COIN))
        return error("ProcessBlock() : CheckBlock FAILED");

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock))
    {
        // *****      This area covers Gridcoin Orphan Handling      *****
        if (WalletOutOfSync())
        {
            if (TimerMain("OrphanBarrage",100))
            {
                // If we stay out of sync for more than 25 orphans and never recover without accepting a block - attempt to recover the node- if we recover, reset the counters.
                // We reset these counters every time a block is accepted successfully in AcceptBlock().
                // Note: This code will never actually be exercised unless the wallet stays out of sync for a very long time - approx. 24 hours - the wallet normally recovers on its own without this code.
                // I'm leaving this in for people who may be on vacation for a long time - it may keep an external node running when everything else fails.
                if (TimerMain("CheckForFutileSync", 25))
                {
                    ClearOrphanBlocks();
                    setStakeSeen.clear();
                    setStakeSeenOrphan.clear();
                }

                LogPrintf("Clearing mapAlreadyAskedFor.");
                mapAlreadyAskedFor.clear();
                AskForOutstandingBlocks(uint256(0));
            }
        }
        else
        {
            // If we successfully synced we can reset the futile state.
            ResetTimerMain("CheckForFutileSync");
        }

        LogPrintf("ProcessBlock: ORPHAN BLOCK, prev=%s", pblock->hashPrevBlock.ToString());
        // ppcoin: check proof-of-stake
        if (pblock->IsProofOfStake())
        {
            // Limited duplicity on stake: prevents block flood attack
            // Duplicate stake allowed only when there is orphan child block
            if (setStakeSeenOrphan.count(pblock->GetProofOfStake()) &&
                !mapOrphanBlocksByPrev.count(hash))
                return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for orphan block %s",
                             pblock->GetProofOfStake().first.ToString().c_str(),
                             pblock->GetProofOfStake().second,
                             hash.ToString().c_str());
            else
                setStakeSeenOrphan.insert(pblock->GetProofOfStake());
        }

        CBlock* pblock2 = new CBlock(*pblock);
        mapOrphanBlocks.insert(make_pair(hash, pblock2));
        mapOrphanBlocksByPrev.insert(make_pair(pblock->hashPrevBlock, pblock2));

        // Ask this guy to fill in what we're missing
        if (pfrom)
        {
            pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(pblock2), true);
            // ppcoin: getblocks may not obtain the ancestor block rejected
            // earlier by duplicate-stake check so we ask for it again directly
            if (!IsInitialBlockDownload())
                pfrom->AskFor(CInv(MSG_BLOCK, WantedByOrphan(pblock2)));
            // Ask a few other nodes for the missing block

        }
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock(generated_by_me))
        return error("ProcessBlock() : AcceptBlock FAILED");

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
            CBlock* pblockOrphan = mi->second;
            if (pblockOrphan->AcceptBlock(generated_by_me))
                vWorkQueue.push_back(pblockOrphan->GetHash());
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            setStakeSeenOrphan.erase(pblockOrphan->GetProofOfStake());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);

    }

    LogPrintf("{PB}: ACC; ");

    // Compatiblity while V8 is in use. Can be removed after the V9 switch.
    if(IsV9Enabled(pindexBest->nHeight) == false)
        GridcoinServices();

    return true;
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
        LogPrintf("*** %s", strMessage);
        uiInterface.ThreadSafeMessageBox(strMessage, "Gridcoin", CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
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
        // GLOBAL TESTNET SETTINGS - R HALFORD
        pchMessageStart[0] = 0xcd;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xc0;
        pchMessageStart[3] = 0xef;
        bnProofOfWorkLimit = bnProofOfWorkLimitTestNet; // 16 bits PoW target limit for testnet
        nStakeMinAge = 1 * 60 * 60; // test net min age is 1 hour
        nCoinbaseMaturity = 10; // test maturity is 10 blocks
        nGrandfather = 196550;
        nNewIndex = 10;
        nNewIndex2 = 36500;
        bOPReturnEnabled = false;
        //1-24-2016
        MAX_OUTBOUND_CONNECTIONS = (int)GetArg("-maxoutboundconnections", 8);
    }


    std::string mode = fTestNet ? "TestNet" : "Prod";
    LogPrintf("Mode=%s",mode);


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
        block.nNonce = !fTestNet ? 130208 : 22436;
        LogPrintf("starting Genesis Check...");
        // If genesis block hash does not match, then generate new genesis hash.
        if (block.GetHash() != (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
        {
            LogPrintf("Searching for genesis block...");
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
                    LogPrintf("nonce %08X: hash = %s (target = %s)", block.nNonce, thash.ToString(), hashTarget.ToString());
                }
                ++block.nNonce;
                if (block.nNonce == 0)
                {
                    LogPrintf("NONCE WRAPPED, incrementing time");
                    ++block.nTime;
                }
            }
            LogPrintf("block.nTime = %u ", block.nTime);
            LogPrintf("block.nNonce = %u ", block.nNonce);
            LogPrintf("block.GetHash = %s", block.GetHash().ToString());
        }


        block.print();

        //// debug print

        //GENESIS3: Official Merkle Root
        uint256 merkle_root = uint256("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f");
        assert(block.hashMerkleRoot == merkle_root);
        assert(block.GetHash() == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
        assert(block.CheckBlock("LoadBlockIndex",1,10*COIN));

        // Start new block file
        unsigned int nFile;
        unsigned int nBlockPos;
        if (!block.WriteToDisk(nFile, nBlockPos))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        if (!block.AddToBlockIndex(nFile, nBlockPos, hashGenesisBlock))
            return error("LoadBlockIndex() : genesis block not accepted");
    }

    return true;
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
        MD5((unsigned char*)chIn, strlen(chIn), (unsigned char*)&digest2);
        char mdString2[33];
        for(int i = 0; i < 16; i++) sprintf(&mdString2[i*2], "%02x", (unsigned int)digest2[i]);
        std::string xmd5(mdString2);
        return xmd5;
    }
    catch (std::exception &e)
    {
        LogPrintf("MD5 INVALID!");
        return "";
    }
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

bool WriteKey(std::string sKey, std::string sValue)
{
    // Allows Gridcoin to store the key value in the config file.
    boost::filesystem::path pathConfigFile(GetArg("-conf", "gridcoinresearch.conf"));
    if (!pathConfigFile.is_complete()) pathConfigFile = GetDataDir(false) / pathConfigFile;
    if (!filesystem::exists(pathConfigFile))  return false;
    boost::to_lower(sKey);
    std::string sLine = "";
    ifstream streamConfigFile;
    streamConfigFile.open(pathConfigFile.string().c_str());
    std::string sConfig = "";
    bool fWritten = false;
    if(streamConfigFile)
    {
       while(getline(streamConfigFile, sLine))
       {
            std::vector<std::string> vEntry = split(sLine,"=");
            if (vEntry.size() == 2)
            {
                std::string sSourceKey = vEntry[0];
                std::string sSourceValue = vEntry[1];
                boost::to_lower(sSourceKey);

                if (sSourceKey==sKey)
                {
                    sSourceValue = sValue;
                    sLine = sSourceKey + "=" + sSourceValue;
                    fWritten=true;
                }
            }
            sLine = strReplace(sLine, "\r", "");
            sLine = strReplace(sLine, "\n", "");
            sLine += "\n";
            sConfig += sLine;
       }
    }
    if (!fWritten)
    {
        sLine = sKey + "=" + sValue + "\n";
        sConfig += sLine;
    }

    streamConfigFile.close();

    FILE *outFile = fopen(pathConfigFile.string().c_str(),"w");
    fputs(sConfig.c_str(), outFile);
    fclose(outFile);

    ReadConfigFile(mapArgs, mapMultiArgs);
    return true;
}




std::string getfilecontents(std::string filename)
{
    std::string buffer;
    std::string line;
    ifstream myfile;
    if (fDebug10) LogPrintf("loading file to string %s",filename);

    filesystem::path path = filename;

    if (!filesystem::exists(path)) {
        LogPrintf("the file does not exist %s",path.string());
        return "-1";
    }

     FILE *file = fopen(filename.c_str(), "rb");
     CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
     int fileSize = GetFilesize(filein);
     filein.fclose();

     myfile.open(filename.c_str());

    buffer.reserve(fileSize);
    if (fDebug10) LogPrintf("opening file %s",filename);

    if(myfile)
    {
      while(getline(myfile, line))
      {
            buffer = buffer + line + "\n";
      }
    }
    myfile.close();
    return buffer;
}


bool IsCPIDValidv3(std::string cpidv2, bool allow_investor)
{
    // Used for checking the local cpid
    bool result=false;
    if (allow_investor) if (cpidv2 == "INVESTOR" || cpidv2=="investor") return true;
    if (cpidv2.length() < 34) return false;
    result = CPID_IsCPIDValid(cpidv2.substr(0,32),cpidv2,0);
    return result;
}

std::set<std::string> GetAlternativeBeaconKeys(const std::string& cpid)
{
    int64_t iMaxSeconds = 60 * 24 * 30 * 6 * 60;
    std::set<std::string> result;

    for(const auto& item : ReadCacheSection("beaconalt"))
    {
        const std::string& key = item.first;
        const std::string& value = item.second.value;
        if(!std::equal(cpid.begin(), cpid.end(), key.begin()))
            continue;

        const int64_t iAge = pindexBest != NULL
            ? pindexBest->nTime - item.second.timestamp
            : 0;
        if (iAge > iMaxSeconds)
            continue;

        result.emplace(value);
    }
    return result;
}

bool IsCPIDValidv2(MiningCPID& mc, int height)
{
    //09-25-2016: Transition to CPID Keypairs.
    if (height < nGrandfather) return true;
    bool result = false;
    int cpidV2CutOverHeight = fTestNet ? 0 : 97000;
    int cpidV3CutOverHeight = fTestNet ? 196300 : 725000;
    if (height < cpidV2CutOverHeight)
    {
        result = IsCPIDValid_Retired(mc.cpid,mc.enccpid);
    }
    else if (height >= cpidV2CutOverHeight && height <= cpidV3CutOverHeight)
    {
        if (!IsResearcher(mc.cpid)) return true;
        result = CPID_IsCPIDValid(mc.cpid, mc.cpidv2, (uint256)mc.lastblockhash);
    }
    else if (height >= cpidV3CutOverHeight)
    {
        if (mc.cpid.empty()) return error("IsCPIDValidv2(): cpid empty");
        if (!IsResearcher(mc.cpid)) return true; /* is investor? */

        const std::string sBPK_n = GetBeaconPublicKey(mc.cpid, false);
        bool kmval = sBPK_n == mc.BoincPublicKey;
        const bool scval_n = CheckMessageSignature("R","cpid", mc.cpid + mc.lastblockhash, mc.BoincSignature, sBPK_n);

        result= scval_n;
        if(!scval_n)
        {
            for(const std::string& key_alt : GetAlternativeBeaconKeys(mc.cpid))
            {
                const bool scval_alt = CheckMessageSignature("R","cpid", mc.cpid + mc.lastblockhash, mc.BoincSignature, key_alt);
                kmval = key_alt == mc.BoincPublicKey;
                if(scval_alt)
                {
                    LogPrintf("WARNING: IsCPIDValidv2: good signature with alternative key");
                    result= true;
                }
            }
        }

        if( !kmval )
            LogPrintf("WARNING: IsCPIDValidv2: block key mismatch");

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
                LogPrintf("CPID length empty.");
                return false;
            }
            if (!IsResearcher(cpid)) return true;
            if (ENCboincpubkey == "" || ENCboincpubkey.length() < 5)
            {
                    if (fDebug10) LogPrintf("ENCBpk length empty.");
                    return false;
            }
            std::string bpk = AdvancedDecrypt(ENCboincpubkey);
            std::string bpmd5 = RetrieveMd5(bpk);
            if (bpmd5==cpid) return true;
            if (fDebug10) LogPrintf("Md5<>cpid, md5 %s cpid %s  root bpk %s",bpmd5, cpid, bpk);

            return false;
    }
    catch (std::exception &e)
    {
                LogPrintf("Error while resolving CPID");
                return false;
    }
    catch(...)
    {
                LogPrintf("Error while Resolving CPID[2].");
                return false;
    }
    return false;

}


double GetTotalOwedAmount(std::string cpid)
{
    StructCPID o = GetInitializedStructCPID2(cpid,mvMagnitudes);
    return o.totalowed;
}

double GetOwedAmount(std::string cpid)
{
    if (mvMagnitudes.size() > 1)
    {
        StructCPID m = GetInitializedStructCPID2(cpid,mvMagnitudes);
        if (m.initialized) return m.owed;
        return 0;
    }
    return 0;
}


double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime,
    double& total_owed, double block_magnitude)
{
    // Gridcoin Payment Magnitude Unit in RSA Owed calculation ensures rewards are capped at MaxBlockSubsidy*BLOCKS_PER_DAY
    // Payment date range is stored in HighLockTime-LowLockTime
    // If newbie has not participated for 14 days, use earliest payment in chain to assess payment window
    // (Important to prevent e-mail change attacks) - Calculate payment timespan window in days
    try
    {
        double payment_timespan = (GetAdjustedTime() - mag.EarliestPaymentTime)/38400;
        if (payment_timespan < 2) payment_timespan =  2;
        if (payment_timespan > 10) payment_timespan = 14;
        mag.PaymentTimespan = Round(payment_timespan,0);
        double research_magnitude = 0;
        // Get neural network magnitude:
        StructCPID stDPOR = GetInitializedStructCPID2(cpid,mvDPOR);
        research_magnitude = LederstrumpfMagnitude2(stDPOR.Magnitude,locktime);
        double owed_standard = payment_timespan * std::min(research_magnitude*GetMagnitudeMultiplier(locktime),
            GetMaximumBoincSubsidy(locktime)*5.0);
        double owed_network_cap = payment_timespan * GRCMagnitudeUnit(locktime) * research_magnitude;
        double owed = std::min(owed_standard, owed_network_cap);
        double paid = mag.payments;
        double outstanding = std::min(owed-paid, GetMaximumBoincSubsidy(locktime) * 5.0);
        total_owed = owed;
        //if (outstanding < 0) outstanding=0;
        return outstanding;
    }
    catch (std::exception &e)
    {
            LogPrintf("Error while Getting outstanding amount owed.");
            return 0;
    }
    catch(...)
    {
            LogPrintf("Error while Getting outstanding amount owed.");
            return 0;
    }
}

bool BlockNeedsChecked(int64_t BlockTime)
{
    if (IsLockTimeWithin14days(BlockTime, GetAdjustedTime()))
    {
        if (fColdBoot) return false;
        bool fOut = OutOfSyncByAge();
        return !fOut;
    }
    else
    {
        return false;
    }
}

void AddResearchMagnitude(CBlockIndex* pIndex)
{
    // TODO: There are 3 different loops which do the same thing:
    //  - this function
    //  - LoadBlockIndex in txdb-leveldb.cpp
    //  - CBlock::ConnectBlock in main.cpp.
    //
    // This function should be the only one and the other two uses should
    // call it to update the data. At the same time, remove mvMagnitudesCopy
    // (and the other struct copies) as they are no longer used in a multi
    // threaded environment when the the tally thread is gone.


    if (pIndex->IsUserCPID() == false || pIndex->nResearchSubsidy <= 0)
        return;

    try
    {
        StructCPID stMag = GetInitializedStructCPID2(pIndex->GetCPID(),mvMagnitudesCopy);
        stMag.InterestSubsidy += pIndex->nInterestSubsidy;
        stMag.ResearchSubsidy += pIndex->nResearchSubsidy;
        if (pIndex->nHeight > stMag.LastBlock)
        {
            stMag.LastBlock = pIndex->nHeight;
            stMag.BlockHash = pIndex->GetBlockHash().GetHex();
        }

        if(pIndex->nMagnitude > 0)
        {
            stMag.Accuracy++;
            stMag.TotalMagnitude += pIndex->nMagnitude;
            stMag.ResearchAverageMagnitude = stMag.TotalMagnitude/(stMag.Accuracy+.01);
        }

        if (pIndex->nTime > stMag.LastPaymentTime)
            stMag.LastPaymentTime = pIndex->nTime;
        if (pIndex->nTime < stMag.EarliestPaymentTime)
            stMag.EarliestPaymentTime = pIndex->nTime;
        if (pIndex->nTime < stMag.LowLockTime)
            stMag.LowLockTime = pIndex->nTime;
        if (pIndex->nTime > stMag.HighLockTime)
            stMag.HighLockTime = pIndex->nTime;

        stMag.entries++;
        stMag.payments += pIndex->nResearchSubsidy;
        stMag.interestPayments += pIndex->nInterestSubsidy;
        stMag.AverageRAC = stMag.rac / (stMag.entries+.01);
        double total_owed = 0;
        stMag.owed = GetOutstandingAmountOwed(stMag,
                                              pIndex->GetCPID(), pIndex->nTime, total_owed, pIndex->nMagnitude);

        stMag.totalowed = total_owed;
        mvMagnitudesCopy[pIndex->GetCPID()] = stMag;
    }
    catch (const std::bad_alloc& ba)
    {
        LogPrintf("Bad Allocation in AddResearchMagnitude()");
    }
}


bool GetEarliestStakeTime(std::string grcaddress, std::string cpid)
{
    if (nBestHeight < 15)
    {
        // Write entries in the cache to get a timestamp.
        WriteCache("global", "nGRCTime", "", GetAdjustedTime());
        WriteCache("global", "nCPIDTime", "", GetAdjustedTime());
        return true;
    }

    int64_t nGRCTime = ReadCache("global", "nGRCTime").timestamp;
    int64_t nCPIDTime = ReadCache("global", "nCPIDTime").timestamp;
    if (IsLockTimeWithinMinutes(nLastGRCtallied, GetAdjustedTime(), 100) &&
        (nGRCTime > 0 || nCPIDTime > 0))
        return true;

    nLastGRCtallied = GetAdjustedTime();
    CBlock block;
    int64_t nStart = GetTimeMillis();
    LOCK(cs_main);
    {
            int nMaxDepth = nBestHeight;
            int nLookback = BLOCKS_PER_DAY*6*30;  //6 months back for performance
            int nMinDepth = nMaxDepth - nLookback;
            if (nMinDepth < 2) nMinDepth = 2;
            // Start at the earliest block index:
            CBlockIndex* pblockindex = blockFinder.FindByHeight(nMinDepth);
            while (pblockindex->nHeight < nMaxDepth-1)
            {
                        pblockindex = pblockindex->pnext;
                        if (pblockindex == pindexBest) break;
                        if (pblockindex == NULL || !pblockindex->IsInMainChain()) continue;
                        std::string myCPID = "";
                        if (pblockindex->nHeight < nNewIndex)
                        {
                            //Between block 1 and nNewIndex, unfortunately, we have to read from disk.
                            block.ReadFromDisk(pblockindex);
                            std::string hashboinc = "";
                            if (block.vtx.size() > 0) hashboinc = block.vtx[0].hashBoinc;
                            MiningCPID bb = DeserializeBoincBlock(hashboinc,block.nVersion);
                            myCPID = bb.cpid;
                        }
                        else
                        {
                            myCPID = pblockindex->GetCPID();
                        }
                        if (cpid == myCPID && nCPIDTime==0 && IsResearcher(myCPID))
                        {
                            nCPIDTime = pblockindex->nTime;
                            nGRCTime = pblockindex->nTime;
                            break;
                        }
            }
    }
    int64_t EarliestStakedWalletTx = GetEarliestWalletTransaction();
    if (EarliestStakedWalletTx > 0 && EarliestStakedWalletTx < nGRCTime) nGRCTime = EarliestStakedWalletTx;
    if (!IsResearcher(cpid) && EarliestStakedWalletTx > 0) nGRCTime = EarliestStakedWalletTx;
    if (fTestNet) nGRCTime -= (86400*30);
    if (nGRCTime <= 0)  nGRCTime = GetAdjustedTime();
    if (nCPIDTime <= 0) nCPIDTime = GetAdjustedTime();

    LogPrintf("Loaded staketime from index in %" PRId64, GetTimeMillis() - nStart);
    LogPrintf("CPIDTime %" PRId64 ", GRCTime %" PRId64 ", WalletTime %" PRId64, nCPIDTime, nGRCTime, EarliestStakedWalletTx);

    // Update caches with new timestamps.
    WriteCache("global", "nGRCTime", "", nGRCTime);
    WriteCache("global", "nCPIDTime", "", nCPIDTime);
    return true;
}

HashSet GetCPIDBlockHashes(const std::string& cpid)
{
    auto hashes = mvCPIDBlockHashes.find(cpid);
    return hashes != mvCPIDBlockHashes.end()
        ? hashes->second
        : HashSet();
}

void AddCPIDBlockHash(const std::string& cpid, const uint256& blockhash)
{
    // Add block hash to CPID hash set.
    mvCPIDBlockHashes[cpid].emplace(blockhash);
}

void RemoveCPIDBlockHash(const std::string& cpid, const uint256& blockhash)
{
   mvCPIDBlockHashes[cpid].erase(blockhash);
}

StructCPID GetLifetimeCPID(const std::string& cpid, const std::string& sCalledFrom)
{
    //Eliminates issues with reorgs, disconnects, double counting, etc..
    if (!IsResearcher(cpid))
        return GetInitializedStructCPID2("INVESTOR",mvResearchAge);

    if (fDebug10) LogPrintf("GetLifetimeCPID.BEGIN: %s %s", sCalledFrom, cpid);

    const HashSet& hashes = GetCPIDBlockHashes(cpid);
    ZeroOutResearcherTotals(cpid);


    StructCPID stCPID = GetInitializedStructCPID2(cpid, mvResearchAge);
    for (HashSet::iterator it = hashes.begin(); it != hashes.end(); ++it)
    {
        const uint256& uHash = *it;
        if (fDebug10) LogPrintf("GetLifetimeCPID: trying %s",uHash.GetHex());

        // Ensure that we have this block.
        auto mapItem = mapBlockIndex.find(uHash);
        if (mapItem == mapBlockIndex.end())
           continue;

        // Ensure that the block is valid
        CBlockIndex* pblockindex = mapItem->second;
        if(pblockindex == NULL ||
           pblockindex->IsInMainChain() == false ||
           pblockindex->GetCPID() != cpid)
            continue;

        // Block located and verified.
        if (fDebug10)
            LogPrintf("GetLifetimeCPID: verified %s height= %d LastBlock= %d nResearchSubsidy= %.3f",
            uHash.GetHex().c_str(),pblockindex->nHeight,(int)stCPID.LastBlock,pblockindex->nResearchSubsidy);
        if(!pblockindex->pnext && pblockindex!=pindexBest)
            LogPrintf("WARNING GetLifetimeCPID: index {%s %d} for cpid %s, "
                "is not in the main chain",pblockindex->GetBlockHash().GetHex(),
                pblockindex->nHeight,cpid);

        if(pblockindex->nResearchSubsidy> 0)
        {
            stCPID.InterestSubsidy += pblockindex->nInterestSubsidy;
            stCPID.ResearchSubsidy += pblockindex->nResearchSubsidy;
            if(pblockindex->nHeight > stCPID.LastBlock)
            {
                stCPID.LastBlock = pblockindex->nHeight;
                stCPID.BlockHash = pblockindex->GetBlockHash().GetHex();
            }

            if (pblockindex->nMagnitude > 0)
            {
                stCPID.Accuracy++;
                stCPID.TotalMagnitude += pblockindex->nMagnitude;
                stCPID.ResearchAverageMagnitude = stCPID.TotalMagnitude/(stCPID.Accuracy+.01);
            }

            if (pblockindex->nTime < stCPID.LowLockTime)  stCPID.LowLockTime  = pblockindex->nTime;
            if (pblockindex->nTime > stCPID.HighLockTime) stCPID.HighLockTime = pblockindex->nTime;
        }
    }

    // Save updated CPID data holder.
    if (fDebug10) LogPrintf("GetLifetimeCPID.END: %s set {%s %d}",cpid, stCPID.BlockHash, (int)stCPID.LastBlock);
    mvResearchAge[cpid] = stCPID;
    return stCPID;
}

MiningCPID GetInitializedMiningCPID(std::string name,std::map<std::string, MiningCPID>& vRef)
{
   MiningCPID& cpid = vRef[name];
    if (!cpid.initialized)
    {
                cpid = GetMiningCPID();
                cpid.initialized=true;
                cpid.LastPaymentTime = 0;
    }

   return cpid;
}


StructCPID GetInitializedStructCPID2(const std::string& name, std::map<std::string, StructCPID>& vRef)
{
    try
    {
        StructCPID& cpid = vRef[name];
        if (!cpid.initialized)
        {
            cpid = GetStructCPID();
            cpid.cpid = name;
            cpid.initialized=true;
            cpid.LowLockTime = std::numeric_limits<unsigned int>::max();
            cpid.HighLockTime = 0;
            cpid.LastPaymentTime = 0;
            cpid.EarliestPaymentTime = 99999999999;
            cpid.Accuracy = 0;
        }

        return cpid;
    }
    catch (const std::bad_alloc& ba)
    {
        LogPrintf("Bad alloc caught in GetInitializedStructCpid2 for %s", name);
    }
    catch(...)
    {
        LogPrintf("Exception caught in GetInitializedStructCpid2 for %s", name);
    }

    // Error during map's heap allocation. Return an empty object.
    return GetStructCPID();
}


bool ComputeNeuralNetworkSupermajorityHashes()
{
    if (nBestHeight < 15)  return true;
    //Clear the neural network hash buffer
    if (mvNeuralNetworkHash.size() > 0)  mvNeuralNetworkHash.clear();
    if (mvNeuralVersion.size() > 0)  mvNeuralVersion.clear();
    if (mvCurrentNeuralNetworkHash.size() > 0) mvCurrentNeuralNetworkHash.clear();

    //Clear the votes
    /* ClearCache was no-op in previous version due to bug. Now it was fixed,
        but we have to emulate the old behaviour to prevent early forks. */
    if(pindexBest && pindexBest->nVersion>=9)
    {
        ClearCache("neuralsecurity");
    }
    WriteCache("neuralsecurity","pending","0",GetAdjustedTime());
    try
    {
        int nMaxDepth = nBestHeight;
        int nLookback = 100;
        int nMinDepth = (nMaxDepth - nLookback);
        if (nMinDepth < 2)   nMinDepth = 2;
        CBlock block;
        CBlockIndex* pblockindex = pindexBest;
        while (pblockindex->nHeight > nMinDepth)
        {
            if (!pblockindex || !pblockindex->pprev) return false;
            pblockindex = pblockindex->pprev;
            if (pblockindex == pindexGenesisBlock) return false;
            if (!pblockindex->IsInMainChain()) continue;
            block.ReadFromDisk(pblockindex);
            std::string hashboinc = "";
            if (block.vtx.size() > 0) hashboinc = block.vtx[0].hashBoinc;
            if (!hashboinc.empty())
            {
                MiningCPID bb = DeserializeBoincBlock(hashboinc,block.nVersion);
                //If block is pending: 7-25-2015
                if (bb.superblock.length() > 20)
                {
                    std::string superblock = UnpackBinarySuperblock(bb.superblock);
                    if (VerifySuperblock(superblock, pblockindex))
                    {
                        WriteCache("neuralsecurity","pending",ToString(pblockindex->nHeight),GetAdjustedTime());
                    }
                }

                IncrementVersionCount(bb.clientversion);
                //Increment Neural Network Hashes Supermajority (over the last N blocks)
                IncrementNeuralNetworkSupermajority(bb.NeuralHash,bb.GRCAddress,(nMaxDepth-pblockindex->nHeight)+10,pblockindex);
                IncrementCurrentNeuralNetworkSupermajority(bb.CurrentNeuralHash,bb.GRCAddress,(nMaxDepth-pblockindex->nHeight)+10);

            }
        }

        if (fDebug3) LogPrintf(".11.");
    }
    catch (std::exception &e)
    {
            LogPrintf("Neural Error while memorizing hashes.");
    }
    catch(...)
    {
        LogPrintf("Neural error While Memorizing Hashes! [1]");
    }
    return true;

}

bool TallyResearchAverages(CBlockIndex* index)
{
    if(IsV9Enabled_Tally(index->nHeight))
        return TallyResearchAverages_v9(index);
    else if(IsResearchAgeEnabled(index->nHeight) && !IsV9Enabled_Tally(index->nHeight))
        return TallyResearchAverages_retired(index);
    else
        return false;
}

bool TallyResearchAverages_retired(CBlockIndex* index)
{
    LogPrintf("Tally (retired)");

    if (!index)
    {
        bNetAveragesLoaded = true;
        return true;
    }

    if(IsV9Enabled_Tally(index->nHeight))
        return error("TallyResearchAverages_retired: called while V9 tally enabled");

    //Iterate throught last 14 days, tally network averages
    if (index->nHeight < 15)
    {
        bNetAveragesLoaded = true;
        return true;
    }

    //8-27-2016
    int64_t nStart = GetTimeMillis();

    bNetAveragesLoaded = false;
    bool superblockloaded = false;
    double NetworkPayments = 0;
    double NetworkInterest = 0;

    //Consensus Start/End block:
    int nMaxDepth = (index->nHeight - CONSENSUS_LOOKBACK) - ( (index->nHeight - CONSENSUS_LOOKBACK) % BLOCK_GRANULARITY);
    int nLookback = BLOCKS_PER_DAY * 14; //Daily block count * Lookback in days
    int nMinDepth = (nMaxDepth - nLookback) - ( (nMaxDepth-nLookback) % TALLY_GRANULARITY);
    if (fDebug3) LogPrintf("START BLOCK %d, END BLOCK %d", nMaxDepth, nMinDepth);
    if (nMinDepth < 2)              nMinDepth = 2;
    if(fDebug) LogPrintf("TallyResearchAverages_retired: beginning start %d end %d",nMaxDepth,nMinDepth);
    mvMagnitudesCopy.clear();
    int iRow = 0;

    CBlockIndex* pblockindex = index;
    while (pblockindex->nHeight > nMaxDepth)
    {
        if (!pblockindex || !pblockindex->pprev || pblockindex == pindexGenesisBlock) return false;
        pblockindex = pblockindex->pprev;
    }

    if (fDebug3) LogPrintf("Max block %d, seektime %" PRId64, pblockindex->nHeight, GetTimeMillis()-nStart);
    nStart=GetTimeMillis();


    // Headless critical section ()
    try
    {
        while (pblockindex->nHeight > nMinDepth)
        {
            if (!pblockindex || !pblockindex->pprev) return false;
            pblockindex = pblockindex->pprev;
            if (pblockindex == pindexGenesisBlock) return false;
            if (!pblockindex->IsInMainChain()) continue;
            NetworkPayments += pblockindex->nResearchSubsidy;
            NetworkInterest += pblockindex->nInterestSubsidy;
            AddResearchMagnitude(pblockindex);

            iRow++;
            if (IsSuperBlock(pblockindex) && !superblockloaded)
            {
                MiningCPID bb = GetBoincBlockByIndex(pblockindex);
                if (bb.superblock.length() > 20)
                {
                    std::string superblock = UnpackBinarySuperblock(bb.superblock);
                    if (VerifySuperblock(superblock, pblockindex))
                    {
                        LoadSuperblock(superblock,pblockindex->nTime,pblockindex->nHeight);
                        superblockloaded=true;
                        if (fDebug)
                            LogPrintf("TallyResearchAverages_retired: Superblock Loaded {%s %i}", pblockindex->GetBlockHash().GetHex(), pblockindex->nHeight);
                    }
                }
            }
        }
        // End of critical section

        if (fDebug3) LogPrintf("TNA loaded in %" PRId64, GetTimeMillis()-nStart);
        nStart=GetTimeMillis();

        if (pblockindex)
        {
            if (fDebug3)
                LogPrintf("Min block %i, Rows %i", pblockindex->nHeight, iRow);

            StructCPID network = GetInitializedStructCPID2("NETWORK",mvNetworkCopy);
            network.projectname="NETWORK";
            network.payments = NetworkPayments;
            network.InterestSubsidy = NetworkInterest;
            mvNetworkCopy["NETWORK"] = network;
            if(fDebug3) LogPrintf(" TMIS1 ");
            TallyMagnitudesInSuperblock();
        }
        // 11-19-2015 Copy dictionaries to live RAM
        mvDPOR = mvDPORCopy;
        mvMagnitudes = mvMagnitudesCopy;
        mvNetwork = mvNetworkCopy;
        bNetAveragesLoaded = true;
        return true;
    }
    catch (bad_alloc ba)
    {
        LogPrintf("Bad Alloc while tallying network averages. [1]");
        bNetAveragesLoaded=true;
    }

    if (fDebug3) LogPrintf("NA loaded in %" PRId64, GetTimeMillis() - nStart);

    bNetAveragesLoaded=true;
    return false;
}

bool TallyResearchAverages_v9(CBlockIndex* index)
{
    if(!IsV9Enabled_Tally(index->nHeight))
        return error("TallyResearchAverages_v9: called while V9 tally disabled");

    LogPrintf("Tally (v9)");

    //Iterate throught last 14 days, tally network averages
    if (index->nHeight < 15)
    {
        bNetAveragesLoaded = true;
        return true;
    }

    //8-27-2016
    int64_t nStart = GetTimeMillis();

    if (fDebug) LogPrintf("Tallying Research Averages (begin) ");
    bNetAveragesLoaded = false;
    double NetworkPayments = 0;
    double NetworkInterest = 0;

    //Consensus Start/End block:
    int nMaxConensusDepth = index->nHeight - CONSENSUS_LOOKBACK;
    int nMaxDepth = nMaxConensusDepth - (nMaxConensusDepth % TALLY_GRANULARITY);
    int nLookback = BLOCKS_PER_DAY * 14; //Daily block count * Lookback in days
    int nMinDepth = nMaxDepth - nLookback;
    if (nMinDepth < 2)
        nMinDepth = 2;

    if(fDebug) LogPrintf("TallyResearchAverages: start %d end %d",nMaxDepth,nMinDepth);

    mvMagnitudesCopy.clear();
    CBlockIndex* pblockindex = index;
    if (!pblockindex)
    {
        bNetAveragesLoaded = true;
        return true;
    }

    // Seek to head of tally window.
    while (pblockindex->nHeight > nMaxDepth)
    {
        if (!pblockindex || !pblockindex->pprev || pblockindex == pindexGenesisBlock) return false;
        pblockindex = pblockindex->pprev;
    }

    if (fDebug3) LogPrintf("Max block %i, seektime %" PRId64, pblockindex->nHeight, GetTimeMillis()-nStart);
    nStart=GetTimeMillis();

    // Load newest superblock in tally window
    for(CBlockIndex* sbIndex = pblockindex;
        sbIndex != NULL;
        sbIndex = sbIndex->pprev)
    {
        if(!IsSuperBlock(sbIndex))
            continue;

        MiningCPID bb = GetBoincBlockByIndex(sbIndex);
        if(bb.superblock.length() <= 20)
            continue;

        const std::string& superblock = UnpackBinarySuperblock(bb.superblock);
        if(!VerifySuperblock(superblock, sbIndex))
            continue;

        LoadSuperblock(superblock, sbIndex->nTime, sbIndex->nHeight);
        if (fDebug)
            LogPrintf("TallyResearchAverages_v9: Superblock Loaded {%s %i}", sbIndex->GetBlockHash().GetHex(), sbIndex->nHeight);
        break;
    }

    // Headless critical section ()
    try
    {
        while (pblockindex->nHeight > nMinDepth)
        {
            if (!pblockindex || !pblockindex->pprev) return false;
            pblockindex = pblockindex->pprev;
            if (pblockindex == pindexGenesisBlock) return false;
            if (!pblockindex->IsInMainChain()) continue;
            NetworkPayments += pblockindex->nResearchSubsidy;
            NetworkInterest += pblockindex->nInterestSubsidy;
            AddResearchMagnitude(pblockindex);
        }
        // End of critical section
        if (fDebug3) LogPrintf("TNA loaded in %" PRId64, GetTimeMillis()-nStart);
        nStart=GetTimeMillis();


        if (pblockindex)
        {
            StructCPID network = GetInitializedStructCPID2("NETWORK",mvNetworkCopy);
            network.projectname="NETWORK";
            network.payments = NetworkPayments;
            network.InterestSubsidy = NetworkInterest;
            mvNetworkCopy["NETWORK"] = network;
            if(fDebug3) LogPrintf(" TMIS1 ");
            TallyMagnitudesInSuperblock();
        }
        // 11-19-2015 Copy dictionaries to live RAM
        mvDPOR = mvDPORCopy;
        mvMagnitudes = mvMagnitudesCopy;
        mvNetwork = mvNetworkCopy;
        bNetAveragesLoaded = true;
        return true;
    }
    catch (const std::bad_alloc& ba)
    {
        LogPrintf("Bad Alloc while tallying network averages. [1]");
        bNetAveragesLoaded=true;
    }

    if (fDebug3) LogPrintf("NA loaded in %" PRId64, GetTimeMillis() - nStart);

    bNetAveragesLoaded=true;
    return false;
}

void PrintBlockTree()
{
    AssertLockHeld(cs_main);
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (BlockMap::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = (*mi).second;
        mapNext[pindex->pprev].push_back(pindex);
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
                LogPrintf("| ");
            LogPrintf("|\\");
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++)
                LogPrintf("| ");
            LogPrintf("|");
       }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            LogPrintf("| ");

        // print item
        CBlock block;
        block.ReadFromDisk(pindex);
        LogPrintf("%d (%u,%u) %s  %08x  %s  mint %7s  tx %" PRIszu "",
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
            LogPrintf("%s() : Deserialize or I/O error caught during load",
                   __PRETTY_FUNCTION__);
        }
    }
    LogPrintf("Loaded %i blocks from external file in %" PRId64 "ms", nLoaded, GetTimeMillis() - nStart);
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

    // Alerts
    {
        LOCK(cs_mapAlerts);
        for (auto const& item : mapAlerts)
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




// The message start string is designed to be unlikely to occur in normal data.
// The characters are rarely used upper ASCII, not valid as UTF-8, and produce
// a large 4-byte int at any alignment.
unsigned char pchMessageStart[4] = { 0x70, 0x35, 0x22, 0x05 };


std::string NodeAddress(CNode* pfrom)
{
    std::string ip = pfrom->addr.ToString();
    return ip;
}

double ExtractMagnitudeFromExplainMagnitude()
{
        if (msNeuralResponse.empty()) return 0;
        try
        {
            std::vector<std::string> vMag = split(msNeuralResponse.c_str(),"<ROW>");
            for (unsigned int i = 0; i < vMag.size(); i++)
            {
                if (Contains(vMag[i],"Total Mag:"))
                {
                    std::vector<std::string> vMyMag = split(vMag[i].c_str(),":");
                    if (vMyMag.size() > 0)
                    {
                        std::string sSubMag = vMyMag[1];
                        sSubMag = strReplace(sSubMag," ","");
                        double dMag = RoundFromString("0"+sSubMag,0);
                        return dMag;
                    }
                }
            }
            return 0;
        }
        catch(...)
        {
            return 0;
        }
        return 0;
}

bool VerifyExplainMagnitudeResponse()
{
    if (msNeuralResponse.empty())
        return false;

            double dMag = ExtractMagnitudeFromExplainMagnitude();
            if (dMag==0)
        msNeuralResponse.clear();

    return dMag != 0;
            }

bool SecurityTest(CNode* pfrom, bool acid_test)
{
    if (pfrom->nStartingHeight > (nBestHeight*.5) && acid_test) return true;
    return false;
}

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv, int64_t nTimeReceived)
{
    RandAddSeedPerfmon();
    if (fDebug10)
        LogPrintf("received: %s from %s (%" PRIszu " bytes)", strCommand, pfrom->addrName, vRecv.size());
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        LogPrintf("dropmessagestest DROPPING RECV MESSAGE");
        return true;
    }

    // Stay in Sync - 8-9-2016
    if (!IsLockTimeWithinMinutes(nBootup, GetAdjustedTime(), 15))
    {
        if ((!IsLockTimeWithinMinutes(nLastAskedForBlocks, GetAdjustedTime(), 5) && WalletOutOfSync()) || (WalletOutOfSync() && fTestNet))
        {
            if(fDebug) LogPrintf("Bootup");
            AskForOutstandingBlocks(uint256(0));
        }
    }

    // Message Attacks ////////////////////////////////////////////////////////
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

        if (fDebug10)
            LogPrintf("received aries version %i boinchashnonce %s boinchashpw %s cpid %s enccpid %s acid %s ..."
                      ,pfrom->nVersion, pfrom->boinchashnonce, pfrom->boinchashpw
                      ,pfrom->cpid.c_str(), pfrom->enccpid, acid);

        int64_t timedrift = std::abs(GetAdjustedTime() - nTime);

            if (timedrift > (8*60))
            {
            if (fDebug10) LogPrintf("Disconnecting unauthorized peer with Network Time so far off by %" PRId64 " seconds!", timedrift);
            pfrom->Misbehaving(100);
            pfrom->fDisconnect = true;
            return false;
        }


        // Ensure testnet users are running latest version as of 12-3-2015 (works in conjunction with block spamming)
        if (pfrom->nVersion < 180321 && fTestNet)
        {
            // disconnect from peers older than this proto version
            if (fDebug10) LogPrintf("Testnet partner %s using obsolete version %i; disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION)
        {
            // disconnect from peers older than this proto version
            if (fDebug10) LogPrintf("partner %s using obsolete version %i; disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion < 180323 && !fTestNet && pindexBest->nHeight > 860500)
        {
            // disconnect from peers older than this proto version - Enforce Beacon Age - 3-26-2017
            if (fDebug10) LogPrintf("partner %s using obsolete version %i (before enforcing beacon age); disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (!fTestNet && pfrom->nVersion < 180314 && IsResearchAgeEnabled(pindexBest->nHeight))
        {
            // disconnect from peers older than this proto version
            if (fDebug10) LogPrintf("ResearchAge: partner %s using obsolete version %i; disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
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
        // 12-5-2015 - Append Trust fields
        pfrom->nTrust = 0;

        if (!vRecv.empty())         vRecv >> pfrom->sGRCAddress;


        // Allow newbies to connect easily with 0 blocks
        if (GetArgument("autoban","true") == "true")
        {

                // Note: Hacking attempts start in this area

                if (pfrom->nStartingHeight < 1 && pfrom->nServices == 0 )
                {
                    pfrom->Misbehaving(100);
                    if (fDebug3) LogPrintf("Disconnecting possible hacker node with no services.  Banned for 24 hours.");
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
            if (fDebug3) LogPrintf("connected to self at %s, disconnecting", pfrom->addr.ToString());
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

        // Moved the below from AddTimeData to here to follow bitcoin's approach.
        int64_t nOffsetSample = nTime - GetTime();
        if (GetBoolArg("-synctime", true))
            AddTimeData(pfrom->addr, nOffsetSample);

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
        }
        else
        {
            if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom)
            {
                if (SecurityTest(pfrom,true))
                {
                    //Dont store the peer unless it passes the test
                    addrman.Add(addrFrom, addrFrom);
                    addrman.Good(addrFrom);
                }
            }
        }


        // Ask the first connected node for block updates
        static int nAskedForBlocks = 0;
        if (!pfrom->fClient && !pfrom->fOneShot &&
            (pfrom->nStartingHeight > (nBestHeight - 144)) &&
            (pfrom->nVersion < NOBLKS_VERSION_START ||
             pfrom->nVersion >= NOBLKS_VERSION_END) &&
             (nAskedForBlocks < 1 || (vNodes.size() <= 1 && nAskedForBlocks < 1)))
        {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(pindexBest, uint256(0), true);
            if (fDebug3) LogPrintf("Asked For blocks.");
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            for (auto const& item : mapAlerts)
                item.second.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        if (fDebug10) LogPrintf("receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s", pfrom->nVersion,
            pfrom->nStartingHeight, addrMe.ToString(), addrFrom.ToString(), pfrom->addr.ToString());

        cPeerBlockCounts.input(pfrom->nStartingHeight);
    }
    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else 1-10-2015 Halford
        LogPrintf("Hack attempt from %s - %s (banned) ",pfrom->addrName, NodeAddress(pfrom));
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
            return error("message addr size() = %" PRIszu "", vAddr.size());
        }

        // Don't store the node address unless they have block height > 50%
        if (pfrom->nStartingHeight < (nBestHeight*.5) && LessVerbose(975)) return true;

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        for (auto &addr : vAddr)
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
                    for (auto const& pnode : vNodes)
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
            pfrom->Misbehaving(50);
            return error("message inv size() = %" PRIszu "", vInv.size());
        }

        // find last block in inv vector
        unsigned int nLastBlock = (unsigned int)(-1);
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK) {
                nLastBlock = vInv.size() - 1 - nInv;
                break;
            }
        }

        LOCK(cs_main);
        CTxDB txdb("r");
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(txdb, inv);
            if (fDebug10)
                LogPrintf("  got inventory: %s  %s", inv.ToString(), fAlreadyHave ? "have" : "new");

            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]), true);
            } else if (nInv == nLastBlock) {
                // In case we are on a very long side-chain, it is possible that we already have
                // the last block in an inv bundle sent in response to getblocks. Try to detect
                // this situation and push another getblocks to continue.
                pfrom->PushGetBlocks(mapBlockIndex[inv.hash], uint256(0), true);
                if (fDebug10)
                    LogPrintf("force getblock request: %s", inv.ToString());
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
            return error("message getdata size() = %" PRIszu "", vInv.size());
        }

        if (fDebugNet || (vInv.size() != 1))
        {
            if (fDebug10)  LogPrintf("received getdata (%" PRIszu " invsz)", vInv.size());
        }

        LOCK(cs_main);
        for (auto const& inv : vInv)
        {
            if (fShutdown)
                return true;
            if (fDebugNet || (vInv.size() == 1))
            {
              if (fDebug10)   LogPrintf("received getdata for: %s", inv.ToString());
            }

            if (inv.type == MSG_BLOCK)
            {
                // Send block from disk
                BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
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
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, hashBestChain));
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

        LOCK(cs_main);

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = locator.GetBlockIndex();

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        int nLimit = 500;

        if (fDebug3) LogPrintf("getblocks %d to %s limit %d", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20), nLimit);
        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                if (fDebug3) LogPrintf("getblocks stopping at %d %s", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20));
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
                if (fDebug3) LogPrintf("getblocks stopping at limit %d %s", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20));
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }
    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            BlockMap::iterator mi = mapBlockIndex.find(hashStop);
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
        int nLimit = 1000;
        LogPrintf("getheaders %d to %s", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20));
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

        LOCK(cs_main);

        bool fMissingInputs = false;
        if (AcceptToMemoryPool(mempool, tx, &fMissingInputs))
        {
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
                        LogPrintf("   accepted orphan tx %s", orphanTxHash.ToString().substr(0,10));
                        RelayTransaction(orphanTx, orphanTxHash);
                        mapAlreadyAskedFor.erase(CInv(MSG_TX, orphanTxHash));
                        vWorkQueue.push_back(orphanTxHash);
                        vEraseQueue.push_back(orphanTxHash);
                        pfrom->nTrust++;
                    }
                    else if (!fMissingInputs2)
                    {
                        // invalid orphan
                        vEraseQueue.push_back(orphanTxHash);
                        LogPrintf("   removed invalid orphan tx %s", orphanTxHash.ToString().substr(0,10));
                    }
                }
            }

            for (auto const& hash : vEraseQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            AddOrphanTx(tx);

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nEvicted = LimitOrphanTxSize(MAX_ORPHAN_TRANSACTIONS);
            if (nEvicted > 0)
                LogPrintf("mapOrphan overflow, removed %u tx", nEvicted);
        }
        if (tx.nDoS) pfrom->Misbehaving(tx.nDoS);
    }


    else if (strCommand == "encrypt")
    {
        //Response from getblocks, message = block

        CBlock block;
        std::string acid = "";
        vRecv >> block >> acid;
        uint256 hashBlock = block.GetHash();

        LogPrintf(" Received block %s; ", hashBlock.ToString());
        if (fDebug10) block.print();

        CInv inv(MSG_BLOCK, hashBlock);
        pfrom->AddInventoryKnown(inv);

        LOCK(cs_main);

        if (ProcessBlock(pfrom, &block, false))
        {
            mapAlreadyAskedFor.erase(inv);
            pfrom->nTrust++;
        }
        if (block.nDoS)
        {
                pfrom->Misbehaving(block.nDoS);
                pfrom->nTrust--;
        }

    }


    else if (strCommand == "getaddr")
    {
        // Don't return addresses older than nCutOff timestamp
        int64_t nCutOff =  GetAdjustedTime() - (nNodeLifespan * 24 * 60 * 60);
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        for (auto const&addr : vAddr)
            if(addr.nTime > nCutOff)
                pfrom->PushAddress(addr);
    }


    else if (strCommand == "mempool")
    {
        LOCK(cs_main);

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
    else if (strCommand == "neural")
    {
            std::string neural_request = "";
            std::string neural_request_id = "";
            vRecv >> neural_request >> neural_request_id;  // foreign node issued neural request with request ID:
            std::string neural_response = "generic_response";

            if (neural_request=="neural_data")
            {
                pfrom->PushMessage("ndata_nresp", NN::GetNeuralContract());
            }
            else if (neural_request=="neural_hash")
            {
            pfrom->PushMessage("hash_nresp", NN::GetNeuralHash());
            }
            else if (neural_request=="explainmag")
            {
            neural_response = NN::ExecuteDotNetStringFunction("ExplainMag",neural_request_id);
                pfrom->PushMessage("expmag_nresp", neural_response);
                }
            else if (neural_request=="quorum")
            {
            // 7-12-2015 Resolve discrepencies in w nodes to speak to each other
            pfrom->PushMessage("quorum_nresp", NN::GetNeuralContract());
            }
    }
    else if (strCommand == "ping")
    {
        std::string acid = "";
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce >> acid;

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
        int64_t pingUsecEnd = GetTimeMicros();
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0)
            {
                if (nonce == pfrom->nPingNonceSent)
                {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
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
            LogPrintf("pong %s %s: %s, %" PRIx64 " expected, %" PRIx64 " received, %" PRIu64 " bytes"
                , pfrom->addr.ToString()
                , pfrom->strSubVer
                , sProblem, pfrom->nPingNonceSent, nonce, nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }
    else if (strCommand == "hash_nresp")
    {
            std::string neural_response = "";
            vRecv >> neural_response;
            // if (pfrom->nNeuralRequestSent != 0)
            // nNeuralNonce must match request ID
            pfrom->NeuralHash = neural_response;
            if (fDebug10) LogPrintf("hash_Neural Response %s ",neural_response);
    }
    else if (strCommand == "expmag_nresp")
    {
            std::string neural_response = "";
            vRecv >> neural_response;
            if (neural_response.length() > 10)
            {
                msNeuralResponse=neural_response;
                //If invalid, try again 10-20-2015
                VerifyExplainMagnitudeResponse();
            }
            if (fDebug10) LogPrintf("expmag_Neural Response %s ",neural_response);
    }
    else if (strCommand == "quorum_nresp")
    {
            std::string neural_contract = "";
            vRecv >> neural_contract;
            if (fDebug && neural_contract.length() > 100) LogPrintf("Quorum contract received %s",neural_contract.substr(0,80));
            if (neural_contract.length() > 10)
            {
                 std::string results = "";
                 //Resolve discrepancies
                results = NN::ExecuteDotNetStringFunction("ResolveDiscrepancies",neural_contract);
                 if (fDebug && !results.empty()) LogPrintf("Quorum Resolution: %s ",results);
            }
    }
    else if (strCommand == "ndata_nresp")
    {
            std::string neural_contract = "";
            vRecv >> neural_contract;
            if (fDebug3 && neural_contract.length() > 100) LogPrintf("Quorum contract received %s",neural_contract.substr(0,80));
            if (neural_contract.length() > 10)
            {
                 std::string results = "";
                 //Resolve discrepancies
                LogPrintf("Sync neural network data from supermajority");
                results = NN::ExecuteDotNetStringFunction("ResolveCurrentDiscrepancies",neural_contract);
                if (fDebug && !results.empty()) LogPrintf("Quorum Resolution: %s ",results);
                 // Resume the full DPOR sync at this point now that we have the supermajority data
                 if (results=="SUCCESS")  FullSyncWithDPORNodes();
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
                    for (auto const& pnode : vNodes)
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
        // Let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever. Currently only SPV clients actually care
        // about this message: it's needed when they are recursively walking the
        // dependencies of relevant unconfirmed transactions. SPV clients want to
        // do that because they want to know about (and store and rebroadcast and
        // risk analyze) the dependencies of transactions relevant to them, without
        // having to download the entire memory pool.


    }

    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "aries" || strCommand == "gridaddr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
            AddressCurrentlyConnected(pfrom->addr);

    return true;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
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

        //if (fDebug10)
        //    LogPrintf("ProcessMessages(message %u msgsz, %zu bytes, complete:%s)",
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, pchMessageStart, sizeof(pchMessageStart)) != 0) {
            if (fDebug10) LogPrintf("PROCESSMESSAGE: INVALID MESSAGESTART");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid())
        {
            LogPrintf("PROCESSMESSAGE: ERRORS IN HEADER %s", hdr.GetCommand());
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
            LogPrintf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x",
               strCommand, nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try
        {
            fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime);
            if (fShutdown)
                break;
        }
        catch (std::ios_base::failure& e)
        {
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length", strCommand, nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught", strCommand, nMessageSize, e.what());
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
           if (fDebug10) LogPrintf("ProcessMessage(%s, %u bytes) FAILED", strCommand, nMessageSize);
        }
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}

double LederstrumpfMagnitude2(double Magnitude, int64_t locktime)
{
    //2-1-2015 - Halford - The MagCap is 2000
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
    if (Magnitude >= MagCap*1.9)                            out_mag = MagCap*1.0;
    return out_mag;
}

std::string GetLastPORBlockHash(std::string cpid)
{
    StructCPID stCPID = GetInitializedStructCPID2(cpid,mvResearchAge);
    return stCPID.BlockHash;
}

std::string SerializeBoincBlock(MiningCPID mcpid, int BlockVersion)
{
    std::string delim = "<|>";
    std::string version = FormatFullVersion();
    int subsidy_places= BlockVersion<8 ? 2 : 8;
    if (!IsResearchAgeEnabled(pindexBest->nHeight))
    {
        mcpid.Organization = GetArg("-org", "windows");
        mcpid.OrganizationKey = "12345678"; //Only reveal 8 characters
    }
    else
    {
        mcpid.projectname = "";
        mcpid.rac = 0;
        mcpid.NetworkRAC = 0;
    }


    mcpid.LastPORBlockHash = GetLastPORBlockHash(mcpid.cpid);

    if (mcpid.lastblockhash.empty()) mcpid.lastblockhash = "0";
    if (mcpid.LastPORBlockHash.empty()) mcpid.LastPORBlockHash="0";

    if (IsResearcher(mcpid.cpid) && mcpid.lastblockhash != "0")
    {
        mcpid.BoincPublicKey = GetBeaconPublicKey(mcpid.cpid, false);
    }

    std::string bb = mcpid.cpid + delim + mcpid.projectname + delim + mcpid.aesskein + delim + RoundToString(mcpid.rac,0)
                    + delim + RoundToString(mcpid.pobdifficulty,5) + delim + RoundToString((double)mcpid.diffbytes,0)
                    + delim + mcpid.enccpid
                    + delim + mcpid.encaes + delim + RoundToString(mcpid.nonce,0) + delim + RoundToString(mcpid.NetworkRAC,0)
                    + delim + version
                    + delim + RoundToString(mcpid.ResearchSubsidy,subsidy_places)
                    + delim + RoundToString(mcpid.LastPaymentTime,0)
                    + delim + RoundToString(mcpid.RSAWeight,0)
                    + delim + mcpid.cpidv2
                    + delim + RoundToString(mcpid.Magnitude,0)
                    + delim + mcpid.GRCAddress + delim + mcpid.lastblockhash
                    + delim + RoundToString(mcpid.InterestSubsidy,subsidy_places) + delim + mcpid.Organization
                    + delim + mcpid.OrganizationKey + delim + mcpid.NeuralHash + delim + mcpid.superblock
                    + delim + RoundToString(mcpid.ResearchSubsidy2,2) + delim + RoundToString(mcpid.ResearchAge,6)
                    + delim + RoundToString(mcpid.ResearchMagnitudeUnit,6) + delim + RoundToString(mcpid.ResearchAverageMagnitude,2)
                    + delim + mcpid.LastPORBlockHash + delim + mcpid.CurrentNeuralHash + delim + mcpid.BoincPublicKey + delim + mcpid.BoincSignature;
    return bb;
}



MiningCPID DeserializeBoincBlock(std::string block, int BlockVersion)
{
    MiningCPID surrogate = GetMiningCPID();
    int subsidy_places= BlockVersion<8 ? 2 : 8;
    try
    {

    std::vector<std::string> s = split(block,"<|>");
    if (s.size() > 7)
    {
        surrogate.cpid = s[0];
        surrogate.projectname = s[1];
        boost::to_lower(surrogate.projectname);
        surrogate.aesskein = s[2];
        surrogate.rac = RoundFromString(s[3],0);
        surrogate.pobdifficulty = RoundFromString(s[4],6);
        surrogate.diffbytes = (unsigned int)RoundFromString(s[5],0);
        surrogate.enccpid = s[6];
        surrogate.encboincpublickey = s[6];
        surrogate.encaes = s[7];
        surrogate.nonce = RoundFromString(s[8],0);
        if (s.size() > 9)
        {
            surrogate.NetworkRAC = RoundFromString(s[9],0);
        }
        if (s.size() > 10)
        {
            surrogate.clientversion = s[10];
        }
        if (s.size() > 11)
        {
            surrogate.ResearchSubsidy = RoundFromString(s[11],2);
        }
        if (s.size() > 12)
        {
            surrogate.LastPaymentTime = RoundFromString(s[12],0);
        }
        if (s.size() > 13)
        {
            surrogate.RSAWeight = RoundFromString(s[13],0);
        }
        if (s.size() > 14)
        {
            surrogate.cpidv2 = s[14];
        }
        if (s.size() > 15)
        {
            surrogate.Magnitude = RoundFromString(s[15],0);
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
            surrogate.InterestSubsidy = RoundFromString(s[18],subsidy_places);
        }
        if (s.size() > 19)
        {
            surrogate.Organization = s[19];
        }
        if (s.size() > 20)
        {
            surrogate.OrganizationKey = s[20];
        }
        if (s.size() > 21)
        {
            surrogate.NeuralHash = s[21];
        }
        if (s.size() > 22)
        {
            surrogate.superblock = s[22];
        }
        if (s.size() > 23)
        {
            surrogate.ResearchSubsidy2 = RoundFromString(s[23],subsidy_places);
        }
        if (s.size() > 24)
        {
            surrogate.ResearchAge = RoundFromString(s[24],6);
        }
        if (s.size() > 25)
        {
            surrogate.ResearchMagnitudeUnit = RoundFromString(s[25],6);
        }
        if (s.size() > 26)
        {
            surrogate.ResearchAverageMagnitude = RoundFromString(s[26],2);
        }
        if (s.size() > 27)
        {
            surrogate.LastPORBlockHash = s[27];
        }
        if (s.size() > 28)
        {
            surrogate.CurrentNeuralHash = s[28];
        }
        if (s.size() > 29)
        {
            surrogate.BoincPublicKey = s[29];
        }
        if (s.size() > 30)
        {
            surrogate.BoincSignature = s[30];
        }

    }
    }
    catch (...)
    {
            LogPrintf("Deserialize ended with an error (06182014) ");
    }
    return surrogate;
}


void InitializeProjectStruct(StructCPID& project)
{
    std::string email = GetArgument("email", "NA");
    boost::to_lower(email);

    project.email = email;
    std::string cpid_non = project.cpidhash+email;
    project.boincruntimepublickey = project.cpidhash;
    project.cpid = CPID(cpid_non).hexdigest();
    std::string ENCbpk = AdvancedCrypt(cpid_non);
    project.boincpublickey = ENCbpk;
    project.cpidv2 = ComputeCPIDv2(email, project.cpidhash, 0);
    // (Old netsoft link) project.link = "http://boinc.netsoft-online.com/get_user.php?cpid=" + project.cpid;
    project.link = "http://boinc.netsoft-online.com/e107_plugins/boinc/get_user.php?cpid=" + project.cpid;
    //Local CPID with struct
    //Must contain cpidv2, cpid, boincpublickey
    project.Iscpidvalid = IsLocalCPIDValid(project);
    if (fDebug10) LogPrintf("Memorizing local project %s, CPID Valid: %s;    ",project.projectname, YesNo(project.Iscpidvalid));
}

bool ProjectIsValid(std::string sProject)
{
    if (sProject.empty())
        return false;

    boost::to_lower(sProject);

    for (const auto& item : ReadCacheSection("project"))
    {
        const AppCacheEntry& entry = item.second;
        std::string sProjectName = ToOfficialName(entry.value);

        if (sProjectName == sProject)
            return true;
    }

    return false;
}

std::string strReplace(std::string& str, const std::string& oldStr, const std::string& newStr)
{
    assert(oldStr.empty() == false && "Cannot replace an empty string");

    size_t pos = 0;
    while((pos = str.find(oldStr, pos)) != std::string::npos)
    {
        str.replace(pos, oldStr.length(), newStr);
        pos += newStr.length();
    }
    return str;
}

std::string LowerUnderscore(std::string data)
{
    boost::to_lower(data);
    data = strReplace(data,"_"," ");
    return data;
}

std::string ToOfficialName(std::string proj)
{
        proj = LowerUnderscore(proj);
        //Convert local XML project name [On the Left] to official [Netsoft] projectname:
    for(const auto& item : ReadCacheSection("projectmapping"))
        {
        const std::string& key = item.first;
        const AppCacheEntry& entry = item.second;

        std::string project_boinc   = key;
        std::string project_netsoft = entry.value;
                                proj=LowerUnderscore(proj);
                                project_boinc=LowerUnderscore(project_boinc);
                                project_netsoft=LowerUnderscore(project_netsoft);
                                if (proj==project_boinc) proj=project_netsoft;
                            }

        return proj;
}

void HarvestCPIDs(bool cleardata)
{

    if (fDebug10) LogPrintf("loading BOINC cpids ...");

    //Remote Boinc Feature - R Halford
    std::string sBoincKey = GetArgument("boinckey","");

    if (!sBoincKey.empty())
    {
        //Deserialize key into Global CPU Mining CPID 2-6-2015
        LogPrintf("Using key %s ",sBoincKey);

        std::string sDec=DecodeBase64(sBoincKey);
        LogPrintf("Using key %s ",sDec);

        if (sDec.empty()) LogPrintf("Error while deserializing boinc key!  Please use execute genboinckey to generate a boinc key from the host with boinc installed.");
        //Version not needed for keys for now
        GlobalCPUMiningCPID = DeserializeBoincBlock(sDec,7);

        GlobalCPUMiningCPID.initialized = true;

        if (GlobalCPUMiningCPID.cpid.empty())
        {
                 LogPrintf("Error while deserializing boinc key!  Please use execute genboinckey to generate a boinc key from the host with boinc installed.");
        }
        else
        {
            LogPrintf("CPUMiningCPID Initialized.");
        }

            GlobalCPUMiningCPID.email = GlobalCPUMiningCPID.aesskein;
            LogPrintf("Using Serialized Boinc CPID %s with orig email of %s and bpk of %s with cpidhash of %s ",GlobalCPUMiningCPID.cpid, GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, GlobalCPUMiningCPID.cpidhash);
            GlobalCPUMiningCPID.cpidhash = GlobalCPUMiningCPID.boincruntimepublickey;
            LogPrintf("Using Serialized Boinc CPID %s with orig email of %s and bpk of %s with cpidhash of %s ",GlobalCPUMiningCPID.cpid, GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, GlobalCPUMiningCPID.cpidhash);
            StructCPID structcpid = GetStructCPID();
            structcpid.initialized = true;
            structcpid.cpidhash = GlobalCPUMiningCPID.cpidhash;
            structcpid.projectname = GlobalCPUMiningCPID.projectname;
            structcpid.team = "gridcoin"; //Will be verified later during Netsoft Call
            structcpid.verifiedteam = "gridcoin";
            structcpid.rac = GlobalCPUMiningCPID.rac;
            structcpid.cpid = GlobalCPUMiningCPID.cpid;
            structcpid.boincpublickey = GlobalCPUMiningCPID.encboincpublickey;
            structcpid.boincruntimepublickey = structcpid.cpidhash;
            structcpid.NetworkRAC = GlobalCPUMiningCPID.NetworkRAC;
            structcpid.email = GlobalCPUMiningCPID.email;
            // 2-6-2015 R Halford - Ensure CPIDv2 Is populated After deserializing GenBoincKey
            LogPrintf("GenBoincKey using email %s and cpidhash %s key %s ", structcpid.email, structcpid.cpidhash, sDec);
            structcpid.cpidv2 = ComputeCPIDv2(structcpid.email, structcpid.cpidhash, 0);
            // Old link: structcpid.link = "http://boinc.netsoft-online.com/get_user.php?cpid=" + structcpid.cpid;
            structcpid.link = "http://boinc.netsoft-online.com/e107_plugins/boinc/get_user.php?cpid=" + structcpid.cpid;
            structcpid.Iscpidvalid = true;
            mvCPIDs.insert(map<string,StructCPID>::value_type(structcpid.projectname,structcpid));
            // CreditCheck(structcpid.cpid,false);
            GetNextProject(false);
            if (fDebug10) LogPrintf("GCMCPI %s",GlobalCPUMiningCPID.cpid);
            if (fDebug10)           LogPrintf("Finished getting first remote boinc project");
        return;
  }

 try
 {
    std::string sourcefile = GetBoincDataDir() + "client_state.xml";
    std::string sout = "";
    sout = getfilecontents(sourcefile);
    if (sout == "-1")
    {
        LogPrintf("Unable to obtain Boinc CPIDs ");

        if (mapArgs.count("-boincdatadir") && mapArgs["-boincdatadir"].length() > 0)
        {
            LogPrintf("Boinc data directory set in gridcoinresearch.conf has been incorrectly specified ");
        }

        else LogPrintf("Boinc data directory is not in the operating system's default location \nPlease move it there or specify its current location in gridcoinresearch.conf");

        return;
    }

    if (cleardata)
    {
        mvCPIDs.clear();
    }
    std::string email = GetArgument("email","");
    boost::to_lower(email);

    int iRow = 0;
    std::vector<std::string> vCPID = split(sout.c_str(),"<project>");
    std::string investor = GetArgument("investor","false");

    if (investor=="true")
    {
            msPrimaryCPID="INVESTOR";
    }
    else
    {

            if (vCPID.size() > 0)
            {
                for (unsigned int i = 0; i < vCPID.size(); i++)
                {
                    std::string email_hash = ExtractXML(vCPID[i],"<email_hash>","</email_hash>");
                    std::string cpidhash = ExtractXML(vCPID[i],"<cross_project_id>","</cross_project_id>");
                    std::string externalcpid = ExtractXML(vCPID[i],"<external_cpid>","</external_cpid>");
                    std::string utc=ExtractXML(vCPID[i],"<user_total_credit>","</user_total_credit>");
                    std::string rac=ExtractXML(vCPID[i],"<user_expavg_credit>","</user_expavg_credit>");
                    std::string proj=ExtractXML(vCPID[i],"<project_name>","</project_name>");
                    std::string team=ExtractXML(vCPID[i],"<team_name>","</team_name>");
                    std::string rectime = ExtractXML(vCPID[i],"<rec_time>","</rec_time>");

                    boost::to_lower(proj);
                    proj = ToOfficialName(proj);
                    ProjectIsValid(proj);
                    int64_t nStart = GetTimeMillis();
                    if (cpidhash.length() > 5 && proj.length() > 3)
                    {
                        std::string cpid_non = cpidhash+email;
                        to_lower(cpid_non);
                        StructCPID structcpid = GetInitializedStructCPID2(proj,mvCPIDs);
                        iRow++;
                        structcpid.cpidhash = cpidhash;
                        structcpid.projectname = proj;
                        boost::to_lower(team);
                        structcpid.team = team;
                        InitializeProjectStruct(structcpid);
                        int64_t elapsed = GetTimeMillis()-nStart;
                        if (fDebug3)
                            LogPrintf("Enumerating boinc local project %s cpid %s valid %s, elapsed %" PRId64, structcpid.projectname, structcpid.cpid, YesNo(structcpid.Iscpidvalid), elapsed);

                        structcpid.rac = RoundFromString(rac,0);
                        structcpid.verifiedrac = RoundFromString(rac,0);
                        std::string sLocalClientEmailHash = RetrieveMd5(email);

                        if (email_hash != sLocalClientEmailHash)
                        {
                            structcpid.errors = "Gridcoin Email setting does not match project Email.  Check Gridcoin e-mail address setting or boinc project e-mail setting.";
                            structcpid.Iscpidvalid=false;
                        }


                        if (!structcpid.Iscpidvalid)
                        {
                            structcpid.errors = "CPID calculation invalid.  Check e-mail address and try resetting the boinc project.";
                        }

                        structcpid.utc = RoundFromString(utc,0);
                        structcpid.rectime = RoundFromString(rectime,0);
                        double currenttime =  GetAdjustedTime();
                        double nActualTimespan = currenttime - structcpid.rectime;
                        structcpid.age = nActualTimespan;
                        std::string sKey = structcpid.cpid + ":" + proj;
                        mvCPIDs[proj] = structcpid;

                        if (!structcpid.Iscpidvalid)
                        {
                            structcpid.errors = "CPID invalid.  Check E-mail address.";
                        }

                        if (structcpid.team != "gridcoin")
                        {
                            structcpid.Iscpidvalid = false;
                            structcpid.errors = "Team invalid";
                        }
                        bool bTestExternal = true;
                        bool bTestInternal = true;

                        if (!externalcpid.empty())
                        {
                            LogPrintf("External CPID not empty %s", externalcpid);

                            bTestExternal = CPIDAcidTest2(cpidhash,externalcpid);
                            bTestInternal = CPIDAcidTest2(cpidhash,structcpid.cpid);
                            if (bTestExternal)
                            {
                                structcpid.cpid = externalcpid;
                                LogPrintf(" Setting CPID to %s ",structcpid.cpid);
                            }
                            else
                            {
                                LogPrintf("External test failed.");
                            }


                            if (!bTestExternal && !bTestInternal)
                            {
                                structcpid.Iscpidvalid = false;
                                structcpid.errors = "CPID corrupted Internal: %s, External: %s" + structcpid.cpid + "," + externalcpid.c_str();
                                LogPrintf("CPID corrupted Internal: %s, External: %s", structcpid.cpid, externalcpid);
                            }
                            mvCPIDs[proj] = structcpid;
                        }

                        if (structcpid.Iscpidvalid)
                        {
                                // Verify the CPID is a valid researcher:
                                if (IsResearcher(structcpid.cpid))
                                {
                                    GlobalCPUMiningCPID.cpidhash = cpidhash;
                                    GlobalCPUMiningCPID.email = email;
                                    GlobalCPUMiningCPID.boincruntimepublickey = cpidhash;
                                    LogPrintf("Setting bpk to %s", cpidhash);

                                    if (structcpid.team=="gridcoin")
                                    {
                                        msPrimaryCPID = structcpid.cpid;
                                            //Let the Neural Network know what your CPID is so it can be charted:
                                            std::string sXML = "<KEY>PrimaryCPID</KEY><VALUE>" + msPrimaryCPID + "</VALUE>";
                                        std::string sData = NN::ExecuteDotNetStringFunction("WriteKey",sXML);
                                        //Try to get a neural RAC report
                                        AsyncNeuralRequest("explainmag",msPrimaryCPID,5);
                                    }
                                }
                        }

                        mvCPIDs[proj] = structcpid;
                        if (fDebug10) LogPrintf("Adding Local Project %s ", structcpid.cpid);

                    }

                }

            }
            // If no valid boinc projects were found:
            if (msPrimaryCPID.empty()) msPrimaryCPID="INVESTOR";

        }
    }
    catch (std::exception &e)
    {
             LogPrintf("Error while harvesting CPIDs.");
    }
    catch(...)
    {
             LogPrintf("Error while harvesting CPIDs 2.");
    }
}

void LoadCPIDs()
{
    LogPrintf("Load CPID; ");
    HarvestCPIDs(true);
    LogPrintf(" Getting first project;");
    GetNextProject(false);
    LogPrintf(" Finished getting first project");
}

StructCPID GetStructCPID()
{
    StructCPID c;
    c.initialized=false;
    c.rac = 0;
    c.utc=0;
    c.rectime=0;
    c.age = 0;
    c.verifiedutc=0;
    c.verifiedrectime=0;
    c.verifiedage=0;
    c.entries=0;
    c.AverageRAC=0;
    c.NetworkProjects=0;
    c.Iscpidvalid=false;
    c.NetworkRAC=0;
    c.TotalRAC=0;
    c.Magnitude=0;
    c.PaymentMagnitude=0;
    c.owed=0;
    c.payments=0;
    c.verifiedTotalRAC=0;
    c.verifiedMagnitude=0;
    c.TotalMagnitude=0;
    c.LowLockTime=0;
    c.HighLockTime=0;
    c.Accuracy=0;
    c.totalowed=0;
    c.LastPaymentTime=0;
    c.EarliestPaymentTime=0;
    c.PaymentTimespan=0;
    c.ResearchSubsidy = 0;
    c.InterestSubsidy = 0;
    c.ResearchAverageMagnitude = 0;
    c.interestPayments = 0;
    c.payments = 0;
    c.LastBlock = 0;
    c.NetworkMagnitude=0;
    c.NetworkAvgMagnitude=0;

    return c;

}

MiningCPID GetMiningCPID()
{
    MiningCPID mc;
    mc.rac = 0;
    mc.pobdifficulty = 0;
    mc.diffbytes = 0;
    mc.initialized = false;
    mc.nonce = 0;
    mc.NetworkRAC=0;
    mc.lastblockhash = "0";
    mc.Magnitude = 0;
    mc.RSAWeight = 0;
    mc.LastPaymentTime=0;
    mc.ResearchSubsidy = 0;
    mc.InterestSubsidy = 0;
    mc.ResearchSubsidy2 = 0;
    mc.ResearchAge = 0;
    mc.ResearchMagnitudeUnit = 0;
    mc.ResearchAverageMagnitude = 0;
    return mc;
}

bool SendMessages(CNode* pto, bool fSendTrickle)
{
    // Treat lock failures as send successes in case the caller disconnects
    // the node based on the return value.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return true;

    // Don't send anything until we get their version message
    if (pto->nVersion == 0)
        return true;

    //
    // Message: ping
    //
    bool pingSend = false;
    if (pto->fPingQueued)
    {
        // RPC ping request by user
        pingSend = true;
    }
    if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros())
    {
        // Ping automatically sent as a latency probe & keepalive.
        pingSend = true;
    }
    if (pingSend)
    {
        uint64_t nonce = 0;
        while (nonce == 0) {
            RAND_bytes((unsigned char*)&nonce, sizeof(nonce));
        }
        pto->fPingQueued = false;
        pto->nPingUsecStart = GetTimeMicros();
        if (pto->nVersion > BIP0031_VERSION)
        {
            pto->nPingNonceSent = nonce;
            std::string acid = GetCommandNonce("ping");
            pto->PushMessage("ping", nonce, acid);
        } else
        {
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
            for (auto const& pnode : vNodes)
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
        for (auto const& addr : pto->vAddrToSend)
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
        for (auto const& inv : pto->vInventoryToSend)
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
            if (fDebugNet)        LogPrintf("sending getdata: %s", inv.ToString());
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

    return true;
}

void IncrementCurrentNeuralNetworkSupermajority(std::string NeuralHash, std::string GRCAddress, double distance)
{
    if (NeuralHash.length() < 5) return;
    double temp_hashcount = 0;
    if (mvCurrentNeuralNetworkHash.size() > 0)
    {
            temp_hashcount = mvCurrentNeuralNetworkHash[NeuralHash];
    }
    // 6-13-2015 ONLY Count Each Neural Hash Once per GRC address / CPID (1 VOTE PER RESEARCHER)
    std::string Security = ReadCache("currentneuralsecurity",GRCAddress).value;
    if (Security == NeuralHash)
    {
        //This node has already voted, throw away the vote
        return;
    }
    WriteCache("currentneuralsecurity",GRCAddress,NeuralHash,GetAdjustedTime());
    if (temp_hashcount == 0)
    {
        mvCurrentNeuralNetworkHash.insert(map<std::string,double>::value_type(NeuralHash,0));
    }
    double multiplier = 200;
    if (distance < 40) multiplier = 400;
    double votes = (1/distance)*multiplier;
    temp_hashcount += votes;
    mvCurrentNeuralNetworkHash[NeuralHash] = temp_hashcount;
}



void IncrementNeuralNetworkSupermajority(const std::string& NeuralHash, const std::string& GRCAddress, double distance, const CBlockIndex* pblockindex)
{
    if (NeuralHash.length() < 5) return;
    if (pblockindex->nVersion >= 8)
    {
        try
        {
            CBitcoinAddress address(GRCAddress);
            bool validaddresstovote = address.IsValid();
            if (!validaddresstovote)
            {
                LogPrintf("INNS : Vote found in block with invalid GRC address. HASH: %s GRC: %s", NeuralHash, GRCAddress);
                return;
            }
            if (!IsNeuralNodeParticipant(GRCAddress, pblockindex->nTime))
            {
                LogPrintf("INNS : Vote found in block from ineligible neural node participant. HASH: %s GRC: %s", NeuralHash, GRCAddress);
                return;
            }
        }
        catch (const bignum_error& innse)
        {
            LogPrintf("INNS : Exception: %s", innse.what());
            return;
        }
    }
    double temp_hashcount = 0;
    if (mvNeuralNetworkHash.size() > 0)
    {
            temp_hashcount = mvNeuralNetworkHash[NeuralHash];
    }
    // 6-13-2015 ONLY Count Each Neural Hash Once per GRC address / CPID (1 VOTE PER RESEARCHER)
    std::string Security = ReadCache("neuralsecurity",GRCAddress).value;
    if (Security == NeuralHash)
    {
        //This node has already voted, throw away the vote
        return;
    }
    WriteCache("neuralsecurity",GRCAddress,NeuralHash,GetAdjustedTime());
    if (temp_hashcount == 0)
    {
        mvNeuralNetworkHash.insert(map<std::string,double>::value_type(NeuralHash,0));
    }
    double multiplier = 200;
    if (distance < 40) multiplier = 400;
    double votes = (1/distance)*multiplier;
    temp_hashcount += votes;
    mvNeuralNetworkHash[NeuralHash] = temp_hashcount;
}


void IncrementVersionCount(const std::string& Version)
{
    if(!Version.empty())
        mvNeuralVersion[Version]++;
}



std::string GetNeuralNetworkSupermajorityHash(double& out_popularity)
{
    double highest_popularity = -1;
    std::string neural_hash;

    for(const auto& network_hash : mvNeuralNetworkHash)
    {
        const std::string& hash = network_hash.first;
        double popularity       = network_hash.second;

        // d41d8 is the hash of an empty magnitude contract - don't count it
        if (popularity > 0 &&
            popularity > highest_popularity &&
            hash != "d41d8cd98f00b204e9800998ecf8427e" &&
            hash != "TOTAL_VOTES")
        {
            highest_popularity = popularity;
            neural_hash = hash;
        }
    }

    out_popularity = highest_popularity;
    return neural_hash;
}


std::string GetCurrentNeuralNetworkSupermajorityHash(double& out_popularity)
{
    double highest_popularity = -1;
    std::string neural_hash = "";
    for(map<std::string,double>::iterator ii=mvCurrentNeuralNetworkHash.begin(); ii!=mvCurrentNeuralNetworkHash.end(); ++ii)
    {
                double popularity = mvCurrentNeuralNetworkHash[(*ii).first];
                // d41d8 is the hash of an empty magnitude contract - don't count it
                if ( ((*ii).first != "d41d8cd98f00b204e9800998ecf8427e") && popularity > 0 && popularity > highest_popularity && (*ii).first != "TOTAL_VOTES")
                {
                    highest_popularity = popularity;
                    neural_hash = (*ii).first;
                }
    }
    out_popularity = highest_popularity;
    return neural_hash;
}






std::string GetNeuralNetworkReport()
{
    //Returns a report of the networks neural hashes in order of popularity
    std::string neural_hash = "";
    std::string report = "Neural_hash, Popularity\n";
    std::string row = "";
    for(map<std::string,double>::iterator ii=mvNeuralNetworkHash.begin(); ii!=mvNeuralNetworkHash.end(); ++ii)
    {
                double popularity = mvNeuralNetworkHash[(*ii).first];
                neural_hash = (*ii).first;
                row = neural_hash+ "," + RoundToString(popularity,0);
                report += row + "\n";
    }

    return report;
}

std::string GetOrgSymbolFromFeedKey(std::string feedkey)
{
    std::string Symbol = ExtractValue(feedkey,"-",0);
    return Symbol;

}



bool MemorizeMessage(const CTransaction &tx, double dAmount, std::string sRecipient)
{
    const std::string &msg = tx.hashBoinc;
    const int64_t &nTime = tx.nTime;
          if (msg.empty()) return false;
          bool fMessageLoaded = false;

          if (Contains(msg,"<MT>"))
          {
              std::string sMessageType      = ExtractXML(msg,"<MT>","</MT>");
              std::string sMessageKey       = ExtractXML(msg,"<MK>","</MK>");
              std::string sMessageValue     = ExtractXML(msg,"<MV>","</MV>");
              std::string sMessageAction    = ExtractXML(msg,"<MA>","</MA>");
              std::string sSignature        = ExtractXML(msg,"<MS>","</MS>");
              std::string sMessagePublicKey = ExtractXML(msg,"<MPK>","</MPK>");
              if (sMessageType=="beacon" && Contains(sMessageValue,"INVESTOR"))
              {
                    sMessageValue="";
              }

              if (sMessageType=="superblock")
              {
                  // Deny access to superblock processing runtime data
                  sMessageValue="";
              }

              if (!sMessageType.empty() && !sMessageKey.empty() && !sMessageValue.empty() && !sMessageAction.empty() && !sSignature.empty())
              {
                  //Verify sig first
                  bool Verified = CheckMessageSignature(sMessageAction,sMessageType,sMessageType+sMessageKey+sMessageValue,
                      sSignature,sMessagePublicKey);

                  if (Verified)
                  {
                        if (sMessageAction=="A")
                        {
                                /* With this we allow verifying blocks with stupid beacon */
                                if("beacon"==sMessageType)
                                {
                                    std::string out_cpid = "";
                                    std::string out_address = "";
                                    std::string out_publickey = "";
                                    GetBeaconElements(sMessageValue, out_cpid, out_address, out_publickey);
                                    WriteCache("beaconalt",sMessageKey+"."+ToString(nTime),out_publickey,nTime);
                                }

                                WriteCache(sMessageType,sMessageKey,sMessageValue,nTime);
                                if(fDebug10 && sMessageType=="beacon" ){
                                    LogPrintf("BEACON add %s %s %s", sMessageKey, DecodeBase64(sMessageValue), TimestampToHRDate(nTime));
                                }
                                fMessageLoaded = true;
                                if (sMessageType=="poll")
                                {
                                        if (Contains(sMessageKey,"[Foundation"))
                                        {
                                                msPoll = "Foundation Poll: " + sMessageKey.substr(0,80);
                                        }
                                        else
                                        {
                                                msPoll = "Poll: " + sMessageKey.substr(0,80);
                                        }
                                }
                        }
                        else if(sMessageAction=="D")
                        {
                                if (fDebug10) LogPrintf("Deleting key type %s Key %s Value %s", sMessageType, sMessageKey, sMessageValue);
                                if(fDebug10 && sMessageType=="beacon" ){
                                    LogPrintf("BEACON DEL %s - %s", sMessageKey, TimestampToHRDate(nTime));
                                }
                                DeleteCache(sMessageType,sMessageKey);
                                fMessageLoaded = true;
                        }
                        // If this is a boinc project, load the projects into the coin:
                        if (sMessageType=="project" || sMessageType=="projectmapping")
                        {
                            //Reserved
                            fMessageLoaded = true;
                        }

                        if(fDebug)
                            WriteCache("TrxID;"+sMessageType,sMessageKey,tx.GetHash().GetHex(),nTime);
            }
                  }
                }

   return fMessageLoaded;
}

double GRCMagnitudeUnit(int64_t locktime)
{
    //7-12-2015 - Calculate GRCMagnitudeUnit (Amount paid per magnitude per day)
    StructCPID network = GetInitializedStructCPID2("NETWORK",mvNetwork);
    double TotalNetworkMagnitude = network.NetworkMagnitude;
    if (TotalNetworkMagnitude < 1000) TotalNetworkMagnitude=1000;
    double MaximumEmission = BLOCKS_PER_DAY*GetMaximumBoincSubsidy(locktime);
    double Kitty = MaximumEmission - (network.payments/14);
    if (Kitty < 1) Kitty = 1;
    double MagnitudeUnit = 0;
    if (AreBinarySuperblocksEnabled(nBestHeight))
    {
        MagnitudeUnit = (Kitty/TotalNetworkMagnitude)*1.25;
    }
    else
    {
        MagnitudeUnit = Kitty/TotalNetworkMagnitude;
    }
    if (MagnitudeUnit > 5) MagnitudeUnit = 5; //Just in case we lose a superblock or something strange happens.
    MagnitudeUnit = SnapToGrid(MagnitudeUnit); //Snaps the value into .025 increments
    return MagnitudeUnit;
}


int64_t ComputeResearchAccrual(int64_t nTime, std::string cpid, std::string operation, CBlockIndex* pindexLast, bool bVerifyingBlock, int iVerificationPhase, double& dAccrualAge, double& dMagnitudeUnit, double& AvgMagnitude)
{
    double dCurrentMagnitude = CalculatedMagnitude2(cpid, nTime, false);
    if(pindexLast->nVersion>=9)
    {
        // Bugfix for newbie rewards always being around 1 GRC
        dMagnitudeUnit = GRCMagnitudeUnit(nTime);
    }
    CBlockIndex* pHistorical = GetHistoricalMagnitude(cpid);
    bool bIsNewbie = (pHistorical->nHeight <= nNewIndex || pHistorical->nTime==0);
    if(pindexLast->nVersion<9)
    {
        // Bugfix for zero mag stakes being mistaken for newbie stake
        bIsNewbie |= pHistorical->nMagnitude==0;
    }
    if (bIsNewbie)
    {
        //No prior block exists... Newbies get .01 age to bootstrap the CPID (otherwise they will not have any prior block to refer to, thus cannot get started):
        if(fDebug && !bVerifyingBlock) LogPrintf("CRE: No prior block exists...");
        if (!AreBinarySuperblocksEnabled(pindexLast->nHeight))
        {
                if(fDebug) LogPrintf("ComputeResearchAccrual: %s Newbie stake, Binary SB not enabled, dCurrentMagnitude= %.1f",
                    cpid, dCurrentMagnitude);
            return dCurrentMagnitude > 0 ? ((dCurrentMagnitude/100)*COIN) : 0;
        }
        else
        {
            // New rules - 12-4-2015 - Pay newbie from the moment beacon was sent as long as it is within 6 months old and NN mag > 0 and newbie is in the superblock and their lifetime paid is zero
            // Note: If Magnitude is zero, or researcher is not in superblock, or lifetimepaid > 0, this function returns zero
            int64_t iBeaconTimestamp = BeaconTimeStamp(cpid, true);
            if (IsLockTimeWithinMinutes(iBeaconTimestamp, pindexBest->GetBlockTime(), 60*24*30*6))
            {
                double dNewbieAccrualAge = ((double)nTime - (double)iBeaconTimestamp) / 86400;
                int64_t iAccrual = (int64_t)((dNewbieAccrualAge*dCurrentMagnitude*dMagnitudeUnit*COIN) + (1*COIN));
                if ((dNewbieAccrualAge*dCurrentMagnitude*dMagnitudeUnit) > 500)
                {
                    LogPrintf("ComputeResearchAccrual: %s Newbie special stake too high, reward=500GRC", cpid);
                    return (500*COIN);
                }
                if(fDebug) LogPrintf("ComputeResearchAccrual: %s Newbie stake, unit=%f, age=%f, magnitude= %.1f -> %f",
                    cpid, dMagnitudeUnit, dNewbieAccrualAge, dCurrentMagnitude, iAccrual/(double)COIN);
                return iAccrual;
            }
            else
            {
                LogPrintf("ComputeResearchAccrual: %s Invalid Beacon, Using 0.01 age bootstrap", cpid);
                return dCurrentMagnitude > 0 ? (((dCurrentMagnitude/100)*COIN) + (1*COIN)): 0;
            }
        }
    }
    // To prevent reorgs and checkblock errors, ensure the research age is > 10 blocks wide:
    int iRABlockSpan = pindexLast->nHeight - pHistorical->nHeight;
    StructCPID stCPID = GetInitializedStructCPID2(cpid,mvResearchAge);
    double dAvgMag = stCPID.ResearchAverageMagnitude;
    // ResearchAge: If the accrual age is > 20 days, add in the midpoint lifetime average magnitude to ensure the overall avg magnitude accurate:
    if (iRABlockSpan > (int)(BLOCKS_PER_DAY*20))
    {
            AvgMagnitude = (pHistorical->nMagnitude + dAvgMag + dCurrentMagnitude) / 3;
    }
    else
    {
            AvgMagnitude = (pHistorical->nMagnitude + dCurrentMagnitude) / 2;
    }
    if (AvgMagnitude > 20000) AvgMagnitude = 20000;

    dAccrualAge = ((double)nTime - (double)pHistorical->nTime) / 86400;
    if (dAccrualAge < 0) dAccrualAge=0;
    dMagnitudeUnit = GRCMagnitudeUnit(nTime);

    int64_t Accrual = (int64_t)(dAccrualAge*AvgMagnitude*dMagnitudeUnit*COIN);
    // Double check researcher lifetime paid
    double days = (nTime - stCPID.LowLockTime) / 86400.0;
    double PPD = stCPID.ResearchSubsidy/(days+.01);
    double ReferencePPD = dMagnitudeUnit*dAvgMag;

    if ((PPD > ReferencePPD*5))
    {
        if(fDebug) LogPrintf("ComputeResearchAccrual: %s RA-PPD, PPD=%f, unit=%f, RAAvgMag=%f, RASubsidy=%f, RALowLockTime=%d -> Accrual 0 (would be %f)",
                cpid, PPD, dMagnitudeUnit, stCPID.ResearchAverageMagnitude,stCPID.ResearchSubsidy,stCPID.LowLockTime, Accrual/(double)COIN);

        return 0; //Since this condition can occur when a user ramps up computing power, lets return 0 so as to not shortchange the researcher, but instead, owed will continue to accrue and will be paid later when PPD falls below 5
    }
    else if (iRABlockSpan < 10 && iVerificationPhase != 2)
    {
        if(fDebug) LogPrintf("ComputeResearchAccrual: %s Block Span less than 10 (%d) -> Accrual 0 (would be %f)", cpid, iRABlockSpan, Accrual/(double)COIN);
        if(fDebug2) LogPrintf(" pHistorical w %s", pHistorical->GetBlockHash().GetHex());

        // Note that if the RA Block Span < 10, we want to return 0 for the Accrual Amount so the CPID can still receive an accurate accrual in the future
        return 0;
    }
    else
    {
        if(fDebug) LogPrintf("ComputeResearchAccrual: %s Normal, unit=%f, Age=%f, AvgMagnitude=%f, Span=%d, CurMag=%.1f, OldMag=%.1f, RAAvgMag=%f, RASubsidy=%f, RALowLockTime=%d -> %d",
            cpid, dMagnitudeUnit,dAccrualAge,AvgMagnitude,iRABlockSpan,dCurrentMagnitude,pHistorical->nMagnitude,
            stCPID.ResearchAverageMagnitude,stCPID.ResearchSubsidy,stCPID.LowLockTime, Accrual/(double)COIN);
        return Accrual;
    }
}



CBlockIndex* GetHistoricalMagnitude(std::string cpid)
{
    if (!IsResearcher(cpid)) return pindexGenesisBlock;

    // Starting at the block prior to StartHeight, find the last instance of the CPID in the chain:
    // Limit lookback to 6 months
    int nMinIndex = pindexBest->nHeight-(6*30*BLOCKS_PER_DAY);
    if (nMinIndex < 2) nMinIndex=2;
    // Last block Hash paid to researcher
    StructCPID stCPID = GetInitializedStructCPID2(cpid,mvResearchAge);
    if (!stCPID.BlockHash.empty())
    {
        uint256 hash(stCPID.BlockHash);

        auto mapItem = mapBlockIndex.find(hash);
        if (mapItem == mapBlockIndex.end())
            return pindexGenesisBlock;

        CBlockIndex* pblockindex = mapItem->second;
        if(!pblockindex->pnext && pblockindex!=pindexBest)
            LogPrintf("WARNING GetHistoricalMagnitude: index {%s %d} for cpid %s, "
            "is not in the main chain",pblockindex->GetBlockHash().GetHex().c_str(),
            pblockindex->nHeight,cpid.c_str());
        if (pblockindex->nHeight < nMinIndex)
        {
            // In this case, the last staked block was Found, but it is over 6 months old....
            LogPrintf("GetHistoricalMagnitude: Last staked block found at height %d, but cannot verify magnitude older than 6 months (min %d)!",pblockindex->nHeight,nMinIndex);
            return pindexGenesisBlock;
        }

        return pblockindex;
    }
    else
    {
        return pindexGenesisBlock;
    }
}

void ZeroOutResearcherTotals(std::string cpid)
{
    if (!cpid.empty())
    {
                StructCPID stCPID = GetInitializedStructCPID2(cpid,mvResearchAge);
                stCPID.LastBlock = 0;
                stCPID.BlockHash = "";
                stCPID.InterestSubsidy = 0;
                stCPID.ResearchSubsidy = 0;
                stCPID.Accuracy = 0;
                stCPID.LowLockTime = std::numeric_limits<unsigned int>::max();
                stCPID.HighLockTime = 0;
                stCPID.TotalMagnitude = 0;
                stCPID.ResearchAverageMagnitude = 0;

                mvResearchAge[cpid]=stCPID;
    }
}


bool LoadAdminMessages(bool bFullTableScan, std::string& out_errors)
{
    int nMaxDepth = nBestHeight;
    int nMinDepth = fTestNet ? 1 : 164618;
    nMinDepth = pindexBest->nHeight - (BLOCKS_PER_DAY*30*12);
    if (nMinDepth < 2) nMinDepth=2;
    if (!bFullTableScan) nMinDepth = nMaxDepth-6;
    if (nMaxDepth < nMinDepth) return false;
    CBlockIndex* pindex = blockFinder.FindByHeight(nMinDepth);
    // These are memorized consecutively in order from oldest to newest

    while (pindex->nHeight < nMaxDepth)
    {
        if (!pindex || !pindex->pnext) return false;
        pindex = pindex->pnext;
        if (pindex==NULL) continue;
        if (!pindex || !pindex->IsInMainChain()) continue;
        if (IsContract(pindex))
        {
            CBlock block;
            if (!block.ReadFromDisk(pindex)) continue;
            int iPos = 0;
            for (auto const &tx : block.vtx)
            {
                  if (iPos > 0)
                  {
                      // Retrieve the Burn Amount for Contracts
                      double dAmount = 0;
                      std::string sRecipient = "";
                      for (unsigned int i = 1; i < tx.vout.size(); i++)
                      {
                            sRecipient = PubKeyToAddress(tx.vout[i].scriptPubKey);
                            dAmount += CoinToDouble(tx.vout[i].nValue);
                      }
                      MemorizeMessage(tx,dAmount,sRecipient);
                  }
                  iPos++;
            }
        }
    }

    return true;
}




MiningCPID GetBoincBlockByIndex(CBlockIndex* pblockindex)
{
    CBlock block;
    MiningCPID bb;
    bb.initialized=false;
    if (!pblockindex || !pblockindex->IsInMainChain()) return bb;
    if (block.ReadFromDisk(pblockindex))
    {
        std::string hashboinc = "";
        if (block.vtx.size() > 0) hashboinc = block.vtx[0].hashBoinc;
        bb = DeserializeBoincBlock(hashboinc,block.nVersion);
        bb.initialized=true;
        return bb;
    }
    return bb;
}

std::string CPIDHash(double dMagIn, std::string sCPID)
{
    std::string sMag = RoundToString(dMagIn,0);
    double dMagLength = (double)sMag.length();
    double dExponent = pow(dMagLength,5);
    std::string sMagComponent1 = RoundToString(dMagIn/(dExponent+.01),0);
    std::string sSuffix = RoundToString(dMagLength * dExponent, 0);
    std::string sHash = sCPID + sMagComponent1 + sSuffix;
    return sHash;
}

std::string GetQuorumHash(const std::string& data)
{
    //Data includes the Magnitudes, and the Projects:
    std::string sMags = ExtractXML(data,"<MAGNITUDES>","</MAGNITUDES>");
    std::vector<std::string> vMags = split(sMags.c_str(),";");
    std::string sHashIn = "";
    for (unsigned int x = 0; x < vMags.size(); x++)
    {
        std::vector<std::string> vRow = split(vMags[x].c_str(),",");

        // Each row should consist of two fields, CPID and magnitude.
        if(vRow.size() < 2)
            continue;

        // First row (CPID) must be exactly 32 bytes.
        const std::string& sCPID = vRow[0];
        if(sCPID.size() != 32)
            continue;

        double dMag = RoundFromString(vRow[1],0);
        sHashIn += CPIDHash(dMag, sCPID) + "<COL>";
    }

    return RetrieveMd5(sHashIn);
}


std::string getHardwareID()
{
    std::string ele1 = "?";
    /*#ifdef QT_GUI
        ele1 = getMacAddress();
    #endif*/
    ele1 += ":" + getCpuHash();
    ele1 += ":" + getHardDriveSerial();

    std::string hwid = RetrieveMd5(ele1);
    return hwid;
}

#ifdef WIN32
static void getCpuid( unsigned int* p, unsigned int ax )
 {
    __asm __volatile
    (   "movl %%ebx, %%esi\n\t"
        "cpuid\n\t"
        "xchgl %%ebx, %%esi"
        : "=a" (p[0]), "=S" (p[1]),
          "=c" (p[2]), "=d" (p[3])
        : "0" (ax)
    );
 }
#endif

 std::string getCpuHash()
 {
    std::string n = boost::asio::ip::host_name();
    #ifdef WIN32
        unsigned int cpuinfo[4] = { 0, 0, 0, 0 };
        getCpuid( cpuinfo, 0 );
        unsigned short hash = 0;
        unsigned int* ptr = (&cpuinfo[0]);
        for ( unsigned int i = 0; i < 4; i++ )
            hash += (ptr[i] & 0xFFFF) + ( ptr[i] >> 16 );
        double dHash = (double)hash;
        return n + ";" + RoundToString(dHash,0);
    #else
        return n;
    #endif
 }



std::string SystemCommand(const char* cmd)
{
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while(!feof(pipe))
    {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}


std::string getHardDriveSerial()
{
    if (!msHDDSerial.empty()) return msHDDSerial;
    std::string cmd1 = "";
    #ifdef WIN32
        cmd1 = "wmic path win32_physicalmedia get SerialNumber";
    #else
        cmd1 = "ls /dev/disk/by-uuid";
    #endif
    std::string result = SystemCommand(cmd1.c_str());
    msHDDSerial = result;
    return result;
}

bool IsContract(CBlockIndex* pIndex)
{
    return pIndex->nIsContract==1 ? true : false;
}

bool IsSuperBlock(CBlockIndex* pIndex)
{
    return pIndex->nIsSuperBlock==1 ? true : false;
}


double SnapToGrid(double d)
{
    double dDither = .04;
    double dOut = RoundFromString(RoundToString(d*dDither,3),3) / dDither;
    return dOut;
}

bool IsNeuralNodeParticipant(const std::string& addr, int64_t locktime)
{
    //Calculate the neural network nodes abililty to particiapte by GRC_Address_Day
    int address_day = GetDayOfYear(locktime);
    std::string address_tohash = addr + "_" + ToString(address_day);
    std::string address_day_hash = RetrieveMd5(address_tohash);

    // For now, let's call for a 25% participation rate (approx. 125 nodes):
    // When RA is enabled, 25% of the neural network nodes will work on a quorum at any given time to alleviate stress on the project sites:
    uint256 uRef;
    if (IsResearchAgeEnabled(pindexBest->nHeight))
    {
        uRef = fTestNet
               ? uint256("0x00000000000000000000000000000000ed182f81388f317df738fd9994e7020b")
               : uint256("0x000000000000000000000000000000004d182f81388f317df738fd9994e7020b"); //This hash is approx 25% of the md5 range (90% for testnet)
    }
    else
    {
        uRef = fTestNet
               ? uint256("0x00000000000000000000000000000000ed182f81388f317df738fd9994e7020b")
               : uint256("0x00000000000000000000000000000000fd182f81388f317df738fd9994e7020b"); //This hash is approx 25% of the md5 range (90% for testnet)
    }

    uint256 uADH = uint256("0x" + address_day_hash);
    return (uADH < uRef);
}


bool StrLessThanReferenceHash(std::string rh)
{
    int address_day = GetDayOfYear(GetAdjustedTime());
    std::string address_tohash = rh + "_" + ToString(address_day);
    std::string address_day_hash = RetrieveMd5(address_tohash);
    uint256 uRef = fTestNet ? uint256("0x000000000000000000000000000000004d182f81388f317df738fd9994e7020b") : uint256("0x000000000000000000000000000000004d182f81388f317df738fd9994e7020b"); //This hash is approx 25% of the md5 range (90% for testnet)
    uint256 uADH = uint256("0x" + address_day_hash);
    return (uADH < uRef);
}

bool IsResearcher(const std::string& cpid)
{
    return cpid.length() == 32;
}
