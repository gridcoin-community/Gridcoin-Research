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
#include <functional>
#include <map>
#include <set>
#include <utility>
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
//! \brief Ordering key for size-based eviction (#3029 Phase 4).
//!
//! The eviction set's first element is always the next victim. Gridcoin's flat,
//! size-scaled fee makes feerate nearly identical across transactions, so the
//! order is: non-contract transactions before contract transactions (contracts
//! are evicted last so reward-bearing MRCs survive), then lowest feerate, then
//! newest-first among ties, then hash for uniqueness.
//!
struct CTxMemPoolEvictionKey
{
    bool is_contract;
    CAmount feerate;
    int64_t time;
    uint256 hash;

    bool operator<(const CTxMemPoolEvictionKey& other) const
    {
        if (is_contract != other.is_contract) return is_contract < other.is_contract;
        if (feerate != other.feerate) return feerate < other.feerate;
        if (time != other.time) return time > other.time;
        return hash < other.hash;
    }
};

//!
//! \brief A transaction stored in the memory pool together with the metadata
//! computed once when it was accepted.
//!
//! Phase 1 of the mempool modernization (#3029). The entry caches the fee, size,
//! time, and entry height that callers previously recomputed, plus lightweight
//! Gridcoin contract tags (contract type(s); the BEACON CPID; the MRC CPID, fee,
//! and last-block-hash; the mandatory-sidestake flag) derived from a single pass
//! over the transaction's
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
    //! Non-const access to the stored transaction, used ONLY to obtain a stable
    //! CTransaction* for legacy non-const APIs: addUnchecked()'s mapNextTx wiring
    //! and the miner's COrphan / vecPriority structures (which hold CTransaction*).
    //! Callers MUST NOT mutate the transaction -- doing so would silently
    //! invalidate this entry's cached fee/size/sigops/contract-tag metadata.
    //! (Making those legacy consumers const-correct so this can be removed is a
    //! tracked follow-up.)
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

//! Aggregate mempool statistics, collected under a single lock for getmempoolinfo.
struct CTxMemPoolInfo
{
    size_t tx_count{0};
    size_t bytes{0};
    size_t usage{0};
    size_t max_size{0};
    size_t mrc_count{0};
    size_t beacon_count{0};
    size_t mandatory_sidestake_count{0};
};

class CTxMemPool
{
public:
    mutable CCriticalSection cs;
    std::map<uint256, CTxMemPoolEntry> mapTx;
    std::map<COutPoint, CInPoint> mapNextTx;

    // Secondary indexes maintained in the single add/remove/clear paths (#3029
    // Phase 2). They replace the former O(n) full-pool contract scans. Keyed off
    // the contract tags cached on CTxMemPoolEntry.
    //! One MRC per CPID; value is the tx hash. Sound as a CPID-keyed map because
    //! AcceptToMemoryPool DoS-rejects a second MRC for a CPID already in the pool,
    //! so mapTx never holds two same-CPID MRCs.
    std::map<GRC::Cpid, uint256> m_mrc_by_cpid;
    //! Beacon advertisement by CPID. EXISTENCE-ONLY index (queried via
    //! HasBeaconForCpid()'s count(); the stored hash is never read). Unlike MRCs,
    //! duplicate PENDING beacons for one CPID are NOT rejected at acceptance (only
    //! a wallet-side viability check guards locally), so two same-CPID beacons can
    //! coexist in mapTx; this map then holds only the last (insert_or_assign) and
    //! erase(cpid) drops the entry while a sibling may remain. That desync is
    //! harmless here -- the worst case is HasBeaconForCpid() letting the wallet-side
    //! guard pass a duplicate that the protocol already permits. Do NOT add a
    //! consumer that reads the stored hash or treats this as 1:1 without switching
    //! to a multimap.
    std::map<GRC::Cpid, uint256> m_beacon_by_cpid;
    size_t m_mandatory_sidestake_count{0};         //!< Mandatory-sidestake txs in the pool.
    //! MRCs in fee-descending order, for the GUI/RPC queue ranking.
    std::multimap<CAmount, GRC::Cpid, std::greater<CAmount>> m_mrc_by_fee;

    // Size accounting + eviction (#3029 Phase 4).
    size_t m_total_tx_size{0};                        //!< Running sum of entry serialized sizes.
    std::set<CTxMemPoolEvictionKey> m_eviction_index; //!< Victim ordering; begin() is evicted first.
    size_t m_max_size_bytes{300 * 1000 * 1000};       //!< -maxmempool cap in bytes (default 300 MB).

