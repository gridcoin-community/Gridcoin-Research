// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "dbwrapper.h"
#include "gridcoin/claim.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/protocol.h"
#include "gridcoin/quorum.h"
#include "gridcoin/superblock.h"
#include "gridcoin/voting/claims.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/poll_result_cache.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/result.h"
#include "gridcoin/voting/vote.h"
#include "index/disktxpos.h"
#include "index/txindex.h"
#include "interfaces/voting.h"
#include "key.h"
#include "node/blockstorage.h"
#include "node/ui_interface.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "rpc/blockchain.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "txdb.h"
#include "util.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <deque>
#include <future>

// Tests drive the PollRegistry contract handler directly, on the test thread.
// The handler is EXCLUSIVE_LOCKS_REQUIRED(cs_main); the tests take cs_main
// where the code asserts it, and suppress the analyzer for the rest.
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
    // A stored poll record carries its block's hash, so each test block index gets one, made from its height and
    // time. The hashes outlive every test (a deque keeps their addresses as it grows).
    static std::deque<uint256> hashes;
    hashes.push_back(ArithToUint256((arith_uint256(static_cast<uint64_t>(height)) << 64)
                                    | arith_uint256(static_cast<uint64_t>(time))));

    CBlockIndex index {};
    index.nHeight = height;
    index.nTime = time;
    index.phashBlock = &hashes.back();

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

//! A version 2 vote contract for \p poll_txid, as it reaches the registry from a block.
GRC::Contract MakeVoteContract(const uint256& poll_txid)
{
    return GRC::MakeContract<GRC::Vote>(
        uint32_t {2}, GRC::ContractAction::ADD,
        GRC::Vote::CURRENT_VERSION, poll_txid, std::vector<uint8_t> {0}, GRC::VoteWeightClaim());
}

//! A transaction carrying \p contract, stamped \p tx_time (which gives each one its own txid).
CTransaction MakeVoteTxFor(const GRC::Contract& contract, const int64_t tx_time)
{
    CMutableTransaction mtx;
    mtx.nTime = tx_time;
    mtx.vContracts.push_back(contract);

    return CTransaction(mtx);
}

//! The registry as a restarted node holds it.
void Restart(GRC::PollRegistry& registry) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    registry.ResetInMemoryOnly();
    registry.Initialize();
}
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
        // Below v15 a second poll with the same title key is registered by its txid, but the first keeps the
        // title.
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_REQUIRE(registry.TryByTxid(second.tx.GetHash()) != nullptr);
        BOOST_REQUIRE(registry.TryByTitle("title") == registry.TryByTxid(first.tx.GetHash()));
        BOOST_CHECK(registry.TitleOwner("title") == first.tx.GetHash());
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

BOOST_AUTO_TEST_CASE(the_poll_registry_is_backed_by_dbs_for_both_contract_types)
{
    BOOST_CHECK(GRC::RegistryBookmarks::IsRegistryBackedByDB(GRC::ContractType::POLL));
    BOOST_CHECK(GRC::RegistryBookmarks::IsRegistryBackedByDB(GRC::ContractType::VOTE));
    BOOST_CHECK(&GRC::RegistryBookmarks::GetRegistryWithDB(GRC::ContractType::POLL) == &GRC::GetPollRegistry());
    BOOST_CHECK(&GRC::RegistryBookmarks::GetRegistryWithDB(GRC::ContractType::VOTE) == &GRC::GetPollRegistry());
}

BOOST_AUTO_TEST_CASE(the_handler_stores_nothing_below_v11)
{
    CBlockIndex index = MakeBlockIndex(Params().GetConsensus().BlockV11Height - 1, 1600000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Old", 1600000000);

    registry.Add({poll.contract, poll.tx, &index});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash()) == nullptr);
    BOOST_CHECK(registry.TryByTitle("old") == nullptr);
    BOOST_CHECK(!registry.TitleOwner("old"));
    BOOST_CHECK(!registry.IsStored(poll.tx.GetHash()));
}

BOOST_AUTO_TEST_CASE(a_restarted_registry_holds_the_same_polls)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx first = MakePollTx("Alpha", 1700000000);
    const PollTx second = MakePollTx("Beta", 1700000100);

    registry.Add({first.contract, first.tx, &index});
    registry.Add({second.contract, second.tx, &index});

    registry.ResetInMemoryOnly();
    registry.Initialize();

    LOCK(GRC::PollRegistry::cs_poll_registry);
    const GRC::PollReference* alpha = registry.TryByTxid(first.tx.GetHash());
    const GRC::PollReference* beta = registry.TryByTxid(second.tx.GetHash());
    BOOST_REQUIRE(alpha != nullptr);
    BOOST_REQUIRE(beta != nullptr);
    BOOST_CHECK_EQUAL(alpha->Title(), "Alpha");
    BOOST_CHECK(registry.TryByTitle("beta") == beta);
    BOOST_CHECK(registry.TitleOwner("alpha") == first.tx.GetHash());
    BOOST_CHECK_EQUAL(registry.GetDBHeight(), V13Height() + 10);
}

BOOST_AUTO_TEST_CASE(votes_come_back_in_connection_order_after_a_restart)
{
    CBlockIndex poll_index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex vote_index = MakeBlockIndex(V13Height() + 11, 1700000600);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &poll_index});

    const GRC::Contract vote = MakeVoteContract(poll.tx.GetHash());
    const CTransaction v1 = MakeVoteTxFor(vote, 1700000500);
    const CTransaction v2 = MakeVoteTxFor(vote, 1700000400); // stamped earlier, later in the block
    registry.Add({vote, v1, &vote_index});
    registry.Add({vote, v2, &vote_index});

    const std::vector<uint256> expected {v1.GetHash(), v2.GetHash()};

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash())->Votes() == expected);
    }

    Restart(registry);

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash())->Votes() == expected);
    BOOST_CHECK(registry.VoteHead(poll.tx.GetHash()) == v2.GetHash());
}

BOOST_AUTO_TEST_CASE(a_vote_already_linked_is_not_linked_again)
{
    CBlockIndex poll_index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex vote_index = MakeBlockIndex(V13Height() + 11, 1700000600);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &poll_index});

    const GRC::Contract vote = MakeVoteContract(poll.tx.GetHash());
    const CTransaction v1 = MakeVoteTxFor(vote, 1700000500);
    registry.Add({vote, v1, &vote_index});
    registry.Add({vote, v1, &vote_index});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK_EQUAL(registry.TryByTxid(poll.tx.GetHash())->Votes().size(), 1U);
}

BOOST_AUTO_TEST_CASE(a_deleted_poll_keeps_its_title_and_leaves_the_display_map)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex remove_index = MakeBlockIndex(V13Height() + 12, 1700002000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    const PollTx remove = MakePollTx("Title", 1700001000, GRC::ContractAction::REMOVE);
    registry.Add({poll.contract, poll.tx, &index});
    registry.Delete({remove.contract, remove.tx, &remove_index});

    for (int pass = 0; pass < 2; ++pass) {
        {
            LOCK(GRC::PollRegistry::cs_poll_registry);
            BOOST_CHECK(registry.TryByTitle("title") == nullptr);
            BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash()) == nullptr);
            BOOST_CHECK(registry.TitleOwner("title") == poll.tx.GetHash());
            BOOST_CHECK(registry.IsStored(remove.tx.GetHash()));
        }

        // The second pass checks the restarted registry.
        Restart(registry);
    }
}

BOOST_AUTO_TEST_CASE(a_remove_resolves_only_to_the_active_display_poll)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex dup_index = MakeBlockIndex(V13Height() + 11, 1700000500);
    CBlockIndex remove_index = MakeBlockIndex(V13Height() + 12, 1700002000);
    CBlockIndex remove2_index = MakeBlockIndex(V13Height() + 13, 1700003000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx first = MakePollTx("Title", 1700000000);
    const PollTx duplicate = MakePollTx("TITLE", 1700000500); // the same title key, below v15
    const PollTx remove = MakePollTx("Title", 1700001000, GRC::ContractAction::REMOVE);
    const PollTx remove_again = MakePollTx("Title", 1700001500, GRC::ContractAction::REMOVE);

    registry.Add({first.contract, first.tx, &index});
    registry.Add({duplicate.contract, duplicate.tx, &dup_index});
    registry.Delete({remove.contract, remove.tx, &remove_index});
    registry.Delete({remove_again.contract, remove_again.tx, &remove2_index});

    LOCK(GRC::PollRegistry::cs_poll_registry);
    // The first REMOVE deletes the poll that owns the title. The second resolves to no ACTIVE poll and stores
    // nothing; the other poll stays registered by its txid.
    BOOST_CHECK(registry.TryByTxid(duplicate.tx.GetHash()) != nullptr);
    BOOST_CHECK(registry.IsStored(remove.tx.GetHash()));
    BOOST_CHECK(!registry.IsStored(remove_again.tx.GetHash()));
}

