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
    }
    return true;
}


bool CTxMemPool::remove(const CTransaction &tx, bool fRecursive)
{
    m_mrc_bloom_dirty = true;
    // Remove transaction from memory pool
    {
        LOCK(cs);
        uint256 hash = tx.GetHash();
        if (mapTx.count(hash))
        {
            if (fRecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                        remove(*it->second.ptx, true);
                }
            }
            for (auto const& txin : tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction &tx)
{
    m_mrc_bloom_dirty = true;
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
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (auto mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back(mi->first);
}
