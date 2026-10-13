// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "dbwrapper.h"
#include "gridcoin/claim.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/protocol.h"
#include "gridcoin/superblock.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "gridcoin/voting/vote.h"
#include "index/disktxpos.h"
#include "index/txindex.h"
#include "node/blockstorage.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "rpc/blockchain.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "util.h"

#include <boost/test/unit_test.hpp>

// Tests are single-threaded and drive the PollRegistry contract handler
// directly. The handler is EXCLUSIVE_LOCKS_REQUIRED(cs_main); the tests take
// cs_main where the code asserts it, and suppress the analyzer for the rest.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

namespace {
//! First height with block version 13, where a poll's magnitude weight factor
//! comes from the protocol registry (unit tests run on mainnet parameters).
int V13Height()
{
    return Params().GetConsensus().BlockV13Height;
}

//! A poll contract and the transaction that carries it.
struct PollTx
{
    GRC::Contract contract;
    CTransaction tx;
};

//! Build a poll as it reaches the registry from the chain. Poll::m_timestamp is
//! not serialized, so a poll read from a block carries a zero timestamp; the
//! transaction's own time gives each test poll its own txid.
PollTx MakePollTx(const std::string& title, const int64_t tx_time,
                  const GRC::ContractAction action = GRC::ContractAction::ADD,
                  const GRC::PollWeightType weight_type = GRC::PollWeightType::BALANCE,
                  const uint32_t payload_version = 2)
{
    GRC::Poll poll(
        GRC::PollType::SURVEY,
        weight_type,
        GRC::PollResponseType::YES_NO_ABSTAIN,
        7,
        title,
        "https://gridcoin.us",
        "Question?",
        GRC::Poll::ChoiceList(),
        0,
        Fraction());

    GRC::Contract contract = GRC::MakeContract<GRC::PollPayload>(
        uint32_t {2},
        action,
        payload_version,
        std::move(poll),
        GRC::PollEligibilityClaim());

    CMutableTransaction mtx;
    mtx.nTime = tx_time;
    mtx.vContracts.push_back(contract);

    return {contract, CTransaction(mtx)};
}

CBlockIndex MakeBlockIndex(const int height, const int64_t time)
{
    CBlockIndex index {};
    index.nHeight = height;
    index.nTime = time;

    return index;
}

void AddProtocolEntry(const std::string& key, const std::string& value, CBlockIndex& index)
{
    GRC::Contract contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
        uint32_t {3},
        GRC::ContractAction::ADD,
        uint32_t {2},
        key,
        value,
        GRC::ProtocolEntryStatus::ACTIVE);

    CMutableTransaction mtx;
    mtx.nTime = index.nTime;
    mtx.vContracts.push_back(contract);

    GRC::GetProtocolRegistry().Add({contract, CTransaction(mtx), &index});
}

//! The registries are process-wide singletons; leave them empty for the next
//! test. The block indexes a test registers must outlive this reset, so each
//! test declares them before its RegistryReset.
struct RegistryReset
{
    RegistryReset() { Reset(); }
    ~RegistryReset() { Reset(); }

    static void Reset()
    {
        LOCK(cs_main);
        GRC::GetPollRegistry().Reset();
        GRC::GetProtocolRegistry().Reset();
    }
};
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(poll_registry_tests)

BOOST_AUTO_TEST_CASE(reverting_a_poll_removes_it)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);

    registry.Add({poll.contract, poll.tx, &index});

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_REQUIRE(registry.TryByTxid(poll.tx.GetHash()) != nullptr);
    }

    registry.Revert({poll.contract, poll.tx, &index});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash()) == nullptr);
    BOOST_CHECK(registry.TryByTitle("title") == nullptr);
}

BOOST_AUTO_TEST_CASE(a_revert_removes_only_the_poll_its_contract_added)
{
    CBlockIndex index_first = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex index_second = MakeBlockIndex(V13Height() + 20, 1700001000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx first = MakePollTx("Title", 1700000000);
    const PollTx second = MakePollTx("Title", 1700001000);

    registry.Add({first.contract, first.tx, &index_first});
    registry.Add({second.contract, second.tx, &index_second});

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_REQUIRE(registry.TryByTxid(second.tx.GetHash()) == nullptr);
    }

    // A revert removes only the poll its own contract added.
    registry.Revert({second.contract, second.tx, &index_second});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    const GRC::PollReference* by_title = registry.TryByTitle("title");
    BOOST_REQUIRE(by_title != nullptr);
    BOOST_CHECK(by_title->Txid() == first.tx.GetHash());
    BOOST_CHECK(registry.TryByTxid(first.tx.GetHash()) == by_title);
}

