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
#include "logging.h"
#include "node/chainman.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "scheduler.h"
#include "script/script.h"
#include "streams.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "uint256.h"
#include "validation.h"
#include "validationinterface.h"
#include "version.h"

#include <boost/test/unit_test.hpp>

#include <functional>
#include <limits>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include <univalue.h>

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

//! Records the two tip-change signals: each UpdatedBlockTip, with its
//! arguments, and each ChainStateFlushed, with its locator.
struct TipProbe final : public CValidationInterface {
    struct Tip {
        const CBlockIndex* pindex_new;
        const CBlockIndex* pindex_fork;
        bool initial_download;
    };

    std::vector<Tip> tips;
    std::vector<CBlockLocator> flushes;

    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) override
    {
        tips.push_back({pindexNew, pindexFork, fInitialDownload});
    }

    void ChainStateFlushed(const CBlockLocator& locator) override
    {
        flushes.push_back(locator);
    }
};

//! Registers a TipProbe with the validation signals for one inner block. The
//! signals are inert in this binary until a scheduler is registered, and the
//! two the probe records are invoked synchronously, so the scheduler never has
//! to run.
class ProbeScope
{
public:
    explicit ProbeScope(TipProbe& probe) : m_probe(probe)
    {
        GetMainSignals().RegisterBackgroundSignalScheduler(m_scheduler);
        RegisterValidationInterface(&m_probe);
    }

    ~ProbeScope()
    {
        UnregisterValidationInterface(&m_probe);
        GetMainSignals().UnregisterBackgroundSignalScheduler();
    }

    ProbeScope(const ProbeScope&) = delete;
    ProbeScope& operator=(const ProbeScope&) = delete;

private:
    CScheduler m_scheduler;
    TipProbe& m_probe;
};

//! Hands each log line to a callback for one inner block. The logger runs the
//! callback under its own lock, so a callback only records: it neither logs nor
//! takes a lock.
class LogWatch
{
public:
    explicit LogWatch(std::function<void(const std::string&)> on_line)
        : m_callback(LogInstance().PushBackCallback(std::move(on_line)))
    {
    }

    ~LogWatch() { LogInstance().DeleteCallback(m_callback); }

    LogWatch(const LogWatch&) = delete;
    LogWatch& operator=(const LogWatch&) = delete;

private:
    std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

//! Turns the VERBOSE log category on for one inner block, and on destruction
//! restores it as it found it, so it does not leak into later cases.
class VerboseScope
{
public:
    VerboseScope() : m_was_enabled(LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE))
    {
        LogInstance().EnableCategory(BCLog::LogFlags::VERBOSE);
    }

    ~VerboseScope()
    {
        if (!m_was_enabled) LogInstance().DisableCategory(BCLog::LogFlags::VERBOSE);
    }

    VerboseScope(const VerboseScope&) = delete;
    VerboseScope& operator=(const VerboseScope&) = delete;

private:
    const bool m_was_enabled;
};

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

// getblocksbatch walks the main chain from its starting block. A block that is
// in the index but no longer in the main chain has no next block, so it is
// refused as a starting block.
BOOST_AUTO_TEST_CASE(getblocksbatch_refuses_a_starting_block_off_the_main_chain)
{
    const CBlock block = MineOne();
    const uint256 block_hash = block.GetHash(true);

    UniValue params(UniValue::VARR);
    params.push_back(block_hash.GetHex());
    params.push_back(10);

    // On the main chain the block starts a batch.
    UniValue batch;
    BOOST_REQUIRE_NO_THROW(batch = getblocksbatch(params));
    BOOST_CHECK_EQUAL(find_value(batch, "block_count").get_int(), 1);

    // Park the chain at genesis, which leaves the block in the index and off
    // the main chain, and where a case that runs after this one expects it.
    BOOST_REQUIRE(WITH_LOCK(cs_main, return pindexGenesisBlock != nullptr));
    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);

    BOOST_CHECK_EXCEPTION(getblocksbatch(params), UniValue, [](const UniValue& error) {
        return find_value(error, "message").get_str() == "Starting block for batch is not in the main chain.";
    });
}

