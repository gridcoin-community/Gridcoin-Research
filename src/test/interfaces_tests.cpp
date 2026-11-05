// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chain.h"
#include "clientversion.h"
#include "interfaces/connection_scope.h"
#include "interfaces/file_transfer.h"
#include "interfaces/gui_state.h"
#include "interfaces/handler.h"
#include "interfaces/init.h"
#include "interfaces/mrc.h"
#include "interfaces/node.h"
#include "interfaces/psgt.h"
#include "interfaces/sidestake.h"
#include "interfaces/staking.h"
#include "interfaces/wallet.h"
#include "key.h"
#include "key_io.h"
#include "node/access_policy.h"
#include "node/gui_cache.h"
#include "node/shutdown.h"
#include "node/ui_interface.h"
#include "policy/fees.h"
#include "gridcoin/accrual/snapshot.h"
#include "gridcoin/cpid.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/sidestake.h"
#include "primitives/transaction.h"
#include "psgt.h"
#include "scheduler.h"
#include "sync.h"
#include "test/encrypted_wallet.h"
#include "test/gui_cache_fakes.h"
#include "test/lone_best_block.h"
#include "test/psgt_test_helpers.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "test/wallet_file_placeholder.h"
#include "txmempool.h"
#include "util.h"
#include "util/strencodings.h"
#include "wallet/capability.h"
#include "wallet/transfer_sources.h"
#include "wallet/wallet.h"

#include <rpc/client.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>
#include "consensus/consensus.h"
#include "crypto/sha256.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>

extern CWallet* pwalletMain;

using namespace std::chrono_literals;

namespace {
using gui_cache_test::PublishedFakeCache;

//! Long enough for the cache's timer to have acted on anything it was going
//! to act on, at a 4 s interval on a manual clock: a wake and a settle.
constexpr auto TIMER_CHANCE = node::GuiCache::SETTLE + 250ms;
} // anonymous namespace

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

BOOST_AUTO_TEST_CASE(node_reports_core_paths)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    // With the fixture's wallet loaded the wallet file is under the datadir.
    {
        BOOST_REQUIRE(pwalletMain != nullptr);
        const interfaces::CorePaths paths = node->getCorePaths();

        BOOST_CHECK_EQUAL(paths.datadir, GetDataDir().string());
        BOOST_CHECK(!paths.datadir.empty());
        BOOST_CHECK_EQUAL(paths.debug_log, LogInstance().m_file_path.string());
        BOOST_CHECK_EQUAL(paths.config_file, GetConfigFile().string());
        BOOST_CHECK(!paths.settings_file.empty());
        BOOST_CHECK_EQUAL(paths.wallet_file, (GetDataDir() / pwalletMain->strWalletFile).string());
    }

    // With no wallet loaded the wallet file is empty and the datadir is not.
    CWallet* const saved_wallet = pwalletMain;
    pwalletMain = nullptr;
    const interfaces::CorePaths no_wallet = node->getCorePaths();
    pwalletMain = saved_wallet;

    BOOST_CHECK(no_wallet.wallet_file.empty());
    BOOST_CHECK(!no_wallet.datadir.empty());
}

BOOST_AUTO_TEST_CASE(node_wraps_chain_globals)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    BOOST_CHECK_EQUAL(node->getNumBlocks(), WITH_LOCK(cs_main, return nBestHeight));
    BOOST_CHECK(node->getBestBlockHash() == WITH_LOCK(cs_main, return hashBestChain));
    BOOST_CHECK_EQUAL(node->isTestNet(), OnTestnet());
    BOOST_CHECK_EQUAL(node->isMainNet(), OnMainnet());
    BOOST_CHECK_EQUAL(node->getClientVersion(), FormatFullVersion());
    // No peers in the unit-test environment.
    BOOST_CHECK_EQUAL(node->getNodeCount(), 0);
}

// With dynamic settings disabled, the settings-file write throws. The Node
// interface must report that as a failed change, not let it escape: the GUI
// calls changeSettings() from OptionsModel::setData, where a throw would skip
// the caller's failure handling and reach Qt's event loop.
BOOST_AUTO_TEST_CASE(node_changesettings_reports_a_throwing_write_as_failure)
{
    const fs::path datadir = gArgs.GetArg("-datadir", "");
    BOOST_REQUIRE(!datadir.empty());
    fs::create_directories(datadir);
    gArgs.ClearPathCache();

    // A registered probe name, so ChangeSettings() gets past its name check to
    // the write; the registration outlives the case, as util_tests' probes do.
    const std::string probe = "node_changesettings_throw_probe";
    if (!gArgs.GetArgFlags("-" + probe)) {
        gArgs.AddArg("-" + probe, "unit-test probe", ArgsManager::ALLOW_ANY, OptionsCategory::HIDDEN);
    }

    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();
    const std::string before = gArgs.GetArg("-" + probe, "unset");

    // A boolean false forced "settings" disables the settings file, as
    // -nosettings does; erased on every exit from this scope.
    bool had_forced = true;
    gArgs.LockSettings([&](util::Settings& s) { had_forced = s.forced_settings.count("settings") != 0; });
    BOOST_REQUIRE(!had_forced);
    struct ForcedSettingsEraser {
        ~ForcedSettingsEraser()
        {
            gArgs.LockSettings([](util::Settings& s) { s.forced_settings.erase("settings"); });
        }
    } eraser;
    gArgs.LockSettings([](util::Settings& s) { s.forced_settings["settings"] = false; });
    BOOST_REQUIRE(!gArgs.GetSettingsPath());

    // A value that differs from the current one, so the store is attempted.
    const std::string value = before == "after" ? "after2" : "after";

    interfaces::SettingChangeResult result;
    BOOST_REQUIRE_NO_THROW(result = node->changeSettings({{probe, value}}));
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(!result.invalid_input);
    BOOST_CHECK(!result.error.empty());
    // CommitRwSettings restored the setting before the throw.
    BOOST_CHECK_EQUAL(gArgs.GetArg("-" + probe, "unset"), before);
    BOOST_CHECK(getRwSetting(probe).isNull());
}

// The GUI console dispatches into the RPC table without the socket server, so it must
// honour the same warmup refusal. stop is exempt, and is covered by the predicate test in
// rpc_bind_policy_tests; running it here would shut the test process down, so an unknown
// method stands in to show that an exempt-or-ready call gets past the gate.
BOOST_AUTO_TEST_CASE(console_command_refused_in_warmup)
{
    struct RestoreWarmup {
        ~RestoreWarmup() { SetRPCWarmupForTest(false); }
    } restore;

    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    SetRPCWarmupForTest(true);
    const interfaces::RpcConsoleResult refused = node->executeRpcConsoleCommand("getblockcount", {});
    BOOST_CHECK(!refused.ok);
    BOOST_CHECK(refused.output.find("(code -28)") != std::string::npos);

    SetRPCWarmupFinished();
    const interfaces::RpcConsoleResult open = node->executeRpcConsoleCommand("nosuchmethod", {});
    BOOST_CHECK(!open.ok);
    BOOST_CHECK(open.output.find("(code -28)") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(node_secret_console_refuses_other_methods)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    const interfaces::RpcConsoleResult result =
        node->executeRpcConsoleSecretCommand("getblockcount", {SecureString("x")}, {});
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.output.find("takes no passphrase") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(node_secret_console_changes_passphrase)
{
    grc_test::EncryptedWallet fx;
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    const interfaces::RpcConsoleResult result = node->executeRpcConsoleSecretCommand(
        "walletpassphrasechange", {grc_test::EncryptedWallet::PASSPHRASE, SecureString("p\xc3\xa4 ss")}, {});
    BOOST_CHECK_MESSAGE(result.ok, result.output);

    BOOST_CHECK(fx.wallet.Unlock(SecureString("p\xc3\xa4 ss")));
}

// The secret path runs the method itself, after the dispatcher's checks through CRPCTable::admit(),
// the argument count among them: an extra argument must get the help text and change nothing.
BOOST_AUTO_TEST_CASE(node_secret_console_checks_argument_count)
{
    grc_test::EncryptedWallet fx;
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    const interfaces::RpcConsoleResult result = node->executeRpcConsoleSecretCommand(
        "walletpassphrasechange", {grc_test::EncryptedWallet::PASSPHRASE, SecureString("changed")}, {"extra"});
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.output.find("walletpassphrasechange") != std::string::npos);

    BOOST_CHECK(!fx.wallet.Unlock(SecureString("changed")));
    BOOST_CHECK(fx.wallet.Unlock(grc_test::EncryptedWallet::PASSPHRASE));
}

// A body's runtime_error comes back formatted as the dispatcher formats it, as the plain console
// formats a dispatcher error.
BOOST_AUTO_TEST_CASE(node_secret_console_formats_a_body_exception_like_the_dispatcher)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    const interfaces::RpcConsoleResult secret = node->executeRpcConsoleSecretCommand("encryptwallet", {SecureString("")}, {});

    std::string dispatched;
    try {
        tableRPC.execute("encryptwallet", RPCConvertValues("encryptwallet", {""}));
    } catch (const UniValue& error) {
        dispatched = strprintf("%s (code %d)", find_value(error, "message").get_str(), find_value(error, "code").get_int());
    }

    BOOST_CHECK(!secret.ok);
    BOOST_CHECK(secret.output.size() > 9 && secret.output.rfind("(code -1)") == secret.output.size() - 9);
    BOOST_CHECK_EQUAL(secret.output, dispatched);
}

// The secret path makes the dispatcher's own checks before the method runs, as tableRPC.execute()
// makes them for HTTP RPC and the plain console. The passphrase gate stops none of the passphrase
// methods: encryptwallet on an encrypted wallet, locked or unlocked for staking only, is refused by
// the method itself with -15 on every route, and a wrong argument count gets the dispatcher's help,
// without the passphrase line. Both answers are the dispatcher's, formatted as the console formats
// them.
BOOST_AUTO_TEST_CASE(node_secret_console_makes_the_dispatcher_checks)
{
    grc_test::EncryptedWallet fx;
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    const auto dispatched = [](const std::vector<std::string>& args) {
        try {
            tableRPC.execute("encryptwallet", RPCConvertValues("encryptwallet", args));
        } catch (const UniValue& error) {
            return strprintf("%s (code %d)", find_value(error, "message").get_str(), find_value(error, "code").get_int());
        } catch (const std::exception& e) {
            return std::string("Error: ") + e.what();
        }
        return std::string("returned");
    };

    for (const bool staking_only : {false, true}) {
        if (staking_only) fx.UnlockFor(UnlockScope::StakingOnly);

        const interfaces::RpcConsoleResult refused =
            node->executeRpcConsoleSecretCommand("encryptwallet", {SecureString("another")}, {});
        BOOST_CHECK(!refused.ok);
        BOOST_CHECK_EQUAL(refused.output, dispatched({"another"}));
        // A passphrase method is never run again under an unlock, so nothing asks for one.
        BOOST_CHECK(!refused.wallet_unlock_needed);
        BOOST_CHECK_EQUAL(refused.output,
                          "Error: running with an encrypted wallet, but encryptwallet was called. (code -15)");
    }

    const interfaces::RpcConsoleResult arity =
        node->executeRpcConsoleSecretCommand("encryptwallet", {SecureString("another")}, {"extra"});
    BOOST_CHECK(!arity.ok);
    BOOST_CHECK_EQUAL(arity.output, dispatched({"another", "extra"}));
    BOOST_CHECK(arity.output.rfind("Error: encryptwallet ", 0) == 0);
    BOOST_CHECK(arity.output.find("Needs the wallet passphrase.") == std::string::npos);

    // Nothing changed: still unlocked for staking only, and the passphrase still opens the wallet
    // once it is locked (Unlock() refuses a wallet that is already unlocked).
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    fx.wallet.Lock();
    BOOST_CHECK(fx.wallet.Unlock(grc_test::EncryptedWallet::PASSPHRASE));
}

