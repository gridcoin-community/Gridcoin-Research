// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/node.h"
#include "interfaces/wallet.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "sync.h"
#include "test/autounlock_fake_store.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "wallet/autounlock_setup.h"
#include "wallet/autounlock_store.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>
#include <univalue.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace {

using grc_test::EncryptedWallet;
using grc_test::FakeStoreInstalled;
using Outcome = grc_test::FakeAutoUnlockStore::Outcome;

const SecureString& Passphrase()
{
    return EncryptedWallet::PASSPHRASE;
}

//! The RPC error \p method throws for \p passphrase through the dispatcher, as
//! {code, message}; nullopt, with the result in \p result_out, when it returns.
std::optional<std::pair<int, std::string>> RpcError(const std::string& method, const SecureString& passphrase,
                                                    UniValue* result_out = nullptr)
{
    UniValue params(UniValue::VARR);
    params.push_back(std::string(passphrase.begin(), passphrase.end()));

    try {
        const UniValue result = tableRPC.execute(method, params);
        if (result_out) *result_out = result;
    } catch (const UniValue& error) {
        return std::make_pair(find_value(error, "code").get_int(), find_value(error, "message").get_str());
    }

    return std::nullopt;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(autounlock_setup_tests, grc_test::StateGuard)

BOOST_AUTO_TEST_CASE(setup_wrong_passphrase_stores_nothing)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/false);
    std::string error;

    // Counted and delayed as any wrong passphrase is: FailedPassphraseDelay(1), half a second.
    BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, SecureString("not the passphrase"), error)
                == AutoUnlockSetupResult::WrongPassphrase);
    BOOST_CHECK(!error.empty());
    BOOST_CHECK_EQUAL(fake.calls->stores, 0);
    BOOST_CHECK(!fake.calls->kept.has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);

    // Turning it off with a wrong passphrase removes nothing and leaves the wallet unlocked.
    FakeStoreInstalled configured(/*configured=*/true);
    BOOST_CHECK(ClearAutoUnlockSecret(fx.wallet, SecureString("not the passphrase"), error)
                == AutoUnlockSetupResult::WrongPassphrase);
    BOOST_CHECK_EQUAL(configured.calls->removes, 0);
    BOOST_CHECK(configured.calls->kept.has_value());
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
}

BOOST_AUTO_TEST_CASE(setup_unencrypted_not_encrypted)
{
    CWallet wallet;
    FakeStoreInstalled fake(/*configured=*/true);
    std::string error;

    BOOST_CHECK(SetAutoUnlockSecret(wallet, Passphrase(), error) == AutoUnlockSetupResult::NotEncrypted);
    BOOST_CHECK(!error.empty());
    BOOST_CHECK(ClearAutoUnlockSecret(wallet, Passphrase(), error) == AutoUnlockSetupResult::NotEncrypted);
    BOOST_CHECK_EQUAL(fake.calls->stores, 0);
    BOOST_CHECK_EQUAL(fake.calls->removes, 0);
}

BOOST_AUTO_TEST_CASE(setup_without_store_unsupported)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::string error;

    // A store that says it cannot store counts as none, and the passphrase is not checked,
    // so a wrong one is not counted either.
    {
        FakeStoreInstalled fake(/*configured=*/false, Outcome::Succeeds, Outcome::Succeeds, /*supported=*/false);

        BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Unsupported);
        BOOST_CHECK_EQUAL(error, AUTOUNLOCK_UNSUPPORTED_TEXT);
        BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, SecureString("not the passphrase"), error)
                    == AutoUnlockSetupResult::Unsupported);
        BOOST_CHECK(ClearAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Unsupported);
        BOOST_CHECK_EQUAL(fake.calls->stores, 0);
        BOOST_CHECK_EQUAL(fake.calls->removes, 0);
        BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
        BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
    }

    // The platform's own store: none here but on Windows, which has the DPAPI store.
