// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! Which MRCs in the memory pool the miner binds to the block it assembles
//! (CreateRestOfTheBlock), and what the pool does with a second MRC for a CPID,
//! on the fixture's regtest chain.
//!
//! gridcoin/mrc_tests.cpp checks the rules an MRC is selected by on a mock
//! chain with an empty pool. The cases here work with signed, funded MRC
//! transactions.

#include "amount.h"
#include "chain.h"
#include "consensus/validation.h"
#include "gridcoin/account.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/cpid.h"
#include "gridcoin/mrc.h"
#include "gridcoin/tally.h"
#include "key.h"
#include "miner.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "random.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"
#include "txmempool.h"
#include "uint256.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

using grc_test::AddToMempool;
using grc_test::CreateSpendWithContract;
using grc_test::MakeBlockAndCoinstake;
using grc_test::PremineCoinbase;
using grc_test::SpendablePremineOutputs;

namespace {
using MrcMap = std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>;

//! The accrual each researcher of a case holds, and the subsidy and fee of an MRC that claims all of it. The fee
//! leaves the foundation a share and is more than the fee that is due.
constexpr CAmount ACCRUAL = 10000;
constexpr CAmount FEE = 4000;

//!
//! \brief Researchers whose MRCs the miner can bind: each a CPID with an active beacon and an accrual at the chain's
//! tip. Everything the fixture puts in the beacon registry, the tally and the pool is taken out again when it goes.
//!
//! The beacons and the accounts are put in place directly. Nothing is mined, so the tip stays where the suite's
//! fixture left it.
//!
class Researchers
{
public:
    Researchers()
    {
        LOCK(cs_main);

        mempool.clear();
        GRC::GetBeaconRegistry().Reset();

        // An account's last reward block has to be below the current superblock's height for the accrual the
        // account holds to count. The fixture chain has no superblock, so the last reward is a block index of the
        // fixture's own at height -1, thirty days before the tip.
        m_reward_index.phashBlock = &m_reward_hash;
        m_reward_index.nHeight = -1;
        m_reward_index.nTime = pindexBest->nTime - 30 * 24 * 60 * 60;
    }

    ~Researchers()
    {
        LOCK(cs_main);

        mempool.clear();
        GRC::GetBeaconRegistry().Reset();

        for (const GRC::Cpid& cpid : m_cpids) {
            GRC::Tally::RemoveAccount(cpid);
        }
    }

    //! Add a researcher whose beacon carries \p key.
    GRC::Cpid Add(const CKey& key)
    {
        LOCK(cs_main);

        const GRC::Cpid cpid(InsecureRandBytes(16));

        const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
            GRC::ContractAction::ADD,
            cpid,
            key.GetPubKey());

        // The registry keeps a beacon under its transaction's hash, so the transaction carries the contract.
        CMutableTransaction beacon_tx;
        beacon_tx.nTime = pindexBest->nTime - 1000;
        beacon_tx.vContracts.push_back(contract);

        GRC::GetBeaconRegistry().Add({contract, CTransaction(beacon_tx), nullptr});
        GRC::GetBeaconRegistry().ActivatePending({key.GetPubKey().GetID()}, beacon_tx.nTime, uint256(), 0);

        BOOST_REQUIRE(GRC::GetBeaconRegistry().TryActive(cpid, pindexBest->nTime));

        GRC::ResearchAccount& account = GRC::Tally::CreateAccount(cpid);
        account.m_first_block_ptr = &m_reward_index;
        account.m_last_block_ptr = &m_reward_index;
        account.m_accrual = ACCRUAL;

        m_cpids.push_back(cpid);

        return cpid;
    }

    //!
    //! \brief Put in the pool a transaction that carries an MRC for \p cpid on the tip, signed with \p key. It spends
    //! premine output \p coin and pays \p tx_fee, by which the miner orders the pool.
    //!
    //! \return The transaction.
    //!
    CTransaction SendMRC(
        const GRC::Cpid& cpid,
        CKey& key,
        const size_t coin,
        const CAmount tx_fee,
        const CAmount research_subsidy = ACCRUAL,
        const CAmount fee = FEE)
    {
        const CTransaction tx = MakeMRC(cpid, key, coin, tx_fee, research_subsidy, fee);

        LOCK(cs_main);

        AddToMempool(tx, tx_fee);
        BOOST_REQUIRE(mempool.exists(tx.GetHash()));

        return tx;
    }

