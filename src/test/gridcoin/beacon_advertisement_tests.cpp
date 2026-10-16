// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! \file beacon_advertisement_tests.cpp
//! \brief The checks a beacon advertisement passes before it is sent.
//!
//! A send refuses while the node is out of sync, while the mempool holds a
//! beacon for the CPID, and while this wallet has a pending beacon for it on
//! chain.

#include "chain.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/cpid.h"
#include "gridcoin/researcher.h"
#include "gridcoin/staking/kernel.h"
#include "interfaces/researcher.h"
#include "key.h"
#include "primitives/block.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "txmempool.h"
#include "util.h"
#include "util/settings.h"
#include "util/time.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <optional>
#include <string>
#include <vector>

namespace {

//! Points pwalletMain at another wallet for the life of the object.
struct SwapWallet
{
    CWallet* m_saved;
    explicit SwapWallet(CWallet* replacement) : m_saved(pwalletMain) { pwalletMain = replacement; }
    ~SwapWallet() { pwalletMain = m_saved; }
};

//! Gives the global researcher a forced CPID for the life of the object, then
//! returns it to non-cruncher mode, as the researcher suite's own cases leave
//! it, and puts the settings back as they were.
struct ForcedResearcher
{
    util::Settings m_saved;

    explicit ForcedResearcher(const std::string& cpid_hex)
    {
        gArgs.LockSettings([&](util::Settings& settings) { m_saved = settings; });

        gArgs.ForceSetArg("forcecpid", cpid_hex);
        gArgs.ForceSetArg("noncruncher", "0");

        LOCK(cs_main);
        GRC::Researcher::Reload(GRC::MiningProjectMap());
    }

