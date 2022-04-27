// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2021 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_BLOCKSTORAGE_H
#define BITCOIN_NODE_BLOCKSTORAGE_H

#include "protocol.h"

class CBlock;
class CBlockIndex;

namespace Consensus {
struct Params;
}

bool WriteBlockToDisk(const CBlock& block, unsigned int& nFileRet, unsigned int& nBlockPosRet, const CMessageHeader::MessageStartChars& messageStart);

bool ReadBlockFromDisk(CBlock& block, unsigned int nFile, unsigned int nBlockPos, const Consensus::Params& params, bool fReadTransactions=true);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& params, bool fReadTransactions=true);


#endif // BITCOIN_NODE_BLOCKSTORAGE_H