    //!
    //! \brief As SendMRC(), but the transaction is only made. With \p valid false the MRC is signed with \p key
    //! although that is not the key of the CPID's beacon.
    //!
    CTransaction MakeMRC(
        const GRC::Cpid& cpid,
        CKey& key,
        const size_t coin,
        const CAmount tx_fee,
        const CAmount research_subsidy = ACCRUAL,
        const CAmount fee = FEE,
        const bool valid = true)
    {
        LOCK(cs_main);

        GRC::MRC mrc;
        mrc.m_mining_id = cpid;
        mrc.m_client_version = "6.0.0.0";
        mrc.m_last_block_hash = pindexBest->GetBlockHash();
        mrc.m_research_subsidy = research_subsidy;
        mrc.m_fee = fee;

        BOOST_REQUIRE(mrc.Sign(key));

        // A valid MRC is valid at the tip, so what becomes of it rests on the rule a case is about alone.
        BOOST_REQUIRE_EQUAL(ValidateMRC(pindexBest, mrc), valid);

        const std::vector<COutPoint> coins = SpendablePremineOutputs();
        BOOST_REQUIRE(coin < coins.size());

        return CreateSpendWithContract(
            PremineCoinbase(),
            coins[coin].n,
            tx_fee,
            GRC::MakeContract<GRC::MRC>(GRC::ContractAction::ADD, mrc),
            0,
            mrc.RequiredBurnAmount());
    }

private:
    uint256 m_reward_hash = InsecureRand256();
    CBlockIndex m_reward_index;
    std::vector<GRC::Cpid> m_cpids;
};

//! What the miner made of the pool: the block's transactions after the coinbase and the coinstake, and the MRCs bound.
struct Assembled
{
    std::vector<CTransaction> m_txs;
    MrcMap m_mrc_map;

    bool Holds(const CTransaction& tx) const
    {
        for (const CTransaction& candidate : m_txs) {
            if (candidate.GetHash() == tx.GetHash()) {
                return true;
            }
        }

        return false;
    }

    bool Binds(const GRC::Cpid& cpid, const CTransaction& tx) const
    {
        const auto iter = m_mrc_map.find(cpid);

        return iter != m_mrc_map.end() && iter->second.first == tx.GetHash();
    }
};

//! Run block assembly over the pool, for a block that carries a superblock or one that does not.
Assembled Assemble(const bool carries_superblock = false)
{
    CBlock block;
    CMutableTransaction coinbase;
    CMutableTransaction coinstake;
    MakeBlockAndCoinstake(block, coinbase, coinstake);

    Assembled assembled;

    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        BOOST_REQUIRE(CreateRestOfTheBlock(block, coinbase, coinstake, pindexBest, assembled.m_mrc_map,
                                           carries_superblock));
    }

    BOOST_REQUIRE(block.vtx.size() >= 2);
    assembled.m_txs.assign(block.vtx.begin() + 2, block.vtx.end());

    return assembled;
}
//! What the memory pool made of a transaction offered to it, as one from a peer is.
struct Offered
{
    bool m_accepted = false;
    int m_score = -1;       //!< The misbehaviour score of the refusal, or -1 if the transaction was not found invalid.
    std::string m_reason;

