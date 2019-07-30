// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef PPCOIN_KERNEL_H
#define PPCOIN_KERNEL_H

#include "main.h"

// To decrease granularity of timestamp
// Supposed to be 2^n-1
static const int STAKE_TIMESTAMP_MASK = 15;

// MODIFIER_INTERVAL: time to elapse before new modifier is computed
extern unsigned int nModifierInterval;

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

// Get time weight using supplied timestamps
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd);

//Block version 8+ Staking
bool CheckProofOfStakeV8(
    CBlockIndex* pindexPrev, //previous block in chain index
    CBlock &Block, //block to check
    bool generated_by_me,
    uint256& hashProofOfStake); //proof hash out-parameter
bool FindStakeModifierRev(uint64_t& StakeModifier,CBlockIndex* pindexPrev);

// Kernel for V8
CBigNum CalculateStakeHashV8(
    const CBlock &CoinBlock, const CTransaction &CoinTx,
    unsigned CoinTxN, unsigned nTimeTx,
    uint64_t StakeModifier,
    const MiningCPID &BoincData);
int64_t CalculateStakeWeightV8(
    const CTransaction &CoinTx, unsigned CoinTxN,
    const MiningCPID &BoincData);

#endif // PPCOIN_KERNEL_H
