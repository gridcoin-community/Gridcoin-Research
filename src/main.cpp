// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"
#include "net.h"
#include "streams.h"
#include "alert.h"
#include "checkpoints.h"
#include "txdb.h"
#include "init.h"
#include "ui_interface.h"
#include "kernel.h"
#include "block.h"
#include "miner.h"
#include "neuralnet/beacon.h"
#include "neuralnet/project.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/superblock.h"
#include "neuralnet/tally.h"
#include "backup.h"
#include "appcache.h"
#include "scraper_net.h"
#include "gridcoin.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <ctime>
#include <math.h>

extern std::string NodeAddress(CNode* pfrom);
extern bool WalletOutOfSync();
extern bool AskForOutstandingBlocks(uint256 hashStart);
extern void ResetTimerMain(std::string timer_name);
extern bool GridcoinServices();
extern bool BlockNeedsChecked(int64_t BlockTime);
int64_t GetEarliestWalletTransaction();
extern bool GetEarliestStakeTime(std::string grcaddress, std::string cpid);
extern double GetTotalBalance();
extern std::string PubKeyToAddress(const CScript& scriptPubKey);
extern const CBlockIndex* GetHistoricalMagnitude(const NN::MiningId mining_id);
std::string GetCommandNonce(std::string command);

unsigned int nNodeLifespan;

using namespace std;
using namespace boost;

//
// Global state
//

CCriticalSection cs_setpwalletRegistered;
set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;

CTxMemPool mempool;
int64_t nLastAskedForBlocks = 0;
int64_t nBootup = 0;
int64_t nLastGRCtallied = 0;
int64_t nLastCleaned = 0;

extern double CoinToDouble(double surrogate);

///////////////////////MINOR VERSION////////////////////////////////

extern int64_t GetCoinYearReward(int64_t nTime);

BlockMap mapBlockIndex;
set<pair<COutPoint, unsigned int> > setStakeSeen;

CBigNum bnProofOfWorkLimit(ArithToUint256(~arith_uint256() >> 20)); // "standard" scrypt target limit for proof of work, results with 0,000244140625 proof-of-work difficulty
CBigNum bnProofOfStakeLimit(ArithToUint256(~arith_uint256() >> 20));
CBigNum bnProofOfStakeLimitV2(ArithToUint256(~arith_uint256() >> 20));
CBigNum bnProofOfWorkLimitTestNet(ArithToUint256(~arith_uint256() >> 16));

//Gridcoin Minimum Stake Age (16 Hours)
unsigned int nStakeMinAge = 16 * 60 * 60; // 16 hours
unsigned int nStakeMaxAge = -1; // unlimited
unsigned int nModifierInterval = 10 * 60; // time to elapse before new modifier is computed

// Gridcoin:
int nCoinbaseMaturity = 100;
CBlockIndex* pindexGenesisBlock = NULL;
int nBestHeight = -1;

arith_uint256 nBestChainTrust = 0;
arith_uint256 nBestInvalidTrust = 0;
uint256 hashBestChain;
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
// This is changed to MIN_TX_FEE * 10 for block version 11 (CTransaction::CURRENT_VERSION 2).
// Note that this is an early init value and will result in overpayment of the fee per kbyte
// if this code is run on a wallet prior to the v11 mandatory switchover unless a manual value
// of -paytxfee is specified as an argument.
int64_t nTransactionFee = MIN_TX_FEE * 10;
int64_t nReserveBalance = 0;
int64_t nMinimumInputValue = 0;

BlockFinder blockFinder;

// Gridcoin - Rob Halford

bool bForceUpdate = false;
bool fQtActive = false;
bool bGridcoinCoreInitComplete = false;

extern void GetGlobalStatus();
bool PollIsActive(const std::string& poll_contract);

extern bool LessVerbose(int iMax1000);

// Mining status variables
std::string    msMiningErrors;
std::string    msPoll;
std::string    msMiningErrorsIncluded;
std::string    msMiningErrorsExcluded;

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

double GetEstimatedNetworkWeight(unsigned int nPoSInterval)
{
    // The number of stakes to include in the average has been reduced to 40 (default) from 72. 72 stakes represented 1.8 hours at
    // standard spacing. This is too long. 40 blocks is nominally 1 hour.
    double result;

    // The constant below comes from (MaxHash / StandardDifficultyTarget) * 16 sec / 90 sec. If you divide it by 80 to convert to GRC you
    // get the familiar 9544517.40667
    result = 763561392.533 * GetAverageDifficulty(nPoSInterval);
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedNetworkWeight debug: Network Weight = %f", result);
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedNetworkWeight debug: Network Weight in GRC = %f", result / 80.0);

    return result;
}

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (pindexBest == NULL)
            return 1.0;
        else
            blockindex = GetLastBlockIndex(pindexBest, false);
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
            (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GetBlockDifficulty(unsigned int nBits)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    int nShift = (nBits >> 24) & 0xff;

    double dDiff =
            (double)0x0000ffff / (double)(nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
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
                LogPrint(BCLog::LogFlags::NOISY, "GetAverageDifficulty debug: dDiff = %f", dDiff);
                LogPrint(BCLog::LogFlags::NOISY, "GetAverageDifficulty debug: nStakesHandled = %u", nStakesHandled);
            }
        }

        pindex = pindex->pprev;
    }

    result = nStakesHandled ? dDiffSum / nStakesHandled : 0;
    LogPrint(BCLog::LogFlags::NOISY, "GetAverageDifficulty debug: Average dDiff = %f", result);

    return result;
}

