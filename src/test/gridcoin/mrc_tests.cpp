// Copyright (c) 2022 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <gridcoin/account.h>
#include <gridcoin/beacon.h>
#include <gridcoin/claim.h>
#include <gridcoin/cpid.h>
#include <gridcoin/mrc.h>
#include <gridcoin/tally.h>
#include <interfaces/gui_state.h>
#include <interfaces/mrc.h>
#include <key.h>
#include <logging.h>
#include <miner.h>
#include <node/gui_cache.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <random.h>
#include <rpc/blockchain.h>
#include <support/allocators/secure.h>
#include <test/state_guard.h>
#include <txmempool.h>
#include <test/test_gridcoin.h>
#include <util.h>
#include <wallet/wallet.h>

#include <memory>

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

// MakeTx, MakeMrcTx and MakeEntry as txmempool_tests has them: a mempool entry
// for an MRC at a given fee and CPID.

//! Build a v2 transaction carrying the supplied contracts (if any).
CTransaction MakeTx(const std::vector<GRC::Contract>& contracts = {},
                    const std::vector<CTxIn>& vin = {},
                    const std::vector<CTxOut>& vout = {})
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin = vin;
    mtx.vout = vout;
    mtx.vContracts = contracts;
    return CTransaction(mtx);
}

//! Build a v2 transaction carrying a single MRC contract for the given CPID.
CTransaction MakeMrcTx(const GRC::Cpid& cpid, CAmount fee, const uint256& last_block = uint256())
{
    GRC::MRC mrc;
    mrc.m_mining_id = cpid;
    mrc.m_fee = fee;
    mrc.m_last_block_hash = last_block;
    return MakeTx({GRC::MakeContract<GRC::MRC>(GRC::ContractAction::ADD, mrc)});
}

