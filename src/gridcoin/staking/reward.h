// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

class CBlockIndex;

namespace GRC {
int64_t GetProofOfStakeReward(
    uint64_t nCoinAge,
    int64_t nTime,
    const CBlockIndex* const pindexLast);

int64_t GetConstantBlockReward(const CBlockIndex* index);
} // namespace GRC
