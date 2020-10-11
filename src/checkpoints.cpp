// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/range/adaptor/reversed.hpp>

#include "chainparams.h"
#include "checkpoints.h"
#include "uint256.h"

namespace Checkpoints
{
    bool CheckHardened(int nHeight, const uint256& hash)
    {
        const MapCheckpoints& checkpoints = Params().Checkpoints().mapCheckpoints;

        MapCheckpoints::const_iterator i = checkpoints.find(nHeight);
        if (i == checkpoints.end()) return true;
        return hash == i->second;
    }

    int GetTotalBlocksEstimate()
    {
        const MapCheckpoints& checkpoints = Params().Checkpoints().mapCheckpoints;

        if (checkpoints.empty())
            return 0;
        return checkpoints.rbegin()->first;
    }

    CBlockIndex* GetLastCheckpoint(const BlockMap& mapBlockIndex)
    {
        const MapCheckpoints& checkpoints = Params().Checkpoints().mapCheckpoints;

        for (auto const& i : boost::adaptors::reverse(checkpoints))
        {
            const uint256& hash = i.second;
            BlockMap::const_iterator t = mapBlockIndex.find(hash);
            if (t != mapBlockIndex.end())
                return t->second;
        }
        return nullptr;
    }

    // Check against synchronized checkpoint
    bool CheckSync(int nHeight)
    {
        const CBlockIndex* pindexSync = GetLastCheckpoint(mapBlockIndex);

        if (pindexSync != nullptr && nHeight <= pindexSync->nHeight)
            return false;
        return true;
    }
}