// listtransactions adds its count and its starting offset. With a wallet that
// has transactions, a count at the top of the int range and an offset must
// give the entries from the offset on, as any count past the end does.
BOOST_AUTO_TEST_CASE(listtransactions_takes_a_count_at_the_top_of_the_int_range)
{
    MineOne();
    MineOne();

    UniValue all(UniValue::VARR);
    all.push_back("*");
    all.push_back(1000);
    all.push_back(0);

    const UniValue listed = listtransactions(all);
    BOOST_REQUIRE(listed.size() >= 2);

    UniValue params(UniValue::VARR);
    params.push_back("*");
    params.push_back(std::numeric_limits<int>::max());
    params.push_back(1);

    UniValue tail;
    BOOST_REQUIRE_NO_THROW(tail = listtransactions(params));
    BOOST_CHECK_EQUAL(tail.size(), listed.size() - 1);

    // Leave the chain at genesis, where a case that runs after this one
    // expects it.
    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);
}

// The cases below mine on the chain they find, register the probe only after
// that mining, and then force a reorganize to a block they mined: back to the
// first of three blocks, or, in the case on the current tip, to the one block
// it mined. Each starts from the height it finds, which is not always genesis
// (the SetBestChain revert case above leaves the chain at height 2), and parks
// the chain at genesis when it ends.

// A forced reorganize reports the new tip, as a block that connects does: one
// UpdatedBlockTip, with the forced tip as both the new tip and the fork point,
// outside initial download.
BOOST_AUTO_TEST_CASE(force_reorganize_reports_the_new_tip)
{
    const int h0 = WITH_LOCK(cs_main, return nBestHeight);
    const uint256 target = MineOne().GetHash(true);
    MineOne();
    MineOne();
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 3);
    const CBlockIndex* const pindex_target = WITH_LOCK(cs_main, return mapBlockIndex.at(target));

    {
        TipProbe probe;
        const ProbeScope registration(probe);

        BOOST_REQUIRE(ForceReorganizeToHash(target));
        BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 1);

        BOOST_CHECK_EQUAL(probe.tips.size(), 1U);
        if (!probe.tips.empty()) {
            BOOST_CHECK(probe.tips[0].pindex_new == pindex_target);
            BOOST_CHECK(probe.tips[0].pindex_fork == pindex_target);
            BOOST_CHECK(!probe.tips[0].initial_download);
        }
    }

    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);
}

// A forced reorganize writes the wallet's best-block locator, as a block that
// connects outside initial download does: one ChainStateFlushed, whose locator
// leads with the forced tip.
BOOST_AUTO_TEST_CASE(force_reorganize_flushes_the_wallet_locator)
{
    const int h0 = WITH_LOCK(cs_main, return nBestHeight);
    const uint256 target = MineOne().GetHash(true);
    MineOne();
    MineOne();
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 3);

    {
        TipProbe probe;
        const ProbeScope registration(probe);

        BOOST_REQUIRE(ForceReorganizeToHash(target));
        BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 1);

        // The locator is written only outside initial download. A tip less
        // than 8 h old, at or above both the peers' median height and the last
        // checkpoint (genesis on regtest), is outside it, as this one is, so the
        // case requires that rather than accept no flush.
        BOOST_REQUIRE(!IsInitialBlockDownload());

        BOOST_CHECK_EQUAL(probe.flushes.size(), 1U);
        if (!probe.flushes.empty()) {
            BOOST_CHECK(!probe.flushes[0].vHave.empty() && probe.flushes[0].vHave[0] == target);
        }
    }

    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);
}

// A forced reorganize clears g_reorg_in_progress before it asks peers for the
// blocks it lacks, which it does just before its success line, so the flag is
// already clear when that line is logged.
BOOST_AUTO_TEST_CASE(force_reorganize_clears_the_reorg_flag_before_asking_for_blocks)
{
    const int h0 = WITH_LOCK(cs_main, return nBestHeight);
    const uint256 target = MineOne().GetHash(true);
    MineOne();
    MineOne();
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 3);

    {
        TipProbe probe;
        const ProbeScope registration(probe);

        bool seen = false;
        bool flag_at_success_line = true;
        const LogWatch watch([&](const std::string& line) {
            if (line.find("ForceReorganizeToHash: success!") != std::string::npos) {
                seen = true;
                flag_at_success_line = g_reorg_in_progress.load();
            }
        });

        BOOST_REQUIRE(ForceReorganizeToHash(target));
        BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 1);

        BOOST_REQUIRE(seen);
        BOOST_CHECK(!flag_at_success_line);
    }

    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);
}

