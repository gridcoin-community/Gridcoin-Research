// Copyright (c) 2022 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <main.h>
#include <gridcoin/account.h>
#include <gridcoin/beacon.h>
#include <gridcoin/cpid.h>
#include <gridcoin/mrc.h>
#include <gridcoin/tally.h>
#include <key.h>
#include <miner.h>
#include <primitives/transaction.h>
#include <rpc/blockchain.h>
#include <test/test_gridcoin.h>

namespace {
struct Setup {
    CBlockIndex* pindex{nullptr};
    GRC::Cpid cpid = GRC::Cpid(InsecureRandBytes(16));
    GRC::ResearchAccount& account = GRC::Tally::CreateAccount(cpid);
    CWallet* wallet = new CWallet();
    GRC::Beacon beacon;
    CKey key;

    Setup() {
        GRC::GetBeaconRegistry().Reset();
        SelectParams(CBaseChainParams::MAIN);

        // Setup a mock chain.
        uint256* phash = new uint256(InsecureRandBytes(32));
        pindex = GRC::MockBlockIndex::InsertBlockIndex(*phash);
        pindexGenesisBlock = pindex;
        pindex->nVersion = 12;
        pindex->phashBlock = phash;
        // Needed because this code does not mock superblocks and inclusion of m_accrual
        // into the calculated accrual is dependent on a block's height being
        // lower than the last superblock's.
        // TODO(div72): Improve mockability of Tally.
        pindex->nHeight = -1;
        mapBlockIndex[pindex->GetBlockHash()] = pindex;
        for (int i = 1; i < Params().GetConsensus().MRCZeroPaymentInterval / (24 * 60 * 60) + 2; ++i) {
            phash = new uint256(InsecureRandBytes(32));
            CBlockIndex* pindexNext = GRC::MockBlockIndex::InsertBlockIndex(*phash);
            pindexNext->nHeight = i;
            pindexNext->nTime = pindex->nTime + 24 * 60 * 60;
            pindexNext->nVersion = 12;
            pindex->pnext = pindexNext;
            pindexNext->pprev = pindex;
            pindexNext->phashBlock = phash;
            pindex = pindexNext;
        }

        // These tests were written with these assumptions. If this statement
        // does not hold true in the future, please update.
        assert(pindex->nHeight >= 7);
        assert(pindex->nHeight < 179);

        CTransaction tx;
        tx.nTime = pindexGenesisBlock->nTime;

        key.MakeNewKey(false);

        LOCK(wallet->cs_wallet);

        wallet->AddKey(key);

        GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
                GRC::ContractAction::ADD,
                cpid,
                key.GetPubKey()
        );

        account.m_first_block_ptr = pindexGenesisBlock;
        account.m_last_block_ptr = pindexGenesisBlock;

        GRC::GetBeaconRegistry().Add({contract, tx, pindexGenesisBlock});
        GRC::GetBeaconRegistry().ActivatePending({key.GetPubKey().GetID()}, tx.nTime, uint256(), 0);
        beacon = *GRC::GetBeaconRegistry().TryActive(cpid, pindex->nTime);

        SetMockTime(1);

        gArgs.ForceSetArg("forcecpid", cpid.ToString());
        GRC::Researcher::Reload({}, {});
    }

    ~Setup() {
        GRC::GetBeaconRegistry().Reset();
        gArgs.ForceSetArg("forcecpid", "");
        gArgs.ForceSetArg("email", "investor");
        GRC::Researcher::Reload();
        gArgs.ForceSetArg("email", "");
        GRC::Tally::RemoveAccount(cpid);

        SetMockTime(0);

        for (CBlockIndex* tip = pindex; tip->pprev; tip = tip->pprev) {
            mapBlockIndex.erase(tip->GetBlockHash());
            delete tip->phashBlock;
        }

        mapBlockIndex.erase(pindexGenesisBlock->GetBlockHash());
        delete pindexGenesisBlock->phashBlock;
        delete wallet;
    }
};
} // Anonymous namespace

BOOST_FIXTURE_TEST_SUITE(mrc_tests, Setup)

BOOST_AUTO_TEST_CASE(it_properly_records_blocks)
{
    pindex->AddMRCResearcherContext(cpid, 72, 0.0);
    pindex->pprev->pprev->SetResearcherContext(cpid, 72, 0.0);
    pindex->pprev->pprev->pprev->pprev->AddMRCResearcherContext(cpid, 72, 0.0);
    pindex->pprev->pprev->pprev->pprev->pprev->pprev->SetResearcherContext(cpid, 72, 0.0);

    CBlockIndex* index{pindexGenesisBlock};
    while (index) {
        GRC::Tally::RecordRewardBlock(index);
        index = index->pnext;
    }

    BOOST_CHECK(account.m_last_block_ptr == pindex->pprev);
    GRC::Tally::ForgetRewardBlock(pindex);
    BOOST_CHECK(account.m_last_block_ptr == pindex->pprev->pprev);
    GRC::Tally::ForgetRewardBlock(pindex->pprev->pprev);
    BOOST_CHECK(account.m_last_block_ptr == pindex->pprev->pprev->pprev->pprev->pprev);
    GRC::Tally::ForgetRewardBlock(pindex->pprev->pprev->pprev->pprev);
    BOOST_CHECK(account.m_last_block_ptr == pindex->pprev->pprev->pprev->pprev->pprev->pprev);
    GRC::Tally::ForgetRewardBlock(pindex->pprev->pprev->pprev->pprev->pprev->pprev);

    BOOST_CHECK(account.m_last_block_ptr == nullptr);
    account.m_last_block_ptr = account.m_first_block_ptr;
}

