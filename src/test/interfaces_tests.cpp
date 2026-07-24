// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chain.h"
#include "clientversion.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/node.h"
#include "interfaces/staking.h"
#include "interfaces/wallet.h"
#include "key.h"
#include "key_io.h"
#include "node/ui_interface.h"
#include "gridcoin/scraper/fwd.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "txmempool.h"
#include "util.h"
#include "wallet/wallet.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

extern CWallet* pwalletMain;

BOOST_AUTO_TEST_SUITE(interfaces_tests)

BOOST_AUTO_TEST_CASE(signal_handler_disconnects)
{
    boost::signals2::signal<void()> sig;
    int calls = 0;

    std::unique_ptr<interfaces::Handler> handler =
        interfaces::MakeSignalHandler(sig.connect([&] { ++calls; }));

    sig();
    BOOST_CHECK_EQUAL(calls, 1);

    handler->disconnect();
    sig();
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(signal_handler_disconnects_on_destruction)
{
    boost::signals2::signal<void()> sig;
    int calls = 0;

    {
        std::unique_ptr<interfaces::Handler> handler =
            interfaces::MakeSignalHandler(sig.connect([&] { ++calls; }));
        sig();
    }

    sig();
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(cleanup_handler_runs_exactly_once)
{
    int cleanups = 0;

    {
        std::unique_ptr<interfaces::Handler> handler =
            interfaces::MakeCleanupHandler([&] { ++cleanups; });
        handler->disconnect();
        BOOST_CHECK_EQUAL(cleanups, 1);
        // Destruction after an explicit disconnect must not run it again.
    }

    BOOST_CHECK_EQUAL(cleanups, 1);
}

BOOST_AUTO_TEST_CASE(cleanup_handler_runs_on_destruction)
{
    int cleanups = 0;

    {
        std::unique_ptr<interfaces::Handler> handler =
            interfaces::MakeCleanupHandler([&] { ++cleanups; });
        // No explicit disconnect: destruction alone must run the cleanup.
    }

    BOOST_CHECK_EQUAL(cleanups, 1);
}

BOOST_AUTO_TEST_CASE(node_wraps_chain_globals)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    BOOST_CHECK_EQUAL(node->getNumBlocks(), WITH_LOCK(cs_main, return nBestHeight));
    BOOST_CHECK(node->getBestBlockHash() == WITH_LOCK(cs_main, return hashBestChain));
    BOOST_CHECK_EQUAL(node->isTestNet(), OnTestnet());
    BOOST_CHECK_EQUAL(node->getClientVersion(), FormatFullVersion());
    // No peers in the unit-test environment.
    BOOST_CHECK_EQUAL(node->getNodeCount(), 0);
}

BOOST_AUTO_TEST_CASE(node_handler_bridges_ui_signal)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();
    int calls = 0;

    std::unique_ptr<interfaces::Handler> handler =
        node->handleBannedListChanged([&] { ++calls; });

    uiInterface.BannedListChanged();
    BOOST_CHECK_EQUAL(calls, 1);

    handler->disconnect();
    uiInterface.BannedListChanged();
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(node_init_shutdown_handler_bridges_queue_shutdown)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();
    int calls = 0;

    std::unique_ptr<interfaces::Handler> handler =
        node->handleInitShutdown([&] { ++calls; });

    uiInterface.QueueShutdown();
    BOOST_CHECK_EQUAL(calls, 1);

    handler->disconnect();
    uiInterface.QueueShutdown();
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(wallet_wraps_cwallet)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    BOOST_CHECK_EQUAL(wallet->getBalance(), pwalletMain->GetBalance());

    const interfaces::WalletLockState lock_state = wallet->getLockState();
    BOOST_CHECK_EQUAL(lock_state.crypted, pwalletMain->IsCrypted());
    BOOST_CHECK_EQUAL(lock_state.locked, pwalletMain->IsLocked());
    BOOST_CHECK_EQUAL(lock_state.unlocked_for_staking_only, wallet->isUnlockedForStakingOnly());
    BOOST_CHECK_EQUAL(lock_state.staking_only_flag, wallet->getUnlockStakingOnlyFlag());

    int calls = 0;
    uint256 seen_hash;
    std::unique_ptr<interfaces::Handler> handler = wallet->handleTransactionChanged(
        [&](const uint256& tx_hash, ChangeType status) {
            ++calls;
            seen_hash = tx_hash;
            BOOST_CHECK(status == CT_NEW);
        });

    const uint256 probe = uint256S("0xdeadbeef");
    pwalletMain->NotifyTransactionChanged(pwalletMain, probe, CT_NEW);
    BOOST_CHECK_EQUAL(calls, 1);
    BOOST_CHECK(seen_hash == probe);

    handler->disconnect();
    pwalletMain->NotifyTransactionChanged(pwalletMain, probe, CT_NEW);
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(wallet_query_surface_wraps_cwallet)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    BOOST_CHECK_EQUAL(wallet->getNumTransactions(),
                      static_cast<int>(WITH_LOCK(pwalletMain->cs_wallet,
                                                 return pwalletMain->mapWallet.size())));

    // No contention in the unit-test environment, so the try-variant must
    // succeed and agree with the unconditional single getters.
    interfaces::WalletBalances balances;
    BOOST_REQUIRE(wallet->tryGetBalances(balances));
    BOOST_CHECK_EQUAL(balances.balance, pwalletMain->GetBalance());
    BOOST_CHECK_EQUAL(balances.stake, pwalletMain->GetStake());
    BOOST_CHECK_EQUAL(balances.unconfirmed_balance, pwalletMain->GetUnconfirmedBalance());
    BOOST_CHECK_EQUAL(balances.immature_balance, pwalletMain->GetImmatureBalance());

    // Staking-only preference: the raw flag getter tracks the global; the
    // composite requires the wallet to actually be unlocked.
    const bool saved_flag = fWalletUnlockStakingOnly;
    fWalletUnlockStakingOnly = true;
    BOOST_CHECK(wallet->getUnlockStakingOnlyFlag());
    BOOST_CHECK_EQUAL(wallet->isUnlockedForStakingOnly(), !pwalletMain->IsLocked());
    fWalletUnlockStakingOnly = false;
    BOOST_CHECK(!wallet->getUnlockStakingOnlyFlag());
    BOOST_CHECK(!wallet->isUnlockedForStakingOnly());

    // The test wallet is unencrypted: Unlock must fail, and a failed unlock
    // must NOT overwrite the staking-only preference.
    BOOST_CHECK(!wallet->unlockWallet(SecureString("passphrase"), true));
    BOOST_CHECK(!wallet->getUnlockStakingOnlyFlag());
    fWalletUnlockStakingOnly = saved_flag;

    // Unknown outpoints are skipped; the coin queries return empty value
    // containers rather than touching wallet internals.
    const std::vector<COutPoint> unknown{COutPoint(uint256S("0xdeadbeef"), 0)};
    BOOST_CHECK(wallet->getOutputs(unknown).empty());
    BOOST_CHECK(wallet->getOutputs({}).empty());
}

