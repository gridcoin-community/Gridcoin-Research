// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file kernel_branch_lookup_tests.cpp
//! \brief Which block GRC::ReadStakedInput takes a staked input from when the
//! block being checked is not on the main chain's tip, on the regtest chain
//! fixture.
//!
//! A transaction broadcast before a fork is usually mined on both sides, in a
//! different block on each. The proof-of-stake check of a block on one side
//! must take the staked input's block (its time feeds the kernel and the
//! minimum stake age) from that block's own history. ReadStakedInput looks the
//! input up in the transaction index first, which follows the main chain, so
//! for a side-branch block it found the main chain's copy. From block version
//! 15 it takes the copy in the checked block's own history, and refuses an
//! input that is not in that history at all.
//!

#include "chain.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/kernel.h"
#include "init.h"
#include "node/blockstorage.h"
#include "node/chainman.h"
#include "primitives/block.h"
#include "script/sign.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "txdb.h"
#include "txmempool.h"
#include "uint256.h"
#include "util/time.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <set>
#include <string>

namespace {

//! Mine one block through the fixture, failing the case with the miner's
//! reason if it cannot.
CBlock MineOne()
{
    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(grc_test::CreateAndProcessBlock(block, err), err);
    return block;
}

//! The miner stamps blocks from GetAdjustedTime() and retries across five
//! consecutive 16-second stake slots. Moving the mock clock past all of them
//! makes the next block on the same parent a different block.
void SkipPastRetrySlots()
{
    SetMockTime(GetAdjustedTime() + 6 * (GRC::STAKE_TIMESTAMP_MASK + 1));
}

//! Puts the clock back the way the other suites expect it, however the case
//! exits.
struct MockTimeReset
{
    ~MockTimeReset() { SetMockTime(0); }
};

//! A premine output that the transaction index records as unspent and the
//! wallet does not mark spent.
COutPoint UnspentPremineOutput()
{
    const std::vector<COutPoint> coins = grc_test::SpendablePremineOutputs();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    for (const COutPoint& coin : coins) {
        const auto it = pwalletMain->mapWallet.find(coin.hash);
        if (it != pwalletMain->mapWallet.end() && it->second.IsSpent(coin.n)) continue;

        return coin;
    }

    BOOST_FAIL("no unspent premine output left");
    return COutPoint();
}

//! Record \p tx in the wallet and mark the output it spends, so the miner
//! cannot draw that output as a kernel while \p tx waits in the mempool: a
//! coinstake spending it would evict \p tx before it is mined.
void HoldInputInWallet(const CTransaction& tx)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletDB walletdb(pwalletMain->strWalletFile);
    CWalletTx wtx(pwalletMain, tx);
    wtx.fFromMe = true;
    BOOST_REQUIRE(pwalletMain->AddToWallet(wtx, &walletdb));
    pwalletMain->WalletUpdateSpent(tx, false, &walletdb);
}

//! A fixture spend of an unspent premine output, held in the wallet and put in
//! the mempool for the next mined block.
CTransaction PoolSpend(const int n_outputs = 1)
{
    const COutPoint coin = UnspentPremineOutput();
    const CTransaction tx = grc_test::CreateSpend(grc_test::PremineCoinbase(), coin.n, 200000, n_outputs);
    HoldInputInWallet(tx);
    grc_test::AddToMempool(tx, 200000);

    return tx;
}

bool BlockHasTx(const CBlock& block, const uint256& hash)
{
    return std::any_of(block.vtx.begin(), block.vtx.end(),
                       [&](const CTransaction& tx) { return tx.GetHash() == hash; });
}

//! Disconnect back to the parent of \p block, which offers its transactions
//! back to the mempool, and move the clock so the next block differs.
void LeaveForParentOf(const CBlock& block)
{
    uint256 parent_hash;
    {
        LOCK(cs_main);
        parent_hash = mapBlockIndex.at(block.GetHash(true))->pprev->GetBlockHash();
    }

    BOOST_REQUIRE(ForceReorganizeToHash(parent_hash));
    SkipPastRetrySlots();
}

//! The block ReadStakedInput takes \p hash from, for a block whose parent is
//! \p pindex_prev, or a null hash when it refuses the input.
uint256 InputBlockFor(const uint256& hash, CBlockIndex* pindex_prev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CTxDB txdb("r");
    CBlockHeader header;
    CTransaction tx_prev;

    if (!GRC::ReadStakedInput(txdb, hash, header, tx_prev, pindex_prev)) {
        return uint256();
    }

    BOOST_CHECK(tx_prev.GetHash() == hash);

    return header.GetHash(true);
}