#ifndef WIN32
    BOOST_REQUIRE(GetAutoUnlockSecretStore() == nullptr);
    BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Unsupported);
    BOOST_CHECK(ClearAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Unsupported);
    BOOST_CHECK(!GetAutoUnlockStatus().supported);
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
#endif
}

BOOST_AUTO_TEST_CASE(setup_stores_exact_passphrase_and_keeps_scope)
{
    EncryptedWallet fx;
    grc_test::SchedulerForThisCase scheduler;
    std::string error;

    // Staking only before, staking only after.
    {
        fx.UnlockFor(UnlockScope::StakingOnly);
        FakeStoreInstalled fake(/*configured=*/false);

        BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Ok);
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
        BOOST_CHECK(fake.calls->last == Passphrase());
        BOOST_CHECK(fake.calls->kept == Passphrase());
        BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
        BOOST_CHECK(!fx.wallet.CanUseKeys());
    }

    // A full unlock stays full.
    {
        fx.UnlockFor(UnlockScope::Full);
        FakeStoreInstalled fake(/*configured=*/true);

        BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Ok);
        BOOST_CHECK(fake.calls->kept == Passphrase());
        BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::Full);
    }

    // A locked wallet is locked again once the capability that checked the passphrase ends.
    {
        fx.wallet.Lock();
        FakeStoreInstalled fake(/*configured=*/false);

        BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Ok);
        BOOST_CHECK(fake.calls->kept == Passphrase());
        BOOST_CHECK(fx.wallet.IsLocked());
    }

    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
}

BOOST_AUTO_TEST_CASE(setup_store_failure_reported)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    std::string error;

    {
        FakeStoreInstalled fake(/*configured=*/false, Outcome::Fails);

        BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::StoreFailed);
        BOOST_CHECK_EQUAL(error, "the fake store refuses");
        BOOST_CHECK_EQUAL(fake.calls->stores, 1);
    }

    // A store that throws is a store that failed.
    {
        FakeStoreInstalled fake(/*configured=*/false, Outcome::Throws);

        BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::StoreFailed);
        BOOST_CHECK_MESSAGE(error.find("the fake store throws") != std::string::npos, error);
    }

    // A removal that fails leaves the wallet as it was.
    {
        FakeStoreInstalled fake(/*configured=*/true, Outcome::Succeeds, Outcome::Fails);

        BOOST_CHECK(ClearAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::StoreFailed);
        BOOST_CHECK_EQUAL(error, "the fake store refuses");
        BOOST_CHECK(fake.calls->kept.has_value());
        BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
    }

    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
}

// A file written but not read back is stored, and reported as not checked.
BOOST_AUTO_TEST_CASE(setup_unverified_store_reported)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/false, Outcome::Unverified);
    std::string error;

    BOOST_CHECK(SetAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::OkUnverified);
    BOOST_CHECK(fake.calls->kept == Passphrase());
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
}

BOOST_AUTO_TEST_CASE(clear_removes_and_locks)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/true);
    std::string error;

    BOOST_CHECK(ClearAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Ok);
    BOOST_CHECK_EQUAL(fake.calls->removes, 1);
    BOOST_CHECK(!fake.calls->kept.has_value());
    BOOST_CHECK(fx.wallet.IsLocked());

    // Nothing stored: the removal succeeds, and a full unlock is locked too.
    fx.UnlockFor(UnlockScope::Full);
    BOOST_CHECK(ClearAutoUnlockSecret(fx.wallet, Passphrase(), error) == AutoUnlockSetupResult::Ok);
    BOOST_CHECK_EQUAL(fake.calls->removes, 2);
    BOOST_CHECK(fx.wallet.IsLocked());
}