double GetEstimatedTimetoStake(bool ignore_staking_status, double dDiff, double dConfidence)
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
        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: Confidence of 0 specified: ETTS = %f", result);
        return result;
    }

    bool staking;
    bool able_to_stake;

    {
        LOCK(MinerStatus.lock);

        staking = MinerStatus.nLastCoinStakeSearchInterval && MinerStatus.WeightSum;

        able_to_stake = MinerStatus.able_to_stake;
    }

    // Get out early if not staking, ignore_staking_status is false, and not able_to_stake and set return value of 0.
    if (!ignore_staking_status && !staking && !able_to_stake)
    {
        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: Not staking: ETTS = %f", result);
        return result;
    }

    int64_t nValue = 0;
    int64_t nCurrentTime = GetAdjustedTime();
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nCurrentTime = %i", nCurrentTime);

    CTxDB txdb("r");

    // Here I am defining a time mask 16 times as long as the normal stake time mask. This is to quantize the UTXO's into a maximum of
    // 16 hours * 3600 / 256 = 225 time bins for evaluation. Otherwise for a large number of UTXO's, this algorithm could become
    // really expensive.
    const int ETTS_TIMESTAMP_MASK = (16 * (STAKE_TIMESTAMP_MASK + 1)) - 1;
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: ETTS_TIMESTAMP_MASK = %x", ETTS_TIMESTAMP_MASK);

    int64_t BalanceAvailForStaking = 0;
    vector<COutput> vCoins;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        BalanceAvailForStaking = pwalletMain->GetBalance() - nReserveBalance;

        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: BalanceAvailForStaking = %u", BalanceAvailForStaking);

        // Get out early if no balance available and set return value of 0. This should already have happened above, because with no
        // balance left after reserve, staking should be disabled; however, just to be safe...
        if (BalanceAvailForStaking <= 0)
        {
            LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: No balance available: ETTS = %f", result);
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
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nStakeMinAge = %i", nStakeMinAge);

    // If dDiff = 0 from supplied argument (which is also the default), then derive a smoothed difficulty over the default PoSInterval of 40 blocks by calling
    // GetAverageDifficulty(40), otherwise let supplied argument dDiff stand.
    if (!dDiff) dDiff = GetAverageDifficulty(40);
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dDiff = %f", dDiff);

    // The stake probability per "throw" of 1 weight unit = target value at diff of 1.0 / (maxhash * diff). This happens effectively every STAKE_TIMESTAMP_MASK+1 sec.
    double dUnitStakeProbability = 1 / (4295032833.0 * dDiff);
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dUnitStakeProbability = %e", dUnitStakeProbability);


    int64_t nTime = 0;
    for (const auto& out : vCoins)
    {
        CTxIndex txindex;
        CBlock CoinBlock; //Block which contains CoinTx
        if (!txdb.ReadTxIndex(out.tx->GetHash(), txindex)) continue; //Ignore transactions that can't be read.

        if (!CoinBlock.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false)) continue;

        // We are going to store as an event the time that the UTXO matures (is available for staking again.)
        nTime = (CoinBlock.GetBlockTime() & ~ETTS_TIMESTAMP_MASK) + nStakeMinAge;

        nValue = out.tx->vout[out.i].nValue;

        // Only consider UTXO's that are actually stakeable - which means that each one must be less than the available balance
        // subtracting the reserve. Each UTXO also has to be greater than 1/80 GRC to result in a weight greater than zero in the CreateCoinStake loop,
        // so eliminate UTXO's with less than 0.0125 GRC balances right here. The test with Satoshi units for that is
        // nValue >= 1250000.
        if (BalanceAvailForStaking >= nValue && nValue >= 1250000)
        {
        vUTXO.push_back(std::pair<int64_t, int64_t>( nTime, nValue));
        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: pair (relative to current time: <%i, %i>", nTime - nCurrentTime, nValue);

        // Only record a time below if it is after nCurrentTime, because UTXO's that have matured already are already stakeable and can be grouped (will be found)
        // by the nCurrentTime record that was already injected above.
        if (nTime > nCurrentTime) UniqueUTXOTimes.insert(nTime);
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
    for (const auto& itertime : UniqueUTXOTimes)
    {

        nTime = itertime;
        dProbAccumulator = 0;

        for (auto& iterUTXO : vUTXO)
        {

            LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: Unique UTXO Time: %u, vector pair <%u, %u>", nTime, iterUTXO.first, iterUTXO.second);

            // If the "negative Gantt chart bar" is ending or has ended for a UTXO, it now accumulates probability. (I.e. the event time being checked
            // is greater than or equal to the cooldown expiration of the UTXO.)
            // accumulation for that UTXO.
            if(nTime >= iterUTXO.first)
            {
                // The below weight calculation is just like the CalculateStakeWeightV8 in kernel.cpp.
                nCoinWeight = iterUTXO.second / 1250000;

                dProbAccumulator = 1 - ((1 - dProbAccumulator) * (1 - (dUnitStakeProbability * nCoinWeight)));
                LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dProbAccumulator = %e", dProbAccumulator);
            }

        }
        nDeltaTime = nTime - nTimePrev;
        nThrows = nDeltaTime / (STAKE_TIMESTAMP_MASK + 1);
        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nThrows = %i", nThrows);
        dCumulativeProbability = 1 - ((1 - dCumulativeProbability) * pow((1 - dProbAccumulator), nThrows));
        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dCumulativeProbability = %e", dCumulativeProbability);

        if (dCumulativeProbability >= dConfidence) break;

        nTimePrev = nTime;
    }

    // If (dConfidence - dCumulativeProbability) > 0, it means we exited the negative Gantt chart area and the desired confidence level
    // has not been reached. All of the eligible UTXO's are contributing probability, and this is the final dProbAccumulator value.
    // If the loop above is degenerate (i.e. only the current time pass through), then dCumulativeProbability will be zero.
    // If it was not degenerate and the positive reqions in the Gantt chart area contributed some probability, then dCumulativeProbability will
    // be greater than zero. We must compute the amount of time beyond nTime that is required to bridge the gap between
    // dCumulativeProbability and dConfidence. If (dConfidence - dCumulativeProbability) <= 0 then we overshot during the Gantt chart area,
    // and we will back off by nThrows amount, which will now be negative.
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dProbAccumulator = %e", dProbAccumulator);

    // Shouldn't happen because if we are down here, we are staking, and there have to be eligible UTXO's, but just in case...
    if (dProbAccumulator == 0.0)
    {
        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: ERROR in dProbAccumulator calculations");
        return result;
    }

    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dConfidence = %f", dConfidence);
    // If nThrows is negative, this just means we overshot in the Gantt chart loop and have to backtrack by nThrows.
    nThrows = (int64_t)((log(1 - dConfidence) - log(1 - dCumulativeProbability)) / log(1 - dProbAccumulator));
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nThrows = %i", nThrows);

    nDeltaTime = nThrows * (STAKE_TIMESTAMP_MASK + 1);
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nDeltaTime = %i", nDeltaTime);

    // Because we are looking at the delta time required past nTime, which is where we exited the Gantt chart loop.
    result = nDeltaTime + nTime - nCurrentTime;
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: ETTS at %d confidence = %i", dConfidence, result);

    return result;
}


void GetGlobalStatus()
{
    //Populate overview

    try
    {
        uint64_t nWeight = 0;
        pwalletMain->GetStakeWeight(nWeight);
        double weight = nWeight/COIN;
        double PORDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
        std::string sWeight = RoundToString((double)weight,0);

        //9-6-2015 Add RSA fields to overview
        if ((double)weight > 100000000000000)
        {
            sWeight = sWeight.substr(0,13) + "E" + RoundToString((double)sWeight.length()-13,0);
        }

        std::string current_poll;

        try
        {
            LOCK(cs_main);
            current_poll = GetCurrentOverviewTabPoll();
        }
        catch (std::exception &e)
        {
            current_poll = _("No current polls");
            LogPrintf("Error obtaining last poll: %s", e.what());
        }

        LOCK(GlobalStatusStruct.lock);

        GlobalStatusStruct.blocks = ToString(nBestHeight);
        GlobalStatusStruct.difficulty = RoundToString(PORDiff,3);
        GlobalStatusStruct.netWeight = RoundToString(GetEstimatedNetworkWeight() / 80.0,2);
        //todo: use the real weight from miner status (requires scaling)
        GlobalStatusStruct.coinWeight = sWeight;
        GlobalStatusStruct.magnitude = NN::Quorum::MyMagnitude().ToString();
        GlobalStatusStruct.cpid = NN::GetPrimaryCpid();
        GlobalStatusStruct.poll = std::move(current_poll);

        GlobalStatusStruct.status = msMiningErrors;

        unsigned long stk_dropped;

        {
            LOCK(MinerStatus.lock);

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

            stk_dropped = MinerStatus.KernelsFound - MinerStatus.AcceptedCnt;
        }

        if (stk_dropped)
            GlobalStatusStruct.errors += "Rejected " + ToString(stk_dropped) + " stakes;";

        return;
    }
    catch (std::exception& e)
    {
        GlobalStatusStruct.errors = _("Error obtaining status.");

        LogPrintf("Error obtaining status");
        return;
    }
}

std::string GetCurrentOverviewTabPoll()
{
    AssertLockHeld(cs_main);

    // The global msPoll variable contains the poll most-recently published to
    // the network. If it hasn't expired, return the title of this poll:
    if (PollIsActive(msPoll)) {
        return ExtractXML(msPoll, "<MK>", "</MK>").substr(0, 80);
    }

    // Otherwise, find the most recent active poll from the AppCache:
    std::string selected_poll_title;
    int64_t published_at = 0;

    for (const auto& item : ReadCacheSection(Section::POLL)) {
        if (item.second.timestamp > published_at && PollIsActive(item.second.value)) {
            selected_poll_title = item.first;
            published_at = item.second.timestamp;
        }
    }

    // If we couldn't find a poll from the AppCache, no active polls exist:
    if (selected_poll_title.empty()) {
        return _("No current polls");
    }

    // The key of the AppCache entry contains the poll title. Take the first 80
    // characters for display in the GUI:
    return selected_poll_title.substr(0, 80);
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

    size_t nSize = GetSerializeSize(tx, SER_NETWORK, CTransaction::CURRENT_VERSION);

    if (nSize > 5000)
    {
        LogPrint(BCLog::LogFlags::MEMPOOL, "ignoring large orphan tx (size: %" PRIszu ", hash: %s)", nSize, hash.ToString().substr(0,10));
        return false;
    }

    mapOrphanTransactions[hash] = tx;
    for (auto const& txin : tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    LogPrint(BCLog::LogFlags::MEMPOOL, "stored orphan tx %s (mapsz %" PRIszu ")", hash.ToString().substr(0,10), mapOrphanTransactions.size());
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
    unsigned int sz = GetSerializeSize(tx, SER_NETWORK, CTransaction::CURRENT_VERSION);
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
    hashBlock = pblock->GetHash(true);

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

bool CTransaction::CheckContracts(const MapPrevTx& inputs) const
{
    if (nVersion <= 1) {
        return true;
    }

    // Although v2 transactions support multiple contracts, we just allow one
    // for now to mitigate spam:
    if (GetContracts().size() > 1) {
        return DoS(100, error("%s: only one contract allowed in tx", __func__));
    }

    if ((IsCoinBase() || IsCoinStake())) {
        return DoS(100, error("%s: contract in non-standard tx", __func__));
    }

    int64_t required_burn_fee = 0;

    for (const auto& contract : GetContracts()) {
        if (contract.m_version <= 1) {
            return DoS(100, error("%s: legacy contract", __func__));
        }

        if (!contract.Validate()) {
            return DoS(100, error("%s: malformed contract", __func__));
        }

        // Reject any transactions with administrative contracts sent from a
        // wallet that does not hold the master key:
        if (contract.RequiresMasterKey() && !HasMasterKeyInput(inputs)) {
            return DoS(100, error("%s: contract requires master key", __func__));
        }

        required_burn_fee += contract.RequiredBurnAmount();
    }

    int64_t supplied_burn_fee = 0;

    for (const auto& output : vout) {
        if (output.scriptPubKey[0] == OP_RETURN) {
            supplied_burn_fee += output.nValue;
        }
    }

    if (supplied_burn_fee < required_burn_fee) {
        return DoS(100, error(
            "%s: insufficient burn output. Required: %s, supplied: %s",
            __func__,
            FormatMoney(required_burn_fee),
            FormatMoney(supplied_burn_fee)));
    }

    return true;
}

bool CTransaction::HasMasterKeyInput(const MapPrevTx& inputs) const
{
    const CTxDestination master_address = CWallet::MasterAddress().Get();

    for (const auto& input : vin) {
        const CTxOut& prev_out = GetOutputFor(input, inputs);
        CTxDestination dest;

        if (!ExtractDestination(prev_out.scriptPubKey, dest)) {
            continue;
        }

        if (dest == master_address) {
            return true;
        }
    }

    return false;
}

int64_t CTransaction::GetBaseFee(enum GetMinFee_mode mode) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

    // For block version 11 onwards, which corresponds to CTransaction::CURRENT_VERSION 2,
    // a multiplier is used on top of MIN_TX_FEE and MIN_RELAY_TX_FEE
    if (nVersion >= 2)
    {
        nBaseFee *= 10;
    }

    return nBaseFee;
}

int64_t CTransaction::GetMinFee(unsigned int nBlockSize, enum GetMinFee_mode mode, unsigned int nBytes) const
{
    int64_t nBaseFee = GetBaseFee(mode);

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

    // Mandatory switch to binary contracts (tx version 2):
    if (IsV11Enabled(nBestHeight + 1) && tx.nVersion < 2) {
        // Disallow tx version 1 after the mandatory block to prohibit the
        // use of legacy string contracts:
        return tx.DoS(100, error("AcceptToMemoryPool : legacy transaction"));
    }

    // Reject version 2 transactions until mandatory threshold.
    //
    // CTransaction::CURRENT_VERSION is now 2, but we cannot send version 2
    // transactions with binary contracts until clients can handle them.
    //
    // TODO: remove this check in the next release after mandatory block.
    //
    if (!IsV11Enabled(nBestHeight + 1) && tx.nVersion > 1) {
        return tx.DoS(100, error("AcceptToMemoryPool : v2 transaction too early"));
    }

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

    // Verify beacon contract in tx if found
    for (const auto& contract : tx.GetContracts()) {
        if (contract.m_type == NN::ContractType::BEACON
            && !NN::GetBeaconRegistry().Validate(contract))
        {
            return tx.DoS(25, error("%s: bad beacon contract in tx %s", __func__, tx.GetHash().ToString()));
        }
    }

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
        if (nFees < tx.GetBaseFee(GMF_RELAY))
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

                LogPrint(BCLog::LogFlags::MEMPOOL, "Rate limit dFreeCount: %g => %g", dFreeCount, dFreeCount+nSize);
                dFreeCount += nSize;
            }
        }

        // Validate any contracts published in the transaction:
        if (!tx.GetContracts().empty() && !tx.CheckContracts(mapInputs)) {
            return false;
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!tx.ConnectInputs(txdb, mapInputs, mapUnused, CDiskTxPos(1,1,1), pindexBest, false, false))
        {
            if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) || true)
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
            LogPrint(BCLog::LogFlags::MEMPOOL, "AcceptToMemoryPool : replacing tx %s with new version", ptxOld->GetHash().ToString());
            pool.remove(*ptxOld);
        }
        pool.addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());

    LogPrint(BCLog::LogFlags::MEMPOOL, "AcceptToMemoryPool : accepted %s (poolsz %" PRIszu ")", hash.ToString(), pool.mapTx.size());

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
    if (hashBlock.IsNull() || nIndex == -1)
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
    BlockMap::iterator mi = mapBlockIndex.find(block.GetHash(true));
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
                hashBlock = block.GetHash(true);
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
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->nFile, pindex->nBlockPos, fReadTransactions))
        return false;
    if (GetHash(true) != pindex->GetBlockHash())
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