    explicit Offered(const CTransaction& tx)
    {
        LOCK(cs_main);

        CTransaction offered = tx;
        CValidationState state;
        m_accepted = AcceptToMemoryPool(mempool, offered, state, nullptr);

        int score = 0;

        if (state.IsInvalid(score)) {
            m_score = score;
        }

        m_reason = state.GetRejectReason();
    }
};
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(miner_mrc_selection_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

// The fixture's own precondition: the miner binds an MRC that breaks no rule, so a case that finds an MRC left out
// has found the rule it is about.
BOOST_AUTO_TEST_CASE(a_valid_mrc_in_the_pool_is_bound)
{
    Researchers researchers;

    CKey key;
    key.MakeNewKey(false);

    const GRC::Cpid cpid = researchers.Add(key);
    const CTransaction tx = researchers.SendMRC(cpid, key, 0, COIN);

    const Assembled assembled = Assemble();

    BOOST_CHECK_EQUAL(assembled.m_mrc_map.size(), 1U);
    BOOST_CHECK(assembled.Binds(cpid, tx));
    BOOST_CHECK(assembled.Holds(tx));
}

// An MRC whose fee leaves the foundation no share is left in the pool, and takes no place from another.
BOOST_AUTO_TEST_CASE(an_mrc_whose_fee_leaves_the_foundation_no_share_is_not_bound)
{
    Researchers researchers;

    CKey key;
    key.MakeNewKey(false);

    CKey other_key;
    other_key.MakeNewKey(false);

    const GRC::Cpid cpid = researchers.Add(key);
    const GRC::Cpid other_cpid = researchers.Add(other_key);

    // An MRC whose fee leaves the foundation no share. It pays the higher transaction fee, so the miner comes to it
    // first.
    const CTransaction no_share_tx = researchers.SendMRC(cpid, key, 0, 2 * COIN, 2, 0);
    const CTransaction tx = researchers.SendMRC(other_cpid, other_key, 1, COIN);

    const Assembled assembled = Assemble();

    BOOST_CHECK_EQUAL(assembled.m_mrc_map.size(), 1U);
    BOOST_CHECK(assembled.Binds(other_cpid, tx));
    BOOST_CHECK(assembled.Holds(tx));
    BOOST_CHECK(!assembled.Holds(no_share_tx));
}


// A block's MRCs are paid at different addresses, so of two MRCs paid at one address the miner binds the first it
// comes to and leaves the other in the pool.
BOOST_AUTO_TEST_CASE(a_second_mrc_paid_at_an_address_already_bound_is_not_bound)
{
    Researchers researchers;

    CKey key;
    key.MakeNewKey(false);

    const GRC::Cpid cpid = researchers.Add(key);
    const GRC::Cpid other_cpid = researchers.Add(key);

    // The first pays the higher transaction fee, so the miner comes to it first.
    const CTransaction tx = researchers.SendMRC(cpid, key, 0, 2 * COIN);
    const CTransaction other_tx = researchers.SendMRC(other_cpid, key, 1, COIN);

    const Assembled assembled = Assemble();

    BOOST_CHECK_EQUAL(assembled.m_mrc_map.size(), 1U);
    BOOST_CHECK(assembled.Binds(cpid, tx));
    BOOST_CHECK(assembled.Holds(tx));
    BOOST_CHECK(!assembled.Holds(other_tx));
}


// A block that carries a superblock pays no MRC, so the miner binds none to it and leaves them in the pool for the
// block after.
BOOST_AUTO_TEST_CASE(no_mrc_is_bound_to_a_block_that_carries_a_superblock)
{
    Researchers researchers;

    CKey key;
    key.MakeNewKey(false);

    const GRC::Cpid cpid = researchers.Add(key);
    const CTransaction tx = researchers.SendMRC(cpid, key, 0, COIN);

    const Assembled with_superblock = Assemble(/* carries_superblock */ true);

    BOOST_CHECK(with_superblock.m_mrc_map.empty());
    BOOST_CHECK(!with_superblock.Holds(tx));
    BOOST_CHECK(mempool.exists(tx.GetHash()));

    // The same pool, for a block that carries none.
    const Assembled without_superblock = Assemble();

    BOOST_CHECK(without_superblock.Binds(cpid, tx));
    BOOST_CHECK(without_superblock.Holds(tx));
}

BOOST_AUTO_TEST_SUITE_END()

// The pool holds one MRC for a CPID. A node that holds one refuses another. The refusal depends on what the node's
// own pool holds, and carries no misbehaviour score.
BOOST_AUTO_TEST_SUITE(mempool_mrc_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

BOOST_AUTO_TEST_CASE(a_second_mrc_for_a_cpid_in_the_pool_is_refused_without_a_score)
{
    Researchers researchers;

    CKey key;
    key.MakeNewKey(false);

    const GRC::Cpid cpid = researchers.Add(key);

    const CTransaction first = researchers.MakeMRC(cpid, key, 0, COIN);
    const CTransaction second = researchers.MakeMRC(cpid, key, 1, COIN, ACCRUAL, FEE + 1);

    const Offered first_offered(first);
    BOOST_REQUIRE_MESSAGE(first_offered.m_accepted, "the first MRC was refused: " << first_offered.m_reason);

    const Offered second_offered(second);

    BOOST_CHECK(!second_offered.m_accepted);
    BOOST_CHECK_EQUAL(second_offered.m_reason, "mrc-duplicate-cpid");
    BOOST_CHECK_EQUAL(second_offered.m_score, 0);

    // The MRC the pool holds, offered again, is not a second one: it is not taken again and not found invalid.
    const Offered first_again(first);

    BOOST_CHECK(!first_again.m_accepted);
    BOOST_CHECK_EQUAL(first_again.m_score, -1);
}

// The second MRC is refused for being the second before its contract is judged.
BOOST_AUTO_TEST_CASE(a_second_mrc_for_a_cpid_in_the_pool_is_refused_before_its_contract_is_judged)
{
    Researchers researchers;

    CKey key;
    key.MakeNewKey(false);

    CKey other_key;
    other_key.MakeNewKey(false);

    const GRC::Cpid cpid = researchers.Add(key);

    const CTransaction first = researchers.MakeMRC(cpid, key, 0, COIN);
    const CTransaction second = researchers.MakeMRC(cpid, other_key, 1, COIN, ACCRUAL, FEE, /* valid */ false);

    const Offered first_offered(first);
    BOOST_REQUIRE_MESSAGE(first_offered.m_accepted, "the first MRC was refused: " << first_offered.m_reason);

    const Offered second_offered(second);

    BOOST_CHECK(!second_offered.m_accepted);
    BOOST_CHECK_EQUAL(second_offered.m_reason, "mrc-duplicate-cpid");
    BOOST_CHECK_EQUAL(second_offered.m_score, 0);
}

// With no MRC for the CPID in the pool, an MRC that is not validly signed is refused for that, with its score.
BOOST_AUTO_TEST_CASE(an_mrc_with_a_bad_signature_is_refused_with_a_score)
{
    Researchers researchers;

    CKey key;
    key.MakeNewKey(false);

    CKey other_key;
    other_key.MakeNewKey(false);

    const GRC::Cpid cpid = researchers.Add(key);

    const Offered offered(researchers.MakeMRC(cpid, other_key, 0, COIN, ACCRUAL, FEE, /* valid */ false));

    BOOST_CHECK(!offered.m_accepted);
    BOOST_CHECK_EQUAL(offered.m_reason, "invalid-contract");
    BOOST_CHECK_EQUAL(offered.m_score, 25);
}

BOOST_AUTO_TEST_SUITE_END()
