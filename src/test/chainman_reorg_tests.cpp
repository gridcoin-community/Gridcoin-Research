// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file chainman_reorg_tests.cpp
//! \brief The trust-regression revert leg of SetBestChain, on the regtest
//! chain fixture.
//!
//! SetBestChain reorganizes to the block it is handed and, if the chain ends
//! with less trust than it started with, reorganizes back to the original tip.
//! Production only reaches SetBestChain through the g_chain_trust.Favors gate
//! in AddToBlockIndex, so the revert leg fires only when the forward reorg
//! fails or connects less than it disconnected; no RPC drives it, and the
//! reorganize RPC goes through ForceReorganizeToHash, which never reverts.
//! The case below drives SetBestChain directly with a stale fork block, which
//! is the one way to reach the leg deterministically.
//!

#include "chain.h"
#include "dbwrapper.h"
#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/kernel.h"
#include "node/chainman.h"
#include "primitives/block.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "uint256.h"
#include "util/time.h"

#include <boost/test/unit_test.hpp>

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
//! guarantees the next block on the same parent is a different block, whichever
//! slot the previous one landed in.
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

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(chainman_reorg_tests,
                      *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

BOOST_AUTO_TEST_CASE(setbestchain_reverts_a_reorg_that_lowers_chain_trust)
{
    const MockTimeReset reset_clock;

    // A one-block branch A1 on genesis, then park the chain back at genesis.
    // ForceReorganizeToHash does not revert on lower trust, which is what lets a
    // competing branch be mined next.
    CBlock a1 = MineOne();
    const uint256 a1_hash = a1.GetHash(true);

    uint256 genesis_hash;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexGenesisBlock);
        BOOST_REQUIRE_EQUAL(nBestHeight, 1);
        genesis_hash = pindexGenesisBlock->GetBlockHash();
    }

    BOOST_REQUIRE(ForceReorganizeToHash(genesis_hash));
    {
        LOCK(cs_main);
        BOOST_REQUIRE_EQUAL(nBestHeight, 0);
        BOOST_REQUIRE(!g_reorg_in_progress);
    }

    // A two-block branch B1 -> B2 on genesis. It carries more trust than A1, so
    // A1 is now a stale fork the Favors gate would never hand to SetBestChain.
    SkipPastRetrySlots();
    CBlock b1 = MineOne();
    BOOST_REQUIRE(b1.GetHash(true) != a1_hash);
    CBlock b2 = MineOne();

    LOCK(cs_main);

    BOOST_REQUIRE_EQUAL(nBestHeight, 2);
    CBlockIndex* const pindex_a1 = mapBlockIndex.at(a1_hash);
    CBlockIndex* const pindex_b1 = mapBlockIndex.at(b1.GetHash(true));
    CBlockIndex* const pindex_b2 = pindexBest;
    BOOST_REQUIRE(pindex_b2->GetBlockHash() == b2.GetHash(true));
    BOOST_REQUIRE(!g_chain_trust.Favors(pindex_a1));

    const arith_uint256 trust_before = g_chain_trust.Best();

    // The forward reorg disconnects B2 and B1 and connects A1; the chain then
    // has less trust than before, and SetBestChain must put B2 back.
    CTxDB txdb;
    BOOST_CHECK(SetBestChain(txdb, a1, pindex_a1));

    BOOST_CHECK(pindexBest == pindex_b2);
    BOOST_CHECK(hashBestChain == pindex_b2->GetBlockHash());
    BOOST_CHECK_EQUAL(nBestHeight, 2);
    BOOST_CHECK(g_chain_trust.Best() == trust_before);
    BOOST_CHECK(!g_reorg_in_progress);

    // The in-memory chain links describe the restored branch, and A1 is off it.
    BOOST_CHECK(pindexGenesisBlock->pnext == pindex_b1);
    BOOST_CHECK(pindex_b1->pnext == pindex_b2);
    BOOST_CHECK(pindex_b2->pnext == nullptr);
    BOOST_CHECK(pindex_a1->pnext == nullptr);
    BOOST_CHECK(!pindex_a1->IsInMainChain());

    // The committed tx index agrees: B's coinstakes are indexed, A1's is not,
    // and the persisted best chain names B2.
    BOOST_CHECK(txdb.ContainsTx(b1.vtx[1].GetHash()));
    BOOST_CHECK(txdb.ContainsTx(b2.vtx[1].GetHash()));
    BOOST_CHECK(!txdb.ContainsTx(a1.vtx[1].GetHash()));

    uint256 best_on_disk;
    BOOST_CHECK(txdb.ReadHashBestChain(best_on_disk));
    BOOST_CHECK(best_on_disk == hashBestChain);
}

BOOST_AUTO_TEST_SUITE_END()
