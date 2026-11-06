// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/autounlock_setup.h"

#include "logging.h"
#include "tinyformat.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <exception>
#include <functional>
#include <optional>
#include <utility>

namespace {
//! Called between the passphrase check and the store's turn (SetAutoUnlockSetupHookForTesting()).
std::function<void()> g_after_check_hook;

//! The store in use, when it can store: nullptr where the platform has none, and where its
//! Status() says it is not supported.
AutoUnlockSecretStore* UsableStore()
{
    AutoUnlockSecretStore* const store = GetAutoUnlockSecretStore();

    if (!store) return nullptr;

    try {
        if (store->Status().supported) return store;
    } catch (const std::exception& e) {
        LogPrintf("ERROR: autounlock: the store could not report its status: %s", e.what());
    } catch (...) {
        LogPrintf("ERROR: autounlock: the store could not report its status");
    }

    return nullptr;
}

//! What setup and turning off do with the store once the passphrase is checked. \p error
//! says why for any result but Ok.
using StoreAction = std::function<AutoUnlockSetupResult(AutoUnlockSecretStore& store, std::string& error)>;

//! The checks setup and turning off share, then \p action, in a passphrase turn of its own
//! and only while the capability that checked the passphrase is still live.
AutoUnlockSetupResult CheckThenAct(CWallet& wallet, const SecureString& passphrase, std::string& error,
                                   const StoreAction& action)
{
    error.clear();

    if (!wallet.IsCrypted()) {
        error = "the wallet is not encrypted";
        return AutoUnlockSetupResult::NotEncrypted;
    }

    AutoUnlockSecretStore* const store = UsableStore();

    if (!store) {
        error = AUTOUNLOCK_UNSUPPORTED_TEXT;
        return AutoUnlockSetupResult::Unsupported;
    }

    // The passphrase is checked as any capability's is, and a wrong one is counted and
    // delayed. A locked wallet is unlocked for staking only while the capability lasts,
    // and ending it locks the wallet again; any other unlock is left as it is.
    CWallet::UnlockFailure failure = CWallet::UnlockFailure::None;
    const std::optional<CapabilityId> id = wallet.BeginElevation(passphrase, CapabilityKind::AutoUnlockSetup, &failure);

    if (!id) {
        switch (failure) {
        case CWallet::UnlockFailure::Passphrase:
            error = "the wallet passphrase entered was incorrect";
            return AutoUnlockSetupResult::WrongPassphrase;
        case CWallet::UnlockFailure::Abandoned:
            error = "the wallet passphrase was not checked, because the node is shutting down or the request "
                    "was given up; nothing was changed";
            return AutoUnlockSetupResult::StoreFailed;
        case CWallet::UnlockFailure::RelockUnavailable:
            error = "the wallet passphrase was accepted, but the wallet's relock could not be scheduled, as "
                    "happens while the node starts or stops; nothing was changed";
            return AutoUnlockSetupResult::StoreFailed;
        case CWallet::UnlockFailure::None:
        case CWallet::UnlockFailure::AlreadyUnlocked:
            break;
        }

        error = "the wallet passphrase could not be checked; nothing was changed";
        return AutoUnlockSetupResult::StoreFailed;
    }

    AutoUnlockSetupResult result = AutoUnlockSetupResult::StoreFailed;
    CWallet::CapabilityRun run = CWallet::CapabilityRun::NotLive;
    {
        // Ended on every way out of this block, a throw included.
        const EndElevationOnExit end_elevation(wallet, *id);

        if (g_after_check_hook) g_after_check_hook();

        // The store's file I/O and encryption run here, with no lock held. A passphrase
        // change that comes meanwhile waits for its turn, then re-encrypts what was stored;
        // one that came first has ended the capability, and nothing is stored.
        run = wallet.RunUnderLiveCapability(*id, CapabilityKind::AutoUnlockSetup, [&] {
            try {
                result = action(*store, error);
            } catch (const std::exception& e) {
                error = e.what();
                result = AutoUnlockSetupResult::StoreFailed;
            } catch (...) {
                error = "an unknown error";
                result = AutoUnlockSetupResult::StoreFailed;
            }
        });
    }

    switch (run) {
    case CWallet::CapabilityRun::Ran:
        return result;
    case CWallet::CapabilityRun::NotLive:
        error = "the passphrase check ended before it could be acted on: the wallet passphrase was changed "
                "meanwhile, or the check expired; nothing was changed";
        return AutoUnlockSetupResult::StoreFailed;
    case CWallet::CapabilityRun::Abandoned:
        error = "nothing was changed, because the node is shutting down or the request was given up";
        return AutoUnlockSetupResult::StoreFailed;
    }

    return AutoUnlockSetupResult::StoreFailed;
}

//! Logs how setup (\p setup) or turning off ended. Never the passphrase: \p error holds a
//! reason only.
void LogOutcome(bool setup, AutoUnlockSetupResult result, const std::string& error)
{
    switch (result) {
    case AutoUnlockSetupResult::Ok:
        if (setup) {
            LogPrintf("autounlock: set up: the wallet passphrase is stored for staking-only autounlock");
        } else {
            LogPrintf("autounlock: turned off: no wallet passphrase is stored, and the wallet is locked");
        }
        return;
    case AutoUnlockSetupResult::OkUnverified:
        LogPrintf("autounlock: set up: the wallet passphrase is stored, but the file could not be read back to "
                  "check it; the next start checks it");
        return;
    case AutoUnlockSetupResult::StoreFailed:
        LogPrintf("ERROR: autounlock: %s: %s", setup ? "not set up" : "not turned off", error);
        return;
    case AutoUnlockSetupResult::WrongPassphrase:
    case AutoUnlockSetupResult::NotEncrypted:
    case AutoUnlockSetupResult::Unsupported:
        break;
    }

    LogPrintf("autounlock: %s: %s", setup ? "not set up" : "not turned off", error);
}
} // namespace