BOOST_AUTO_TEST_CASE(reverting_two_votes_of_one_block_newest_first_keeps_the_chain)
{
    CBlockIndex poll_index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex first_vote_index = MakeBlockIndex(V13Height() + 11, 1700000600);
    CBlockIndex vote_index = MakeBlockIndex(V13Height() + 12, 1700001200);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &poll_index});

    const GRC::Contract vote = MakeVoteContract(poll.tx.GetHash());
    const CTransaction v0 = MakeVoteTxFor(vote, 1700000550);
    const CTransaction v1 = MakeVoteTxFor(vote, 1700001100);
    const CTransaction v2 = MakeVoteTxFor(vote, 1700001150);
    registry.Add({vote, v0, &first_vote_index});
    registry.Add({vote, v1, &vote_index});
    registry.Add({vote, v2, &vote_index});

    // A block disconnect pops its records newest first: v2, then v1.
    registry.Revert({vote, v2, &vote_index});
    registry.Revert({vote, v1, &vote_index});

    for (int pass = 0; pass < 2; ++pass) {
        {
            LOCK(GRC::PollRegistry::cs_poll_registry);
            BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash())->Votes() == std::vector<uint256> {v0.GetHash()});
            BOOST_CHECK(registry.VoteHead(poll.tx.GetHash()) == v0.GetHash());
            BOOST_CHECK(!registry.IsStored(v1.GetHash()));
            BOOST_CHECK(!registry.IsStored(v2.GetHash()));
        }

        Restart(registry);
    }
}

BOOST_AUTO_TEST_CASE(reverting_a_removal_brings_the_poll_back)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex remove_index = MakeBlockIndex(V13Height() + 12, 1700002000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    const PollTx remove = MakePollTx("Title", 1700001000, GRC::ContractAction::REMOVE);
    registry.Add({poll.contract, poll.tx, &index});
    registry.Delete({remove.contract, remove.tx, &remove_index});
    registry.Revert({remove.contract, remove.tx, &remove_index});

    for (int pass = 0; pass < 2; ++pass) {
        {
            LOCK(GRC::PollRegistry::cs_poll_registry);
            BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash()) != nullptr);
            BOOST_CHECK(registry.TryByTitle("title") == registry.TryByTxid(poll.tx.GetHash()));
            BOOST_CHECK(!registry.IsStored(remove.tx.GetHash()));
        }

        Restart(registry);
    }
}

BOOST_AUTO_TEST_CASE(reverting_a_poll_frees_its_title)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &index});
    registry.Revert({poll.contract, poll.tx, &index});

    for (int pass = 0; pass < 2; ++pass) {
        {
            LOCK(GRC::PollRegistry::cs_poll_registry);
            BOOST_CHECK(!registry.TitleOwner("title"));
            BOOST_CHECK(!registry.IsStored(poll.tx.GetHash()));
        }

        Restart(registry);
    }
}

BOOST_AUTO_TEST_CASE(reconnecting_a_reverted_block_rebuilds_the_same_chain)
{
    CBlockIndex poll_index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex vote_index = MakeBlockIndex(V13Height() + 12, 1700001200);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &poll_index});

    const GRC::Contract vote = MakeVoteContract(poll.tx.GetHash());
    const CTransaction v1 = MakeVoteTxFor(vote, 1700001100);
    const CTransaction v2 = MakeVoteTxFor(vote, 1700001150);

    registry.Add({vote, v1, &vote_index});
    registry.Add({vote, v2, &vote_index});
    registry.Revert({vote, v2, &vote_index});
    registry.Revert({vote, v1, &vote_index});
    registry.Add({vote, v1, &vote_index});
    registry.Add({vote, v2, &vote_index});

    const std::vector<uint256> expected {v1.GetHash(), v2.GetHash()};

    for (int pass = 0; pass < 2; ++pass) {
        {
            LOCK(GRC::PollRegistry::cs_poll_registry);
            BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash())->Votes() == expected);
        }

        Restart(registry);
    }
}

BOOST_AUTO_TEST_CASE(a_block_revert_below_v15_pops_poll_and_vote_records_newest_first)
{
    grc_test::RequireV15Unscheduled();

    CBlockIndex poll_index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex vote_index = MakeBlockIndex(V13Height() + 12, 1700001200);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &poll_index});

    const GRC::Contract vote = MakeVoteContract(poll.tx.GetHash());
    const CTransaction v1 = MakeVoteTxFor(vote, 1700001100);
    const CTransaction v2 = MakeVoteTxFor(vote, 1700001150);

    CBlock block;
    block.vtx.resize(2);
    block.vtx.push_back(v1);
    block.vtx.push_back(v2);

    GRC::RegistryBookmarks heights;
    bool found = false;
    GRC::ApplyContracts(block, &vote_index, heights, found, false);
    GRC::RevertContracts(block, &vote_index);

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash())->Votes().empty());
    BOOST_CHECK(registry.VoteHead(poll.tx.GetHash()).IsNull());
    BOOST_CHECK(!registry.IsStored(v1.GetHash()));
    BOOST_CHECK(!registry.IsStored(v2.GetHash()));
}

BOOST_AUTO_TEST_CASE(a_link_reverted_out_of_order_flags_a_rebuild)
{
    CBlockIndex poll_index = MakeBlockIndex(V13Height() + 10, 1700000000);
    CBlockIndex vote_index = MakeBlockIndex(V13Height() + 12, 1700001200);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &poll_index});

    const GRC::Contract vote = MakeVoteContract(poll.tx.GetHash());
    const CTransaction v1 = MakeVoteTxFor(vote, 1700001100);
    const CTransaction v2 = MakeVoteTxFor(vote, 1700001150);
    registry.Add({vote, v1, &vote_index});
    registry.Add({vote, v2, &vote_index});

    // The older link first: out of order. Nothing is erased and the registry is flagged for a rebuild.
    registry.Revert({vote, v1, &vote_index});

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_CHECK(registry.IsStored(v1.GetHash()));
        BOOST_CHECK(registry.IsStored(v2.GetHash()));
    }

    uint32_t version = 1;
    std::pair<std::string, std::string> key = std::make_pair(std::string("poll_db"), std::string("version"));
    CTxDB txdb("r");
    BOOST_REQUIRE(txdb.ReadGenericSerializable(key, version));
    BOOST_CHECK_EQUAL(version, 0U);
}

BOOST_AUTO_TEST_CASE(the_replay_of_the_db_height_block_skips_its_vote_contracts)
{
    // The vote comes before its poll in the block, so when the block connects the poll is not registered yet and the
    // vote links to nothing. The startup replay of that block skips the vote: the loaded state is the state after
    // the block.
    CBlockIndex index = MakeBlockIndex(V13Height() + 12, 1700010000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700010000);
    const CTransaction vote_tx = MakeVoteTxFor(MakeVoteContract(poll.tx.GetHash()), 1700010100);

    CBlock block;
    block.vtx.resize(2);
    block.vtx.push_back(vote_tx);
    block.vtx.push_back(poll.tx);

    GRC::RegistryBookmarks live_heights;
    bool found = false;
    GRC::ApplyContracts(block, &index, live_heights, found, false);

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_REQUIRE(registry.TryByTxid(poll.tx.GetHash()) != nullptr);
        BOOST_REQUIRE(registry.TryByTxid(poll.tx.GetHash())->Votes().empty());
    }

    Restart(registry);

    GRC::RegistryBookmarks db_heights;
    BOOST_REQUIRE(db_heights.GetRegistryBlockHeight(GRC::ContractType::VOTE) == index.nHeight);
    GRC::ReplayBlock(block, &index, db_heights, true, found);

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(registry.TryByTxid(poll.tx.GetHash())->Votes().empty());
    BOOST_CHECK(!registry.IsStored(vote_tx.GetHash()));
}