// The plain console call carries its arguments as plain strings, so it refuses the three passphrase
// methods before anything runs: the right old passphrase sent this way changes nothing, and the
// wallet stays locked.
BOOST_AUTO_TEST_CASE(node_plain_console_refuses_passphrase_methods)
{
    grc_test::EncryptedWallet fx;
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();
    const std::string passphrase(grc_test::EncryptedWallet::PASSPHRASE.begin(), grc_test::EncryptedWallet::PASSPHRASE.end());

    const interfaces::RpcConsoleResult change = node->executeRpcConsoleCommand("walletpassphrasechange", {passphrase, "changed"});
    BOOST_CHECK(!change.ok);
    BOOST_CHECK(!change.wallet_unlock_needed);
    BOOST_CHECK_EQUAL(change.output, "Error: walletpassphrasechange takes a passphrase; the console sends it separately");
    BOOST_CHECK(!fx.wallet.Unlock(SecureString("changed")));

    const interfaces::RpcConsoleResult unlock = node->executeRpcConsoleCommand("walletpassphrase", {passphrase, "60", "true"});
    BOOST_CHECK(!unlock.ok);
    BOOST_CHECK_EQUAL(unlock.output, "Error: walletpassphrase takes a passphrase; the console sends it separately");
    BOOST_CHECK(fx.wallet.IsLocked());

    const interfaces::RpcConsoleResult encrypt = node->executeRpcConsoleCommand("encryptwallet", {"another"});
    BOOST_CHECK(!encrypt.ok);
    BOOST_CHECK_EQUAL(encrypt.output, "Error: encryptwallet takes a passphrase; the console sends it separately");

    BOOST_CHECK(fx.wallet.Unlock(grc_test::EncryptedWallet::PASSPHRASE));
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
    // Against the WALLET, not against the interface's own sibling accessors:
    // getLockState() and getUnlockStakingOnlyFlag() are the same expression
    // over the same source, so comparing them to each other cannot fail no
    // matter what the snapshot is built from.
    BOOST_CHECK_EQUAL(lock_state.unlocked_for_staking_only, pwalletMain->IsUnlockedForStakingOnly());
    BOOST_CHECK_EQUAL(lock_state.staking_only_flag, pwalletMain->IsUnlockedForStakingOnly());

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

    // Staking-only is no longer a flag that can be posed independently of the
    // lock state; it is one value on the wallet. The test wallet is
    // unencrypted, which has nothing to restrict, so it reads Full.
    BOOST_CHECK(pwalletMain->GetUnlockScope() == UnlockScope::Full);
    BOOST_CHECK(!wallet->isUnlockedForStakingOnly());
    BOOST_CHECK(!wallet->getUnlockStakingOnlyFlag());

    // Unlocking an unencrypted wallet fails, and a failed unlock must not move
    // the scope -- the old assertion that a failed unlock does not overwrite
    // the preference, restated against the value that now carries it.
    BOOST_CHECK(!wallet->unlockWallet(SecureString("passphrase"), true));
    BOOST_CHECK(pwalletMain->GetUnlockScope() == UnlockScope::Full);
    BOOST_CHECK(!wallet->isUnlockedForStakingOnly());

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

BOOST_AUTO_TEST_CASE(wallet_reports_the_fee_rate_in_effect)
{
    BOOST_REQUIRE(pwalletMain != nullptr);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);

    // The least a send pays per kB.
    const CAmount minimum = GetBaseFee(CTransaction(), GMF_SEND);

    {
        // A configured rate above the minimum is the rate a send pays.
        const grc_test::TransactionFeeGuard fee(minimum + 23456);
        BOOST_CHECK_EQUAL(wallet->getTransactionFeeRate(), minimum + 23456);
    }

    {
        // A configured rate below the minimum, here none, pays the minimum.
        const grc_test::TransactionFeeGuard fee(0);
        BOOST_CHECK_EQUAL(wallet->getTransactionFeeRate(), minimum);
    }

    {
        const grc_test::TransactionFeeGuard fee(minimum - 1);
        BOOST_CHECK_EQUAL(wallet->getTransactionFeeRate(), minimum);
    }
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

BOOST_AUTO_TEST_CASE(gui_state_wraps_the_cache)
{
    using interfaces::GuiStateSnapshot;

    // No cache runs in the unit-test environment, so there is no GuiState.
    BOOST_CHECK(interfaces::MakeGuiState() == nullptr);
    BOOST_CHECK(interfaces::MakeGridcoinInit()->makeGuiState() == nullptr);

    // The recorders outlive the cache, so its timer is joined before they go.
    gui_cache_test::FakeCompute compute;
    gui_cache_test::Deliveries deliveries;
    compute.height = 42;
    PublishedFakeCache published(compute, 4000ms, 30000ms);

    BOOST_CHECK(interfaces::MakeGridcoinInit()->makeGuiState() != nullptr);
    std::unique_ptr<interfaces::GuiState> gui_state = interfaces::MakeGuiState();
    BOOST_REQUIRE(gui_state);

    // The subscription's own update is the first publish, and it reaches the
    // registered fn.
    std::unique_ptr<interfaces::Handler> handler = gui_state->handleChanged(deliveries.Make());
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    // current() is the cache's Current().
    const GuiStateSnapshot current = gui_state->current();
    const GuiStateSnapshot cached = published.Cache().Current();
    BOOST_CHECK_EQUAL(current.sequence, 1U);
    BOOST_CHECK_EQUAL(current.sequence, cached.sequence);
    BOOST_CHECK_EQUAL(current.height, 42);
    BOOST_CHECK_EQUAL(current.height, cached.height);
    BOOST_CHECK_EQUAL(current.has_balances, cached.has_balances);

    // So does the next publish: a block in SetBestChain's scope once the
    // interval has passed.
    compute.height = 43;
    compute.Advance(4000ms);
    gui_cache_test::InScope(published.Cache(), node::GuiCache::CHAIN);
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) {
        return snap.sequence == 2 && snap.height == 43;
    }));
    BOOST_CHECK_EQUAL(gui_state->current().height, 43);
}

BOOST_AUTO_TEST_CASE(gui_state_request_update_delivers_a_forced_update)
{
    using interfaces::GuiStateSnapshot;

    gui_cache_test::FakeCompute compute;
    gui_cache_test::Deliveries deliveries;
    PublishedFakeCache published(compute, 4000ms, 30000ms);
    std::unique_ptr<interfaces::GuiState> gui_state = interfaces::MakeGuiState();
    BOOST_REQUIRE(gui_state);

    std::unique_ptr<interfaces::Handler> handler = gui_state->handleChanged(deliveries.Make());
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    // Counted from the timer at rest after the subscription's update.
    std::this_thread::sleep_for(TIMER_CHANCE);
    const int runs = compute.cs_main_runs.load();
    const int chain_calls = compute.chain_calls.load();

    // Inside the interval (the clock does not move): the request is answered by
    // one more delivery, from an update the timer forced under cs_main.
    gui_state->requestUpdate();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 2; }));
    std::this_thread::sleep_for(TIMER_CHANCE);

    BOOST_CHECK_EQUAL(deliveries.Count(), 2U);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), runs + 1);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), chain_calls + 1);
    const std::vector<std::string> threads = deliveries.Threads();
    BOOST_REQUIRE_EQUAL(threads.size(), 2U);
    BOOST_CHECK_EQUAL(threads[1], gui_cache_test::TIMER_THREAD);
    BOOST_CHECK_EQUAL(deliveries.DeliveredUnderALock(), 0);
}

BOOST_AUTO_TEST_CASE(gui_state_quote_latest_request_wins)
{
    gui_cache_test::FakeCompute compute;
    gui_cache_test::Deliveries deliveries;
    gui_cache_test::Quotes quotes;

    // Holds the timer inside the subscription's update, so that both requests
    // below are made before it can serve either.
    Mutex latch_mutex;
    std::condition_variable latch_cv;
    bool released = false; // guarded by latch_mutex
    std::atomic<bool> entered{false};

    compute.chain_hook = [&] {
        entered = true;
        WAIT_LOCK(latch_mutex, lock);
        while (!released) latch_cv.wait(lock);
    };

    PublishedFakeCache published(compute, 4000ms, 30000ms);

    // Declared after the published cache, so it runs before the cache is
    // stopped: a failure that unwinds the case while the timer is held in the
    // hook would otherwise leave the stop joining a thread nobody can release.
    struct OpenLatchOnExit
    {
        Mutex& mutex;
        std::condition_variable& cv;
        bool& released;

        ~OpenLatchOnExit()
        {
            {
                LOCK(mutex);
                released = true;
            }
            cv.notify_all();
        }
    } open_latch{latch_mutex, latch_cv, released};

    std::unique_ptr<interfaces::GuiState> gui_state = interfaces::MakeGuiState();
    BOOST_REQUIRE(gui_state);

    std::unique_ptr<interfaces::Handler> quote_handler = gui_state->handleMRCQuote(quotes.Make());
    std::unique_ptr<interfaces::Handler> handler = gui_state->handleChanged(deliveries.Make());
    BOOST_REQUIRE(gui_cache_test::WaitUntil([&] { return entered.load(); }));

    // Both return at once, with the timer busy.
    gui_state->requestMRCQuote(10, 1);
    gui_state->requestMRCQuote(20, 2);

    {
        LOCK(latch_mutex);
        released = true;
    }
    latch_cv.notify_all();

    BOOST_REQUIRE(quotes.WaitForCount(1));
    std::this_thread::sleep_for(TIMER_CHANCE);

    // One answer: the second request's, at its boost.
    const std::vector<gui_cache_test::Quotes::Answer> answers = quotes.Answers();
    BOOST_REQUIRE_EQUAL(answers.size(), 1U);
    BOOST_CHECK_EQUAL(answers[0].request_id, 2U);
    BOOST_CHECK_EQUAL(answers[0].quote.fee, 20);
    BOOST_CHECK_EQUAL(answers[0].thread, gui_cache_test::TIMER_THREAD);
    BOOST_CHECK_EQUAL(compute.quote_calls.load(), 1);
    BOOST_CHECK_EQUAL(quotes.AnsweredUnderALock(), 0);
}

BOOST_AUTO_TEST_CASE(gui_state_computes_only_while_a_gui_is_subscribed)
{
    using interfaces::GuiStateSnapshot;

    gui_cache_test::FakeCompute compute;
    gui_cache_test::Deliveries first;
    gui_cache_test::Deliveries second;
    gui_cache_test::Deliveries third;
    PublishedFakeCache published(compute, 4000ms, 30000ms);
    node::GuiCache& cache = published.Cache();

    // No GUI: blocks in SetBestChain's scope compute nothing, and the timer
    // takes no cs_main.
    for (int i = 0; i < 3; ++i) {
        compute.Advance(4000ms);
        gui_cache_test::InScope(cache, node::GuiCache::ALL);
    }
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 0);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 0);
    BOOST_CHECK_EQUAL(cache.Current().sequence, 0U);

    std::unique_ptr<interfaces::GuiState> gui_state = interfaces::MakeGuiState();
    BOOST_REQUIRE(gui_state);

    // A GUI subscribes inside the interval (the clock does not move): one
    // forced update, delivered to it.
    std::unique_ptr<interfaces::Handler> first_handler = gui_state->handleChanged(first.Make());
    BOOST_REQUIRE(first.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(first.Count(), 1U);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 1);
    BOOST_CHECK_GE(compute.cs_main_runs.load(), 1);

    // A second subscription forces nothing: only the first does. Counted from
    // the timer at rest.
    const int runs_at_rest = compute.cs_main_runs.load();
    std::unique_ptr<interfaces::Handler> second_handler = gui_state->handleChanged(second.Make());
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), runs_at_rest);
    BOOST_CHECK_EQUAL(second.Count(), 0U);

    // The first disconnects: it gets nothing more, and the cache goes on
    // computing for the second.
    first_handler->disconnect();
    compute.Advance(4000ms);
    gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_REQUIRE(second.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 2; }));
    BOOST_CHECK_EQUAL(first.Count(), 1U);

    // The second is destroyed: computing stops, in scope and on the timer.
    // Counted from the timer at rest: a machine that stalls the update above
    // for longer than the settle lets the timer take cs_main for it once
    // (node/gui_cache.h, SETTLE).
    second_handler.reset();
    std::this_thread::sleep_for(TIMER_CHANCE);
    const int runs = compute.cs_main_runs.load();

    compute.Advance(4000ms);
    gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), runs);

    // Destroying the handler the first disconnected releases nothing twice:
    // the next subscription is a first one again, and forces its update.
    first_handler.reset();
    std::unique_ptr<interfaces::Handler> third_handler = gui_state->handleChanged(third.Make());
    BOOST_REQUIRE(third.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 3; }));
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 3);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), runs + 1);
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

