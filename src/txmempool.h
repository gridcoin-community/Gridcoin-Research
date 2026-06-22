// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXMEMPOOL_H
#define BITCOIN_TXMEMPOOL_H

#include "primitives/transaction.h"
#include "sync.h"
#include "uint256.h"

#include <cstdint>
#include <map>
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

class CTxMemPool
{
public:
    mutable CCriticalSection cs;
    std::map<uint256, CTransaction> mapTx;
    std::map<COutPoint, CInPoint> mapNextTx;
    uint64_t m_mrc_bloom{0};
    bool m_mrc_bloom_dirty{false};

    bool addUnchecked(const uint256& hash, CTransaction &tx);
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
        std::map<uint256, CTransaction>::const_iterator i = mapTx.find(hash);
        if (i == mapTx.end()) return false;
        result = i->second;
        return true;
    }
};

extern CTxMemPool mempool;

#endif // BITCOIN_TXMEMPOOL_H