BOOST_AUTO_TEST_CASE(the_replay_of_the_db_height_block_skips_its_poll_contracts)
{
    // A master-key removal comes before a poll with the title it names, so when the block connects it finds no poll
    // and does nothing. The startup replay of that block skips both contracts: the loaded state is the state after
    // the block.
    CBlockIndex index = MakeBlockIndex(V13Height() + 14, 1700020000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx removal = MakePollTx("Title", 1700019000, GRC::ContractAction::REMOVE);
    const PollTx poll = MakePollTx("Title", 1700020000);

    CBlock block;
    block.vtx.resize(2);
    block.vtx.push_back(removal.tx);
    block.vtx.push_back(poll.tx);

    GRC::RegistryBookmarks live_heights;
    bool found = false;
    GRC::ApplyContracts(block, &index, live_heights, found, false);

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_REQUIRE(registry.TryByTitle("title") != nullptr);
        BOOST_REQUIRE(!registry.IsStored(removal.tx.GetHash()));
    }

    Restart(registry);

    GRC::RegistryBookmarks db_heights;
    BOOST_REQUIRE(db_heights.GetRegistryBlockHeight(GRC::ContractType::POLL) == index.nHeight);
    GRC::ReplayBlock(block, &index, db_heights, true, found);

    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_CHECK(registry.TryByTitle("title") != nullptr);
        BOOST_CHECK(!registry.IsStored(removal.tx.GetHash()));
    }

    // The registry holds the same poll after a second restart.
    Restart(registry);

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(registry.TryByTitle("title") != nullptr);
}

BOOST_AUTO_TEST_CASE(clearing_either_history_clears_both_dbs)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    const GRC::Contract vote = MakeVoteContract(poll.tx.GetHash());
    const CTransaction v1 = MakeVoteTxFor(vote, 1700000100);
    registry.Add({poll.contract, poll.tx, &index});
    registry.Add({vote, v1, &index});

    // What -clearpollhistory or -clearvotehistory does before Initialize.
    registry.Reset();
    registry.Initialize();

    LOCK(GRC::PollRegistry::cs_poll_registry);
    BOOST_CHECK(!registry.IsStored(poll.tx.GetHash()));
    BOOST_CHECK(!registry.IsStored(v1.GetHash()));
    BOOST_CHECK_EQUAL(registry.GetDBHeight(), 0);
}

BOOST_AUTO_TEST_CASE(a_replayed_block_announces_no_poll)
{
    const int64_t now = GetAdjustedTime();
    CBlockIndex replayed_index = MakeBlockIndex(V13Height() + 10, now);
    CBlockIndex live_index = MakeBlockIndex(V13Height() + 11, now + 60);
    RegistryReset reset;
    LOCK(cs_main);

    int announced = 0;
    boost::signals2::scoped_connection watch = uiInterface.NewPollReceived_connect([&announced](int64_t) { ++announced; });

    const PollTx replayed = MakePollTx("Replayed", now);
    const PollTx live = MakePollTx("Live", now + 60);

    CBlock replayed_block;
    replayed_block.vtx.resize(2);
    replayed_block.vtx.push_back(replayed.tx);

    CBlock live_block;
    live_block.vtx.resize(2);
    live_block.vtx.push_back(live.tx);

    GRC::RegistryBookmarks heights;
    bool found = false;

    // A poll the startup replay or a rebuild applies is registered but not announced.
    GRC::ReplayBlock(replayed_block, &replayed_index, heights, true, found);
    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_REQUIRE(GRC::GetPollRegistry().TryByTxid(replayed.tx.GetHash()) != nullptr);
    }
    BOOST_CHECK_EQUAL(announced, 0);

    // A poll a connecting block applies is announced.
    GRC::ApplyContracts(live_block, &live_index, heights, found, false);
    BOOST_CHECK_EQUAL(announced, 1);
}

