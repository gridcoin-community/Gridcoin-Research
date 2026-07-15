// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_WALLET_H
#define GRIDCOIN_INTERFACES_WALLET_H

#include "interfaces/handler.h"
#include "node/ui_interface.h" // For ChangeType.
#include "primitives/transaction.h" // For COutPoint.
#include "support/allocators/secure.h" // For SecureString.
#include "uint256.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class CCoinControl;
class CKeyID;
class CPubKey;
class CWallet;

namespace interfaces {

//! Snapshot of the four GUI-displayed balances, filled atomically under one
//! lock acquisition (see Wallet::tryGetBalances).
struct WalletBalances
{
    int64_t balance{0};
    int64_t stake{0};
    int64_t unconfirmed_balance{0};
    int64_t immature_balance{0};
};

//! Value snapshot of one unspent wallet output for the coin-control views.
//! Carries no pointers into the wallet: everything the GUI renders —
//! including the maturity flag, which needs cs_main — is computed node-side.
struct WalletOutput
{
    COutPoint outpoint;
    int64_t amount{0};
    //! Encoded destination of the output's scriptPubKey; empty when a
    //! destination cannot be extracted.
    std::string address;
    int depth{0};
    int64_t time{0};
    //! Coinstake still maturing: shown disabled in the views. Deliberately
    //! matches the old GUI-side check, which tested IsCoinStake() only.
    bool immature{false};
};

//! One send-coins recipient (the Qt-free mirror of SendCoinsRecipient).
struct WalletSendRecipient
{
    std::string address;
    std::string label;
    int64_t amount{0};
    //! Optional user message, embedded as a GRC::TxMessage contract.
    std::string message;
    bool subtract_fee_from_amount{false};
};

//! Result statuses for Wallet::sendCoins. GUI-detectable conditions
//! (duplicate recipient, user abort) are pre-checked by the GUI and never
//! reach the interface; addresses and amounts are nevertheless re-validated
//! node-side (InvalidAddress/InvalidAmount), because the node must not rely
//! on the client for checks whose failure would fund an empty —
//! anyone-can-spend — output script or sign-overflow the totals.
enum class SendCoinsStatus
{
    OK,
    InvalidAddress,
    InvalidAmount,
    AmountExceedsBalance,
    AmountWithFeeExceedsBalance,
    FeeExceedsSubtractedAmount,
    TransactionCreationFailed,
    TransactionCommitFailed,
    //! The required fee exceeds both the configured transaction fee and the
    //! caller's accepted_fee, and confirmation is needed before committing.
    //! Nothing was committed; `fee` carries the amount to confirm. Re-invoke
    //! with accepted_fee >= fee to proceed. This replaces the eliminated
    //! ThreadSafeAskFee modal, which used to block inside
    //! LOCK2(cs_main, cs_wallet) (doc/multiprocess_design.md section 4.5).
    FeeConfirmationRequired,
};

//! Result of Wallet::sendCoins.
struct SendCoinsResult
{
    SendCoinsStatus status{SendCoinsStatus::TransactionCreationFailed};
    //! The fee at issue when status is AmountWithFeeExceedsBalance,
    //! FeeExceedsSubtractedAmount, or FeeConfirmationRequired.
    int64_t fee{0};
    //! Committed transaction hash (hex) when status is OK.
    std::string txid_hex;
};

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

    //! Fill all four balances in one shot, or return false without blocking
    //! when the core is busy (cs_main/cs_wallet unavailable). The balance
    //! scans are expensive on large wallets, so refresh-path callers must be
    //! able to bow out instead of stalling the GUI thread; use the
    //! unconditional single getters only where an initial value is required.
    virtual bool tryGetBalances(WalletBalances& balances) = 0;

    //! Number of transactions in the wallet.
    virtual int getNumTransactions() = 0;

    //! Whether the wallet is encrypted.
    virtual bool isCrypted() = 0;

    //! Whether the wallet is locked.
    virtual bool isLocked() = 0;

    //! Whether an unlocked wallet is restricted to staking only.
    virtual bool isUnlockedForStakingOnly() = 0;

    //! The persisted staking-only unlock preference, independent of the
    //! current lock state (unlike isUnlockedForStakingOnly). Seeds the
    //! unlock dialog's checkbox while the wallet is still locked.
    virtual bool getUnlockStakingOnlyFlag() = 0;

    //! Encrypt the wallet with the given passphrase.
    virtual bool encryptWallet(const SecureString& passphrase) = 0;

    //! Lock the wallet. Does not clear the staking-only preference.
    virtual bool lockWallet() = 0;

    //! Unlock the wallet; on success the staking-only preference is set to
    //! staking_only (a full unlock clears a stale staking-only restriction).
    virtual bool unlockWallet(const SecureString& passphrase, bool staking_only) = 0;

    //! Change the wallet passphrase (locks the wallet first).
    virtual bool changeWalletPassphrase(const SecureString& old_passphrase,
                                        const SecureString& new_passphrase) = 0;

    //! Look up the public key for a key id. Public keys only — the wallet
    //! stays in-node in every build configuration, so this interface never
    //! carries private key material.
    virtual bool getPubKey(const CKeyID& address, CPubKey& pub_key_out) = 0;

    //! Fetch a fresh public key from the key pool, labeling its address in
    //! the address book when label is non-empty.
    virtual bool getKeyFromPool(CPubKey& pub_key_out, const std::string& label) = 0;

    //! Value snapshots of the given outpoints; unknown or conflicted
    //! (negative-depth) outpoints are skipped.
    virtual std::vector<WalletOutput> getOutputs(const std::vector<COutPoint>& outpoints) = 0;

    //! Spendable outputs grouped by address, with change outputs grouped
    //! under the address they derive from (walked node-side).
    virtual std::map<std::string, std::vector<WalletOutput>> listCoins() = 0;

    //! Create, fee-converge, and commit a transaction to the given
    //! recipients. Stateless one-shot: when the required fee needs user
    //! confirmation (it exceeds both the configured transaction fee and
    //! accepted_fee), returns FeeConfirmationRequired without committing —
    //! the reserved change key is released and no locks stay held while the
    //! GUI prompts. The caller re-invokes with accepted_fee set to the
    //! returned fee; the transaction is recreated from current wallet state,
    //! and a fee that meanwhile grew past accepted_fee simply returns
    //! FeeConfirmationRequired again, so no fee the user has not accepted is
    //! ever committed. coin_control may be null.
    virtual SendCoinsResult sendCoins(const std::vector<WalletSendRecipient>& recipients,
                                      const CCoinControl* coin_control,
                                      int64_t accepted_fee) = 0;

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
