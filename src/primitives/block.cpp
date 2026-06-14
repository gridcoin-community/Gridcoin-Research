// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "main.h"

// These methods are defined out-of-line (rather than inline in primitives/block.h)
// because they reference chain-state globals declared in main.h — pindexBest,
// mapBlockIndex, pindexGenesisBlock, fUseFastIndex, and the genesis-block hashes.
// Keeping them here lets primitives/block.h stay free of main.h. The
// EXCLUSIVE_LOCKS_REQUIRED(cs_main) annotations live on the in-class declarations
// in the header and are intentionally not repeated on these definitions.

bool CBlockIndex::IsInMainChain() const
{
    return (pnext || this == pindexBest);
}

uint256 CDiskBlockIndex::GetBlockHash() const
{
    if (fUseFastIndex && (nTime < GetAdjustedTime() - 24 * 60 * 60) && !blockHash.IsNull())
        return blockHash;

    CBlockHeader block;
    block.nVersion        = nVersion;
    block.hashPrevBlock   = hashPrev;
    block.hashMerkleRoot  = hashMerkleRoot;
    block.nTime           = nTime;
    block.nBits           = nBits;
    block.nNonce          = nNonce;

    const_cast<CDiskBlockIndex*>(this)->blockHash = block.GetHash();

    return blockHash;
}

CBlockLocator::CBlockLocator(uint256 hashBlock)
{
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi != mapBlockIndex.end())
        Set(mi->second);
}

void CBlockLocator::Set(const CBlockIndex* pindex)
{
    vHave.clear();
    int nStep = 1;
    while (pindex)
    {
        vHave.push_back(pindex->GetBlockHash());

        // Exponentially larger steps back
        for (int i = 0; pindex && i < nStep; i++)
            pindex = pindex->pprev;
        if (vHave.size() > 10)
            nStep *= 2;
    }
    vHave.push_back((Params().IsMockableChain() ? hashGenesisBlockRegTest : !fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
}

int CBlockLocator::GetDistanceBack()
{
    // Retrace how far back it was in the sender's branch
    int nDistance = 0;
    int nStep = 1;
    for (auto const& hash : vHave)
    {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = mi->second;
            if (pindex->IsInMainChain())
                return nDistance;
        }
        nDistance += nStep;
        if (nDistance > 10)
            nStep *= 2;
    }
    return nDistance;
}

CBlockIndex* CBlockLocator::GetBlockIndex()
{
    // Find the first block the caller has in the main chain
    for (auto const& hash : vHave)
    {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = mi->second;
            if (pindex->IsInMainChain())
                return pindex;
        }
    }
    return pindexGenesisBlock;
}

uint256 CBlockLocator::GetBlockHash()
{
    // Find the first block the caller has in the main chain
    for (auto const& hash : vHave)
    {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = mi->second;
            if (pindex->IsInMainChain())
                return hash;
        }
    }
    return (Params().IsMockableChain() ? hashGenesisBlockRegTest : !fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet);
}

int CBlockLocator::GetHeight()
{
    CBlockIndex* pindex = GetBlockIndex();
    if (!pindex)
        return 0;
    return pindex->nHeight;
}