// miner's coin stake reward based on coin age spent (coin-days)
int64_t GetConstantBlockReward(const CBlockIndex* index)
{
    // The constant block reward is set to a default, voted on value, but this can
    // be overridden using an admin message. This allows us to change the reward
    // amount without having to release a mandatory with updated rules. In the case
    // there is a breach or leaked admin keys the rewards are clamped to twice that
    // of the default value.
    const int64_t MIN_CBR = 0;
    const int64_t MAX_CBR = DEFAULT_CBR * 2;

    int64_t reward = DEFAULT_CBR;
    AppCacheEntry oCBReward = ReadCache(Section::PROTOCOL, "blockreward1");

    //TODO: refactor the expire checking to subroutine
    //Note: time constant is same as GetBeaconPublicKey
    if( (index->nTime - oCBReward.timestamp) <= (60 * 24 * 30 * 6 * 60) )
    {
        reward = atoi64(oCBReward.value);
    }

    reward = std::max(reward, MIN_CBR);
    reward = std::min(reward, MAX_CBR);
    return reward;
}

int64_t GetProofOfStakeReward(
    const uint64_t nCoinAge,
    const int64_t nTime,
    const CBlockIndex* const pindexLast)
{
    if (pindexLast->nVersion >= 10) {
        return GetConstantBlockReward(pindexLast);
    }

    return nCoinAge * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8);
}



static const int64_t nTargetTimespan = 16 * 60;  // 16 mins

