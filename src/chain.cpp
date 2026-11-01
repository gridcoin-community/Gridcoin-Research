// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"

#include "gridcoin/block_index.h"

// Definitions for the active-chain state declared in chain.h, moved out of
// main.cpp (issue #3125 C9).

//! The chain-state lock. The canonical lock order is
//! cs_main -> signals -> cs_wallet (see doc/developer-notes.md).
CCriticalSection cs_main;

namespace GRC {
BlockIndexPool::Pool<CBlockIndex> BlockIndexPool::m_block_index_pool;
BlockIndexPool::Pool<ResearcherContext> BlockIndexPool::m_researcher_context_pool;
} // namespace GRC

BlockMap mapBlockIndex;
CBlockIndex* pindexGenesisBlock GUARDED_BY(cs_main) = nullptr;
int nBestHeight GUARDED_BY(cs_main) = -1;
uint256 hashBestChain GUARDED_BY(cs_main);
CBlockIndex* pindexBest GUARDED_BY(cs_main) = nullptr;
std::atomic<int64_t> g_previous_block_time;
std::atomic<int64_t> g_nTimeBestReceived;
std::atomic<bool> g_reorg_in_progress = false;

arith_uint256 CBlockIndex::GetBlockTrust() const
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for an arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (bnTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}
