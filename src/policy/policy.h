// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_POLICY_H
#define BITCOIN_POLICY_POLICY_H

#include "main.h"

/**
 * Mandatory script verification flags that all new transactions must comply with for
 * them to be valid. Failing one of these tests may trigger a DoS ban;
 * see CheckInputScripts() for details.
 *
 * Note that this does not affect consensus validity; see GetBlockScriptFlags()
 * for that.
 */
static constexpr unsigned int MANDATORY_SCRIPT_VERIFY_FLAGS{SCRIPT_VERIFY_P2SH |
                                                            SCRIPT_VERIFY_DERSIG |
                                                            SCRIPT_VERIFY_NULLDUMMY};

/**
 * Standard script verification flags that standard transactions will comply
 * with. However we do not ban/disconnect nodes that forward txs violating
 * the additional (non-mandatory) rules here, to improve forwards and
 * backwards compatibility.
 */
static constexpr unsigned int STANDARD_SCRIPT_VERIFY_FLAGS{MANDATORY_SCRIPT_VERIFY_FLAGS |
                                                             SCRIPT_VERIFY_STRICTENC |
                                                             SCRIPT_VERIFY_MINIMALDATA |
                                                             SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
                                                             SCRIPT_VERIFY_CLEANSTACK |
                                                             SCRIPT_VERIFY_LOW_S};

/** For convenience, standard but not mandatory verify flags. */
static constexpr unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS{STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS};

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType);

/** Check for standard transaction types
    @return True if all outputs (scriptPubKeys) use only standard transaction forms
*/
bool IsStandardTx(const CTransaction& tx);

/** Check for standard transaction types
    @param[in] tx   Transaction to check
    @param[in] mapInputs	Map of previous transactions that have outputs tx is spending
    @return True if all inputs (scriptSigs) use only standard transaction forms
    @see FetchInputs
*/
bool AreInputsStandard(const CTransaction& tx, const MapPrevTx& mapInputs);

//!
//! \brief Gets the minimum number of connections required for a wallet to stake.
//!
//! \return unsigned int minimum number of connections to stake
//!
unsigned int GetMinimumConnectionsRequiredForStaking();

//!
//! \brief Gets the maximum number of inputs supported for a UTXO consolidation transaction to ensure
//! the transaction does not exceed the maximum size and fail as a result.
//!
//! \return unsigned int maximum number of consolidation inputs.
//!
unsigned int GetMaxInputsForConsolidationTxn();

#endif // BITCOIN_POLICY_POLICY_H
