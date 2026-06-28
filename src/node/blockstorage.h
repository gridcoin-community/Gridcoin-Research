// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2021 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_BLOCKSTORAGE_H
#define BITCOIN_NODE_BLOCKSTORAGE_H

#include "protocol.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

class CBlock;
class CBlockIndex;

namespace Consensus {
struct Params;
}

bool WriteBlockToDisk(const CBlock& block, unsigned int& nFileRet, unsigned int& nBlockPosRet, const CMessageHeader::MessageStartChars& messageStart);

bool ReadBlockFromDisk(CBlock& block, unsigned int nFile, unsigned int nBlockPos, const Consensus::Params& params, bool fReadTransactions=true);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& params, bool fReadTransactions=true);

bool CheckDiskSpace(uint64_t nAdditionalBytes=0);
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
FILE* AppendBlockFile(unsigned int& nFileRet);
bool LoadExternalBlockFile(FILE* fileIn, size_t file_size = 0,
                           unsigned int percent_start = 0, unsigned int percent_end = 100);


#endif // BITCOIN_NODE_BLOCKSTORAGE_H