// ppcoin: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = pindex->pprev;
    return pindex;
}

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast)
{
    CBigNum bnTargetLimit = GetProofOfStakeLimit(pindexLast->nHeight);

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, true);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, true);
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

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > bnProofOfWorkLimit)
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > UintToArith256(bnTarget.getuint256()))
        return error("CheckProofOfWork() : hash doesn't match nBits");

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
        CTxDB().WriteBestInvalidTrust(CBigNum(ArithToUint256(nBestInvalidTrust)));
        uiInterface.NotifyBlocksChanged();
    }

    arith_uint256 nBestInvalidBlockTrust = pindexNew->nChainTrust - pindexNew->pprev->nChainTrust;
    arith_uint256 nBestBlockTrust = pindexBest->nHeight != 0
        ? (pindexBest->nChainTrust - pindexBest->pprev->nChainTrust)
        : pindexBest->nChainTrust;

    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s",
      pindexNew->GetBlockHash().ToString().substr(0,20),
      pindexNew->nHeight,
      CBigNum(ArithToUint256(pindexNew->nChainTrust)).ToString(),
      nBestInvalidBlockTrust.GetLow64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexNew->GetBlockTime()));
    LogPrintf("InvalidChainFound:  current best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s",
      hashBestChain.ToString().substr(0,20),
      nBestHeight,
      CBigNum(ArithToUint256(pindexBest->nChainTrust)).ToString(),
      nBestBlockTrust.GetLow64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()));
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
                LogPrint(BCLog::LogFlags::VERBOSE, "FetchInputs() : %s mempool Tx prev not found %s", GetHash().ToString().substr(0,10),  prevout.hash.ToString().substr(0,10));
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

                    if (fMiner) return false;
                    return LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) ? error("ConnectInputs() : %s prev tx already used at %s", GetHash().ToString().c_str(), txindex.vSpent[prevout.n].ToString().c_str()) : false;
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

        if (pindex->nIsContract == 1)
        {
            NN::RevertContracts(vtx[i].GetContracts());
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    // Brod: I do not like this...
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext.SetNull();
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

double ClientVersionNew()
{
    double cv = BlockVersion(FormatFullVersion());
    return cv;
}

//
// Gridcoin-specific ConnectBlock() routines:
//
namespace {
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

bool GetCoinstakeAge(CTxDB& txdb, const CBlock& block, uint64_t& out_coin_age)
{
    out_coin_age = 0;

    // ppcoin: coin stake tx earns reward instead of paying fee
    //
    // With block version 10, Gridcoin switched to constant block rewards
    // that do not depend on coin age, so we can avoid reading the blocks
    // and transactions from the disk. The CheckProofOfStake*() functions
    // of the kernel verify the transaction timestamp and that the staked
    // inputs exist in the main chain.
    //
    if (block.nVersion <= 9 && !block.vtx[1].GetCoinAge(txdb, out_coin_age)) {
        return error("ConnectBlock[] : %s unable to get coin age for coinstake",
            block.vtx[1].GetHash().ToString().substr(0,10));
    }

    return true;
}

//!
//! \brief Checks reward claims in generated blocks.
//!
class ClaimValidator
{
public:
    ClaimValidator(
        const CBlock& block,
        const CBlockIndex* const pindex,
        const int64_t total_claimed,
        const int64_t fees,
        const uint64_t coin_age)
        : m_block(block)
        , m_pindex(pindex)
        , m_claim(block.GetClaim())
        , m_total_claimed(total_claimed)
        , m_fees(fees)
        , m_coin_age(coin_age)
    {
    }

    bool Check() const
    {
        return m_claim.HasResearchReward()
            ? CheckResearcherClaim()
            : CheckInvestorClaim();
    }

private:
    const CBlock& m_block;
    const CBlockIndex* const m_pindex;
    const NN::Claim& m_claim;
    const int64_t m_total_claimed;
    const int64_t m_fees;
    const uint64_t m_coin_age;

    bool CheckReward(const int64_t research_owed, int64_t& out_stake_owed) const
    {
        out_stake_owed = GetProofOfStakeReward(m_coin_age, m_block.nTime, m_pindex);

        if (m_block.nVersion >= 11) {
            return m_total_claimed <= research_owed + out_stake_owed + m_fees;
        }

        // Blocks version 10 and below represented rewards as floating-point
        // values and needed to accomodate floating-point errors so we'll do
        // the same rounding on the floating-point representations:
        //
        double subsidy = ((double)research_owed / COIN) * 1.25;
        subsidy += (double)out_stake_owed / COIN;

        int64_t max_owed = roundint64(subsidy * COIN) + m_fees;

        // Block version 9 and below allowed a 1 GRC wiggle.
        if (m_block.nVersion <= 9) {
            max_owed += 1 * COIN;
        }

        return m_total_claimed <= max_owed;
    }

    bool CheckInvestorClaim() const
    {
        int64_t out_stake_owed;
        if (CheckReward(0, out_stake_owed)) {
            return true;
        }

        if (GetBadBlocks().count(m_pindex->GetBlockHash())) {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored bad investor claim on block %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        return m_block.DoS(10, error(
            "ConnectBlock[%s]: investor claim %s exceeds %s. Expected %s, fees %s",
            __func__,
            FormatMoney(m_total_claimed),
            FormatMoney(out_stake_owed + m_fees),
            FormatMoney(out_stake_owed),
            FormatMoney(m_fees)));
    }

    bool CheckResearcherClaim() const
    {
        // For version 11 blocks and higher, just validate the reward and check
        // the signature. No need for the rest of these shenanigans.
        //
        if (m_block.nVersion >= 11) {
            return CheckResearchReward() && CheckBeaconSignature();
        }

        if (!CheckResearchRewardLimit()) {
            return false;
        }

        if (!CheckResearchRewardDrift()) {
            return false;
        }

        if (m_block.nVersion <= 8) {
            return true;
        }

        if (!CheckClaimMagnitude()) {
            return false;
        }

        if (!CheckBeaconSignature()) {
            return false;
        }

        if (!CheckResearchReward()) {
            return false;
        }

        return true;
    }

    bool CheckResearchRewardLimit() const
    {
        // TODO: determine max reward from accrual computer implementation:
        const int64_t max_reward = 12750 * COIN;

        return m_claim.m_research_subsidy <= max_reward
            || m_block.DoS(1, error(
                "ConnectBlock[%s]: research claim %s exceeds max %s. CPID %s",
                __func__,
                FormatMoney(m_claim.m_research_subsidy),
                FormatMoney(max_reward),
                m_claim.m_mining_id.ToString()));
    }

    bool CheckResearchRewardDrift() const
    {
        // ResearchAge: Since the best block may increment before the RA is
        // connected but After the RA is computed, the ResearchSubsidy can
        // sometimes be slightly smaller than we calculate here due to the
        // RA timespan increasing.  So we will allow for time shift before
        // rejecting the block.
        const int64_t reward_claimed = m_total_claimed - m_fees;
        int64_t drift_allowed = m_claim.m_research_subsidy * 0.15;

        if (drift_allowed < 10 * COIN) {
            drift_allowed = 10 * COIN;
        }

        return m_claim.TotalSubsidy() + drift_allowed >= reward_claimed
            || m_block.DoS(20, error(
                "ConnectBlock[%s]: reward claim %s exceeds allowed %s. CPID %s",
                __func__,
                FormatMoney(reward_claimed),
                FormatMoney(m_claim.TotalSubsidy() + drift_allowed),
                m_claim.m_mining_id.ToString()));
    }

    bool CheckClaimMagnitude() const
    {
        // Magnitude as of the last superblock:
        const double mag = NN::Quorum::GetMagnitude(m_claim.m_mining_id).Floating();

        return m_claim.m_magnitude <= (mag * 1.25)
            || m_block.DoS(20, error(
                "ConnectBlock[%s]: magnitude claim %f exceeds superblock %f. CPID %s",
                __func__,
                m_claim.m_magnitude,
                mag,
                m_claim.m_mining_id.ToString()));
    }

    bool CheckBeaconSignature() const
    {
        const NN::CpidOption cpid = m_claim.m_mining_id.TryCpid();

        if (!cpid) {
            // Investor claims are not signed by a beacon key.
            return false;
        }

        const uint256 last_block_hash = m_pindex->pprev->GetBlockHash();

        // The legacy beacon functions determined beacon expiration by the time
        // of the previous block. For block version 11+, compute the expiration
        // threshold from the current block:
        //
        const int64_t now = m_block.nVersion >= 11 ? m_block.nTime : m_pindex->pprev->nTime;

        if (const NN::BeaconOption beacon = NN::GetBeaconRegistry().TryActive(*cpid, now)) {
            if (m_claim.VerifySignature(beacon->m_public_key, last_block_hash)) {
                return true;
            }
        }

        if (GetBadBlocks().count(m_pindex->GetBlockHash())) {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored invalid signature in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        // An old bug caused some nodes to sign research reward claims with a
        // previous beacon key (beaconalt). Mainnet declares block exceptions
        // for this problem. To avoid declaring exceptions for the 55 testnet
        // blocks, the following check ignores beaconalt verification failure
        // for the range of heights that include these blocks:
        //
        if (fTestNet
            && (m_pindex->nHeight >= 495352 && m_pindex->nHeight <= 600876))
        {
            LogPrintf(
                "WARNING: %s: likely testnet beaconalt signature ignored in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        return m_block.DoS(20, error(
            "ConnectBlock[%s]: signature verification failed. CPID %s, LBH %s",
            __func__,
            m_claim.m_mining_id.ToString(),
            m_pindex->pprev->GetBlockHash().ToString()));
    }

    bool CheckResearchReward() const
    {
        int64_t research_owed = 0;

        if (const NN::CpidOption cpid = m_claim.m_mining_id.TryCpid()) {
            research_owed = NN::Tally::GetComputer(*cpid, m_block.nTime, m_pindex)->Accrual();
        }

        int64_t out_stake_owed;
        if (CheckReward(research_owed, out_stake_owed)) {
            return true;
        }

        // Testnet contains some blocks with bad interest claims that were masked
        // by research age short 10-block-span pending accrual:
        if (fTestNet
            && m_block.nVersion <= 9
            && !CheckReward(0, out_stake_owed))
        {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored bad testnet claim in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        if (GetBadBlocks().count(m_pindex->GetBlockHash())) {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored bad research claim in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        return m_block.DoS(10, error(
            "ConnectBlock[%s]: researcher claim %s exceeds %s for CPID %s. "
            "Expected research %s, stake %s, fees %s. "
            "Claimed research %s, stake %s",
            __func__,
            FormatMoney(m_total_claimed),
            FormatMoney(research_owed + out_stake_owed + m_fees),
            m_claim.m_mining_id.ToString(),
            FormatMoney(research_owed),
            FormatMoney(out_stake_owed),
            FormatMoney(m_fees),
            FormatMoney(m_claim.m_research_subsidy),
            FormatMoney(m_claim.m_block_subsidy)));
    }
}; // ClaimValidator

bool TryLoadSuperblock(
    CBlock& block,
    const CBlockIndex* const pindex,
    const NN::Claim& claim)
{
    // Note: PullSuperblock() invalidates the m_claim.m_superblock field
    // by moving it. This must be the last instance where we reference a
    // superblock in a block's claim field:
    //
    NN::SuperblockPtr superblock = NN::SuperblockPtr::BindShared(block.PullSuperblock(), pindex);

    // TODO: find the invalid historical superblocks so we can remove
    // the fColdBoot condition that skips this check when syncing the
    // initial chain:
    //
    if ((!fColdBoot || block.nVersion >= 11)
        && !NN::Quorum::ValidateSuperblockClaim(claim, superblock, pindex))
    {
        return block.DoS(25, error("ConnectBlock: Rejected invalid superblock."));
    }

    // Block versions 11+ calculate research rewards from snapshots of
    // accrual taken at each superblock:
    //
    if (block.nVersion >= 11) {
        if (!NN::Tally::ApplySuperblock(superblock)) {
            return false;
        }

        NN::GetBeaconRegistry().ActivatePending(
            superblock->m_verified_beacons.m_verified,
            superblock.m_timestamp);
    }

    NN::Quorum::PushSuperblock(std::move(superblock));

    return true;
}

bool GridcoinConnectBlock(
    CBlock& block,
    CBlockIndex* const pindex,
    CTxDB& txdb,
    const int64_t total_claimed,
    const int64_t fees)
{
    const NN::Claim& claim = block.GetClaim();

    if (pindex->nHeight > nGrandfather) {
        uint64_t out_coin_age;
        if (!GetCoinstakeAge(txdb, block, out_coin_age)) {
            return false;
        }

        if (!ClaimValidator(block, pindex, total_claimed, fees, out_coin_age).Check()) {
            return false;
        }

        if (claim.ContainsSuperblock()) {
            if (!TryLoadSuperblock(block, pindex, claim)) {
                return false;
            }

            pindex->nIsSuperBlock = 1;
        } else if (block.nVersion <= 10) {
            // Block versions 11+ validate superblocks from scraper convergence
            // instead of the legacy quorum system so we only record votes from
            // version 10 blocks and below:
            //
            NN::Quorum::RecordVote(claim.m_quorum_hash, claim.m_quorum_address, pindex);
        }
    }

    for (auto iter = ++++block.vtx.begin(), end = block.vtx.end(); iter != end; ++iter) {
        if (!iter->GetContracts().empty()) {
            pindex->nIsContract = 1;
            NN::ApplyContracts(iter->PullContracts());
        }
    }

    pindex->SetMiningId(claim.m_mining_id);
    pindex->nResearchSubsidy = claim.m_research_subsidy;
    pindex->nInterestSubsidy = claim.m_block_subsidy;

    if (block.nVersion >= 11) {
        pindex->nMagnitude = NN::Quorum::GetMagnitude(claim.m_mining_id).Floating();
    } else {
        pindex->nMagnitude = claim.m_magnitude;
    }

    NN::Tally::RecordRewardBlock(pindex);

    return true;
}
} // Anonymous namespace

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock("ConnectBlock",pindex->nHeight, 395*COIN, !fJustCheck, !fJustCheck, false,false))
    {
        LogPrintf("ConnectBlock::Failed - ");
        return false;
    }

    unsigned int nTxPos;
    if (fJustCheck) {
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    } else {
        nTxPos = pindex->nBlockPos
            + ::GetSerializeSize<CBlockHeader>(*this, SER_DISK, CLIENT_VERSION)
            + GetSizeOfCompactSize(vtx.size());
    }

    map<uint256, CTxIndex> mapQueuedChanges;
    int64_t nFees = 0;
    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    int64_t nStakeReward = 0;
    unsigned int nSigOps = 0;

    bool bIsDPOR = false;

    if (nVersion >= 8 && pindex->nStakeModifier == 0 && pindex->nStakeModifierChecksum == 0)
    {
        uint256 tmp_hashProof;
        if (!CheckProofOfStakeV8(txdb, pindex->pprev, *this, /*generated_by_me*/ false, tmp_hashProof))
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
                if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
                {
                    int64_t nTotalCoinstake = 0;
                    for (unsigned int i = 0; i < tx.vout.size(); i++)
                    {
                        nTotalCoinstake += tx.vout[i].nValue;
                    }
                    LogPrint(BCLog::LogFlags::NOISY, " nHeight %d; nTCS %f; nTxValueOut %f",
                                              pindex->nHeight,CoinToDouble(nTotalCoinstake),CoinToDouble(nTxValueOut));
                }

                if (pindex->nVersion >= 10)
                {
                    if (tx.vout.size() > 8)
                        return DoS(100,error("Too many coinstake outputs"));
                }
                else if (bIsDPOR && pindex->nHeight > nGrandfather && pindex->nVersion < 10)
                {
                    // Old rules, does not make sense
                    // Verify no recipients exist after coinstake (Recipients start at output position 3 (0=Coinstake flag, 1=coinstake amount, 2=splitstake amount)
                    for (unsigned int i = 3; i < tx.vout.size(); i++)
                    {
                        double      Amount    = CoinToDouble(tx.vout[i].nValue);
                        if (Amount > 0)
                        {
                            return DoS(50,error("Coinstake output %u forbidden", i));
                        }
                    }
                }
            }

            // Validate any contracts published in the transaction:
            if (!tx.GetContracts().empty() && !tx.CheckContracts(mapInputs)) {
                return false;
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false))
                return false;
        }

        mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());
    }

    if (IsResearchAgeEnabled(pindex->nHeight)
        && !GridcoinConnectBlock(*this, pindex, txdb, nStakeReward, nFees))
    {
        return false;
    }

    pindex->nMint = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = ReturnCurrentMoneySupply(pindex) + nValueOut - nValueIn;

    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("Connect() : WriteBlockIndex for pindex failed");

    if (!OutOfSyncByAge())
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

    AskForOutstandingBlocks(uint256());
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

        LogPrint(BCLog::LogFlags::VERBOSE, "DisconnectBlocksBatch: %s",pindexBest->GetBlockHash().GetHex());

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

        if(pindexBest->IsUserCPID()) {
            // The user has no longer staked this block.
            NN::Tally::ForgetRewardBlock(pindexBest);
        }

        if (pindexBest->nIsSuperBlock == 1) {
            NN::Quorum::PopSuperblock(pindexBest);

            if (pindexBest->nVersion >= 11 && !NN::Tally::RevertSuperblock()) {
                return false;
            }
        }

        if (pindexBest->nHeight > nGrandfather && pindexBest->nVersion <= 10) {
            NN::Quorum::ForgetVote(pindexBest);
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
        // Resurrect memory transactions that were in the disconnected branch
        for( CTransaction& tx : vResurrect)
            AcceptToMemoryPool(mempool, tx, NULL);

        if (!txdb.TxnCommit())
            return error("DisconnectBlocksBatch: TxnCommit failed"); /*fatal*/

        NN::ReplayContracts(pindexBest);
        NN::Quorum::LoadSuperblockIndex(pindexBest);

        // Tally research averages.
        if(IsV9Enabled_Tally(nBestHeight) && !IsV11Enabled(nBestHeight)) {
            assert(NN::Tally::IsLegacyTrigger(nBestHeight));
            NN::Tally::LegacyRecount(pindexBest);
        }
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
    assert(pindexNew->GetBlockHash()==blockNew.GetHash(true));
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

        // Blocks version 11+ do not use the legacy tally system triggered by
        // block height intervals:
        //
        if (!IsV11Enabled(pcommon->nHeight) && pcommon != pindexBest)
        {
            pcommon = NN::Tally::FindLegacyTrigger(pcommon);
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

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && cnt_dis > 0) LogPrintf("ReorganizeChain: disconnected %d blocks",cnt_dis);

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
            assert(pindex->GetBlockHash()==block.GetHash(true));
        }
        else
        {
            assert(pindex==pindexNew);
            assert(pindexNew->GetBlockHash()==block.GetHash(true));
            assert(pindexNew->GetBlockHash()==blockNew.GetHash(true));
        }

        uint256 hash = block.GetHash(true);
        arith_uint256 nBestBlockTrust;

        LogPrint(BCLog::LogFlags::VERBOSE, "ReorganizeChain: connect %s",hash.ToString());

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
            assert(pindex->GetBlockHash()==block.GetHash(true));
            assert(pindex->pprev == pindexBest);
            if (!block.ConnectBlock(txdb, pindex, false))
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

        if (IsV9Enabled_Tally(nBestHeight)
            && !IsV11Enabled(nBestHeight)
            && NN::Tally::IsLegacyTrigger(nBestHeight))
        {
            NN::Tally::LegacyRecount(pindexBest);
        }
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && (cnt_dis > 0 || cnt_con > 1))
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

    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload)
    {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE))
    {
        LogPrintf("{SBC} {%s %d}  trust=%s  date=%s",
               hashBestChain.ToString(), nBestHeight,
               CBigNum(ArithToUint256(nBestChainTrust)).ToString(),
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

    return GridcoinServices();
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
        CBlockHeader header;
        CTransaction txPrev;

        if (!ReadStakedInput(txdb, txin.prevout.hash, header, txPrev))
        {
            return false;
        }

        if (nTime < txPrev.nTime)
        {
            return false; // Transaction timestamp violation
        }

        if (header.GetBlockTime() + nStakeMinAge > nTime)
        {
            continue; // only count coins meeting min age requirement
        }

        int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += CBigNum(nValueIn) * (nTime-txPrev.nTime) / CENT;

        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && GetBoolArg("-printcoinage"))
            LogPrintf("coin age nValueIn=%" PRId64 " nTimeDiff=%d bnCentSecond=%s", nValueIn, nTime - txPrev.nTime, bnCentSecond.ToString());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && GetBoolArg("-printcoinage"))
        LogPrintf("coin age bnCoinDay=%s", bnCoinDay.ToString());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}

bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos, const uint256& hashProof)
{
    // Check for duplicate
    uint256 hash = GetHash(true);
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
    // Allow the genesis block to pass.
    if(hashPrevBlock.IsNull() &&
       GetHash(true) == (fTestNet ? hashGenesisBlockTestNet : hashGenesisBlock))
        return true;

    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty()
        || vtx.size() > MAX_BLOCK_SIZE
        || ::GetSerializeSize(*this, (SER_NETWORK & SER_SKIPSUPERBLOCK), PROTOCOL_VERSION) > MAX_BLOCK_SIZE
        || ::GetSerializeSize(GetSuperblock(), SER_NETWORK, PROTOCOL_VERSION) > NN::Superblock::MAX_SIZE)
    {
        return DoS(100, error("CheckBlock[] : size limits failed"));
    }

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(GetHash(true), nBits))
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
    const NN::Claim& claim = GetClaim();

    // Version 11+ blocks store the claim context in the block itself instead
    // of the hashBoinc field of the first transaction. The hash of the claim
    // is placed in the coinbase transaction instead to verify its integrity:
    //
    if (nVersion >= 11) {
        if (claim.m_version <= 1) {
            return DoS(100, error("%s: legacy claim", __func__));
        }

        if (!claim.WellFormed()) {
            return DoS(100, error("%s: malformed claim", __func__));
        }

        if (claim.GetHash() != uint256S(vtx[0].hashBoinc)) {
            return DoS(100, error("%s: claim hash mismatch", __func__));
        }
    }

    if(nVersion<9)
    {
        //For higher security, plus lets catch these bad blocks before adding them to the chain to prevent reorgs:
        //Orphan Flood Attack
        if (height1 > nGrandfather)
        {
            double blockVersion = BlockVersion(claim.m_client_version);
            double cvn = ClientVersionNew();
            LogPrint(BCLog::LogFlags::NOISY, "BV %f, CV %f   ",blockVersion,cvn);
            // Enforce Beacon Age
            if (blockVersion < 3588 && height1 > 860500 && !fTestNet)
                return error("CheckBlock[]:  Old client spamming new blocks after mandatory upgrade ");
        }
    }

    if (!fLoadingIndex && claim.HasResearchReward() && height1 > nGrandfather && BlockNeedsChecked(nTime))
    {
        // Full "v3" signature check is performed in ConnectBlock
        if (claim.m_signature.size() < 16)
        {
            return DoS(20, error(
                "Bad CPID or Block Signature : height %i, CPID %s, Bad Hashboinc [%s]",
                 height1,
                 claim.m_mining_id.ToString(),
                 vtx[0].hashBoinc));
        }
    }

    // Gridcoin: check proof-of-stake block signature
    if (IsProofOfStake() && height1 > nGrandfather)
    {
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
        // Mandatory switch to binary contracts (tx version 2):
        if (IsV11Enabled(height1) && tx.nVersion < 2) {
            // Disallow tx version 1 after the mandatory block to prohibit the
            // use of legacy string contracts:
            return tx.DoS(100, error("CheckBlock[] : legacy transaction"));
        }

        // Reject version 2 transactions until mandatory threshold.
        //
        // CTransaction::CURRENT_VERSION is now 2, but we cannot send version 2
        // transactions with binary contracts until clients can handle them.
        //
        // TODO: remove this check in the next release after mandatory block.
        //
        if (!IsV11Enabled(height1) && tx.nVersion > 1) {
            return tx.DoS(100, error("CheckBlock[] : v2 transaction too early"));
        }

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

    return true;
}