// No stale passphrase is left on disk: a passphrase change that comes while the setup
// stores waits for it, then re-encrypts what it stored.
BOOST_AUTO_TEST_CASE(setup_holds_the_passphrase_turn_while_it_stores)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/false);
    const SecureString changed("autounlock-setup-changed-passphrase");

    std::thread change;
    std::atomic<bool> change_returned{false};
    bool change_succeeded = false;
    bool returned_while_storing = true;
    bool locks_free_while_storing = false;

    // The setup's Store() starts a change and gives it a second, several times what the
    // change needs on its own, to finish. It must not finish: the setup holds the turn.
    fake.calls->on_store = [&](const SecureString& passphrase) {
        if (passphrase != Passphrase() || change.joinable()) return;

        // The store's file I/O and encryption run with neither cs_main nor cs_wallet held,
        // so another thread can take both meanwhile.
        std::thread([&] {
            TRY_LOCK(cs_main, main_lock);
            TRY_LOCK(fx.wallet.cs_wallet, wallet_lock);
            locks_free_while_storing = main_lock && wallet_lock;
        }).join();

        change = std::thread([&] {
            change_succeeded = fx.wallet.ChangeWalletPassphrase(Passphrase(), changed);
            change_returned = true;
        });

        for (int i = 0; i < 100 && !change_returned; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        returned_while_storing = change_returned;
    };

    std::string error;
    const AutoUnlockSetupResult result = SetAutoUnlockSecret(fx.wallet, Passphrase(), error);
    BOOST_REQUIRE(change.joinable());
    change.join();

    BOOST_CHECK(result == AutoUnlockSetupResult::Ok);
    BOOST_CHECK(locks_free_while_storing);
    BOOST_CHECK(!returned_while_storing);
    BOOST_CHECK(change_succeeded);

    // The setup stored the passphrase it checked, and the change then replaced it.
    BOOST_CHECK_EQUAL(fake.calls->stores, 2);
    BOOST_CHECK(fake.calls->kept == changed);
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
}

// A capability a passphrase change has ended authorises nothing in the turn that follows,
// so a setup whose check came before a change stores nothing.
BOOST_AUTO_TEST_CASE(capability_ended_by_a_passphrase_change_runs_nothing)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    bool ran = false;
    const auto action = [&ran] { ran = true; };

    // Positive control: a live capability of the kind runs the action, under its grant.
    std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::AutoUnlockSetup);
    BOOST_REQUIRE(id);
    bool keys_in_action = false;
    BOOST_CHECK(fx.wallet.RunUnderLiveCapability(*id, CapabilityKind::AutoUnlockSetup, [&] {
        ran = true;
        keys_in_action = fx.wallet.CanUseKeys();
    }) == CWallet::CapabilityRun::Ran);
    BOOST_CHECK(ran);
    BOOST_CHECK(keys_in_action);

    // Another kind's capability does not count.
    ran = false;
    BOOST_CHECK(fx.wallet.RunUnderLiveCapability(*id, CapabilityKind::Send, action) == CWallet::CapabilityRun::NotLive);
    BOOST_CHECK(!ran);

    // A passphrase change ends it.
    const SecureString changed("autounlock-setup-changed-passphrase");
    BOOST_REQUIRE(fx.wallet.ChangeWalletPassphrase(Passphrase(), changed));
    BOOST_CHECK(fx.wallet.RunUnderLiveCapability(*id, CapabilityKind::AutoUnlockSetup, action)
                == CWallet::CapabilityRun::NotLive);
    BOOST_CHECK(!ran);
    fx.wallet.EndElevation(*id);

    // A wait given up runs nothing either.
    id = fx.wallet.BeginElevation(changed, CapabilityKind::AutoUnlockSetup);
    BOOST_REQUIRE(id);
    {
        const std::atomic<bool> cancel{true};
        const PassphraseAttemptCancel attempt(cancel);
        BOOST_CHECK(fx.wallet.RunUnderLiveCapability(*id, CapabilityKind::AutoUnlockSetup, action)
                    == CWallet::CapabilityRun::Abandoned);
    }
    BOOST_CHECK(!ran);
    fx.wallet.EndElevation(*id);
}

