// Copyright (c) 2011-2012 The PPCoin developers
// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

class CBlockIndex;
class CWallet;

namespace GRC {
// Note that dDiff cannot be = 0 normally. This is set as default because you can't specify the output of
// GetAverageDifficulty(nPosInterval) = to dDiff here.
// The defeult confidence is 1-1/e which is the mean for the geometric distribution for small probabilities.
const double DEFAULT_ETTS_CONFIDENCE = 1.0 - 1.0 / exp(1.0);

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast);
double GetDifficulty(const CBlockIndex* blockindex = nullptr);
double GetBlockDifficulty(unsigned int nBits);
double GetCurrentDifficulty();
double GetTargetDifficulty();
double GetAverageDifficulty(unsigned int nPoSInterval = 40);
double GetSmoothedDifficulty(int64_t nStakeableBalance);

uint64_t GetStakeWeight(const CWallet& wallet);
double GetEstimatedNetworkWeight(unsigned int nPoSInterval = 40);
double GetEstimatedTimetoStake(bool ignore_staking_status = false, double dDiff = 0.0, double dConfidence = DEFAULT_ETTS_CONFIDENCE);
} // namespace GRC
