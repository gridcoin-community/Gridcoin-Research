// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/autounlock_store.h"

#include "logging.h"

#include <atomic>
#include <exception>

namespace {
//! The store a test installed, owned here. Written only by
//! SetAutoUnlockSecretStoreForTesting().
std::unique_ptr<AutoUnlockSecretStore> g_test_store;

//! What GetAutoUnlockSecretStore() returns in place of the platform's store; null for the
//! platform's. Atomic, because the store is looked up from whichever thread changes the
//! passphrase.
std::atomic<AutoUnlockSecretStore*> g_test_store_in_use{nullptr};
} // namespace

AutoUnlockSecretStore* GetAutoUnlockSecretStore()
{
    if (AutoUnlockSecretStore* const store = g_test_store_in_use.load()) return store;

    return PlatformAutoUnlockSecretStore();
}

void SetAutoUnlockSecretStoreForTesting(std::unique_ptr<AutoUnlockSecretStore> store)
{
    // The new store is in use before the one it replaces is destroyed.
    g_test_store_in_use.store(store.get());
    g_test_store = std::move(store);
}

#ifndef WIN32
AutoUnlockSecretStore* PlatformAutoUnlockSecretStore()
{
    return nullptr;
}
#endif

void OnWalletPassphraseChanged(const SecureString& new_passphrase)
{
    AutoUnlockSecretStore* const store = GetAutoUnlockSecretStore();

    if (!store) return;

    std::string location;
    std::string failure;

    try {
        const AutoUnlockStoreStatus status = store->Status();

        if (!status.configured) return;

        location = status.location;

        if (store->Store(new_passphrase, failure)) {
            LogPrintf("autounlock: the stored passphrase now matches the new wallet passphrase (%s)", location);
            return;
        }
    } catch (const std::exception& e) {
        failure = e.what();
    } catch (...) {
        failure = "an unknown error";
    }

    LogPrintf("ERROR: autounlock: the stored passphrase could not be changed to the new wallet passphrase: %s",
              failure);

    // What is stored is the old passphrase, which no longer unlocks the wallet: every start
    // would spend a passphrase attempt on it, and it would stay on disk. So it is removed,
    // and the next start says autounlock is not set up.
    std::string error;
    bool removed = false;

    try {
        removed = store->Remove(error);
    } catch (const std::exception& e) {
        error = e.what();
    } catch (...) {
        error = "an unknown error";
    }

    if (removed) {
        LogPrintf("autounlock: the old stored passphrase was removed; set autounlock up again to use it");
    } else {
        LogPrintf("ERROR: autounlock: the old stored passphrase could not be removed either (%s): %s. Remove it, "
                  "then set autounlock up again.", location, error);
    }
}