BOOST_AUTO_TEST_CASE(a_master_key_remove_deletes_the_poll_it_names)
{
    CBlockIndex index_poll = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex index_remove = MakeBlockIndex(V13Height() + 20, 1700001000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    const PollTx remove = MakePollTx("Title", 1700001000, GRC::ContractAction::REMOVE);

    registry.Add({poll.contract, poll.tx, &index_poll});

    // A master-key REMOVE deletes the poll it names from both maps.
    registry.Delete({remove.contract, remove.tx, &index_remove});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(registry.TryByTitle("title") == nullptr);
    BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash()) == nullptr);
}

BOOST_AUTO_TEST_CASE(a_poll_registers_again_after_a_reorg_undoes_its_removal)
{
    CBlockIndex index_poll = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex index_remove = MakeBlockIndex(V13Height() + 20, 1700001000);
    CBlockIndex index_poll_again = MakeBlockIndex(V13Height() + 11, 1700000500);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    const PollTx remove = MakePollTx("Title", 1700001000, GRC::ContractAction::REMOVE);

    registry.Add({poll.contract, poll.tx, &index_poll});
    registry.Delete({remove.contract, remove.tx, &index_remove});

    // A reorg disconnects both blocks, the REMOVE's first. Reverting a REMOVE
    // reaches Add with the REMOVE's own transaction, which must not register
    // anything under the poll's title.
    registry.Revert({remove.contract, remove.tx, &index_remove});
    registry.Revert({poll.contract, poll.tx, &index_poll});

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_CHECK(registry.TryByTitle("title") == nullptr);
        BOOST_CHECK(registry.TryByTxid(remove.tx.GetHash()) == nullptr);
    }

    // The new chain carries the poll again but not the REMOVE.
    registry.Add({poll.contract, poll.tx, &index_poll_again});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);
    BOOST_CHECK(registry.TryByTitle("title") == ref);
}

BOOST_AUTO_TEST_CASE(a_poll_without_a_block_is_not_registered)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);

    // A poll still in the mempool has no block (the historical lookup can meet
    // one). Registering it would pin it to no block for good, because the later
    // add from its block finds the txid already present.
    registry.Add({poll.contract, poll.tx, nullptr});

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash()) == nullptr);
    }

    registry.Add({poll.contract, poll.tx, &index});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);
    BOOST_CHECK(ref->GetStartingBlockIndexPtr() == &index);
}

BOOST_AUTO_TEST_CASE(starting_block_is_the_block_that_registered_the_poll)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);

    // The poll transaction is not on disk. The starting block must come from
    // the registration, not from a transaction lookup.
    registry.Add({poll.contract, poll.tx, &index});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);
    BOOST_CHECK(ref->GetStartingBlockIndexPtr() == &index);
    BOOST_CHECK(ref->GetStartingHeight() == V13Height() + 10);
}

BOOST_AUTO_TEST_CASE(magnitude_weight_factor_is_the_entry_in_force_at_poll_start)
{
    CBlockIndex index_entry = MakeBlockIndex(V13Height() + 5, 1699990000);
    CBlockIndex index_poll = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    AddProtocolEntry("magnitudeweightfactor", "618034/1000000", index_entry);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);

    registry.Add({poll.contract, poll.tx, &index_poll});

    // The entry in force at the poll's start, not the consensus default, and
    // without waiting for a read from disk to correct it.
    LOCK(GRC::PollRegistry::cs_poll_registry);
    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);
    BOOST_CHECK(ref->GetMagnitudeWeightFactor() == Fraction(618034, 1000000, true));
}

