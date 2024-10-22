// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "gridcoin/protocol.h"
#include "gridcoin/staking/reward.h"
#include "main.h"

namespace {
CAmount GetCoinYearReward(int64_t nTime)
{
    // Gridcoin Global Interest Rate Schedule
    CAmount INTEREST = 9;

    if (nTime >= 1410393600 && nTime <= 1417305600) INTEREST =   9 * CENT; // 09% between inception  and 11-30-2014
    if (nTime >= 1417305600 && nTime <= 1422576000) INTEREST =   8 * CENT; // 08% between 11-30-2014 and 01-30-2015
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

CAmount GRC::GetConstantBlockReward(const CBlockIndex* index)
{
    CAmount reward = 0;
    CAmount MIN_CBR = 0;
    CAmount MAX_CBR = 0;

    // GetConstantBlockReward is called with a CBlockIndex pointer as an argument, which means it is expected to
    // return the correct value at that position in the chain. The TryActive call in the protocol registry returns
    // the CURRENT active value if that exists. This is not what we need if index is not actually pIndexBest.
    // Here we find the last protocol entry in the historical linked list of protocol entries for the key "blockreward1"
    // that has a timestamp that is less than or equal to the block time. This will filter for the correct historical
    // value of blockreward1 that was valid at the index given. One could argue there is some fuzziness here in using
    // the block time versus the transaction context time implied by the protocol registry entry; however the actual
    // change of CBR must occur on a block boundary anyway, and as long as there is consistent application for consensus
    // for V13+ we are safe.
    ProtocolEntryOption reward_entry = GetProtocolRegistry().TryLastBeforeTimestamp("blockreward1",
                                                                                    index->GetBlockTime());

    // The constant block reward is set to a default, voted on value, but this can
    // be overridden using an admin message. This allows us to change the reward
    // amount without having to release a mandatory with updated rules. In the case
    // there is a breach or leaked admin keys the rewards are clamped.

    // Note that the use of the IsV13Enabled test here on the block index does not consider the possibility of
    // an administrative contract entry prior to v13 that has aged out from the lookback window which could affect consensus.
    // However this possibility is actually covered because 5.4.8.0 was actually a mandatory due to a problem with
    // message contracts, and this included the registry for protocol entries, eliminating the lookback window limitation.
    // There has been no administrative entry to change the CBR in mainnet prior to 5.4.8.0, so in fact this is a moot issue.
    if (IsV13Enabled(index->nHeight)) {
        // The default and the clamp for block v13+ is controlled by blockchain consensus parameters.
        reward = Params().GetConsensus().DefaultConstantBlockReward;
        MIN_CBR = Params().GetConsensus().ConstantBlockRewardFloor;
        MAX_CBR = Params().GetConsensus().ConstantBlockRewardCeiling;

        if (reward_entry != nullptr && reward_entry->m_status == ProtocolEntryStatus::ACTIVE) {
            // There is no contract lookback limitation v13+.
            if (!ParseInt64(reward_entry->m_value, &reward)) {
                error("%s: Cannot parse constant block reward from protocol entry: %s",
                      __func__, reward_entry->m_value);
            }
        }
    } else {
        // This is the default and clamp pre block v13. Note that an administrative entry to change the CBR prior to block v13
        // that is outside the pre v13 clamp rules could cause the CBR to change at the v13 height as the new clamp rules above
        // would then apply on the existing protocol entry.
        reward = 10 * COIN;
        MIN_CBR = 0;
        MAX_CBR = Params().GetConsensus().DefaultConstantBlockReward * 2;

        if (reward_entry != nullptr && reward_entry->m_status == ProtocolEntryStatus::ACTIVE) {
            // The contract lookback window here for block version less than v13 is the same as the standard contract
            // replay lookback.
            if (index->nTime - reward_entry->m_timestamp <= Params().GetConsensus().StandardContractReplayLookback) {
                // This is a little slippery, because if we ever change CAmount from a int64_t, this will
                // break. It is unlikely to ever change, however, and avoids an extra copy/implicit cast.
                if (!ParseInt64(reward_entry->m_value, &reward)) {
                    error("%s: Cannot parse constant block reward from protocol entry: %s",
                          __func__, reward_entry->m_value);
                }
            }
        }
    }

    reward = std::clamp(reward, MIN_CBR, MAX_CBR);

    return reward;
}

CAmount GRC::GetProofOfStakeReward(
    const uint64_t nCoinAge,
    const int64_t nTime,
    const CBlockIndex* const pindexLast)
{
    if (pindexLast->nVersion >= 10) {
        return GetConstantBlockReward(pindexLast);
    }

    return nCoinAge * GetCoinYearReward(nTime) * 33 / (365 * 33 + 8);
}
