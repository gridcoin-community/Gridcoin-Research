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
//!
//! The automated renewal is the one advertisement a wallet unlocked for staking
//! only sends. It is built in a fixed shape at the minimum fee, whatever fee is
//! configured for other sends, and every other advertisement on such a wallet
//! is refused.

#include "chain.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/cpid.h"
#include "gridcoin/researcher.h"
#include "gridcoin/sidestake.h"
#include "gridcoin/tx_message.h"
#include "init.h"
#include "interfaces/researcher.h"
#include "key.h"
#include "policy/fees.h"
#include "primitives/block.h"
#include "random.h"
#include "rpc/blockchain.h"
#include "script/interpreter.h"
#include "script/sign.h"
#include "support/allocators/secure.h"
#include "test/chain_setup.h"
#include "test/staking_test_peers.h"
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

BOOST_AUTO_TEST_SUITE(renewal_shape)

namespace {

//! What a beacon advertisement burns.
constexpr CAmount BURN = COIN / 2;

//! \brief A wallet with one coin, and the transaction the automated renewal
//! builds from it.
struct RenewalShapeFixture
{
    const GRC::Cpid m_cpid = GRC::Cpid::Parse("a11ce000a11ce000a11ce000a11ce000");
    const CAmount m_coin_value = 2 * COIN;

    CWallet m_wallet;
    CKey m_key;
    CKey m_beacon_key;
    CScript m_to_us;
    CTransaction m_funding;

    RenewalShapeFixture()
    {
        m_key.MakeNewKey(true);
        m_beacon_key.MakeNewKey(true);
        m_to_us.SetDestination(m_key.GetPubKey().GetID());

        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(GetRandHash(), 0);
        mtx.vout.emplace_back(m_coin_value, m_to_us);
        m_funding = CTransaction(mtx);

        LOCK(m_wallet.cs_wallet);
        BOOST_REQUIRE(m_wallet.AddKey(m_key));
        BOOST_REQUIRE(m_wallet.AddKey(m_beacon_key));

        m_wallet.mapWallet[m_funding.GetHash()] = CWalletTx(&m_wallet, m_funding);
    }

    CAmount MinimumFee(const CMutableTransaction& mtx)
    {
        LOCK(m_wallet.cs_wallet);
        const std::optional<CAmount> fee = GRC::RenewalMinimumFee(mtx, m_wallet);
        BOOST_REQUIRE(fee);

        return *fee;
    }

    //! Set output \p change to what is left of the coin after the other outputs
    //! and the minimum fee.
    void Rebalance(CMutableTransaction& mtx, const size_t change)
    {
        CAmount others = 0;

        for (size_t i = 0; i < mtx.vout.size(); ++i) {
            if (i != change) others += mtx.vout[i].nValue;
        }

        mtx.vout[change].nValue = m_coin_value - others - MinimumFee(mtx);
    }

    //! The fee \p mtx pays from the one coin.
    CAmount FeePaid(const CMutableTransaction& mtx) const
    {
        CAmount out = 0;

        for (const CTxOut& txout : mtx.vout) {
            out += txout.nValue;
        }

        return m_coin_value - out;
    }

    bool IsShape(const CMutableTransaction& mtx, const CAmount fee)
    {
        LOCK(m_wallet.cs_wallet);
        return GRC::IsRenewalShape(mtx, m_cpid, BURN, fee, m_wallet);
    }

    //! The shape the automated renewal builds: the beacon contract, the burn,
    //! change back to this wallet, and the minimum fee for its size.
    CMutableTransaction Canonical()
    {
        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.vin.emplace_back(COutPoint(m_funding.GetHash(), 0));
        mtx.vContracts.push_back(GRC::MakeContract<GRC::BeaconPayload>(
            GRC::ContractAction::ADD, m_cpid, m_beacon_key.GetPubKey()));
        mtx.vout.emplace_back(BURN, CScript() << OP_RETURN);
        mtx.vout.emplace_back(0, m_to_us);

        Rebalance(mtx, 1);

        return mtx;
    }
};

} // anonymous namespace