BOOST_AUTO_TEST_CASE(wallet_send_coins_boundary_guards)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    // An empty recipient list must fail cleanly rather than index
    // recipients[0] (the GUI pre-checks this; a direct caller must not UB).
    interfaces::SendCoinsResult result = wallet->sendCoins({}, std::nullopt, 0);
    BOOST_CHECK(result.status == interfaces::SendCoinsStatus::TransactionCreationFailed);

    // An undecodable address must be rejected node-side — it would
    // otherwise become an empty (anyone-can-spend) output script.
    interfaces::WalletSendRecipient bad_recipient;
    bad_recipient.address = "not-a-valid-address";
    bad_recipient.amount = 1;
    result = wallet->sendCoins({bad_recipient}, std::nullopt, 0);
    BOOST_CHECK(result.status == interfaces::SendCoinsStatus::InvalidAddress);

    // With a well-formed address, the empty test wallet has no spendable
    // coins, so any positive send must fail the node-side balance pre-check
    // without creating anything.
    interfaces::WalletSendRecipient recipient;
    recipient.address = EncodeDestination(CKeyID());
    recipient.amount = 1;
    result = wallet->sendCoins({recipient}, std::nullopt, 0);
    BOOST_CHECK(result.status == interfaces::SendCoinsStatus::AmountExceedsBalance);
    BOOST_CHECK(result.txid_hex.empty());
}