BOOST_AUTO_TEST_CASE(a_cyclic_vote_chain_is_cut_and_flags_a_rebuild)
{
    CBlockIndex index = MakeBlockIndex(V13Height() + 10, 1700000000);
    RegistryReset reset;
    LOCK(cs_main);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    const PollTx poll = MakePollTx("Title", 1700000000);
    registry.Add({poll.contract, poll.tx, &index});

    // Two links that point at each other, written straight to the vote db.
    const uint256 a = uint256S("a1");
    const uint256 b = uint256S("b2");
    GRC::VoteLinkDB links(1);

    for (const auto& [hash, previous] : {std::make_pair(a, b), std::make_pair(b, a)}) {
        GRC::VoteLinkEntry link;
        link.m_poll_txid = poll.tx.GetHash();
        link.m_hash = hash;
        link.m_previous_hash = previous;
        link.m_block_height = index.nHeight;
        link.m_status = GRC::VoteLinkStatus::ACTIVE;
        BOOST_REQUIRE(links.insert(hash, index.nHeight, link));
    }

    Restart(registry);

    // Reading the votes ends, and the registry is flagged for a rebuild at the next start.
    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_CHECK_LE(registry.TryByTxid(poll.tx.GetHash())->Votes().size(), 2U);
    }

    uint32_t version = 1;
    std::pair<std::string, std::string> key = std::make_pair(std::string("poll_db"), std::string("version"));
    BOOST_REQUIRE(CTxDB("r").ReadGenericSerializable(key, version));
    BOOST_CHECK_EQUAL(version, 0U);
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

    // Linking on a block that already has a successor is a reorg: unlink the branch it abandons, as
    // DisconnectBlocksBatch does, so IsInMainChain() sees those blocks off the main chain.
    for (CBlockIndex* pindex = prev->pnext; pindex;) {
        CBlockIndex* const next = pindex->pnext;
        pindex->pnext = nullptr;
        pindex = next;
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

//! A superblock claim with one CPID that no pool holds, of \p magnitude.
GRC::Claim SuperblockClaim(const double magnitude = 100)
{
    GRC::Superblock superblock(GRC::Superblock::CURRENT_VERSION);
    superblock.m_cpids.Add(GRC::Cpid::Parse("00010203040506070809101112131415"),
                           GRC::Magnitude::RoundFrom(magnitude));
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

constexpr int64_t DAY = 86400;

//! Make \p superblock, bound to \p pindex, the live current superblock for the scope, as connecting its block would.
class LiveSuperblockScope
{
public:
    LiveSuperblockScope(GRC::SuperblockPtr superblock, const CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
        : m_pindex(pindex)
    {
        superblock.Rebind(pindex);
        GRC::Quorum::PushSuperblock(std::move(superblock));
    }

    ~LiveSuperblockScope() EXCLUSIVE_LOCKS_REQUIRED(cs_main) { GRC::Quorum::PopSuperblock(m_pindex); }

    LiveSuperblockScope(const LiveSuperblockScope&) = delete;
    LiveSuperblockScope& operator=(const LiveSuperblockScope&) = delete;

private:
    const CBlockIndex* m_pindex;
};

//! The poll result cache is a process-wide singleton; leave it empty for the next test.
struct CacheReset
{
    CacheReset() { GRC::GetPollResultCache().Clear(); }
    ~CacheReset() { GRC::GetPollResultCache().Clear(); }
};

//! The registry's snapshot of the poll \p txid on \p pindex_tip, taken as the RPCs and the poll table take it.
GRC::PollSnapshot TakeSnapshot(const uint256& txid, const CBlockIndex* pindex_tip)
{
    LOCK2(cs_main, GRC::PollRegistry::cs_poll_registry);

    const GRC::PollReference* ref = GRC::GetPollRegistry().TryByTxid(txid);
    BOOST_REQUIRE(ref != nullptr);

    return GRC::GetPollRegistry().Snapshot(*ref, pindex_tip);
}

//! Whether the registry links \p vote to the poll \p poll_txid.
bool Linked(const uint256& poll_txid, const CTransaction& vote)
{
    LOCK(GRC::PollRegistry::cs_poll_registry);

    const GRC::PollReference* ref = GRC::GetPollRegistry().TryByTxid(poll_txid);
    BOOST_REQUIRE(ref != nullptr);

    const std::vector<uint256> votes = ref->Votes();

    return std::find(votes.begin(), votes.end(), vote.GetHash()) != votes.end();
}

//!
//! A poll, a vote in a block inside its window, its first block past the end, and a vote in the block after that.
//! The two votes carry one contract and differ by their transactions' times.
//!
struct WindowChain
{
    explicit WindowChain(const int64_t t0) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
        : poll(MakePollTx("Title", t0))
        , vote_contract(MakeVoteContract(poll.tx.GetHash()))
        , inside_vote(MakeVoteTxFor(vote_contract, t0 + DAY))
        , late_vote(MakeVoteTxFor(vote_contract, t0 + 7 * DAY - 20))
        , polled(LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, pindexGenesisBlock->nBits))
        , inside(LinkBlock(polled.m_pindex, t0 + DAY, GRC::Claim(), {inside_vote}, pindexGenesisBlock->nBits))
        , past_end(LinkBlock(inside.m_pindex, t0 + 7 * DAY + 60, GRC::Claim(), {}, pindexGenesisBlock->nBits))
        , late(LinkBlock(past_end.m_pindex, t0 + 7 * DAY - 10, GRC::Claim(), {late_vote}, pindexGenesisBlock->nBits))
    {
    }

    //! Apply the poll and both votes, in block order, as connecting the blocks does.
    void Register() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        GRC::PollRegistry& registry = GRC::GetPollRegistry();
        registry.Add({poll.contract, poll.tx, polled.m_pindex});
        registry.Add({vote_contract, inside_vote, inside.m_pindex});
        registry.Add({vote_contract, late_vote, late.m_pindex});
    }

    const PollTx poll;
    const GRC::Contract vote_contract;
    const CTransaction inside_vote;
    const CTransaction late_vote;
    const LinkedTip polled;
    const LinkedTip inside;
    const LinkedTip past_end;
    const LinkedTip late;
};

//! A transaction paying \p amount to \p key, its input made unique by \p salt.
CTransaction PayTo(const CKey& key, const CAmount amount, const int64_t tx_time, const uint32_t salt)
{
    CScript script;
    script.SetDestination(CTxDestination(key.GetPubKey().GetID()));

    CMutableTransaction mtx;
    mtx.nTime = tx_time;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(Hash(ToString(salt)), 0);
    mtx.vout.emplace_back(amount, script);

    return CTransaction(mtx);
}

//! A version 2 vote for \p poll_txid whose claim names output 0 of \p coin, signed by \p key, which \p coin pays.
CTransaction MakeClaimedVoteTx(const uint256& poll_txid, const CTransaction& coin, const CKey& key,
                               const int64_t tx_time)
{
    CMutableTransaction mtx;
    mtx.nTime = tx_time;

    GRC::Vote unsigned_vote;
    unsigned_vote.m_poll_txid = poll_txid;
    unsigned_vote.m_responses = {0};

    GRC::AddressClaim address_claim;
    address_claim.m_public_key = key.GetPubKey();
    address_claim.m_outpoints = {COutPoint(coin.GetHash(), 0)};
    CKey signer = key; // AddressClaim::Sign takes a mutable key.
    BOOST_REQUIRE(address_claim.Sign(signer, GRC::PackVoteMessage(unsigned_vote, CTransaction(mtx))));

    GRC::VoteWeightClaim claim;
    claim.m_balance_claim.m_address_claims.push_back(std::move(address_claim));

    mtx.vContracts.push_back(GRC::MakeContract<GRC::Vote>(
        uint32_t {2}, GRC::ContractAction::ADD,
        GRC::Vote::CURRENT_VERSION, poll_txid, std::vector<uint8_t> {0}, std::move(claim)));

    return CTransaction(mtx);
}
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

        return GRC::ActiveVoteWeight(registry.Snapshot(*ref, tip), GRC::PollResult(std::move(weighed)));
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
    LOCK(cs_main);

    const PollTx poll = MakePollTx("Title", GetAdjustedTime(), GRC::ContractAction::ADD,
                                   GRC::PollWeightType::BALANCE, /*payload_version=*/3);

    // A target this easy puts the network weight, and so the active vote
    // weight of a poll weighed by balance alone, at zero.
    const LinkedTip tip = LinkTip(GRC::Claim(), {poll.tx}, 0x207fffff);
    BOOST_REQUIRE_EQUAL(tip.m_tx_positions.size(), 1U);
    const TxIndexScope indexed(poll.tx, tip.m_tx_positions[0]);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    registry.Add({poll.contract, poll.tx, tip.m_pindex});

    // The result is built as the RPCs and the GUI build it: without the registry lock, which BuildFor takes where it
    // needs it. Reading a vote takes the block commit lock, which block connection takes before the registry lock.
    const GRC::PollReference* ref = nullptr;
    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        ref = registry.TryByTxid(poll.tx.GetHash());
        BOOST_REQUIRE(ref != nullptr);
        BOOST_REQUIRE_EQUAL(ref->GetPollPayloadVersion(), 3U);
    }

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

//!
//! A legacy poll weighs the magnitude a legacy vote states with the money supply
//! of the poll's last block and the total magnitude of the last superblock at or
//! before it. The result stays as it was when the poll ended, whatever the money
//! supply and the superblocks after it.
//!
BOOST_AUTO_TEST_CASE(a_legacy_poll_weighs_magnitude_as_of_its_end)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const int64_t start = GetAdjustedTime() - 20 * 86400;
    const auto legacy_tx = [](const int64_t time, const std::string& contract) {
        CMutableTransaction mtx;
        mtx.nVersion = 1;
        mtx.nTime = time;
        mtx.vout.emplace_back(0, CScript() << OP_RETURN);
        mtx.hashBoinc = contract;
        return CTransaction(mtx);
    };
    const CTransaction poll_tx = legacy_tx(start,
        "<MT>poll</MT><MK>Legacy poll</MK><MA>A</MA><MV><TITLE>Legacy poll</TITLE><DAYS>7</DAYS>"
        "<QUESTION>Question?</QUESTION><ANSWERS>Yes;No</ANSWERS><SHARETYPE>"
        + ToString(static_cast<uint32_t>(GRC::PollWeightType::BALANCE_AND_MAGNITUDE))
        + "</SHARETYPE><URL>https://gridcoin.us</URL></MV>");
    const CTransaction vote_tx = legacy_tx(start + 60,
        "<MT>vote</MT><MK>Legacy poll;voter</MK><MA>A</MA><MV><CPID>00010203040506070809101112131415</CPID>"
        "<BALANCE>100</BALANCE><MAGNITUDE>50</MAGNITUDE><ANSWER>Yes</ANSWER></MV>");

    // A superblock before the poll, one in it, the poll's last block, and a
    // superblock after it with a larger money supply.
    const unsigned int bits = pindexGenesisBlock->nBits;
    const LinkedTip before = LinkBlock(pindexGenesisBlock, start - 86400, SuperblockClaim(100), {}, bits);
    const LinkedTip polled = LinkBlock(before.m_pindex, start + 60, GRC::Claim(), {poll_tx, vote_tx}, bits);
    const LinkedTip within = LinkBlock(polled.m_pindex, start + 2 * 86400, SuperblockClaim(300), {}, bits);
    const LinkedTip last = LinkBlock(within.m_pindex, start + 6 * 86400, GRC::Claim(), {}, bits);
    const LinkedTip after = LinkBlock(last.m_pindex, start + 9 * 86400, SuperblockClaim(900), {}, bits);
    before.m_pindex->nMoneySupply = 400000 * COIN;
    polled.m_pindex->nMoneySupply = 410000 * COIN;
    within.m_pindex->nMoneySupply = 420000 * COIN;
    last.m_pindex->nMoneySupply = 430000 * COIN;
    after.m_pindex->nMoneySupply = 900000 * COIN;
    BOOST_REQUIRE_EQUAL(polled.m_tx_positions.size(), 2U);

    const TxIndexScope poll_indexed(poll_tx, polled.m_tx_positions[0]);
    const TxIndexScope vote_indexed(vote_tx, polled.m_tx_positions[1]);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    registry.Add({poll_tx.GetContracts()[0], poll_tx, polled.m_pindex});
    registry.Add({vote_tx.GetContracts()[0], vote_tx, polled.m_pindex});

    // Built without the registry lock, as the RPCs and the GUI build a result (see the case above).
    const GRC::PollReference* ref = nullptr;
    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        ref = registry.TryByTxid(poll_tx.GetHash());
        BOOST_REQUIRE(ref != nullptr);
        BOOST_REQUIRE_EQUAL(ref->Votes().size(), 1U);
    }

    const GRC::PollResultOption result = GRC::PollResult::BuildFor(*ref, after.m_pindex);
    BOOST_REQUIRE(result);
    BOOST_REQUIRE_EQUAL(result->m_votes.size(), 1U);

    // The legacy factor: money supply / (total magnitude + 0.01) / 5.67.
    const GRC::PollResult::Weight factor = static_cast<GRC::PollResult::Weight>(
        last.m_pindex->nMoneySupply / (300 + 0.01) / 5.67);
    const GRC::PollResult::Weight expected =
        factor * GRC::Magnitude::RoundFrom(50).Scaled() / GRC::Magnitude::SCALE_FACTOR + 100 * COIN;

    BOOST_CHECK_EQUAL(result->m_total_weight, expected);
}

