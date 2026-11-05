// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_AUTOUNLOCK_DPAPI_H
#define GRIDCOIN_WALLET_AUTOUNLOCK_DPAPI_H

#include "fs.h"
#include "support/allocators/secure.h"
#include "wallet/autounlock_store.h"

#include <functional>
#include <optional>
#include <string>

#ifdef WIN32
//! The passphrase in the autounlock file at \p path (autounlock_blob.h), decrypted with
//! DPAPI. The file must be in the layout, for this network, and of the scope this mode
//! writes: ServiceAccount when the core runs as a Windows service, LocalMachine
//! otherwise. A LocalMachine file must also give no account but this one, SYSTEM and
//! Administrators a right to read it or to change it. A sharing or lock violation is
//! retried twice, after 1 s and 2 s; anything else fails at once.
//!
//! Logs its own outcome, never the passphrase or the file's bytes: on a missing file
//! "autounlock is not set up (<path>); the wallet stays locked", on any other failure its
//! reason. Returns nullopt, with \p error saying why, in both cases.
std::optional<SecureString> FetchAutoUnlockFromDpapiBlob(const fs::path& path, std::string& error);

//! The Windows store: the autounlock file at AutoUnlockBlobPath(), encrypted for the
//! scope this mode reads. Store() writes the file under a temporary name beside it,
//! owner-only from creation, moves it into place, then reads it back and checks it as
//! FetchAutoUnlockFromDpapiBlob() would. It removes a file that check refuses for what
//! it is (its size, layout, scope, network or access control list) and returns false. A
//! file that could not be read back, after the fetch's retries or because a shutdown
//! ended them, is kept, logged as not checked, and Store() returns true. Remove()
//! succeeds when no file is there. Status() checks the file without decrypting it.
class DpapiBlobStore final : public AutoUnlockSecretStore
{
public:
    bool Store(const SecureString& passphrase, std::string& error) override;
    bool Remove(std::string& error) override;
    AutoUnlockStoreStatus Status() override;
};

//! For tests: \p hook is called by DpapiBlobStore::Store() with the file's path once the
//! file is in place and before it is read back. An empty one restores the default, which
//! does nothing.
void SetAutoUnlockReadBackHookForTesting(std::function<void(const fs::path&)> hook);
#endif // WIN32

#endif // GRIDCOIN_WALLET_AUTOUNLOCK_DPAPI_H