BOOST_AUTO_TEST_CASE(wallet_key_from_pool_labels_address_book)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    CPubKey pub_key;
    const std::string label = "interfaces-test-label";
    BOOST_REQUIRE(wallet->getKeyFromPool(label, pub_key));
    BOOST_CHECK(pub_key.IsValid());

    LOCK(pwalletMain->cs_wallet);
    auto it = pwalletMain->mapAddressBook.find(pub_key.GetID());
    BOOST_REQUIRE(it != pwalletMain->mapAddressBook.end());
    BOOST_CHECK_EQUAL(it->second.name, label);
}

BOOST_AUTO_TEST_CASE(wallet_address_book_handler_bridges_value_types)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    int calls = 0;
    std::string seen_address, seen_label, seen_purpose;
    ChangeType seen_status = CT_DELETED;
    std::unique_ptr<interfaces::Handler> handler = wallet->handleAddressBookChanged(
        [&](const std::string& address, const std::string& label, bool is_mine,
            const std::string& purpose, ChangeType status) {
            ++calls;
            seen_address = address;
            seen_label = label;
            seen_purpose = purpose;
            seen_status = status;
            BOOST_CHECK(is_mine);
        });

    const CTxDestination dest = CKeyID();
    pwalletMain->NotifyAddressBookChanged(pwalletMain, dest, "label", true, "purpose", CT_NEW);
    BOOST_CHECK_EQUAL(calls, 1);
    BOOST_CHECK_EQUAL(seen_address, EncodeDestination(dest));
    BOOST_CHECK_EQUAL(seen_label, "label");
    BOOST_CHECK_EQUAL(seen_purpose, "purpose");
    BOOST_CHECK(seen_status == CT_NEW);

    handler->disconnect();
    pwalletMain->NotifyAddressBookChanged(pwalletMain, dest, "label", true, "purpose", CT_NEW);
    BOOST_CHECK_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(wallet_address_book_query_surface)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    // Reserve a fresh owned address; the private key stays node-side and only
    // the encoded destination comes back.
    std::string address;
    BOOST_REQUIRE(wallet->getNewReceiveAddress(address));
    BOOST_CHECK(!address.empty());
    BOOST_CHECK(IsValidDestination(DecodeDestination(address)));

    // It is ours (from the key pool) but not yet in the address book.
    BOOST_CHECK(wallet->isMine(address));
    std::string label;
    BOOST_CHECK(!wallet->getAddressLabel(address, label));

    // Book it: the label round-trips and it shows up in the full snapshot as
    // an owned (receiving) entry.
    wallet->setAddressBook(address, "interfaces-book-label");
    BOOST_REQUIRE(wallet->getAddressLabel(address, label));
    BOOST_CHECK_EQUAL(label, "interfaces-book-label");

    bool found_in_snapshot = false;
    for (const interfaces::WalletAddress& entry : wallet->getAddresses()) {
        if (entry.address == address) {
            found_in_snapshot = true;
            BOOST_CHECK(entry.is_mine);
            BOOST_CHECK_EQUAL(entry.label, "interfaces-book-label");
        }
    }
    BOOST_CHECK(found_in_snapshot);

    // Removing it drops it from the book. The bool return depends on the wallet
    // being file-backed (the unit-test wallet is not), so assert via the book
    // rather than the return value.
    wallet->delAddressBook(address);
    BOOST_CHECK(!wallet->getAddressLabel(address, label));

    // Failure cases: an unparseable address is not mine, has no label, and
    // cannot be removed.
    const std::string bogus = "not-a-valid-address";
    BOOST_CHECK(!wallet->isMine(bogus));
    BOOST_CHECK(!wallet->getAddressLabel(bogus, label));
    BOOST_CHECK(!wallet->delAddressBook(bogus));

    // getUnbookedReceiveAddresses is callable and yields well-formed encoded
    // strings (contents depend on balances the unit-test wallet lacks).
    for (const std::string& unbooked : wallet->getUnbookedReceiveAddresses()) {
        BOOST_CHECK(!unbooked.empty());
    }
}

