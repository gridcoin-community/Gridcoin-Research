// Copyright (c) 2022 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <gridcoin/account.h>
#include <gridcoin/beacon.h>
#include <gridcoin/cpid.h>
#include <gridcoin/mrc.h>
#include <gridcoin/tally.h>
#include <interfaces/gui_state.h>
#include <key.h>
#include <logging.h>
#include <miner.h>
#include <node/gui_cache.h>
#include <primitives/transaction.h>
#include <rpc/blockchain.h>
#include <test/state_guard.h>
#include <test/test_gridcoin.h>

#include <functional>
#include <list>
#include <string>
#include <vector>

namespace {
//! Collects the log lines containing "ERROR" while alive.
class CapturedErrors
{
public:
    std::vector<std::string> m_lines;

    CapturedErrors()
        : m_callback(LogInstance().PushBackCallback([this](const std::string& line) {
              if (line.find("ERROR") != std::string::npos) m_lines.push_back(line);
          }))
    {
    }

    ~CapturedErrors() { LogInstance().DeleteCallback(m_callback); }
    CapturedErrors(const CapturedErrors&) = delete;
    CapturedErrors& operator=(const CapturedErrors&) = delete;

private:
    std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

struct Setup {
    // First member: restores the tip globals, mock time and the arguments
    // this fixture forces, after the members below are gone. The destructor
    // used to leave pindexGenesisBlock pointing at the index it had just
    // freed and to write empty strings where the arguments were absent.
    grc_test::StateGuard guard;

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

        CMutableTransaction tx;
        tx.nTime = pindexGenesisBlock->nTime;
        CTransaction ctx_tx(tx);

        key.MakeNewKey(false);

        // Scoped to the key insertion alone. Everything below reaches cs_main
        // -- the beacon registry calls, and Researcher::Reload by way of
        // StoreResearcher -- so holding cs_wallet across them registers
        // wallet->cs_wallet -> cs_main, inverting the canonical
        // cs_main -> cs_wallet order that the test bodies below take with
        // LOCK2. Nothing after this needs the wallet lock.
        {
            LOCK(wallet->cs_wallet);
            BOOST_REQUIRE(wallet->AddKey(key));
        }

        GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
                GRC::ContractAction::ADD,
                cpid,
                key.GetPubKey()
        );

        account.m_first_block_ptr = pindexGenesisBlock;
        account.m_last_block_ptr = pindexGenesisBlock;

        GRC::GetBeaconRegistry().Add({contract, ctx_tx, pindexGenesisBlock});
        GRC::GetBeaconRegistry().ActivatePending({key.GetPubKey().GetID()}, tx.nTime, uint256(), 0);
        beacon = *GRC::GetBeaconRegistry().TryActive(cpid, pindex->nTime);

        SetMockTime(1);

        gArgs.ForceSetArg("forcecpid", cpid.ToString());
        GRC::Researcher::Reload({}, {});
    }