BOOST_AUTO_TEST_CASE(rpc_setautounlock_wrong_passphrase_code)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/false);

    const auto refused = RpcError("setautounlock", SecureString("not the passphrase"));
    BOOST_REQUIRE(refused);
    BOOST_CHECK_EQUAL(refused->first, RPC_WALLET_PASSPHRASE_INCORRECT);
    BOOST_CHECK_EQUAL(fake.calls->stores, 0);

    const auto clear_refused = RpcError("clearautounlock", SecureString("not the passphrase"));
    BOOST_REQUIRE(clear_refused);
    BOOST_CHECK_EQUAL(clear_refused->first, RPC_WALLET_PASSPHRASE_INCORRECT);
    BOOST_CHECK_EQUAL(fake.calls->removes, 0);
}

BOOST_AUTO_TEST_CASE(rpc_setautounlock_ok_returns_scope_location_warning)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    {
        FakeStoreInstalled fake(/*configured=*/false);
        UniValue result;

        BOOST_REQUIRE(!RpcError("setautounlock", Passphrase(), &result));
        BOOST_CHECK_EQUAL(find_value(result, "scope").get_str(), "machine");
        BOOST_CHECK_EQUAL(find_value(result, "location").get_str(), grc_test::FakeAutoUnlockStore::LOCATION);
        BOOST_CHECK_EQUAL(find_value(result, "warning").get_str(), AutoUnlockScopeWarning("machine"));
        BOOST_CHECK(find_value(result, "verified").get_bool());
        BOOST_CHECK(fake.calls->kept == Passphrase());
    }

    // Written but not read back: verified is false.
    {
        FakeStoreInstalled fake(/*configured=*/false, Outcome::Unverified);
        UniValue result;

        BOOST_REQUIRE(!RpcError("setautounlock", Passphrase(), &result));
        BOOST_CHECK(!find_value(result, "verified").get_bool());
        BOOST_CHECK(fake.calls->kept == Passphrase());
    }

    BOOST_CHECK(!AutoUnlockScopeWarning("machine").empty());
    BOOST_CHECK(!AutoUnlockScopeWarning("service").empty());
    BOOST_CHECK(AutoUnlockScopeWarning("machine") != AutoUnlockScopeWarning("service"));
    BOOST_CHECK(AutoUnlockScopeWarning("elsewhere").empty());
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
}

BOOST_AUTO_TEST_CASE(rpc_clearautounlock_locks)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/true);
    UniValue result;

    BOOST_REQUIRE(!RpcError("clearautounlock", Passphrase(), &result));
    BOOST_CHECK(find_value(result, "removed").get_bool());
    BOOST_CHECK(find_value(result, "wallet_locked").get_bool());
    BOOST_CHECK(!fake.calls->kept.has_value());
    BOOST_CHECK(fx.wallet.IsLocked());
}

BOOST_AUTO_TEST_CASE(rpc_autounlock_error_codes)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    {
        FakeStoreInstalled fake(/*configured=*/false, Outcome::Succeeds, Outcome::Succeeds, /*supported=*/false);
        const auto refused = RpcError("setautounlock", Passphrase());
        BOOST_REQUIRE(refused);
        BOOST_CHECK_EQUAL(refused->first, RPC_MISC_ERROR);
        BOOST_CHECK_EQUAL(refused->second, AUTOUNLOCK_UNSUPPORTED_TEXT);
    }

    {
        FakeStoreInstalled fake(/*configured=*/false, Outcome::Fails);
        const auto refused = RpcError("setautounlock", Passphrase());
        BOOST_REQUIRE(refused);
        BOOST_CHECK_EQUAL(refused->first, RPC_MISC_ERROR);
        BOOST_CHECK_MESSAGE(refused->second.find("the fake store refuses") != std::string::npos, refused->second);
    }

    {
        FakeStoreInstalled fake(/*configured=*/true, Outcome::Succeeds, Outcome::Fails);
        const auto refused = RpcError("clearautounlock", Passphrase());
        BOOST_REQUIRE(refused);
        BOOST_CHECK_EQUAL(refused->first, RPC_MISC_ERROR);
        BOOST_CHECK_MESSAGE(refused->second.find("the fake store refuses") != std::string::npos, refused->second);
        BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
    }

    {
        CWallet unencrypted;
        grc_test::SwapMainWallet swap(&unencrypted);
        FakeStoreInstalled fake(/*configured=*/false);

        for (const char* method : {"setautounlock", "clearautounlock"}) {
            const auto refused = RpcError(method, Passphrase());
            BOOST_REQUIRE(refused);
            BOOST_CHECK_EQUAL(refused->first, RPC_WALLET_WRONG_ENC_STATE);
        }
    }

    // An empty passphrase is refused before anything is checked.
    {
        FakeStoreInstalled fake(/*configured=*/false);
        const auto refused = RpcError("setautounlock", SecureString());
        BOOST_REQUIRE(refused);
        BOOST_CHECK_EQUAL(refused->first, RPC_INVALID_PARAMETER);
        BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
    }
}