BOOST_AUTO_TEST_CASE(node_value_queries_are_safe_in_empty_environment)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    // BanMan exists via the global test fixture and is shared across every
    // suite in the process, so do not assume emptiness: assert the query is
    // safe and any rows are well-formed value types.
    for (const auto& banned : node->getBanned()) {
        BOOST_CHECK(!banned.address.empty());
    }

    // No alerts in the unit-test environment: unknown hash yields empty.
    BOOST_CHECK(node->getAlertStatusBarMessage(uint256S("0x01")).empty());

    // The scraper convergence cache is process-global: assert the snapshot
    // query is safe and the value data is well-formed without assuming the
    // cache is empty (no scraper threads run under the unit tests today,
    // but suite order must not matter).
    const interfaces::ScraperConvergenceSnapshot snapshot = node->getScraperConvergenceSnapshot();
    BOOST_CHECK(snapshot.time >= 0);
    for (const auto& scraper : snapshot.included_scrapers) {
        BOOST_CHECK(!scraper.empty());
    }

    // Pure conversion: the difficulty-1 compact target maps to ~1.0.
    BOOST_CHECK_CLOSE(node->getBlockDifficulty(0x1d00ffff), 1.0, 0.1);
}

BOOST_AUTO_TEST_CASE(node_caches_last_scraper_event_for_icon_hydration)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    // A non-Log scraper event is remembered and surfaces on the convergence
    // snapshot, so a GUI that connects after the event fired (e.g. the
    // -multiprocess GUI attaching to an already-running node) can initialize its
    // status icon to the current state instead of a stale default. Each check
    // follows an explicit emission, so suite order does not matter.
    uiInterface.NotifyScraperEvent(scrapereventtypes::Convergence, CT_NEW, {});
    {
        const interfaces::ScraperConvergenceSnapshot snapshot = node->getScraperConvergenceSnapshot();
        BOOST_CHECK_EQUAL(snapshot.current_event_type, (int)scrapereventtypes::Convergence);
        BOOST_CHECK_EQUAL(snapshot.current_event_status, (int)CT_NEW);
    }

    // Log events carry console text rather than an icon state and must not
    // overwrite the cached status.
    uiInterface.NotifyScraperEvent(scrapereventtypes::Log, CT_NEW, "a scraper log line");
    {
        const interfaces::ScraperConvergenceSnapshot snapshot = node->getScraperConvergenceSnapshot();
        BOOST_CHECK_EQUAL(snapshot.current_event_type, (int)scrapereventtypes::Convergence);
        BOOST_CHECK_EQUAL(snapshot.current_event_status, (int)CT_NEW);
    }

    // A later non-Log event replaces the cached status.
    uiInterface.NotifyScraperEvent(scrapereventtypes::Sleep, CT_UPDATED, {});
    {
        const interfaces::ScraperConvergenceSnapshot snapshot = node->getScraperConvergenceSnapshot();
        BOOST_CHECK_EQUAL(snapshot.current_event_type, (int)scrapereventtypes::Sleep);
        BOOST_CHECK_EQUAL(snapshot.current_event_status, (int)CT_UPDATED);
    }
}

BOOST_AUTO_TEST_CASE(staking_status_wraps_miner_status)
{
    std::unique_ptr<interfaces::StakingStatus> staking = interfaces::MakeStakingStatus();

    // Not staking in the unit-test environment, and the error summary is
    // callable. The network-weight/ETTS queries are deliberately NOT
    // exercised here: they walk real chain state from pindexBest, and
    // earlier suites leave sparse/foreign block indexes in the global chain
    // state (the same pre-existing hazard project_tests' TestStateGuard
    // works around) -- a suite-ordering trap, not an interface property.
    BOOST_CHECK(!staking->isStaking());
    BOOST_CHECK_NO_THROW(staking->getErrors());
}

BOOST_AUTO_TEST_CASE(init_hands_out_interfaces)
{
    std::unique_ptr<interfaces::Init> init = interfaces::MakeGridcoinInit();

    BOOST_REQUIRE(init != nullptr);
    BOOST_CHECK(init->makeNode() != nullptr);
    BOOST_CHECK(init->makeStakingStatus() != nullptr);
    // pwalletMain exists in the unit-test environment, so the monolithic
    // Init must hand out a wallet interface for it.
    BOOST_CHECK(init->makeWallet() != nullptr);
}

