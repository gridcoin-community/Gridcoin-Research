// Copyright (c) 2011-2012 The PPCoin developers
// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_STAKING_DIFFICULTY_H
#define GRIDCOIN_STAKING_DIFFICULTY_H

#include <cstdint>
class CBlockIndex;
class CWallet;
#include <cmath>

namespace GRC {
// Note that dDiff cannot be = 0 normally. This is set as default because you can't specify the output of
// GetAverageDifficulty(nPosInterval) = to dDiff here.
// The default confidence is 1-1/e which is the mean for the geometric distribution for small probabilities.
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

//!
//! \brief This returns the precise average network weight in units of Halfords as a 64 bit unsigned integer. This form takes
//! two arguments: the number of blocks to lookback and include in the average, and the ending point CBlockIndex pointer. Note
//! if the index pointer is not specified the lookback start is the current (best) block.
//!
//! Please refer to https://gridcoin.us/assets/docs/grc-bluepaper-section-1.pdf equations 1 and 16 and footnote 5.
//! This method of computing net_weight is from first principles using the target from the nBits representation
//! recorded in the index, rather than the GetEstimatedNetworkWeight() function, which uses double fp arithmetic.
//!
//! \param block_interval The number of blocks looking back from the index_start to include in the average.
//! \param index_start The CBlockIndex pointer to the starting index with which to take the average.
//!
//! \return uint64_t of the average network weight in Halford units.
//!
uint64_t GetAvgNetworkWeight(const unsigned int& block_interval, CBlockIndex* index_start = nullptr);

//!
//! \brief This returns the precise average network weight in units of Halfords as a 64 bit unsigned integer. The starting index
//! and the ending index are defaulted to nullptr if not provided. If neither is provided, the network weight for the current (best)
//! block will be returned. If only the starting index pointer is provided, the network weight of that block will be returned.
//! If both are provided, the network weight average will be returned over the interval of blocks inclusive of both start and end.
//! Both indexes, if specified, must be in the main chain.
//!
//! Please refer to https://gridcoin.us/assets/docs/grc-bluepaper-section-1.pdf equations 1 and 16 and footnote 5.
//! This method of computing net_weight is from first principles using the target from the nBits representation
//! recorded in the index, rather than the GetEstimatedNetworkWeight() function, which uses double fp arithmetic.
//!
//! \param index_start The CBlockIndex pointer to the starting index with which to take the average. Note that this is inclusive.
//! \param index_end The BClockIndex pointer to the ending index with which to take the average. Note that this is inclusive.
//!
//! \return uint64_t of the average network weight in Halford units.
//!
uint64_t GetAvgNetworkWeight(CBlockIndex* index_start = nullptr, CBlockIndex* index_end = nullptr);
double GetEstimatedTimetoStake(bool ignore_staking_status = false, double dDiff = 0.0, double dConfidence = DEFAULT_ETTS_CONFIDENCE);
} // namespace GRC

#endif // GRIDCOIN_STAKING_DIFFICULTY_H
