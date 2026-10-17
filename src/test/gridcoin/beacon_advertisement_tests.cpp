// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! \file beacon_advertisement_tests.cpp
//! \brief The checks a beacon advertisement passes before it is sent, and what
//! happens to one that never confirms.
//!
//! A send refuses while the node is out of sync, while the mempool holds a
//! beacon for the CPID, and while this wallet has a pending beacon for it on
//! chain. The pool can hold the wallet's own transaction indefinitely, so
//! RetireStaleBeaconAdvertisements() takes this wallet's stale advertisement out
//! of it, and advertising again works.

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
#include "random.h"
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
#include <set>
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

//! \brief A wallet with coins to spend, and the node in sync.
//!
//! The transactions are synthetic and are put in the pool unvalidated: what is
//! under test is which pooled transactions are retired, not whether they would
//! be accepted.
struct RetirementFixture
{
    // Clears the pool on entry and exit, and puts g_previous_block_time back.
    grc_test::MempoolStateGuard m_guard;

    const GRC::Cpid m_cpid = GRC::Cpid::Parse("5e4a1c0ad3a3eaa2c5f2e8c1b2a0d1e3");

    CWallet m_wallet;
    CKey m_key;
    CScript m_to_us;
    CTransaction m_funding;

    RetirementFixture()
    {
        g_previous_block_time.store(GetAdjustedTime());

        m_key.MakeNewKey(true);
        m_to_us = CScript() << m_key.GetPubKey() << OP_CHECKSIG;

        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(GetRandHash(), 0);

        for (int i = 0; i < 4; ++i) {
            mtx.vout.emplace_back(COIN, m_to_us);
        }

        m_funding = CTransaction(mtx);

        LOCK(m_wallet.cs_wallet);
        BOOST_REQUIRE(m_wallet.AddKey(m_key));

        CWalletTx wtx(&m_wallet, m_funding);
        wtx.SetTxState(TxStateInMempool{});
        m_wallet.mapWallet[m_funding.GetHash()] = wtx;
    }

    //! A beacon contract transaction for \p cpid that spends \p prevout and pays
    //! its change to this wallet.
    CTransaction MakeBeaconTx(
        const GRC::Cpid& cpid,
        const COutPoint& prevout,
        const GRC::ContractAction action = GRC::ContractAction::ADD) const
    {
        CKey beacon_key;
        beacon_key.MakeNewKey(true);

        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = prevout;
        mtx.vout.emplace_back(COIN / 2, m_to_us);
        mtx.vContracts.push_back(GRC::MakeContract<GRC::BeaconPayload>(action, cpid, beacon_key.GetPubKey()));

        return CTransaction(mtx);
    }

    //! A plain transaction spending \p prevout, paying its change to this wallet.
    CTransaction MakeChildTx(const COutPoint& prevout) const
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = prevout;
        mtx.vout.emplace_back(COIN / 4, m_to_us);

        return CTransaction(mtx);
    }

    //! Record \p tx as this wallet's send, received \p age seconds ago, as
    //! CommitTransaction does: its spends, and its parents' outputs marked spent.
    void AddToWallet(const CTransaction& tx, const int64_t age)
    {
        LOCK(m_wallet.cs_wallet);

        CWalletTx wtx(&m_wallet, tx);
        wtx.SetTxState(TxStateInMempool{});
        wtx.fFromMe = true;
        wtx.nTimeReceived = GetAdjustedTime() - age;
        m_wallet.mapWallet[tx.GetHash()] = wtx;
        m_wallet.AddToSpends(tx, tx.GetHash());

        for (const CTxIn& txin : tx.vin) {
            const auto parent = m_wallet.mapWallet.find(txin.prevout.hash);

            if (parent != m_wallet.mapWallet.end()) {
                parent->second.MarkSpent(txin.prevout.n);
            }
        }
    }

    static void AddToPool(const CTransaction& tx)
    {
        LOCK(cs_main);
        mempool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(
            tx, /*fee=*/0, /*time=*/0, /*height=*/0,
            ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)));
    }

    unsigned int Retire(const GRC::Cpid& cpid)
    {
        LOCK2(cs_main, m_wallet.cs_wallet);
        return GRC::RetireStaleBeaconAdvertisements(m_wallet, cpid);
    }

    bool IsAbandoned(const uint256& hash)
    {
        LOCK(m_wallet.cs_wallet);
        return m_wallet.IsAbandoned(hash);
    }

    bool FundingSpent(const unsigned int n)
    {
        LOCK(m_wallet.cs_wallet);
        return m_wallet.mapWallet.at(m_funding.GetHash()).IsSpent(n);
    }
};

