#include "block.h"
#include "main.h"

#include <cstdlib>

BlockFinder::BlockFinder()
    : cache(nullptr)
{}

CBlockIndex* BlockFinder::FindByHeight(int height)
{
    // If the height is at the bottom half of the chain, start searching from
    // the start to the end, otherwise search backwards from the end.
    CBlockIndex *index = height < nBestHeight / 2
            ? pindexGenesisBlock
            : pindexBest;

    if(index != nullptr)
    {
        // Use the cache if it's closer to the target than the current
        // start block.
        if (cache && abs(height - index->nHeight) > std::abs(height - cache->nHeight))
            index = cache;

        // Traverse towards the tail.
        while (index && index->pprev && index->nHeight > height)
            index = index->pprev;

        // Traverse towards the head.
        while (index && index->pnext && index->nHeight < height)
            index = index->pnext;
    }
   
    cache = index;
    return index;  
}

CBlockIndex* BlockFinder::FindByTime(int64_t time)
{
    CBlockIndex *index = abs(time - pindexBest->nTime) < abs(time - pindexGenesisBlock->nTime)
            ? pindexBest
            : pindexGenesisBlock;

    if(index != nullptr)
    {
        if(cache && abs(time - index->nTime) > abs(time - int64_t(cache->nTime)))
            index = cache;

        // Move back until the previous block is no longer younger than "time".
        while(index && index->pprev && index->pprev->nTime > time)
            index = index->pprev;

        // Move forward until the current block is younger than "time".
        while(index && index->pnext && index->nTime < time)
            index = index->pnext;
    }

    cache = index;
    return index;
}

void BlockFinder::Reset()
{
    cache = nullptr;
}
