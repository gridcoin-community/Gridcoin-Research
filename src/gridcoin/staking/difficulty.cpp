// Copyright (c) 2011-2012 The PPCoin developers
// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "bignum.h"
#include "init.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/status.h"
#include "main.h"
#include "txdb.h"
#include "wallet/wallet.h"

#include <vector>

using namespace GRC;

namespace {
constexpr int64_t TARGET_TIMESPAN = 16 * 60;  // 16 mins in seconds
const CBigNum PROOF_OF_STAKE_LIMIT(ArithToUint256(~arith_uint256() >> 20));

// ppcoin: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = pindex->pprev;
    return pindex;
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

unsigned int GRC::GetNextTargetRequired(const CBlockIndex* pindexLast)
{
    if (pindexLast == nullptr) {
        return PROOF_OF_STAKE_LIMIT.GetCompact(); // genesis block
    }

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, true);
    if (pindexPrev->pprev == nullptr) {
        return PROOF_OF_STAKE_LIMIT.GetCompact(); // first block
    }

    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, true);
    if (pindexPrevPrev->pprev == nullptr) {
        return PROOF_OF_STAKE_LIMIT.GetCompact(); // second block
    }

    const int64_t nTargetSpacing = GetTargetSpacing(pindexLast->nHeight);
    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();

    if (nActualSpacing < 0) {
        nActualSpacing = nTargetSpacing;
    }

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->nBits);

    // Gridcoin - Reset Diff to 1 on 12-19-2014 (R Halford) - Diff sticking at
    // 2065 due to many incompatible features:
    if (pindexLast->nHeight >= 91387 && pindexLast->nHeight <= 91500) {
        return PROOF_OF_STAKE_LIMIT.GetCompact();
    }

    // 1-14-2015 R Halford - Make diff reset to zero after periods of exploding
    // diff:
    if (GetCurrentDifficulty() > 900000) {
        return PROOF_OF_STAKE_LIMIT.GetCompact();
    }

    // Since TARGET_TIMESPAN is (16 * 60) or 16 mins and our TargetSpacing = 64,
    // the nInterval = 15 min

    const int64_t nInterval = TARGET_TIMESPAN / nTargetSpacing;
    bnNew *= (nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing;
    bnNew /= (nInterval + 1) * nTargetSpacing;

    if (bnNew <= 0 || bnNew > PROOF_OF_STAKE_LIMIT) {
        bnNew = PROOF_OF_STAKE_LIMIT;
    }

    return bnNew.GetCompact();
}

double GRC::GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == nullptr) {
        if (pindexBest == nullptr) {
            return 1.0;
        } else {
            blockindex = GetLastBlockIndex(pindexBest, false);
        }
    }

    return GetBlockDifficulty(blockindex->nBits);
}

double GRC::GetBlockDifficulty(unsigned int nBits)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    int nShift = (nBits >> 24) & 0xff;
    double dDiff = (double)0x0000ffff / (double)(nBits & 0x00ffffff);

    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }

    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GRC::GetCurrentDifficulty()
{
    return GetDifficulty(GetLastBlockIndex(pindexBest, true));
}

double GRC::GetTargetDifficulty()
{
    return GetBlockDifficulty(GetNextTargetRequired(pindexBest));
}

