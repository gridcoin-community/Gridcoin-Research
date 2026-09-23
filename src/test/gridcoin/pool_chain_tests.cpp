// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! Pool contracts through real block connection and disconnection.
//!
//! pool_tests drives the POOL registry through GRC::ApplyContracts and
//! GRC::RevertContracts on a CBlockIndex built on the stack. That pins the
//! registry's own Apply/Revert symmetry, but not the wiring a real chain uses:
//! ConnectBlock marks a block as carrying contracts, and on a reorganize
//! DisconnectBlocksBatch walks that block's transactions and calls the
//! registry's Revert directly -- GRC::RevertContracts is not on that path at
//! all. A registry type missing from the revert-capable list, or a block never
//! marked as a contract block, would leave the registry holding a contract the
//! chain no longer has, and every stack-built test would still pass.
//!
//! These cases mine a POOL_REGISTER into a real regtest block
//! (RegtestChainSetup, test/chain_setup.h), reorganize it away, and reconnect it.
//!

#include "chain.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/pool.h"
#include "init.h"
#include "key.h"
#include "node/chainman.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "scheduler.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "txmempool.h"
#include "validation.h"
#include "validationinterface.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

using grc_test::CreateAndProcessBlock;
using grc_test::PremineCoinbase;
using grc_test::SpendablePremineOutputs;

namespace {

//! Registers the wallet for the validation signals for the scope, so that
//! AcceptToMemoryPool tells it about the funding spend. The wallet then marks
//! the premine output spent, and the staker -- which skips spent outputs --
//! cannot take it as the kernel of the block this case mines. Every signal used
//! is invoked synchronously; the scheduler never has to run.
struct SignalsForThisScope {
    CScheduler m_scheduler;
    SignalsForThisScope()
    {
        GetMainSignals().RegisterBackgroundSignalScheduler(m_scheduler);
        RegisterValidationInterface(pwalletMain);
    }
    ~SignalsForThisScope()
    {
        UnregisterValidationInterface(pwalletMain);
        GetMainSignals().UnregisterBackgroundSignalScheduler();
    }
};

bool Contains(const std::vector<CTransaction>& txs, const uint256& hash)
{
    for (const CTransaction& tx : txs) {
        if (tx.GetHash() == hash) return true;
    }
    return false;
}

//! A contract-only transaction carrying \p contract, applied off-chain. Used for
//! the Foundation OPEN the REGISTER consumes: the case is about the REGISTER's
//! block, and a real OPEN would need the Foundation master key.
CTransaction MakeOffChainPoolTx(GRC::Contract contract)
{
    CMutableTransaction mtx;
    mtx.nTime = static_cast<unsigned int>(grc_test::FixtureTxTime());
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256(), 1);
    mtx.vContracts.push_back(std::move(contract));
    return CTransaction(mtx);
}

//! Every field of a registry entry but the CPID, which is the lookup key,
//! compared one by one so a failure names the field that did not come back.
void CheckSameEntry(const GRC::Pool& got, const GRC::Pool& want, const std::string& when)
{
    BOOST_CHECK_MESSAGE(got.m_hash == want.m_hash, when << ": m_hash");
    BOOST_CHECK_MESSAGE(got.m_previous_hash == want.m_previous_hash, when << ": m_previous_hash");
    BOOST_CHECK_MESSAGE(got.m_status == want.m_status, when << ": m_status");
    BOOST_CHECK_MESSAGE(got.m_operator_key == want.m_operator_key, when << ": m_operator_key");
    BOOST_CHECK_MESSAGE(got.m_authorized_operator_key == want.m_authorized_operator_key,
                        when << ": m_authorized_operator_key");
    BOOST_CHECK_MESSAGE(got.m_authorization_height == want.m_authorization_height,
                        when << ": m_authorization_height");
    BOOST_CHECK_MESSAGE(got.m_height == want.m_height, when << ": m_height");
    BOOST_CHECK_MESSAGE(got.m_name == want.m_name, when << ": m_name");
    BOOST_CHECK_MESSAGE(got.m_url == want.m_url, when << ": m_url");
    BOOST_CHECK_MESSAGE(got.m_timestamp == want.m_timestamp, when << ": m_timestamp");
}

GRC::Pool EntryFor(const GRC::Cpid& cpid)
{
    LOCK(cs_main);
    GRC::Pool_ptr entry = GRC::GetPoolRegistry().Try(cpid);
    BOOST_REQUIRE_MESSAGE(entry, "no registry entry for the pool");
    return *entry;
}

//! Reorganize to \p hash and require that it became the tip.
void ReorganizeTo(const uint256& hash, const std::string& what)
{
    LOCK(cs_main);
    BOOST_REQUIRE_MESSAGE(ForceReorganizeToHash(hash), "could not reorganize to " << what);
    BOOST_REQUIRE_MESSAGE(hashBestChain == hash, what << " is not the tip after the reorganize");
}

} // anonymous namespace

