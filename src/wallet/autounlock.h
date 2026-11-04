// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_AUTOUNLOCK_H
#define GRIDCOIN_WALLET_AUTOUNLOCK_H

#include "support/allocators/secure.h"

#include <functional>
#include <optional>

class CWallet;

//! Platform backend for the wallet passphrase used to unlock for staking at start. Returns nullopt when no backend is
//! configured, which is always the case until a platform branch is added here. Each branch registers its own
//! COMMAND_LINE_ONLY option, does its own bounded retry and logs its own failure reason.
std::optional<SecureString> FetchAutoUnlockPassphrase();

namespace wallet {
//! The start-up call site. Returns false without fetching when the wallet is not encrypted or is not locked. Otherwise
//! fetches the passphrase and, if there is one, unlocks for staking only (never fully), cleanses the passphrase and logs
//! the outcome. Returns true when the wallet was unlocked. The fetch function is a parameter so tests can supply a
//! passphrase.
bool TryAutoUnlockStakingOnly(CWallet& wallet,
                              const std::function<std::optional<SecureString>()>& fetch = FetchAutoUnlockPassphrase);
} // namespace wallet

#endif // GRIDCOIN_WALLET_AUTOUNLOCK_H
