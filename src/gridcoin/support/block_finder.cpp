// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/support/block_finder.h"

using namespace GRC;

// TODO(#2869 Phase 2 — block_finder): nBestHeight / pindexGenesisBlock /
// pindexBest reads need cs_main. 29 call sites across the codebase, some
// already holding cs_main and some not. Annotate as required + audit callers
// in Phase 2 rather than cascading Phase 1.
CBlockIndex* BlockFinder::FindByHeight(int height) NO_THREAD_SAFETY_ANALYSIS
{
    // If the height is at the bottom half of the chain, start searching from
    // the start to the end, otherwise search backwards from the end.
    CBlockIndex *index = height < nBestHeight / 2
            ? pindexGenesisBlock
            : pindexBest;

    if(index != nullptr)
    {
        // Traverse towards the tail.
        while (index && index->pprev && index->nHeight > height) {
            index = index->pprev;
        }

        // Traverse towards the head.
        while (index && index->pnext && index->nHeight < height) {
            index = index->pnext;
        }
    }

    return index;
}

CBlockIndex* BlockFinder::FindByMinTime(int64_t time) NO_THREAD_SAFETY_ANALYSIS
{
    // Select starting point depending on time proximity. While this is not as
    // accurate as in the FindByHeight case it will still give us a reasonable
    // estimate.
    CBlockIndex *index = abs(time - pindexBest->nTime) < abs(time - pindexGenesisBlock->nTime)
            ? pindexBest
            : pindexGenesisBlock;

    if (index != nullptr)
    {
        // Move back until the previous block is no longer younger than "time".
        while (index && index->pprev && index->pprev->nTime > time) {
            index = index->pprev;
        }

        // Move forward until the current block is younger than "time".
        while (index && index->pnext && index->nTime < time) {
            index = index->pnext;
        }
    }

    return index;
}

// The arguments are passed by value on purpose.
CBlockIndex* BlockFinder::FindByMinTimeFromGivenIndex(int64_t time, CBlockIndex* const index_in) NO_THREAD_SAFETY_ANALYSIS
{
    CBlockIndex* index = index_in;

    // If no starting index is provided (i.e. second parameter is omitted or nullptr is passed in,
    // then start at the Genesis Block. This is in general expensive and should be avoided.
    if (index == nullptr) {
        index = pindexGenesisBlock;
    }

    while (index && index->pnext && index->nTime < time) {
        index = index->pnext;
    }

    return index;
}

CBlockIndex* BlockFinder::FindLatestSuperblock(CBlockIndex* const index_in) NO_THREAD_SAFETY_ANALYSIS
{
    CBlockIndex* index = index_in;

    // If no input index is provided, start at the head of the chain.
    if (index == nullptr) {
        index = pindexBest;
    }

    while (index && index->pprev) {
        if (index->IsSuperblock()) {
            break;
        }

        index = index->pprev;
    }

    return index;
}
