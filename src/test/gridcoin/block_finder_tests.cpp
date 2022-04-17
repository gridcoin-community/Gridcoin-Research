// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/support/block_finder.h"

#include <boost/test/unit_test.hpp>
#include <array>
#include <cstdint>

namespace
{
    template<size_t Size>
    class BlockChain
    {
    public:
        BlockChain()
        {
            // Initialize block link.
            for(CBlockIndex* block = blocks.begin(); block != blocks.end(); ++block)
            {
                block->SetNull();
                CBlockIndex* prev = std::prev(block);
                CBlockIndex* next = std::next(block);
                if(block != &blocks.front())
                {
                    block->pprev = prev;
                    block->nHeight = prev->nHeight + 1;
                    block->nTime = prev->nTime + 10;
                }
                if(block != &blocks.back())
                    block->pnext = next;
            }
            // Setup global variables.
            pindexBest = &blocks.back();
            pindexGenesisBlock = &blocks.front();
            nBestHeight = blocks.back().nHeight;
        }
        std::array<CBlockIndex, Size> blocks;
    };
}

BOOST_AUTO_TEST_SUITE(block_finder_tests);

BOOST_AUTO_TEST_CASE(FindBlockInNormalChainShouldWork)
{
    BlockChain<100> chain;
    for(auto& block : chain.blocks)
        BOOST_CHECK_EQUAL(&block, GRC::BlockFinder::FindByHeight(block.nHeight));
}

BOOST_AUTO_TEST_CASE(FindBlockAboveHighestHeightShouldReturnHighestBlock)
{
    BlockChain<100> chain;
    CBlockIndex& last = chain.blocks.back();
    BOOST_CHECK_EQUAL(&last, GRC::BlockFinder::FindByHeight(101));
}

BOOST_AUTO_TEST_CASE(FindBlockByHeightShouldWorkOnChainsWithJustOneBlock)
{
    BlockChain<1> chain;
    BOOST_CHECK_EQUAL(&chain.blocks.front(), GRC::BlockFinder::FindByHeight(0));
    BOOST_CHECK_EQUAL(&chain.blocks.front(), GRC::BlockFinder::FindByHeight(1));
    BOOST_CHECK_EQUAL(&chain.blocks.front(), GRC::BlockFinder::FindByHeight(-1));
}

BOOST_AUTO_TEST_CASE(FindBlockByTimeShouldReturnNextYoungestBlock)
{
    // Chain with block times 0, 10, 20, 30, 40 etc.
    BlockChain<10> chain;

    // Finding the block older than time 10 should return block #2
    // which has time 20.
    BOOST_CHECK_EQUAL(&chain.blocks[2], GRC::BlockFinder::FindByMinTime(11));
    BOOST_CHECK_EQUAL(&chain.blocks[1], GRC::BlockFinder::FindByMinTime(10));
    BOOST_CHECK_EQUAL(&chain.blocks[1], GRC::BlockFinder::FindByMinTime(9));
}

BOOST_AUTO_TEST_CASE(FindBlockByTimeShouldReturnLastBlockIfOlderThanTime)
{
    BlockChain<10> chain;
    BOOST_CHECK_EQUAL(&chain.blocks.back(), GRC::BlockFinder::FindByMinTime(999999));
}

BOOST_AUTO_TEST_SUITE_END()
