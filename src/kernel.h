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

//!
//! \brief Calculate the provided block's proof hash with the version 3 staking
//! kernel algorithm for version 7 blocks to carry the stake modifier.
//!
//! The stake modifier is a value that increases the difficulty of precomputing
//! future blocks in the chain. The validity of blocks in succession depends on
//! the modifiers computed for previous blocks. Gridcoin updated the protocol's
//! block proving algorithm for block version 8, but the new algorithm requires
//! proof hash inputs from the version 7 blocks generated before the threshold.
//!
//! Computation of a stake modifier requires a block's proof hash as input. The
//! version 3 staking kernel calculates the hash by including the RSA weight in
//! the digest. This legacy parameter comes from the claim section of coinstake
//! transactions and is not used in later staking security or reward protocols.
//!
//! To avoid the need to maintain and call unwieldy legacy functions that carry
//! version 7 stake modifiers into version 8 blocks, this function consolidates
//! the version 3 staking kernel into a routine that produces the proof hash so
//! that nodes index version 7 blocks with accurate historical stake modifiers.
//!
//! \param block          The block to compute a proof hash for.
//! \param por_nonce      An input parameter for the proof hash.
//! \param out_hash_proof Assigned the final computed value of the proof hash.
//!
//! \return \c false when reading the coinstake input transaction or block from
//! disk fails.
//!
bool CalculateLegacyV3HashProof(
    const CBlock& block,
    const double por_nonce,
    uint256& out_hash_proof);

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