BOOST_AUTO_TEST_CASE(magnitude_weight_factor_sees_an_earlier_stamped_entry_mined_later)
{
    CBlockIndex index_poll = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex index_entry = MakeBlockIndex(V13Height() + 11, 1699990000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);

    registry.Add({poll.contract, poll.tx, &index_poll});

    // An admin entry created before the poll but mined after it, as when the
    // admin transaction waits in the mempool. A node that replays the chain loads
    // it before it re-adds the poll, so a node that saw the poll arrive must
    // resolve the same factor once the entry is mined.
    AddProtocolEntry("magnitudeweightfactor", "618034/1000000", index_entry);

    LOCK(GRC::PollRegistry::cs_poll_registry);
    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);
    BOOST_CHECK(ref->GetMagnitudeWeightFactor() == Fraction(618034, 1000000, true));
}

BOOST_AUTO_TEST_SUITE_END()

// The active vote weight walks real blocks: it reads the last superblock at or
// before the poll's block from disk. These cases write that block to the
// regtest chain's block file and link it as the tip by hand. Connecting it
// would also store an accrual snapshot, which the research tally in this binary
// is not set up to do.
namespace {
//! A block linked as the tip, and where each extra transaction sits in it.
struct LinkedTip
{
    CBlockIndex* m_pindex;
    std::vector<CDiskTxPos> m_tx_positions;
};

//!
//! \brief Write a proof-of-stake block on \p prev to the block file and make it
//! the tip, as connecting it would.
//!
//! The block is stamped \p now, its coinbase carries \p claim, and \p extra
//! follows the coinstake. The caller's StateGuard puts the tip back and unlinks
//! the index again.
//!
LinkedTip LinkBlock(CBlockIndex* prev, const int64_t now, GRC::Claim claim,
                    const std::vector<CTransaction>& extra, const unsigned int bits)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const bool has_superblock = claim.ContainsSuperblock();

    CMutableTransaction coinbase;
    coinbase.nTime = now;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vout.resize(1);
    coinbase.vout[0].SetEmpty();
    coinbase.vContracts.push_back(GRC::MakeContract<GRC::Claim>(
        uint32_t {3}, GRC::ContractAction::ADD, std::move(claim)));

    // A coinstake makes the block proof-of-stake, so reading it back does not
    // check a proof of work.
    CMutableTransaction coinstake;
    coinstake.nTime = now;
    coinstake.vin.resize(1);
    coinstake.vin[0].prevout = COutPoint(grc_test::PremineCoinbase().GetHash(), 0);
    coinstake.vout.resize(2);
    coinstake.vout[0].SetEmpty();
    coinstake.vout[1] = CTxOut(COIN, grc_test::PremineScript());

    CBlock block;
    block.hashPrevBlock = prev->GetBlockHash();
    block.nTime = now;
    block.nBits = bits;
    block.vtx.push_back(CTransaction(coinbase));
    block.vtx.push_back(CTransaction(coinstake));
    block.vtx.insert(block.vtx.end(), extra.begin(), extra.end());
    block.hashMerkleRoot = BlockMerkleRoot(block);

    unsigned int file = 0;
    unsigned int block_pos = 0;
    BOOST_REQUIRE(WriteBlockToDisk(block, file, block_pos, Params().MessageStart()));

    LinkedTip tip;
    tip.m_pindex = GRC::MockBlockIndex::InsertBlockIndex(block.GetHash(true));
    tip.m_pindex->pprev = prev;
    tip.m_pindex->nHeight = prev->nHeight + 1;
    tip.m_pindex->nTime = now;
    tip.m_pindex->nBits = bits;
    tip.m_pindex->nFile = file;
    tip.m_pindex->nBlockPos = block_pos;
    tip.m_pindex->nMoneySupply = 1000 * COIN;

    if (has_superblock) {
        tip.m_pindex->MarkAsSuperblock();
    }

    for (const CTransaction& tx : extra) {
        if (!tx.vContracts.empty()) {
            tip.m_pindex->MarkAsContract();
        }
    }

    // As ConnectBlock locates each transaction.
    unsigned int tx_pos = block_pos
        + ::GetSerializeSize<CBlockHeader>(block, SER_DISK, CLIENT_VERSION)
        + GetSizeOfCompactSize(block.vtx.size());