//! A proof-of-stake block on \p pindex_prev, stamped \p time, whose coinstake
//! spends output \p n of \p tx_from with the premine key.
CBlock StakeBlockOn(const CBlockIndex* pindex_prev, const CTransaction& tx_from, const uint32_t n,
                    const uint32_t time, const int version) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CMutableTransaction coinbase;
    coinbase.nTime = time;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << (pindex_prev->nHeight + 1) << OP_0;
    coinbase.vout.resize(1);
    coinbase.vout[0].SetEmpty();

    CMutableTransaction coinstake;
    coinstake.nTime = time;
    coinstake.vin.resize(1);
    coinstake.vin[0].prevout = COutPoint(tx_from.GetHash(), n);
    coinstake.vout.resize(2);
    coinstake.vout[0].SetEmpty();
    coinstake.vout[1].nValue = tx_from.vout[n].nValue;
    coinstake.vout[1].scriptPubKey = grc_test::PremineScript();
    BOOST_REQUIRE(SignSignature(grc_test::PremineKeystore(), tx_from, coinstake, 0));

    CBlock block;
    block.nVersion = version;
    block.hashPrevBlock = pindex_prev->GetBlockHash();
    block.nTime = time;
    block.nBits = GRC::GetNextTargetRequired(pindex_prev);
    block.vtx.push_back(CTransaction(coinbase));
    block.vtx.push_back(CTransaction(coinstake));
    BOOST_REQUIRE(block.IsProofOfStake());

    return block;
}

//! Takes a transaction's entry out of the transaction index for the scope and
//! puts it back exactly, however the case exits.
struct IndexEntryRemoved
{
    uint256 m_hash;
    CTxIndex m_saved;

    explicit IndexEntryRemoved(const CTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(cs_main) : m_hash(tx.GetHash())
    {
        CTxDB txdb;
        BOOST_REQUIRE(txdb.ReadTxIndex(m_hash, m_saved));
        BOOST_REQUIRE(txdb.EraseTxIndex(tx));
    }

    ~IndexEntryRemoved()
    {
        CTxDB txdb;
        txdb.UpdateTxIndex(m_hash, m_saved);
    }

    IndexEntryRemoved(const IndexEntryRemoved&) = delete;
    IndexEntryRemoved& operator=(const IndexEntryRemoved&) = delete;
};

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(kernel_branch_lookup_tests, grc_test::WalletTxScope, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

BOOST_AUTO_TEST_CASE(a_side_branch_block_takes_its_staked_input_from_its_own_branch_from_v15)
{
    const MockTimeReset reset_clock;
    grc_test::RequireV15Unscheduled();

    mempool.clear();

    // T is mined twice: in A1, which then leaves the main chain, and again in
    // B1, which replaces it. A1 and B1 share a parent.
    const CTransaction tx_t = PoolSpend();
    const uint256 t_hash = tx_t.GetHash();

    const CBlock a1 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(a1, t_hash), "A1 does not carry T");
    const uint256 a1_hash = a1.GetHash(true);

    LeaveForParentOf(a1);
    BOOST_REQUIRE_MESSAGE(mempool.exists(t_hash), "T did not return to the mempool when A1 was disconnected");

    const CBlock b1 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(b1, t_hash), "B1 does not carry T");
    const uint256 b1_hash = b1.GetHash(true);
    BOOST_REQUIRE(b1_hash != a1_hash);

    // U is mined only on the main chain, in B2, above the fork.
    const CTransaction tx_u = PoolSpend();
    const uint256 u_hash = tx_u.GetHash();

