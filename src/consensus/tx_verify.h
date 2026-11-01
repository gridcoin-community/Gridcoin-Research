// Copyright (c) 2017-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_TX_VERIFY_H
#define BITCOIN_CONSENSUS_TX_VERIFY_H

#include "primitives/block.h"
#include <validation.h>

#include <utility>
#include <vector>

class CTransaction;

/** Count ECDSA signature operations the old-fashioned (pre-0.6) way
    @param[in] tx The transaction to count
    @return number of sigops tx's outputs will produce when spent
    @see FetchInputs
*/
unsigned int GetLegacySigOpCount(const CTransaction& tx);

/** Count ECDSA signature operations in pay-to-script-hash inputs.
    @param[in] tx The transaction to count
    @param[in] mapInputs	Map of previous transactions that have outputs tx is spending
    @return maximum number of sigops required to validate tx's inputs
    @see FetchInputs
*/
unsigned int GetP2SHSigOpCount(const CTransaction& tx, const MapPrevTx& inputs);

/**
 * Check if transaction is final and can be included in a block with the
 * specified height and time. Consensus critical.
 */
bool IsFinalTx(const CTransaction &tx, int nBlockHeight = 0, int64_t nBlockTime = 0);

/**
 * Calculates the block height and time at which the transaction's sequence
 * locks are satisfied, for each input of the transaction. The returned pair's
 * first element is the minimum block height, and the second is the minimum
 * median time past (of the block prior to the evaluating block).
 *
 * @param[in]     tx          The transaction to evaluate
 * @param[in]     flags       Combination of LOCKTIME_ flags
 * @param[in,out] prevHeights Vector of heights at which each input was confirmed
 * @param[in]     block       The block index against which to evaluate
 */
std::pair<int, int64_t> CalculateSequenceLocks(
    const CTransaction& tx, int flags,
    std::vector<int>& prevHeights, const CBlockIndex& block);

/**
 * Checks whether the sequence lock requirements are met for the given block.
 */
bool EvaluateSequenceLocks(const CBlockIndex& block,
                           std::pair<int, int64_t> lockPair);

/**
 * Check sequence locks: look up each input's confirmation height and verify
 * that relative lock-time constraints encoded in nSequence are satisfied.
 * Requires cs_main.
 */
bool CheckSequenceLocks(const CTransaction& tx, int flags,
                        const CBlockIndex* pindexPrev);

#endif // BITCOIN_CONSENSUS_TX_VERIFY_H
