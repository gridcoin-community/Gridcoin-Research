// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_WALLET_H
#define GRIDCOIN_INTERFACES_WALLET_H

#include "interfaces/handler.h"
#include "node/ui_interface.h" // For ChangeType.
#include "uint256.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class CWallet;

namespace interfaces {

//! Wallet interface for the GUI: wallet queries plus notification
//! registration. The same boundary rules as interfaces::Node apply (see
//! src/interfaces/README.md); notification callbacks are bridged from
//! CWallet's own signals, which fire on core threads and, depending on the
//! signal, may or may not hold core locks at emission time (e.g.
//! NotifyTransactionChanged fires with cs_wallet held while
//! NotifyAddressBookChanged deliberately does not). Callbacks must not
//! assume either way: enqueue and return, and never take core locks or
//! re-enter interface methods that do.
//!
//! The method set covers only what the migrated consumers need (Phase 1c-i
//! starts with WalletModel's balance/encryption surface); it grows with each
//! migration. The wallet stays in-node in every build configuration, so this
//! interface never carries key material.
class Wallet
{
public:
    virtual ~Wallet() = default;

    //! Spendable balance.
    virtual int64_t getBalance() = 0;

    //! Balance currently staking (in coinstakes awaiting maturity).
    virtual int64_t getStake() = 0;

    //! Unconfirmed balance.
    virtual int64_t getUnconfirmedBalance() = 0;

    //! Immature (newly generated) balance.
    virtual int64_t getImmatureBalance() = 0;

    //! Whether the wallet is encrypted.
    virtual bool isCrypted() = 0;

    //! Whether the wallet is locked.
    virtual bool isLocked() = 0;

    //! Whether an unlocked wallet is restricted to staking only.
    virtual bool isUnlockedForStakingOnly() = 0;

    //! Register a handler for encryption/lock status changes.
    using StatusChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) = 0;

    //! Register a handler for address-book changes.
    using AddressBookChangedFn = std::function<void(const std::string& address,
                                                    const std::string& label,
                                                    bool is_mine,
                                                    const std::string& purpose,
                                                    ChangeType status)>;
    virtual std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) = 0;

    //! Register a handler for wallet-transaction changes.
    using TransactionChangedFn = std::function<void(const uint256& tx_hash, ChangeType status)>;
    virtual std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) = 0;
};

//! Return an in-process Wallet interface implementation wrapping the given
//! wallet, which must outlive the returned object.
std::unique_ptr<Wallet> MakeWallet(CWallet* wallet);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_WALLET_H