    for (size_t i = 0; i < block.vtx.size(); ++i) {
        // The extra transactions follow the coinbase and the coinstake.
        if (i >= 2) {
            tip.m_tx_positions.emplace_back(file, block_pos, tx_pos);
        }

        tx_pos += ::GetSerializeSize(block.vtx[i], SER_DISK, CLIENT_VERSION);
    }

    prev->pnext = tip.m_pindex;
    pindexBest = tip.m_pindex;
    hashBestChain = tip.m_pindex->GetBlockHash();
    nBestHeight = tip.m_pindex->nHeight;

    return tip;
}

//!
//! \brief Write a block on genesis, stamped now, and make it the tip. A poll it
//! carries is still open.
//!
LinkedTip LinkTip(GRC::Claim claim, const std::vector<CTransaction>& extra, const unsigned int bits)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return LinkBlock(pindexGenesisBlock, GetAdjustedTime(), std::move(claim), extra, bits);
}

//! A version 2 vote for \p poll_txid. Linking a vote reads none of its claim.
CTransaction MakeVoteTx(const uint256& poll_txid, const int64_t tx_time)
{
    CMutableTransaction mtx;
    mtx.nTime = tx_time;
    mtx.vContracts.push_back(GRC::MakeContract<GRC::Vote>(
        uint32_t {2}, GRC::ContractAction::ADD,
        GRC::Vote::CURRENT_VERSION, poll_txid, std::vector<uint8_t> {0}, GRC::VoteWeightClaim()));

    return CTransaction(mtx);
}

//! A superblock claim with one CPID that no pool holds.
GRC::Claim SuperblockClaim()
{
    GRC::Superblock superblock(GRC::Superblock::CURRENT_VERSION);
    superblock.m_cpids.Add(GRC::Cpid::Parse("00010203040506070809101112131415"),
                           GRC::Magnitude::RoundFrom(100));
    superblock.m_projects.Add("regtest", GRC::Superblock::ProjectStats(1, 1, 1));
    BOOST_REQUIRE(superblock.WellFormed());

    GRC::Claim claim;
    claim.m_quorum_hash = superblock.GetHash();
    claim.m_superblock.Replace(std::move(superblock));

    return claim;
}

//! Index a transaction in the transaction index for the scope, so ReadDiskTx
//! finds it in the block LinkTip wrote.
class TxIndexScope
{
public:
    TxIndexScope(const CTransaction& tx, const CDiskTxPos& pos) : m_tx(tx)
    {
        CTxDB txdb;
        BOOST_REQUIRE(txdb.UpdateTxIndex(m_tx.GetHash(), CTxIndex(pos, m_tx.vout.size())));
    }

    ~TxIndexScope()
    {
        CTxDB txdb;
        txdb.EraseTxIndex(m_tx);
    }

    TxIndexScope(const TxIndexScope&) = delete;
    TxIndexScope& operator=(const TxIndexScope&) = delete;

private:
    const CTransaction m_tx;
};
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(poll_registry_chain_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

//!
//! The active vote weight uses the magnitude weight factor in the tallied
//! result's poll, the one the votes were weighed with. Resolving the factor
//! again could pick up a protocol entry connected during the tally, and the vote
//! percentage would then divide weights computed with two different factors.
//!
BOOST_AUTO_TEST_CASE(active_vote_weight_uses_the_factor_in_the_result)
{
    // Puts the tip back, and unlinks and drops the block index LinkTip adds.
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK2(cs_main, GRC::PollRegistry::cs_poll_registry);

    const CBlockIndex* const tip = LinkTip(SuperblockClaim(), {}, pindexGenesisBlock->nBits).m_pindex;
    BOOST_REQUIRE(tip->IsSuperblock());

    // The poll starts in the superblock's block, which is also the tip, so the
    // walk covers one block.
    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", tip->nTime, GRC::ContractAction::ADD,
                                   GRC::PollWeightType::BALANCE_AND_MAGNITUDE);

    registry.Add({poll.contract, poll.tx, tip});

    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);

    const auto active_vote_weight = [&](const Fraction& factor) {
        GRC::Poll weighed = poll.contract.CopyPayloadAs<GRC::PollPayload>().m_poll;
        weighed.m_magnitude_weight_factor = factor;

        return ref->GetActiveVoteWeight(GRC::PollResult(std::move(weighed)), tip);
    };

    const std::optional<CAmount> without_magnitude = active_vote_weight(Fraction(0, 1));
    const std::optional<CAmount> with_magnitude = active_vote_weight(Fraction(1, 1));
    BOOST_REQUIRE(without_magnitude && with_magnitude);

    // No pool is in the superblock, so for one block the active vote weight is
    // the network weight plus the money supply times the factor.
    BOOST_CHECK_EQUAL(*with_magnitude - *without_magnitude, tip->nMoneySupply);
}