    ~ForcedResearcher()
    {
        gArgs.ForceSetArg("noncruncher", "1");

        {
            LOCK(cs_main);
            GRC::Researcher::Reload(GRC::MiningProjectMap());
        }

        gArgs.LockSettings([&](util::Settings& settings) { settings = m_saved; });
    }
};

//! Empties the mempool on entry and exit.
struct PoolCleared
{
    PoolCleared() { mempool.clear(); }
    ~PoolCleared() { mempool.clear(); }
};

//! The pooled beacon transactions for \p cpid.
std::vector<uint256> PooledBeacons(const GRC::Cpid& cpid)
{
    std::vector<uint256> hashes;

    LOCK(mempool.cs);

    for (const auto& [hash, entry] : mempool.mapTx) {
        if (entry.HasBeacon() && entry.GetBeaconCpid() == cpid) {
            hashes.push_back(hash);
        }
    }

    return hashes;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(beacon_advertisement_tests)

BOOST_AUTO_TEST_SUITE(sync_check)

BOOST_AUTO_TEST_CASE(a_beacon_is_not_advertised_while_out_of_sync)
{
    grc_test::MempoolStateGuard guard;

    // Empty and unencrypted: in sync, the send gets as far as the balance check.
    CWallet wallet;
    SwapWallet swap(&wallet);

    ForcedResearcher researcher("c0ffee00c0ffee00c0ffee00c0ffee00");
    BOOST_REQUIRE(GRC::Researcher::Get()->Id().Which() == GRC::MiningId::Kind::CPID);

    g_previous_block_time.store(0);
    BOOST_REQUIRE(OutOfSyncByAge());

    BOOST_CHECK(GRC::Researcher::Get()->AdvertiseBeacon().Error() == GRC::BeaconError::OUT_OF_SYNC);

    // Not kept as the researcher's beacon error: the snapshot would show it as
    // the beacon status after the node had caught up.
    BOOST_CHECK(GRC::Researcher::Get()->BeaconError() == GRC::BeaconError::NONE);

    g_previous_block_time.store(GetAdjustedTime());
    BOOST_REQUIRE(!OutOfSyncByAge());

    BOOST_CHECK(GRC::Researcher::Get()->AdvertiseBeacon().Error() == GRC::BeaconError::INSUFFICIENT_FUNDS);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(chain_flows,
                      *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::BEACON>>()
                      * boost::unit_test::fixture<grc_test::RegtestChainSetup>())

namespace {

void MineOne()
{
    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(grc_test::CreateAndProcessBlock(block, err), "could not mine: " << err);
}

//! Mine until the wallet can pay for a beacon and the node is in sync.
//!
//! The premine is at depth 1, and the balance check counts only coins with ten
//! confirmations, or the wallet's own with three; a coinstake is the wallet's
//! own. OutOfSyncByAge() measures the block BEFORE the tip, which is the
//! genesis block until a second block is mined.
void MineUntilReadyToAdvertise()
{
    for (int i = 0; i < 15 && (pwalletMain->GetBalance() < COIN || OutOfSyncByAge()); ++i) {
        MineOne();
    }

    BOOST_REQUIRE(pwalletMain->GetBalance() >= COIN);
    BOOST_REQUIRE(!OutOfSyncByAge());
}

//! Puts the mock clock and the sync time back as they were, however the case
//! exits.
struct ClockReset
{
    const std::chrono::seconds m_mock_time{GetMockTime()};
    const int64_t m_previous_block_time{g_previous_block_time.load()};

    ~ClockReset()
    {
        SetMockTime(m_mock_time);
        g_previous_block_time.store(m_previous_block_time);
    }
};

//! Move the mock clock forward, keeping the node in sync as if blocks had kept
//! arriving. The caller holds a ClockReset.
void AdvanceClock(const int64_t seconds)
{
    SetMockTime(GetAdjustedTime() + seconds);
    g_previous_block_time.store(GetAdjustedTime());
}

//! Mine until no beacon for \p cpid is left in the mempool.
//!
//! The miner leaves out a transaction stamped later than the block
//! (CreateRestOfTheBlock). Block times are masked down to a 16-second stake
//! slot, and blocks mined in quick succession share one, so the clock is moved
//! past the slot the transactions were stamped in. The caller holds a
//! ClockReset.
void MineUntilBeaconsConfirm(const GRC::Cpid& cpid)
{
    SetMockTime(GetAdjustedTime() + GRC::STAKE_TIMESTAMP_MASK + 1);

    for (int i = 0; i < 6 && !PooledBeacons(cpid).empty(); ++i) {
        MineOne();
    }

    BOOST_REQUIRE(PooledBeacons(cpid).empty());
}

interfaces::ResearcherSnapshot Snapshot()
{
    return interfaces::MakeResearcherContext(pwalletMain)->snapshot();
}

} // anonymous namespace

//! A beacon is pending, for the send path, once its advertisement is on chain,
//! and not before: until then the mempool check refuses a second
//! advertisement. It stays pending until a superblock sweeps it out, past its
//! pending age. The snapshot is what the GUI reads, across IPC in the
//! multiprocess build.
//!
//! Not a StateGuard in these cases: its restore would rewind the blocks they
//! mine underneath the suite's chain fixture, which rewinds them itself.
BOOST_FIXTURE_TEST_CASE(a_beacon_is_pending_once_its_advertisement_is_on_chain, grc_test::WalletTxScope)
{
    const ClockReset reset_clock;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "be11be11be11be11be11be11be11be11";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);
    ForcedResearcher forced(cpid_hex);

    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
    BOOST_REQUIRE(researcher->Id().Which() == GRC::MiningId::Kind::CPID);

    const GRC::AdvertiseBeaconResult first = researcher->AdvertiseBeacon();
    BOOST_REQUIRE_MESSAGE(first.TryPublicKey(), "advertisement failed: " << static_cast<int>(first.Error()));
    BOOST_REQUIRE_EQUAL(PooledBeacons(cpid).size(), 1U);

    const std::string verification_code = GRC::Beacon(*first.TryPublicKey()).GetVerificationCode();

    // In the mempool: not pending to the send path, which refuses a second
    // advertisement there, forced or not.
    BOOST_CHECK(!researcher->TryPendingBeacon());
    BOOST_CHECK(researcher->AdvertiseBeacon().Error() == GRC::BeaconError::ALEADY_IN_MEMPOOL);
    BOOST_CHECK(researcher->AdvertiseBeacon(/*force=*/true).Error() == GRC::BeaconError::ALEADY_IN_MEMPOOL);
    BOOST_CHECK_EQUAL(PooledBeacons(cpid).size(), 1U);

    // Waiting in the mempool, it is not shown as pending yet.
    interfaces::ResearcherSnapshot snap = Snapshot();
    BOOST_CHECK(!snap.has_pending_beacon);
    BOOST_CHECK(snap.beacon_status == interfaces::BeaconStatus::ALREADY_IN_MEMPOOL);

    // On chain: pending.
    MineUntilBeaconsConfirm(cpid);

    std::optional<GRC::Beacon> pending = researcher->TryPendingBeacon();
    BOOST_REQUIRE(pending);
    BOOST_CHECK(pending->m_public_key == *first.TryPublicKey());
    BOOST_CHECK(researcher->AdvertiseBeacon().Error() == GRC::BeaconError::PENDING);

    snap = Snapshot();
    BOOST_CHECK(snap.has_pending_beacon);
    BOOST_CHECK(snap.beacon_status == interfaces::BeaconStatus::PENDING);
    BOOST_CHECK_EQUAL(snap.beacon_verification_code, verification_code);

    // Past its pending age, with no superblock to sweep it out: it can still
    // activate, so it is still pending.
    AdvanceClock(GRC::PendingBeacon::RETENTION_AGE + 60 * 60);

    pending = researcher->TryPendingBeacon();
    BOOST_REQUIRE(pending);
    BOOST_CHECK(pending->m_public_key == *first.TryPublicKey());
    BOOST_CHECK(researcher->AdvertiseBeacon().Error() == GRC::BeaconError::PENDING);
    BOOST_CHECK(PooledBeacons(cpid).empty());

    snap = Snapshot();
    BOOST_CHECK(snap.has_pending_beacon);
    BOOST_CHECK_EQUAL(snap.time_to_pending_beacon_expiration, 0);
}

//! Only this wallet's pending beacon refuses; another wallet's does not.
BOOST_FIXTURE_TEST_CASE(another_wallets_pending_beacon_does_not_block_this_wallet, grc_test::WalletTxScope)
{
    const ClockReset reset_clock;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "0ddba11f0ddba11f0ddba11f0ddba11f";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);
    ForcedResearcher forced(cpid_hex);

    // The other wallet's beacon key: never added to this wallet. This wallet
    // only pays for the transaction.
    CKey other_key;
    other_key.MakeNewKey(true);

    GRC::BeaconPayload payload(cpid, GRC::Beacon(other_key.GetPubKey()));
    BOOST_REQUIRE(payload.Sign(other_key));

    {
        LOCK(cs_main);
        const std::string error = GRC::SendContract(
            GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, std::move(payload))).second;
        BOOST_REQUIRE_MESSAGE(error.empty(), error);
    }

    // Waiting in the mempool, it is not shown as this wallet's pending beacon.
    const interfaces::ResearcherSnapshot snap = Snapshot();
    BOOST_CHECK(!snap.has_pending_beacon);
    BOOST_CHECK(snap.beacon_status == interfaces::BeaconStatus::ALREADY_IN_MEMPOOL);

    MineUntilBeaconsConfirm(cpid);

    {
        LOCK(cs_main);
        BOOST_REQUIRE_EQUAL(GRC::GetBeaconRegistry().FindPending(cpid).size(), 1U);
    }

    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

    BOOST_CHECK(!researcher->TryPendingBeacon());

    const GRC::AdvertiseBeaconResult result = researcher->AdvertiseBeacon();
    BOOST_CHECK_MESSAGE(result.TryPublicKey(), "advertisement refused: " << static_cast<int>(result.Error()));
}


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END() // beacon_advertisement_tests
