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

void BlockFinder::Reset()
{
    cache = nullptr;
}
