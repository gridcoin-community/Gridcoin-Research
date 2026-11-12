// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "key.h"
#include "sync.h"
#include "wallet/autounlock.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <optional>

namespace {
//! An encrypted, locked wallet holding one key.
void MakeLockedWallet(CWallet& wallet, const SecureString& passphrase)
{
    CKey key;
    key.MakeNewKey(true);
    {
        LOCK(wallet.cs_wallet);
        BOOST_REQUIRE(wallet.AddKey(key));
    }
    BOOST_REQUIRE(wallet.EncryptWallet(passphrase));
    wallet.Lock();
    BOOST_REQUIRE(wallet.IsLocked());
}
} // namespace

BOOST_AUTO_TEST_SUITE(autounlock_tests)

BOOST_AUTO_TEST_CASE(fetch_returns_nullopt_without_a_backend)
{
    BOOST_CHECK(!FetchAutoUnlockPassphrase());
}

BOOST_AUTO_TEST_CASE(unlocks_staking_only)
{
    CWallet wallet;
    const SecureString passphrase("autounlock-test-passphrase");
    MakeLockedWallet(wallet, passphrase);

    BOOST_CHECK(wallet::TryAutoUnlockStakingOnly(wallet, [&]() { return std::optional<SecureString>(passphrase); }));
    BOOST_CHECK(wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!wallet.CanUseKeys());
}

// A wrong passphrase is counted and answered after FailedPassphraseDelay(1), half a second.
BOOST_AUTO_TEST_CASE(wrong_passphrase_stays_locked)
{
    CWallet wallet;
    MakeLockedWallet(wallet, SecureString("autounlock-test-passphrase"));

    BOOST_CHECK(!wallet::TryAutoUnlockStakingOnly(wallet, []() { return std::optional<SecureString>(SecureString("wrong")); }));
    BOOST_CHECK(wallet.IsLocked());
}

// An attempt cancelled before it takes its turn is given up with the passphrase unchecked, which is not a wrong
// passphrase: nothing is counted and the wallet stays locked.
BOOST_AUTO_TEST_CASE(a_cancelled_attempt_leaves_the_wallet_locked_and_uncounted)
{
    CWallet wallet;
    const SecureString passphrase("autounlock-test-passphrase");
    MakeLockedWallet(wallet, passphrase);

    const std::atomic<bool> cancel{true};
    const PassphraseAttemptCancel token(cancel);

    BOOST_CHECK(!wallet::TryAutoUnlockStakingOnly(wallet, [&]() { return std::optional<SecureString>(passphrase); }));
    BOOST_CHECK(wallet.IsLocked());
    BOOST_CHECK_EQUAL(wallet.FailedUnlockCount(), 0U);
}

BOOST_AUTO_TEST_CASE(no_fetch_when_not_needed)
{
    int calls = 0;
    const auto fetch = [&]() {
        ++calls;
        return std::optional<SecureString>(SecureString("autounlock-test-passphrase"));
    };

    // Not encrypted.
    CWallet plain;
    BOOST_CHECK(!wallet::TryAutoUnlockStakingOnly(plain, fetch));
    BOOST_CHECK_EQUAL(calls, 0);

    // Encrypted and already unlocked for staking.
    CWallet unlocked;
    const SecureString passphrase("autounlock-test-passphrase");
    MakeLockedWallet(unlocked, passphrase);
    BOOST_REQUIRE(unlocked.Unlock(passphrase, UnlockScope::StakingOnly));
    BOOST_CHECK(!wallet::TryAutoUnlockStakingOnly(unlocked, fetch));
    BOOST_CHECK_EQUAL(calls, 0);
}

BOOST_AUTO_TEST_SUITE_END()