//!
//! A tally reads only its snapshot: a vote linked after the snapshot is not counted, and the poll leaving the
//! registry does not stop the tally.
//!
BOOST_AUTO_TEST_CASE(a_tally_reads_only_its_snapshot)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 3600;
    const PollTx poll = MakePollTx("Title", t0);
    const CTransaction first = MakeVoteTx(poll.tx.GetHash(), t0 + 60);
    const CTransaction second = MakeVoteTx(poll.tx.GetHash(), t0 + 120);

    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip first_block = LinkBlock(polled.m_pindex, t0 + 60, GRC::Claim(), {first}, bits);
    const TxIndexScope poll_indexed(poll.tx, polled.m_tx_positions[0]);
    const TxIndexScope first_indexed(first, first_block.m_tx_positions[0]);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    registry.Add({poll.contract, poll.tx, polled.m_pindex});
    registry.Add({first.GetContracts()[0], first, first_block.m_pindex});

    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), first_block.m_pindex);
    BOOST_CHECK(snapshot.m_txid == poll.tx.GetHash());
    BOOST_CHECK(snapshot.m_pindex_start == polled.m_pindex);
    BOOST_CHECK(snapshot.m_pindex_tip == first_block.m_pindex);
    BOOST_CHECK(!snapshot.m_v15);
    BOOST_REQUIRE_EQUAL(snapshot.m_votes.size(), 1U);

    // A second vote is linked after the snapshot, and then the registry is emptied.
    const LinkedTip second_block = LinkBlock(first_block.m_pindex, t0 + 120, GRC::Claim(), {second}, bits);
    registry.Add({second.GetContracts()[0], second, second_block.m_pindex});
    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        BOOST_REQUIRE_EQUAL(registry.TryByTxid(poll.tx.GetHash())->Votes().size(), 2U);
    }
    RegistryReset::Reset();

    const GRC::PollResultOption result = GRC::PollResult::BuildFor(snapshot);
    BOOST_REQUIRE(result);
    BOOST_CHECK_EQUAL(result->m_vote_count, 1U);
}

//!
//! The superblock that weighs magnitude comes from the pinned tip's own chain. The live current superblock here is in
//! a block above the pinned tip: a tally on the pinned tip takes the superblock below the tip, and a tally on the
//! live superblock's block takes that one.
//!
BOOST_AUTO_TEST_CASE(the_superblock_that_weighs_magnitude_is_on_the_pinned_tip_s_chain)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 2 * DAY;
    const PollTx poll_tx = MakePollTx("Title", t0, GRC::ContractAction::ADD, GRC::PollWeightType::BALANCE_AND_MAGNITUDE);
    const LinkedTip before = LinkBlock(pindexGenesisBlock, t0 - DAY, SuperblockClaim(100), {}, bits);
    const LinkedTip polled = LinkBlock(before.m_pindex, t0, GRC::Claim(), {poll_tx.tx}, bits);
    const LinkedTip pinned = LinkBlock(polled.m_pindex, t0 + DAY, GRC::Claim(), {}, bits);
    const LinkedTip late = LinkBlock(pinned.m_pindex, t0 + DAY + 60, SuperblockClaim(900), {}, bits);
    const LiveSuperblockScope live(SuperblockClaim(900).m_superblock, late.m_pindex);

    // A poll read from a block carries no timestamp; the registry sets it from the transaction, as here.
    GRC::Poll poll = poll_tx.contract.CopyPayloadAs<GRC::PollPayload>().m_poll;
    poll.m_timestamp = t0;
    BOOST_REQUIRE(!poll.Expired(late.m_pindex->nTime));

    BOOST_CHECK_EQUAL(GRC::ResolveSuperblockForPoll(poll, pinned.m_pindex).m_height, before.m_pindex->nHeight);
    BOOST_CHECK_EQUAL(GRC::ResolveSuperblockForPoll(poll, late.m_pindex).m_height, late.m_pindex->nHeight);
}

//!
//! A poll stays open until a block past its end is on the chain the tally runs on, however far the wall clock has
//! passed the end.
//!
BOOST_AUTO_TEST_CASE(a_poll_is_open_until_a_block_past_its_end)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY; // the clock is a day past the poll's end
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip tip = LinkBlock(polled.m_pindex, t0 + DAY, GRC::Claim(), {}, bits);
    const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), tip.m_pindex);
    BOOST_CHECK(!snapshot.m_finished);
    BOOST_CHECK(snapshot.m_pindex_past_end == nullptr);
    BOOST_CHECK(snapshot.m_pindex_end == nullptr);

    const GRC::PollResultOption result = GRC::PollResult::BuildFor(snapshot);
    BOOST_REQUIRE(result);
    BOOST_CHECK(!result->m_finished);
}

//!
//! A poll finishes at its first block past the end, though the wall clock has not reached the end.
//!
BOOST_AUTO_TEST_CASE(a_poll_finishes_at_its_first_block_past_the_end_whatever_the_clock)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - DAY; // the clock is six days before the poll's end
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip past_end = LinkBlock(polled.m_pindex, t0 + 7 * DAY + 1, GRC::Claim(), {}, bits);
    const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex);
    BOOST_CHECK(snapshot.m_finished);
    BOOST_CHECK(snapshot.m_pindex_past_end == past_end.m_pindex);

    const GRC::PollResultOption result = GRC::PollResult::BuildFor(snapshot);
    BOOST_REQUIRE(result);
    BOOST_CHECK(result->m_finished);
}

//!
//! A table pinned on an earlier tip sees the poll as that tip's chain shows it: open, with its range ending at the
//! pinned tip, though the live chain already has a block past the end.
//!
BOOST_AUTO_TEST_CASE(the_pinned_tip_decides_whether_a_poll_has_finished)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip inside = LinkBlock(polled.m_pindex, t0 + DAY, GRC::Claim(), {}, bits);
    const LinkedTip past_end = LinkBlock(inside.m_pindex, t0 + 7 * DAY + 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    const GRC::PollSnapshot at_inside = TakeSnapshot(poll.tx.GetHash(), inside.m_pindex);
    BOOST_CHECK(!at_inside.m_finished);
    BOOST_CHECK(at_inside.m_pindex_past_end == nullptr);
    BOOST_CHECK(at_inside.m_pindex_end == nullptr);

    const GRC::PollSnapshot at_tip = TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex);
    BOOST_CHECK(at_tip.m_finished);
    BOOST_CHECK(at_tip.m_pindex_past_end == past_end.m_pindex);

    // The block found for the later tip is cached; the earlier tip still sees an open poll.
    BOOST_CHECK(!TakeSnapshot(poll.tx.GetHash(), inside.m_pindex).m_finished);
}

//!
//! A poll at or above the v15 height is finished from its first block past the end on: a later block does not reopen
//! it.
//!
BOOST_AUTO_TEST_CASE(a_later_block_does_not_reopen_a_v15_poll)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip past_end = LinkBlock(polled.m_pindex, end + 60, GRC::Claim(), {}, bits);
    const LinkedTip retrograde = LinkBlock(past_end.m_pindex, end - 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    {
        grc_test::V15HeightGuard v15(0);

        const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), retrograde.m_pindex);
        BOOST_REQUIRE(snapshot.m_v15);
        BOOST_CHECK(snapshot.m_finished);
        BOOST_CHECK(snapshot.m_pindex_past_end == past_end.m_pindex);
    }
}