constexpr int64_t STALE = GRC::STALE_BEACON_ADVERTISEMENT_AGE + 60;
constexpr int64_t RECENT = GRC::STALE_BEACON_ADVERTISEMENT_AGE - 60;

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(beacon_advertisement_tests)

BOOST_AUTO_TEST_SUITE(retirement)

BOOST_FIXTURE_TEST_CASE(a_stale_advertisement_from_this_wallet_is_retired_with_its_descendants, RetirementFixture)
{
    const CTransaction advert = MakeBeaconTx(m_cpid, COutPoint(m_funding.GetHash(), 0));
    AddToWallet(advert, STALE);
    AddToPool(advert);

    // A later send that spent the advertisement's change. It cannot confirm
    // without the advertisement, so it goes too.
    const CTransaction child = MakeChildTx(COutPoint(advert.GetHash(), 0));
    AddToWallet(child, 0);
    AddToPool(child);

    BOOST_REQUIRE(mempool.HasBeaconForCpid(m_cpid));
    BOOST_REQUIRE(FundingSpent(0));

    BOOST_CHECK_EQUAL(Retire(m_cpid), 1U);

    BOOST_CHECK(!mempool.exists(advert.GetHash()));
    BOOST_CHECK(!mempool.exists(child.GetHash()));
    BOOST_CHECK(!mempool.HasBeaconForCpid(m_cpid));

    BOOST_CHECK(IsAbandoned(advert.GetHash()));
    BOOST_CHECK(IsAbandoned(child.GetHash()));

    // The coin the advertisement spent is selectable for the next one.
    BOOST_CHECK(!FundingSpent(0));
}

BOOST_FIXTURE_TEST_CASE(a_recent_advertisement_is_left_alone, RetirementFixture)
{
    const CTransaction advert = MakeBeaconTx(m_cpid, COutPoint(m_funding.GetHash(), 0));
    AddToWallet(advert, RECENT);
    AddToPool(advert);

    BOOST_CHECK_EQUAL(Retire(m_cpid), 0U);

    BOOST_CHECK(mempool.exists(advert.GetHash()));
    BOOST_CHECK(!IsAbandoned(advert.GetHash()));
    BOOST_CHECK(FundingSpent(0));
}

BOOST_FIXTURE_TEST_CASE(another_wallets_advertisement_is_left_alone, RetirementFixture)
{
    // Not in this wallet at all.
    const CTransaction foreign = MakeBeaconTx(m_cpid, COutPoint(GetRandHash(), 0));
    AddToPool(foreign);

    // In this wallet, because it pays to it, but spending none of its coins.
    const CTransaction paying_us = MakeBeaconTx(m_cpid, COutPoint(GetRandHash(), 1));
    AddToWallet(paying_us, STALE);
    AddToPool(paying_us);

    BOOST_CHECK_EQUAL(Retire(m_cpid), 0U);

    BOOST_CHECK(mempool.exists(foreign.GetHash()));
    BOOST_CHECK(mempool.exists(paying_us.GetHash()));
    BOOST_CHECK(!IsAbandoned(paying_us.GetHash()));
}

BOOST_FIXTURE_TEST_CASE(nothing_is_retired_while_out_of_sync, RetirementFixture)
{
    const CTransaction advert = MakeBeaconTx(m_cpid, COutPoint(m_funding.GetHash(), 0));
    AddToWallet(advert, STALE);
    AddToPool(advert);

    g_previous_block_time.store(0);
    BOOST_REQUIRE(OutOfSyncByAge());

    BOOST_CHECK_EQUAL(Retire(m_cpid), 0U);

    BOOST_CHECK(mempool.exists(advert.GetHash()));
    BOOST_CHECK(!IsAbandoned(advert.GetHash()));
}