BOOST_FIXTURE_TEST_CASE(renewal_shape_accepts_the_canonical_shape, RenewalShapeFixture)
{
    const CMutableTransaction canonical = Canonical();

    BOOST_CHECK(IsShape(canonical, FeePaid(canonical)));

    // The minimum is the one for the signed transaction's size, so the answer is
    // the same before signing and after.
    CMutableTransaction signed_tx = canonical;
    {
        LOCK(m_wallet.cs_wallet);
        BOOST_REQUIRE(SignSignature(m_wallet, m_funding, signed_tx, 0));
    }

    const CTransaction final_tx(signed_tx);
    const unsigned int size = ::GetSerializeSize(final_tx, SER_NETWORK, PROTOCOL_VERSION);

    BOOST_CHECK_EQUAL(FeePaid(canonical), GetMinFee(final_tx, 1000, GMF_SEND, size));
    BOOST_CHECK_LT(FeePaid(canonical), COIN);
    BOOST_CHECK(IsShape(signed_tx, FeePaid(signed_tx)));
}

BOOST_FIXTURE_TEST_CASE(renewal_shape_rejects_extra_output, RenewalShapeFixture)
{
    CKey stranger;
    stranger.MakeNewKey(true);
    CScript to_stranger;
    to_stranger.SetDestination(stranger.GetPubKey().GetID());

    // A payment to a key this wallet does not hold.
    CMutableTransaction paying_out = Canonical();
    paying_out.vout.emplace_back(COIN / 10, to_stranger);
    Rebalance(paying_out, 1);
    BOOST_CHECK(!IsShape(paying_out, FeePaid(paying_out)));

    // A second output to this wallet. There is room for one change output.
    CMutableTransaction second_change = Canonical();
    second_change.vout.emplace_back(COIN / 10, m_to_us);
    Rebalance(second_change, 1);
    BOOST_CHECK(!IsShape(second_change, FeePaid(second_change)));

    // The change paid to a key this wallet does not hold.
    CMutableTransaction foreign_change = Canonical();
    foreign_change.vout[1].scriptPubKey = to_stranger;
    BOOST_CHECK(!IsShape(foreign_change, FeePaid(foreign_change)));

    // A burn of another amount.
    CMutableTransaction burning_more = Canonical();
    burning_more.vout[0].nValue += COIN / 10;
    Rebalance(burning_more, 1);
    BOOST_CHECK(!IsShape(burning_more, FeePaid(burning_more)));

    // No burn at all.
    CMutableTransaction no_burn = Canonical();
    no_burn.vout.erase(no_burn.vout.begin());
    Rebalance(no_burn, 0);
    BOOST_CHECK(!IsShape(no_burn, FeePaid(no_burn)));

    // The same check passes the transaction these were made from.
    const CMutableTransaction canonical = Canonical();
    BOOST_CHECK(IsShape(canonical, FeePaid(canonical)));
}

BOOST_FIXTURE_TEST_CASE(renewal_shape_rejects_a_fee_above_the_minimum, RenewalShapeFixture)
{
    CMutableTransaction overpaying = Canonical();
    const CAmount minimum = FeePaid(overpaying);

    overpaying.vout[1].nValue -= 1;
    BOOST_CHECK(!IsShape(overpaying, minimum + 1));

    // Nor does the minimum, stated as the fee, pass a transaction that pays more.
    BOOST_CHECK(!IsShape(overpaying, minimum));

    // The fee a configured fee rate of a tenth of a coin would set.
    CMutableTransaction configured = Canonical();
    configured.vout[1].nValue -= COIN / 10 - minimum;
    BOOST_CHECK(!IsShape(configured, FeePaid(configured)));

    // Less than the minimum fails too: the transaction would not be relayed.
    CMutableTransaction underpaying = Canonical();
    underpaying.vout[1].nValue += 1;
    BOOST_CHECK(!IsShape(underpaying, FeePaid(underpaying)));
}

