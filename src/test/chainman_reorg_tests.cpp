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

#include "amount.h"
#include "chain.h"
#include "consensus/merkle.h"
#include "dbwrapper.h"
#include "gridcoin/claim.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/staking/chain_trust.h"
#include "node/chainman.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "streams.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "uint256.h"
#include "validation.h"
#include "version.h"

#include <boost/test/unit_test.hpp>

#include <functional>
#include <string>
#include <vector>

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

//! A second block on the same parent with the same stake as \p block: the same
//! coinstake, so the same stake proof, and a claim that differs, so another
//! hash. Signed with the premine key, which the fixture stakes with.
CBlock SiblingWithSameStake(const CBlock& block)
{
    CBlock changed = block;

    GRC::Claim claim = changed.PullClaim();
    claim.m_organization = "sibling";

    CMutableTransaction coinbase(changed.vtx[0]);
    coinbase.vContracts[0] = GRC::MakeContract<GRC::Claim>(
        coinbase.vContracts[0].m_version,
        GRC::ContractAction::ADD,
        std::move(claim));
    changed.vtx[0] = CTransaction(std::move(coinbase));
    changed.hashMerkleRoot = BlockMerkleRoot(changed);

    CKey key = grc_test::PremineKey();
    BOOST_REQUIRE(key.Sign(changed.GetHash(), changed.vchBlockSig));

    // Through serialization, so that the sibling carries none of the memory-only
    // caches of the block it was copied from.
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << changed;

    CBlock sibling;
    stream >> sibling;

    return sibling;
}

//! A transaction whose one input carries a relative lock of \p blocks blocks
//! on a premine output. It is not signed; the case runs no scripts.
CTransaction SpendWithRelativeLock(const uint32_t blocks, const uint32_t time)
{
    const std::vector<COutPoint> outputs = grc_test::SpendablePremineOutputs();
    BOOST_REQUIRE(!outputs.empty());

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = time;
    mtx.vin.emplace_back(outputs.front(), CScript(), blocks);
    mtx.vout.emplace_back(1 * COIN, grc_test::PremineScript());

    return CTransaction(mtx);
}

//! A copy of \p block after \p change, with its merkle root and signature made
//! again. It shares the block's parent and, unless \p change touches the
//! coinstake, its stake.
CBlock ChangedCopy(const CBlock& block, const std::function<void(CBlock&)>& change)
{
    CBlock changed = block;
    change(changed);
    changed.hashMerkleRoot = BlockMerkleRoot(changed);

    CKey key = grc_test::PremineKey();
    BOOST_REQUIRE(key.Sign(changed.GetHash(), changed.vchBlockSig));

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << changed;

    CBlock copy;
    stream >> copy;

    return copy;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(chainman_reorg_tests,
                      *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

BOOST_AUTO_TEST_CASE(setbestchain_reverts_a_reorg_that_lowers_chain_trust)
{
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
    // The fixture mines B1 in a later stake slot than A1, so it is a different
    // block on the same parent.
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

// A node's block data files can hold two blocks with the same stake proof on
// one parent: the one that arrived first, and the one the chain went on with.
// From a peer the second is refused while no block that builds on it is
// waiting. An import has no peer and keeps no waiting blocks, so it must take
// the second, or it leaves every block after it in the file without a parent.
BOOST_AUTO_TEST_CASE(an_imported_block_is_taken_with_a_stake_proof_another_block_used)
{
    const CBlock first = MineOne();
    CBlock second = SiblingWithSameStake(first);

    const uint256 first_hash = first.GetHash(true);
    const uint256 second_hash = second.GetHash(true);
    BOOST_REQUIRE(second_hash != first_hash);
    BOOST_REQUIRE(second.vtx[1].GetHash() == first.vtx[1].GetHash());

    LOCK(cs_main);

    BOOST_REQUIRE(hashBestChain == first_hash);

    // As from a peer: refused, and not indexed.
    CValidationState peer_state;
    BOOST_CHECK(!AcceptBlock(second, peer_state, /*generated_by_me=*/false, /*imported=*/false));
    BOOST_CHECK(!mapBlockIndex.count(second_hash));

    // As from a block data file: indexed beside the first. Its trust is the
    // same, so the first stays the tip, and a block that builds on either one
    // decides between them.
    CValidationState import_state;
    BOOST_CHECK(ProcessBlock(nullptr, &second, /*generated_by_me=*/false, import_state));
    BOOST_REQUIRE(mapBlockIndex.count(second_hash));
    BOOST_CHECK(mapBlockIndex.at(second_hash)->pprev == mapBlockIndex.at(first_hash)->pprev);
    BOOST_CHECK(hashBestChain == first_hash);

    // The suite's cases share one chain and may run in any order. Park the
    // chain at genesis again, where a case that runs after this one expects it.
    BOOST_REQUIRE(pindexGenesisBlock);
    BOOST_CHECK(ForceReorganizeToHash(pindexGenesisBlock->GetBlockHash()));
    BOOST_CHECK_EQUAL(nBestHeight, 0);
}

// AcceptBlock() checks a block's stake before the sequence locks of its
// transactions. A block that breaks both rules is refused for its stake, and a
// block with a sound stake is still refused for an unsatisfied sequence lock.
BOOST_AUTO_TEST_CASE(acceptblock_checks_the_stake_before_the_sequence_locks)
{
    const CBlock mined = MineOne();

    // A relative lock far longer than the chain is unsatisfied in any block here.
    const CTransaction locked = SpendWithRelativeLock(1000, mined.nTime);

    const CBlock with_locked_spend = ChangedCopy(mined, [&](CBlock& block) {
        block.vtx.push_back(locked);
    });

    const CBlock with_locked_spend_and_unsigned_stake = ChangedCopy(mined, [&](CBlock& block) {
        block.vtx.push_back(locked);

        CMutableTransaction coinstake(block.vtx[1]);
        coinstake.vin[0].scriptSig = CScript();
        block.vtx[1] = CTransaction(std::move(coinstake));
    });

    {
        LOCK(cs_main);

        // The stake is checked first.
        CBlock block = with_locked_spend_and_unsigned_stake;
        CValidationState state;
        int score = 0;
        BOOST_CHECK(!AcceptBlock(block, state, /*generated_by_me=*/false, /*imported=*/false));
        BOOST_CHECK(state.IsInvalid(score));
        BOOST_CHECK_EQUAL(score, 100);
        BOOST_CHECK(!mapBlockIndex.count(block.GetHash(true)));
    }

    {
        LOCK(cs_main);

        // The stake is the mined block's own, which is why this copy is given
        // as an imported block: the stake proof is one the node has seen.
        CBlock block = with_locked_spend;
        CValidationState state;
        int score = 0;
        BOOST_CHECK(!AcceptBlock(block, state, /*generated_by_me=*/false, /*imported=*/true));
        BOOST_CHECK(state.IsInvalid(score));
        BOOST_CHECK_EQUAL(score, 10);
        BOOST_CHECK(!mapBlockIndex.count(block.GetHash(true)));
    }

    // Leave the chain at genesis, where a case that runs after this one
    // expects it.
    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);
}

BOOST_AUTO_TEST_SUITE_END()