BOOST_FIXTURE_TEST_CASE(only_advertisements_for_the_given_cpid_are_retired, RetirementFixture)
{
    const GRC::Cpid other_cpid = GRC::Cpid::Parse("0123456789abcdef0123456789abcdef");

    const CTransaction ours = MakeBeaconTx(m_cpid, COutPoint(m_funding.GetHash(), 0));
    AddToWallet(ours, STALE);
    AddToPool(ours);

    const CTransaction other = MakeBeaconTx(other_cpid, COutPoint(m_funding.GetHash(), 1));
    AddToWallet(other, STALE);
    AddToPool(other);

    BOOST_CHECK_EQUAL(Retire(m_cpid), 1U);

    BOOST_CHECK(!mempool.exists(ours.GetHash()));
    BOOST_CHECK(mempool.exists(other.GetHash()));
    BOOST_CHECK(!IsAbandoned(other.GetHash()));
}

BOOST_FIXTURE_TEST_CASE(a_stale_beacon_removal_is_not_an_advertisement, RetirementFixture)
{
    const CTransaction removal = MakeBeaconTx(
        m_cpid, COutPoint(m_funding.GetHash(), 0), GRC::ContractAction::REMOVE);
    AddToWallet(removal, STALE);
    AddToPool(removal);

    BOOST_CHECK_EQUAL(Retire(m_cpid), 0U);

    BOOST_CHECK(mempool.exists(removal.GetHash()));
    BOOST_CHECK(!IsAbandoned(removal.GetHash()));
}

BOOST_FIXTURE_TEST_CASE(an_advertisement_spending_another_is_retired_with_it, RetirementFixture)
{
    // A second stale advertisement for the CPID that spends the first's change.
    // Retiring the first takes it out of the pool too, and it is not counted
    // again. The pool is visited in hash order, so the pair is drawn until the
    // first's hash sorts before the second's: visited first, the second would be
    // retired on its own. Both hashes are random, so both are drawn each time.
    CTransaction first = MakeBeaconTx(m_cpid, COutPoint(m_funding.GetHash(), 0));
    CTransaction second = MakeBeaconTx(m_cpid, COutPoint(first.GetHash(), 0));

    for (int i = 0; i < 64 && !(first.GetHash() < second.GetHash()); ++i) {
        first = MakeBeaconTx(m_cpid, COutPoint(m_funding.GetHash(), 0));
        second = MakeBeaconTx(m_cpid, COutPoint(first.GetHash(), 0));
    }

    BOOST_REQUIRE(first.GetHash() < second.GetHash());
    AddToWallet(first, STALE);
    AddToPool(first);
    AddToWallet(second, STALE);
    AddToPool(second);

    BOOST_CHECK_EQUAL(Retire(m_cpid), 1U);

    BOOST_CHECK(!mempool.exists(first.GetHash()));
    BOOST_CHECK(!mempool.exists(second.GetHash()));
    BOOST_CHECK(IsAbandoned(first.GetHash()));
    BOOST_CHECK(IsAbandoned(second.GetHash()));
}

