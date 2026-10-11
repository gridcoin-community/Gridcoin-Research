// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/protocol.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/registry.h"
#include "primitives/transaction.h"

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
                  const GRC::ContractAction action = GRC::ContractAction::ADD)
{
    GRC::Poll poll(
        GRC::PollType::SURVEY,
        GRC::PollWeightType::BALANCE,
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
        uint32_t {2},
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

BOOST_AUTO_TEST_SUITE_END()

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