// RegistryResetFor is declared BEFORE the chain fixture on purpose. Decorator
// fixtures tear down in reverse order, so the chain rewinds to genesis -- which
// reverts any pool block still connected -- while the registry still holds the
// entries that block's revert needs, and only then is the registry reset.
BOOST_AUTO_TEST_SUITE(pool_chain_tests, *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::POOL_REGISTER>>() *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

//!
//! A mined POOL_REGISTER changes the registry when its block connects, a
//! reorganize that disconnects the block puts every field of the entry back,
//! and reconnecting the same block reapplies it exactly.
//!
//! The REGISTER claims a builtin slot through a Foundation OPEN applied
//! off-chain first, so the chain carries only the block under test. The case
//! ends on the chain it started on, with the pool empty.
//!
BOOST_AUTO_TEST_CASE(a_mined_pool_register_reverts_on_disconnect_and_reapplies_on_reconnect)
{
    mempool.clear();
    grc_test::V15HeightGuard v15(0);

    const GRC::Cpid cpid =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);

    CKey operator_key;
    operator_key.MakeNewKey(true);

    uint256 start_tip;
    int start_height = 0;
    {
        LOCK(cs_main);
        BOOST_REQUIRE(pindexBest != nullptr);
        start_tip = hashBestChain;
        start_height = nBestHeight;
    }

    // The Foundation OPEN, at the tip's height so the authorization covers the
    // block about to be mined.
    const CTransaction open_tx = MakeOffChainPoolTx(GRC::MakeContract<GRC::PoolApprovePayload>(
        GRC::ContractAction::OPEN, cpid, operator_key.GetPubKey()));
    {
        CBlockIndex open_index;
        open_index.nHeight = start_height;

        GRC::RegistryBookmarks bookmarks;
        bool found_contract = false;

        LOCK(cs_main);
        GRC::ApplyContracts(open_tx, &open_index, bookmarks, found_contract);
    }

    const GRC::Pool opened = EntryFor(cpid);
    BOOST_REQUIRE_MESSAGE(opened.m_hash == open_tx.GetHash(), "the OPEN did not apply");
    BOOST_REQUIRE(opened.m_authorized_operator_key == operator_key.GetPubKey());

    GRC::PoolRegisterPayload payload(cpid, "grcpool.com", "https://grcpool.com/", operator_key.GetPubKey());
    BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, opened.m_hash));
    const GRC::Contract contract =
        GRC::MakeContract<GRC::PoolRegisterPayload>(GRC::ContractAction::ADD, std::move(payload));

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE_GE(coins.size(), 1u);

    // A transaction time of its own, later than the OPEN's, so m_timestamp is one
    // of the fields that changes on connect and has to come back on disconnect.
    const int64_t register_time = grc_test::FixtureTxTime() + 60;
    CTransaction register_tx = grc_test::CreateSpendWithContract(
        PremineCoinbase(), coins[0].n, 200000, contract, register_time, contract.RequiredBurnAmount());
    const uint256 register_hash = register_tx.GetHash();

    {
        const SignalsForThisScope signals;

        LOCK(cs_main);
        CValidationState state;
        BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, register_tx, state, nullptr),
                              "POOL_REGISTER rejected: " << state.GetRejectReason());
    }

    // Accepting to the pool applies nothing.
    CheckSameEntry(EntryFor(cpid), opened, "after the pool accept");

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(CreateAndProcessBlock(block, err), "could not mine: " << err);
    BOOST_REQUIRE_MESSAGE(Contains(block.vtx, register_hash), "the POOL_REGISTER was not mined");

    const uint256 mined = block.GetHash();
    {
        LOCK(cs_main);
        BOOST_REQUIRE_MESSAGE(hashBestChain == mined, "the mined block is not the tip");
    }

    // Connected: the REGISTER's entry, chained to the OPEN.
    const GRC::Pool registered = EntryFor(cpid);
    BOOST_CHECK(registered.m_hash == register_hash);
    BOOST_CHECK(registered.m_previous_hash == opened.m_hash);
    BOOST_CHECK(registered.m_status == GRC::PoolStatus::PENDING);
    BOOST_CHECK(registered.m_operator_key == operator_key.GetPubKey());
    BOOST_CHECK_EQUAL(registered.m_height, start_height + 1);
    BOOST_CHECK_EQUAL(registered.m_timestamp, register_time);
    BOOST_REQUIRE(registered.m_timestamp != opened.m_timestamp);

    // Disconnected by a reorganize: the entry the OPEN left, every field.
    ReorganizeTo(start_tip, "the starting tip");
    CheckSameEntry(EntryFor(cpid), opened, "after disconnecting the block");

    // Reconnected: the same block reapplies to the same entry.
    ReorganizeTo(mined, "the mined block");
    CheckSameEntry(EntryFor(cpid), registered, "after reconnecting the block");

    // Leave the chain as the case found it. The disconnect offers the block's
    // transactions back to the pool, so the pool is cleared as well.
    ReorganizeTo(start_tip, "the starting tip");
    CheckSameEntry(EntryFor(cpid), opened, "after the final disconnect");
    mempool.clear();

    // The OPEN was applied off-chain, so no block reverts it; the suite's
    // RegistryResetFor restores the booted registry after the chain rewinds.
}

BOOST_AUTO_TEST_SUITE_END()
