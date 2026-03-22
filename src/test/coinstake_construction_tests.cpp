// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Tests for the coinstake construction pipeline: CreateGridcoinReward,
// CreateMRCRewards, SplitCoinStakeOutput, and round-trip serialization
// of the resulting transactions. These verify that the staking path
// produces correct outputs after CMutableTransaction conversion.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <main.h>
#include <miner.h>
#include <primitives/transaction.h>
#include <streams.h>
#include <gridcoin/beacon.h>
#include <gridcoin/claim.h>
#include <gridcoin/cpid.h>
#include <gridcoin/mrc.h>
#include <gridcoin/sidestake.h>
#include <gridcoin/tally.h>
#include <gridcoin/contract/contract.h>
#include <test/test_gridcoin.h>
#include <wallet/wallet.h>

namespace {

// Reuses the fixture pattern from mrc_tests.cpp with additions for
// coinstake construction testing (pindexBest, sidestake config, etc.)
struct CoinstakeSetup {
    CBlockIndex* pindex{nullptr};
    GRC::Cpid cpid = GRC::Cpid(InsecureRandBytes(16));
    GRC::ResearchAccount& account = GRC::Tally::CreateAccount(cpid);
    CWallet* wallet = new CWallet();
    GRC::Beacon beacon;
    CKey key;

    CoinstakeSetup()
    {
        GRC::GetBeaconRegistry().Reset();
        SelectParams(CBaseChainParams::MAIN);

        // Build a mock chain long enough for MRC interval requirements.
        uint256* phash = new uint256(InsecureRandBytes(32));
        pindex = GRC::MockBlockIndex::InsertBlockIndex(*phash);
        pindexGenesisBlock = pindex;
        pindex->nVersion = 12;
        pindex->phashBlock = phash;
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

        assert(pindex->nHeight >= 7);
        assert(pindex->nHeight < 179);

        CMutableTransaction tx;
        tx.nTime = pindexGenesisBlock->nTime;
        CTransaction ctx_tx(tx);

        key.MakeNewKey(false);

        {
            LOCK(wallet->cs_wallet);
            wallet->AddKey(key);
        }

        GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
            GRC::ContractAction::ADD,
            cpid,
            key.GetPubKey()
        );

        account.m_first_block_ptr = pindexGenesisBlock;
        account.m_last_block_ptr = pindexGenesisBlock;

        GRC::GetBeaconRegistry().Add({contract, ctx_tx, pindexGenesisBlock});
        GRC::GetBeaconRegistry().ActivatePending(
            {key.GetPubKey().GetID()}, tx.nTime, uint256(), 0);
        beacon = *GRC::GetBeaconRegistry().TryActive(cpid, pindex->nTime);

        SetMockTime(1);

        gArgs.ForceSetArg("forcecpid", cpid.ToString());
        GRC::Researcher::Reload({}, {});

        // Set pindexBest for SplitCoinStakeOutput
        pindexBest = pindex;
    }