BOOST_FIXTURE_TEST_CASE(an_advertisement_a_reorganization_put_back_is_left_alone, RetirementFixture)
{
    const CTransaction advert = MakeBeaconTx(m_cpid, COutPoint(m_funding.GetHash(), 0));
    AddToWallet(advert, STALE);
    AddToPool(advert);

    // As a block disconnect marks it: it was in a block, so peers have it. A
    // peer fetching it since does not change that.
    mempool.AddUnbroadcast(advert.GetHash(), UnbroadcastReason::Reannounce);
    mempool.ClearUnbroadcastOnFetch(advert.GetHash());

    BOOST_CHECK_EQUAL(Retire(m_cpid), 0U);

    BOOST_CHECK(mempool.exists(advert.GetHash()));
    BOOST_CHECK(!IsAbandoned(advert.GetHash()));
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------

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

    // The snapshot shows it as pending while it waits to confirm.
    interfaces::ResearcherSnapshot snap = Snapshot();
    BOOST_CHECK(snap.has_pending_beacon);
    BOOST_CHECK(snap.beacon_status == interfaces::BeaconStatus::PENDING);
    BOOST_CHECK_EQUAL(snap.beacon_verification_code, verification_code);

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


//! An advertisement that never confirms: refused, and shown as pending, while
//! it is recent; once it is stale, shown as in the mempool and replaced by the
//! next advertisement.
BOOST_FIXTURE_TEST_CASE(advertising_again_replaces_an_advertisement_that_never_confirmed, grc_test::WalletTxScope)
{
    const ClockReset reset_clock;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "d1ce0000d1ce0000d1ce0000d1ce0000";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);
    ForcedResearcher forced(cpid_hex);

    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
    BOOST_REQUIRE(researcher->Id().Which() == GRC::MiningId::Kind::CPID);

    const GRC::AdvertiseBeaconResult first = researcher->AdvertiseBeacon();
    BOOST_REQUIRE_MESSAGE(first.TryPublicKey(), "first advertisement failed: " << static_cast<int>(first.Error()));

    const std::vector<uint256> first_pooled = PooledBeacons(cpid);
    BOOST_REQUIRE_EQUAL(first_pooled.size(), 1U);
    const uint256 first_hash = first_pooled.front();

    BOOST_CHECK(researcher->AdvertiseBeacon().Error() == GRC::BeaconError::ALEADY_IN_MEMPOOL);
    BOOST_CHECK(mempool.exists(first_hash));
    BOOST_CHECK(Snapshot().has_pending_beacon);

    AdvanceClock(STALE);

    interfaces::ResearcherSnapshot snap = Snapshot();
    BOOST_CHECK(!snap.has_pending_beacon);
    BOOST_CHECK(snap.beacon_status == interfaces::BeaconStatus::ALREADY_IN_MEMPOOL);

    const GRC::AdvertiseBeaconResult second = researcher->AdvertiseBeacon();
    BOOST_REQUIRE_MESSAGE(second.TryPublicKey(), "second advertisement failed: " << static_cast<int>(second.Error()));

    // The CPID has no beacon, so each advertisement generates a new key.
    BOOST_CHECK(*second.TryPublicKey() != *first.TryPublicKey());

    BOOST_CHECK(!mempool.exists(first_hash));
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK(pwalletMain->IsAbandoned(first_hash));
    }

    const std::vector<uint256> second_pooled = PooledBeacons(cpid);
    BOOST_REQUIRE_EQUAL(second_pooled.size(), 1U);
    BOOST_CHECK(second_pooled.front() != first_hash);

    snap = Snapshot();
    BOOST_CHECK(snap.has_pending_beacon);
    BOOST_CHECK_EQUAL(snap.beacon_verification_code, GRC::Beacon(*second.TryPublicKey()).GetVerificationCode());
}

//! Marks every unspent output of pwalletMain spent, except the ones given, and
//! puts them back however the case exits: a wallet with only those coins, for
//! a case that needs a small balance. A second wallet would need its own key
//! pool, which the test binary's shared mock database cannot keep apart.
struct OnlyTheseCoins
{
    std::vector<COutPoint> m_hidden;

    explicit OnlyTheseCoins(const std::set<COutPoint>& keep)
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        for (auto& [hash, wtx] : pwalletMain->mapWallet) {
            for (unsigned int n = 0; n < wtx.vout.size(); ++n) {
                if (keep.count(COutPoint(hash, n)) || wtx.IsSpent(n)
                    || pwalletMain->IsMine(wtx.vout[n]) == ISMINE_NO)
                {
                    continue;
                }

                wtx.MarkSpent(n);
                wtx.MarkDirty();
                m_hidden.emplace_back(hash, n);
            }
        }
    }

    ~OnlyTheseCoins()
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        for (const COutPoint& out : m_hidden) {
            const auto it = pwalletMain->mapWallet.find(out.hash);

            if (it != pwalletMain->mapWallet.end()) {
                it->second.MarkUnspent(out.n);
                it->second.MarkDirty();
            }
        }
    }
};