bool IsKnownAutoUnlockSetupResult(AutoUnlockSetupResult result)
{
    switch (result) {
    case AutoUnlockSetupResult::Ok:
    case AutoUnlockSetupResult::WrongPassphrase:
    case AutoUnlockSetupResult::NotEncrypted:
    case AutoUnlockSetupResult::Unsupported:
    case AutoUnlockSetupResult::StoreFailed:
    case AutoUnlockSetupResult::OkUnverified:
        return true;
    }

    return false;
}

AutoUnlockSetupResult SetAutoUnlockSecret(CWallet& wallet, const SecureString& passphrase, std::string& error)
{
    const AutoUnlockSetupResult result = CheckThenAct(
        wallet, passphrase, error, [&passphrase](AutoUnlockSecretStore& store, std::string& store_error) {
            switch (store.Store(passphrase, store_error)) {
            case AutoUnlockStoreOutcome::Verified:
                return AutoUnlockSetupResult::Ok;
            case AutoUnlockStoreOutcome::Unverified:
                return AutoUnlockSetupResult::OkUnverified;
            case AutoUnlockStoreOutcome::Failed:
                break;
            }

            return AutoUnlockSetupResult::StoreFailed;
        });

    LogOutcome(/*setup=*/true, result, error);

    return result;
}

AutoUnlockSetupResult ClearAutoUnlockSecret(CWallet& wallet, const SecureString& passphrase, std::string& error)
{
    const AutoUnlockSetupResult result = CheckThenAct(
        wallet, passphrase, error, [](AutoUnlockSecretStore& store, std::string& store_error) {
            return store.Remove(store_error) ? AutoUnlockSetupResult::Ok : AutoUnlockSetupResult::StoreFailed;
        });

    // Turning autounlock off locks the wallet now, however it was unlocked, but only once
    // it is off: on any other result the wallet is left as it was.
    if (result == AutoUnlockSetupResult::Ok) wallet.Lock();

    LogOutcome(/*setup=*/false, result, error);

    return result;
}

AutoUnlockStoreStatus GetAutoUnlockStatus()
{
    AutoUnlockStoreStatus status;
    AutoUnlockSecretStore* const store = GetAutoUnlockSecretStore();

    if (!store) return status;

    try {
        return store->Status();
    } catch (const std::exception& e) {
        status.problem = strprintf("the store could not report its status: %s", e.what());
    } catch (...) {
        status.problem = "the store could not report its status";
    }

    return status;
}

std::string AutoUnlockScopeWarning(const std::string& scope)
{
    if (scope == "machine") {
        return "On Windows, any program running as you can obtain your wallet passphrase whenever same-account "
               "autounlock is set up. For protection against that, use the service mode. Autounlock only ever "
               "unlocks the wallet for staking.";
    }

    if (scope == "service") {
        return "The passphrase is stored encrypted for the Gridcoin service account. Code running as that "
               "service can decrypt it. You are typing it on this desktop, where programs running as you can "
               "capture it.";
    }

    return {};
}

void SetAutoUnlockSetupHookForTesting(std::function<void()> hook)
{
    g_after_check_hook = std::move(hook);
}