// The interface methods that use the wallet's keys run under a capability the
// client began for that kind of operation, and refuse without one while the
// wallet is unlocked for staking only. The fixture's wallet holds one key, added
// before encryption, and is swapped into pwalletMain for the case.
namespace {

using grc_test::EncryptedWallet;

//! A PSGT with one input, spending a coin paid to the fixture's key, so that
//! only this wallet can sign it.
interfaces::PSGTBytes PsgtSpendingTheFixtureCoin(const EncryptedWallet& fx)
{
    const CTransaction previous = psgt_test::MakePrevTx(fx.key, 10 * COIN);

    return SerializePSGT(psgt_test::MakeSpendPSGT(previous, 9 * COIN));
}

//! Signatures on the PSGT's first input, partial or final. An empty or
//! undecodable buffer carries none.
size_t SignaturesOnFirstInput(const interfaces::PSGTBytes& bytes)
{
    PartiallySignedTransaction psgt;
    std::string error;

    if (bytes.empty() || !DecodePSGTBytes(psgt, bytes, error) || psgt.inputs.empty()) {
        return 0;
    }

    return psgt.inputs[0].partial_sigs.size() + (psgt.inputs[0].final_script_sig.empty() ? 0 : 1);
}

//! A well-formed pool image that is not in the pool.
const std::string ABSENT_POOL_IMAGE(40, 'a');

//! The refusal MRC submit gives a wallet unlocked for staking only.
const std::string MRC_STAKING_ONLY_REFUSAL =
    "The wallet is unlocked for staking only. Unlock it fully to submit a manual research claim.";

} // namespace

BOOST_AUTO_TEST_CASE(ipc_signpsgt_needs_a_signpsgt_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::PSGTPoolContext> psgt_context = interfaces::MakePSGTPoolContext(&fx.wallet);

    const interfaces::PSGTSignResult result = psgt_context->signPSGT(PsgtSpendingTheFixtureCoin(fx));

    // Refused as such, not reported as a PSGT the wallet could not sign.
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(!result.error.empty());
    BOOST_CHECK_EQUAL(result.signed_now, 0);
    BOOST_CHECK_EQUAL(SignaturesOnFirstInput(result.psgt), 0U);

    // The pool path refuses before it looks the image up.
    std::string error;
    std::string txid;
    BOOST_CHECK(psgt_context->signPoolEntry(ABSENT_POOL_IMAGE, error, txid) == interfaces::PSGTSignStatus::FAILED);
    BOOST_CHECK(!error.empty());

    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_CASE(ipc_signpsgt_runs_under_a_signpsgt_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);
    std::unique_ptr<interfaces::PSGTPoolContext> psgt_context = interfaces::MakePSGTPoolContext(&fx.wallet);
    const interfaces::PSGTBytes unsigned_psgt = PsgtSpendingTheFixtureCoin(fx);

    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignPSGT));

    // The wallet's own unlock scope is unchanged.
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!fx.wallet.CanUseKeys());

    const interfaces::PSGTSignResult result = psgt_context->signPSGT(unsigned_psgt);
    BOOST_CHECK(result.ok);
    BOOST_CHECK_EQUAL(result.signed_now, 1);
    BOOST_CHECK_EQUAL(SignaturesOnFirstInput(result.psgt), 1U);

    // The pool path gets past the refusal and finds the image absent.
    std::string error;
    std::string txid;
    BOOST_CHECK(psgt_context->signPoolEntry(ABSENT_POOL_IMAGE, error, txid) == interfaces::PSGTSignStatus::NOT_FOUND);

    wallet->endElevation();

    const interfaces::PSGTSignResult after = psgt_context->signPSGT(unsigned_psgt);
    BOOST_CHECK(!after.ok);
    BOOST_CHECK_EQUAL(after.signed_now, 0);
    BOOST_CHECK_EQUAL(SignaturesOnFirstInput(after.psgt), 0U);

    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_CASE(ipc_signmessage_needs_a_signmessage_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);

    std::string signature;
    BOOST_CHECK(wallet->signMessage(EncodeDestination(fx.id), "a message", signature)
                == interfaces::MessageSignStatus::KeyNotAvailable);
    BOOST_CHECK(signature.empty());
}

BOOST_AUTO_TEST_CASE(ipc_signmessage_runs_under_a_signmessage_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);
    const std::string address = EncodeDestination(fx.id);

    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));

    std::string signature;
    BOOST_REQUIRE(wallet->signMessage(address, "a message", signature) == interfaces::MessageSignStatus::OK);

    bool invalid = false;
    const std::vector<unsigned char> raw = DecodeBase64(signature.c_str(), &invalid);
    BOOST_REQUIRE(!invalid);
    BOOST_CHECK(wallet->verifyMessage(address, "a message", raw) == interfaces::MessageVerifyStatus::OK);

    wallet->endElevation();

    std::string after;
    BOOST_CHECK(wallet->signMessage(address, "a message", after) == interfaces::MessageSignStatus::KeyNotAvailable);
    BOOST_CHECK(after.empty());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_CASE(ipc_capability_wrong_kind)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);
    std::unique_ptr<interfaces::PSGTPoolContext> psgt_context = interfaces::MakePSGTPoolContext(&fx.wallet);

    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));

    std::string signature;
    BOOST_CHECK(wallet->signMessage(EncodeDestination(fx.id), "a message", signature)
                == interfaces::MessageSignStatus::KeyNotAvailable);
    BOOST_CHECK(signature.empty());

    const interfaces::PSGTSignResult result = psgt_context->signPSGT(PsgtSpendingTheFixtureCoin(fx));
    BOOST_CHECK(!result.ok);
    BOOST_CHECK_EQUAL(SignaturesOnFirstInput(result.psgt), 0U);

    wallet->endElevation();
}

BOOST_AUTO_TEST_CASE(ipc_elevatewallet_always_false)
{
    EncryptedWallet fx;
    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);

    // Unlocked for staking: the right passphrase widens nothing.
    fx.UnlockFor(UnlockScope::StakingOnly);
    BOOST_CHECK(!wallet->elevateWallet(EncryptedWallet::PASSPHRASE));
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
    BOOST_CHECK(!fx.wallet.CanUseKeys());

    CKey key;
    BOOST_CHECK(!fx.wallet.GetKey(fx.id, key));

    // Locked: it unlocks nothing either.
    fx.wallet.Lock();
    BOOST_CHECK(!wallet->elevateWallet(EncryptedWallet::PASSPHRASE));
    BOOST_CHECK(fx.wallet.IsLocked());

    // Nor is it counted as a passphrase attempt.
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
}

BOOST_AUTO_TEST_CASE(ipc_begin_elevation_refuses_an_unknown_kind)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);

    // A value past the last kind, as a client built against another enumeration
    // could send. Refused before the passphrase is checked, so it is not
    // counted as a failed attempt and the slot stays empty.
    BOOST_CHECK(!wallet->beginElevation(EncryptedWallet::PASSPHRASE, static_cast<CapabilityKind>(200)));
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    // A wrong passphrase is refused and counted.
    BOOST_CHECK(!wallet->beginElevation(SecureString("not the passphrase"), CapabilityKind::Send));
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);

    // Ending with nothing begun is a no-op.
    wallet->endElevation();
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

// A GUI can go away while its beginElevation() is still checking the passphrase.
// Its connection's teardown then finds no capability to end, so the capability
// the call goes on to begin must be ended by the call itself, or it would serve
// the next GUI to attach. Driven deterministically: the wallet is bound to a
// scope that is already closed when the call is made.
BOOST_AUTO_TEST_CASE(ipc_begin_elevation_on_a_closed_connection_ends_it)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);
    auto scope = interfaces::MakeConnectionScope();
    wallet->bindConnectionScope(scope);
    scope->close();

    BOOST_CHECK(!wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    // The passphrase was right, so it is not counted as a failed attempt.
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    std::string signature;
    BOOST_CHECK(wallet->signMessage(EncodeDestination(fx.id), "a message", signature)
                == interfaces::MessageSignStatus::KeyNotAvailable);
}