//!
//! A v3 poll whose active vote weight is zero is recorded as not validated.
//! MeetsMinimumVotePercentAVW fails a zero active vote weight; the tally must
//! record that answer rather than leave the poll's validation unset.
//!
BOOST_AUTO_TEST_CASE(a_zero_active_vote_weight_does_not_validate_a_poll)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK2(cs_main, GRC::PollRegistry::cs_poll_registry);

    const PollTx poll = MakePollTx("Title", GetAdjustedTime(), GRC::ContractAction::ADD,
                                   GRC::PollWeightType::BALANCE, /*payload_version=*/3);

    // A target this easy puts the network weight, and so the active vote
    // weight of a poll weighed by balance alone, at zero.
    const LinkedTip tip = LinkTip(GRC::Claim(), {poll.tx}, 0x207fffff);
    BOOST_REQUIRE_EQUAL(tip.m_tx_positions.size(), 1U);
    const TxIndexScope indexed(poll.tx, tip.m_tx_positions[0]);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    registry.Add({poll.contract, poll.tx, tip.m_pindex});

    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);
    BOOST_REQUIRE_EQUAL(ref->GetPollPayloadVersion(), 3U);

    const GRC::PollResultOption result = GRC::PollResult::BuildFor(*ref, tip.m_pindex);
    BOOST_REQUIRE(result);
    BOOST_REQUIRE(result->m_active_vote_weight == CAmount {0});

    BOOST_CHECK(!result->m_vote_percent_avw);
    BOOST_REQUIRE(result->m_poll_results_validated);
    BOOST_CHECK(!*result->m_poll_results_validated);
}

//!
//! A poll loaded from the chain after a restart links its votes by the rule
//! AddVote applies as each block connects: from the transaction after the poll's
//! own, and by the time of the block that carries each vote.
//!
BOOST_AUTO_TEST_CASE(a_poll_loaded_from_the_chain_links_votes_by_the_live_rule)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK2(cs_main, GRC::PollRegistry::cs_poll_registry);

    const int64_t start = GetAdjustedTime() - 10 * 86400;
    PollTx poll = MakePollTx("Title", start);

    // GetTransaction reads a transaction through its first output, and a poll
    // transaction on the chain always has one.
    CMutableTransaction poll_mtx(poll.tx);
    poll_mtx.vout.emplace_back(0, CScript() << OP_RETURN);
    poll.tx = CTransaction(poll_mtx);
    const CTransaction ahead_of_poll = MakeVoteTx(poll.tx.GetHash(), start - 60);
    const CTransaction in_window = MakeVoteTx(poll.tx.GetHash(), start + 60);
    const CTransaction after_end = MakeVoteTx(poll.tx.GetHash(), start + 6 * 86400);

    const LinkedTip first = LinkBlock(pindexGenesisBlock, start, GRC::Claim(), {ahead_of_poll, poll.tx, in_window},
                                      pindexGenesisBlock->nBits);
    const LinkedTip second = LinkBlock(first.m_pindex, start + 8 * 86400, GRC::Claim(), {after_end},
                                       pindexGenesisBlock->nBits);
    BOOST_REQUIRE_EQUAL(first.m_tx_positions.size(), 3U);
    BOOST_REQUIRE_EQUAL(second.m_tx_positions.size(), 1U);

    const TxIndexScope poll_indexed(poll.tx, first.m_tx_positions[1]);

    const GRC::PollReference* ref =
        GRC::GetPollRegistry().TryByTxidWithAddHistoricalPollAndVotes(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);

    BOOST_REQUIRE_EQUAL(ref->Votes().size(), 1U);
    BOOST_CHECK(ref->Votes().front() == in_window.GetHash());
}

BOOST_AUTO_TEST_SUITE_END()

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
