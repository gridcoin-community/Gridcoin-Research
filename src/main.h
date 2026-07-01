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

class CWallet;
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


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Genesis - MainNet - Production Genesis: as of 10-20-2014:
static const uint256 hashGenesisBlock = uint256S("0x000005a247b397eadfefa58e872bc967c2614797bdc8d4d0e6b09fea5c191599");

//TestNet Genesis:
static const uint256 hashGenesisBlockTestNet = uint256S("0x00006e037d7b84104208ecf2a8638d23149d712ea810da604ee2f2cb39bae713");

//RegTest Genesis (deterministic; computed from the regtest premine
//coinbase + nTime=1296688602 + nNonce=0 + nVersion=14 under trivial powLimit):
static const uint256 hashGenesisBlockRegTest = uint256S("0x4692b9564c585f76f41e02e304767b2ad16f1d18f72efcf0a724efe01e065371");
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


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
extern CCriticalSection cs_setpwalletRegistered;
extern std::set<CWallet*> setpwalletRegistered GUARDED_BY(cs_setpwalletRegistered);
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

void RegisterWallet(CWallet* pwalletIn);
void UnregisterWallet(CWallet* pwalletIn);
bool ProcessBlock(CNode* pfrom, CBlock* pblock, bool Generated_By_Me, CValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
// Block-file I/O helpers (CheckDiskSpace, OpenBlockFile, AppendBlockFile,
// LoadExternalBlockFile) live in node/blockstorage.h, included above.
// Takes cs_main internally; callers MUST NOT hold cs_main when calling
// (the internal LOCK would deadlock under non-recursive locking; cs_main
// is currently recursive but the annotation contract documents the intent).
bool LoadBlockIndex(bool fAllowNew=true);
void PrintBlockTree() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double CoinToDouble(double surrogate);

// ProcessMessages / SendMessages moved to net_processing.h (issue #2558 PR 2a).

//! Abandonment-style rewind of the chain to `pindex_target`. Updates the in-memory chain
//! globals (pindexBest, nBestHeight, hashBestChain, g_chain_trust, sync time) and persists
//! the new hashBestChain to LevelDB via the supplied CTxDB. Unlike DisconnectBlocksBatch,
//! this does NOT call DisconnectBlock on the abandoned range -- the on-disk data for those
//! blocks is by definition unreadable (we are recovering from corruption that made them
//! unhashable). The downstream cleanup of chainstate (CTxIndex / vSpent) and the in-memory
//! mapBlockIndex purge happen separately via CTxDB::CleanAbandonedRange and
//! PurgeOrphanedBlockIndexEntries below. Phase 2 of issue #2865; see src/node/coherence.cpp
//! and doc/block_corruption_recovery_design.md.
bool AbandonChainTo(class CBlockIndex* pindex_target, class CTxDB& txdb) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Purge the abandoned CBlockIndex entries from in-memory mapBlockIndex AND from on-disk
//! LevelDB (CDiskBlockIndex records). Called by the Phase 2 abandonment path after
//! AbandonChainTo + CTxDB::CleanAbandonedRange. The caller must guarantee that no consumer
//! holds references to the abandoned entries -- in the Phase 2 hook this is true by
//! construction (runs after LoadBlockIndex, before GRC::Initialize -- the wallet, Quorum,
//! Tally, mempool, and net have not yet started).
//!
//! Erases the mapBlockIndex slot and the LevelDB CDiskBlockIndex record. Does NOT delete
//! the CBlockIndex object: the index objects are allocated out of GRC::BlockIndexPool,
//! which never reclaims slots (see src/gridcoin/block_index.h:54-55 and
//! .claude/memory/reference_block_index_pool.md). Calling `delete` on a pool slot is
//! undefined behaviour and corrupts the heap free list -- the symptom (originally hit
//! 2026-05-16 on isolated testnet) is `assert(!pindexBest->pnext)` firing in
//! DisconnectBlocksBatch on the first P2P block delivered after Phase 2 reported clean
//! completion, because heap corruption rewrites neighbouring pool memory at allocation
//! time. The pool slot leaks by design (a few hundred bytes per discarded entry,
//! permanently); the entry becomes inert because nothing in mapBlockIndex points at it
//! and the on-disk record is gone.
//!
//! The LevelDB erase is what makes Phase 2 durable across restarts -- without it,
//! LoadBlockIndex would rebuild the same ghost pnext linkage from the stale on-disk
//! hashNext values, causing the recovered tip to appear corrupt again on every boot.
//!
//! The input vector entries are nulled out after the map erase to make it harder to
//! accidentally dereference a still-live pool pointer.
//!
//! Doing this at startup-init is safe; doing it at runtime would require coordinating
//! with live consumers (e.g. blockindex iteration in RPC handlers, Quorum lookups),
//! which is why the runtime DisconnectBlocksBatch path does NOT also purge -- it leaves
//! orphans in mapBlockIndex and accepts the small live-state weight.
void PurgeOrphanedBlockIndexEntries(class CTxDB& txdb, std::vector<class CBlockIndex*>& abandoned)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

GRC::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

int GetNumBlocksOfPeers();
bool IsInitialBlockDownload();
std::string GetWarnings(std::string strFor);
bool GetTransaction(const uint256 &hash, CTransaction &tx, uint256 &hashBlock);
void ResendWalletTransactions(bool fForce = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_setpwalletRegistered);
// Persists the best-block locator to registered wallets. Now also called from
// node/chainman.cpp's SetBestChain, so it has external linkage (issue #3030 A4).
void SetBestChain(const CBlockLocator& loc) EXCLUSIVE_LOCKS_REQUIRED(cs_setpwalletRegistered);
bool OutOfSyncByAge();

/** (try to) add transaction to memory pool **/
bool AcceptToMemoryPool(CTxMemPool& pool, CTransaction &tx,
                        CValidationState& state, bool* pfMissingInputs,
                        int64_t entry_time = 0, bool test_only = false,
                        CAmount* fee_out = nullptr, size_t* vsize_out = nullptr)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);


/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
protected:
    virtual int GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const;
public:
    uint256 hashBlock;
    int nIndex;

    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    virtual ~CMerkleTx() = default;

    void Init()
    {
        hashBlock.SetNull();
        nIndex = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        std::vector<uint256> dummy_vector1; //!< Used to be vMerkleBranch
        READWRITEAS(CTransaction, *this);
        READWRITE(hashBlock);
        READWRITE(dummy_vector1);
        READWRITE(nIndex);
    }

    int SetMerkleBranch(const CBlock* pblock = nullptr);

    // Return depth of transaction in blockchain:
    // -1  : not in blockchain, and not in memory pool (conflicted transaction)
    //  0  : in memory pool, waiting to be included in a block
    // >=1 : this many blocks deep in the main chain
    int GetDepthInMainChain(CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChainINTERNAL(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    bool AcceptToMemoryPool();
};

#endif