bool CBlock::AcceptBlock(bool generated_by_me)
{
    AssertLockHeld(cs_main);

    if (nVersion > CURRENT_VERSION)
        return DoS(100, error("AcceptBlock() : reject unknown block version %d", nVersion));

    // Check for duplicate
    uint256 hash = GetHash(true);
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
              || (IsV10Enabled(nHeight) && nVersion < 10)
              || (IsV11Enabled(nHeight) && nVersion < 11)
              )
        return DoS(20, error("AcceptBlock() : reject too old nVersion = %d", nVersion));
    else if( (!IsProtocolV2(nHeight) && nVersion >= 7)
             ||(!IsV8Enabled(nHeight) && nVersion >= 8)
             ||(!IsV9Enabled(nHeight) && nVersion >= 9)
             ||(!IsV10Enabled(nHeight) && nVersion >= 10)
             ||(!IsV11Enabled(nHeight) && nVersion >= 11)
             )
        return DoS(100, error("AcceptBlock() : reject too new nVersion = %d", nVersion));

    if (IsProofOfWork() && nHeight > LAST_POW_BLOCK)
        return DoS(100, error("AcceptBlock() : reject proof-of-work at height %d", nHeight));

    if (nHeight > nGrandfather)
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
        if (nBits != GetNextTargetRequired(pindexPrev))
            return DoS(100, error("AcceptBlock() : incorrect %s", IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));
    }

    for (auto const& tx : vtx)
    {
        // Check that all transactions are finalized
        if (!IsFinalTx(tx, nHeight, GetBlockTime()))
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));

        // Verify beacon contract if a transaction contains a beacon contract
        // Current bad contracts in chain would cause a fork on sync, skip them
        if (nVersion >= 9) {
            for (const auto& contract : tx.GetContracts()) {
                if (contract.m_type == NN::ContractType::BEACON
                    && !NN::GetBeaconRegistry().Validate(contract))
                {
                    return tx.DoS(25, error("%s: bad beacon contract in tx %s", __func__, tx.GetHash().ToString()));
                }
            }
        }
    }

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

    uint256 hashProof;

    if (nVersion >= 8)
    {
        //must be proof of stake
        //no grandfather exceptions
        //if (IsProofOfStake())
        CTxDB txdb("r");
        if(!CheckProofOfStakeV8(txdb, pindexPrev, *this, generated_by_me, hashProof))
        {
            error("WARNING: AcceptBlock(): check proof-of-stake failed for block %s, nonce %f    ", hash.ToString().c_str(),(double)nNonce);
            LogPrintf(" prev %s",pindexPrev->GetBlockHash().ToString());
            return false;
        }
    }
    else if (nVersion == 7 && (nHeight >= 999000 || nHeight > nGrandfather))
    {
        // Calculate a proof hash for these version 7 blocks for the block index
        // so we can carry the stake modifier into version 8+:
        //
        // mainnet: block 999000 to version 8 (1010000)
        // testnet: nGrandfather (196551) to version 8 (311999)
        //
        CTxDB txdb("r");
        if (!CalculateLegacyV3HashProof(txdb, *this, nNonce, hashProof)) {
            return error("AcceptBlock(): Failed to carry v7 proof hash.");
        }
    }

    // PoW is checked in CheckBlock[]
    if (IsProofOfWork())
    {
        hashProof = GetHash(true);
    }

    //Grandfather
    if (nHeight > nGrandfather)
    {
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

    nLastAskedForBlocks=GetAdjustedTime();
    ResetTimerMain("OrphanBarrage");
    return true;
}


arith_uint256 CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    if (bnTarget <= 0) return 0;
    int64_t block_mag = 0;
    uint256 chaintrust = (((CBigNum(1)<<256) / (bnTarget+1)) - (block_mag)).getuint256();
    return UintToArith256(chaintrust);
}

bool GridcoinServices()
{
    //Dont do this on headless - SeP
    if (fQtActive && (nBestHeight % 125) == 0 && nBestHeight > 0)
    {
        GetGlobalStatus();
        bForceUpdate=true;
        uiInterface.NotifyBlocksChanged();
    }

    // Block version 9 tally transition:
    //
    // This block controls the switch to a new tallying system introduced with
    // block version 9. Mainnet and testnet activate at different heights seen
    // below.
    //
    if (!IsV9Enabled_Tally(nBestHeight)
        && IsV9Enabled(nBestHeight + (fTestNet ? 200 : 40))
        && nBestHeight % 20 == 0)
    {
        LogPrint(BCLog::LogFlags::TALLY,
            "GridcoinServices: Priming tally system for v9 threshold.");

        NN::Tally::LegacyRecount(pindexBest);
    }

    // Block version 11 tally transition:
    //
    // Before the first version 11 block arrives, activate the snapshot accrual
    // system by creating a baseline of the research rewards owed in historical
    // superblocks so that we can validate the reward for the next block.
    //
    if (nBestHeight + 1 == GetV11Threshold()) {
        LogPrint(BCLog::LogFlags::TALLY,
            "GridcoinServices: Priming tally system for v11 threshold.");

        if (!NN::Tally::ActivateSnapshotAccrual(pindexBest)) {
            return error("GridcoinServices: Failed to prepare tally for v11.");
        }
    }

    //Dont perform the following functions if out of sync
    if (OutOfSyncByAge()) {
        return true;
    }

    //Backup the wallet once per 900 blocks or as specified in config:
    int nWBI = GetArg("-walletbackupinterval", 900);
    if (nWBI && TimerMain("backupwallet", nWBI))
    {
        bool bWalletBackupResults = BackupWallet(*pwalletMain, GetBackupFilename("wallet.dat"));
        bool bConfigBackupResults = BackupConfigFile(GetBackupFilename("gridcoinresearch.conf"));
        LogPrintf("Daily backup results: Wallet -> %s Config -> %s", (bWalletBackupResults ? "true" : "false"), (bConfigBackupResults ? "true" : "false"));
    }

    // Attempt to advertise or renew a beacon automatically if the wallet is
    // unlocked and funded.
    //
    if (TimerMain("send_beacon", 180)) {
        const NN::ResearcherPtr researcher = NN::Researcher::Get();

        // Do not perform an automated renewal for participants with existing
        // beacons before a superblock is due. This avoids overwriting beacon
        // timestamps in the beacon registry in a way that causes the renewed
        // beacon to appear ahead of the scraper beacon consensus window.
        //
        if (!researcher->Eligible() || !NN::Quorum::SuperblockNeeded()) {
            researcher->AdvertiseBeacon();
        }
    }

    return true;
}

