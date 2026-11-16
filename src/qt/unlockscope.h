// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_UNLOCKSCOPE_H
#define BITCOIN_QT_UNLOCKSCOPE_H

namespace interfaces {
class Wallet;
} // namespace interfaces

//! What RestoreWalletScope did.
enum class RestoreOutcome {
    //! Neither flag was set: nothing to restore.
    Nothing,
    //! The wallet was locked.
    Relocked,
    //! The wallet was narrowed back to staking only.
    Narrowed,
    //! The narrowing was refused and the wallet is not fully unlocked: it
    //! locked itself meanwhile, or had nothing to narrow.
    NarrowRefusedLocked,
    //! The narrowing failed and the wallet is still fully unlocked; a warning
    //! was logged.
    NarrowFailedLeftOpen,
    //! A wallet call threw; a warning was logged and nothing escaped.
    Threw,
};

//! Gives a wallet back the scope it had before an operation unlocked it: with
//! `restore_staking_only` it narrows the wallet back to staking only, else
//! with `relock` it locks it. Every exception is caught and logged under
//! `caller`, so a destructor can call it.
RestoreOutcome RestoreWalletScope(interfaces::Wallet& wallet, bool relock, bool restore_staking_only,
                                  const char* caller);

#endif // BITCOIN_QT_UNLOCKSCOPE_H
