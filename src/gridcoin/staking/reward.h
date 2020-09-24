#pragma once

class CBlockIndex;

namespace GRC {
int64_t GetProofOfStakeReward(
    uint64_t nCoinAge,
    int64_t nTime,
    const CBlockIndex* const pindexLast);

int64_t GetConstantBlockReward(const CBlockIndex* index);
} // namespace GRC
