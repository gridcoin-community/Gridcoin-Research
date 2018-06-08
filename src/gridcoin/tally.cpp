#include "tally.h"
#include "main.h"

#include <cassert>

CBlockIndex* FindTallyTrigger(CBlockIndex* block)
{
    // Scan backwards until we find one where accepting it would
    // trigger a tally.
    for(;
        block && block->pprev && !IsTallyTrigger(block);
        block = block->pprev);

    return block;
}

bool IsTallyTrigger(const CBlockIndex *block)
{
    return block && block->nHeight % TALLY_GRANULARITY == 0;
}