//!
//! A poll that straddles the v15 height (its block below it, its first block past the end at or above it) finishes at
//! that block, and a later block does not reopen it. Only its end follows v15; its start rules do not.
//!
BOOST_AUTO_TEST_CASE(a_poll_that_straddles_the_v15_height_finishes_by_block)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip past_end = LinkBlock(polled.m_pindex, end + 60, GRC::Claim(), {}, bits);
    const LinkedTip retrograde = LinkBlock(past_end.m_pindex, end - 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    {
        grc_test::V15HeightGuard v15(past_end.m_pindex->nHeight);

        const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), retrograde.m_pindex);
        BOOST_REQUIRE(!snapshot.m_v15);
        BOOST_CHECK(snapshot.m_v15_end);
        BOOST_CHECK(snapshot.m_finished);
        BOOST_CHECK(snapshot.m_pindex_past_end == past_end.m_pindex);
    }
}

//!
//! A finished poll's active vote weight range ends at its first block at or past the end: a block stamped exactly at
//! the end closes it. Its first block past the end is the next one.
//!
BOOST_AUTO_TEST_CASE(a_finished_polls_range_ends_at_its_first_block_at_or_past_the_end)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip at_end = LinkBlock(polled.m_pindex, end, GRC::Claim(), {}, bits);
    const LinkedTip past_end = LinkBlock(at_end.m_pindex, end + 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex);
    BOOST_CHECK(snapshot.m_finished);
    BOOST_CHECK(snapshot.m_pindex_end == at_end.m_pindex);
    BOOST_CHECK(snapshot.m_pindex_past_end == past_end.m_pindex);
}

//!
//! A snapshot on a tip a reorg moved off the main chain throws, so the poll table retries on a fresh tip and an RPC
//! reports the reorg. The reorg here carries no poll or vote contract, so
//! reorg_occurred_during_reg_traversal is not set, and the snapshot's own check throws.
//!
BOOST_AUTO_TEST_CASE(a_snapshot_on_a_tip_off_the_main_chain_throws)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK2(cs_main, GRC::PollRegistry::cs_poll_registry);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip stale = LinkBlock(polled.m_pindex, t0 + 60, GRC::Claim(), {}, bits);
    const LinkedTip replacement = LinkBlock(polled.m_pindex, t0 + 120, GRC::Claim(), {}, bits);

    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    registry.Add({poll.contract, poll.tx, polled.m_pindex});
    const GRC::PollReference* ref = registry.TryByTxid(poll.tx.GetHash());
    BOOST_REQUIRE(ref != nullptr);

    BOOST_CHECK_THROW(registry.Snapshot(*ref, stale.m_pindex), GRC::InvalidDuetoReorgFork);
    BOOST_CHECK_NO_THROW(registry.Snapshot(*ref, replacement.m_pindex));
}

//!
//! The registry holds a poll whose block is off the main chain only through a corrupt poll record: a reorg that
//! disconnects a poll's block reverts the poll under the same cs_main. A snapshot reads such a poll as one with no
//! block and does not throw, so the poll's title and the poll table are still read. A pinned tip off the main chain
//! still throws.
//!
BOOST_AUTO_TEST_CASE(a_poll_whose_block_left_the_main_chain_is_read_as_having_no_block)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    CacheReset cache_reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    // Replace the poll's block without reverting the poll, as a corrupt record leaves the registry.
    const LinkedTip replacement = LinkBlock(pindexGenesisBlock, t0 + 60, GRC::Claim(), {}, bits);
    BOOST_REQUIRE(!polled.m_pindex->IsInMainChain());

    GRC::PollSnapshot snapshot;
    BOOST_REQUIRE_NO_THROW(snapshot = TakeSnapshot(poll.tx.GetHash(), replacement.m_pindex));
    BOOST_CHECK(snapshot.m_pindex_start == nullptr);
    BOOST_CHECK(!snapshot.m_finished);

    BOOST_CHECK_EQUAL(GRC::GetCurrentPollTitle(), "Title");
    BOOST_CHECK_EQUAL(GRC::GetPollResultCache().BuildPollTable(GRC::NO_FILTER).size(), 1U);

    BOOST_CHECK_THROW(TakeSnapshot(poll.tx.GetHash(), polled.m_pindex), GRC::InvalidDuetoReorgFork);
}

//!
//! A finished poll read as having no block (above) has no active vote weight range end either, not the one its search
//! for the end found while its block was on the main chain.
//!
BOOST_AUTO_TEST_CASE(a_poll_read_as_having_no_block_has_no_range_end)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip past_end = LinkBlock(polled.m_pindex, end + 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex);

    // Replace the poll's block without reverting the poll, as a corrupt record leaves the registry.
    const LinkedTip replacement = LinkBlock(pindexGenesisBlock, end + 120, GRC::Claim(), {}, bits);
    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), replacement.m_pindex);

    BOOST_REQUIRE(snapshot.m_finished);
    BOOST_CHECK(snapshot.m_pindex_end == nullptr);
}

//!
//! A reorg that replaces a poll's first block past the end: the next snapshot finds the new one.
//!
BOOST_AUTO_TEST_CASE(a_replaced_first_block_past_the_end_is_found_again)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip first = LinkBlock(polled.m_pindex, end + 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    BOOST_CHECK(TakeSnapshot(poll.tx.GetHash(), first.m_pindex).m_pindex_past_end == first.m_pindex);

    const LinkedTip second = LinkBlock(polled.m_pindex, end + 120, GRC::Claim(), {}, bits);
    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), second.m_pindex);
    BOOST_CHECK(snapshot.m_finished);
    BOOST_CHECK(snapshot.m_pindex_past_end == second.m_pindex);
}

//!
//! A reorg that leaves no block past the end on the main chain reopens a v15 poll: the first block past the end that a
//! snapshot found before the reorg is dropped, not served.
//!
BOOST_AUTO_TEST_CASE(a_reorg_that_removes_the_first_block_past_the_end_reopens_a_v15_poll)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip past_end = LinkBlock(polled.m_pindex, end + 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    grc_test::V15HeightGuard v15(0);

    BOOST_REQUIRE(TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex).m_pindex_past_end == past_end.m_pindex);

    // The replacement is as tall as the block it replaces, so the pinned-tip bound alone would still serve the cached
    // block; only the main-chain check drops it.
    const LinkedTip replacement = LinkBlock(polled.m_pindex, end - 60, GRC::Claim(), {}, bits);
    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), replacement.m_pindex);
    BOOST_REQUIRE(snapshot.m_v15);
    BOOST_CHECK(!snapshot.m_finished);
    BOOST_CHECK(snapshot.m_pindex_past_end == nullptr);
    BOOST_CHECK(snapshot.m_pindex_end == nullptr);
}

//!
//! A finished poll's cached result is served whatever the wall clock reads.
//!
BOOST_AUTO_TEST_CASE(a_finished_polls_cached_result_survives_any_wall_clock)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    CacheReset cache_reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    LinkBlock(polled.m_pindex, t0 + 8 * DAY, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    GRC::PollResultCache& cache = GRC::GetPollResultCache();
    {
        const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
        const std::vector<GRC::PollResultItem> table = cache.BuildPollTable(GRC::NO_FILTER);
        BOOST_REQUIRE_EQUAL(table.size(), 1U);
        BOOST_REQUIRE(table[0].result.m_finished);
    }

    SetMockTime(t0 + DAY); // the clock moves back before the poll's end
    const std::vector<GRC::PollResultItem> table = cache.BuildPollTable(GRC::NO_FILTER);
    BOOST_REQUIRE_EQUAL(table.size(), 1U);
    BOOST_CHECK(table[0].result.m_finished);
}

//!
//! An open poll's cached result is served at the tip it was tallied on, whatever the wall clock reads, and nowhere
//! else.
//!
BOOST_AUTO_TEST_CASE(an_open_polls_cached_result_serves_only_its_tip)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    CacheReset cache_reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    GRC::PollResultCache& cache = GRC::GetPollResultCache();
    {
        const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
        BOOST_REQUIRE_EQUAL(cache.BuildPollTable(GRC::NO_FILTER).size(), 1U);
    }

    SetMockTime(t0 + 8 * DAY); // the clock passes the poll's end; no block does
    const std::vector<GRC::PollResultItem> table = cache.BuildPollTable(GRC::NO_FILTER);
    BOOST_REQUIRE_EQUAL(table.size(), 1U);
    BOOST_CHECK(!table[0].result.m_finished);

    LinkBlock(polled.m_pindex, t0 + 3600, GRC::Claim(), {}, bits);
    BOOST_CHECK(cache.BuildPollTable(GRC::NO_FILTER).empty());
}