CTxMemPoolEntry MakeEntry(const CTransaction& tx)
{
    return CTxMemPoolEntry(tx, 0, 0, 0, ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION));
}

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

    //! Encrypt the wallet and unlock it for staking only, as a researcher who
    //! stakes runs it.
    void EncryptAndUnlockForStakingOnly()
    {
        const SecureString passphrase("mrc-tests");

        BOOST_REQUIRE(wallet->EncryptWallet(passphrase));
        wallet->Lock();
        BOOST_REQUIRE(wallet->Unlock(passphrase, UnlockScope::StakingOnly));
        BOOST_REQUIRE(wallet->IsUnlockedForStakingOnly());

        // The general route to the beacon key is refused from here on, so a
        // signature a case gets can only have come through a staking path.
        CKey general;
        BOOST_REQUIRE(!wallet->GetKey(key.GetPubKey().GetID(), general));
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

// The quote also measures the request against the mempool's MRC queue, with
// GRC::MeasureMRCQueue at the fee the request would carry: the minimum fee
// unboosted, the boosted fee once the boosted rerun has set it. The queue built
// here gives the head, tail and pay-limit fees three different values, and a
// position that is not the queue length.
BOOST_AUTO_TEST_CASE(the_gui_cache_quote_carries_the_queue_position)
{
    // The fixture's StateGuard leaves the mempool as it is; this one empties it
    // on entry and on exit, so no entry for the fixture's CPID outlives the case.
    grc_test::MempoolStateGuard mempool_guard;

    // Regtest opens the MRC version gate on this mock chain. The accrual is
    // large enough that the minimum fee is not rounded away.
    SelectParams(CBaseChainParams::REGTEST);
    account.m_accrual = 1000 * COIN;
    g_previous_block_time = GetAdjustedTime();

    LOCK(cs_main);
    pindexBest = pindex->pprev;
    nBestHeight = pindexBest->nHeight;

    interfaces::GuiStateSnapshot current;
    current.out_of_sync = false;
    current.has_researcher = true;
    current.researcher.has_active_beacon = true;

    const node::GuiCache::Compute compute = node::MakeProductionCompute();

    // The empty pool gives the request's minimum fee m, the reward r and the
    // block's MRC output limit L.
    const interfaces::MRCSnapshot empty = compute.mrc_quote(0, current);
    BOOST_REQUIRE_MESSAGE(!empty.create_error, empty.create_error_msg);
    const CAmount m = empty.min_fee;
    const CAmount r = empty.reward;
    const int L = empty.output_limit;
    BOOST_REQUIRE_GE(L, 1);
    BOOST_REQUIRE_GE(m, 3);
    BOOST_REQUIRE_GE(r - m, L + 3);

    // L + 1 foreign entries above m (m + 2 to m + L + 2), one foreign entry
    // below it (m - 1), then the fixture's own CPID at m - 2, the tail.
    for (int i = 0; i <= L; ++i) {
        const CTransaction tx = MakeMrcTx(GRC::Cpid(InsecureRandBytes(16)), m + 2 + i);
        BOOST_REQUIRE(mempool.addUnchecked(tx.GetHash(), MakeEntry(tx)));
    }
    const CTransaction below = MakeMrcTx(GRC::Cpid(InsecureRandBytes(16)), m - 1);
    BOOST_REQUIRE(mempool.addUnchecked(below.GetHash(), MakeEntry(below)));
    const CTransaction own = MakeMrcTx(cpid, m - 2);
    BOOST_REQUIRE(mempool.addUnchecked(own.GetHash(), MakeEntry(own)));

    // The pay-limit slot is descending index L - 1. Head, tail and pay limit
    // differ from one another, and pos (L + 1) differs from the length (L + 3).
    const CAmount head = m + L + 2;
    const CAmount tail = m - 2;
    const CAmount pay_limit = m + 3;
    BOOST_REQUIRE(head != tail && head != pay_limit && tail != pay_limit);

    // Unboosted, the request carries m: the L + 1 entries above it are ahead,
    // and the m - 1 entry is ahead of the own entry but below the fee.
    const interfaces::MRCSnapshot quote = compute.mrc_quote(0, current);
    BOOST_REQUIRE_MESSAGE(!quote.create_error, quote.create_error_msg);
    BOOST_CHECK_EQUAL(quote.queue_length, L + 3);
    BOOST_CHECK_EQUAL(quote.pos, L + 1);
    BOOST_CHECK_EQUAL(quote.found_in_queue, true);
    BOOST_CHECK_EQUAL(quote.queue_head_fee, head);
    BOOST_CHECK_EQUAL(quote.queue_tail_fee, tail);
    BOOST_CHECK_EQUAL(quote.queue_pay_limit_fee, pay_limit);

    // The same six values as the function gives for the mempool's queue.
    const GRC::MRCQueuePosition measured = GRC::MeasureMRCQueue(mempool.GetMRCQueue(), cpid, m, L);
    BOOST_CHECK_EQUAL(quote.queue_length, measured.queue_length);
    BOOST_CHECK_EQUAL(quote.pos, measured.pos);
    BOOST_CHECK_EQUAL(quote.found_in_queue, measured.found);
    BOOST_CHECK_EQUAL(quote.queue_head_fee, measured.head_fee);
    BOOST_CHECK_EQUAL(quote.queue_tail_fee, measured.tail_fee);
    BOOST_CHECK_EQUAL(quote.queue_pay_limit_fee, measured.pay_limit_fee);

    // Boosted to the reward r, above the head, the request has nothing ahead.
    // CreateMRC takes a fee up to the reward, inclusive.
    const interfaces::MRCSnapshot boosted = compute.mrc_quote(r - m, current);
    BOOST_REQUIRE_MESSAGE(!boosted.boosted_fee_error, boosted.boosted_fee_error_msg);
    BOOST_CHECK_EQUAL(boosted.pos, 0);
    BOOST_CHECK_EQUAL(boosted.found_in_queue, true);
}

// DESIGN rule: a submit whose rebuilt fee is higher, or whose net payout is lower,
// than the terms the user confirmed is refused with the fresh figures and
// broadcasts nothing. This runs the real MRCImpl::Submit on the mock chain of the
// quote case above, with the fixture wallet signing the rebuilt claim.
BOOST_AUTO_TEST_CASE(submit_refuses_a_higher_fee_than_confirmed_and_broadcasts_nothing)
{
    SelectParams(CBaseChainParams::REGTEST);
    account.m_accrual = 72;
    g_previous_block_time = GetAdjustedTime();

    LOCK(cs_main);
    pindexBest = pindex->pprev;
    nBestHeight = pindexBest->nHeight;

    interfaces::GuiStateSnapshot current;
    current.out_of_sync = false;
    current.has_researcher = true;
    current.researcher.has_active_beacon = true;

    const node::GuiCache::Compute compute = node::MakeProductionCompute();
    // The fee the user confirms is the quoted one: the minimum plus a boost of one
    // unit (the mock chain's minimum fee is zero).
    constexpr CAmount boost = 1;
    const interfaces::MRCSnapshot quote = compute.mrc_quote(boost, current);
    BOOST_REQUIRE_MESSAGE(!quote.create_error, quote.create_error_msg);
    BOOST_REQUIRE(!quote.boosted_fee_error);
    BOOST_REQUIRE_GT(quote.reward, quote.fee);
    BOOST_REQUIRE_EQUAL(quote.fee, quote.min_fee + boost);

    const size_t wallet_txs = WITH_LOCK(wallet->cs_wallet, return wallet->mapWallet.size());
    const unsigned long pool_txs = mempool.size();

    const std::unique_ptr<interfaces::MRC> mrc = interfaces::MakeMRC(wallet);

    // Fee: the user confirmed one unit below what the rebuilt claim pays.
    interfaces::MRCSubmitResult res = mrc->submitConfirmed(boost, /*wallet_locked=*/false,
                                                           quote.fee - 1, quote.reward - quote.fee);
    BOOST_CHECK(res.terms_changed);
    BOOST_CHECK(!res.success);
    BOOST_CHECK(!res.error.empty());
    BOOST_CHECK_EQUAL(res.fresh_fee, quote.fee);
    BOOST_CHECK_EQUAL(res.fresh_reward, quote.reward);
    BOOST_CHECK_EQUAL(WITH_LOCK(wallet->cs_wallet, return wallet->mapWallet.size()), wallet_txs);
    BOOST_CHECK_EQUAL(mempool.size(), pool_txs);

    // Net payout: the fee holds, the user confirmed one unit more than it nets.
    res = mrc->submitConfirmed(boost, /*wallet_locked=*/false, quote.fee, quote.reward - quote.fee + 1);
    BOOST_CHECK(res.terms_changed);
    BOOST_CHECK(!res.success);
    BOOST_CHECK_EQUAL(res.fresh_fee, quote.fee);
    BOOST_CHECK_EQUAL(res.fresh_reward, quote.reward);
    BOOST_CHECK_EQUAL(WITH_LOCK(wallet->cs_wallet, return wallet->mapWallet.size()), wallet_txs);
    BOOST_CHECK_EQUAL(mempool.size(), pool_txs);
}

// DESIGN rule: a user action re-checks the state in the core. The GUI's eligibility
// comes from a pushed snapshot that can be seconds old, so the core refuses a
// submit while the tip has fallen behind, with nothing built or sent. The fixture's
// StateGuard puts the clock back.
BOOST_AUTO_TEST_CASE(submit_refuses_while_out_of_sync_and_broadcasts_nothing)
{
    SelectParams(CBaseChainParams::REGTEST);
    account.m_accrual = 72;
    g_previous_block_time = GetAdjustedTime();

    LOCK(cs_main);
    pindexBest = pindex->pprev;
    nBestHeight = pindexBest->nHeight;

    interfaces::GuiStateSnapshot current;
    current.out_of_sync = false;
    current.has_researcher = true;
    current.researcher.has_active_beacon = true;

    const node::GuiCache::Compute compute = node::MakeProductionCompute();
    constexpr CAmount boost = 1;
    const interfaces::MRCSnapshot quote = compute.mrc_quote(boost, current);
    BOOST_REQUIRE_MESSAGE(!quote.create_error, quote.create_error_msg);
    BOOST_REQUIRE(!quote.boosted_fee_error);
    BOOST_REQUIRE_GT(quote.reward, quote.fee);

    const size_t wallet_txs = WITH_LOCK(wallet->cs_wallet, return wallet->mapWallet.size());
    const unsigned long pool_txs = mempool.size();

    const std::unique_ptr<interfaces::MRC> mrc = interfaces::MakeMRC(wallet);

    // The tip is now 900 s old (the limit), so the node is out of sync.
    SetMockTime(GetTime() + 900);
    BOOST_REQUIRE(OutOfSyncByAge());

    const interfaces::MRCSubmitResult res = mrc->submitConfirmed(boost, /*wallet_locked=*/false,
                                                                 quote.fee, quote.reward - quote.fee);
    BOOST_CHECK(!res.success);
    BOOST_CHECK(!res.terms_changed);
    BOOST_CHECK_EQUAL(res.error, "The wallet is out of sync; the claim was not sent.");
    BOOST_CHECK_EQUAL(WITH_LOCK(wallet->cs_wallet, return wallet->mapWallet.size()), wallet_txs);
    BOOST_CHECK_EQUAL(mempool.size(), pool_txs);
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

// GRC::MeasureMRCQueue on queues built by hand, fees in satoshis. pos counts
// the entries ahead of the own entry whose fee is at least the request's, so a
// tie counts and an entry after the own one does not. The pay limit is the fee
// at index min(size, limit) - 1, capped at the head fee.
BOOST_AUTO_TEST_CASE(the_queue_position_counts_ties_ahead_of_the_own_entry)
{
    using Queue = std::vector<std::pair<CAmount, GRC::Cpid>>;
    const auto make_cpid = [](unsigned char byte) { return GRC::Cpid(std::vector<unsigned char>(16, byte)); };
    const GRC::Cpid a = make_cpid(0x0a);
    const GRC::Cpid b = make_cpid(0x0b);
    const GRC::Cpid c = make_cpid(0x0c);
    const GRC::Cpid d = make_cpid(0x0d);
    const GRC::Cpid e = make_cpid(0x0e);

    // An empty queue: nothing ahead, and all three fees are 0.
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{}, a, 4, 2);
        BOOST_CHECK_EQUAL(q.queue_length, 0);
        BOOST_CHECK_EQUAL(q.pos, 0);
        BOOST_CHECK_EQUAL(q.found, false);
        BOOST_CHECK_EQUAL(q.head_fee, 0);
        BOOST_CHECK_EQUAL(q.tail_fee, 0);
        BOOST_CHECK_EQUAL(q.pay_limit_fee, 0);
    }

    // No own entry: 5 and 4 are at least the fee 4 (a tie counts), 3 is not.
    // The pay limit is queue[1].
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{{5, a}, {4, b}, {3, c}}, d, 4, 2);
        BOOST_CHECK_EQUAL(q.queue_length, 3);
        BOOST_CHECK_EQUAL(q.pos, 2);
        BOOST_CHECK_EQUAL(q.found, false);
        BOOST_CHECK_EQUAL(q.head_fee, 5);
        BOOST_CHECK_EQUAL(q.tail_fee, 3);
        BOOST_CHECK_EQUAL(q.pay_limit_fee, 4);
    }

    // The own entry is present: above the head nothing is ahead; at the head
    // fee the head counts.
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{{5, a}, {4, d}}, d, 6, 2);
        BOOST_CHECK_EQUAL(q.pos, 0);
        BOOST_CHECK_EQUAL(q.found, true);
    }
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{{5, a}, {4, d}}, d, 5, 2);
        BOOST_CHECK_EQUAL(q.pos, 1);
        BOOST_CHECK_EQUAL(q.found, true);
    }

    // Ties: both entries at the request's fee count; above it neither does.
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{{4, a}, {4, b}}, c, 4, 2);
        BOOST_CHECK_EQUAL(q.pos, 2);
    }
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{{4, a}, {4, b}}, c, 5, 2);
        BOOST_CHECK_EQUAL(q.pos, 0);
    }

    // An entry after the own one is not counted, though its fee is the
    // request's.
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{{5, a}, {4, d}, {3, e}}, d, 3, 2);
        BOOST_CHECK_EQUAL(q.pos, 1);
        BOOST_CHECK_EQUAL(q.found, true);
    }

    // A limit inside the queue takes the slot at limit - 1; a limit beyond it
    // takes the last entry.
    const Queue four{{9, a}, {7, b}, {5, c}, {3, d}};
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(four, e, 1, 2);
        BOOST_CHECK_EQUAL(q.queue_length, 4);
        BOOST_CHECK_EQUAL(q.pay_limit_fee, 7);
    }
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(four, e, 1, 10);
        BOOST_CHECK_EQUAL(q.queue_length, 4);
        BOOST_CHECK_EQUAL(q.pay_limit_fee, 3);
    }

    // A limit of 0 has no paid slot: the pay limit is the head fee.
    {
        const GRC::MRCQueuePosition q = GRC::MeasureMRCQueue(Queue{{5, a}}, b, 1, 0);
        BOOST_CHECK_EQUAL(q.pay_limit_fee, 5);
        BOOST_CHECK_EQUAL(q.head_fee, 5);
        BOOST_CHECK_EQUAL(q.tail_fee, 5);
    }
}

