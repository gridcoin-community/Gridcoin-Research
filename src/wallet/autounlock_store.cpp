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
    try {
        AutoUnlockSecretStore* const store = GetAutoUnlockSecretStore();

        if (!store) return;

        const AutoUnlockStoreStatus status = store->Status();

        if (!status.configured) return;

        std::string error;

        if (store->Store(new_passphrase, error)) {
            LogPrintf("autounlock: the stored passphrase now matches the new wallet passphrase (%s)", status.location);
        } else {
            LogPrintf("ERROR: autounlock: the stored passphrase could not be changed to the new wallet passphrase: %s. "
                      "Autounlock will not unlock the wallet until it is set up again.", error);
        }
    } catch (const std::exception& e) {
        LogPrintf("ERROR: autounlock: the stored passphrase could not be changed to the new wallet passphrase: %s. "
                  "Autounlock will not unlock the wallet until it is set up again.", e.what());
    } catch (...) {
        LogPrintf("ERROR: autounlock: the stored passphrase could not be changed to the new wallet passphrase. "
                  "Autounlock will not unlock the wallet until it is set up again.");
    }
}
