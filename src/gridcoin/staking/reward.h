// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"

class CBlockIndex;

namespace GRC {
CAmount GetProofOfStakeReward(
    uint64_t nCoinAge,
    int64_t nTime,
    const CBlockIndex* const pindexLast);

CAmount GetConstantBlockReward(const CBlockIndex* index);
} // namespace GRC
