// Copyright (c) 2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "main.h"
#include "server.h"
#include "protocol.h"
#include "init.h" // for pwalletMain
#include "checkpoints.h"
#include "txdb.h"
#include "gridcoin/appcache.h"
#include "gridcoin/backup.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/block_finder.h"
#include "gridcoin/tally.h"
#include "gridcoin/tx_message.h"
#include "util.h"

namespace GRC
{
class MockBlockIndex : CDiskBlockIndex
{
    MockBlockIndex() : CDiskBlockIndex() {};

    MockBlockIndex(CBlockIndex* pindex) : CDiskBlockIndex(pindex)
    {

    };

public:
    static CBlockIndex* InsertBlockIndex(const uint256& hash)
    {
        if (hash.IsNull())
            return NULL;

        // Return existing
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
            return (*mi).second;

        // Create new
        CBlockIndex* pindexNew = GRC::BlockIndexPool::GetNextBlockIndex();
        if (!pindexNew)
            throw std::runtime_error("LoadBlockIndex() : new CBlockIndex failed");
        mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
        pindexNew->phashBlock = &((*mi).first);

        return pindexNew;
    }
};

struct ExportContractElement
{
    ExportContractElement() {};

    ExportContractElement(CBlockIndex* pindex) : m_disk_block_index(pindex) {};

    // This is similar to GRC::ContractContext but without the full pindex.
    std::vector<std::pair<GRC::Contract, CTransaction>> m_ctx;

    // Use the disk format of the pindex for serialization/deserialization from disk.
    CDiskBlockIndex m_disk_block_index;

    // We need this for the superblock beacon activations. For other types of
    // contracts it will be empty.
    std::vector<uint160> m_verified_beacons;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_ctx);
        READWRITE(m_disk_block_index);
        READWRITE(m_verified_beacons);
    }
};

} // namespace GRC