BOOST_FIXTURE_TEST_CASE(renewal_shape_rejects_a_non_beacon_contract, RenewalShapeFixture)
{
    // A message.
    CMutableTransaction message = Canonical();
    message.vContracts = {GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "not a renewal")};
    Rebalance(message, 1);
    BOOST_CHECK(!IsShape(message, FeePaid(message)));

    // A beacon for another CPID.
    CMutableTransaction other_cpid = Canonical();
    other_cpid.vContracts = {GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::ADD, GRC::Cpid::Parse("0123456789abcdef0123456789abcdef"), m_beacon_key.GetPubKey())};
    Rebalance(other_cpid, 1);
    BOOST_CHECK(!IsShape(other_cpid, FeePaid(other_cpid)));

    // A beacon removal.
    CMutableTransaction removal = Canonical();
    removal.vContracts = {GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::REMOVE, m_cpid, m_beacon_key.GetPubKey())};
    Rebalance(removal, 1);
    BOOST_CHECK(!IsShape(removal, FeePaid(removal)));

    // The beacon contract and a second contract.
    CMutableTransaction two_contracts = Canonical();
    two_contracts.vContracts.push_back(GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "and more"));
    Rebalance(two_contracts, 1);
    BOOST_CHECK(!IsShape(two_contracts, FeePaid(two_contracts)));

    // No contract.
    CMutableTransaction no_contract = Canonical();
    no_contract.vContracts.clear();
    Rebalance(no_contract, 1);
    BOOST_CHECK(!IsShape(no_contract, FeePaid(no_contract)));
}

BOOST_FIXTURE_TEST_CASE(renewal_shape_rejects_a_coin_this_wallet_does_not_hold, RenewalShapeFixture)
{
    CMutableTransaction foreign_input = Canonical();
    const CAmount fee = FeePaid(foreign_input);

    foreign_input.vin.emplace_back(COutPoint(GetRandHash(), 0));

    BOOST_CHECK(!IsShape(foreign_input, fee));

    LOCK(m_wallet.cs_wallet);
    BOOST_CHECK(!GRC::RenewalMinimumFee(foreign_input, m_wallet));
}

