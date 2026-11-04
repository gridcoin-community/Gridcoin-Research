// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_LONE_BEST_BLOCK_H
#define GRIDCOIN_TEST_LONE_BEST_BLOCK_H

#include "chain.h"
#include "sync.h"
#include "test/state_guard.h"
#include "uint256.h"

#include <cstdint>

namespace grc_test {

//! A best block for a case in a suite without a chain (grc_test::RegtestChainSetup
//! builds a real one): one block index at height 0, which the tip globals name
//! for the case's life. What reads the tip then finds one: a wallet dump's
//! header, and an import's rescan, which finds no block file to read and so
//! adds nothing. The StateGuard puts the tip globals, and the mock time, back.
class LoneBestBlock
{
public:
    explicit LoneBestBlock(int64_t block_time = 1700000000)
    {
        m_index.phashBlock = &m_hash;
        m_index.nHeight = 0;
        m_index.nTime = static_cast<unsigned int>(block_time);

        LOCK(cs_main);
        pindexBest = &m_index;
        nBestHeight = 0;
        hashBestChain = m_hash;
    }

    LoneBestBlock(const LoneBestBlock&) = delete;
    LoneBestBlock& operator=(const LoneBestBlock&) = delete;

private:
    const uint256 m_hash{uint256S("0x6c6f6e652062657374206279206120746573742063617365")};
    CBlockIndex m_index{};
    //! Last, so it captures the globals before the constructor's body changes
    //! them, and is destroyed first, before the index the globals name.
    StateGuard m_state{};
};

} // namespace grc_test

#endif // GRIDCOIN_TEST_LONE_BEST_BLOCK_H