// A begin whose connection has closed before the capability reaches the slot
// leaves the slot exactly as it is, so a capability the next GUI has begun in the
// meantime stays installed and live, and the late capability is never in the
// slot. Driven deterministically: A's connection is closed, B then begins a
// capability, and only then does A's begin reach the slot.
BOOST_AUTO_TEST_CASE(ipc_begin_elevation_after_close_keeps_the_next_guis_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> gone = interfaces::MakeWallet(&fx.wallet);
    auto gone_scope = interfaces::MakeConnectionScope();
    gone->bindConnectionScope(gone_scope);
    gone_scope->close();

    std::unique_ptr<interfaces::Wallet> next = interfaces::MakeWallet(&fx.wallet);
    next->bindConnectionScope(interfaces::MakeConnectionScope());
    BOOST_REQUIRE(next->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    const std::optional<CapabilityId> next_id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(next_id.has_value());

    BOOST_CHECK(!gone->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    BOOST_CHECK(fx.wallet.IpcCapability({}) == next_id);
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    // The next GUI's capability is still live.
    std::string signature;
    BOOST_CHECK(next->signMessage(EncodeDestination(fx.id), "a message", signature)
                == interfaces::MessageSignStatus::OK);

    next->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

// The slot is process-wide, so a wallet served on a connection that has closed
// must neither use nor end what the slot holds: by then it may hold the next
// GUI's capability. Its operations read the slot as empty, and its end leaves
// the slot alone (the closed connection's own hook ended what was its). Driven
// deterministically: A's connection is closed, B begins a capability, and then
// A's wallet signs and ends.
BOOST_AUTO_TEST_CASE(ipc_closed_connection_neither_uses_nor_ends_the_next_guis_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    const std::string address = EncodeDestination(fx.id);

    std::unique_ptr<interfaces::Wallet> gone = interfaces::MakeWallet(&fx.wallet);
    auto gone_scope = interfaces::MakeConnectionScope();
    gone->bindConnectionScope(gone_scope);
    gone_scope->close();

    std::unique_ptr<interfaces::Wallet> next = interfaces::MakeWallet(&fx.wallet);
    next->bindConnectionScope(interfaces::MakeConnectionScope());
    BOOST_REQUIRE(next->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    const std::optional<CapabilityId> next_id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(next_id.has_value());

    std::string signature;
    BOOST_CHECK(gone->signMessage(address, "a message", signature) == interfaces::MessageSignStatus::KeyNotAvailable);
    BOOST_CHECK(signature.empty());

    gone->endElevation();
    BOOST_CHECK(fx.wallet.IpcCapability({}) == next_id);

    BOOST_CHECK(next->signMessage(address, "a message", signature) == interfaces::MessageSignStatus::OK);
    next->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

// Bound to an open connection, or not bound at all (the monolithic build's
// in-process wallet), beginElevation() is unchanged.
BOOST_AUTO_TEST_CASE(ipc_begin_elevation_on_an_open_or_no_connection_is_unchanged)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> served = interfaces::MakeWallet(&fx.wallet);
    served->bindConnectionScope(interfaces::MakeConnectionScope());
    BOOST_CHECK(served->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    BOOST_CHECK(fx.wallet.IpcCapability({}).has_value());
    served->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    std::unique_ptr<interfaces::Wallet> monolith = interfaces::MakeWallet(&fx.wallet);
    BOOST_CHECK(monolith->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    BOOST_CHECK(fx.wallet.IpcCapability({}).has_value());
    monolith->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

namespace {
//! A connection scope that only keeps its hooks, so a case can count the ones
//! still registered on it.
class CountingScope final : public interfaces::ConnectionScope
{
public:
    HookId addCloseHook(std::function<void()> fn) override
    {
        m_hooks.emplace(++m_next_id, std::move(fn));
        return m_next_id;
    }

    void removeCloseHook(HookId id) override { m_hooks.erase(id); }

    void close() override
    {
        m_closed = true;
        std::map<HookId, std::function<void()>> hooks = std::move(m_hooks);
        m_hooks.clear();
        for (auto& hook : hooks) hook.second();
    }

    bool closed() const override { return m_closed.load(); }

    size_t Hooks() const { return m_hooks.size(); }

private:
    std::map<HookId, std::function<void()>> m_hooks{};
    HookId m_next_id{0};
    std::atomic<bool> m_closed{false};
};
} // namespace

// Bound again, a wallet takes its close hook off the connection it was bound to
// before, so that connection's close no longer reaches it, and a wallet that
// goes away takes its hook off the connection it is bound to.
BOOST_AUTO_TEST_CASE(wallet_rebound_removes_its_hook_from_the_earlier_connection)
{
    const auto first = std::make_shared<CountingScope>();
    const auto second = std::make_shared<CountingScope>();

    {
        std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);

        w->bindConnectionScope(first);
        BOOST_CHECK_EQUAL(first->Hooks(), 1U);

        w->bindConnectionScope(second);
        BOOST_CHECK_EQUAL(first->Hooks(), 0U);
        BOOST_CHECK_EQUAL(second->Hooks(), 1U);

        // Unbound (in process), it keeps no hook anywhere.
        w->bindConnectionScope(nullptr);
        BOOST_CHECK_EQUAL(second->Hooks(), 0U);

        w->bindConnectionScope(second);
        BOOST_CHECK_EQUAL(second->Hooks(), 1U);
    }

    BOOST_CHECK_EQUAL(first->Hooks(), 0U);
    BOOST_CHECK_EQUAL(second->Hooks(), 0U);
}

namespace {
//! Replaces the encryption shutdown function with a counter for a case, and
//! puts the real one back however the case exits.
struct EncryptionShutdownCounter
{
    int calls{0};
    EncryptionShutdownCounter() { interfaces::SetEncryptionShutdownFnForTesting([this] { ++calls; }); }
    ~EncryptionShutdownCounter() { interfaces::SetEncryptionShutdownFnForTesting({}); }
};

//! A scheduler nobody services, published as the node's for a case, so a queued
//! task can be inspected without waiting for it. The previous handle is put back.
struct UnservicedSchedulerHandle
{
    CScheduler scheduler;
    CScheduler* saved;
    UnservicedSchedulerHandle() : saved(g_scheduler_handle.exchange(&scheduler, std::memory_order_acq_rel)) {}
    ~UnservicedSchedulerHandle() { g_scheduler_handle.store(saved, std::memory_order_release); }
};

//! An in-memory wallet with one key, not yet encrypted, swapped into pwalletMain.
struct PlainWallet
{
    CWallet wallet;
    grc_test::SwapMainWallet swap{&wallet};

    PlainWallet()
    {
        CKey key;
        key.MakeNewKey(true);
        LOCK(wallet.cs_wallet);
        BOOST_REQUIRE(wallet.AddKey(key));
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(ipc_encryptwallet_stops_core_on_disconnect)
{
    PlainWallet fx;
    EncryptionShutdownCounter counter;

    auto w = interfaces::MakeWallet(&fx.wallet);
    auto s = interfaces::MakeConnectionScope();
    w->bindConnectionScope(s);

    BOOST_REQUIRE(w->encryptWallet(SecureString("pp")));
    BOOST_CHECK_EQUAL(counter.calls, 0);

    s->close();
    BOOST_CHECK_EQUAL(counter.calls, 1);
    BOOST_CHECK(!ShutdownRequested());
}

BOOST_AUTO_TEST_CASE(ipc_encryptwallet_schedules_the_shutdown_fallback)
{
    PlainWallet fx;
    EncryptionShutdownCounter counter;

    // A scheduler nobody services, published as the node's for this case, so the
    // queued task can be inspected without waiting for it.
    UnservicedSchedulerHandle handle;

    auto w = interfaces::MakeWallet(&fx.wallet);
    auto s = interfaces::MakeConnectionScope();
    w->bindConnectionScope(s);

    const auto before = std::chrono::system_clock::now();
    BOOST_REQUIRE(w->encryptWallet(SecureString("pp")));

    std::chrono::system_clock::time_point first, last;
    BOOST_REQUIRE_EQUAL(handle.scheduler.getQueueInfo(first, last), 1U);
    BOOST_CHECK(first >= before + std::chrono::seconds{59});
    BOOST_CHECK(first <= std::chrono::system_clock::now() + std::chrono::seconds{61});

    // Nothing has stopped the core yet, and the connection still stands.
    BOOST_CHECK_EQUAL(counter.calls, 0);

    // Unbound wallets schedule nothing.
    PlainWallet other;
    auto monolith = interfaces::MakeWallet(&other.wallet);
    BOOST_REQUIRE(monolith->encryptWallet(SecureString("pp")));
    BOOST_CHECK_EQUAL(handle.scheduler.getQueueInfo(first, last), 1U);
}

BOOST_AUTO_TEST_CASE(encryptwallet_without_scope_does_not_stop)
{
    PlainWallet fx;
    EncryptionShutdownCounter counter;
    UnservicedSchedulerHandle handle;

    {
        auto w = interfaces::MakeWallet(&fx.wallet);
        BOOST_REQUIRE(w->encryptWallet(SecureString("pp")));
    }
    BOOST_CHECK_EQUAL(counter.calls, 0);

    // No scope: no fallback task was queued either.
    std::chrono::system_clock::time_point first, last;
    BOOST_CHECK_EQUAL(handle.scheduler.getQueueInfo(first, last), 0U);
    BOOST_CHECK(!ShutdownRequested());
}

BOOST_AUTO_TEST_CASE(ipc_sendcoins_runs_under_a_send_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);

    interfaces::WalletSendRecipient recipient;
    recipient.address = EncodeDestination(fx.id);
    recipient.amount = 1 * COIN;

    BOOST_CHECK(wallet->sendCoins({recipient}, std::nullopt, 0).status
                == interfaces::SendCoinsStatus::WalletUnlockedForStakingOnly);

    // A capability of another kind does not let the send through.
    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage));
    BOOST_CHECK(wallet->sendCoins({recipient}, std::nullopt, 0).status
                == interfaces::SendCoinsStatus::WalletUnlockedForStakingOnly);

    // Under a Send capability the send gets past the refusal, to the balance
    // check the empty wallet fails.
    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    BOOST_CHECK(wallet->sendCoins({recipient}, std::nullopt, 0).status
                == interfaces::SendCoinsStatus::AmountExceedsBalance);

    wallet->endElevation();
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_CASE(ipc_mrc_submit_runs_under_a_contract_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);
    std::unique_ptr<interfaces::MRC> mrc = interfaces::MakeMRC(&fx.wallet);

    BOOST_CHECK_EQUAL(mrc->submit(0, false).error, MRC_STAKING_ONLY_REFUSAL);

    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    BOOST_CHECK_EQUAL(mrc->submit(0, false).error, MRC_STAKING_ONLY_REFUSAL);

    // Under a Contract capability the claim gets past the refusal and fails
    // further on, for whatever this environment lacks.
    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Contract));
    const interfaces::MRCSubmitResult result = mrc->submit(0, false);
    BOOST_CHECK(!result.success);
    BOOST_CHECK(result.error != MRC_STAKING_ONLY_REFUSAL);

    wallet->endElevation();
    BOOST_CHECK_EQUAL(mrc->submit(0, false).error, MRC_STAKING_ONLY_REFUSAL);
}

BOOST_AUTO_TEST_CASE(ipc_console_command_runs_under_an_rpc_request_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();
    const std::vector<std::string> args{EncodeDestination(fx.id), "a message"};

    // Refused for the keys, and flagged so the console can ask for the
    // passphrase and run the command again.
    interfaces::RpcConsoleResult result = node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.wallet_unlock_needed);

    // A capability of another kind does not let it through.
    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    result = node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.wallet_unlock_needed);

    BOOST_REQUIRE(wallet->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::RpcRequest));
    result = node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(result.ok);
    BOOST_CHECK(!result.wallet_unlock_needed);
    BOOST_CHECK(!result.output.empty());

    wallet->endElevation();
    result = node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.wallet_unlock_needed);

    // Any other failure is not flagged.
    result = node->executeRpcConsoleCommand("nosuchcommand", {});
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(!result.wallet_unlock_needed);

    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

// The settings getters answer for the settings the GUI reads. Every other name
// answers with the caller's default, as if it were not set.
BOOST_AUTO_TEST_CASE(node_setting_getters_answer_only_gui_readable_settings)
{
    grc_test::StateGuard state_guard;
    gArgs.ForceSetArg("-rpcpassword", "pw");
    gArgs.ForceSetArg("-blocknotify", "x");
    gArgs.ForceSetArg("-staking", "0");

    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    BOOST_CHECK_EQUAL(node->getSettingStr("rpcpassword", "d"), "d");
    BOOST_CHECK(!node->isSettingSet("rpcpassword"));
    BOOST_CHECK(node->getSettingBool("rpcpassword", true));
    BOOST_CHECK_EQUAL(node->getSettingInt("rpcpassword", 7), 7);
    BOOST_CHECK_EQUAL(node->getSettingStr("blocknotify", "d"), "d");
    BOOST_CHECK(!node->isSettingSet("blocknotify"));

    BOOST_CHECK(!node->getSettingBool("staking", true));
    BOOST_CHECK(node->isSettingSet("staking"));

    for (const char* name : {"contractchangetoinputaddress", "disableupdatecheck", "enablesidestaking",
                             "enablestakesplit", "minstakesplitvalue", "proxy", "reservebalance", "showorphans",
                             "staking", "stakingefficiency", "upnp"}) {
        BOOST_CHECK_MESSAGE(IsGuiReadableSetting(name), name);
    }

    for (const char* name : {"rpcpassword", "email", "datadir", "blocknotify", "sidestakeaddresses", ""}) {
        BOOST_CHECK_MESSAGE(!IsGuiReadableSetting(name), name);
    }
}

namespace {
//! Create the data directory, so that a changed setting can be written to the
//! settings file.
void EnsureDataDir()
{
    const fs::path datadir = gArgs.GetArg("-datadir", "");
    BOOST_REQUIRE(!datadir.empty());
    fs::create_directories(datadir);
    gArgs.ClearPathCache();
}

//! Erases the named read-write settings, stored and forced, on exit, so that no
//! case leaves one behind in the settings file.
struct RwSettingsEraser
{
    std::vector<std::string> m_names;

    explicit RwSettingsEraser(std::vector<std::string> names) : m_names(std::move(names)) {}

    ~RwSettingsEraser()
    {
        for (const std::string& name : m_names) {
            (void)updateRwSetting(name, {}, true);
        }
    }
};
} // namespace

// An Elevate-class setting changed through the node interface is stored only
// under the Settings capability the client began, or with a standing full
// unlock; a capability of another kind does not count.
BOOST_AUTO_TEST_CASE(node_changesettings_needs_a_settings_elevation)
{
    EnsureDataDir();

    // Registered as an Elevate-class setting, as util_tests registers it; the
    // registration outlives the case.
    const std::string probe = "elevate_probe";
    if (!gArgs.GetArgFlags("-" + probe)) {
        gArgs.AddArg("-" + probe, "unit-test probe", ArgsManager::ALLOW_ANY, OptionsCategory::HIDDEN);
    }

    const RwSettingsEraser eraser({probe});

    grc_test::EncryptedWalletSwap swap;
    BOOST_REQUIRE(swap.wallet().Unlock(grc_test::EncryptedWalletSwap::Passphrase(), UnlockScope::StakingOnly));

    std::unique_ptr<interfaces::Init> init = interfaces::MakeGridcoinInit();
    std::unique_ptr<interfaces::Wallet> wallet = init->makeWallet();
    std::unique_ptr<interfaces::Node> node = init->makeNode();
    BOOST_REQUIRE(wallet != nullptr);
    BOOST_REQUIRE(node != nullptr);

    interfaces::SettingChangeResult result = node->changeSettings({{probe, "1"}});
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.elevation_required);
    BOOST_CHECK(result.invalid_input);
    BOOST_CHECK(getRwSetting(probe).isNull());

    BOOST_REQUIRE(wallet->beginElevation(grc_test::EncryptedWalletSwap::Passphrase(), CapabilityKind::Send));
    result = node->changeSettings({{probe, "1"}});
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.elevation_required);
    wallet->endElevation();

    BOOST_REQUIRE(wallet->beginElevation(grc_test::EncryptedWalletSwap::Passphrase(), CapabilityKind::Settings));
    result = node->changeSettings({{probe, "1"}});
    BOOST_CHECK(result.ok);
    BOOST_CHECK(!result.elevation_required);
    wallet->endElevation();
    BOOST_CHECK_EQUAL(getRwSetting(probe).write(), "\"1\"");

    result = node->changeSettings({{probe, "2"}});
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.elevation_required);
    BOOST_CHECK_EQUAL(getRwSetting(probe).write(), "\"1\"");

    BOOST_CHECK(swap.wallet().IsUnlockedForStakingOnly());
}