//! The renewal spends whichever of the wallet's coins are selected, and the
//! minimum fee sizes each standard form at the largest signature it carries, so
//! the shape holds before signing and after.
BOOST_FIXTURE_TEST_CASE(renewal_shape_sizes_each_form_of_coin_the_wallet_signs, RenewalShapeFixture)
{
    const CScript pay_to_pubkey = CScript() << m_key.GetPubKey() << OP_CHECKSIG;

    const CScript redeem = m_to_us;
    CScript pay_to_script_hash;
    pay_to_script_hash.SetDestination(CScriptID(redeem));
    {
        LOCK(m_wallet.cs_wallet);
        BOOST_REQUIRE(m_wallet.AddCScript(redeem));
    }

    for (const CScript& form : {pay_to_pubkey, pay_to_script_hash}) {
        CMutableTransaction funding_mtx;
        funding_mtx.vin.resize(1);
        funding_mtx.vin[0].prevout = COutPoint(GetRandHash(), 0);
        funding_mtx.vout.emplace_back(m_coin_value, form);
        const CTransaction funding(funding_mtx);
        {
            LOCK(m_wallet.cs_wallet);
            m_wallet.mapWallet[funding.GetHash()] = CWalletTx(&m_wallet, funding);
        }

        CMutableTransaction renewal = Canonical();
        renewal.vin[0].prevout = COutPoint(funding.GetHash(), 0);
        Rebalance(renewal, 1);
        BOOST_CHECK(IsShape(renewal, FeePaid(renewal)));

        CMutableTransaction signed_tx = renewal;
        {
            LOCK(m_wallet.cs_wallet);
            BOOST_REQUIRE(SignSignature(m_wallet, funding, signed_tx, 0));
        }

        const CTransaction final_tx(signed_tx);
        const unsigned int size = ::GetSerializeSize(final_tx, SER_NETWORK, PROTOCOL_VERSION);

        BOOST_CHECK_EQUAL(FeePaid(renewal), GetMinFee(final_tx, 1000, GMF_SEND, size));
        BOOST_CHECK(IsShape(signed_tx, FeePaid(signed_tx)));
    }
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

//! Puts the sync time back as it was, however the case exits. The clock is the
//! chain fixture's, and it only moves forward (test/chain_setup.h).
struct SyncTimeReset
{
    const int64_t m_previous_block_time{g_previous_block_time.load()};

    ~SyncTimeReset() { g_previous_block_time.store(m_previous_block_time); }
};

//! Move the mock clock forward, keeping the node in sync as if blocks had kept
//! arriving. The caller holds a SyncTimeReset.
void AdvanceClock(const int64_t seconds)
{
    SetMockTime(GetAdjustedTime() + seconds);
    g_previous_block_time.store(GetAdjustedTime());
}

//! Mine until no beacon for \p cpid is left in the mempool.
//!
//! The miner leaves out a transaction stamped later than the block
//! (CreateRestOfTheBlock). The chain fixture mines each block in a stake slot
//! after the clock the transactions were stamped from, so none of them is.
void MineUntilBeaconsConfirm(const GRC::Cpid& cpid)
{
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
    const SyncTimeReset reset_sync;
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
    const SyncTimeReset reset_sync;
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
    const SyncTimeReset reset_sync;
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
    const SyncTimeReset reset_sync;
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

    // A retry before it confirms is refused as in the mempool, which is not kept
    // as the beacon error, and not for the balance, which would be, and shown
    // until the next successful send, long after the beacon activated.
    BOOST_CHECK(researcher->AdvertiseBeacon().Error() == GRC::BeaconError::ALEADY_IN_MEMPOOL);
    BOOST_CHECK(researcher->BeaconError() == GRC::BeaconError::NONE);

    AdvanceClock(STALE);

    const GRC::AdvertiseBeaconResult second = researcher->AdvertiseBeacon();
    BOOST_CHECK_MESSAGE(second.TryPublicKey(), "replacement refused: " << static_cast<int>(second.Error()));
    BOOST_CHECK_EQUAL(PooledBeacons(cpid).size(), 1U);
}

//! A send refused for another reason retires nothing: retirement only makes way
//! for an advertisement that is about to go out.
BOOST_FIXTURE_TEST_CASE(a_refused_send_retires_nothing, grc_test::WalletTxScope)
{
    const SyncTimeReset reset_sync;
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

namespace {

//! \brief A researcher's wallet unlocked for staking only, in pwalletMain for
//! the life of the object.
//!
//! It holds the premine key, and so the coins pwalletMain holds, and a beacon
//! key, and is encrypted and unlocked for staking only. It is a wallet of its
//! own, so that pwalletMain, which the other cases stake from, is never
//! encrypted. Nothing may be mined while it is swapped in: the block would
//! reach it and not pwalletMain. It holds no database, so a send from it can be
//! built but not committed.
struct StakingOnlyResearcherWallet
{
    static inline const SecureString PASSPHRASE{"renewal-staking-only"};

    CWallet m_wallet;
    CKey m_beacon_key;
    std::unique_ptr<grc_test::SwapMainWallet> m_swap;

    StakingOnlyResearcherWallet()
    {
        PlantRegtestPremineKey(&m_wallet);

        int mismatch = 0;
        CAmount in_question = 0;
        m_wallet.FixSpentCoins(mismatch, in_question);

        m_beacon_key.MakeNewKey(true);
        {
            LOCK(m_wallet.cs_wallet);
            BOOST_REQUIRE(m_wallet.AddKey(m_beacon_key));
        }

        BOOST_REQUIRE(m_wallet.EncryptWallet(PASSPHRASE));
        m_wallet.Lock();
        BOOST_REQUIRE(m_wallet.Unlock(PASSPHRASE, UnlockScope::StakingOnly));
        BOOST_REQUIRE(m_wallet.IsUnlockedForStakingOnly());
        BOOST_REQUIRE(m_wallet.GetBalance() >= COIN);

        // The general route to the beacon key is closed.
        BOOST_REQUIRE(!GeneralKeyAccess());

        m_swap = std::make_unique<grc_test::SwapMainWallet>(&m_wallet);
    }

    ~StakingOnlyResearcherWallet()
    {
        m_swap.reset();
        m_wallet.Lock();
    }

    StakingOnlyResearcherWallet(const StakingOnlyResearcherWallet&) = delete;
    StakingOnlyResearcherWallet& operator=(const StakingOnlyResearcherWallet&) = delete;

    bool GeneralKeyAccess() const
    {
        CKey key;
        return m_wallet.GetKey(m_beacon_key.GetPubKey().GetID(), key);
    }

    //! What a send, or a new beacon key, would change.
    struct Footprint
    {
        size_t transactions{0};
        size_t address_book{0};
        unsigned int key_pool{0};

        bool operator==(const Footprint& other) const
        {
            return transactions == other.transactions
                && address_book == other.address_book
                && key_pool == other.key_pool;
        }
    };

    Footprint TakeFootprint()
    {
        LOCK(m_wallet.cs_wallet);
        return {m_wallet.mapWallet.size(), m_wallet.mapAddressBook.size(), m_wallet.GetKeyPoolSize()};
    }
};

//! \brief Makes \p key the active beacon for \p cpid, advertised long enough ago
//! to be due for renewal, as a superblock activates a verified advertisement.
struct RenewableBeacon
{
    uint256 m_block_hash;

    RenewableBeacon(const GRC::Cpid& cpid, const CPubKey& key)
    {
        // Half way through the renewal window, which closes when the beacon
        // expires.
        const int64_t advertised = GetAdjustedTime() - GRC::Beacon::RENEWAL_AGE
            - (GRC::Beacon::MAX_AGE - GRC::Beacon::RENEWAL_AGE) / 2;

        LOCK(cs_main);

        CMutableTransaction mtx;
        mtx.nTime = advertised;
        const CTransaction tx(mtx);

        m_block_hash = GetRandHash();
        CBlockIndex* const pindex = GRC::MockBlockIndex::InsertBlockIndex(m_block_hash);
        pindex->nHeight = nBestHeight;
        pindex->nTime = advertised;

        const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(GRC::ContractAction::ADD, cpid, key);

        GRC::BeaconRegistry& registry = GRC::GetBeaconRegistry();
        registry.Add({contract, tx, pindex});
        registry.ActivatePending({key.GetID()}, advertised + 60 * 60, GetRandHash(), nBestHeight);

        const GRC::BeaconOption active = registry.TryActive(cpid, GetAdjustedTime());
        BOOST_REQUIRE(active);
        BOOST_REQUIRE(active->m_public_key == key);
        BOOST_REQUIRE(active->Renewable(GetAdjustedTime()));
    }

    ~RenewableBeacon()
    {
        LOCK(cs_main);
        mapBlockIndex.erase(m_block_hash);
    }

    RenewableBeacon(const RenewableBeacon&) = delete;
    RenewableBeacon& operator=(const RenewableBeacon&) = delete;
};

} // anonymous namespace

//! The automated renewal pays the minimum fee for its size, however high the
//! fee configured for other sends. That fee is configured without unlocking the
//! wallet, and the renewal runs unattended.
//!
//! End to end, from the renewal's access to the pool, on pwalletMain: the
//! wallets these cases encrypt hold no database, and so cannot commit a send.
//! The fee rule does not depend on the unlock; the next case builds the same
//! renewal on a wallet unlocked for staking only.
BOOST_FIXTURE_TEST_CASE(renewal_pays_minimum_fee, grc_test::WalletTxScope)
{
    const SyncTimeReset reset_sync;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "fee0fee0fee0fee0fee0fee0fee0fee0";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);

    CKey beacon_key;
    beacon_key.MakeNewKey(true);
    {
        LOCK(pwalletMain->cs_wallet);
        BOOST_REQUIRE(pwalletMain->AddKey(beacon_key));
    }

    const RenewableBeacon beacon(cpid, beacon_key.GetPubKey());
    ForcedResearcher forced(cpid_hex);
    const grc_test::TransactionFeeGuard configured_fee(COIN / 10);

    const StakingKeyAccess renewal_access = StakingKeyAccessTestPeer::Make();
    const GRC::AdvertiseBeaconResult result =
        GRC::Researcher::Get()->AdvertiseBeacon(/*force=*/false, &renewal_access);

    BOOST_REQUIRE_MESSAGE(result.TryPublicKey(), "renewal failed: " << static_cast<int>(result.Error()));
    BOOST_CHECK(*result.TryPublicKey() == beacon_key.GetPubKey());

    const std::vector<uint256> pooled = PooledBeacons(cpid);
    BOOST_REQUIRE_EQUAL(pooled.size(), 1U);

    std::optional<CMutableTransaction> renewal;
    CAmount fee = 0;
    size_t size = 0;
    {
        LOCK(mempool.cs);
        const CTxMemPoolEntry& entry = mempool.mapTx.at(pooled.front());
        renewal.emplace(entry.GetTx());
        fee = entry.GetFee();
        size = entry.GetTxSize();
    }

    BOOST_CHECK_EQUAL(fee, GetMinFee(CTransaction(*renewal), 1000, GMF_SEND, size));
    BOOST_CHECK_LT(fee, COIN);

    LOCK2(cs_main, pwalletMain->cs_wallet);
    BOOST_CHECK(GRC::IsRenewalShape(*renewal, cpid, COIN / 2, fee, *pwalletMain));
}

//! The same renewal on a researcher's wallet unlocked for staking only: the
//! builder signs it through the staking accessor at the minimum fee, in the
//! fixed shape, and opens nothing else. A transaction outside that shape is
//! refused before anything is signed.
BOOST_FIXTURE_TEST_CASE(renewal_builds_under_staking_only_at_the_minimum_fee, grc_test::WalletTxScope)
{
    const SyncTimeReset reset_sync;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "fee1fee1fee1fee1fee1fee1fee1fee1";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);

    StakingOnlyResearcherWallet staking;
    ForcedResearcher forced(cpid_hex);
    const grc_test::TransactionFeeGuard configured_fee(COIN / 10);

    const StakingKeyAccess renewal_access = StakingKeyAccessTestPeer::Make();
    const StakingOnlyResearcherWallet::Footprint before = staking.TakeFootprint();

    // The renewal's contract, as SendBeaconContract() makes it. The case holds
    // its own copy of the beacon key to sign the payload with.
    GRC::BeaconPayload payload(cpid, GRC::Beacon(staking.m_beacon_key.GetPubKey()));
    BOOST_REQUIRE(payload.Sign(staking.m_beacon_key));

    const auto build = [&](GRC::Contract contract, CWalletTx& wtx, CAmount& fee) {
        CMutableTransaction mtx;
        mtx.vContracts.push_back(std::move(contract));
        static_cast<CTransaction&>(wtx) = CTransaction(std::move(mtx));

        CReserveKey reserve_key(&staking.m_wallet);

        return staking.m_wallet.CreateTransaction(
            {std::make_pair(CScript() << OP_RETURN, COIN / 2)}, wtx, reserve_key, fee,
            /*coinControl=*/nullptr, /*change_back_to_input_address=*/false, &renewal_access);
    };

    CWalletTx renewal;
    CAmount fee = 0;
    BOOST_REQUIRE(build(GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, payload), renewal, fee));

    const unsigned int size = ::GetSerializeSize(static_cast<const CTransaction&>(renewal), SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_EQUAL(fee, GetMinFee(renewal, 1000, GMF_SEND, size));
    BOOST_CHECK_LT(fee, COIN);

    {
        LOCK2(cs_main, staking.m_wallet.cs_wallet);

        BOOST_CHECK(GRC::IsRenewalShape(CMutableTransaction(renewal), cpid, COIN / 2, fee, staking.m_wallet));

        // Every input carries a signature that spends the coin it names.
        BOOST_REQUIRE(!renewal.vin.empty());

        for (unsigned int i = 0; i < renewal.vin.size(); ++i) {
            const auto prev = staking.m_wallet.mapWallet.find(renewal.vin[i].prevout.hash);
            BOOST_REQUIRE(prev != staking.m_wallet.mapWallet.end());
            BOOST_CHECK(VerifySignature(prev->second, renewal, SCRIPT_VERIFY_P2SH, i, 0));
        }
    }

    // Built without drawing a key, and nothing opened: the wallet is still
    // unlocked for staking only, and the general route to the key is closed.
    BOOST_CHECK(staking.TakeFootprint() == before);
    BOOST_CHECK(staking.m_wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!staking.GeneralKeyAccess());

    // Any other contract with the same access is refused, unsigned.
    CWalletTx message;
    CAmount message_fee = 0;
    BOOST_CHECK(!build(GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "not a renewal"), message, message_fee));
    BOOST_CHECK(message.vin.empty());
}