//!
//! A finished poll's cached result is served while its first block past the end stays on the chain, and dropped
//! when a reorg replaces that block.
//!
BOOST_AUTO_TEST_CASE(replacing_a_finished_polls_past_end_block_drops_its_cached_result)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    CacheReset cache_reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip past_end = LinkBlock(polled.m_pindex, t0 + 8 * DAY, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    GRC::PollResultCache& cache = GRC::GetPollResultCache();
    {
        const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
        BOOST_REQUIRE_EQUAL(cache.BuildPollTable(GRC::NO_FILTER).size(), 1U);
    }

    LinkBlock(past_end.m_pindex, t0 + 9 * DAY, GRC::Claim(), {}, bits);
    BOOST_CHECK_EQUAL(cache.BuildPollTable(GRC::NO_FILTER).size(), 1U);

    LinkBlock(polled.m_pindex, t0 + 8 * DAY + 60, GRC::Claim(), {}, bits);
    BOOST_CHECK(cache.BuildPollTable(GRC::NO_FILTER).empty());
}

//!
//! The poll table lists a poll as active until a block past its end, though the wall clock has passed the end.
//!
BOOST_AUTO_TEST_CASE(a_poll_table_lists_a_poll_as_active_until_a_block_past_its_end)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    CacheReset cache_reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    GRC::PollResultCache& cache = GRC::GetPollResultCache();
    const std::vector<GRC::PollResultItem> active = cache.BuildPollTable(GRC::ACTIVE);
    BOOST_REQUIRE_EQUAL(active.size(), 1U);
    BOOST_CHECK(!active[0].result.m_finished);
    BOOST_CHECK(cache.BuildPollTable(GRC::FINISHED).empty());
}

//!
//! The newest open poll, which getmininginfo's current_poll, the GUI's current poll title and the GUI's latest poll time
//! show, is judged by blocks on the tip like the poll table: open until a block past its end, however far the clock
//! has passed the end.
//!
BOOST_AUTO_TEST_CASE(the_newest_open_poll_is_judged_by_blocks)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip inside = LinkBlock(polled.m_pindex, t0 + DAY, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});
    const std::unique_ptr<interfaces::VotingManager> voting = interfaces::MakeVotingManager();

    // The clock is a day past the end, but no block is: the poll is open.
    BOOST_CHECK_EQUAL(GRC::GetCurrentPollTitle(), "Title");
    BOOST_CHECK_EQUAL(voting->latestActivePollTime(), t0);

    LinkBlock(inside.m_pindex, end + 60, GRC::Claim(), {}, bits);

    BOOST_CHECK_EQUAL(GRC::GetCurrentPollTitle(), _("No current polls"));
    BOOST_CHECK_EQUAL(voting->latestActivePollTime(), 0);
}

//!
//! The GUI reads the newest open poll's title and time through the voting interface on its own thread, at startup and
//! every minute. While another thread holds cs_main (a rescan can hold it for minutes), each read returns the last
//! value it read rather than wait for the lock.
//!
BOOST_AUTO_TEST_CASE(the_guis_reads_of_the_newest_open_poll_do_not_wait_on_cs_main)
{
    grc_test::StateGuard guard;
    RegistryReset reset;

    const int64_t t0 = GetAdjustedTime() - 8 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const std::unique_ptr<interfaces::VotingManager> voting = interfaces::MakeVotingManager();
    unsigned int bits = 0;
    CBlockIndex* pindex_polled = nullptr;
    {
        LOCK(cs_main);
        bits = pindexGenesisBlock->nBits;
        pindex_polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits).m_pindex;
        GRC::GetPollRegistry().Add({poll.contract, poll.tx, pindex_polled});
    }

    BOOST_REQUIRE_EQUAL(voting->currentPollTitle(), "Title");
    BOOST_REQUIRE_EQUAL(voting->latestActivePollTime(), t0);

    std::future<std::string> title;
    std::future<int64_t> time;
    bool title_waited = true;
    bool time_waited = true;
    {
        // A block past the end finishes the poll: a read that took cs_main now would find no open poll.
        LOCK(cs_main);
        LinkBlock(pindex_polled, end + 60, GRC::Claim(), {}, bits);

        title = std::async(std::launch::async, [&voting] { return voting->currentPollTitle(); });
        time = std::async(std::launch::async, [&voting] { return voting->latestActivePollTime(); });
        title_waited = title.wait_for(std::chrono::seconds(30)) != std::future_status::ready;
        time_waited = time.wait_for(std::chrono::seconds(30)) != std::future_status::ready;
    }

    BOOST_CHECK(!title_waited);
    BOOST_CHECK(!time_waited);
    BOOST_CHECK_EQUAL(title.get(), "Title");
    BOOST_CHECK_EQUAL(time.get(), t0);

    // With cs_main free again, each read sees the block past the end.
    BOOST_CHECK_EQUAL(voting->currentPollTitle(), _("No current polls"));
    BOOST_CHECK_EQUAL(voting->latestActivePollTime(), 0);
}

//!
//! A finished poll's active vote weight range end is found once, with its first block past the end, and served from
//! the poll's cache while that block stays on the main chain. A block's time never changes in production, so a second
//! walk would find the same block; the test changes one only to see whether the walk runs again.
//!
BOOST_AUTO_TEST_CASE(a_finished_polls_range_end_is_found_once)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip at_end = LinkBlock(polled.m_pindex, end, GRC::Claim(), {}, bits);
    const LinkedTip past_end = LinkBlock(at_end.m_pindex, end + 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    BOOST_REQUIRE(TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex).m_pindex_end == at_end.m_pindex);

    at_end.m_pindex->nTime = end - 1;
    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex);
    at_end.m_pindex->nTime = end;

    BOOST_CHECK(snapshot.m_pindex_end == at_end.m_pindex);
}

//!
//! An open poll's search for its first block past the end resumes where the last snapshot left it, rather than
//! walking again from the poll's block on every snapshot. As above, the test changes a block's time
//! only to see whether the walk passes that block again.
//!
BOOST_AUTO_TEST_CASE(an_open_polls_search_for_its_end_resumes_where_it_stopped)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 3 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip first = LinkBlock(polled.m_pindex, t0 + DAY, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    BOOST_REQUIRE(!TakeSnapshot(poll.tx.GetHash(), first.m_pindex).m_finished);

    const LinkedTip second = LinkBlock(first.m_pindex, t0 + 2 * DAY, GRC::Claim(), {}, bits);
    first.m_pindex->nTime = end + 60;
    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), second.m_pindex);
    first.m_pindex->nTime = t0 + DAY;

    BOOST_CHECK(snapshot.m_pindex_past_end == nullptr);
}

//!
//! From v15 a poll starts at its block: a tip past the end counted from the poll's transaction, but not
//! from its block, leaves the poll open, and the tally reads the block's time as the poll's start.
//!
BOOST_AUTO_TEST_CASE(a_v15_poll_starts_at_its_block)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const int64_t block_time = t0 + 600;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, block_time, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip between = LinkBlock(polled.m_pindex, t0 + 7 * DAY + 60, GRC::Claim(), {}, bits);
    const TxIndexScope indexed(poll.tx, polled.m_tx_positions[0]);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    {
        grc_test::V15HeightGuard v15(0);

        const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), between.m_pindex);
        BOOST_CHECK_EQUAL(snapshot.m_start_time, block_time);
        BOOST_CHECK_EQUAL(snapshot.m_end_time, block_time + 7 * DAY);
        BOOST_CHECK(!snapshot.m_finished);

        const GRC::PollResultOption result = GRC::PollResult::BuildFor(snapshot);
        BOOST_REQUIRE(result);
        BOOST_CHECK_EQUAL(result->m_poll.m_timestamp, block_time);
    }
}