namespace {
CKey NewKey(bool compressed)
{
    CKey key;
    key.MakeNewKey(compressed);
    return key;
}

//! One transaction carrying every output shape computeCoinControlSummary's byte
//! estimate distinguishes, plus a tiny output for the after_fee floor case.
//!
//! Outputs (value, expected input bytes):
//!   0: 1 COIN, P2PKH to a compressed key in the wallet    -> 148
//!   1: 2 COIN, P2PKH to an uncompressed key in the wallet -> 180
//!   2: 3 COIN, P2PKH to a key the wallet does not hold    -> 148 (GetPubKey fails)
//!   3: 4 COIN, P2SH                                       -> 148 (destination is not a CKeyID)
//!   4: 5 COIN, OP_RETURN                                  -> 148 (ExtractDestination fails)
//!   5: 1000,   P2PKH to the compressed key                -> 148
CTransaction MakeFundingTx(const CKey& compressed, const CKey& uncompressed, const CKey& foreign)
{
    CMutableTransaction mtx;
    mtx.vout.resize(6);

    mtx.vout[0].nValue = 1 * COIN;
    mtx.vout[0].scriptPubKey.SetDestination(compressed.GetPubKey().GetID());
    mtx.vout[1].nValue = 2 * COIN;
    mtx.vout[1].scriptPubKey.SetDestination(uncompressed.GetPubKey().GetID());
    mtx.vout[2].nValue = 3 * COIN;
    mtx.vout[2].scriptPubKey.SetDestination(foreign.GetPubKey().GetID());
    mtx.vout[3].nValue = 4 * COIN;
    mtx.vout[3].scriptPubKey.SetDestination(CScriptID(CScript() << OP_TRUE));
    mtx.vout[4].nValue = 5 * COIN;
    mtx.vout[4].scriptPubKey = CScript() << OP_RETURN;
    mtx.vout[5].nValue = 1000;
    mtx.vout[5].scriptPubKey.SetDestination(compressed.GetPubKey().GetID());

    return CTransaction(mtx);
}

//! A second wallet transaction, deliberately never placed in the mempool. Its
//! distinct value keeps its txid off the funding transaction's (see the txid
//! aliasing note on CoinControlSetup).
CTransaction MakeOrphanTx(const CKey& compressed)
{
    CMutableTransaction mtx;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 7 * COIN;
    mtx.vout[0].scriptPubKey.SetDestination(compressed.GetPubKey().GetID());

    return CTransaction(mtx);
}

//! Fixture for the computeCoinControlSummary cases.
//!
//! The implementation skips any selected outpoint whose GetDepthInMainChain()
//! is < 0, and depth 0 is the only non-negative depth reachable here: the
//! global test fixture builds no chain and leaves pindexBest null, so the
//! TxStateConfirmed arm of CWalletTx::GetDepthInMainChainINTERNAL() -- which
//! dereferences pindexBest -- must never be taken. Do not extend these cases to
//! confirmed transactions without building a mock block index first.
//!
//! Mempool residence, not the wallet transaction state, is what lifts the depth
//! off -1: CMerkleTx::GetDepthInMainChain() demotes 0 to -1 only when
//! !mempool.exists(). The seeding below is therefore load-bearing for every
//! case that expects quantity > 0, and TxStateInMempool is set for realism
//! rather than as the mechanism.
//!
//! One multi-output funding transaction covers every byte-sizing branch, the
//! out-of-range-n skip and the multi-input cases with a single mapWallet entry.
//! Separate synthetic transactions would be a hazard: CMutableTransaction::nTime
//! defaults to GetAdjustedTime() and is inside the hash preimage, so two built
//! in the same second with the same outputs share a txid, which would silently
//! collapse two selected "inputs" into one.
struct CoinControlSetup
{
    CWallet test_wallet;
    CKey key_compressed{NewKey(true)};
    CKey key_uncompressed{NewKey(false)};
    //! Never added to the wallet: exercises the GetPubKey-fails byte fallback.
    CKey key_foreign{NewKey(true)};
    CTransaction funding{MakeFundingTx(key_compressed, key_uncompressed, key_foreign)};
    CTransaction orphan{MakeOrphanTx(key_compressed)};
    const int64_t saved_transaction_fee{nTransactionFee};