//! The renewal never takes its fee out of an output. With the renewal's access
//! CreateTransaction ignores a "subtract fee from amount" set: the burn stays
//! whole, the minimum fee is paid on top of it, and the transaction is the one
//! built without the set. Index 0 is the burn; index 1 is past the outputs,
//! which a build without the access refuses.
BOOST_FIXTURE_TEST_CASE(renewal_ignores_subtract_fee_outputs, grc_test::WalletTxScope)
{
    const SyncTimeReset reset_sync;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "fee3fee3fee3fee3fee3fee3fee3fee3";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);

    StakingOnlyResearcherWallet staking;
    ForcedResearcher forced(cpid_hex);

    const StakingKeyAccess renewal_access = StakingKeyAccessTestPeer::Make();

    GRC::BeaconPayload payload(cpid, GRC::Beacon(staking.m_beacon_key.GetPubKey()));
    BOOST_REQUIRE(payload.Sign(staking.m_beacon_key));

    const CScript burn_script = CScript() << OP_RETURN;

    const auto build = [&](const std::set<size_t>& subtract_fee_outputs, CWalletTx& wtx, CAmount& fee) {
        CMutableTransaction mtx;
        mtx.vContracts.push_back(GRC::MakeContract<GRC::BeaconPayload>(3, GRC::ContractAction::ADD, payload));
        static_cast<CTransaction&>(wtx) = CTransaction(std::move(mtx));

        CReserveKey reserve_key(&staking.m_wallet);

        return staking.m_wallet.CreateTransaction(
            {std::make_pair(burn_script, COIN / 2)}, wtx, reserve_key, fee,
            /*coinControl=*/nullptr, /*change_back_to_input_address=*/false, &renewal_access,
            subtract_fee_outputs);
    };

    const auto burned = [&](const CTransaction& tx) {
        CAmount total = 0;

        for (const CTxOut& txout : tx.vout) {
            if (txout.scriptPubKey == burn_script) total += txout.nValue;
        }

        return total;
    };

    CWalletTx plain;
    CAmount plain_fee = 0;
    BOOST_REQUIRE(build({}, plain, plain_fee));

    for (const std::set<size_t>& subtract_fee_outputs : {std::set<size_t>{0}, std::set<size_t>{1}}) {
        CWalletTx renewal;
        CAmount fee = 0;
        BOOST_REQUIRE(build(subtract_fee_outputs, renewal, fee));

        const unsigned int size = ::GetSerializeSize(static_cast<const CTransaction&>(renewal), SER_NETWORK, PROTOCOL_VERSION);
        BOOST_CHECK_EQUAL(fee, GetMinFee(renewal, 1000, GMF_SEND, size));
        BOOST_CHECK_EQUAL(fee, plain_fee);
        BOOST_CHECK_EQUAL(burned(renewal), COIN / 2);
        BOOST_CHECK_EQUAL(renewal.GetValueOut(), plain.GetValueOut());

        LOCK2(cs_main, staking.m_wallet.cs_wallet);
        BOOST_CHECK(GRC::IsRenewalShape(CMutableTransaction(renewal), cpid, COIN / 2, fee, staking.m_wallet));
    }
}