// A forced reorganize to the block that is already the tip changes nothing, so
// it notifies nothing. ReorganizeChain still sets g_reorg_in_progress on that
// path, and the flag is clear by the success line there too.
BOOST_AUTO_TEST_CASE(force_reorganize_to_the_current_tip_notifies_nothing)
{
    const int h0 = WITH_LOCK(cs_main, return nBestHeight);
    const uint256 tip = MineOne().GetHash(true);
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 1);

    {
        TipProbe probe;
        const ProbeScope registration(probe);

        bool seen = false;
        bool flag_at_success_line = true;
        const LogWatch watch([&](const std::string& line) {
            if (line.find("ForceReorganizeToHash: success!") != std::string::npos) {
                seen = true;
                flag_at_success_line = g_reorg_in_progress.load();
            }
        });

        BOOST_REQUIRE(ForceReorganizeToHash(tip));
        BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 1);

        BOOST_CHECK_EQUAL(probe.tips.size(), 0U);
        BOOST_CHECK_EQUAL(probe.flushes.size(), 0U);
        BOOST_REQUIRE(seen);
        BOOST_CHECK(!flag_at_success_line);
    }

    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);
}

// The "new best block" line names the function that changed the tip: a forced
// reorganize logs it as ForceReorganizeToHash, and a block that connects
// afterwards still logs it as SetBestChain. Both forms of the line are checked:
// the plain one, and the longer one VERBOSE logs.
BOOST_AUTO_TEST_CASE(the_tip_change_lines_name_their_caller)
{
    const int h0 = WITH_LOCK(cs_main, return nBestHeight);
    const uint256 target = MineOne().GetHash(true);
    MineOne();
    MineOne();
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 3);

    {
        TipProbe probe;
        const ProbeScope registration(probe);

        bool forced_line_seen = false;
        bool mined_line_seen = false;
        const LogWatch watch([&](const std::string& line) {
            if (line.find("ForceReorganizeToHash: new best block") != std::string::npos) forced_line_seen = true;
            if (line.find("SetBestChain: new best block") != std::string::npos) mined_line_seen = true;
        });

        BOOST_REQUIRE(ForceReorganizeToHash(target));
        BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 1);
        BOOST_CHECK(forced_line_seen);

        MineOne();
        BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 2);
        BOOST_CHECK(mined_line_seen);

        // The same with VERBOSE on, which logs the longer form of the line, with
        // the chain trust and the block date: a forced reorganize back to the
        // target, then a block mined on it. Neither line names FinishTipChange,
        // the function that logs both.
        {
            const VerboseScope verbose;
            bool forced_verbose_line_seen = false;
            bool mined_verbose_line_seen = false;
            bool helper_line_seen = false;
            const LogWatch verbose_watch([&](const std::string& line) {
                if (line.find("trust=") == std::string::npos) return;
                if (line.find("ForceReorganizeToHash: new best block") != std::string::npos) forced_verbose_line_seen = true;
                if (line.find("SetBestChain: new best block") != std::string::npos) mined_verbose_line_seen = true;
                if (line.find("FinishTipChange: new best block") != std::string::npos) helper_line_seen = true;
            });

            BOOST_REQUIRE(ForceReorganizeToHash(target));
            BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 1);
            MineOne();
            BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), h0 + 2);

            BOOST_CHECK(forced_verbose_line_seen);
            BOOST_CHECK(mined_verbose_line_seen);
            BOOST_CHECK(!helper_line_seen);
        }
    }

    BOOST_REQUIRE(ForceReorganizeToHash(WITH_LOCK(cs_main, return pindexGenesisBlock->GetBlockHash())));
    BOOST_REQUIRE_EQUAL(WITH_LOCK(cs_main, return nBestHeight), 0);
}

BOOST_AUTO_TEST_SUITE_END()
