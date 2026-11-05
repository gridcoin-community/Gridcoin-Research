// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_AUTOUNLOCK_STORE_H
#define GRIDCOIN_WALLET_AUTOUNLOCK_STORE_H

#include "support/allocators/secure.h"

#include <memory>
#include <string>

//! What a store reports about the passphrase it keeps for autounlock.
struct AutoUnlockStoreStatus
{
    //! Whether this platform has a store.
    bool supported{false};
    //! Whether something is stored, whether or not it could be used.
    bool configured{false};
    //! Who can decrypt what Store() writes: "machine" or "service" on Windows.
    std::string scope;
    //! Where the passphrase is kept, for the user.
    std::string location;
    //! Why what is stored would not be used, or why nothing can be stored. Empty when
    //! nothing is wrong.
    std::string problem;
};

//! Where the wallet passphrase is kept for autounlock. The interface is the same on every
//! platform; each platform provides its own store.
class AutoUnlockSecretStore
{
public:
    virtual ~AutoUnlockSecretStore() = default;

    //! Keep \p passphrase in place of anything kept before. False, with \p error saying
    //! why, when it is not kept.
    virtual bool Store(const SecureString& passphrase, std::string& error) = 0;

    //! Remove what is kept. True when nothing is kept afterwards, as when nothing was.
    virtual bool Remove(std::string& error) = 0;

    virtual AutoUnlockStoreStatus Status() = 0;
};

//! The store in use: the one a test installed, or else the platform's. That is the DPAPI
//! store on Windows; other platforms have none (nullptr) unless their plan registers one.
AutoUnlockSecretStore* GetAutoUnlockSecretStore();

//! Use \p store in place of the platform's; nullptr restores the platform's. For tests
//! only: no other thread may still be using the store this replaces.
void SetAutoUnlockSecretStoreForTesting(std::unique_ptr<AutoUnlockSecretStore> store);

//! The platform's own store, defined by the platform's backend (autounlock_dpapi.cpp on
//! Windows), and as nullptr in autounlock_store.cpp for a platform without one.
AutoUnlockSecretStore* PlatformAutoUnlockSecretStore();

//! Re-encrypts an existing secret after a passphrase change; logs; never throws. It acts
//! only when the store's Status() says something is stored. Call it once for each change
//! that succeeded, with neither cs_main nor cs_wallet held: the store does file I/O and
//! encryption.
void OnWalletPassphraseChanged(const SecureString& new_passphrase);

#endif // GRIDCOIN_WALLET_AUTOUNLOCK_STORE_H