//! The renewal's access reaches the beacon key on a wallet unlocked for staking
//! only, where the general route refuses: the renewal checks it holds the key,
//! and signs the payload with it.
//!
//! The send stops after the transaction is built, at the pool's pending
//! mandatory sidestake, as any contract send does: this wallet holds no
//! database, and so cannot commit it. Refused as TX_FAILED there, not as
//! MISSING_KEY, which is what reaching the key the general way returns.
BOOST_FIXTURE_TEST_CASE(renewal_signs_its_payload_under_staking_only, grc_test::WalletTxScope)
{
    const SyncTimeReset reset_sync;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "fee2fee2fee2fee2fee2fee2fee2fee2";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);

    StakingOnlyResearcherWallet staking;
    const RenewableBeacon beacon(cpid, staking.m_beacon_key.GetPubKey());
    ForcedResearcher forced(cpid_hex);

    // A mandatory sidestake transaction waiting in the pool. Unvalidated: only
    // its presence matters.
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(GetRandHash(), 0);
        mtx.vContracts.push_back(GRC::MakeContract<GRC::SideStakePayload>(
            GRC::ContractAction::ADD, GRC::SideStakePayload()));
        const CTransaction sidestake(mtx);

        LOCK(cs_main);
        mempool.addUnchecked(sidestake.GetHash(), CTxMemPoolEntry(
            sidestake, /*fee=*/0, /*time=*/0, /*height=*/0,
            ::GetSerializeSize(sidestake, SER_NETWORK, PROTOCOL_VERSION)));
    }
    BOOST_REQUIRE(mempool.HasMandatorySidestake());

    const StakingKeyAccess renewal_access = StakingKeyAccessTestPeer::Make();
    BOOST_CHECK(GRC::Researcher::Get()->AdvertiseBeacon(/*force=*/false, &renewal_access).Error()
                == GRC::BeaconError::TX_FAILED);

    BOOST_CHECK(PooledBeacons(cpid).empty());
    BOOST_CHECK(staking.m_wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!staking.GeneralKeyAccess());
}

