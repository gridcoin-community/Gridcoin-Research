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
class CNode;
class CTxDB;
class CValidationState;

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

//! Run the Gridcoin tally-transition services for the current chain tip.
//! Called from SetBestChain after the tip advances (issue #3125, C1).
bool GridcoinServices() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//! Ask up to 10 connected peers for blocks starting at hashStart (or the
//! current tip when hashStart is the default/empty hash uint256()).
bool AskForOutstandingBlocks(uint256 hashStart);

//! Validate and accept a block arriving from the network, a bootstrap file or
//! the internal miner, parking orphans until their parents arrive.
bool ProcessBlock(CNode* pfrom, CBlock* pblock, bool generated_by_me, CValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

#endif // GRIDCOIN_NODE_CHAINMAN_H