    const CBlock b2 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(b2, u_hash), "B2 does not carry U");
    const uint256 b2_hash = b2.GetHash(true);

    LOCK(cs_main);

    CBlockIndex* const pindex_a1 = mapBlockIndex.at(a1_hash);
    CBlockIndex* const pindex_b1 = mapBlockIndex.at(b1_hash);
    CBlockIndex* const pindex_b2 = mapBlockIndex.at(b2_hash);
    BOOST_REQUIRE(!pindex_a1->IsInMainChain());
    BOOST_REQUIRE(pindex_b1->IsInMainChain());
    BOOST_REQUIRE(pindex_b2 == pindexBest);
    BOOST_REQUIRE(pindex_a1->nTime != pindex_b1->nTime);

    const uint256 premine_hash = grc_test::PremineCoinbase().GetHash();
    const uint256 genesis_hash = pindexGenesisBlock->GetBlockHash();

    // The block a child of A1 would be: the gate is judged at its height.
    const int child_of_a1 = pindex_a1->nHeight + 1;

    // Below v15 every lookup goes through the transaction index, so a child of
    // A1 gets T from B1 and U from B2, blocks it does not descend from, and a
    // child of B1 gets U from B2, above it.
    BOOST_CHECK(InputBlockFor(t_hash, pindex_a1) == b1_hash);
    BOOST_CHECK(InputBlockFor(u_hash, pindex_a1) == b2_hash);
    BOOST_CHECK(InputBlockFor(u_hash, pindex_b1) == b2_hash);

    {
        // The last height below the gate behaves the same.
        const grc_test::V15HeightGuard v15(child_of_a1 + 1);

        BOOST_CHECK(InputBlockFor(t_hash, pindex_a1) == b1_hash);
        BOOST_CHECK(InputBlockFor(u_hash, pindex_a1) == b2_hash);
    }

    {
        const grc_test::V15HeightGuard v15(child_of_a1);

        // From v15 a child of A1 takes T from A1, its own branch's copy, and
        // refuses U, which its history does not have.
        BOOST_CHECK(InputBlockFor(t_hash, pindex_a1) == a1_hash);
        BOOST_CHECK(InputBlockFor(u_hash, pindex_a1) == uint256());

        // An input confirmed below the fork is shared history and still comes
        // from the transaction index.
        BOOST_CHECK(InputBlockFor(premine_hash, pindex_a1) == genesis_hash);

        // A child of the tip gets the transaction index's answer.
        BOOST_CHECK(InputBlockFor(t_hash, pindex_b2) == b1_hash);
        BOOST_CHECK(InputBlockFor(u_hash, pindex_b2) == b2_hash);

        // A child of B1, a main-chain block below the tip, takes T from B1 in
        // its history and refuses U, which is above it.
        BOOST_CHECK(InputBlockFor(t_hash, pindex_b1) == b1_hash);
        BOOST_CHECK(InputBlockFor(u_hash, pindex_b1) == uint256());
    }
}

BOOST_AUTO_TEST_CASE(a_deeper_side_branch_block_finds_its_input_in_the_branch)
{
    const MockTimeReset reset_clock;
    grc_test::RequireV15Unscheduled();

    mempool.clear();

    // A1 carries T and A2 extends A1; both leave the main chain for B1, which
    // carries T again.
    const CTransaction tx_t = PoolSpend();
    const uint256 t_hash = tx_t.GetHash();

    const CBlock a1 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(a1, t_hash), "A1 does not carry T");
    SkipPastRetrySlots();
    const CBlock a2 = MineOne();
    BOOST_REQUIRE(!BlockHasTx(a2, t_hash));

    LeaveForParentOf(a1);
    BOOST_REQUIRE_MESSAGE(mempool.exists(t_hash), "T did not return to the mempool when A1 was disconnected");

    const CBlock b1 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(b1, t_hash), "B1 does not carry T");

    LOCK(cs_main);

    CBlockIndex* const pindex_a2 = mapBlockIndex.at(a2.GetHash(true));
    BOOST_REQUIRE(pindex_a2->pprev == mapBlockIndex.at(a1.GetHash(true)));
    BOOST_REQUIRE(!pindex_a2->IsInMainChain());

    BOOST_CHECK(InputBlockFor(t_hash, pindex_a2) == b1.GetHash(true));

    const grc_test::V15HeightGuard v15(pindex_a2->nHeight + 1);

    // A child of A2 searches A2, then A1, where T is.
    BOOST_CHECK(InputBlockFor(t_hash, pindex_a2) == a1.GetHash(true));
    BOOST_CHECK(InputBlockFor(grc_test::PremineCoinbase().GetHash(), pindex_a2)
                == pindexGenesisBlock->GetBlockHash());
}