//! On a wallet unlocked for staking only, an advertisement without the
//! renewal's access -- the advertisebeacon RPC's or the GUI's -- is refused as
//! locked before anything is built: no transaction, and no beacon key drawn.
BOOST_FIXTURE_TEST_CASE(ordinary_contract_refused_under_staking_only, grc_test::WalletTxScope)
{
    const SyncTimeReset reset_sync;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "0dd00dd00dd00dd00dd00dd00dd00dd0";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);

    StakingOnlyResearcherWallet staking;
    const RenewableBeacon beacon(cpid, staking.m_beacon_key.GetPubKey());
    ForcedResearcher forced(cpid_hex);

    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
    const StakingOnlyResearcherWallet::Footprint before = staking.TakeFootprint();

    // A new beacon.
    BOOST_CHECK(researcher->AdvertiseBeacon(/*force=*/true, /*renewal_access=*/nullptr).Error()
                == GRC::BeaconError::WALLET_LOCKED);

    // The renewal, asked for by hand.
    BOOST_CHECK(researcher->AdvertiseBeacon(/*force=*/false, /*renewal_access=*/nullptr).Error()
                == GRC::BeaconError::WALLET_LOCKED);

    BOOST_CHECK(PooledBeacons(cpid).empty());
    BOOST_CHECK(staking.TakeFootprint() == before);
}