    //! Estimated per-entry bookkeeping overhead (CTxMemPoolEntry + map/index nodes)
    //! added on top of the serialized size in DynamicMemoryUsage().
    static constexpr size_t PER_ENTRY_OVERHEAD = 256;

    bool addUnchecked(const uint256& hash, const CTxMemPoolEntry& entry);
    bool remove(const CTransaction &tx, bool fRecursive = false,
                MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);
    bool removeConflicts(const CTransaction &tx);
    void clear();
    void queryHashes(std::vector<uint256>& vtxid);

    //! \brief Estimated dynamic memory used by the pool and its indexes.
    size_t DynamicMemoryUsage() const
    {
        LOCK(cs);
        return m_total_tx_size + mapTx.size() * PER_ENTRY_OVERHEAD;
    }

    //! \brief Evict transactions until DynamicMemoryUsage() is at or below \p limit,
    //! lowest-priority first (see CTxMemPoolEvictionKey). Routes every eviction
    //! through remove() so all indexes stay consistent.
    void TrimToSize(size_t limit, std::vector<CTransaction>* removed = nullptr);

    size_t GetMaxSize() const { LOCK(cs); return m_max_size_bytes; }
    void SetMaxSize(size_t bytes) { LOCK(cs); m_max_size_bytes = bytes; }

    //! \brief Whether the pool already holds an MRC for \p cpid. When it does and
    //! \p existing is non-null, the colliding transaction hash is written there.
    bool HasMRCForCpid(const GRC::Cpid& cpid, uint256* existing = nullptr) const
    {
        LOCK(cs);
        auto it = m_mrc_by_cpid.find(cpid);
        if (it == m_mrc_by_cpid.end()) return false;
        if (existing) *existing = it->second;
        return true;
    }

    bool HasBeaconForCpid(const GRC::Cpid& cpid) const
    {
        LOCK(cs);
        return m_beacon_by_cpid.count(cpid) != 0;
    }

    bool HasMandatorySidestake() const
    {
        LOCK(cs);
        return m_mandatory_sidestake_count != 0;
    }

    //! \brief A fee-descending snapshot of the pooled MRCs as (fee, cpid) pairs.
    std::vector<std::pair<CAmount, GRC::Cpid>> GetMRCQueue() const
    {
        LOCK(cs);
        std::vector<std::pair<CAmount, GRC::Cpid>> queue;
        queue.reserve(m_mrc_by_fee.size());
        for (const auto& [fee, cpid] : m_mrc_by_fee) queue.emplace_back(fee, cpid);
        return queue;
    }

    //! \brief Aggregate counts/size for getmempoolinfo, collected in one locked pass.
    CTxMemPoolInfo GetMempoolInfo() const
    {
        LOCK(cs);
        CTxMemPoolInfo info;
        info.tx_count = mapTx.size();
        info.bytes = m_total_tx_size;
        info.usage = m_total_tx_size + mapTx.size() * PER_ENTRY_OVERHEAD;
        info.max_size = m_max_size_bytes;
        info.mrc_count = m_mrc_by_cpid.size();
        info.beacon_count = m_beacon_by_cpid.size();
        info.mandatory_sidestake_count = m_mandatory_sidestake_count;
        return info;
    }

    //! \brief Hashes of pooled MRCs whose anchor block is not \p best_block_hash.
    std::vector<uint256> GetStaleMRCs(const uint256& best_block_hash) const
    {
        LOCK(cs);
        std::vector<uint256> stale;
        for (const auto& [cpid, hash] : m_mrc_by_cpid) {
            auto it = mapTx.find(hash);
            if (it != mapTx.end() && it->second.GetMRCLastBlockHash() != best_block_hash)
                stale.push_back(hash);
        }
        return stale;
    }

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

private:
    //! Remove an entry's contributions from the secondary indexes, the running
    //! size total, and the eviction index. Caller holds cs.
    void eraseIndexes(const CTxMemPoolEntry& entry);

    //! Build the eviction-ordering key for an entry.
    static CTxMemPoolEvictionKey EvictionKeyFor(const CTxMemPoolEntry& entry)
    {
        return {!entry.GetContractTypes().empty(), entry.GetFeeRate(),
                entry.GetTime(), entry.GetTx().GetHash()};
    }
};

extern CTxMemPool mempool;

#endif // BITCOIN_TXMEMPOOL_H