// Every side-stake edit through the interface needs the Settings capability the
// client began, or a standing full unlock. An edit that would change nothing is
// answered without one.
BOOST_AUTO_TEST_CASE(sidestake_edits_need_a_settings_elevation)
{
    grc_test::StateGuard state_guard;
    EnsureDataDir();
    gArgs.ForceSetArg("-enablesidestaking", "1");

    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();
    uint160 hash;
    *(hash.begin()) = 0x61;
    const CTxDestination dest = CKeyID(hash);
    const std::string address = EncodeDestination(dest);

    // Start and end with no entry for the address and no side-stake keys stored.
    struct Cleanup {
        GRC::SideStakeRegistry& registry;
        CTxDestination dest;
        void Run() const
        {
            registry.NonContractDelete(dest, /*save_to_file=*/false);
            BOOST_CHECK(updateRwSettings({{"sidestakeaddresses", util::SettingsValue{}},
                                          {"sidestakeallocations", util::SettingsValue{}},
                                          {"sidestakedescriptions", util::SettingsValue{}}}));
        }
        ~Cleanup() { Run(); }
    } cleanup{registry, dest};
    cleanup.Run();

    grc_test::EncryptedWalletSwap swap;
    CWallet& w = swap.wallet();
    BOOST_REQUIRE(w.Unlock(grc_test::EncryptedWalletSwap::Passphrase(), UnlockScope::StakingOnly));

    std::unique_ptr<interfaces::Init> init = interfaces::MakeGridcoinInit();
    std::unique_ptr<interfaces::Wallet> wallet = init->makeWallet();
    std::unique_ptr<interfaces::SideStakeManager> sidestakes = init->makeSideStakeManager();
    BOOST_REQUIRE(wallet != nullptr);
    BOOST_REQUIRE(sidestakes != nullptr);

    const auto addresses = [&sidestakes] {
        std::vector<std::string> out;
        for (const interfaces::SideStakeEntry& entry : sidestakes->entries().entries) {
            out.push_back(entry.address);
        }
        return out;
    };
    const std::vector<std::string> before = addresses();

    BOOST_CHECK(sidestakes->addLocal(address, 10, "a").status == interfaces::SideStakeEditStatus::ELEVATION_REQUIRED);
    BOOST_CHECK(addresses() == before);
    BOOST_CHECK(registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL).empty());

    // An edit that would change nothing gets its own answer without the
    // passphrase: invalid input, or no entry to edit.
    BOOST_CHECK(sidestakes->addLocal("not an address", 10, "a").status
                == interfaces::SideStakeEditStatus::INVALID_ADDRESS);
    BOOST_CHECK(sidestakes->addLocal(address, 150, "a").status == interfaces::SideStakeEditStatus::INVALID_ALLOCATION);
    BOOST_CHECK(sidestakes->addLocal(address, 10, "a,b").status == interfaces::SideStakeEditStatus::INVALID_DESCRIPTION);
    BOOST_CHECK(sidestakes->setAllocation(address, 20).status == interfaces::SideStakeEditStatus::INVALID_ADDRESS);
    BOOST_CHECK(sidestakes->setDescription(address, "b").status == interfaces::SideStakeEditStatus::INVALID_ADDRESS);
    BOOST_CHECK(sidestakes->deleteLocal(address).status == interfaces::SideStakeEditStatus::INVALID_ADDRESS);
    BOOST_CHECK(addresses() == before);

    BOOST_REQUIRE(wallet->beginElevation(grc_test::EncryptedWalletSwap::Passphrase(), CapabilityKind::Settings));
    BOOST_CHECK(sidestakes->addLocal(address, 10, "a").status == interfaces::SideStakeEditStatus::OK);
    BOOST_CHECK(sidestakes->setDescription(address, "b").status == interfaces::SideStakeEditStatus::OK);
    wallet->endElevation();

    // The same holds for an existing entry: a duplicate, an invalid value, or
    // the value it already has.
    BOOST_CHECK(sidestakes->addLocal(address, 10, "a").status == interfaces::SideStakeEditStatus::DUPLICATE_ADDRESS);
    BOOST_CHECK(sidestakes->setAllocation(address, 150).status == interfaces::SideStakeEditStatus::INVALID_ALLOCATION);
    BOOST_CHECK(sidestakes->setDescription(address, "x,y").status == interfaces::SideStakeEditStatus::INVALID_DESCRIPTION);
    BOOST_CHECK(sidestakes->setAllocation(address, 10).status == interfaces::SideStakeEditStatus::NO_CHANGES);
    BOOST_CHECK(sidestakes->setDescription(address, "b").status == interfaces::SideStakeEditStatus::NO_CHANGES);

    BOOST_CHECK(sidestakes->setAllocation(address, 20).status == interfaces::SideStakeEditStatus::ELEVATION_REQUIRED);
    BOOST_CHECK(sidestakes->deleteLocal(address).status == interfaces::SideStakeEditStatus::ELEVATION_REQUIRED);
    {
        const auto entries = registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL);
        BOOST_REQUIRE_EQUAL(entries.size(), 1u);
        BOOST_CHECK_CLOSE(entries.front()->GetAllocation().ToPercent(), 10.0, 1e-6);
        BOOST_CHECK_EQUAL(entries.front()->GetDescription(), "b");
    }

    // A standing full unlock needs no capability.
    w.Lock();
    BOOST_REQUIRE(w.Unlock(grc_test::EncryptedWalletSwap::Passphrase()));
    BOOST_CHECK(sidestakes->deleteLocal(address).status == interfaces::SideStakeEditStatus::OK);
    BOOST_CHECK(registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL).empty());
    w.Lock();
}

// The other interfaces served on a connection read the slot for that connection
// as the wallet does: once it has closed, the console, PSGT signing and MRC
// claims run under no capability, even when the slot holds the next GUI's
// capability of the right kind.
BOOST_AUTO_TEST_CASE(ipc_closed_connection_interfaces_do_not_use_the_next_guis_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    auto gone_scope = interfaces::MakeConnectionScope();
    std::unique_ptr<interfaces::Node> gone_node = interfaces::MakeNode();
    std::unique_ptr<interfaces::PSGTPoolContext> gone_psgt = interfaces::MakePSGTPoolContext(&fx.wallet);
    std::unique_ptr<interfaces::MRC> gone_mrc = interfaces::MakeMRC(&fx.wallet);
    gone_node->bindConnectionScope(gone_scope);
    gone_psgt->bindConnectionScope(gone_scope);
    gone_mrc->bindConnectionScope(gone_scope);
    gone_scope->close();

    auto next_scope = interfaces::MakeConnectionScope();
    std::unique_ptr<interfaces::Wallet> next = interfaces::MakeWallet(&fx.wallet);
    std::unique_ptr<interfaces::Node> next_node = interfaces::MakeNode();
    next->bindConnectionScope(next_scope);
    next_node->bindConnectionScope(next_scope);

    const std::vector<std::string> args{EncodeDestination(fx.id), "a message"};
    BOOST_REQUIRE(next->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::RpcRequest));
    interfaces::RpcConsoleResult result = gone_node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.wallet_unlock_needed);
    result = next_node->executeRpcConsoleCommand("signmessage", args);
    BOOST_CHECK(result.ok);

    BOOST_REQUIRE(next->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::SignPSGT));
    const interfaces::PSGTSignResult signed_result = gone_psgt->signPSGT(PsgtSpendingTheFixtureCoin(fx));
    BOOST_CHECK(!signed_result.ok);
    BOOST_CHECK_EQUAL(SignaturesOnFirstInput(signed_result.psgt), 0U);

    BOOST_REQUIRE(next->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Contract));
    BOOST_CHECK_EQUAL(gone_mrc->submit(0, false).error, MRC_STAKING_ONLY_REFUSAL);

    next->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