    CoinControlSetup()
    {
        // An abort() in an earlier case unwinds no destructors, so start from a
        // known-empty pool rather than trusting the previous case's cleanup.
        mempool.clear();

        // Every fee expectation below is anchored to this value, so pin it
        // rather than inheriting whatever the process left behind.
        nTransactionFee = MIN_TX_FEE * 10;

        BOOST_REQUIRE(funding.GetHash() != orphan.GetHash());

        {
            LOCK2(cs_main, test_wallet.cs_wallet);

            BOOST_REQUIRE(test_wallet.AddKey(key_compressed));
            BOOST_REQUIRE(test_wallet.AddKey(key_uncompressed));

            CWalletTx funded(&test_wallet, funding);
            funded.SetTxState(TxStateInMempool{});
            test_wallet.mapWallet[funding.GetHash()] = funded;

            // Left in the default (unrecognized) state and out of the mempool:
            // GetDepthInMainChain() reports -1, the skip the selection loop
            // applies to vanished and conflicted coins.
            CWalletTx orphaned(&test_wallet, orphan);
            test_wallet.mapWallet[orphan.GetHash()] = orphaned;

            // addUnchecked does no internal locking (unlike remove/clear/exists):
            // the caller must hold mempool.cs, acquired last per the canonical
            // cs_main -> cs_wallet -> subsystem order.
            LOCK(mempool.cs);
            mempool.addUnchecked(funding.GetHash(), CTxMemPoolEntry(
                funding, /*fee=*/0, /*time=*/0, /*height=*/0,
                ::GetSerializeSize(funding, SER_NETWORK, PROTOCOL_VERSION)));
        }
    }

    ~CoinControlSetup()
    {
        nTransactionFee = saved_transaction_fee;

        LOCK(cs_main);
        mempool.remove(funding);
    }

    //! The wallet locks are released before every call: computeCoinControlSummary
    //! takes cs_main then cs_wallet itself, and holding cs_wallet across it would
    //! invert the canonical order.
    interfaces::CoinControlSummary Compute(const interfaces::WalletCoinControl& selection,
                                           const std::vector<int64_t>& recipient_amounts,
                                           bool subtract_fee_from_amount)
    {
        return interfaces::MakeWallet(&test_wallet)
            ->computeCoinControlSummary(selection, recipient_amounts, subtract_fee_from_amount);
    }

