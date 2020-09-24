#pragma once

#include "sync.h"

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

    void Clear();
    MinerStatus();

    bool SetReasonNotStaking(ReasonNotStakingCategory not_staking_error);
    void ClearReasonsNotStaking();
}; // MinerStatus
} // namespace GRC

extern GRC::MinerStatus g_miner_status;