// A settings change and a side-stake edit read the slot for their own connection,
// as the wallet does: once it has closed, they run under no capability, even when
// the slot holds the next GUI's Settings capability.
BOOST_AUTO_TEST_CASE(ipc_closed_connection_settings_do_not_use_the_next_guis_capability)
{
    grc_test::StateGuard state_guard;
    EnsureDataDir();
    gArgs.ForceSetArg("-enablesidestaking", "1");

    const std::string probe = "elevate_probe";
    if (!gArgs.GetArgFlags("-" + probe)) {
        gArgs.AddArg("-" + probe, "unit-test probe", ArgsManager::ALLOW_ANY, OptionsCategory::HIDDEN);
    }
    const RwSettingsEraser eraser({probe});

    GRC::SideStakeRegistry& registry = GRC::GetSideStakeRegistry();
    uint160 hash;
    *(hash.begin()) = 0x62;
    const CTxDestination dest = CKeyID(hash);
    const std::string address = EncodeDestination(dest);

    struct Cleanup {
        GRC::SideStakeRegistry& registry;
        CTxDestination dest;
        void Run() const
        {
            registry.NonContractDelete(dest, /*save_to_file=*/false);
            BOOST_CHECK(updateRwSettings({{"sidestakeaddresses", util::SettingsValue{}},
                                          {"sidestakeallocations", util::SettingsValue{}},
                                          {"sidestakedescriptions", util::SettingsValue{}}}));
        }
        ~Cleanup() { Run(); }
    } cleanup{registry, dest};
    cleanup.Run();

    grc_test::EncryptedWalletSwap swap;
    BOOST_REQUIRE(swap.wallet().Unlock(grc_test::EncryptedWalletSwap::Passphrase(), UnlockScope::StakingOnly));

    auto gone_scope = interfaces::MakeConnectionScope();
    std::unique_ptr<interfaces::Node> gone_node = interfaces::MakeNode();
    std::unique_ptr<interfaces::SideStakeManager> gone_sidestakes = interfaces::MakeSideStakeManager();
    gone_node->bindConnectionScope(gone_scope);
    gone_sidestakes->bindConnectionScope(gone_scope);
    gone_scope->close();

    auto next_scope = interfaces::MakeConnectionScope();
    std::unique_ptr<interfaces::Wallet> next = interfaces::MakeWallet(&swap.wallet());
    std::unique_ptr<interfaces::Node> next_node = interfaces::MakeNode();
    std::unique_ptr<interfaces::SideStakeManager> next_sidestakes = interfaces::MakeSideStakeManager();
    next->bindConnectionScope(next_scope);
    next_node->bindConnectionScope(next_scope);
    next_sidestakes->bindConnectionScope(next_scope);

    BOOST_REQUIRE(next->beginElevation(grc_test::EncryptedWalletSwap::Passphrase(), CapabilityKind::Settings));

    interfaces::SettingChangeResult result = gone_node->changeSettings({{probe, "1"}});
    BOOST_CHECK(!result.ok);
    BOOST_CHECK(result.elevation_required);
    BOOST_CHECK(getRwSetting(probe).isNull());

    BOOST_CHECK(gone_sidestakes->addLocal(address, 10, "a").status
                == interfaces::SideStakeEditStatus::ELEVATION_REQUIRED);
    BOOST_CHECK(registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL).empty());

    // The open connection's own interfaces use it.
    result = next_node->changeSettings({{probe, "1"}});
    BOOST_CHECK(result.ok);
    BOOST_CHECK_EQUAL(getRwSetting(probe).write(), "\"1\"");
    BOOST_CHECK(next_sidestakes->addLocal(address, 10, "a").status == interfaces::SideStakeEditStatus::OK);
    BOOST_CHECK_EQUAL(registry.Try(dest, GRC::SideStake::FilterFlag::LOCAL).size(), 1u);

    next->endElevation();
    BOOST_CHECK(!swap.wallet().IpcCapability({}).has_value());
    BOOST_CHECK(swap.wallet().IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_CASE(node_console_refuses_file_commands)
{
    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    const fs::path in_datadir = GetDataDir() / "x";
    const fs::path in_cwd = fs::current_path() / "x";
    BOOST_REQUIRE(!fs::exists(in_datadir));
    BOOST_REQUIRE(!fs::exists(in_cwd));

    const std::vector<std::pair<std::string, std::vector<std::string>>> calls{
        {"dumpwallet", {"x"}},
        {"importwallet", {"x"}},
        {"dumpcontracts", {"*", "x"}},
        {"parseaccrualsnapshotfile", {"x"}},
    };

    for (const auto& [method, args] : calls) {
        const interfaces::RpcConsoleResult r = node->executeRpcConsoleCommand(method, args);
        BOOST_CHECK_MESSAGE(!r.ok, method);
        BOOST_CHECK_MESSAGE(r.output.find("moves a file") != std::string::npos, method + ": " + r.output);
        BOOST_CHECK_MESSAGE(!fs::exists(in_datadir), method);
        BOOST_CHECK_MESSAGE(!fs::exists(in_cwd), method);
    }

    // help for a file command takes no path and still works, as does an ordinary command.
    BOOST_CHECK(node->executeRpcConsoleCommand("help", {"dumpwallet"}).ok);
    BOOST_CHECK(node->executeRpcConsoleCommand("getblockcount", {}).ok);
}

// The GUI backs up through beginBackup(): the core snapshots into its own
// transfer directory and streams the copies. The file-backed cases copy the
// fixture's pwalletMain, whose mock database never reads the placeholder that
// stands in for its file.
namespace {

const uint8_t BACKUP_WALLET = static_cast<uint8_t>(interfaces::BackupItem::Wallet);
const uint8_t BACKUP_CONFIG = static_cast<uint8_t>(interfaces::BackupItem::Config);
const uint8_t BACKUP_SETTINGS = static_cast<uint8_t>(interfaces::BackupItem::Settings);

//! A directory of the case's own under the data directory, and the transfer
//! directory. Both are removed when the case ends, and the transfer directory
//! also when it begins.
struct BackupCase
{
    const fs::path dir = GetDataDir() / "interfaces_tests_backup";

    BackupCase()
    {
        RemoveTransferDir();
        fs::create_directories(dir);
    }

    ~BackupCase()
    {
        boost::system::error_code ec;
        fs::remove_all(dir, ec);
        RemoveTransferDir();
        // A case that fails before beginBackup() ends it must not leave
        // pwalletMain's IPC slot holding a capability.
        pwalletMain->EndIpcElevation({});
    }

    static void RemoveTransferDir()
    {
        boost::system::error_code ec;
        fs::remove_all(GetDataDir() / "transfers", ec);
    }

    //! Files in the transfer directory, or 0 when it does not exist.
    static size_t TransferFileCount()
    {
        const fs::path tdir = GetDataDir() / "transfers";
        if (!fs::exists(tdir)) return 0;

        size_t n = 0;
        for (fs::directory_iterator it(tdir); it != fs::directory_iterator(); ++it) {
            ++n;
        }
        return n;
    }

    static void Write(const fs::path& path, const std::string& text)
    {
        fsbridge::ofstream stream(path);
        stream << text;
    }
};

//! Makes an in-memory wallet read as file-backed, with \p name as its file, for
//! a case, and in-memory again when the case ends. EncryptWallet on a
//! file-backed wallet rewrites the wallet file, which this binary's mock
//! database refuses, so a case encrypts an in-memory wallet first and then
//! names it after the placeholder file. Nothing in such a case writes to the
//! wallet's database.
struct ReadAsFileBacked
{
    CWallet& m_wallet;

    ReadAsFileBacked(CWallet& wallet, const std::string& name) : m_wallet(wallet)
    {
        m_wallet.strWalletFile = name;
        m_wallet.fFileBacked = true;
    }

    ~ReadAsFileBacked()
    {
        m_wallet.fFileBacked = false;
        m_wallet.strWalletFile.clear();
    }

    ReadAsFileBacked(const ReadAsFileBacked&) = delete;
    ReadAsFileBacked& operator=(const ReadAsFileBacked&) = delete;
};

//! Read an entry to its end, requiring every chunk to be error-free and the
//! trailer to be there.
std::string ReadEntry(interfaces::FileSource& source, uint32_t entry)
{
    std::string all;

    // Bounds a source that never reaches eof.
    for (int i = 0; i < 1000; ++i) {
        const interfaces::FileChunk chunk = source.read(entry, 4096);
        BOOST_REQUIRE_MESSAGE(chunk.error.empty(), chunk.error);
        all.append(chunk.data.begin(), chunk.data.end());
        if (chunk.eof) {
            BOOST_REQUIRE(source.trailer(entry).ok);
            return all;
        }
    }

    BOOST_FAIL("the source never reached the end of the entry");
    return all;
}

} // namespace

BOOST_FIXTURE_TEST_CASE(begin_backup_refused_while_staking_only_without_capability, BackupCase)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);

    std::string err;
    BOOST_CHECK(!w->beginBackup(BACKUP_WALLET, err));
    BOOST_CHECK_MESSAGE(err.find("passphrase") != std::string::npos, err);
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
}

BOOST_FIXTURE_TEST_CASE(begin_backup_with_capability_passes_the_gate, BackupCase)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Backup));

    // The in-memory wallet passes the gate, and the next step reports that it
    // has no file to back up.
    std::string err;
    BOOST_CHECK(!w->beginBackup(BACKUP_WALLET, err));
    BOOST_CHECK_MESSAGE(err.find("file-backed") != std::string::npos, err);

    // A capability authorises one operation, and that attempt was it, failed or
    // not.
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    // A capability of another kind does not pass the gate, and the refusal
    // leaves it in place.
    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    const std::optional<CapabilityId> send_id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(send_id.has_value());
    BOOST_CHECK(!w->beginBackup(BACKUP_WALLET, err));
    BOOST_CHECK_MESSAGE(err.find("passphrase") != std::string::npos, err);
    BOOST_CHECK(fx.wallet.IpcCapability({}) == send_id);

    w->endElevation();
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_FIXTURE_TEST_CASE(begin_backup_unencrypted_streams_wallet_and_ends_capability, BackupCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    // A settings file of the case's own, so the settings entry has bytes to send.
    grc_test::ForcedArgGuard settings_arg("settings", "interfaces_tests_settings.json");
    fs::path settings;
    BOOST_REQUIRE(gArgs.GetSettingsPath(&settings));
    const std::string settings_text = "{\"interfaces_tests\": true}\n";
    Write(settings, settings_text);
    struct RemoveOnExit
    {
        fs::path path;
        ~RemoveOnExit()
        {
            boost::system::error_code ec;
            fs::remove(path, ec);
        }
    } remove_settings{settings};

    // An unencrypted wallet still gets an id, which beginBackup() ends.
    BOOST_REQUIRE(pwalletMain->BeginIpcElevation(SecureString{}, CapabilityKind::Backup, {}));
    const std::optional<CapabilityId> id = pwalletMain->IpcCapability({});
    BOOST_REQUIRE(id.has_value());

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginBackup(BACKUP_WALLET | BACKUP_SETTINGS, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_CHECK(err.empty());

    // The capability is gone as soon as the snapshots exist.
    {
        const CapabilityScope after(*pwalletMain, id, CapabilityKind::Backup);
        BOOST_CHECK(!after.Active());
    }
    BOOST_CHECK(!pwalletMain->IpcCapability({}).has_value());

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 2U);
    BOOST_CHECK_EQUAL(infos[0].name, pwalletMain->strWalletFile);
    BOOST_CHECK_EQUAL(infos[0].size, static_cast<int64_t>(grc_test::WalletFilePlaceholder::CONTENT.size()));
    BOOST_CHECK_EQUAL(infos[1].name, "interfaces_tests_settings.json");

    BOOST_CHECK_EQUAL(ReadEntry(*source, 0), grc_test::WalletFilePlaceholder::CONTENT);
    BOOST_CHECK_EQUAL(ReadEntry(*source, 1), settings_text);
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);

    // The originals are untouched.
    BOOST_CHECK(fs::exists(wallet_file.m_path));
    BOOST_CHECK(fs::exists(settings));
}

// A locked, encrypted, file-backed wallet: the Backup capability unlocks it for
// staking only, the copy is made under it, and ending the capability once the
// copy exists locks the wallet again, on the calling thread and while that
// thread's scope for the capability is still open.
BOOST_FIXTURE_TEST_CASE(begin_backup_locked_wallet_relocks_once_the_copy_exists, BackupCase)
{
    // Read before EncryptedWallet swaps pwalletMain.
    const std::string file_name = pwalletMain->strWalletFile;
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    EncryptedWallet fx;
    const ReadAsFileBacked file_backed(fx.wallet, file_name);
    // After the wallet, so it is destroyed first (see SchedulerForThisCase).
    grc_test::SchedulerForThisCase scheduler;
    BOOST_REQUIRE(fx.wallet.IsLocked());

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Backup));
    BOOST_REQUIRE(fx.wallet.IsUnlockedForStakingOnly());

    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginBackup(BACKUP_WALLET, err);
    BOOST_REQUIRE_MESSAGE(source, err);

    BOOST_CHECK(fx.wallet.IsLocked());
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    // The copy streams with the wallet locked.
    BOOST_CHECK_EQUAL(ReadEntry(*source, 0), grc_test::WalletFilePlaceholder::CONTENT);
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
}

