// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "gridcoin/account.h"
#include "gridcoin/cpid.h"
#include "gridcoin/superblock.h"
#include "gridcoin/tally.h"
#include "node/shutdown.h"
#include "primitives/block.h"
#include "sync.h"
#include "test/test_gridcoin.h"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace {
//!
//! \brief Block index entries linked by pprev, one minute apart. They are not
//! in mapBlockIndex.
//!
struct MockChain
{
    std::vector<std::unique_ptr<CBlockIndex>> m_blocks;

    explicit MockChain(const int length)
    {
        for (int height = 0; height < length; ++height) {
            m_blocks.push_back(MakeBlock(height, height > 0 ? m_blocks.back().get() : nullptr));
        }
    }

    static std::unique_ptr<CBlockIndex> MakeBlock(const int height, CBlockIndex* const pprev)
    {
        auto block = std::make_unique<CBlockIndex>();
        block->nHeight = height;
        block->nTime = 1600000000 + height * 60;
        block->pprev = pprev;

        return block;
    }

    CBlockIndex* operator[](const int height) const { return m_blocks[height].get(); }

    CBlockIndex* Tip() const { return m_blocks.back().get(); }

    //!
    //! \brief Record every block's research reward data in the tally, as
    //! connecting the chain does.
    //!
    void RecordAll() const
    {
        for (size_t height = 1; height < m_blocks.size(); ++height) {
            GRC::Tally::RecordRewardBlock(m_blocks[height].get());
        }
    }
};

constexpr int LENGTH = 20;
} // Anonymous namespace

BOOST_AUTO_TEST_SUITE(tally_tests)

BOOST_AUTO_TEST_CASE(it_refuses_a_superblock_that_is_not_well_formed)
{
    LOCK(cs_main);

    const GRC::Cpid cpid(InsecureRandBytes(16));
    GRC::Tally::CreateAccount(cpid).m_accrual = 12345;

    // SuperblockPtr::ReadFromDisk() returns an empty superblock when it cannot
    // read a superblock block. The tally refuses it before it changes any
    // account, without stopping the node.
    BOOST_CHECK(!GRC::Tally::ApplySuperblock(GRC::SuperblockPtr::Empty(), nullptr));
    BOOST_CHECK_EQUAL(GRC::Tally::GetAccount(cpid).m_accrual, 12345);
    BOOST_CHECK(!ShutdownRequested());

    GRC::Tally::RemoveAccount(cpid);
}

// The check passes for a tally that recorded every reward block of the chain:
// research rewards, an MRC (recorded at the parent of the block that pays it),
// and a block that pays a research reward right after an MRC.
BOOST_AUTO_TEST_CASE(the_check_passes_for_a_tally_that_recorded_every_reward_block)
{
    LOCK(cs_main);

    const GRC::Cpid cpid(InsecureRandBytes(16));
    const GRC::Cpid other_cpid(InsecureRandBytes(16));

    MockChain chain(LENGTH);
    chain[3]->SetResearcherContext(cpid, 100, 10.0);
    chain[7]->AddMRCResearcherContext(cpid, 200, 20.0);
    chain[8]->SetResearcherContext(cpid, 300, 30.0);
    chain[11]->SetResearcherContext(other_cpid, 400, 40.0);
    chain[14]->AddMRCResearcherContext(cpid, 500, 50.0);
    chain.RecordAll();

    BOOST_REQUIRE(GRC::Tally::GetAccount(cpid).m_last_block_ptr == chain[13]);
    BOOST_CHECK(!GRC::Tally::VerifyRewardBlocks(cpid, chain.Tip()));
    BOOST_CHECK(!GRC::Tally::VerifyRewardBlocks(other_cpid, chain.Tip()));

    GRC::Tally::RemoveAccount(cpid);
    GRC::Tally::RemoveAccount(other_cpid);
}