bool AskForOutstandingBlocks(uint256 hashStart)
{
    if (IsLockTimeWithinMinutes(nLastAskedForBlocks, 2, GetAdjustedTime())) return true;
    nLastAskedForBlocks = GetAdjustedTime();

    int iAsked = 0;
    LOCK(cs_vNodes);
    for (auto const& pNode : vNodes)
    {
                if (!pNode->fClient && !pNode->fOneShot && (pNode->nStartingHeight > (nBestHeight - 144)) && (pNode->nVersion < NOBLKS_VERSION_START || pNode->nVersion >= NOBLKS_VERSION_END) )
                {
                        if (hashStart==uint256())
                        {
                            pNode->PushGetBlocks(pindexBest, uint256(), true);
                        }
                        else
                        {
                            CBlockIndex* pblockindex = mapBlockIndex[hashStart];
                            if (pblockindex)
                            {
                                pNode->PushGetBlocks(pblockindex, uint256(), true);
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
    uint256 hash = pblock->GetHash(true);
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
    if (!pblock->CheckBlock("ProcessBlock", pindexBest->nHeight + 1, 100*COIN))
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
                AskForOutstandingBlocks(uint256());
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
        return key.Verify(GetHash(true), vchBlockSig);
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
    FILE* file = fsbridge::fopen(BlockFilePath(nFile), pszMode);
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
        //1-24-2016
        MAX_OUTBOUND_CONNECTIONS = (int)GetArg("-maxoutboundconnections", 8);
    }

    LogPrintf("Mode=%s", fTestNet ? "TestNet" : "Prod");

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
        txNew.nVersion = 1;
        txNew.nTime = 1413033777;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 0 << CBigNum(42) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock.SetNull();
        block.hashMerkleRoot = block.BuildMerkleTree();
        block.nVersion = 1;
        //R&D - Testers Wanted Thread:
        block.nTime    = !fTestNet ? 1413033777 : 1406674534;
        //Official Launch time:
        block.nBits    = bnProofOfWorkLimit.GetCompact();
        block.nNonce = !fTestNet ? 130208 : 22436;
        LogPrintf("starting Genesis Check...");
        // If genesis block hash does not match, then generate new genesis hash.
        if (block.GetHash(true) != (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
        {
            LogPrintf("Searching for genesis block...");
            // This will figure out a valid hash and Nonce if you're
            // creating a different genesis block: 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000xFFF
            arith_uint256 hashTarget = arith_uint256().SetCompact(block.nBits);
            arith_uint256 thash;
            while (true)
            {
                thash = UintToArith256(block.GetHash(true));
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
            LogPrintf("block.GetHash = %s", block.GetHash(true).ToString());
        }


        block.print();

        //// debug print

        //GENESIS3: Official Merkle Root
        uint256 merkle_root = uint256S("0x5109d5782a26e6a5a5eb76c7867f3e8ddae2bff026632c36afec5dc32ed8ce9f");
        assert(block.hashMerkleRoot == merkle_root);
        assert(block.GetHash(true) == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
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

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end)
{
    string::size_type loc = XMLdata.find(key, 0);

    if (loc == string::npos) {
        return "";
    }

    string::size_type loc_end = XMLdata.find(key_end, loc + 3);

    if (loc_end == string::npos) {
        return "";
    }

    return XMLdata.substr(loc + (key.length()), loc_end - loc - (key.length()));
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

bool GetEarliestStakeTime(std::string grcaddress, std::string cpid)
{
    if (nBestHeight < 15)
    {
        // Write entries in the cache to get a timestamp.
        WriteCache(Section::GLOBAL, "nGRCTime", "", GetAdjustedTime());
        WriteCache(Section::GLOBAL, "nCPIDTime", "", GetAdjustedTime());
        return true;
    }

    int64_t nGRCTime = ReadCache(Section::GLOBAL, "nGRCTime").timestamp;
    int64_t nCPIDTime = ReadCache(Section::GLOBAL, "nCPIDTime").timestamp;
    if (IsLockTimeWithinMinutes(nLastGRCtallied, 100, GetAdjustedTime()) &&
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
                            myCPID = block.GetClaim().m_mining_id.ToString();
                        }
                        else
                        {
                            myCPID = pblockindex->GetMiningId().ToString();
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
    WriteCache(Section::GLOBAL, "nGRCTime", "", nGRCTime);
    WriteCache(Section::GLOBAL, "nCPIDTime", "", nCPIDTime);
    return true;
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
            block.GetHash(true).ToString().c_str(),
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
            while (nPos != (unsigned int)-1 && !fRequestShutdown)
            {
                unsigned char pchData[65536];
                do {
                    fseek(blkdat.Get(), nPos, SEEK_SET);
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat.Get());
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
                fseek(blkdat.Get(), nPos, SEEK_SET);
                unsigned int nSize;
                blkdat >> nSize;
                if (nSize > 0 && nSize <= MAX_BLOCK_SIZE)
                {
                    CBlock block;
                    blkdat >> block;
                    if (ProcessBlock(NULL,&block,false))
                    {
                        nLoaded++;
                        LogPrintf("Blocks/s: %f", nLoaded / ((GetTimeMillis() - nStart) / 1000.0));
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
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
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

bool SecurityTest(CNode* pfrom, bool acid_test)
{
    if (pfrom->nStartingHeight > (nBestHeight*.5) && acid_test) return true;
    return false;
}

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv, int64_t nTimeReceived)
{
    RandAddSeedPerfmon();

    LogPrint(BCLog::LogFlags::NOISY, "received: %s from %s (%" PRIszu " bytes)", strCommand, pfrom->addrName, vRecv.size());

    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        LogPrintf("dropmessagestest DROPPING RECV MESSAGE");
        return true;
    }

    // Stay in Sync - 8-9-2016
    if (!IsLockTimeWithinMinutes(nBootup, 15, GetAdjustedTime()))
    {
        if ((!IsLockTimeWithinMinutes(nLastAskedForBlocks, 5, GetAdjustedTime()) && WalletOutOfSync()) || (WalletOutOfSync() && fTestNet))
        {
            LogPrint(BCLog::LogFlags::VERBOSE, "Bootup");
            AskForOutstandingBlocks(uint256());
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
        std::string legacy_dummy;

        vRecv >> pfrom->nVersion
            >> legacy_dummy      // pfrom->boinchashnonce
            >> legacy_dummy      // pfrom->boinchashpw
            >> legacy_dummy      // pfrom->cpid
            >> legacy_dummy      // pfrom->enccpid
            >> legacy_dummy      // acid
            >> pfrom->nServices
            >> nTime
            >> addrMe;

        LogPrint(BCLog::LogFlags::NOISY, "received aries version %i ...", pfrom->nVersion);

        int64_t timedrift = std::abs(GetAdjustedTime() - nTime);

            if (timedrift > (8*60))
            {
            LogPrint(BCLog::LogFlags::NOISY, "Disconnecting unauthorized peer with Network Time so far off by %" PRId64 " seconds!", timedrift);
            pfrom->Misbehaving(100);
            pfrom->fDisconnect = true;
            return false;
        }


        // Ensure testnet users are running latest version as of 12-3-2015 (works in conjunction with block spamming)
        if (pfrom->nVersion < 180321 && fTestNet)
        {
            // disconnect from peers older than this proto version
            LogPrint(BCLog::LogFlags::NOISY, "Testnet partner %s using obsolete version %i; disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION)
        {
            // disconnect from peers older than this proto version
            LogPrint(BCLog::LogFlags::NOISY, "partner %s using obsolete version %i; disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion < 180323 && !fTestNet && pindexBest->nHeight > 860500)
        {
            // disconnect from peers older than this proto version - Enforce Beacon Age - 3-26-2017
            LogPrint(BCLog::LogFlags::NOISY, "partner %s using obsolete version %i (before enforcing beacon age); disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (!fTestNet && pfrom->nVersion < 180314)
        {
            // disconnect from peers older than this proto version
            LogPrint(BCLog::LogFlags::NOISY, "ResearchAge: partner %s using obsolete version %i; disconnecting", pfrom->addr.ToString(), pfrom->nVersion);
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

        if (!vRecv.empty())         vRecv >> legacy_dummy; // pfrom->sGRCAddress;


        // Allow newbies to connect easily with 0 blocks
        if (GetArgument("autoban","true") == "true")
        {

                // Note: Hacking attempts start in this area

                if (pfrom->nStartingHeight < 1 && pfrom->nServices == 0 )
                {
                    pfrom->Misbehaving(100);
                    LogPrint(BCLog::LogFlags::NET, "Disconnecting possible hacker node with no services.  Banned for 24 hours.");
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
            LogPrint(BCLog::LogFlags::NET, "connected to self at %s, disconnecting", pfrom->addr.ToString());
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
        pfrom->nTimeOffset = nOffsetSample;
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
            pfrom->PushGetBlocks(pindexBest, uint256(), true);
            LogPrint(BCLog::LogFlags::NET, "Asked For blocks.");
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            for (auto const& item : mapAlerts)
                item.second.RelayTo(pfrom);
        }

        /* Notify the peer about statsscraper blobs we have */
        LOCK(CScraperManifest::cs_mapManifest);

        CScraperManifest::PushInvTo(pfrom);

        pfrom->fSuccessfullyConnected = true;

        LogPrint(BCLog::LogFlags::NOISY, "receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s", pfrom->nVersion,
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
                    static arith_uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = UintToArith256(GetRandHash());
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = ArithToUint256(hashSalt ^ (hashAddr<<32) ^ (( GetAdjustedTime() +hashAddr)/(24*60*60)));
                    hashRand = Hash(hashRand.begin(), hashRand.end());
                    multimap<uint256, CNode*> mapMix;
                    for (auto const& pnode : vNodes)
                    {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = ArithToUint256(UintToArith256(hashRand) ^ nPointer);
                        hashKey = Hash(hashKey.begin(), hashKey.end());
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

            /* Check also the scraper data propagation system to see if it needs
             * this inventory object */
            {
                LOCK(CScraperManifest::cs_mapManifest);

                fAlreadyHave = fAlreadyHave && CScraperManifest::AlreadyHave(pfrom, inv);
            }

            LogPrint(BCLog::LogFlags::NOISY, " got inventory: %s  %s", inv.ToString(), fAlreadyHave ? "have" : "new");

            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]), true);
            } else if (nInv == nLastBlock) {
                // In case we are on a very long side-chain, it is possible that we already have
                // the last block in an inv bundle sent in response to getblocks. Try to detect
                // this situation and push another getblocks to continue.
                pfrom->PushGetBlocks(mapBlockIndex[inv.hash], uint256(), true);
                LogPrint(BCLog::LogFlags::NOISY, "force getblock request: %s", inv.ToString());
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

        if (vInv.size() != 1)
        {
            LogPrint(BCLog::LogFlags::NET, "received getdata (%" PRIszu " invsz)", vInv.size());
        }

        LOCK(cs_main);
        for (auto const& inv : vInv)
        {
            if (fShutdown)
                return true;
            if (vInv.size() == 1)
            {
              LogPrint(BCLog::LogFlags::NET, "received getdata for: %s", inv.ToString());
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
                        pfrom->hashContinue.SetNull();
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
                else if(!pushed && inv.type == MSG_PART) {
                    LOCK(CSplitBlob::cs_mapParts);

                    CSplitBlob::SendPartTo(pfrom, inv.hash);
                }
                else if(!pushed &&  inv.type == MSG_SCRAPERINDEX)
                {
                    LOCK(CScraperManifest::cs_mapManifest);

                    // Do not send manifests while out of sync.
                    if (!OutOfSyncByAge())
                    {
                        // Do not send unauthorized manifests. This check needs to be done here, because in the
                        // case of a scraper deauthorization, a request from another node to forward the manifest
                        // may come before the housekeeping loop has a chance to do the periodic culling. This could
                        // result in unnecessary node banscore. This will suppress "this" node from sending any
                        // unauthorized manifests.

                        auto iter = CScraperManifest::mapManifest.find(inv.hash);
                        if (iter != CScraperManifest::mapManifest.end())
                        {
                            CScraperManifest& manifest = *iter->second;

                            // We are not going to do anything with the banscore here, because this is the sending node,
                            // but it is an out parameter of IsManifestAuthorized.
                            unsigned int banscore_out = 0;

                            // Also don't send a manifest that is not current.
                            if (CScraperManifest::IsManifestAuthorized(manifest.nTime, manifest.pubkey, banscore_out) && manifest.IsManifestCurrent())
                            {
                                CScraperManifest::SendManifestTo(pfrom, inv.hash);
                            }
                        }
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

        LogPrint(BCLog::LogFlags::NET, "getblocks %d to %s limit %d", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20), nLimit);
        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                LogPrint(BCLog::LogFlags::NET, "getblocks stopping at %d %s", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20));
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
                LogPrint(BCLog::LogFlags::NET, "getblocks stopping at limit %d %s", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20));
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

        vector<CBlockHeader> vHeaders;
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
        uint256 hashBlock = block.GetHash(true);

        LogPrintf(" Received block %s; ", hashBlock.ToString());
        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY)) block.print();

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
                        pfrom->nMinPingUsecTime = std::min(pfrom->nMinPingUsecTime.load(), pingUsecTime);
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

    else if (strCommand == "scraperindex")
    {
        LOCK(CScraperManifest::cs_mapManifest);

        CScraperManifest::RecvManifest(pfrom,vRecv);
    }
    else if (strCommand == "part")
    {
        LOCK(CSplitBlob::cs_mapParts);

        CSplitBlob::RecvPart(pfrom,vRecv);
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

        LogPrint(BCLog::LogFlags::NOISY, "ProcessMessages(message %u msgsz, %zu bytes, complete:%s)",
                 msg.hdr.nMessageSize, msg.vRecv.size(),
                 msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, pchMessageStart, sizeof(pchMessageStart)) != 0) {
            LogPrint(BCLog::LogFlags::NOISY, "PROCESSMESSAGE: INVALID MESSAGESTART");
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
        // TODO: hardcoded checksum size;
        //  will no longer be used once we adopt CNetMessage from Bitcoin
        uint8_t nChecksum[CMessageHeader::CHECKSUM_SIZE];
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (!std::equal(std::begin(nChecksum), std::end(nChecksum), std::begin(hdr.pchChecksum)))
        {
            LogPrintf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x",
               strCommand, nMessageSize, nChecksum, hdr.pchChecksum);
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
           LogPrint(BCLog::LogFlags::NOISY, "ProcessMessage(%s, %u bytes) FAILED", strCommand, nMessageSize);
        }
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}

// Note: this function requires a lock on cs_main before calling. (See below comments.)
bool SendMessages(CNode* pto, bool fSendTrickle)
{
    // Some comments and TODOs in order...
    // 1. This function never returns anything but true... (try to find a return other than true).
    // 2. The try lock inside this function causes a potential deadlock due to a lock order reversal in main.
    // 3. The reason for the interior lock is vacated by 1. So the below is commented out, and moved to
    //    the ThreadMessageHandler2 in net.cpp.
    // 4. We need to research why we never return false at all, and subordinately, why we never consume
    //    the value of this function.

    /*
    // Treat lock failures as send successes in case the caller disconnects
    // the node based on the return value.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return true;
    */

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
                static arith_uint256 hashSalt;
                if (hashSalt == 0)
                    hashSalt = UintToArith256(GetRandHash());
                uint256 hashRand = ArithToUint256(UintToArith256(inv.hash) ^ hashSalt);
                hashRand = Hash(hashRand.begin(), hashRand.end());
                bool fTrickleWait = ((UintToArith256(hashRand) & 3) != 0);

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

        // Brod: do not request stuff if it was already removed from this map
        // TODO: check thread safety - JCO - I think I have addressed.
        LOCK(CScraperManifest::cs_mapManifest);

        const auto iaaf= mapAlreadyAskedFor.find(inv);

        bool fAlreadyHave = AlreadyHave(txdb, inv);

        /* Check also the scraper data propagation system to see if it needs
         * this inventory object */
        fAlreadyHave = fAlreadyHave && CScraperManifest::AlreadyHave(0, inv);

        if ( iaaf!=mapAlreadyAskedFor.end() && !fAlreadyHave )
        {
            LogPrint(BCLog::LogFlags::NET, "sending getdata: %s", inv.ToString());
            vGetData.push_back(inv);
            if (vGetData.size() >= 1000)
            {
                pto->PushMessage("getdata", vGetData);
                vGetData.clear();
            }

            // mapAlreadyAskedFor[inv] = nNow; //TODO: check why this was here
        }
        pto->mapAskFor.erase(pto->mapAskFor.begin());
    }
    if (!vGetData.empty())
        pto->PushMessage("getdata", vGetData);

    return true;
}

const CBlockIndex* GetHistoricalMagnitude(const NN::MiningId mining_id)
{
    if (const NN::CpidOption cpid = mining_id.TryCpid())
    {
        const NN::ResearchAccount account = NN::Tally::GetAccount(*cpid);

        // Last block Hash paid to researcher
        if (const auto pindex_option = account.LastRewardBlock())
        {
            const CBlockIndex* pblockindex = *pindex_option;

            // Starting at the block prior to StartHeight, find the last instance of the CPID in the chain:
            // Limit lookback to 6 months
            int nMinIndex = pindexBest->nHeight-(6*30*BLOCKS_PER_DAY);
            if (nMinIndex < 2) nMinIndex=2;


            if(!pblockindex->pnext && pblockindex!=pindexBest)
                LogPrintf("WARNING GetHistoricalMagnitude: index {%s %d} for cpid %s, "
                    "is not in the main chain",
                    pblockindex->GetBlockHash().ToString(),
                    pblockindex->nHeight,
                    cpid->ToString());

            if (pblockindex->nHeight < nMinIndex)
            {
                // In this case, the last staked block was Found, but it is over 6 months old....
                LogPrintf("GetHistoricalMagnitude: Last staked block found at height %d, but cannot verify magnitude older than 6 months (min %d)!",pblockindex->nHeight,nMinIndex);
                return pindexGenesisBlock;
            }

            return pblockindex;
        }
    }

    return pindexGenesisBlock;
}

NN::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex)
{
    CBlock block;

    if (!pblockindex || !pblockindex->IsInMainChain()
        || !block.ReadFromDisk(pblockindex))
    {
        return boost::none;
    }

    return block.PullClaim();
}

bool IsResearcher(const std::string& cpid)
{
    return cpid.length() == 32;
}

/** Fees collected in block by miner **/
int64_t GetFeesCollected(const CBlock& block)
{
    // Return 0 if there is no transactions in block.
    if (block.vtx.size() < 2)
        return 0;

    int64_t nFees = 0;

    CTxDB txdb("r");

    for (unsigned int i = 2; i < block.vtx.size(); i++)
    {
        int64_t nDebit = 0;
        int64_t nCredit = 0;
        const CTransaction& txData = block.vtx[i];

        // Scan inputs
        for (const auto& txvinDataParse : txData.vin)
        {
            CTransaction txvinData;

            if (txdb.ReadDiskTx(txvinDataParse.prevout.hash, txvinData))
                nDebit += txvinData.vout[txvinDataParse.prevout.n].nValue;

            else
                return 0;
        }

        // Scan outputs
        for (const auto& txvoutDataParse : txData.vout)
            nCredit += txvoutDataParse.nValue;

        // Since we now have the input and output amount results we calculate the fees spent
        nFees += nDebit - nCredit;
    }

    return nFees;
}
