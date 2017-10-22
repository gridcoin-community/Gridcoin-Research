#include "main.h"
#include "block.h"

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
         for(auto block = blocks.begin(); block != blocks.end(); ++block)
         {
            CBlockIndex& prev = *std::prev(block);
            CBlockIndex& next = *std::next(block);
            if(block != &blocks.front())
            {
               block->pprev = &prev;
               block->nHeight = prev.nHeight + 1;
            }
            if(block != &blocks.back())
               block->pnext = &next;      
         }
         
         // Setup global variables.
         pindexBest = &blocks.back();
         pindexGenesisBlock = &blocks.front();         
         nBestHeight = blocks.back().nHeight;
      }
      
      std::array<CBlockIndex, Size> blocks;      
   };
}

BOOST_AUTO_TEST_SUITE(block_tests);

BOOST_AUTO_TEST_CASE(FindBlockInNormalChainShouldWork)
{
   BlockChain<100> chain;
   BlockFinder finder;
   
   for(auto& block : chain.blocks)
       BOOST_CHECK_EQUAL(&block, finder.FindByHeight(block.nHeight));
}

BOOST_AUTO_TEST_CASE(FindBlockAboveHighestHeightShouldReturnHighestBlock)
{
   BlockChain<100> chain;
   BlockFinder finder;
   
   CBlockIndex& last = chain.blocks.back();
   BOOST_CHECK_EQUAL(&last, finder.FindByHeight(101));   
}

BOOST_AUTO_TEST_CASE(FindBlockByHeightShouldWorkOnChainsWithJustOneBlock)
{
   BlockChain<1> chain;
   BlockFinder finder;
   
   BOOST_CHECK_EQUAL(&chain.blocks.front(), finder.FindByHeight(0));
   BOOST_CHECK_EQUAL(&chain.blocks.front(), finder.FindByHeight(1));
   BOOST_CHECK_EQUAL(&chain.blocks.front(), finder.FindByHeight(-1));
}

BOOST_AUTO_TEST_SUITE_END()
