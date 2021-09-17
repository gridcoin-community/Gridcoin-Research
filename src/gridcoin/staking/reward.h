// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_STAKING_REWARD_H
#define GRIDCOIN_STAKING_REWARD_H

#include "amount.h"

class CBlockIndex;

namespace GRC {
CAmount GetProofOfStakeReward(
    uint64_t nCoinAge,
    int64_t nTime,
    const CBlockIndex* const pindexLast);

CAmount GetConstantBlockReward(const CBlockIndex* index);
} // namespace GRC

#endif // GRIDCOIN_STAKING_REWARD_H