//! A researcher staking from a wallet unlocked for staking only signs the claim
//! in each block it stakes with its beacon key.
BOOST_AUTO_TEST_CASE(claim_signed_under_staking_only)
{
    EncryptAndUnlockForStakingOnly();

    CMutableTransaction coinstake;
    coinstake.nTime = pindex->nTime;
    coinstake.vin.resize(1);
    coinstake.vin[0].prevout = COutPoint(GetRandHash(), 0);
    coinstake.vout.resize(2);
    coinstake.vout[0].SetEmpty();

    CBlock block;
    block.nTime = pindex->nTime;
    block.hashPrevBlock = pindex->pprev->GetBlockHash();
    block.vtx.resize(2);
    block.vtx[1] = CTransaction(coinstake);

    GRC::Claim claim;
    claim.m_mining_id = cpid;

    LOCK(cs_main);

    // The dry run that CreateMRCRewards makes before it commits to a research
    // claim checks only that the key can be had.
    BOOST_CHECK(TrySignClaim(wallet, claim, block, /*dry_run=*/true));
    BOOST_CHECK(claim.m_signature.empty());

    BOOST_REQUIRE(TrySignClaim(wallet, claim, block, /*dry_run=*/false));
    BOOST_CHECK(claim.VerifySignature(key.GetPubKey(), block.hashPrevBlock, block.vtx[1]));
}

