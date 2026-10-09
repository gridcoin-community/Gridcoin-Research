// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/handler.h"
#include "gridcoin/staking/status.h"
#include "gridcoin/voting/payloads.h"
#include "gridcoin/voting/poll.h"
#include "gridcoin/voting/registry.h"
#include "gridcoin/voting/vote.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/node.h"
#include "interfaces/voting.h"
#include "primitives/transaction.h"
#include "sync.h"

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// The GUI learns about new polls, votes and staking problems from these
// notifications. Under -multiprocess the node is gridcoinresearchd and the GUI
// subscribes over IPC, so each case subscribes the way that GUI does: through
// the interfaces the daemon serves (Init::makeVotingManager, Init::makeNode).

namespace {
const std::string POLL_TITLE = "voting notify test poll";

//! A transaction stamped with the current adjusted time (the default), so a
//! poll it carries is running now. The lock time only keeps the hashes apart.
CTransaction MakeTx(const uint32_t lock_time)
{
    CMutableTransaction tx;
    tx.nLockTime = lock_time;

    return CTransaction(tx);
}

GRC::Contract MakePollContract()
{
    GRC::PollPayload payload;
    payload.m_poll.m_title = POLL_TITLE;
    // The registry notifies about an added poll, and about a vote, only while
    // the poll is running, counted from its transaction time. A poll's default
    // duration is zero days.
    payload.m_poll.m_duration_days = GRC::Poll::MIN_DURATION_DAYS;

    return GRC::MakeContract<GRC::PollPayload>(GRC::ContractAction::ADD, std::move(payload));
}

GRC::Contract MakeVoteContract(const uint256& poll_txid)
{
    GRC::Vote vote;
    vote.m_poll_txid = poll_txid;

    return GRC::MakeContract<GRC::Vote>(GRC::ContractAction::ADD, std::move(vote));
}

//! A version 1 vote contract, which names its poll by title.
GRC::Contract MakeLegacyVoteContract()
{
    GRC::Contract contract = GRC::MakeLegacyContract(
        GRC::ContractType::VOTE,
        GRC::ContractAction::ADD,
        POLL_TITLE + ";investor",
        "<CPID>INVESTOR</CPID><BALANCE>0</BALANCE><MAGNITUDE>0</MAGNITUDE><ANSWER>yes</ANSWER>");

    contract.m_version = 1;

    return contract;
}

//! Adds and deletes one poll and its votes in the global poll registry, and
//! counts the poll and vote notifications a subscriber receives. The registry
//! is reset on entry and on exit.
struct PollRegistryFixture
{
    const CTransaction poll_tx{MakeTx(1)};
    const CTransaction vote_tx{MakeTx(2)};
    const CTransaction legacy_vote_tx{MakeTx(3)};
    const GRC::Contract poll_contract{MakePollContract()};
    const GRC::Contract vote_contract{MakeVoteContract(poll_tx.GetHash())};
    const GRC::Contract legacy_vote_contract{MakeLegacyVoteContract()};

    CBlockIndex index;

    int poll_calls{0};
    int vote_calls{0};

    std::unique_ptr<interfaces::VotingManager> voting;
    std::unique_ptr<interfaces::Handler> poll_handler;
    std::unique_ptr<interfaces::Handler> vote_handler;

    PollRegistryFixture()
    {
        // Of these steps only the version 2 vote's finished-poll check reads
        // the block time. Give it the poll's own time.
        index.nTime = poll_tx.nTime;

        ResetRegistry();
    }

    ~PollRegistryFixture()
    {
        poll_handler.reset();
        vote_handler.reset();

        ResetRegistry();
    }

    static void ResetRegistry()
    {
        LOCK(cs_main);
        GRC::GetPollRegistry().Reset();
    }

    //! Subscribe only after the setup steps, so a count covers the step
    //! under test and nothing before it.
    void Subscribe()
    {
        voting = interfaces::MakeGridcoinInit()->makeVotingManager();
        poll_handler = voting->handleNewPollReceived([this](int64_t) { ++poll_calls; });
        vote_handler = voting->handleNewVoteReceived([this](std::string) { ++vote_calls; });
    }

    void Add(const GRC::Contract& contract, const CTransaction& tx)
    {
        LOCK(cs_main);
        const GRC::ContractContext ctx{contract, tx, &index};
        GRC::GetPollRegistry().Add(ctx);
    }

    //! Revert an ADD contract, as disconnecting its block does.
    void Delete(const GRC::Contract& contract, const CTransaction& tx)
    {
        LOCK(cs_main);
        const GRC::ContractContext ctx{contract, tx, &index};
        GRC::GetPollRegistry().Delete(ctx);
    }

    bool PollRegistered() const
    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        return GRC::GetPollRegistry().TryByTxid(poll_tx.GetHash()) != nullptr;
    }

    size_t VoteCount() const
    {
        LOCK(GRC::PollRegistry::cs_poll_registry);
        const GRC::PollReference* poll_ref = GRC::GetPollRegistry().TryByTxid(poll_tx.GetHash());
        return poll_ref ? poll_ref->Votes().size() : 0;
    }
};