BOOST_AUTO_TEST_CASE(a_side_branch_stake_passes_the_proof_of_stake_check_from_v15)
{
    const MockTimeReset reset_clock;
    grc_test::RequireV15Unscheduled();

    mempool.clear();

    // T, with four outputs to stake, is mined in A1 and again in the later B1.
    const CTransaction tx_t = PoolSpend(4);
    const uint256 t_hash = tx_t.GetHash();

    const CBlock a1 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(a1, t_hash), "A1 does not carry T");

    LeaveForParentOf(a1);

    const CBlock b1 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(b1, t_hash), "B1 does not carry T");

    LOCK(cs_main);

    CBlockIndex* const pindex_a1 = mapBlockIndex.at(a1.GetHash(true));
    const CBlockIndex* const pindex_b1 = mapBlockIndex.at(b1.GetHash(true));
    BOOST_REQUIRE(pindex_a1->nTime < pindex_b1->nTime);

    // A child of A1 stakes an output of T at a time after A1 but before B1.
    // Regtest's minimum stake age is zero, so the age check is "the confirming
    // block is not later than the stake": true for A1, false for B1. Below v15
    // the check reads B1 and must fail; from v15 it reads A1 and can pass.
    int pass_from_v15 = 0;
    int pass_below_v15 = 0;

    for (uint32_t time = pindex_a1->nTime; time < pindex_b1->nTime; time += GRC::STAKE_TIMESTAMP_MASK + 1) {
        for (uint32_t n = 0; n < tx_t.vout.size(); ++n) {
            uint256 proof;

            {
                CBlock block = StakeBlockOn(pindex_a1, tx_t, n, time, 14);
                CTxDB txdb("r");
                CValidationState state;
                pass_below_v15 += GRC::CheckProofOfStakeV8(txdb, pindex_a1, block, false, state, proof);
            }

            {
                const grc_test::V15HeightGuard v15(pindex_a1->nHeight + 1);
                CBlock block = StakeBlockOn(pindex_a1, tx_t, n, time, 15);
                CTxDB txdb("r");
                CValidationState state;
                pass_from_v15 += GRC::CheckProofOfStakeV8(txdb, pindex_a1, block, false, state, proof);
            }
        }
    }

    BOOST_TEST_MESSAGE(pass_from_v15 << " stakes pass from v15, " << pass_below_v15 << " below it");
    BOOST_CHECK_GT(pass_from_v15, 0);
    BOOST_CHECK_EQUAL(pass_below_v15, 0);
}

BOOST_AUTO_TEST_CASE(an_input_missing_from_the_index_is_searched_for_only_in_the_blocks_own_branch)
{
    const MockTimeReset reset_clock;
    grc_test::RequireV15Unscheduled();

    mempool.clear();

    // X is mined only in A1, which then leaves the main chain, so the index does
    // not hold it. Clearing the mempool keeps X out of B1.
    const CTransaction tx_x = PoolSpend();
    const uint256 x_hash = tx_x.GetHash();

    const CBlock a1 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(a1, x_hash), "A1 does not carry X");

    LeaveForParentOf(a1);
    mempool.clear();

    const CBlock b1 = MineOne();
    BOOST_REQUIRE(!BlockHasTx(b1, x_hash));

    // U is mined on the main chain, in B2.
    const CTransaction tx_u = PoolSpend();
    const uint256 u_hash = tx_u.GetHash();

    const CBlock b2 = MineOne();
    BOOST_REQUIRE_MESSAGE(BlockHasTx(b2, u_hash), "B2 does not carry U");

    LOCK(cs_main);

    CBlockIndex* const pindex_a1 = mapBlockIndex.at(a1.GetHash(true));
    CBlockIndex* const pindex_b2 = mapBlockIndex.at(b2.GetHash(true));
    BOOST_REQUIRE(!pindex_a1->IsInMainChain());
    BOOST_REQUIRE(pindex_b2 == pindexBest);
    BOOST_REQUIRE(!CTxDB("r").ContainsTx(x_hash));

    // A child of A1 finds X in its own branch; a child of the tip cannot have it.
    BOOST_CHECK(InputBlockFor(x_hash, pindex_a1) == a1.GetHash(true));
    BOOST_CHECK(InputBlockFor(x_hash, pindex_b2) == uint256());

    // The index is authoritative for the main chain: with U's entry gone, a
    // child of the tip refuses U.
    const IndexEntryRemoved removed(tx_u);

    BOOST_CHECK(InputBlockFor(u_hash, pindex_b2) == uint256());
}

BOOST_AUTO_TEST_SUITE_END()