//! The renewal's access renews a beacon and does nothing else. With no beacon
//! to renew, the job would advertise a new one, which takes a new key; on a
//! wallet unlocked for staking only that is refused as locked, before any key
//! is drawn.
BOOST_FIXTURE_TEST_CASE(renewal_access_does_not_advertise_a_new_beacon, grc_test::WalletTxScope)
{
    const SyncTimeReset reset_sync;
    const PoolCleared pool;
    MineUntilReadyToAdvertise();

    const std::string cpid_hex = "0e110e110e110e110e110e110e110e11";
    const GRC::Cpid cpid = GRC::Cpid::Parse(cpid_hex);

    StakingOnlyResearcherWallet staking;
    ForcedResearcher forced(cpid_hex);

    const StakingOnlyResearcherWallet::Footprint before = staking.TakeFootprint();

    const StakingKeyAccess renewal_access = StakingKeyAccessTestPeer::Make();
    BOOST_CHECK(GRC::Researcher::Get()->AdvertiseBeacon(/*force=*/false, &renewal_access).Error()
                == GRC::BeaconError::WALLET_LOCKED);

    BOOST_CHECK(PooledBeacons(cpid).empty());
    BOOST_CHECK(staking.TakeFootprint() == before);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END() // beacon_advertisement_tests
