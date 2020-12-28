// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "amount.h"
#include "main.h"

namespace GRC {
// To decrease granularity of timestamp
// Supposed to be 2^n-1
static const int STAKE_TIMESTAMP_MASK = 15;

// MODIFIER_INTERVAL: time to elapse before new modifier is computed
extern unsigned int nModifierInterval;

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

//!
//! \brief Apply the stake timestamp mask to the supplied timestamp.
//!
//! \param timestamp Usually a coinstake transaction or block timestamp.
//!
template <typename TimeType>
TimeType MaskStakeTime(const TimeType timestamp)
{
    return timestamp & (~STAKE_TIMESTAMP_MASK);
}

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

// Get time weight using supplied timestamps
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd);

//!
//! \brief Load the previous transaction and its containing block header from
//! disk for the specified output.
//!
//! This function provides an optimized routine to load the staked transaction
//! and block header for a coinstake output by reading both from disk at once.
//!
//! \param txdb         Finds the previous transaction location on disk.
//! \param prevout_hash Hash of the input transaction to load from disk.
//! \param out_header   Header of the previous transaction's block.
//! \param out_txprev   The previous transaction data loaded from disk.
//!
//! \return \c true if the previous transaction and block header were loaded
//! from disk successfully.
//!
bool ReadStakedInput(
    CTxDB& txdb,
    const uint256 prevout_hash,
    CBlockHeader& out_header,
    CTransaction& out_txprev);

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
    CTxDB& txdb,
    const CBlock& block,
    const double por_nonce,
    uint256& out_hash_proof);

//Block version 8+ Staking
bool CheckProofOfStakeV8(
    CTxDB& txdb,
    CBlockIndex* pindexPrev, //previous block in chain index
    CBlock& Block, //block to check
    bool generated_by_me,
    uint256& hashProofOfStake); //proof hash out-parameter

bool FindStakeModifierRev(uint64_t& StakeModifier, CBlockIndex* pindexPrev, int &nHeight);

// Kernel for V8
uint256 CalculateStakeHashV8(
    const CBlockHeader& CoinBlock,
    const CTransaction& CoinTx,
    unsigned CoinTxN,
    unsigned nTimeTx,
    uint64_t StakeModifier);

// overload
// Kernel for V8
uint256 CalculateStakeHashV8(
    unsigned int nBlockTime,
    const CTransaction& CoinTx,
    unsigned CoinTxN,
    unsigned nTimeTx,
    uint64_t StakeModifier);


int64_t CalculateStakeWeightV8(const CTransaction &CoinTx, unsigned CoinTxN);
int64_t CalculateStakeWeightV8(const CAmount& nValueIn);
} // namespace GRC
