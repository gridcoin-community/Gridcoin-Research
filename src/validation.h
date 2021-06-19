// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#include "amount.h"
#include "index/disktxpos.h"
#include "primitives/transaction.h"

#include <map>

class CTxDB;
class CBlockHeader;

typedef std::map<uint256, std::pair<CTxIndex, CTransaction>> MapPrevTx;

bool ReadTxFromDisk(CTransaction& tx, CDiskTxPos pos, FILE** pfileRet = nullptr);
bool ReadTxFromDisk(CTransaction& tx, CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);
bool ReadTxFromDisk(CTransaction& tx, CTxDB& txdb, COutPoint prevout);
bool ReadTxFromDisk(CTransaction& tx, COutPoint prevout);

bool CheckTransaction(const CTransaction& tx);

//! \brief Check the validity of any contracts contained in the transaction.
//!
//! \param tx The transaction to check.
//!
//! \param inputs Map of the previous transactions with outputs spent by
//! tx to search for the master key address for validating administrative
//! contracts.
//!
//! \return \c true if all of the contracts in the transaction validate.
//!
bool CheckContracts(const CTransaction& tx, const MapPrevTx& inputs);

//! \brief Determine whether a transaction contains an input spent by the
//! master key holder.
//!
//! \param tx The transaction to check for.
//!
//! \param inputs Map of the previous transactions with outputs spent by
//! this transaction to search for the master key address.
//!
//! \return \c true if at least one of the inputs from one of the previous
//! transactions comes from the master key address.
//!
bool HasMasterKeyInput(const CTransaction& tx, const MapPrevTx& inputs);

const CTxOut& GetOutputFor(const CTxIn& input, const MapPrevTx& inputs);

/** Amount of bitcoins coming in to a transaction
    Note that lightweight clients may not know anything besides the hash of previous transactions,
    so may not be able to calculate this.
    @param[in] tx The transaction
    @param[in] mapInputs	Map of previous transactions that have outputs tx is spending
    @return	Sum of value of all inputs (scriptSigs)
    @see FetchInputs
*/
CAmount GetValueIn(const CTransaction& tx, const MapPrevTx& inputs);

bool DisconnectInputs(CTransaction& tx, CTxDB& txdb);

/** Fetch from memory and/or disk. inputsRet keys are transaction hashes.
    @param[in] tx The transaction to fetch inputs for
    @param[in] txdb	Transaction database
    @param[in] mapTestPool	List of pending changes to the transaction index database
    @param[in] fBlock	True if being called to add a new best-block to the chain
    @param[in] fMiner	True if being called by CreateNewBlock
    @param[out] inputsRet	Pointers to this tx's inputs
    @param[out] fInvalid	returns true if tx is invalid
    @return	Returns true if all inputs are in txdb or mapTestPool
*/
bool FetchInputs(CTransaction& tx, CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool, bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid);

/** Sanity check previous transactions, then, if all checks succeed,
    mark them as spent by tx.
    @param[in] tx The transaction to connect inputs
    @param[in] inputs	Previous transactions (from FetchInputs)
    @param[out] mapTestPool	Keeps track of inputs that need to be updated on disk
    @param[in] posThisTx	Position of tx on disk
    @param[in] pindexBlock
    @param[in] fBlock	true if called from ConnectBlock
    @param[in] fMiner	true if called from CreateNewBlock
    @return Returns true if all checks succeed
    */
bool ConnectInputs(CTransaction& tx, CTxDB& txdb, MapPrevTx inputs, std::map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx, const CBlockIndex* pindexBlock, bool fBlock, bool fMiner);

bool GetCoinAge(const CTransaction& tx, CTxDB& txdb, uint64_t& nCoinAge); // ppcoin: get transaction coin age

int GetDepthInMainChain(const CTxIndex& txi);

#endif // BITCOIN_VALIDATION_H
