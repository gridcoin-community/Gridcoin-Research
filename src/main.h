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
// LoadExternalBlockFile) live in node/blockstorage.h, included above.
// Takes cs_main internally; callers MUST NOT hold cs_main when calling
// (the internal LOCK would deadlock under non-recursive locking; cs_main
// is currently recursive but the annotation contract documents the intent).
bool LoadBlockIndex(bool fAllowNew=true);
void PrintBlockTree() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
double CoinToDouble(double surrogate);

// ProcessMessages / SendMessages moved to net_processing.h (issue #2558 PR 2a).

// AbandonChainTo / PurgeOrphanedBlockIndexEntries live in node/coherence.h
// with the rest of the #2865 recovery logic (issue #3125, workstream C4).

GRC::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

int GetNumBlocksOfPeers();
bool IsInitialBlockDownload();
// GetWarnings lives in alert.h (issue #3125, workstream C6).
bool GetTransaction(const uint256 &hash, CTransaction &tx, uint256 &hashBlock);
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
