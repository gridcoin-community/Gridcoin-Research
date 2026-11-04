// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/autounlock.h"

#include "logging.h"
#include "wallet/wallet.h"

std::optional<SecureString> FetchAutoUnlockPassphrase()
{
    return std::nullopt;
}

namespace wallet {
bool TryAutoUnlockStakingOnly(CWallet& wallet, const std::function<std::optional<SecureString>()>& fetch)
{
    if (!wallet.IsCrypted() || !wallet.IsLocked()) {
        return false;
    }

    bool unlocked = false;
    CWallet::UnlockFailure failure = CWallet::UnlockFailure::None;

    {
        // The passphrase stays in the secure allocator and is cleansed when this goes out of scope.
        const std::optional<SecureString> passphrase = fetch();

        if (!passphrase) {
            return false;
        }

        unlocked = wallet.Unlock(*passphrase, UnlockScope::StakingOnly, /*relock_after=*/std::nullopt, &failure);
    }

    if (unlocked) {
        LogPrintf("autounlock: wallet unlocked for staking");
    } else if (failure == CWallet::UnlockFailure::Passphrase) {
        LogPrintf("autounlock: the passphrase did not unlock the wallet; it stays locked");
    } else {
        const char* reason = "unknown";

        switch (failure) {
        case CWallet::UnlockFailure::RelockUnavailable: reason = "relock unavailable"; break;
        case CWallet::UnlockFailure::AlreadyUnlocked: reason = "already unlocked"; break;
        case CWallet::UnlockFailure::Abandoned: reason = "attempt abandoned"; break;
        default: break;
        }

        LogPrintf("autounlock: the wallet was not unlocked (%s); it stays locked", reason);
    }

    return unlocked;
}
} // namespace wallet
