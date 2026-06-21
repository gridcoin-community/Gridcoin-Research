// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXMEMPOOL_H
#define BITCOIN_TXMEMPOOL_H

#include "amount.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/cpid.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "uint256.h"

#include <cstdint>
#include <map>
#include <set>
#include <vector>

/** Reason why transaction was removed from mempool */
enum class MemPoolRemovalReason {
    UNKNOWN = 0,      //!< Manually removed or unknown reason
    EXPIRY = 1,       //!< Expired from mempool
    SIZELIMIT = 2,    //!< Removed due to size limit
    REORG = 3,        //!< Removed for reorganization
    BLOCK = 4,        //!< Removed because included in block
    CONFLICT = 5,     //!< Removed due to conflict
    REPLACED = 6      //!< Removed due to replacement (RBF)
};

//!
//! \brief A transaction stored in the memory pool together with the metadata
//! computed once when it was accepted.
//!
//! Phase 1 of the mempool modernization (#3029). The entry caches the fee, size,
//! time, and entry height that callers previously recomputed, plus lightweight
//! Gridcoin contract tags (contract type(s); CPID and fee for MRC/BEACON; the
//! mandatory-sidestake flag) derived from a single pass over the transaction's
//! contracts. The tags are populated here but not yet consumed -- the O(n)
//! contract scans they will replace remain in place until Phase 2 swaps them for
//! index lookups.
//!
class CTxMemPoolEntry
{
private:
    CTransaction tx;
    CAmount nFee;            //!< GetValueIn() - GetValueOut(), at accept time.
    size_t nTxSize;          //!< Serialized size (SER_NETWORK, PROTOCOL_VERSION).
    int64_t nTime;           //!< Adjusted time when accepted.
    int entryHeight;         //!< Chain height when accepted.
    unsigned int nSigOps;    //!< Legacy sig-op count.

    // Gridcoin contract tags -- computed in the constructor (txmempool.cpp).
    std::set<GRC::ContractType> m_contract_types;
    bool m_has_mrc{false};
    GRC::Cpid m_mrc_cpid;
    CAmount m_mrc_fee{0};
    uint256 m_mrc_last_block_hash;
    bool m_has_beacon{false};
    GRC::Cpid m_beacon_cpid;
    bool m_has_mandatory_sidestake{false};

public:
    CTxMemPoolEntry(const CTransaction& tx, CAmount fee, int64_t time,
                    int height, size_t tx_size);

    const CTransaction& GetTx() const { return tx; }
    //! Non-const access for addUnchecked() to wire mapNextTx pointers into the
    //! pool's stable map node. Not for general use.
    CTransaction& GetTxMutable() { return tx; }

    CAmount GetFee() const { return nFee; }
    size_t GetTxSize() const { return nTxSize; }
    int64_t GetTime() const { return nTime; }
    int GetHeight() const { return entryHeight; }
    unsigned int GetSigOps() const { return nSigOps; }
    //! Fee per 1000 bytes. No CFeeRate type exists in this tree; returns a plain
    //! CAmount derived from the cached fee and size.
    CAmount GetFeeRate() const { return nTxSize ? nFee * 1000 / (CAmount)nTxSize : 0; }

    const std::set<GRC::ContractType>& GetContractTypes() const { return m_contract_types; }
    bool HasMRC() const { return m_has_mrc; }
    const GRC::Cpid& GetMRCCpid() const { return m_mrc_cpid; }
    CAmount GetMRCFee() const { return m_mrc_fee; }
    const uint256& GetMRCLastBlockHash() const { return m_mrc_last_block_hash; }
    bool HasBeacon() const { return m_has_beacon; }
    const GRC::Cpid& GetBeaconCpid() const { return m_beacon_cpid; }
    bool HasMandatorySidestake() const { return m_has_mandatory_sidestake; }
};

class CTxMemPool
{
public:
    mutable CCriticalSection cs;
    std::map<uint256, CTxMemPoolEntry> mapTx;
    std::map<COutPoint, CInPoint> mapNextTx;
    uint64_t m_mrc_bloom{0};
    bool m_mrc_bloom_dirty{false};

    bool addUnchecked(const uint256& hash, const CTxMemPoolEntry& entry);
    bool remove(const CTransaction &tx, bool fRecursive = false);
    bool removeConflicts(const CTransaction &tx);
    void clear();
    void queryHashes(std::vector<uint256>& vtxid);

    unsigned long size() const
    {
        LOCK(cs);
        return mapTx.size();
    }

    bool exists(uint256 hash) const
    {
        LOCK(cs);
        return (mapTx.count(hash) != 0);
    }

    bool lookup(uint256 hash, CTransaction& result) const
    {
        LOCK(cs);
        auto i = mapTx.find(hash);
        if (i == mapTx.end()) return false;
        result = i->second.GetTx();
        return true;
    }
};

extern CTxMemPool mempool;

#endif // BITCOIN_TXMEMPOOL_H