//! A wallet with no more than one advertisement's worth of coins can still
//! replace its stale advertisement: the coins it holds come back before the
//! balance is checked, instead of only its change being counted.
BOOST_FIXTURE_TEST_CASE(a_wallet_with_one_advertisements_worth_can_replace_it, grc_test::WalletTxScope)
{
    const ClockReset reset_clock;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    // 1.2 GRC to the wallet itself. An advertisement burns 0.5, which leaves its
    // change under the 1 GRC the balance check asks for.
    CPubKey own_key;
    BOOST_REQUIRE(pwalletMain->GetKeyFromPool(own_key, false));

    CWalletTx funding;
    const std::string error = pwalletMain->SendMoneyToDestination(own_key.GetID(), 12 * COIN / 10, funding);
    BOOST_REQUIRE_MESSAGE(error.empty(), error);

    std::optional<COutPoint> coin;
    for (unsigned int n = 0; n < funding.vout.size(); ++n) {
        if (funding.vout[n].nValue == 12 * COIN / 10) coin = COutPoint(funding.GetHash(), n);
    }
    BOOST_REQUIRE(coin);

    for (int i = 0; i < 6 && mempool.exists(funding.GetHash()); ++i) {
        SetMockTime(GetAdjustedTime() + GRC::STAKE_TIMESTAMP_MASK + 1);
        MineOne();
    }
    BOOST_REQUIRE(!mempool.exists(funding.GetHash()));
    g_previous_block_time.store(GetAdjustedTime());

    const OnlyTheseCoins only({*coin});
    BOOST_REQUIRE_EQUAL(pwalletMain->GetBalance(), 12 * COIN / 10);

    const std::string cpid_hex = "10a10a10a10a10a10a10a10a10a10a10";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);
    ForcedResearcher forced(cpid_hex);

    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

    const GRC::AdvertiseBeaconResult first = researcher->AdvertiseBeacon();
    BOOST_REQUIRE_MESSAGE(first.TryPublicKey(), "first advertisement failed: " << static_cast<int>(first.Error()));

    // What is left counted is the advertisement's change.
    BOOST_REQUIRE(pwalletMain->GetBalance() < COIN);

    AdvanceClock(STALE);

    const GRC::AdvertiseBeaconResult second = researcher->AdvertiseBeacon();
    BOOST_CHECK_MESSAGE(second.TryPublicKey(), "replacement refused: " << static_cast<int>(second.Error()));
    BOOST_CHECK_EQUAL(PooledBeacons(cpid).size(), 1U);
}

//! A send refused for another reason retires nothing: retirement only makes way
//! for an advertisement that is about to go out.
BOOST_FIXTURE_TEST_CASE(a_refused_send_retires_nothing, grc_test::WalletTxScope)
{
    const ClockReset reset_clock;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "5afe5afe5afe5afe5afe5afe5afe5afe";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);
    ForcedResearcher forced(cpid_hex);

    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

    BOOST_REQUIRE(researcher->AdvertiseBeacon().TryPublicKey());
    MineUntilBeaconsConfirm(cpid);
    BOOST_REQUIRE(researcher->TryPendingBeacon());

    // A forced advertisement goes out beside the pending beacon, which is what
    // forcing is for, and goes stale.
    BOOST_REQUIRE(researcher->AdvertiseBeacon(/*force=*/true).TryPublicKey());

    const std::vector<uint256> pooled = PooledBeacons(cpid);
    BOOST_REQUIRE_EQUAL(pooled.size(), 1U);

    AdvanceClock(STALE);

    // Refused as pending before any send, so the stale advertisement stays.
    BOOST_CHECK(researcher->AdvertiseBeacon().Error() == GRC::BeaconError::PENDING);
    BOOST_CHECK(mempool.exists(pooled.front()));
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_CHECK(!pwalletMain->IsAbandoned(pooled.front()));
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END() // beacon_advertisement_tests