// On a fully unlocked wallet the backup needs no capability, so it ends none:
// one of another kind the client holds is left for its own operation.
BOOST_FIXTURE_TEST_CASE(begin_backup_on_a_full_unlock_leaves_another_kinds_capability, BackupCase)
{
    const std::string file_name = pwalletMain->strWalletFile;
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    EncryptedWallet fx;
    const ReadAsFileBacked file_backed(fx.wallet, file_name);
    fx.UnlockFor(UnlockScope::Full);

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    const std::optional<CapabilityId> send_id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(send_id.has_value());

    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginBackup(BACKUP_WALLET, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_CHECK_EQUAL(ReadEntry(*source, 0), grc_test::WalletFilePlaceholder::CONTENT);

    BOOST_CHECK(fx.wallet.IpcCapability({}) == send_id);
    {
        const CapabilityScope send(fx.wallet, send_id, CapabilityKind::Send);
        BOOST_CHECK(send.Active());
    }

    w->endElevation();
}

BOOST_FIXTURE_TEST_CASE(begin_backup_config_refused_in_service_mode, BackupCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    // This account's own configuration file: the attach group alone refuses it.
    const fs::path conf = dir / "service.conf";
    Write(conf, "# interfaces_tests\n");
    grc_test::ForcedArgGuard conf_arg("conf", conf.string());
    grc_test::ForcedArgGuard group("ipcattachgroup", "x");

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);

    std::string reason;
    BOOST_REQUIRE(!wallet::ConfigBackupPermitted(reason));

    // The refusal does not fail the backup: the configuration file is an entry
    // with nothing to send and the reason as its note, and the wallet arrives.
    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginBackup(BACKUP_WALLET | BACKUP_CONFIG, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_CHECK(err.empty());

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 2U);
    BOOST_CHECK_EQUAL(infos[1].name, "service.conf");
    BOOST_CHECK_EQUAL(infos[1].size, 0);
    BOOST_CHECK_EQUAL(infos[1].note, reason);
    BOOST_CHECK_MESSAGE(infos[1].note.find("settings file") != std::string::npos, infos[1].note);

    // No byte of the configuration file was copied, and none is sent.
    BOOST_CHECK_EQUAL(TransferFileCount(), 1U);
    const interfaces::FileChunk refused = source->read(1, 4096);
    BOOST_CHECK_EQUAL(refused.error, reason);
    BOOST_CHECK(refused.data.empty());
    BOOST_CHECK_EQUAL(ReadEntry(*source, 0), grc_test::WalletFilePlaceholder::CONTENT);

    // One transfer at a time: the next begins once this source is gone.
    source.reset();

    err.clear();
    source = w->beginBackup(BACKUP_WALLET | BACKUP_SETTINGS, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_REQUIRE_EQUAL(source->entries().size(), 2U);
    BOOST_CHECK_EQUAL(ReadEntry(*source, 0), grc_test::WalletFilePlaceholder::CONTENT);
}

BOOST_FIXTURE_TEST_CASE(begin_backup_lists_a_missing_file_with_a_note, BackupCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    grc_test::ForcedArgGuard conf_arg("conf", (dir / "missing.conf").string());
    grc_test::ForcedArgGuard settings_arg("settings", "interfaces_tests_missing.json");

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    std::string err;
    std::unique_ptr<interfaces::FileSource> source =
        w->beginBackup(BACKUP_WALLET | BACKUP_CONFIG | BACKUP_SETTINGS, err);
    BOOST_REQUIRE_MESSAGE(source, err);

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 3U);
    BOOST_CHECK_EQUAL(infos[1].name, "missing.conf");
    BOOST_CHECK_EQUAL(infos[1].size, 0);
    BOOST_CHECK_EQUAL(infos[1].note, "no configuration file");
    BOOST_CHECK_EQUAL(infos[2].name, "interfaces_tests_missing.json");
    BOOST_CHECK_EQUAL(infos[2].note, "no settings file");

    BOOST_CHECK_EQUAL(source->read(1, 4096).error, "no configuration file");
    BOOST_CHECK_EQUAL(source->read(2, 4096).error, "no settings file");

    // Only the wallet was snapshotted, and it still streams.
    BOOST_CHECK_EQUAL(TransferFileCount(), 1U);
    BOOST_CHECK_EQUAL(ReadEntry(*source, 0), grc_test::WalletFilePlaceholder::CONTENT);
}

BOOST_FIXTURE_TEST_CASE(begin_backup_failure_removes_the_snapshots_made, BackupCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    // -conf names a directory: the wallet is snapshotted first, and then the
    // configuration file cannot be.
    const fs::path conf_dir = dir / "conf.d";
    fs::create_directories(conf_dir);
    grc_test::ForcedArgGuard conf_arg("conf", conf_dir.string());

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    std::string err;
    BOOST_CHECK(!w->beginBackup(BACKUP_WALLET | BACKUP_CONFIG, err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);

    // The failed attempt left no transfer live.
    err.clear();
    BOOST_CHECK_MESSAGE(w->beginBackup(BACKUP_WALLET, err), err);
}

BOOST_FIXTURE_TEST_CASE(begin_backup_snapshot_goes_with_the_connection, BackupCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    auto scope = interfaces::MakeConnectionScope();
    w->bindConnectionScope(scope);

    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginBackup(BACKUP_WALLET, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_CHECK_EQUAL(TransferFileCount(), 1U);

    scope->close();
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
    BOOST_CHECK(!source->read(0, 4096).error.empty());
}

BOOST_FIXTURE_TEST_CASE(old_backup_methods_always_false, BackupCase)
{
    // A wallet file and a configuration file that a copy would succeed with.
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();
    const fs::path conf = dir / "old.conf";
    Write(conf, "# interfaces_tests\n");
    grc_test::ForcedArgGuard conf_arg("conf", conf.string());

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    const fs::path wallet_dest = dir / "old-wallet.dat";
    const fs::path conf_dest = dir / "old-copy.conf";

    BOOST_CHECK(!w->backupWallet(wallet_dest.string()));
    BOOST_CHECK(!w->backupConfigFile(conf_dest.string()));
    BOOST_CHECK(!fs::exists(wallet_dest));
    BOOST_CHECK(!fs::exists(conf_dest));
}

BOOST_FIXTURE_TEST_CASE(second_transfer_refused_while_one_is_live, BackupCase)
{
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    std::string err;

    std::unique_ptr<interfaces::FileSource> first = w->beginBackup(BACKUP_WALLET, err);
    BOOST_REQUIRE_MESSAGE(first, err);

    std::unique_ptr<interfaces::FileSource> second = w->beginBackup(BACKUP_WALLET, err);
    BOOST_CHECK(!second);
    BOOST_CHECK_EQUAL(err, "a transfer is already in progress");
    BOOST_CHECK_EQUAL(TransferFileCount(), 1U);

    // Still live once read to its end and cancelled: what ends it is the source
    // going away.
    BOOST_CHECK_EQUAL(ReadEntry(*first, 0), grc_test::WalletFilePlaceholder::CONTENT);
    first->cancel();
    BOOST_CHECK(!w->beginBackup(BACKUP_WALLET, err));

    first.reset();
    err.clear();
    std::unique_ptr<interfaces::FileSource> third = w->beginBackup(BACKUP_WALLET, err);
    BOOST_CHECK_MESSAGE(third, err);
}

// The console's file commands run through the transfer channel: the core
// generates or consumes the data in a stream under the client's FileTransfer
// capability, and no path the client names reaches the core.
namespace {

const std::string FILE_TRANSFER_REFUSAL{"Enter the wallet passphrase to transfer files."};

//! An accrual snapshot file of one record, as the accrual repository writes it.
fs::path WriteSnapshotFile(const fs::path& dir)
{
    const fs::path path = dir / "5000000";
    GRC::AccrualSnapshotWriter writer(path);
    writer.WriteHeader(5000000);
    writer.WriteRecord(GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294"), 12345);
    return path;
}

//! An in-memory wallet keeps key pool entries with no database to read them
//! from, so a dump of it (GetAllReserveKeys()) would fail; a dump case on one
//! drops the pool first. A node's wallet is file-backed.
void DropKeyPool(CWallet& wallet)
{
    LOCK(wallet.cs_wallet);
    wallet.setKeyPool.clear();
}

//! Whether capability \p id still lets a FileTransfer scope be opened on \p wallet.
bool FileTransferLive(CWallet& wallet, const std::optional<CapabilityId>& id)
{
    const CapabilityScope scope(wallet, id, CapabilityKind::FileTransfer);
    return scope.Active();
}

} // namespace

BOOST_FIXTURE_TEST_CASE(console_download_refused_without_capability_while_staking_only, BackupCase)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);

    std::string err;
    BOOST_CHECK(!w->beginConsoleDownload("dumpwallet", {}, err));
    BOOST_CHECK_EQUAL(err, FILE_TRANSFER_REFUSAL);
    err.clear();
    BOOST_CHECK(!w->beginConsoleUpload("importwallet", {}, 10, err));
    BOOST_CHECK_EQUAL(err, FILE_TRANSFER_REFUSAL);

    // A capability of another kind does not pass the gate, and the refusal
    // leaves it in the slot.
    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Backup));
    const std::optional<CapabilityId> backup_id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(backup_id.has_value());

    err.clear();
    BOOST_CHECK(!w->beginConsoleDownload("dumpwallet", {}, err));
    BOOST_CHECK_EQUAL(err, FILE_TRANSFER_REFUSAL);
    BOOST_CHECK(fx.wallet.IpcCapability({}) == backup_id);

    w->endElevation();
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
}

// An unencrypted wallet can use its keys, so its console transfers need no
// capability: the dump streams and the import adds the key with the slot empty,
// which they leave empty.
BOOST_FIXTURE_TEST_CASE(console_transfer_on_an_unencrypted_wallet_needs_no_capability, BackupCase)
{
    const grc_test::LoneBestBlock tip;

    CWallet plain;
    CKey own;
    own.MakeNewKey(true);
    {
        LOCK(plain.cs_wallet);
        BOOST_REQUIRE(plain.AddKey(own));
    }
    BOOST_REQUIRE(!plain.IsCrypted());
    BOOST_REQUIRE(!plain.IpcCapability({}).has_value());

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&plain);
    w->bindConnectionScope(interfaces::MakeConnectionScope());

    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginConsoleDownload("dumpwallet", {}, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_CHECK(ReadEntry(*source, 0).find(EncodeSecret(own) + " ") != std::string::npos);
    source.reset();

    CKey fresh;
    fresh.MakeNewKey(true);
    const std::string dump = EncodeSecret(fresh) + " 2026-01-01T00:00:00Z label=plain\n";
    const std::vector<unsigned char> data(dump.begin(), dump.end());
    uint256 sha;
    CSHA256().Write(data.data(), data.size()).Finalize(sha.begin());

    err.clear();
    std::unique_ptr<interfaces::FileSink> sink = w->beginConsoleUpload("importwallet", {}, data.size(), err);
    BOOST_REQUIRE_MESSAGE(sink, err);
    BOOST_REQUIRE(sink->write(data));
    const interfaces::FileSinkResult result = sink->finish(data.size(), sha);
    BOOST_CHECK_MESSAGE(result.ok, result.error);
    BOOST_CHECK(plain.HaveKey(fresh.GetPubKey().GetID()));

    BOOST_CHECK(!plain.IpcCapability({}).has_value());
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
}

// On a fully unlocked wallet the transfer needs no capability, so it ends none:
// one of another kind the client holds is left for its own operation.
BOOST_FIXTURE_TEST_CASE(console_transfer_on_a_full_unlock_leaves_another_kinds_capability, BackupCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::Full);
    DropKeyPool(fx.wallet);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    const std::optional<CapabilityId> send_id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(send_id.has_value());

    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginConsoleDownload("dumpwallet", {}, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_CHECK(ReadEntry(*source, 0).find(EncodeSecret(fx.key) + " ") != std::string::npos);
    source.reset();

    BOOST_CHECK(fx.wallet.IpcCapability({}) == send_id);
    {
        const CapabilityScope send(fx.wallet, send_id, CapabilityKind::Send);
        BOOST_CHECK(send.Active());
    }

    w->endElevation();
}

BOOST_AUTO_TEST_CASE(console_download_rejects_non_file_command)
{
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    std::string err;

    BOOST_CHECK(!w->beginConsoleDownload("getinfo", {}, err));
    BOOST_CHECK_EQUAL(err, "not a file command");

    // The wrong direction.
    err.clear();
    BOOST_CHECK(!w->beginConsoleDownload("importwallet", {}, err));
    BOOST_CHECK_EQUAL(err, "not a file command");
    err.clear();
    BOOST_CHECK(!w->beginConsoleUpload("dumpwallet", {}, 10, err));
    BOOST_CHECK_EQUAL(err, "not a file command");
    err.clear();
    BOOST_CHECK(!w->beginConsoleUpload("getblockcount", {}, 10, err));
    BOOST_CHECK_EQUAL(err, "not a file command");

    // dumpcontracts takes its contract type before the file.
    err.clear();
    BOOST_CHECK(!w->beginConsoleDownload("dumpcontracts", {}, err));
    BOOST_CHECK_MESSAGE(err.find("argument") != std::string::npos, err);
}