    interfaces::WalletCoinControl SelectFunded(const std::vector<unsigned int>& indexes)
    {
        interfaces::WalletCoinControl selection;
        for (const unsigned int n : indexes) {
            selection.Select(COutPoint(funding.GetHash(), n));
        }

        return selection;
    }
};

//! The base fee for every case below: nTransactionFee (pinned by the fixture)
//! and GetMinFee(GMF_SEND) both scale MIN_TX_FEE * 10 by (1 + bytes / 1000), and
//! no case exceeds 1000 bytes.
const int64_t BASE_FEE = MIN_TX_FEE * 10;
} // namespace

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_empty_selection, CoinControlSetup)
{
    interfaces::WalletCoinControl selection;

    // Nothing selected and nothing to pay: every field keeps its default.
    interfaces::CoinControlSummary summary = Compute(selection, {}, false);
    BOOST_CHECK_EQUAL(summary.quantity, 0);
    BOOST_CHECK_EQUAL(summary.amount, 0);
    BOOST_CHECK_EQUAL(summary.fee, 0);
    BOOST_CHECK_EQUAL(summary.after_fee, 0);
    BOOST_CHECK_EQUAL(summary.bytes, 0u);
    BOOST_CHECK_EQUAL(summary.change, 0);
    BOOST_CHECK(!summary.low_output);
    // dust is never written on any path: it mirrors a pre-migration flag that
    // was declared but never set true (see interfaces/wallet.h).
    BOOST_CHECK(!summary.dust);

    // The recipient scan runs before the quantity > 0 gate, so a sub-CENT
    // recipient raises low_output even with nothing selected.
    summary = Compute(selection, {CENT / 2}, false);
    BOOST_CHECK(summary.low_output);
    BOOST_CHECK_EQUAL(summary.quantity, 0);
    BOOST_CHECK_EQUAL(summary.bytes, 0u);
    BOOST_CHECK_EQUAL(summary.fee, 0);
    BOOST_CHECK_EQUAL(summary.change, 0);
    BOOST_CHECK_EQUAL(summary.after_fee, 0);
}

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_sizes_every_input_shape, CoinControlSetup)
{
    // No recipients: pay_amount stays 0, so the change/byte adjustment block is
    // skipped entirely and bytes is the unadjusted estimate. This is also the
    // state the send dialog starts in, before any amount is typed.
    const interfaces::CoinControlSummary summary = Compute(SelectFunded({0, 1, 2, 3, 4}), {}, false);

    BOOST_CHECK_EQUAL(summary.quantity, 5);
    BOOST_CHECK_EQUAL(summary.amount, 15 * COIN);

    // 148 (compressed) + 180 (uncompressed) + 148 (key not ours) + 148 (P2SH)
    // + 148 (no destination) = 772 input bytes, + 2 * 34 for the empty-recipient
    // arm of the output term, + 10 fixed. The 34 for the change output is NOT
    // subtracted here: that only happens when pay_amount > 0 leaves change at 0.
    BOOST_CHECK_EQUAL(summary.bytes, 850u);

    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(summary.after_fee, 15 * COIN - BASE_FEE);
    BOOST_CHECK_EQUAL(summary.change, 0);
    BOOST_CHECK(!summary.low_output);

    // Outputs 2-4 pay destinations the wallet does not own, and the selection
    // loop applies no IsMine, spent or maturity filter -- those live upstream in
    // listCoins/AvailableCoins. The summary counts whatever it is handed.
    const interfaces::CoinControlSummary unowned = Compute(SelectFunded({2}), {}, false);
    BOOST_CHECK_EQUAL(unowned.quantity, 1);
    BOOST_CHECK_EQUAL(unowned.amount, 3 * COIN);
}

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_fee_takes_larger_of_configured_and_minimum, CoinControlSetup)
{
    const interfaces::WalletCoinControl selection = SelectFunded({0, 1, 2, 3, 4});

    // The two arms of the max() are equal under the default configured fee, so
    // move it to prove each one can win. The fixture restores the global.
    nTransactionFee = 0;
    interfaces::CoinControlSummary summary = Compute(selection, {}, false);
    BOOST_CHECK_EQUAL(summary.bytes, 850u);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE); // the GetMinFee floor wins

    nTransactionFee = 5 * CENT;
    summary = Compute(selection, {}, false);
    BOOST_CHECK_EQUAL(summary.fee, 5 * CENT); // the configured fee wins
    BOOST_CHECK_EQUAL(summary.after_fee, 15 * COIN - 5 * CENT);
}

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_absorbs_subcent_change, CoinControlSetup)
{
    // One compressed input: 148 + (1 + 1) * 34 + 10 = 226 bytes before the
    // change-output adjustment, 192 after.
    const interfaces::WalletCoinControl selection = SelectFunded({0});
    const int64_t input = 1 * COIN;

    // Sub-CENT change is folded into the fee rather than paid out.
    int64_t pay = input - BASE_FEE - 500000;
    interfaces::CoinControlSummary summary = Compute(selection, {pay}, false);
    BOOST_CHECK_EQUAL(summary.amount, input);
    BOOST_CHECK_EQUAL(summary.change, 0);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE + 500000);
    BOOST_CHECK_EQUAL(summary.bytes, 192u);
    // The migration replaced the pre-migration `nPayFee = nChange` with
    // `fee += change`. These two invariants are what distinguish them: the old
    // form dropped the already-computed base fee, breaking conservation and
    // short-changing the recipients by that amount.
    BOOST_CHECK_EQUAL(summary.fee + pay + summary.change, summary.amount);
    BOOST_CHECK_EQUAL(summary.after_fee, pay);

    // Boundary: exactly CENT of change is not sub-CENT, so it is paid out and
    // the change output stays in the byte estimate.
    pay = input - BASE_FEE - CENT;
    summary = Compute(selection, {pay}, false);
    BOOST_CHECK_EQUAL(summary.change, CENT);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(summary.bytes, 226u);
    BOOST_CHECK_EQUAL(summary.fee + pay + summary.change, summary.amount);

    // Boundary: a single satoshi of change is absorbed.
    pay = input - BASE_FEE - 1;
    summary = Compute(selection, {pay}, false);
    BOOST_CHECK_EQUAL(summary.change, 0);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE + 1);
    BOOST_CHECK_EQUAL(summary.bytes, 192u);
    BOOST_CHECK_EQUAL(summary.after_fee, pay);
}

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_subtract_fee_from_amount, CoinControlSetup)
{
    const interfaces::WalletCoinControl selection = SelectFunded({0});
    const int64_t input = 1 * COIN;

    // The fee comes out of the recipients, so change is the whole remainder.
    int64_t pay = input - CENT;
    interfaces::CoinControlSummary summary = Compute(selection, {pay}, true);
    BOOST_CHECK_EQUAL(summary.change, CENT);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(summary.bytes, 226u);
    BOOST_CHECK_EQUAL(summary.after_fee, input - BASE_FEE);

    // The absorption block is NOT gated on the flag: it runs on whichever
    // change value the branch above produced, so a sub-CENT remainder is still
    // folded into the fee here.
    pay = input - (CENT - 1);
    summary = Compute(selection, {pay}, true);
    BOOST_CHECK_EQUAL(summary.change, 0);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE + CENT - 1);
    BOOST_CHECK_EQUAL(summary.bytes, 192u);
}

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_floors_after_fee, CoinControlSetup)
{
    // Output 5 holds 1000 satoshis, far below the fee its own size implies.
    const interfaces::CoinControlSummary summary = Compute(SelectFunded({5}), {500}, false);

    BOOST_CHECK_EQUAL(summary.amount, 1000);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(summary.after_fee, 0); // floored, not negative
    // Change is left negative: only sub-CENT positive change is absorbed, and
    // the byte estimate keeps its change output.
    BOOST_CHECK_EQUAL(summary.change, 1000 - BASE_FEE - 500);
    BOOST_CHECK_EQUAL(summary.bytes, 226u);
    BOOST_CHECK(summary.low_output);
    BOOST_CHECK(!summary.dust);
}

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_skips_unusable_outpoints, CoinControlSetup)
{
    interfaces::WalletCoinControl selection;
    // Not in the wallet at all.
    selection.Select(COutPoint(uint256S("0xdeadbeef"), 0));
    // In the wallet, but past the end of the transaction's outputs.
    selection.Select(COutPoint(funding.GetHash(), 6));
    // In the wallet, but neither confirmed nor in the mempool: depth -1, the
    // vanished/conflicted case.
    selection.Select(COutPoint(orphan.GetHash(), 0));

    interfaces::CoinControlSummary summary = Compute(selection, {}, false);
    BOOST_CHECK_EQUAL(summary.quantity, 0);
    BOOST_CHECK_EQUAL(summary.amount, 0);
    BOOST_CHECK_EQUAL(summary.bytes, 0u);
    BOOST_CHECK_EQUAL(summary.fee, 0);

    // Mixed with a usable outpoint, only the usable one contributes.
    selection.Select(COutPoint(funding.GetHash(), 0));
    summary = Compute(selection, {}, false);
    BOOST_CHECK_EQUAL(summary.quantity, 1);
    BOOST_CHECK_EQUAL(summary.amount, 1 * COIN);
    BOOST_CHECK_EQUAL(summary.bytes, 226u); // 148 + 2 * 34 + 10
}

