// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "clientversion.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/node.h"
#include "interfaces/staking.h"
#include "interfaces/wallet.h"
#include "key_io.h"
#include "node/ui_interface.h"
#include "sync.h"
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
    BOOST_CHECK_EQUAL(node->isTestNet(), fTestNet);
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

BOOST_AUTO_TEST_CASE(wallet_wraps_cwallet)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    BOOST_CHECK_EQUAL(wallet->getBalance(), pwalletMain->GetBalance());
    BOOST_CHECK_EQUAL(wallet->isCrypted(), pwalletMain->IsCrypted());
    BOOST_CHECK_EQUAL(wallet->isLocked(), pwalletMain->IsLocked());

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
    BOOST_CHECK(wallet->listCoins().empty());
}

BOOST_AUTO_TEST_CASE(wallet_send_coins_boundary_guards)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    // An empty recipient list must fail cleanly rather than index
    // recipients[0] (the GUI pre-checks this; a direct caller must not UB).
    interfaces::SendCoinsResult result = wallet->sendCoins({}, nullptr, 0);
    BOOST_CHECK(result.status == interfaces::SendCoinsStatus::TransactionCreationFailed);

    // An undecodable address must be rejected node-side — it would
    // otherwise become an empty (anyone-can-spend) output script.
    interfaces::WalletSendRecipient bad_recipient;
    bad_recipient.address = "not-a-valid-address";
    bad_recipient.amount = 1;
    result = wallet->sendCoins({bad_recipient}, nullptr, 0);
    BOOST_CHECK(result.status == interfaces::SendCoinsStatus::InvalidAddress);

    // With a well-formed address, the empty test wallet has no spendable
    // coins, so any positive send must fail the node-side balance pre-check
    // without creating anything.
    interfaces::WalletSendRecipient recipient;
    recipient.address = EncodeDestination(CKeyID());
    recipient.amount = 1;
    result = wallet->sendCoins({recipient}, nullptr, 0);
    BOOST_CHECK(result.status == interfaces::SendCoinsStatus::AmountExceedsBalance);
    BOOST_CHECK(result.txid_hex.empty());
}

BOOST_AUTO_TEST_CASE(wallet_key_from_pool_labels_address_book)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    CPubKey pub_key;
    const std::string label = "interfaces-test-label";
    BOOST_REQUIRE(wallet->getKeyFromPool(pub_key, label));
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

BOOST_AUTO_TEST_SUITE_END()
