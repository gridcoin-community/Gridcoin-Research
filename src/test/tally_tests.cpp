#include "tally.h"
#include "main.h"

#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

namespace
{
    typedef std::shared_ptr<CBlockIndex> CBlockIndexPtr;
    std::vector<CBlockIndexPtr> CreateChain()
    {
        std::vector<CBlockIndexPtr> blocks;

        // Create a chain of 100 blocks
        for(int i=1; i<=100; ++i)
        {
            CBlockIndexPtr block = std::make_shared<CBlockIndex>();
            block->nHeight = i;

            // Attach to chain
            if(!blocks.empty())
            {
                CBlockIndexPtr previous = blocks.back();
                previous->pnext = block.get();
                block->pprev = previous.get();
            }

            blocks.push_back(block);
        }

        return blocks;
    }
}

BOOST_AUTO_TEST_SUITE(tally_tests)

BOOST_AUTO_TEST_CASE(tally_FindTallyTriggerShouldUseTallyGranularity)
{
    BOOST_CHECK_EQUAL(TALLY_GRANULARITY, 10);

    auto chain = CreateChain();
    CBlockIndexPtr head = chain.back();

    const CBlockIndex* block = FindTallyTrigger(head.get()->pprev);
    BOOST_CHECK_EQUAL(block->nHeight, 90);
}

BOOST_AUTO_TEST_CASE(tally_IsTallyTriggerShouldUseTallyGranularity)
{
    auto chain = CreateChain();
    CBlockIndexPtr head = chain.back();

    BOOST_CHECK(IsTallyTrigger(head.get()));
    BOOST_CHECK(IsTallyTrigger(head->pprev) == false);
}

BOOST_AUTO_TEST_SUITE_END()
