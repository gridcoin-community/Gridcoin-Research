// Copyright (c) 2011-2021 The Bitcoin Core developers
// Copyright (c) 2021 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_BLOCKSTORAGE_H
#define BITCOIN_NODE_BLOCKSTORAGE_H

#include "protocol.h"
#include "sync.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

class CBlock;
class CBlockIndex;

//! Declared in chain.h; redeclared for the PrintBlockTree lock annotation
//! below (the gridcoin/staking/chain_trust.h idiom).
extern CCriticalSection cs_main;

namespace Consensus {
struct Params;
}

bool WriteBlockToDisk(const CBlock& block, unsigned int& nFileRet, unsigned int& nBlockPosRet, const CMessageHeader::MessageStartChars& messageStart);

bool ReadBlockFromDisk(CBlock& block, unsigned int nFile, unsigned int nBlockPos, const Consensus::Params& params, bool fReadTransactions=true);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& params, bool fReadTransactions=true);

bool CheckDiskSpace(uint64_t nAdditionalBytes=0);
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
//! Append handle for the current block file. The returned FILE* is CACHED and
//! owned by blockstorage -- callers must NOT fclose it; use CloseBlockFile().
//! Requires cs_main.
FILE* AppendBlockFile(unsigned int& nFileRet);

//! Close the cached block-file append handle, if one is open. Call after the final
//! FileCommit at shutdown, and any time the handle must not outlive the caller.
//! Requires cs_main. Safe to call when nothing is open.
void CloseBlockFile();
bool LoadExternalBlockFile(FILE* fileIn, size_t file_size = 0,
                           unsigned int percent_start = 0, unsigned int percent_end = 100);

//! Load the block index from LevelDB, creating and writing the genesis block
//! first on an empty datadir (via CreateGenesisBlock in chainparams.cpp).
//! Moved from main.{h,cpp} (issue #3125, workstream C5). Takes cs_main
//! internally; callers MUST NOT hold cs_main when calling (the internal LOCK
//! would deadlock under non-recursive locking; cs_main is currently recursive
//! but the annotation contract documents the intent).
bool LoadBlockIndex(bool fAllowNew=true);

//! Debug helper (-printblocktree): dump the whole block-index tree to the
//! log. Moved from main.{h,cpp} (issue #3125 C9).
void PrintBlockTree() EXCLUSIVE_LOCKS_REQUIRED(cs_main);


#endif // BITCOIN_NODE_BLOCKSTORAGE_H