    ~Setup() {
        GRC::GetBeaconRegistry().Reset();
        gArgs.ForceSetArg("forcecpid", "");
        gArgs.ForceSetArg("email", "noncruncher");
        GRC::Researcher::Reload();
        gArgs.ForceSetArg("email", "");
        GRC::Tally::RemoveAccount(cpid);

        // Mock time and the tip pointers are restored by `guard` once this
        // destructor has freed the mock chain.
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

    {
        // Test runs single-threaded; pindexGenesisBlock is a Setup-fixture-
        // owned pointer, not the production global. Suppress the analyzer
        // warning rather than introducing a cs_main lock the test doesn't need.
        #if defined(__clang__)
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wthread-safety-analysis"
        #endif
        CBlockIndex* index{pindexGenesisBlock};
        while (index) {
            GRC::Tally::RecordRewardBlock(index);
            index = index->pnext;
        }
        #if defined(__clang__)
        #pragma clang diagnostic pop
        #endif
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

    LOCK(cs_main);

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
    CMutableTransaction mtxCoinstake;
    mtxCoinstake.vin.resize(1);
    mtxCoinstake.vout.resize(2);
    CMutableTransaction mtxCoinbase;
    std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>> mrc_map;
    std::map<GRC::Cpid, uint256> mrc_tx_map;

    account.m_accrual = 72;

    LOCK2(cs_main, wallet->cs_wallet);

    pindex->pprev->AddMRCResearcherContext(cpid, 72, 0.0);

    BOOST_CHECK(CreateRestOfTheBlock(block, mtxCoinbase, mtxCoinstake, pindex->pprev, mrc_map));

    GRC::MRC mrc;
    CAmount reward{0}, fee{0};
    GRC::CreateMRC(pindex->pprev, mrc, reward, fee, wallet);
    mrc_map[cpid] = {uint256{}, mrc};

    GRC::Claim claim;

    uint32_t claim_contract_version = 2;

    BOOST_CHECK(CreateGridcoinReward(mtxCoinbase, mtxCoinstake, block, pindex->pprev, reward, claim));

    BOOST_CHECK(CreateMRCRewards(mtxCoinbase, mtxCoinstake, block, mrc_map, mrc_tx_map, reward, claim_contract_version, claim, wallet));

    // TODO(div72): Separate this test into pieces and actually have it do
    // some useful testing by testing the validation logic against it.
    // Currently the miner code is too coupled together which makes this
    // infeasible without having a mess of spaghetti code for working
    // around this.
}

// The node's GUI state cache quotes the MRC request page's figures on every tip
// once synced, with a trial claim. For a researcher just paid, with nothing
// accrued, the trial's "no positive research reward pending" is routine, and must
// not write a default-level ERROR line per tip. The RPC and submit paths still do.
BOOST_AUTO_TEST_CASE(the_gui_cache_trial_with_nothing_accrued_logs_no_error)
{
    // Regtest opens the MRC version gate from genesis, so the quote reaches its
    // trial claim on this mock chain. The fixture's StateGuard puts the network,
    // the tip and the clock back.
    SelectParams(CBaseChainParams::REGTEST);
    account.m_accrual = 0;
    g_previous_block_time = GetAdjustedTime();

    LOCK(cs_main);
    pindexBest = pindex;
    nBestHeight = pindex->nHeight;

    // A synced, eligible researcher: what the cache's researcher part reports
    // for this fixture's active beacon.
    interfaces::GuiStateSnapshot snap;
    snap.out_of_sync = false;
    snap.has_researcher = true;
    snap.researcher.has_active_beacon = true;

    CapturedErrors errors;

    node::MakeProductionCompute().mrc(snap);

    BOOST_REQUIRE(snap.has_mrc);
    BOOST_CHECK(!snap.mrc.out_of_sync);
    BOOST_CHECK(snap.mrc.block_version_valid);
    BOOST_CHECK(snap.mrc.create_error);
    BOOST_CHECK(snap.mrc.create_error_msg.find("No positive research reward pending") != std::string::npos);
    BOOST_CHECK(errors.m_lines.empty());

    // The same claim through the RPC's and the submit's call still logs it.
    GRC::MRC mrc;
    CAmount reward{0}, fee{0};
    BOOST_CHECK_THROW(GRC::CreateMRC(pindex, mrc, reward, fee, wallet), GRC::MRC_error);
    BOOST_REQUIRE_EQUAL(errors.m_lines.size(), 1U);
    BOOST_CHECK(errors.m_lines[0].find("No positive research reward pending") != std::string::npos);
}

// A GUI's fee-boost quote (GuiState::requestMRCQuote) is the cache's trial claim
// at the boost asked: the same shared MRC snapshot code with that boost, with the
// researcher's eligibility read from the cache's snapshot, and as quiet.
BOOST_AUTO_TEST_CASE(the_gui_cache_quote_is_the_trial_at_the_boost_asked)
{
    // Regtest opens the MRC version gate on this mock chain (see above). The
    // fixture's StateGuard puts the network, the tip and the clock back.
    SelectParams(CBaseChainParams::REGTEST);
    account.m_accrual = 72;
    g_previous_block_time = GetAdjustedTime();

    LOCK(cs_main);
    // The block createmrc_creates_valid_mrcs claims at.
    pindexBest = pindex->pprev;
    nBestHeight = pindexBest->nHeight;

    // A synced, eligible researcher in the cache's snapshot.
    interfaces::GuiStateSnapshot current;
    current.out_of_sync = false;
    current.has_researcher = true;
    current.researcher.has_active_beacon = true;

    const node::GuiCache::Compute compute = node::MakeProductionCompute();
    CapturedErrors errors;

    // Unboosted, the trial gives the reward and the minimum fee.
    const interfaces::MRCSnapshot unboosted = compute.mrc_quote(0, current);
    BOOST_CHECK(!unboosted.out_of_sync);
    BOOST_CHECK(unboosted.block_version_valid);
    BOOST_REQUIRE_MESSAGE(!unboosted.create_error, unboosted.create_error_msg);
    BOOST_REQUIRE_GT(unboosted.reward, unboosted.min_fee);

    // Boosted, the quote asks for the minimum fee plus the boost, and reruns the
    // trial at that fee.
    const CAmount boost = unboosted.reward - unboosted.min_fee;
    interfaces::MRCSnapshot quote = compute.mrc_quote(boost, current);
    BOOST_CHECK(!quote.create_error);
    BOOST_CHECK(!quote.boosted_fee_error);
    BOOST_CHECK_EQUAL(quote.reward, unboosted.reward);
    BOOST_CHECK_EQUAL(quote.min_fee, unboosted.min_fee);
    BOOST_CHECK_EQUAL(quote.fee, unboosted.reward);

    // One unit more and the boosted rerun refuses the fee, in the quote alone.
    quote = compute.mrc_quote(boost + 1, current);
    BOOST_CHECK(quote.boosted_fee_error);
    BOOST_CHECK(quote.boosted_fee_error_msg.find("greater than the computed research reward") != std::string::npos);
    BOOST_CHECK_EQUAL(quote.fee, unboosted.reward + 1);
    BOOST_CHECK(errors.m_lines.empty());

    // Not an eligible researcher in the cache's snapshot: no trial at all.
    current.researcher.has_active_beacon = false;
    quote = compute.mrc_quote(boost, current);
    BOOST_CHECK(!quote.create_error);
    BOOST_CHECK_EQUAL(quote.reward, 0);
    BOOST_CHECK_EQUAL(quote.fee, 0);
}

BOOST_AUTO_TEST_CASE(terms_hold_only_when_paying_no_more_and_receiving_no_less)
{
    // The user confirmed paying at most 10 and receiving at least 90.
    BOOST_CHECK(GRC::MRCTermsHold(10, 100, 10, 90));
    BOOST_CHECK(!GRC::MRCTermsHold(11, 100, 10, 90)); // fee higher
    BOOST_CHECK(!GRC::MRCTermsHold(10, 99, 10, 90));  // net 89 < 90
    BOOST_CHECK(GRC::MRCTermsHold(9, 100, 10, 90));   // pays less, nets more
    BOOST_CHECK(!GRC::MRCTermsHold(10, 100, 10, 91)); // net 90 < 91
}

BOOST_AUTO_TEST_SUITE_END()