// This requires a lock on cs_main when called.
double GRC::GetAverageDifficulty(unsigned int nPoSInterval)
{
    /*
     * Diff is inversely related to Target (without the coinweight multiplier), but proportional to the
     * effective number of coins on the network. This is tricky, if you want to get the average target value
     * used over an interval you should use a harmonic average, since target is inversely related to diff. If
     * on the other hand, you want to average diff in a way to also determine the average coins active in
     * the network, you should simply use an arithmetic average. See the relation between diff and estimated
     * network weight above. We do not need to take into account the actual spacing of the blocks, because this
     * already handled by the retargeting in GetNextTargetRequired, and in fact, given the random distribution
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
            }
        }

        pindex = pindex->pprev;
    }

    result = nStakesHandled ? dDiffSum / nStakesHandled : 0;
    LogPrint(BCLog::LogFlags::NOISY, "GetAverageDifficulty debug: nStakesHandled = %u", nStakesHandled);
    LogPrint(BCLog::LogFlags::NOISY, "GetAverageDifficulty debug: Average dDiff = %f", result);

    return result;
}

// This requires a lock on cs_main when called.
double GRC::GetSmoothedDifficulty(int64_t nStakeableBalance)
{
    // The smoothed difficulty is derived via a two step process. There is a coupling between the desired block
    // span to compute difficulty and essentially the stakeable balance: If the balance is low, the ETTS is
    // expected to be relatively long, so it is appropriate to use a longer span to compute the difficulty.
    // Conversely, if the stakeable balance is high, it is appropriate to use a corresponding short span so that
    // the staking estimate reflects appropriate network conditions compared to the expected staking interval.
    // Since this is a coupled problem, the approach here, which works reasonably well, uses the last hour
    // (40 block) diff to bootstrap a span estimate from using the thumbrule estimate of ETTS with that diff,
    // and then re-computes the diff using the block span, with clamp of [40, BLOCKS_PER_DAY], since it is silly to
    // allow difficulty to become more sensitive than one hour of change, and is of minimal value to long term
    // projections to use more than a day's history of diff. (Difficulty patterns tend to repeat on a daily basis.
    // Longer term historical variations of more than a day are due to extraneous variables of which history is of
    // little predictive value.)

    double dDiff = 1.0;

    // First estimate the difficulty based on the last 40 blocks.
    dDiff = GetAverageDifficulty(40);

    // Compute an appropriate block span for the second iteration of dificulty computation based on the
    // above diff calc. Clamp to no less than 40 (~1 hour) and no more than 960 (~1 day). Note that those
    // familiar with the thumbrule for ETTS, ETTS = 10000 / Balance * Diff should recognize it in the below
    // expression. Note that the actual constant is 9942.2056 (from the bluepaper, eq. 12), but it suffices to
    // use the rounded thumbrule value here.
    unsigned int nEstAppropriateDiffSpan = clamp<unsigned int>(10000.0 * BLOCKS_PER_DAY * COIN
                                                               / nStakeableBalance * dDiff,
                                                               40, 960);

    LogPrint(BCLog::LogFlags::NOISY, "GetSmoothedDifficulty debug: nStakeableBalance: %u", nStakeableBalance);
    LogPrint(BCLog::LogFlags::NOISY, "GetSmoothedDifficulty debug: nEstAppropriateDiffSpan: %u", nEstAppropriateDiffSpan);

    dDiff = GetAverageDifficulty(nEstAppropriateDiffSpan);

    return dDiff;
}

uint64_t GRC::GetStakeWeight(const CWallet& wallet)
{
    if (wallet.GetBalance() <= nReserveBalance) {
        return 0;
    }

    const int64_t now = GetAdjustedTime();

    std::vector<std::pair<const CWalletTx*, unsigned int>> coins;
    GRC::MinerStatus::ReasonNotStakingCategory unused;
    int64_t balance = 0;

    LOCK2(cs_main, wallet.cs_wallet);

    if (!wallet.SelectCoinsForStaking(now, coins, unused, balance)) {
        return 0;
    }

    CTxDB txdb("r");
    uint64_t weight = 0;

    for (const auto& pcoin : coins) {
        CTxIndex txindex;

        if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex)) {
            continue;
        }

        if (now - pcoin.first->nTime > nStakeMinAge) {
            weight += (pcoin.first->vout[pcoin.second].nValue);
        }
    }

    return weight;
}

double GRC::GetEstimatedNetworkWeight(unsigned int nPoSInterval)
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

double GRC::GetEstimatedTimetoStake(bool ignore_staking_status, double dDiff, double dConfidence)
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
        LOCK(g_miner_status.lock);

        staking = g_miner_status.nLastCoinStakeSearchInterval && g_miner_status.WeightSum;

        able_to_stake = g_miner_status.able_to_stake;
    }

    // Get out early if not staking, ignore_staking_status is false, and not able_to_stake and set return value of 0.
    if (!ignore_staking_status && !staking && !able_to_stake)
    {
        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: Not staking: ETTS = %f", result);
        return result;
    }

    CAmount nValue = 0;
    int64_t nCurrentTime = GetAdjustedTime();
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nCurrentTime = %i", nCurrentTime);

    CTxDB txdb("r");

    // Here I am defining a time mask 16 times as long as the normal stake time mask. This is to quantize the UTXO's into a maximum of
    // 16 hours * 3600 / 256 = 225 time bins for evaluation. Otherwise for a large number of UTXO's, this algorithm could become
    // really expensive.
    const int ETTS_TIMESTAMP_MASK = (16 * (GRC::STAKE_TIMESTAMP_MASK + 1)) - 1;
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: ETTS_TIMESTAMP_MASK = %x", ETTS_TIMESTAMP_MASK);

    CAmount BalanceAvailForStaking = 0;
    std::vector<COutput> vCoins;

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

        //reminder... void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl=nullptr, bool fIncludeStakingCoins=false) const;
        pwalletMain->AvailableCoins(vCoins, true, nullptr, true);
    }


    // An efficient local structure to store the UTXO's with the bare minimum info we need.
    typedef std::vector< std::pair<int64_t, CAmount> > vCoinsExt;
    vCoinsExt vUTXO;
    // A local ordered set to store the unique "bins" corresponding to the UTXO transaction times. We are going to use this
    // for the outer loop.
    std::set<int64_t> UniqueUTXOTimes;
    // We want the first "event" to be the CurrentTime. This does not have to be quantized.
    UniqueUTXOTimes.insert(nCurrentTime);

    // Debug output cooldown...
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nStakeMinAge = %i", nStakeMinAge);

    int64_t nTime = 0;
    int64_t nStakeableBalance = 0;
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
        vUTXO.push_back(std::pair<int64_t, CAmount>( nTime, nValue));
        nStakeableBalance += nValue;

        LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: pair (relative to current time: <%i, %i>", nTime - nCurrentTime, nValue);

        // Only record a time below if it is after nCurrentTime, because UTXO's that have matured already are already stakeable and can be grouped (will be found)
        // by the nCurrentTime record that was already injected above.
        if (nTime > nCurrentTime) UniqueUTXOTimes.insert(nTime);
        }
    }

    // If dDiff = 0 from supplied argument (which is also the default), then derive a smoothed difficulty, otherwise
    // let the supplied argument dDiff stand.
    if (dDiff == 0)
    {
        LOCK(cs_main);

        // First estimate the difficulty based on the last 40 blocks.
        dDiff = GetSmoothedDifficulty(nStakeableBalance);
    }

    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dDiff = %f", dDiff);

    // The stake probability per "throw" of 1 weight unit = target value at diff of 1.0 / (maxhash * diff). This happens effectively every STAKE_TIMESTAMP_MASK+1 sec.
    double dUnitStakeProbability = 1 / (4295032833.0 * dDiff);
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: dUnitStakeProbability = %e", dUnitStakeProbability);

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
        nThrows = nDeltaTime / (GRC::STAKE_TIMESTAMP_MASK + 1);
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

    nDeltaTime = nThrows * (GRC::STAKE_TIMESTAMP_MASK + 1);
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: nDeltaTime = %i", nDeltaTime);

    // Because we are looking at the delta time required past nTime, which is where we exited the Gantt chart loop.
    result = nDeltaTime + nTime - nCurrentTime;
    LogPrint(BCLog::LogFlags::NOISY, "GetEstimatedTimetoStake debug: ETTS at %d confidence = %i", dConfidence, result);

    return result;
}
