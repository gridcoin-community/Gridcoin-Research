#include "gridcoin/appcache.h"
#include "gridcoin/staking/reward.h"
#include "main.h"

namespace {
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
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

int64_t GRC::GetConstantBlockReward(const CBlockIndex* index)
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
    if ((index->nTime - oCBReward.timestamp) <= (60 * 24 * 30 * 6 * 60)) {
        reward = atoi64(oCBReward.value);
    }

    reward = std::max(reward, MIN_CBR);
    reward = std::min(reward, MAX_CBR);

    return reward;
}

int64_t GRC::GetProofOfStakeReward(
    const uint64_t nCoinAge,
    const int64_t nTime,
    const CBlockIndex* const pindexLast)
{
    if (pindexLast->nVersion >= 10) {
        return GetConstantBlockReward(pindexLast);
    }

    return nCoinAge * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8);
}
