// Copyright (c) 2011-2012 The PPCoin developers
// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_STAKING_DIFFICULTY_H
#define GRIDCOIN_STAKING_DIFFICULTY_H

#include "amount.h"
#include "arith_uint256.h"
#include "sync.h"

#include <cstdint>
#include <vector>
class CBlockIndex;
class CWallet;
struct StakeCandidate;
#include <cmath>

extern CCriticalSection cs_main;

namespace GRC {
// Note that dDiff cannot be = 0 normally. This is set as default because you can't specify the output of
// GetAverageDifficulty(nPosInterval) = to dDiff here.
// The default confidence is 1-1/e which is the mean for the geometric distribution for small probabilities.
const double DEFAULT_ETTS_CONFIDENCE = 1.0 - 1.0 / exp(1.0);

//! v15 retarget and easing constants, in seconds.
constexpr int64_t V15_TARGET_SPACING = 90;
constexpr int64_t V15_RETARGET_TIME_CONSTANT = 990;
constexpr int64_t V15_SPACING_CAP = 2500;
constexpr int64_t V15_EASING_THRESHOLD = 300;
constexpr int64_t V15_EASING_DOUBLING = 256;
constexpr int64_t V15_EASING_MAX_DOUBLINGS = 64;

//! v15 block timestamp window, in seconds: a block's stamp may trail its parent's by at most V15_PAST_TIME_BOUND and
//! lead the receiver's adjusted time by at most V15_FUTURE_TIME_BOUND.
constexpr int64_t V15_PAST_TIME_BOUND = 48;
constexpr int64_t V15_FUTURE_TIME_BOUND = 48;

//! The proof-of-stake target limit, the easiest target a block may use.
const arith_uint256& GetProofOfStakeLimit();

//!
//! \brief Scale a target by 2^(exponent_q16 / 65536) in fixed point.
//!
//! The fractional power uses the cubic approximation from BCH's aserti3-2d. The target is first clamped to
//! \p limit; the result is clamped to [1, limit], and any overflow of the 256-bit intermediate maps to \p limit.
//!
arith_uint256 ScaleTargetExp2(const arith_uint256& target, int64_t exponent_q16, const arith_uint256& limit);

//!
//! \brief The v15 retarget: the next base target from the parent's base target and its signed spacing.
//!
//! target * exp(2 (min(spacing, 2500) - 90) / 990). The exponents telescope, so the long-run mean spacing is 90 s
//! whatever the spacing distribution.
//!
arith_uint256 RetargetLogLinear(const arith_uint256& parent_target, int64_t spacing);

//!
//! \brief The v15 easing: the effective target for a block stamped \p elapsed seconds after its parent.
//!
//! base * 2^((elapsed - 300) / 256) once elapsed exceeds 300 s, capped at the proof-of-stake limit; unchanged
//! otherwise.
//!
arith_uint256 EaseTarget(const arith_uint256& base_target, int64_t elapsed);

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double GetDifficulty(const CBlockIndex* blockindex = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double GetBlockDifficulty(unsigned int nBits);
double GetCurrentDifficulty() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double GetTargetDifficulty() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double GetAverageDifficulty(unsigned int nPoSInterval = 40) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double GetSmoothedDifficulty(int64_t nStakeableBalance) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Self-managed locking: takes LOCK2(cs_main, wallet.cs_wallet) internally.
//! Safe to call with or without cs_main already held (cs_main is recursive).
uint64_t GetStakeWeight(const CWallet& wallet);

//! The value the candidates stake with: each coin's value in whole V8 weight units, which is all the kernel weighs.
CAmount GetStakingValue(const std::vector<StakeCandidate>& candidates);
double GetEstimatedNetworkWeight(unsigned int nPoSInterval = 40) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

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
uint64_t GetAvgNetworkWeight(const unsigned int& block_interval, CBlockIndex* index_start = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

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
uint64_t GetAvgNetworkWeight(CBlockIndex* index_start = nullptr, CBlockIndex* index_end = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
//! Self-managed locking: takes LOCK2(cs_main, pwalletMain->cs_wallet) internally.
//! Safe to call with or without cs_main already held (cs_main is recursive).
double GetEstimatedTimetoStake(bool ignore_staking_status = false, double dDiff = 0.0, double dConfidence = DEFAULT_ETTS_CONFIDENCE);
} // namespace GRC

#endif // GRIDCOIN_STAKING_DIFFICULTY_H
