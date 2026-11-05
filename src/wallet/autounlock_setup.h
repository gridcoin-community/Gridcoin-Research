// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_AUTOUNLOCK_SETUP_H
#define GRIDCOIN_WALLET_AUTOUNLOCK_SETUP_H

#include "support/allocators/secure.h"
#include "wallet/autounlock_store.h"

#include <cstdint>
#include <string>

class CWallet;

//! How setting up or turning off autounlock ended. It crosses IPC as its integer value
//! (wallet.capnp), so a new member is appended, never inserted.
enum class AutoUnlockSetupResult : uint8_t {
    //! Done: the passphrase is stored and was read back and checked, or the stored one is
    //! removed.
    Ok = 0,
    //! The passphrase did not open the wallet. Nothing was changed.
    WrongPassphrase = 1,
    //! The wallet is not encrypted, so there is no passphrase to store. Nothing was changed.
    NotEncrypted = 2,
    //! This platform has no store for the passphrase. Nothing was changed.
    Unsupported = 3,
    //! The store failed, or the passphrase could not be acted on (a shutdown, or a
    //! passphrase change that came first); the error says why.
    StoreFailed = 4,
    //! The passphrase is stored, but the file could not be read back to check it. The
    //! fetch at the next start checks it. Only setup ends so.
    OkUnverified = 5,
};

//! Whether \p result is one of the enumerators above. A result that arrives from another
//! process crosses as a byte, and every value of a byte is a value of the enumeration.
bool IsKnownAutoUnlockSetupResult(AutoUnlockSetupResult result);

//! What setup and turning off say on a platform without a store.
inline constexpr const char* AUTOUNLOCK_UNSUPPORTED_TEXT{"autounlock setup is not available on this platform"};

//! \brief Store the wallet passphrase for autounlock, once it has been checked.
//!
//! In order: an unencrypted wallet ends NotEncrypted, and a platform without a store (or
//! whose store's Status() says it is not supported) Unsupported. The passphrase is then
//! checked by beginning a capability for AutoUnlockSetup (CWallet::BeginElevation()),
//! which applies the failed-passphrase delay to a wrong one. The store is written in a
//! passphrase turn of its own while that capability is still live
//! (CWallet::RunUnderLiveCapability()), so a passphrase change either comes after it,
//! and re-encrypts what it stored, or came before and ended the capability, and then
//! nothing is stored. The wallet's unlock is left as it was.
//!
//! The passphrase is taken in-band, not from a capability already held: the store needs
//! the passphrase itself. \p error says why for every result but Ok. It never holds the
//! passphrase. The caller must hold neither cs_main nor cs_wallet: the store does file
//! I/O and encryption, and a wrong passphrase waits out its delay.
AutoUnlockSetupResult SetAutoUnlockSecret(CWallet& wallet, const SecureString& passphrase, std::string& error);

//! \brief Remove the stored passphrase, once the wallet passphrase has been checked, and
//! lock the wallet: turning autounlock off locks the wallet now.
//!
//! As SetAutoUnlockSecret(), with the store's Remove() in place of Store(). Removing when
//! nothing is stored succeeds. The wallet is locked only on Ok; on any other result it is
//! left as it was.
AutoUnlockSetupResult ClearAutoUnlockSecret(CWallet& wallet, const SecureString& passphrase, std::string& error);

//! The status of the store in use. Not supported, and nothing else set, where the
//! platform has no store.
AutoUnlockStoreStatus GetAutoUnlockStatus();

//! What a passphrase stored in \p scope is exposed to, for the user: the text for
//! "machine" or for "service", and an empty string for any other scope.
std::string AutoUnlockScopeWarning(const std::string& scope);

#endif // GRIDCOIN_WALLET_AUTOUNLOCK_SETUP_H
