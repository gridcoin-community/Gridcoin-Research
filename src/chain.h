// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAIN_H
#define BITCOIN_CHAIN_H

#include "arith_uint256.h"
#include "primitives/block.h"
#include "sync.h"
#include "uint256.h"

#include <atomic>
#include <unordered_map>

class CBlockIndex;

// BlockHasher lives in primitives/block.h (extracted in #3060); use it here for
// the block-hash-keyed mapBlockIndex rather than redefining it.
typedef std::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;

//! The chain-state lock. Guards the active-chain globals below.
extern CCriticalSection cs_main;

//! Active chain state, extracted from main.h (issue #3030, workstream A6).
//! These remain defined in main.cpp; only the declarations live here so
//! consumers can depend on chain state without pulling in all of main.h.
extern BlockMap mapBlockIndex GUARDED_BY(cs_main);
extern CBlockIndex* pindexGenesisBlock GUARDED_BY(cs_main);
extern int nBestHeight GUARDED_BY(cs_main);
extern arith_uint256 nBestChainTrust GUARDED_BY(cs_main);
extern uint256 hashBestChain GUARDED_BY(cs_main);
extern CBlockIndex* pindexBest GUARDED_BY(cs_main);
extern std::atomic<bool> g_reorg_in_progress;
// Sync clocks updated by UpdateSyncTime() (node/chainman.cpp) and read by
// OutOfSyncByAge() (main.cpp).
extern std::atomic<int64_t> g_previous_block_time;
extern std::atomic<int64_t> g_nTimeBestReceived;

#endif // BITCOIN_CHAIN_H
