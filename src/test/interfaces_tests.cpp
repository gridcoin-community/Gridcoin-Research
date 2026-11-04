// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chain.h"
#include "clientversion.h"
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
#include "node/ui_interface.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/sidestake.h"
#include "primitives/transaction.h"
#include "psgt.h"
#include "sync.h"
#include "test/encrypted_wallet.h"
#include "test/gui_cache_fakes.h"
#include "test/psgt_test_helpers.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "txmempool.h"
#include "util.h"
#include "util/strencodings.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>
#include "consensus/consensus.h"

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

    const int64_t saved_transaction_fee{nTransactionFee};
    nTransactionFee = 123456;

    std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(pwalletMain);
    BOOST_CHECK_EQUAL(wallet->getTransactionFeeRate(), 123456);

    nTransactionFee = saved_transaction_fee;
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
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    // A wrong passphrase is refused and counted.
    BOOST_CHECK(!wallet->beginElevation(SecureString("not the passphrase"), CapabilityKind::Send));
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);

    // Ending with nothing begun is a no-op.
    wallet->endElevation();
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
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
    const std::string probe = "k3_elevate_probe";
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

BOOST_AUTO_TEST_SUITE_END()
