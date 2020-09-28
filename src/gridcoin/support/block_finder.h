// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

class CBlockIndex;

namespace GRC {
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
    //! \brief Find block by time.
    //!
    //! Traverses the chain in the same way as FindByHeight() and stops at the
    //! block which is not older than \p time, or the youngest block if it is
    //! older than \p time.
    //!
    //! \param time Block time to search for.
    //! \return The youngest block which is not older than \p time, or the
    //! head of the chain if it is older than \p time.
    //!
    CBlockIndex* FindByMinTime(int64_t time);

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
} // namespace GRC
