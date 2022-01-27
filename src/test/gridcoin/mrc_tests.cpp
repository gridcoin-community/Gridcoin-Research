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
#include <primitives/transaction.h>
#include <rpc/blockchain.h>
#include <test/test_gridcoin.h>

namespace {
struct Setup {
    uint256 hash = uint256(InsecureRandBytes(32));
    CBlockIndex* pindex = GRC::MockBlockIndex::InsertBlockIndex(hash);
    GRC::Cpid cpid = GRC::Cpid(InsecureRandBytes(16));
    GRC::ResearchAccount& account = GRC::Tally::CreateAccount(cpid);
    GRC::Beacon beacon;
    CKey key;

    Setup() {
        SelectParams(CBaseChainParams::MAIN);

        // Setup a mock chain.
        pindexGenesisBlock = pindex;
        pindex->nVersion = 12;
        // Needed because we do not mock superblocks and inclusion of m_accrual
        // into the calculated accrual is dependant on a block's height being
        // lower than the last superblock's.
        // TODO(div72): Improve mockability of Tally.
        pindex->nHeight = -1;
        mapBlockIndex[pindex->GetBlockHash()] = pindex;
        for (int i = 1; i < Params().GetConsensus().MRCZeroPaymentInterval / (24 * 60 * 60) + 2; ++i) {
            uint256* phash = new uint256(InsecureRandBytes(32));
            CBlockIndex* pindexNext = GRC::MockBlockIndex::InsertBlockIndex(*phash);

            pindexNext->nHeight = i;
            pindexNext->nTime = pindex->nTime + 24 * 60 * 60;
            pindexNext->nVersion = 12;
            pindex->pnext = pindexNext;
            pindexNext->pprev = pindex;
            pindexNext->phashBlock = phash;
            mapBlockIndex[*phash] = pindexNext;
            pindex = pindexNext;
            hash = *phash;
        }

        // These tests were written with these assumptions. If this statement
        // does not hold true in the future, please update.
        assert(pindex->nHeight >= 7);
        assert(pindex->nHeight < 179);

        CTransaction tx;
        tx.nTime = pindexGenesisBlock->nTime;

        key.MakeNewKey(false);

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
    }

    ~Setup() {
        GRC::GetBeaconRegistry().Reset();
        // TODO: cleanup blockindex
        //delete pindex->phashBlock;
        //mapBlockIndex.erase(hash);
    }
};
} // Anonymous namespace

BOOST_FIXTURE_TEST_SUITE(MRC, Setup)

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

    mrc.m_mining_id = cpid;
    mrc.m_client_version = "6.0.0.0";
    mrc.m_last_block_hash = hash;

    BOOST_CHECK(!mrc.WellFormed()); 

    mrc.m_research_subsidy = 72;
    account.m_accrual = 72;

    mrc.Sign(key);
    BOOST_CHECK(mrc.WellFormed());

    BOOST_CHECK(!ValidateMRC(pindex, mrc));
    
    mrc.m_fee = mrc.ComputeMRCFee();
    BOOST_CHECK(ValidateMRC(pindex, mrc));

    mrc.m_research_subsidy = 9223372036854775807; // Get rich.
    BOOST_CHECK(!ValidateMRC(pindex, mrc));
    mrc.m_research_subsidy = 72;

    mrc.m_last_block_hash = pindex->pprev->GetBlockHash(); // Older request.
    mrc.Sign(key);
    BOOST_CHECK(!ValidateMRC(pindex, mrc));
    mrc.m_last_block_hash = hash;
    mrc.Sign(key);

    mrc.m_fee = 0; // Tax evasion.
    BOOST_CHECK(!ValidateMRC(pindex, mrc));

    pindexGenesisBlock->nHeight = 0;
}

BOOST_AUTO_TEST_SUITE_END()