//!
//! A poll that closes by block (its first block past the end at or above the v15 height) ends its active vote weight
//! range at its closing block, the block before its first block past the end, and its tally is bounded there; an
//! open v15 poll's tally is bounded at the pinned tip. A poll that straddles the height closes the same
//! way but has no bound while it is open.
//!
BOOST_AUTO_TEST_CASE(a_poll_that_closes_by_block_ends_at_its_closing_block)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip at_end = LinkBlock(polled.m_pindex, end, GRC::Claim(), {}, bits);
    const LinkedTip closing = LinkBlock(at_end.m_pindex, end - 10, GRC::Claim(), {}, bits);
    const LinkedTip past_end = LinkBlock(closing.m_pindex, end + 60, GRC::Claim(), {}, bits);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});

    {
        grc_test::V15HeightGuard v15(0);

        const GRC::PollSnapshot finished = TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex);
        BOOST_REQUIRE(finished.m_v15_end);
        BOOST_CHECK(finished.m_pindex_end == closing.m_pindex);
        BOOST_CHECK(finished.m_pindex_bound == closing.m_pindex);

        const GRC::PollSnapshot open = TakeSnapshot(poll.tx.GetHash(), closing.m_pindex);
        BOOST_REQUIRE(!open.m_finished);
        BOOST_CHECK(open.m_pindex_bound == closing.m_pindex);
    }

    {
        grc_test::V15HeightGuard v15(past_end.m_pindex->nHeight);

        const GRC::PollSnapshot finished = TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex);
        BOOST_REQUIRE(finished.m_v15_end);
        BOOST_REQUIRE(!finished.m_v15);
        BOOST_CHECK(finished.m_pindex_end == closing.m_pindex);
        BOOST_CHECK(finished.m_pindex_bound == closing.m_pindex);

        const GRC::PollSnapshot open = TakeSnapshot(poll.tx.GetHash(), closing.m_pindex);
        BOOST_REQUIRE(!open.m_finished);
        BOOST_CHECK(open.m_pindex_bound == nullptr);
    }
}

//!
//! From v15 a vote in the poll's own block is not linked, for a poll whose block is at the v15 height
//! itself.
//!
BOOST_AUTO_TEST_CASE(from_v15_a_vote_in_the_polls_own_block_is_not_linked)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const int64_t t0 = GetAdjustedTime() - DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const GRC::Contract vote_contract = MakeVoteContract(poll.tx.GetHash());
    const CTransaction vote = MakeVoteTxFor(vote_contract, t0);
    const unsigned int bits = pindexGenesisBlock->nBits;
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx, vote}, bits);
    GRC::PollRegistry& registry = GRC::GetPollRegistry();

    {
        grc_test::V15HeightGuard v15(polled.m_pindex->nHeight);
        registry.Add({poll.contract, poll.tx, polled.m_pindex});
        registry.Add({vote_contract, vote, polled.m_pindex});

        BOOST_CHECK(!Linked(poll.tx.GetHash(), vote));
    }
}

//!
//! From v15 a vote is linked from a block after the poll's block through its closing block, the block before its
//! first block past the end; no vote in a later block is linked.
//!
BOOST_AUTO_TEST_CASE(a_v15_vote_is_linked_only_through_the_closing_block)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const WindowChain chain(GetAdjustedTime() - 10 * DAY);

    {
        grc_test::V15HeightGuard v15(0);
        chain.Register();

        BOOST_CHECK(Linked(chain.poll.tx.GetHash(), chain.inside_vote));
        BOOST_CHECK(!Linked(chain.poll.tx.GetHash(), chain.late_vote));
    }
}

//!
//! A poll that straddles the v15 height (its block below it): a vote in a block after its first block past the end is
//! not linked when that block is at or above the height.
//!
BOOST_AUTO_TEST_CASE(a_vote_for_a_poll_that_straddles_the_v15_height_follows_its_first_block_past_the_end)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const WindowChain chain(GetAdjustedTime() - 10 * DAY);

    {
        grc_test::V15HeightGuard v15(chain.past_end.m_pindex->nHeight);
        chain.Register();

        BOOST_CHECK(Linked(chain.poll.tx.GetHash(), chain.inside_vote));
        BOOST_CHECK(!Linked(chain.poll.tx.GetHash(), chain.late_vote));
    }
}

//!
//! A reorg that replaces a v15 poll's first block past the end with a block stamped before the end reopens the poll:
//! a vote in a later block of the new branch is linked, and stays linked across a restart. The link reads the poll's
//! cached search, which holds only while its last block is on the main chain.
//!
BOOST_AUTO_TEST_CASE(a_vote_after_a_reorg_that_removes_the_first_block_past_the_end_is_linked)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - 10 * DAY;
    const int64_t end = t0 + 7 * DAY;
    const PollTx poll = MakePollTx("Title", t0);
    const GRC::Contract vote_contract = MakeVoteContract(poll.tx.GetHash());
    const CTransaction vote = MakeVoteTxFor(vote_contract, end - 25);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx}, bits);
    const LinkedTip inside = LinkBlock(polled.m_pindex, t0 + DAY, GRC::Claim(), {}, bits);
    const LinkedTip past_end = LinkBlock(inside.m_pindex, end + 60, GRC::Claim(), {}, bits);
    grc_test::V15HeightGuard v15(0);
    GRC::PollRegistry& registry = GRC::GetPollRegistry();
    registry.Add({poll.contract, poll.tx, polled.m_pindex});

    BOOST_REQUIRE(TakeSnapshot(poll.tx.GetHash(), past_end.m_pindex).m_pindex_past_end == past_end.m_pindex);

    const LinkedTip replacement = LinkBlock(inside.m_pindex, end - 30, GRC::Claim(), {}, bits);
    const LinkedTip voted = LinkBlock(replacement.m_pindex, end - 20, GRC::Claim(), {vote}, bits);
    registry.Add({vote_contract, vote, voted.m_pindex});

    BOOST_CHECK(Linked(poll.tx.GetHash(), vote));

    Restart(registry);

    BOOST_CHECK(Linked(poll.tx.GetHash(), vote));
}

//!
//! A tally bounded by blocks whose bound has left the main chain since its snapshot throws InvalidDuetoReorgFork, so
//! the poll table retries on a fresh tip and an RPC reports the reorg. The vote claims a coin, so the tally
//! looks a block up against its bound.
//!
BOOST_AUTO_TEST_CASE(a_tally_whose_bound_left_the_main_chain_throws)
{
    grc_test::StateGuard guard;
    RegistryReset reset;
    LOCK(cs_main);

    const unsigned int bits = pindexGenesisBlock->nBits;
    const int64_t t0 = GetAdjustedTime() - DAY;
    CKey key;
    key.MakeNewKey(true);
    const PollTx poll = MakePollTx("Title", t0);
    const CTransaction coin = PayTo(key, 100 * COIN, t0, 1);
    const CTransaction vote = MakeClaimedVoteTx(poll.tx.GetHash(), coin, key, t0 + 60);
    const LinkedTip polled = LinkBlock(pindexGenesisBlock, t0, GRC::Claim(), {poll.tx, coin}, bits);
    const LinkedTip voted = LinkBlock(polled.m_pindex, t0 + 60, GRC::Claim(), {vote}, bits);
    const TxIndexScope poll_indexed(poll.tx, polled.m_tx_positions[0]);
    const TxIndexScope coin_indexed(coin, polled.m_tx_positions[1]);
    const TxIndexScope vote_indexed(vote, voted.m_tx_positions[0]);
    grc_test::V15HeightGuard v15(0);
    GRC::GetPollRegistry().Add({poll.contract, poll.tx, polled.m_pindex});
    GRC::GetPollRegistry().Add({vote.vContracts[0], vote, voted.m_pindex});

    const GRC::PollSnapshot snapshot = TakeSnapshot(poll.tx.GetHash(), voted.m_pindex);
    BOOST_REQUIRE(snapshot.m_pindex_bound == voted.m_pindex);
    BOOST_REQUIRE_EQUAL(snapshot.m_votes.size(), 1U);

    // A reorg replaces the bound.
    LinkBlock(polled.m_pindex, t0 + 120, GRC::Claim(), {}, bits);

    BOOST_CHECK_THROW(GRC::PollResult::BuildFor(snapshot), GRC::InvalidDuetoReorgFork);
}

BOOST_AUTO_TEST_SUITE_END()

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
