#pragma once

#include "fwd.h"

//!
//! \brief Chain traversing block finder.
//!
class BlockFinder
{
public:
    //!
    //! \brief Constructor.
    //!
    BlockFinder();
    
    //!
    //! \brief Find a block with a specific height.
    //! 
    //! Traverses the chain from head or tail, depending on what's closest to
    //! find the block that matches \p height. This is a caching operation
    //! 
    //! \param nHeight Block height to find.
    //! \return The block with the height closest to \p nHeight if found, otherwise
    //! \a nullptr is returned.
    //!
    CBlockIndex* FindByHeight(int height);
    
    //!
    //! \brief Reset finder cache.
    //!
    //! Clears the block finder cache. This should be used when blocks are removed
    //! from the chain to avoid accessing deleted memory.
    //! 
    void Reset();
    
private:
    CBlockIndex* cache;
};