BOOST_AUTO_TEST_CASE(it_has_proper_fees_for_newbies)
{
    GRC::MRC mrc;
    mrc.m_mining_id = cpid;
    mrc.m_research_subsidy = 72;

    // Before:
    mrc.m_last_block_hash = pindex->pprev->pprev->GetBlockHash();
    BOOST_CHECK_EQUAL(mrc.ComputeMRCFee(), mrc.m_research_subsidy);

    // At the border:
    mrc.m_last_block_hash = pindex->pprev->GetBlockHash();
    BOOST_CHECK_EQUAL(mrc.ComputeMRCFee(), 28);

    // After:
    mrc.m_last_block_hash = pindex->GetBlockHash();
    BOOST_CHECK_EQUAL(mrc.ComputeMRCFee(), 26);
}

BOOST_AUTO_TEST_CASE(it_rejects_invalid_claims)
{
    GRC::MRC mrc;

    LOCK(cs_main);

    mrc.m_mining_id = cpid;
    mrc.m_client_version = "6.0.0.0";
    mrc.m_last_block_hash = pindex->GetBlockHash();

    BOOST_CHECK(!mrc.WellFormed()); 

    mrc.m_research_subsidy = 72;
    account.m_accrual = 72;

    mrc.Sign(key);
    BOOST_CHECK(mrc.WellFormed());

    BOOST_CHECK(!ValidateMRC(pindex, mrc));
    
    mrc.m_fee = mrc.ComputeMRCFee();

    // The hash now includes the fee.
    mrc.Sign(key);
    BOOST_CHECK(ValidateMRC(pindex, mrc));

    mrc.m_research_subsidy = 9223372036854775807; // Get rich.
    BOOST_CHECK(!ValidateMRC(pindex, mrc));
    mrc.m_research_subsidy = 72;

    mrc.m_last_block_hash = pindex->pprev->GetBlockHash(); // Older request.
    mrc.Sign(key);
    BOOST_CHECK(!ValidateMRC(pindex, mrc));
    mrc.m_last_block_hash = pindex->GetBlockHash();
    mrc.Sign(key);

    mrc.m_fee = 0; // Tax evasion.
    BOOST_CHECK(!ValidateMRC(pindex, mrc));
}

BOOST_AUTO_TEST_CASE(createmrc_creates_valid_mrcs)
{
    account.m_accrual = 72;
    GRC::MRC mrc;
    CAmount reward{0}, fee{0};

    LOCK2(cs_main, wallet->cs_wallet);

    GRC::CreateMRC(pindex->pprev, mrc, reward, fee, wallet);

    BOOST_CHECK_EQUAL(reward, 72);
    BOOST_CHECK_EQUAL(fee, 28);

    BOOST_CHECK(mrc.WellFormed());
    BOOST_CHECK(ValidateMRC(pindex->pprev, mrc));
}

BOOST_AUTO_TEST_CASE(it_accepts_valid_fees)
{
    account.m_accrual = 72;
    GRC::MRC mrc;
    CAmount reward{0}, fee{0};

    LOCK2(cs_main, wallet->cs_wallet);

    GRC::CreateMRC(pindex->pprev, mrc, reward, fee, wallet);

    mrc.m_fee = 14;
    mrc.Sign(key);
    BOOST_CHECK(!ValidateMRC(pindex->pprev, mrc));
    mrc.m_fee = 28;
    mrc.Sign(key);
    BOOST_CHECK(ValidateMRC(pindex->pprev, mrc));
    mrc.m_fee = 56;
    mrc.Sign(key);
    BOOST_CHECK(ValidateMRC(pindex->pprev, mrc));
    mrc.m_fee = 73;
    mrc.Sign(key);
    BOOST_CHECK(!ValidateMRC(pindex->pprev, mrc));
}

BOOST_AUTO_TEST_CASE(it_creates_valid_mrc_claims)
{
    CBlock block;
    block.vtx.resize(2);
    block.vtx[1].vin.resize(1);
    block.vtx[1].vout.resize(2);
    std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>> mrc_map;
    std::map<GRC::Cpid, uint256> mrc_tx_map;

    account.m_accrual = 72;

    LOCK2(cs_main, wallet->cs_wallet);

    pindex->pprev->AddMRCResearcherContext(cpid, 72, 0.0);

    BOOST_CHECK(CreateRestOfTheBlock(block, pindex->pprev, mrc_map));

    GRC::MRC mrc;
    CAmount reward{0}, fee{0};
    GRC::CreateMRC(pindex->pprev, mrc, reward, fee, wallet);
    mrc_map[cpid] = {uint256{}, mrc};

    GRC::Claim claim;

    uint32_t claim_contract_version = 2;

    BOOST_CHECK(CreateGridcoinReward(block, pindex->pprev, reward, claim));

    BOOST_CHECK(CreateMRCRewards(block, mrc_map, mrc_tx_map, reward, claim_contract_version, claim, wallet));

    // TODO(div72): Separate this test into pieces and actually have it do
    // some useful testing by testing the validation logic against it.
    // Currently the miner code is too coupled together which makes this
    // infeasible without having a mess of spaghetti code for working
    // around this.
}

BOOST_AUTO_TEST_SUITE_END()
