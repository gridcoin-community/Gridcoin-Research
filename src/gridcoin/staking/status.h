// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "sync.h"
#include "uint256.h"

#include <string>
#include <vector>

namespace GRC {
class MinerStatus
{
public:
    CCriticalSection lock;

    // Update STAKING_ERROR_STRINGS when adding or removing items.
    enum ReasonNotStakingCategory
    {
        NONE,
        NO_MATURE_COINS,
        NO_COINS,
        ENTIRE_BALANCE_RESERVED,
        NO_UTXOS_AVAILABLE_DUE_TO_RESERVE,
        WALLET_LOCKED,
        TESTNET_ONLY,
        OFFLINE
    };

    std::vector<ReasonNotStakingCategory> vReasonNotStaking;
    bool able_to_stake = true;
    std::string ReasonNotStaking;

    uint64_t WeightSum, WeightMin, WeightMax;
    double ValueSum;
    int Version;
    uint64_t CreatedCnt;
    uint64_t AcceptedCnt;
    uint64_t KernelsFound;
    int64_t nLastCoinStakeSearchInterval;
    uint256 m_last_pos_tx_hash;

    uint64_t masked_time_intervals_covered = 0;
    uint64_t masked_time_intervals_elapsed = 0;

    double actual_cumulative_weight = 0.0;
    double ideal_cumulative_weight = 0.0;

    void Clear();
    MinerStatus();

    bool SetReasonNotStaking(ReasonNotStakingCategory not_staking_error);
    void ClearReasonsNotStaking();
}; // MinerStatus
} // namespace GRC

extern GRC::MinerStatus g_miner_status;
