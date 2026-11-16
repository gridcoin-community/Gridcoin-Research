// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/unlockscope.h"

#include "interfaces/wallet.h"
#include "util.h" /* for LogPrintf */

#include <exception>

RestoreOutcome RestoreWalletScope(interfaces::Wallet& wallet, bool relock, bool restore_staking_only,
                                  const char* caller)
{
    // Both arms cross IPC in the split build, and the caller may be a
    // destructor, which is implicitly noexcept: an exception escaping here --
    // a disconnect mid-call is the realistic one -- would terminate the GUI
    // outright rather than leave it to the disconnect handling that already
    // exists. Nothing here can be retried without the passphrase, so a record
    // of what was left behind is all that can be owed.
    try
    {
        if (restore_staking_only)
        {
            // Hand the elevation back rather than locking. Narrowing needs no
            // passphrase: the master key stays installed and only the permission
            // is removed.
            //
            // Not discarded: in the split build this crosses IPC and can fail,
            // and a failure leaves the wallet FULLY unlocked, which is the
            // unsafe direction.
            //
            // But false does NOT mean that on its own. It also means there was
            // nothing to narrow -- a locked wallet or an unencrypted one -- and
            // the locked case is ordinary here: the timed unlock can simply
            // have run out while the operation was in progress. Warning on that
            // would report a wallet left wide open when it had just closed
            // itself, so read the state, once, and warn only if it really is.
            if (wallet.restrictToStakingOnly()) return RestoreOutcome::Narrowed;

            const interfaces::WalletLockState state = wallet.getLockState();
            if (state.crypted && !state.locked && !state.unlocked_for_staking_only) {
                LogPrintf("WARN: %s: could not restore the staking-only scope; the wallet is left "
                          "fully unlocked", caller);
                return RestoreOutcome::NarrowFailedLeftOpen;
            }
            return RestoreOutcome::NarrowRefusedLocked;
        }

        if (relock)
        {
            wallet.lockWallet();
            return RestoreOutcome::Relocked;
        }
        return RestoreOutcome::Nothing;
    }
    catch (const std::exception& e)
    {
        LogPrintf("WARN: %s: could not restore the wallet lock state (%s); it may be left more "
                  "permissive than it was", caller, e.what());
    }
    catch (...)
    {
        LogPrintf("WARN: %s: could not restore the wallet lock state; it may be left more "
                  "permissive than it was", caller);
    }
    return RestoreOutcome::Threw;
}