    ~CoinstakeSetup()
    {
        pindexBest = nullptr;

        GRC::GetBeaconRegistry().Reset();
        GRC::GetSideStakeRegistry().ResetInMemoryOnly();

        gArgs.ForceSetArg("forcecpid", "");
        gArgs.ForceSetArg("email", "noncruncher");
        GRC::Researcher::Reload();
        gArgs.ForceSetArg("email", "");
        gArgs.ForceSetArg("-enablestakesplit", "");
        gArgs.ForceSetArg("-enablesidestaking", "");
        gArgs.ForceSetArg("-minstakesplitvalue", "");
        gArgs.ForceSetArg("-stakingefficiency", "");
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

    // Helper: build a minimal coinstake transaction with empty vout[0]
    // and a funded vout[1] using the fixture key.
    CMutableTransaction MakeCoinstake(CAmount stake_value)
    {
        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.nTime = pindex->nTime;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(uint256(InsecureRandBytes(32)), 0);

        CTxOut empty;
        empty.SetEmpty();
        mtx.vout.push_back(empty);

        CScript scriptPubKey;
        scriptPubKey << OP_DUP << OP_HASH160
                     << key.GetPubKey().GetID()
                     << OP_EQUALVERIFY << OP_CHECKSIG;
        mtx.vout.push_back(CTxOut(stake_value, scriptPubKey));

        return mtx;
    }

    // Helper: build a minimal coinbase transaction.
    CMutableTransaction MakeCoinbase(CAmount fees = 0)
    {
        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.nTime = pindex->nTime;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.SetNull();
        mtx.vin[0].scriptSig = CScript() << 0;
        mtx.vout.push_back(CTxOut(fees, CScript()));

        return mtx;
    }

    // Helper: build a CBlock with version 12 and the given coinbase/coinstake.
    CBlock MakeBlock()
    {
        CBlock block;
        block.nVersion = 12;
        block.nTime = pindex->nTime;
        block.vtx.resize(2);

        return block;
    }
};

static CScript P2PKH(const CKeyID& id)
{
    CScript s;
    s << OP_DUP << OP_HASH160 << id << OP_EQUALVERIFY << OP_CHECKSIG;
    return s;
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(coinstake_construction_tests, CoinstakeSetup)

// --------------------------------------------------------------------------
// Test 1: Pure structural — verify coinstake detection and round-trip.
// --------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(basic_coinstake_structure)
{
    CMutableTransaction mtx = MakeCoinstake(10000 * COIN);

    CTransaction ctx(mtx);
    BOOST_CHECK(ctx.IsCoinStake());
    BOOST_CHECK(!ctx.IsCoinBase());
    BOOST_CHECK_EQUAL(ctx.vout.size(), 2u);
    BOOST_CHECK(ctx.vout[0].IsEmpty());
    BOOST_CHECK_EQUAL(ctx.vout[1].nValue, 10000 * COIN);

    // Round-trip serialize/deserialize
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << ctx;
    CTransaction ctx2;
    ss >> ctx2;

    BOOST_CHECK(ctx2.GetHash() == ctx.GetHash());
    BOOST_CHECK(ctx2.IsCoinStake());
}

// --------------------------------------------------------------------------
// Test 2: CreateGridcoinReward adds reward to coinstake.
// --------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(gridcoin_reward_application)
{
    account.m_accrual = 72;

    CMutableTransaction coinbase = MakeCoinbase(1000); // 1000 Halfords fee
    CMutableTransaction coinstake = MakeCoinstake(5000 * COIN);
    CBlock block = MakeBlock();

    CAmount original_stake = coinstake.vout[1].nValue;
    int64_t nReward = 0;
    GRC::Claim claim;

    LOCK(cs_main);
    BOOST_CHECK(CreateGridcoinReward(block, pindex->pprev, nReward, claim, coinbase, coinstake));

    // Reward should be positive (block subsidy + fees at minimum)
    BOOST_CHECK(nReward > 0);
    BOOST_CHECK(claim.m_block_subsidy > 0);

    // Coinstake output should have increased by the reward
    BOOST_CHECK_EQUAL(coinstake.vout[1].nValue, original_stake + nReward);

    // Coinbase fees should have been emptied
    BOOST_CHECK(coinbase.vout[0].IsEmpty());
}

// --------------------------------------------------------------------------
// Test 3: CreateMRCRewards appends MRC outputs and embeds claim contract.
// --------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(mrc_outputs_appended)
{
    account.m_accrual = 72;

    CMutableTransaction coinbase = MakeCoinbase();
    CMutableTransaction coinstake = MakeCoinstake(5000 * COIN);
    CBlock block = MakeBlock();
    block.vtx.resize(2);

    std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>> mrc_map;
    std::map<GRC::Cpid, uint256> mrc_tx_map;

    LOCK2(cs_main, wallet->cs_wallet);

    // First create the reward so the claim is populated
    int64_t nReward = 0;
    GRC::Claim claim;
    BOOST_CHECK(CreateGridcoinReward(block, pindex->pprev, nReward, claim, coinbase, coinstake));

    // Add MRC researcher context and create an MRC
    pindex->pprev->AddMRCResearcherContext(cpid, 72, 0.0);
    BOOST_CHECK(CreateRestOfTheBlock(block, pindex->pprev, mrc_map, coinbase, coinstake));

    GRC::MRC mrc;
    CAmount mrc_reward{0}, mrc_fee{0};
    GRC::CreateMRC(pindex->pprev, mrc, mrc_reward, mrc_fee, wallet);
    mrc_map[cpid] = {uint256{}, mrc};

    uint32_t claim_contract_version = 2;
    size_t vout_before = coinstake.vout.size();

    BOOST_CHECK(CreateMRCRewards(block, mrc_map, mrc_tx_map, nReward,
                                 claim_contract_version, claim, wallet,
                                 coinbase, coinstake));

    // MRC outputs should have been appended to coinstake
    BOOST_CHECK(coinstake.vout.size() > vout_before);

    // Coinbase should now have the claim contract
    BOOST_CHECK(!coinbase.vContracts.empty());
}

// --------------------------------------------------------------------------
// Test 4: SplitCoinStakeOutput splits large stakes into multiple outputs.
// --------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(split_coinstake_distributes_reward)
{
    gArgs.ForceSetArg("-enablestakesplit", "1");
    gArgs.ForceSetArg("-minstakesplitvalue", "800");
    gArgs.ForceSetArg("-stakingefficiency", "98");

    CMutableTransaction coinstake = MakeCoinstake(10000 * COIN);
    CBlock block = MakeBlock();

    CAmount total_before = coinstake.vout[1].nValue;
    int64_t nReward = 100 * COIN; // Simulate reward portion

    bool fEnableStakeSplit = true;
    bool fEnableSideStaking = false;
    int64_t nMinStakeSplitValue = 800;
    double dEfficiency = 98.0;

    SplitCoinStakeOutput(block, nReward, fEnableStakeSplit, fEnableSideStaking,
                         nMinStakeSplitValue, dEfficiency, coinstake);

    // Should have split into multiple outputs
    BOOST_CHECK(coinstake.vout.size() > 2);

    // vout[0] must still be empty (coinstake marker)
    BOOST_CHECK(coinstake.vout[0].IsEmpty());

    // All outputs must sum to the original total
    CAmount total_after = 0;
    for (const auto& out : coinstake.vout) {
        total_after += out.nValue;
    }
    BOOST_CHECK_EQUAL(total_after, total_before);
}

// --------------------------------------------------------------------------
// Test 5: SplitCoinStakeOutput creates sidestake outputs.
// --------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(sidestake_outputs_placed)
{
    // Create a different key for the sidestake destination so it differs
    // from the coinstake address (otherwise the sidestake is skipped).
    CKey sidestakeKey;
    sidestakeKey.MakeNewKey(true);
    CTxDestination sidestakeDest = sidestakeKey.GetPubKey().GetID();

    GRC::LocalSideStake sidestake(
        sidestakeDest,
        GRC::Allocation(0.10), // 10%
        "test sidestake",
        GRC::LocalSideStake::LocalSideStakeStatus::ACTIVE
    );
    GRC::GetSideStakeRegistry().NonContractAdd(sidestake, false);

    CMutableTransaction coinstake = MakeCoinstake(10000 * COIN);
    CBlock block = MakeBlock();

    int64_t nReward = 100 * COIN;
    bool fEnableStakeSplit = false;
    bool fEnableSideStaking = true;
    int64_t nMinStakeSplitValue = 800;
    double dEfficiency = 98.0;

    SplitCoinStakeOutput(block, nReward, fEnableStakeSplit, fEnableSideStaking,
                         nMinStakeSplitValue, dEfficiency, coinstake);

    // Should have at least 3 outputs: empty + coinstake + sidestake
    BOOST_CHECK(coinstake.vout.size() >= 3);

    // vout[0] must be empty
    BOOST_CHECK(coinstake.vout[0].IsEmpty());

    // Check that a sidestake output exists with approximately the right value
    CScript expectedScript;
    expectedScript.SetDestination(sidestakeDest);

    bool found_sidestake = false;
    CAmount expected_sidestake = (GRC::Allocation(0.10) * nReward).ToCAmount();
    for (size_t i = 1; i < coinstake.vout.size(); ++i) {
        if (coinstake.vout[i].scriptPubKey == expectedScript) {
            BOOST_CHECK_EQUAL(coinstake.vout[i].nValue, expected_sidestake);
            found_sidestake = true;
        }
    }
    BOOST_CHECK(found_sidestake);

    // All outputs must sum to original total
    CAmount total = 0;
    for (const auto& out : coinstake.vout) {
        total += out.nValue;
    }
    BOOST_CHECK_EQUAL(total, 10000 * COIN);
}

// --------------------------------------------------------------------------
// Test 6: Full pipeline round-trip — reward, MRC, split, then serialize.
// --------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(full_pipeline_roundtrip)
{
    account.m_accrual = 72;

    gArgs.ForceSetArg("-enablestakesplit", "1");
    gArgs.ForceSetArg("-minstakesplitvalue", "800");
    gArgs.ForceSetArg("-stakingefficiency", "98");

    CMutableTransaction coinbase = MakeCoinbase();
    CMutableTransaction coinstake = MakeCoinstake(5000 * COIN);
    CBlock block = MakeBlock();
    block.vtx.resize(2);

    std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>> mrc_map;
    std::map<GRC::Cpid, uint256> mrc_tx_map;

    LOCK2(cs_main, wallet->cs_wallet);

    // Step 1: CreateGridcoinReward
    int64_t nReward = 0;
    GRC::Claim claim;
    BOOST_CHECK(CreateGridcoinReward(block, pindex->pprev, nReward, claim, coinbase, coinstake));

    // Step 2: CreateMRCRewards
    pindex->pprev->AddMRCResearcherContext(cpid, 72, 0.0);
    BOOST_CHECK(CreateRestOfTheBlock(block, pindex->pprev, mrc_map, coinbase, coinstake));

    GRC::MRC mrc;
    CAmount mrc_reward{0}, mrc_fee{0};
    GRC::CreateMRC(pindex->pprev, mrc, mrc_reward, mrc_fee, wallet);
    mrc_map[cpid] = {uint256{}, mrc};

    uint32_t claim_contract_version = 2;
    CreateMRCRewards(block, mrc_map, mrc_tx_map, nReward,
                     claim_contract_version, claim, wallet,
                     coinbase, coinstake);

    // Step 3: SplitCoinStakeOutput
    bool fEnableStakeSplit = true;
    bool fEnableSideStaking = false;
    int64_t nMinStakeSplitValue = 800;
    double dEfficiency = 98.0;
    SplitCoinStakeOutput(block, nReward, fEnableStakeSplit, fEnableSideStaking,
                         nMinStakeSplitValue, dEfficiency, coinstake);

    // Convert to CTransaction and verify round-trip
    CTransaction ctx(coinstake);
    uint256 hash_before = ctx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << ctx;
    CTransaction ctx2;
    ss >> ctx2;

    BOOST_CHECK(ctx2.GetHash() == hash_before);
    BOOST_CHECK(ctx2.IsCoinStake());

    // Also verify the coinbase round-trips with its claim contract
    CTransaction cb(coinbase);
    uint256 cb_hash = cb.GetHash();

    CDataStream ss2(SER_NETWORK, PROTOCOL_VERSION);
    ss2 << cb;
    CTransaction cb2;
    ss2 >> cb2;

    BOOST_CHECK(cb2.GetHash() == cb_hash);
}

// --------------------------------------------------------------------------
// Test 7: Claim contract survives coinbase conversion and round-trip.
// --------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(coinbase_claim_contract_preserved)
{
    // Build a claim with identifiable fields
    GRC::Claim claim;
    claim.m_mining_id = GRC::MiningId::ForNoncruncher();
    claim.m_block_subsidy = 10 * COIN;
    claim.m_client_version = "test_version";
    claim.m_organization = "test_org";

    // Build a coinbase with the claim contract
    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vContracts.emplace_back(GRC::MakeContract<GRC::Claim>(
        2, // claim contract version
        GRC::ContractAction::ADD,
        std::move(claim)));

    BOOST_CHECK_EQUAL(coinbase.vContracts.size(), 1u);
    BOOST_CHECK(coinbase.vContracts[0].m_type == GRC::ContractType::CLAIM);

    // Convert to CTransaction
    CTransaction ctx(coinbase);
    BOOST_CHECK_EQUAL(ctx.vContracts.size(), 1u);
    BOOST_CHECK(ctx.vContracts[0].m_type == GRC::ContractType::CLAIM);

    // Serialize → deserialize
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << ctx;
    CTransaction ctx2;
    ss >> ctx2;

    BOOST_CHECK(ctx2.GetHash() == ctx.GetHash());
    BOOST_CHECK_EQUAL(ctx2.vContracts.size(), 1u);
    BOOST_CHECK(ctx2.vContracts[0].m_type == GRC::ContractType::CLAIM);
    BOOST_CHECK(ctx2.vContracts[0].m_action == GRC::ContractAction::ADD);
}

BOOST_AUTO_TEST_SUITE_END()