BOOST_FIXTURE_TEST_CASE(wallet_coin_control_summary_counts_every_recipient_entry, CoinControlSetup)
{
    const interfaces::WalletCoinControl selection = SelectFunded({0});
    const int64_t input = 1 * COIN;

    // The byte estimate counts every entry in recipient_amounts, while
    // pay_amount, low_output and the dust-check outputs only consider entries
    // above zero. A zero-amount row therefore adds 34 bytes for an output that
    // would never be created: 148 + (2 + 1) * 34 + 10 = 260.
    interfaces::CoinControlSummary summary = Compute(selection, {2 * CENT, 0}, false);
    BOOST_CHECK_EQUAL(summary.bytes, 260u);
    BOOST_CHECK(!summary.low_output);
    BOOST_CHECK_EQUAL(summary.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(summary.change, input - BASE_FEE - 2 * CENT);
    BOOST_CHECK_EQUAL(summary.fee + 2 * CENT + summary.change, summary.amount);

    // A recipient list summing to zero or less leaves pay_amount at 0, which
    // skips the change/byte adjustment entirely despite the list being
    // non-empty -- the same path as no recipients at all.
    summary = Compute(selection, {2 * CENT, -3 * CENT}, false);
    BOOST_CHECK_EQUAL(summary.bytes, 260u);
    BOOST_CHECK_EQUAL(summary.change, 0);
    BOOST_CHECK(!summary.low_output);
    BOOST_CHECK_EQUAL(summary.after_fee, input - BASE_FEE);
}

BOOST_AUTO_TEST_SUITE_END()
