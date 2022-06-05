// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SUPPORT_BLOCK_FINDER_H
#define GRIDCOIN_SUPPORT_BLOCK_FINDER_H

#include <cstdint>

class CBlockIndex;

namespace GRC {
//!
//! \brief Chain traversing block finder.
//!
class BlockFinder
{
public:
    //!
    //! \brief Find a block with a specific height.
    //!
    //! Traverses the chain from head or tail, depending on what's closest to
    //! find the block that matches \p height.
    //!
    //! \param nHeight Block height to find.
    //! \return The block with the height closest to \p nHeight if found, otherwise
    //! \a nullptr is returned.
    //!
    static CBlockIndex* FindByHeight(int height);

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
    static CBlockIndex* FindByMinTime(int64_t time);

    //!
    //! \brief Find block by time going forward from given index.
    //! \param time
    //! \param CBlockIndex from where to start
    //! \return CBlockIndex pointing to the youngest block which is not older than \p time, or
    //! the head of the chain if it is older than \p time.
    //!
    static CBlockIndex* FindByMinTimeFromGivenIndex(int64_t time, CBlockIndex* index = nullptr);
};
} // namespace GRC

#endif // GRIDCOIN_SUPPORT_BLOCK_FINDER_H
