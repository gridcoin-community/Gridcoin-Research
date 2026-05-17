// Copyright (c) 2017-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <consensus/tx_verify.h>

#include <node/blockstorage.h>
#include <primitives/transaction.h>
#include "txdb.h"

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = nBestHeight;
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (auto const& txin : tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    for (auto const& txin : tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (auto const& txout : tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const MapPrevTx& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& prevout = GetOutputFor(tx.vin[i], inputs);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

/**
 * Walk the pprev chain from the given block to find the ancestor at nHeight.
 */
static const CBlockIndex* GetAncestor(const CBlockIndex* pindex, int nHeight)
{
    if (nHeight < 0 || !pindex || nHeight > pindex->nHeight) {
        return nullptr;
    }

    const CBlockIndex* pindexWalk = pindex;
    while (pindexWalk && pindexWalk->nHeight != nHeight) {
        pindexWalk = pindexWalk->pprev;
    }
    return pindexWalk;
}

std::pair<int, int64_t> CalculateSequenceLocks(
    const CTransaction& tx, int flags,
    std::vector<int>& prevHeights, const CBlockIndex& block)
{
    assert(prevHeights.size() == tx.vin.size());

    // BIP68 sequence locks are only enforced for tx version >= 2.
    // For older transactions, return locks that are always satisfied.
    if (tx.nVersion < 2) {
        return std::make_pair(0, -1);
    }

    int nMinHeight = -1;
    int64_t nMinTime = -1;

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const uint32_t nSequence = tx.vin[txinIndex].nSequence;

        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning.
        if (nSequence & SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            // The height of this input is not relevant for sequence locks.
            prevHeights[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = prevHeights[txinIndex];

        if (nSequence & SEQUENCE_LOCKTIME_TYPE_FLAG) {
            // Time-based relative lock-time: the lock is specified in
            // units of 512 seconds. We compare against the median time
            // past of the block *before* the one that confirmed this
            // input.
            const CBlockIndex* pbi = GetAncestor(block.pprev, std::max(nCoinHeight - 1, 0));
            int64_t nCoinTime = pbi ? pbi->GetMedianTimePast() : 0;

            // Time-based nSequence: add the relative offset.
            nMinTime = std::max(nMinTime,
                nCoinTime + (int64_t)((nSequence & SEQUENCE_LOCKTIME_MASK) << SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            // Height-based relative lock-time.
            nMinHeight = std::max(nMinHeight,
                nCoinHeight + (int)(nSequence & SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

bool EvaluateSequenceLocks(const CBlockIndex& block,
                           std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);

    int64_t nBlockTime = block.pprev->GetMedianTimePast();

    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime) {
        return false;
    }

    return true;
}

/**
 * Helper: given a CTxIndex (which contains the disk position of the
 * transaction), find the height of the block that contains it.
 * Returns 0 on failure.
 */
static int GetHeightForTxIndex(const CTxIndex& txindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CBlock block;
    if (!ReadBlockFromDisk(block, txindex.pos.nFile, txindex.pos.nBlockPos,
                           Params().GetConsensus(), false))
    {
        return 0;
    }

    BlockMap::iterator mi = mapBlockIndex.find(block.GetHash(true));
    if (mi == mapBlockIndex.end()) {
        return 0;
    }

    CBlockIndex* pindex = mi->second;
    if (!pindex || !pindex->IsInMainChain()) {
        return 0;
    }

    return pindex->nHeight;
}

bool CheckSequenceLocks(const CTransaction& tx, int flags,
                        const CBlockIndex* pindexPrev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    // BIP68 sequence locks are only enforced for tx version >= 2.
    if (tx.nVersion < 2) {
        return true;
    }

    // Build prevHeights: for each input, find the height at which it was confirmed.
    std::vector<int> prevHeights(tx.vin.size());
    CTxDB txdb("r");

    for (size_t i = 0; i < tx.vin.size(); i++) {
        const COutPoint& prevout = tx.vin[i].prevout;

        CTransaction txPrev;
        CTxIndex txindex;

        if (!ReadTxFromDisk(txPrev, txdb, prevout, txindex)) {
            // If the input isn't on disk, it might be in the mempool
            // (unconfirmed). Treat it as confirmed at the next block.
            prevHeights[i] = pindexPrev->nHeight + 1;
            continue;
        }

        prevHeights[i] = GetHeightForTxIndex(txindex);
        if (prevHeights[i] == 0) {
            // Could not determine height; treat as unconfirmed.
            prevHeights[i] = pindexPrev->nHeight + 1;
        }
    }

    // Construct a CBlockIndex for the block being evaluated
    // (pindexPrev->nHeight + 1).
    CBlockIndex index;
    index.pprev = const_cast<CBlockIndex*>(pindexPrev);
    index.nHeight = pindexPrev->nHeight + 1;

    std::pair<int, int64_t> lockPair = CalculateSequenceLocks(tx, flags, prevHeights, index);
    return EvaluateSequenceLocks(index, lockPair);
}
