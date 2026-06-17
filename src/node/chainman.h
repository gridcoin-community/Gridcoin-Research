// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_CHAINMAN_H
#define GRIDCOIN_NODE_CHAINMAN_H

#include "sync.h"
#include "uint256.h"

class CBlock;
class CBlockIndex;
class CTxDB;

extern CCriticalSection cs_main;

//! Make blockNew / pindexNew the new active chain tip, reorganizing the chain
//! to it if necessary. Moved out of main.cpp into the chain-management module
//! (issue #3030, workstream A4); the body lives in node/chainman.cpp.
bool SetBestChain(CTxDB& txdb, CBlock& blockNew, CBlockIndex* pindexNew) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Force the active chain to reorganize to the block with hash NewHash.
bool ForceReorganizeToHash(uint256 NewHash);

//! Update the "previous block time" / "best received" sync clocks from the new
//! tip. Called from chain connection and from the abandonment path in main.cpp.
void UpdateSyncTime(const CBlockIndex* pindexBest);

#endif // GRIDCOIN_NODE_CHAINMAN_H
