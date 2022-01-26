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

    Setup() {    
        CTransaction tx;
        tx.nTime = GetAdjustedTime();
        CPubKey pubkey(InsecureRandBytes(33));
        GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
                GRC::ContractAction::ADD,
                cpid,
                pubkey
        );

        GRC::GetBeaconRegistry().Add({contract, tx, nullptr});
        GRC::GetBeaconRegistry().ActivatePending({pubkey.GetID()}, tx.nTime, uint256(), -1);
        beacon = *GRC::GetBeaconRegistry().Try(cpid);

        pindex->pprev = pindex; // Should not be needed?
    }

    ~Setup() {
        GRC::GetBeaconRegistry().Reset();
        mapBlockIndex.erase(hash);
    }
};
} // Anonymous namespace

BOOST_FIXTURE_TEST_SUITE(MRC, Setup)

BOOST_AUTO_TEST_CASE(it_has_proper_payout_for_newbies)
{
    GRC::MRC mrc;
    mrc.m_mining_id = cpid;
    mrc.m_research_subsidy = 72;
    mrc.m_last_block_hash = hash;

    pindex->nTime = beacon.m_timestamp + Params().GetConsensus().MRCZeroPaymentInterval - 1;
    BOOST_CHECK_EQUAL(mrc.ComputeMRCFee(), mrc.m_research_subsidy);
    
    ++pindex->nTime;
    BOOST_CHECK_EQUAL(mrc.ComputeMRCFee(), 28);

    ++pindex->nTime;
    BOOST_CHECK_EQUAL(mrc.ComputeMRCFee(), 28);
}

BOOST_AUTO_TEST_SUITE_END()