BOOST_AUTO_TEST_CASE(console_upload_refuses_too_large)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);

    for (const char* method : {"importwallet", "parseaccrualsnapshotfile"}) {
        const interfaces::ConsoleFileCommand* command = interfaces::FindConsoleFileCommand(method);
        BOOST_REQUIRE(command);

        std::string err;
        BOOST_CHECK(!w->beginConsoleUpload(method, {}, command->max_upload_bytes + 1, err));
        BOOST_CHECK_MESSAGE(err.find("too large") != std::string::npos, method << ": " << err);

        // At the cap it is not too large; this wallet, unlocked for staking only,
        // refuses it for want of a capability.
        err.clear();
        BOOST_CHECK(!w->beginConsoleUpload(method, {}, command->max_upload_bytes, err));
        BOOST_CHECK_EQUAL(err, FILE_TRANSFER_REFUSAL);
    }
}

// A console file command with more arguments than its RPC takes is refused
// before anything else, as the RPC refuses it. Each row's maximum is its RPC's
// own arity, the file included.
BOOST_AUTO_TEST_CASE(console_transfer_refuses_too_many_arguments)
{
    const std::vector<std::pair<const char*, const RPCHelpMan& (*)()>> rows{
        {"dumpwallet", &dumpwallet_helpman},
        {"importwallet", &importwallet_helpman},
        {"dumpcontracts", &dumpcontracts_helpman},
        {"parseaccrualsnapshotfile", &parseaccrualsnapshotfile_helpman},
    };
    for (const auto& [method, helpman] : rows) {
        const interfaces::ConsoleFileCommand* command = interfaces::FindConsoleFileCommand(method);
        BOOST_REQUIRE(command);
        BOOST_CHECK_MESSAGE(command->max_args == helpman().GetArgs().size(), method);
    }

    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    std::string err;

    BOOST_CHECK(!w->beginConsoleDownload("dumpwallet", {"extra"}, err));
    BOOST_CHECK_MESSAGE(err.find("at most 1 argument") != std::string::npos, err);

    err.clear();
    BOOST_CHECK(!w->beginConsoleUpload("importwallet", {"extra"}, 10, err));
    BOOST_CHECK_MESSAGE(err.find("at most 1 argument") != std::string::npos, err);

    err.clear();
    BOOST_CHECK(!w->beginConsoleUpload("parseaccrualsnapshotfile", {"extra"}, 10, err));
    BOOST_CHECK_MESSAGE(err.find("at most 1 argument") != std::string::npos, err);

    err.clear();
    BOOST_CHECK(!w->beginConsoleDownload("dumpcontracts", {"beacon", "true", "0", "100", "7"}, err));
    BOOST_CHECK_MESSAGE(err.find("at most 5 argument") != std::string::npos, err);

}

// dumpcontracts exports public chain data, so over the transfer channel it runs with no
// passphrase: on a locked wallet and on one unlocked for staking only it needs no
// capability, and it neither uses nor ends one the client holds. dumpwallet and
// parseaccrualsnapshotfile on the same wallet are still refused without one.
BOOST_FIXTURE_TEST_CASE(console_dumpcontracts_needs_no_passphrase, BackupCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet fx;
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    w->bindConnectionScope(interfaces::MakeConnectionScope());

    for (const bool staking_only : {false, true}) {
        if (staking_only) fx.UnlockFor(UnlockScope::StakingOnly);
        BOOST_REQUIRE(!fx.wallet.CanUseKeys());

        std::string err;
        std::unique_ptr<interfaces::FileSource> source =
            w->beginConsoleDownload("dumpcontracts", {"beacon", "true"}, err);
        BOOST_REQUIRE_MESSAGE(source, err);
        BOOST_CHECK_EQUAL(source->entries().size(), 1U);
        source.reset();
        BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

        err.clear();
        BOOST_CHECK(!w->beginConsoleDownload("dumpwallet", {}, err));
        BOOST_CHECK_EQUAL(err, FILE_TRANSFER_REFUSAL);
        err.clear();
        BOOST_CHECK(!w->beginConsoleUpload("parseaccrualsnapshotfile", {}, 10, err));
        BOOST_CHECK_EQUAL(err, FILE_TRANSFER_REFUSAL);
    }

    // A FileTransfer capability the client holds is left as it is.
    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer));
    const std::optional<CapabilityId> held = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(held.has_value());
    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginConsoleDownload("dumpcontracts", {"beacon"}, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    source.reset();
    BOOST_CHECK(fx.wallet.IpcCapability({}) == held);
    BOOST_CHECK(FileTransferLive(fx.wallet, held));
    w->endElevation();

    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
}

// A console dumpcontracts with a height that is not on the chain reports the RPC's
// refusal and leaves no file in the transfer directory.
BOOST_FIXTURE_TEST_CASE(console_dumpcontracts_refuses_a_height_off_the_chain, BackupCase)
{
    const grc_test::LoneBestBlock tip; // one block, at height 0

    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(pwalletMain);
    w->bindConnectionScope(interfaces::MakeConnectionScope());

    // A high_height above the tip comes last: before the fix it crashed the process.
    const std::vector<std::pair<std::vector<std::string>, std::string>> cases{
        {{"beacon", "true", "1"}, "low_height 1"},
        {{"beacon", "true", "-1"}, "low_height -1"},
        {{"beacon", "false", "0", "100"}, "high_height 100"},
    };
    for (const auto& [args, named] : cases) {
        std::string err;
        BOOST_CHECK(!w->beginConsoleDownload("dumpcontracts", args, err));
        BOOST_CHECK_MESSAGE(err.find(named) != std::string::npos, err);
        BOOST_CHECK_MESSAGE(err.find("0 to 0") != std::string::npos, err);
        BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
    }
}

// dumpwallet: the source holds the capability while it streams the dump, and
// ends it at the dump's end. Nothing goes through the core's disk.
BOOST_FIXTURE_TEST_CASE(console_download_streams_the_dump_and_ends_capability, BackupCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    DropKeyPool(fx.wallet);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    w->bindConnectionScope(interfaces::MakeConnectionScope());

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer));
    const std::optional<CapabilityId> id = fx.wallet.IpcCapability({});
    BOOST_REQUIRE(id.has_value());

    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginConsoleDownload("dumpwallet", {}, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_CHECK(err.empty());

    // Held for the run.
    BOOST_CHECK(FileTransferLive(fx.wallet, id));

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 1U);
    BOOST_CHECK_EQUAL(infos[0].name, "wallet-dump.txt");

    const std::string dump = ReadEntry(*source, 0);
    BOOST_CHECK(dump.find(EncodeSecret(fx.key) + " ") != std::string::npos);
    BOOST_CHECK(dump.find("# End of dump\n") != std::string::npos);

    // Ended, and taken out of the slot, so no ended id is left there.
    BOOST_CHECK(!FileTransferLive(fx.wallet, id));
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);

    w->endElevation();
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_FIXTURE_TEST_CASE(console_download_goes_with_the_connection, BackupCase)
{
    const grc_test::LoneBestBlock tip;

    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    DropKeyPool(fx.wallet);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    const auto scope = interfaces::MakeConnectionScope();
    w->bindConnectionScope(scope);

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer));
    const std::optional<CapabilityId> id = fx.wallet.IpcCapability({});

    std::string err;
    std::unique_ptr<interfaces::FileSource> source = w->beginConsoleDownload("dumpwallet", {}, err);
    BOOST_REQUIRE_MESSAGE(source, err);
    BOOST_REQUIRE(source->read(0, 64).error.empty());

    scope->close();
    BOOST_CHECK(!FileTransferLive(fx.wallet, id));
    BOOST_CHECK(!source->read(0, 64).error.empty());
}

// parseaccrualsnapshotfile: the upload goes to a core temporary file, which the
// command reads at finish and which is then gone. The command reaches no key,
// so the capability that let the transfer begin is ended as it begins.
BOOST_FIXTURE_TEST_CASE(console_upload_parses_a_snapshot_and_ends_capability, BackupCase)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    w->bindConnectionScope(interfaces::MakeConnectionScope());

    const fs::path local = WriteSnapshotFile(dir);
    const uint64_t size = fs::file_size(local);

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer));
    const std::optional<CapabilityId> id = fx.wallet.IpcCapability({});

    std::string err;
    std::unique_ptr<interfaces::FileSink> sink = w->beginConsoleUpload("parseaccrualsnapshotfile", {}, size, err);
    BOOST_REQUIRE_MESSAGE(sink, err);

    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK(!FileTransferLive(fx.wallet, id));

    const std::atomic<bool> cancel{false};
    const interfaces::FileSinkResult result = interfaces::UploadFile(*sink, local, {}, cancel, 7);
    BOOST_CHECK_MESSAGE(result.ok, result.error);
    BOOST_CHECK_MESSAGE(result.output.find("\"height\": 5000000") != std::string::npos, result.output);
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);

    // The file named on the client's side is untouched.
    BOOST_CHECK_EQUAL(fs::file_size(local), size);
}

// An upload the core refuses partway, here for passing the size it announced,
// ends with the core's reason, not only that the data was refused. Nothing is
// left in the transfer directory.
BOOST_FIXTURE_TEST_CASE(console_upload_refused_partway_reports_the_reason, BackupCase)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);
    w->bindConnectionScope(interfaces::MakeConnectionScope());

    const fs::path local = WriteSnapshotFile(dir);
    const uint64_t size = fs::file_size(local);
    BOOST_REQUIRE(size > 7);

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer));

    std::string err;
    std::unique_ptr<interfaces::FileSink> sink =
        w->beginConsoleUpload("parseaccrualsnapshotfile", {}, size - 1, err);
    BOOST_REQUIRE_MESSAGE(sink, err);

    const std::atomic<bool> cancel{false};
    const interfaces::FileSinkResult result = interfaces::UploadFile(*sink, local, {}, cancel, 7);
    BOOST_CHECK(!result.ok);
    BOOST_CHECK_EQUAL(result.error, "the upload is larger than its announced size");
    BOOST_CHECK(result.output.empty());

    sink.reset();
    BOOST_CHECK_EQUAL(TransferFileCount(), 0U);
}

BOOST_FIXTURE_TEST_CASE(second_console_transfer_refused_while_one_is_live, BackupCase)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::unique_ptr<interfaces::Wallet> w = interfaces::MakeWallet(&fx.wallet);

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer));
    const std::optional<CapabilityId> id = fx.wallet.IpcCapability({});

    std::string err;
    std::unique_ptr<interfaces::FileSink> first = w->beginConsoleUpload("importwallet", {}, 100, err);
    BOOST_REQUIRE_MESSAGE(first, err);

    err.clear();
    BOOST_CHECK(!w->beginConsoleUpload("importwallet", {}, 100, err));
    BOOST_CHECK_EQUAL(err, "a transfer is already in progress");
    err.clear();
    BOOST_CHECK(!w->beginConsoleDownload("dumpwallet", {}, err));
    BOOST_CHECK_EQUAL(err, "a transfer is already in progress");
    err.clear();
    BOOST_CHECK(!w->beginBackup(BACKUP_WALLET, err));
    BOOST_CHECK_EQUAL(err, "a transfer is already in progress");

    // The sink holds the capability until it goes away, and then the next
    // transfer can begin. It takes the capability out of the slot as it ends it.
    BOOST_CHECK(FileTransferLive(fx.wallet, id));
    first.reset();
    BOOST_CHECK(!FileTransferLive(fx.wallet, id));
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    BOOST_REQUIRE(w->beginElevation(EncryptedWallet::PASSPHRASE, CapabilityKind::FileTransfer));
    err.clear();
    std::unique_ptr<interfaces::FileSink> second = w->beginConsoleUpload("importwallet", {}, 100, err);
    BOOST_CHECK_MESSAGE(second, err);
    second.reset();

    w->endElevation();
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_SUITE_END()
