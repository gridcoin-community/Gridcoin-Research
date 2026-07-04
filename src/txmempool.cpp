// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "txmempool.h"

#include "consensus/tx_verify.h"
#include "gridcoin/beacon.h"
#include "gridcoin/mrc.h"
#include "sync.h"

CTxMemPool mempool;

CTxMemPoolEntry::CTxMemPoolEntry(const CTransaction& tx_in, CAmount fee, int64_t time,
                                 int height, size_t tx_size)
    : tx(tx_in)
    , nFee(fee)
    , nTxSize(tx_size)
    , nTime(time)
    , entryHeight(height)
    , nSigOps(GetLegacySigOpCount(tx_in))
{
    // Single pass over the transaction's contracts to populate the metadata tags
    // that Phase 2 will index on. By protocol a transaction carries at most one
    // contract, but we tolerate more without assuming an order.
    for (const auto& contract : tx.GetContracts()) {
        m_contract_types.insert(contract.m_type.Value());

        switch (contract.m_type.Value()) {
        case GRC::ContractType::MRC: {
            GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();
            m_has_mrc = true;
            if (auto cpid = mrc.m_mining_id.TryCpid()) {
                m_mrc_cpid = *cpid;
            }
            m_mrc_fee = mrc.m_fee;
            m_mrc_last_block_hash = mrc.m_last_block_hash;
            break;
        }
        case GRC::ContractType::BEACON: {
            GRC::BeaconPayload beacon = contract.CopyPayloadAs<GRC::BeaconPayload>();
            m_has_beacon = true;
            m_beacon_cpid = beacon.m_cpid;
            break;
        }
        case GRC::ContractType::SIDESTAKE: {
            m_has_mandatory_sidestake = true;
            break;
        }
        default:
            break;
        }
    }
}

bool CTxMemPool::addUnchecked(const uint256& hash, const CTxMemPoolEntry& entry)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptToMemoryPool to properly check the transaction first.
    {
        // Caller contract: AcceptToMemoryPool holds cs and has already rejected a
        // tx that exists() in the pool, so this is a fresh txid. insert_or_assign
        // would only "overwrite" on a duplicate txid -- and an identical txid
        // implies identical vins, so re-pointing mapNextTx below to the new node
        // is still correct. (Other mutators lock internally; this one relies on
        // the caller holding cs, matching the "unchecked = caller-locked" idiom.)
        auto it = mapTx.insert_or_assign(hash, entry).first;
        // Point mapNextTx at the transaction stored inside the pool's own map
        // node (std::map never relocates nodes, so the pointer stays valid until
        // the entry is removed).
        CTransaction& stored_tx = it->second.GetTxMutable();
        for (unsigned int i = 0; i < stored_tx.vin.size(); i++)
            mapNextTx[stored_tx.vin[i].prevout] = CInPoint(&stored_tx, i);

        // Maintain the secondary contract indexes from the cached tags. These rely
        // on the same fresh-txid contract as above: the CPID-keyed maps overwrite
        // harmlessly on a same-txid re-add (identical CPID), but m_mrc_by_fee is a
        // multimap whose emplace would double-insert -- the upstream exists() guard
        // is what prevents a duplicate add from leaving a stale fee row.
        const CTxMemPoolEntry& e = it->second;
        if (e.HasMRC()) {
            m_mrc_by_cpid.insert_or_assign(e.GetMRCCpid(), hash);
            m_mrc_by_fee.emplace(e.GetMRCFee(), e.GetMRCCpid());
        }
        if (e.HasBeacon())
            m_beacon_by_cpid.insert_or_assign(e.GetBeaconCpid(), hash);
        if (e.HasMandatorySidestake())
            ++m_mandatory_sidestake_count;

        // Size accounting + eviction ordering.
        m_total_tx_size += e.GetTxSize();
        m_eviction_index.insert(EvictionKeyFor(e));
    }
    return true;
}

void CTxMemPool::eraseIndexes(const CTxMemPoolEntry& entry)
{
    if (entry.HasMRC()) {
        m_mrc_by_cpid.erase(entry.GetMRCCpid());
        auto range = m_mrc_by_fee.equal_range(entry.GetMRCFee());
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == entry.GetMRCCpid()) {
                m_mrc_by_fee.erase(it);
                break;
            }
        }
    }
    if (entry.HasBeacon())
        m_beacon_by_cpid.erase(entry.GetBeaconCpid());
    if (entry.HasMandatorySidestake() && m_mandatory_sidestake_count > 0)
        --m_mandatory_sidestake_count;

    // Size accounting + eviction ordering. add/erase are balanced in practice
    // (every addUnchecked is matched by exactly one erase), so this cannot
    // underflow; clamp to 0 anyway -- rather than wrapping size_t or leaving the
    // counter stuck high -- so it stays self-correcting if it ever desynced.
    m_total_tx_size = (m_total_tx_size >= entry.GetTxSize())
                          ? m_total_tx_size - entry.GetTxSize()
                          : 0;
    m_eviction_index.erase(EvictionKeyFor(entry));

    // A tx leaving the pool (confirmed, evicted, or conflicted) is no longer ours
    // to rebroadcast.
    m_unbroadcast.erase(entry.GetTx().GetHash());
}


bool CTxMemPool::remove(const CTransaction &tx, bool fRecursive, MemPoolRemovalReason reason)
{
    // NOTE: `reason` is threaded through (including into the recursive call below)
    // as groundwork for a future TransactionRemovedFromMempool validation signal.
    // No subscriber consumes it yet, so it has no observable effect today.

    // Remove transaction from memory pool
    {
        LOCK(cs);
        uint256 hash = tx.GetHash();
        auto entry_it = mapTx.find(hash);
        if (entry_it != mapTx.end())
        {
            if (fRecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                        remove(*it->second.ptx, true, reason);
                }
            }
            // Tear down the secondary indexes before erasing the entry.
            eraseIndexes(entry_it->second);
            for (auto const& txin : tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(entry_it);
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction &tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    for (auto const &txin : tx.vin)
    {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction &txConflict = *it->second.ptx;
            if (txConflict != tx)
                remove(txConflict, true);
        }
    }
    return true;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
    m_mrc_by_cpid.clear();
    m_beacon_by_cpid.clear();
    m_mandatory_sidestake_count = 0;
    m_mrc_by_fee.clear();
    m_total_tx_size = 0;
    m_eviction_index.clear();
    m_unbroadcast.clear();
}

void CTxMemPool::TrimToSize(size_t limit, std::vector<CTransaction>* removed,
                            const uint256* protect)
{
    LOCK(cs);

    while (DynamicMemoryUsage() > limit) {
        // Lowest-priority entry is the next victim, but never evict the protected
        // transaction (the just-accepted one) -- skip it and take the next.
        auto key_it = m_eviction_index.begin();
        if (protect && key_it != m_eviction_index.end() && key_it->hash == *protect)
            ++key_it;
        if (key_it == m_eviction_index.end())
            break; // nothing evictable remains but the protected tx (it alone is over-limit)

        const uint256 victim_hash = key_it->hash;

        auto it = mapTx.find(victim_hash);
        if (it == mapTx.end()) {
            // Defensive: drop a stale key that has no backing entry.
            m_eviction_index.erase(key_it);
            continue;
        }

        const CTransaction victim = it->second.GetTx();
        if (removed) removed->push_back(victim);

        // Route through remove() so every index (and the running size total)
        // stays consistent. Recursive so dependents go too.
        remove(victim, /*fRecursive=*/true, MemPoolRemovalReason::SIZELIMIT);
    }
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (auto mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back(mi->first);
}
