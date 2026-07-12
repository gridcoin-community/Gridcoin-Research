// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#include "amount.h"
#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "index/disktxpos.h"
#include "index/txindex.h"
#include "util.h"
#include "net.h"
#include "node/blockstorage.h"
#include "gridcoin/block_index.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/cpid.h"
#include "primitives/transaction.h"
#include "primitives/block.h"
#include "sync.h"
#include "script.h"
#include "scrypt.h"
#include "txmempool.h"
#include "validation.h"

#include <map>
#include <unordered_map>
#include <set>

class CBlock;
class CBlockIndex;
class CKeyItem;
class CReserveKey;
class COutPoint;
class CAddress;
class CInv;
class CNode;

namespace GRC {
class Claim;
class SuperblockPtr;
class MRC;
//!
//! \brief An optional type that either contains some claim object or does not.
//!
typedef std::optional<Claim> ClaimOption;
}


// The genesis block-hash constants (hashGenesisBlock, hashGenesisBlockTestNet,
// hashGenesisBlockRegTest) live in chainparams.h, included above, next to
// CreateGenesisBlock (issue #3125, workstream C5).

// BlockHasher, BlockMap, cs_main and the active-chain-state globals
// (mapBlockIndex, pindexBest, hashBestChain, nBestHeight, ...) now live in
// chain.h, included above (issue #3030, workstream A6). FutureDrift and
// GetTargetSpacing live in primitives/block.h (extracted in #3060).

extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_tx_val_commit_to_disk;
extern unsigned int nStakeMinAge;
extern unsigned int nStakeMaxAge;
extern unsigned int nNodeLifespan;
extern int nCoinbaseMaturity;
extern const std::string strMessageMagic;
// Orphan block storage is managed by g_orphan_blocks in node/orphan_blocks.h

// Settings
extern int64_t nTransactionFee;
extern int64_t nReserveBalance;
extern int64_t nMinimumInputValue;

extern bool fUseFastIndex;
extern unsigned int nDerivationMethodIndex;

extern bool fEnforceCanonical;

//! \brief Guards \ref msMiningErrors. Written by Researcher::StoreResearcher
//! on the GUI / timer thread, read by getmininginfo RPC and the Qt researcher
//! model on their respective threads. std::string assignment / copy is not
//! atomic, so the writer's release of internal buffer storage can race with a
//! reader's traversal of the same buffer — undefined behaviour without
//! serialization.
extern CCriticalSection cs_msMiningErrors;
extern std::string msMiningErrors GUARDED_BY(cs_msMiningErrors);

extern int nGrandfather;

class CReserveKey;
class CTxDB;
class CTxIndex;

// ProcessBlock, AskForOutstandingBlocks and GridcoinServices live in
// node/chainman.h (issue #3125, workstream C1); callers include it directly
// (a main.h re-include would create a main <-> node/chainman module cycle).
// Block-file I/O helpers (CheckDiskSpace, OpenBlockFile, AppendBlockFile,
// LoadExternalBlockFile) and LoadBlockIndex live in node/blockstorage.h,
// included above (issue #3125, workstream C5).
void PrintBlockTree() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double CoinToDouble(double surrogate);

// ProcessMessages / SendMessages moved to net_processing.h (issue #2558 PR 2a).

// AbandonChainTo / PurgeOrphanedBlockIndexEntries live in node/coherence.h
// with the rest of the #2865 recovery logic (issue #3125, workstream C4).

GRC::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

int GetNumBlocksOfPeers();
bool IsInitialBlockDownload();
// GetWarnings lives in alert.h (issue #3125, workstream C6). GetTransaction
// lives in validation.h, included above, and CMerkleTx in wallet/wallet.h --
// only the wallet derives from it (issue #3125, workstream C3).
bool OutOfSyncByAge();

/** (try to) add transaction to memory pool **/
bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx,
                        CValidationState& state, bool* pfMissingInputs,
                        int64_t entry_time = 0, bool test_only = false,
                        CAmount* fee_out = nullptr, size_t* vsize_out = nullptr)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);



#endif
