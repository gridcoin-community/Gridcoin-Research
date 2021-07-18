// Copyright (c) 2017-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_TX_VERIFY_H
#define BITCOIN_CONSENSUS_TX_VERIFY_H

#include "main.h"

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

#endif // BITCOIN_CONSENSUS_TX_VERIFY_H