BOOST_AUTO_TEST_CASE(ipc_wallet_autounlock_roundtrip)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/false);
    const std::unique_ptr<interfaces::Wallet> wallet = interfaces::MakeWallet(&fx.wallet);

    AutoUnlockStoreStatus status = wallet->getAutoUnlockStatus();
    BOOST_CHECK(status.supported);
    BOOST_CHECK(!status.configured);
    BOOST_CHECK_EQUAL(status.scope, "machine");
    BOOST_CHECK_EQUAL(status.location, grc_test::FakeAutoUnlockStore::LOCATION);

    BOOST_CHECK(wallet->setAutoUnlockSecret(SecureString("not the passphrase")) == AutoUnlockSetupResult::WrongPassphrase);
    BOOST_CHECK(!wallet->getAutoUnlockStatus().configured);

    BOOST_CHECK(wallet->setAutoUnlockSecret(Passphrase()) == AutoUnlockSetupResult::Ok);
    BOOST_CHECK(wallet->getAutoUnlockStatus().configured);
    BOOST_CHECK(fake.calls->kept == Passphrase());
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);

    BOOST_CHECK(wallet->clearAutoUnlockSecret(Passphrase()) == AutoUnlockSetupResult::Ok);
    BOOST_CHECK(!wallet->getAutoUnlockStatus().configured);
    BOOST_CHECK(fx.wallet.IsLocked());
}

// The console sends both with the passphrase as a SecureString, as it sends the other
// passphrase methods.
BOOST_AUTO_TEST_CASE(console_secret_command_runs_autounlock_setup)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    FakeStoreInstalled fake(/*configured=*/false);
    const std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    const interfaces::RpcConsoleResult set = node->executeRpcConsoleSecretCommand("setautounlock", {Passphrase()}, {});
    BOOST_CHECK_MESSAGE(set.ok, set.output);
    BOOST_CHECK_MESSAGE(set.output.find("\"verified\": true") != std::string::npos, set.output);
    BOOST_CHECK(fake.calls->kept == Passphrase());

    const interfaces::RpcConsoleResult clear = node->executeRpcConsoleSecretCommand("clearautounlock", {Passphrase()}, {});
    BOOST_CHECK_MESSAGE(clear.ok, clear.output);
    BOOST_CHECK(!fake.calls->kept.has_value());
    BOOST_CHECK(fx.wallet.IsLocked());
}

BOOST_AUTO_TEST_CASE(setup_result_known_values)
{
    for (unsigned int value = 0; value <= 5; ++value) {
        BOOST_CHECK(IsKnownAutoUnlockSetupResult(static_cast<AutoUnlockSetupResult>(value)));
    }

    for (unsigned int value = 6; value <= 255; ++value) {
        BOOST_CHECK(!IsKnownAutoUnlockSetupResult(static_cast<AutoUnlockSetupResult>(value)));
    }
}

BOOST_AUTO_TEST_SUITE_END()
