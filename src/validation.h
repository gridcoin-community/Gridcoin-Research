// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#include <atomic>
#include "amount.h"
#include "consensus/validation.h"
#include "index/disktxpos.h"
#include "index/txindex.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "util.h"  // Fraction

#include <map>

class CTxDB;
class CBlockHeader;
class CTxMemPool;
extern CCriticalSection cs_main;
namespace Consensus {
    struct Params;
}
namespace GRC {
    class MRC;
}

typedef std::map<uint256, std::pair<CTxIndex, CTransaction>> MapPrevTx;

//
// Sync / chain-progress state, moved out of main.{h,cpp} (issue #3125 C9).
//

//! Serializes the transaction-validation commit-to-disk window (issue #2865).
extern CCriticalSection cs_tx_val_commit_to_disk;

//! Coinbase/coinstake maturity depth.
extern int nCoinbaseMaturity;

//! When syncing, block-rejection rules are grandfathered up to this height,
//! as rules became stricter over time and fields changed.
extern int nGrandfather;

//! Enforce canonical script pushes for standardness.
extern bool fEnforceCanonical;

//! Temporary block-version-11 transition helper.
extern int64_t g_v11_timestamp;

//! Median of the block heights that connected peers claim to have, floored
//! at the hardened-checkpoint height.
int GetNumBlocksOfPeers();

//! Heuristic: still syncing the historical chain (peer-height median and
//! tip-age based).
bool IsInitialBlockDownload();

//! Heuristic: the tip is older than ten target block spacings.
bool OutOfSyncByAge();

bool ReadTxFromDisk(CTransaction& tx, CDiskTxPos pos, FILE** pfileRet = nullptr);
bool ReadTxFromDisk(CTransaction& tx, CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);
bool ReadTxFromDisk(CTransaction& tx, CTxDB& txdb, COutPoint prevout);
bool ReadTxFromDisk(CTransaction& tx, COutPoint prevout);

bool CheckTransaction(const CTransaction& tx, CValidationState& state);

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
bool CheckContracts(const CTransaction& tx, CValidationState& state, const MapPrevTx& inputs, int block_height);

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
bool HasMasterKeyInput(const CTransaction& tx, const MapPrevTx& inputs, int block_height);

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

bool DisconnectInputs(CTransaction& tx, CTxDB& txdb) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

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
// No EXCLUSIVE_LOCKS_REQUIRED(cs_main): FetchInputs reads only txdb (its
// own subsystem) and mempool (which takes mempool.cs internally via
// mempool.lookup); it does not touch mapBlockIndex / pindexBest /
// nBestHeight. Block-processing callers (ConnectInputs / CreateNewBlock)
// hold cs_main for other reasons, but FetchInputs itself is callable
// from RPC paths (signrawtransaction*) without cs_main.
bool FetchInputs(CTransaction& tx, CValidationState& state, CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool, bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid);

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
//! Count of signature verifications performed inside ConnectInputs().
//!
//! Exists so the ordering guarantee is testable: the conflict pre-scan means a
//! transaction with an already-spent input must cost ZERO verifications, and
//! without a counter that is only observable by instrumenting a build. A
//! regression that moved the conflict check back inside the signature loop
//! would otherwise still pass every test, since the transaction is rejected
//! either way -- only the cost differs.
extern std::atomic<uint64_t> g_connectinputs_signature_checks;

bool ConnectInputs(CTransaction& tx, CValidationState& state, CTxDB& txdb, MapPrevTx inputs, std::map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx, const CBlockIndex* pindexBlock, bool fBlock, bool fMiner) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

bool GetCoinAge(const CTransaction& tx, CTxDB& txdb, uint64_t& nCoinAge) EXCLUSIVE_LOCKS_REQUIRED(cs_main); // ppcoin: get transaction coin age

int GetDepthInMainChain(const CTxIndex& txi) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Look a transaction up by hash, first in the mempool and then in the txdb.
//! If it was found inside a block, its hash is placed in hashBlock. Takes
//! cs_main internally. Moved from main.cpp (issue #3125, workstream C3).
bool GetTransaction(const uint256& hash, CTransaction& tx, uint256& hashBlock);

/** (try to) add transaction to memory pool. Moved from main.{h,cpp}
 * (issue #3125, workstream C2). **/
bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx,
                        CValidationState& state, bool* pfMissingInputs,
                        int64_t entry_time = 0, bool test_only = false,
                        CAmount* fee_out = nullptr, size_t* vsize_out = nullptr)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params);

bool DisconnectBlock(CBlock& block, CTxDB& txdb, CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool ConnectBlock(CBlock& block, CValidationState& state, CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck=false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool AddToBlockIndex(CBlock& block, unsigned int nFile, unsigned int nBlockPos, const uint256& hashProof) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool CheckBlock(const CBlock& block, CValidationState& state, int height1, bool fCheckPOW=true, bool fCheckMerkleRoot=true, bool fCheckSig=true, bool fLoadingIndex=false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool AcceptBlock(CBlock& block, CValidationState& state, bool generated_by_me) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool CheckBlockSignature(const CBlock& block);

// Returns the script flags which should be checked for a given block
unsigned int GetBlockScriptFlags(const CBlockIndex& block_index);

unsigned int GetCoinstakeOutputLimit(const int& block_version);
unsigned int GetMandatorySideStakeOutputLimit(const int& block_version);
Fraction FoundationSideStakeAllocation();
CTxDestination FoundationSideStakeAddress();
unsigned int GetMRCOutputLimit(const int& block_version, bool include_foundation_sidestake);
bool ValidateMRC(const GRC::Contract &contract, const CTransaction& tx, int& DoS) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
bool ValidateMRC(const CBlockIndex* mrc_last_pindex, const GRC::MRC& mrc) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

#endif // BITCOIN_VALIDATION_H