//! The MRC request page's preview, on a researcher wallet unlocked for staking
//! only, shows the fee and no error. Its trial claim is built unsigned: the
//! preview needs the fee, not a signature, and the general route to the beacon
//! key is refused under staking only.
BOOST_AUTO_TEST_CASE(mrc_snapshot_no_error_under_staking_only)
{
    // Large enough that the fee, a small fraction of it under regtest's short
    // zero-payout interval, is not rounded away.
    const CAmount accrual = 1000 * COIN;
    account.m_accrual = accrual;
    EncryptAndUnlockForStakingOnly();

    // The snapshot reaches CreateMRC only from the V12 height, which regtest
    // puts at genesis and mainnet far above this mock chain. The fixture's
    // guard puts the network back.
    SelectParams(CBaseChainParams::REGTEST);

    {
        LOCK(cs_main);
        pindexBest = pindex;
    }

    // In sync.
    g_previous_block_time.store(GetAdjustedTime());

    const std::unique_ptr<interfaces::MRC> mrc_interface = interfaces::MakeMRC(wallet);
    const interfaces::MRCSnapshot snap = mrc_interface->snapshot(/*fee_boost=*/0,
                                                                 /*wallet_locked=*/false,
                                                                 /*researcher_eligible=*/true);

    BOOST_REQUIRE(!snap.out_of_sync);
    BOOST_REQUIRE(snap.block_version_valid);
    BOOST_CHECK_MESSAGE(!snap.create_error, snap.create_error_msg);
    BOOST_CHECK_EQUAL(snap.reward, accrual);
    BOOST_CHECK(snap.min_fee > 0);
    BOOST_CHECK(snap.min_fee < snap.reward);
}

BOOST_AUTO_TEST_SUITE_END()