// The check for a research reward. It reports the account's last reward block
// and the block found above it that pays the CPID a research reward.
BOOST_AUTO_TEST_CASE(the_check_reports_the_reward_blocks_found_for_a_research_reward)
{
    LOCK(cs_main);

    const GRC::Cpid cpid(InsecureRandBytes(16));

    MockChain chain(LENGTH);
    chain[5]->SetResearcherContext(cpid, 100, 10.0);
    chain[12]->SetResearcherContext(cpid, 300, 30.0);
    chain.RecordAll();

    // The tally forgets the block at 12:
    GRC::Tally::ForgetRewardBlock(chain[12]);
    BOOST_REQUIRE(GRC::Tally::GetAccount(cpid).m_last_block_ptr == chain[5]);

    const std::optional<GRC::Tally::RewardBlocks> found = GRC::Tally::VerifyRewardBlocks(cpid, chain.Tip());

    BOOST_REQUIRE(found);
    BOOST_CHECK(found->m_recorded == chain[5]);
    BOOST_CHECK(found->m_expected == chain[12]);
    BOOST_CHECK(found->m_paid_in == chain[12]);
    BOOST_CHECK_EQUAL(found->m_amount, 300);
    BOOST_CHECK(!found->m_mrc);
    BOOST_CHECK(!found->ToString().empty());

    GRC::Tally::RemoveAccount(cpid);
}

// The same for an MRC: the last reward block the chain implies is the parent
// of the block that pays the MRC.
BOOST_AUTO_TEST_CASE(the_check_reports_the_reward_blocks_found_for_an_mrc)
{
    LOCK(cs_main);

    const GRC::Cpid cpid(InsecureRandBytes(16));

    MockChain chain(LENGTH);
    chain[5]->SetResearcherContext(cpid, 100, 10.0);
    chain[15]->AddMRCResearcherContext(cpid, 200, 20.0);
    chain.RecordAll();

    GRC::Tally::ForgetRewardBlock(chain[15]);
    BOOST_REQUIRE(GRC::Tally::GetAccount(cpid).m_last_block_ptr == chain[5]);

    const std::optional<GRC::Tally::RewardBlocks> found = GRC::Tally::VerifyRewardBlocks(cpid, chain.Tip());

    BOOST_REQUIRE(found);
    BOOST_CHECK(found->m_recorded == chain[5]);
    BOOST_CHECK(found->m_expected == chain[14]);
    BOOST_CHECK(found->m_paid_in == chain[15]);
    BOOST_CHECK_EQUAL(found->m_amount, 200);
    BOOST_CHECK(found->m_mrc);

    GRC::Tally::RemoveAccount(cpid);
}

// The check compares the account's last reward block with the chain's block at
// its height. Here the account's is a block of another branch, and the check
// reports it.
BOOST_AUTO_TEST_CASE(the_check_compares_the_last_reward_block_with_the_chain_s_block_at_its_height)
{
    LOCK(cs_main);

    const GRC::Cpid cpid(InsecureRandBytes(16));

    MockChain chain(LENGTH);
    chain.RecordAll();

    std::unique_ptr<CBlockIndex> other_branch = MockChain::MakeBlock(10, chain[9]);
    other_branch->SetResearcherContext(cpid, 100, 10.0);
    GRC::Tally::RecordRewardBlock(other_branch.get());

    const std::optional<GRC::Tally::RewardBlocks> found = GRC::Tally::VerifyRewardBlocks(cpid, chain.Tip());

    BOOST_REQUIRE(found);
    BOOST_CHECK(found->m_recorded == other_branch.get());
    BOOST_CHECK(found->m_expected == nullptr);
    BOOST_CHECK(found->m_paid_in == nullptr);

    GRC::Tally::RemoveAccount(cpid);
}

// The check passes for a CPID with no reward block in the tally or on the
// chain.
BOOST_AUTO_TEST_CASE(the_check_passes_for_a_cpid_without_reward_blocks)
{
    LOCK(cs_main);

    const GRC::Cpid cpid(InsecureRandBytes(16));

    MockChain chain(LENGTH);
    chain.RecordAll();

    BOOST_CHECK(!GRC::Tally::VerifyRewardBlocks(cpid, chain.Tip()));
    BOOST_CHECK(!GRC::Tally::VerifyRewardBlocks(cpid, nullptr));
}

BOOST_AUTO_TEST_SUITE_END()
