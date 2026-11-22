// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! Which MRCs in the memory pool the miner binds to the block it assembles
//! (CreateRestOfTheBlock), on the fixture's regtest chain.
//!
//! gridcoin/mrc_tests.cpp checks the rules an MRC is selected by on a mock
//! chain with an empty pool. The cases here put signed, funded MRC transactions
//! in the pool and read what the miner did with them.

#include "amount.h"
#include "chain.h"
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
        LOCK(cs_main);

        GRC::MRC mrc;
        mrc.m_mining_id = cpid;
        mrc.m_client_version = "6.0.0.0";
        mrc.m_last_block_hash = pindexBest->GetBlockHash();
        mrc.m_research_subsidy = research_subsidy;
        mrc.m_fee = fee;

        BOOST_REQUIRE(mrc.Sign(key));

        // The MRC is valid at the tip, so whether the miner binds it rests on the rule a case is about alone.
        BOOST_REQUIRE(ValidateMRC(pindexBest, mrc));

        const std::vector<COutPoint> coins = SpendablePremineOutputs();
        BOOST_REQUIRE(coin < coins.size());

        const CTransaction tx = CreateSpendWithContract(
            PremineCoinbase(),
            coins[coin].n,
            tx_fee,
            GRC::MakeContract<GRC::MRC>(GRC::ContractAction::ADD, mrc),
            0,
            mrc.RequiredBurnAmount());

        AddToMempool(tx, tx_fee);
        BOOST_REQUIRE(mempool.exists(tx.GetHash()));

        return tx;
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

//! Run block assembly over the pool.
Assembled Assemble()
{
    CBlock block;
    CMutableTransaction coinbase;
    CMutableTransaction coinstake;
    MakeBlockAndCoinstake(block, coinbase, coinstake);

    Assembled assembled;

    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        BOOST_REQUIRE(CreateRestOfTheBlock(block, coinbase, coinstake, pindexBest, assembled.m_mrc_map));
    }

    BOOST_REQUIRE(block.vtx.size() >= 2);
    assembled.m_txs.assign(block.vtx.begin() + 2, block.vtx.end());

    return assembled;
}
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

BOOST_AUTO_TEST_SUITE_END()