//! Disconnects the subscriber, then clears the global miner status's errors
//! and last search.
struct MinerStatusCleanup
{
    std::unique_ptr<interfaces::Handler> handler;

    ~MinerStatusCleanup()
    {
        if (handler) handler->disconnect();
        g_miner_status.ClearErrors();
        g_miner_status.ClearLastSearch();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(voting_notify_tests)

BOOST_FIXTURE_TEST_CASE(adding_a_poll_notifies_new_poll_subscribers, PollRegistryFixture)
{
    Subscribe();

    Add(poll_contract, poll_tx);

    BOOST_REQUIRE(PollRegistered());
    BOOST_CHECK_EQUAL(poll_calls, 1);
    BOOST_CHECK_EQUAL(vote_calls, 0);
}

BOOST_FIXTURE_TEST_CASE(deleting_a_poll_notifies_new_poll_subscribers, PollRegistryFixture)
{
    Add(poll_contract, poll_tx);
    BOOST_REQUIRE(PollRegistered());

    Subscribe();

    Delete(poll_contract, poll_tx);

    BOOST_REQUIRE(!PollRegistered());
    BOOST_CHECK_EQUAL(poll_calls, 1);
    BOOST_CHECK_EQUAL(vote_calls, 0);
}

BOOST_FIXTURE_TEST_CASE(adding_a_vote_notifies_new_vote_subscribers, PollRegistryFixture)
{
    Add(poll_contract, poll_tx);
    BOOST_REQUIRE(PollRegistered());

    Subscribe();

    Add(vote_contract, vote_tx);

    BOOST_REQUIRE_EQUAL(VoteCount(), 1U);
    BOOST_CHECK_EQUAL(vote_calls, 1);
    BOOST_CHECK_EQUAL(poll_calls, 0);
}

BOOST_FIXTURE_TEST_CASE(deleting_a_vote_notifies_new_vote_subscribers, PollRegistryFixture)
{
    Add(poll_contract, poll_tx);
    Add(vote_contract, vote_tx);
    BOOST_REQUIRE_EQUAL(VoteCount(), 1U);

    Subscribe();

    Delete(vote_contract, vote_tx);

    BOOST_REQUIRE_EQUAL(VoteCount(), 0U);
    BOOST_CHECK_EQUAL(vote_calls, 1);
    BOOST_CHECK_EQUAL(poll_calls, 0);
}

BOOST_FIXTURE_TEST_CASE(adding_a_legacy_vote_notifies_new_vote_subscribers, PollRegistryFixture)
{
    Add(poll_contract, poll_tx);
    BOOST_REQUIRE(PollRegistered());

    Subscribe();

    Add(legacy_vote_contract, legacy_vote_tx);

    BOOST_REQUIRE_EQUAL(VoteCount(), 1U);
    BOOST_CHECK_EQUAL(vote_calls, 1);
    BOOST_CHECK_EQUAL(poll_calls, 0);
}

BOOST_FIXTURE_TEST_CASE(deleting_a_legacy_vote_notifies_new_vote_subscribers, PollRegistryFixture)
{
    Add(poll_contract, poll_tx);
    Add(legacy_vote_contract, legacy_vote_tx);
    BOOST_REQUIRE_EQUAL(VoteCount(), 1U);

    Subscribe();

    Delete(legacy_vote_contract, legacy_vote_tx);

    BOOST_REQUIRE_EQUAL(VoteCount(), 0U);
    BOOST_CHECK_EQUAL(vote_calls, 1);
    BOOST_CHECK_EQUAL(poll_calls, 0);
}

// When SelectCoinsForStaking fails, the stake miner records the search with
// UpdateLastSearch, whose notification always says "staking", and then the
// error with UpdateCurrentErrors. Only that second notification can tell a
// subscriber that staking is not active.
BOOST_AUTO_TEST_CASE(a_staking_error_reaches_miner_status_subscribers_as_not_staking)
{
    std::vector<std::pair<bool, double>> delivered;
    MinerStatusCleanup cleanup;

    g_miner_status.ClearLastSearch();
    g_miner_status.ClearErrors();

    std::unique_ptr<interfaces::Node> node = interfaces::MakeGridcoinInit()->makeNode();
    cleanup.handler = node->handleMinerStatusChanged([&delivered](bool staking, double coin_weight) {
        delivered.emplace_back(staking, coin_weight);
    });

    // A zero search time keeps UpdateLastSearch from adding to the efficiency
    // metrics, which nothing resets.
    g_miner_status.UpdateLastSearch(
        /*kernel_found=*/false,
        /*search_timestamp=*/0,
        /*block_version=*/13,
        /*weight_sum=*/0,
        /*weight_min=*/0,
        /*weight_max=*/0,
        /*balance_weight=*/0);
    g_miner_status.UpdateCurrentErrors(GRC::MinerStatus::NO_COINS);

    BOOST_REQUIRE(!delivered.empty());
    BOOST_CHECK_EQUAL(delivered.size(), 2U);
    BOOST_CHECK(!delivered.back().first);
}

BOOST_AUTO_TEST_SUITE_END()
