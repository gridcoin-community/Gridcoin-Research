# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xd46631119dfcd074;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/wallet.h");
$Proxy.includeTypes("ipc/capnp/wallet-types.h");

using Handler = import "handler.capnp";
using Node = import "node.capnp";

# Mirrors interfaces::Wallet (src/interfaces/wallet.h). interfaces::Wallet is a
# pure-abstract interface, so every one of its methods must appear here: the
# generated ProxyClient<Wallet> overrides them one-for-one and would remain
# abstract (uninstantiable) if any were omitted.
#
# Conventions: methods with C++ out-parameters map each out-param to a leading
# result field (in argument order) and the return value to a trailing `result`
# field. Enums (MessageSignStatus/MessageVerifyStatus/SendCoinsStatus,
# ChangeType) cross as their integer values. SecureString passphrases, CKeyID,
# CPubKey, and uint256 cross as Data via the custom hooks in common-types.h.
interface Wallet $Proxy.wrap("interfaces::Wallet") {
    destroy @0 (context :Proxy.Context) -> ();

    getBalance @1 (context :Proxy.Context) -> (result :Int64);
    getStake @2 (context :Proxy.Context) -> (result :Int64);
    getUnconfirmedBalance @3 (context :Proxy.Context) -> (result :Int64);
    getImmatureBalance @4 (context :Proxy.Context) -> (result :Int64);
    tryGetBalances @5 (context :Proxy.Context) -> (balances :WalletBalances, result :Bool);
    getNumTransactions @6 (context :Proxy.Context) -> (result :Int32);
    getLockState @7 (context :Proxy.Context) -> (result :WalletLockState);
    isUnlockedForStakingOnly @8 (context :Proxy.Context) -> (result :Bool);
    getUnlockStakingOnlyFlag @9 (context :Proxy.Context) -> (result :Bool);
    encryptWallet @10 (context :Proxy.Context, passphrase :Data) -> (result :Bool);
    lockWallet @11 (context :Proxy.Context) -> (result :Bool);
    unlockWallet @12 (context :Proxy.Context, passphrase :Data, stakingOnly :Bool) -> (result :Bool);
    changeWalletPassphrase @13 (context :Proxy.Context, oldPassphrase :Data, newPassphrase :Data) -> (result :Bool);
    getPubKey @14 (context :Proxy.Context, address :Data) -> (pubKeyOut :Data, result :Bool);
    getKeyFromPool @15 (context :Proxy.Context, label :Text) -> (pubKeyOut :Data, result :Bool);
    getOutputs @16 (context :Proxy.Context, outpoints :List(OutPoint)) -> (result :List(WalletOutput));
    computeCoinControlSummary @17 (context :Proxy.Context, selection :WalletCoinControl, recipientAmounts :List(Int64), subtractFeeFromAmount :Bool) -> (result :CoinControlSummary);
    getMaxConsolidationInputs @18 (context :Proxy.Context) -> (result :UInt32);
    sendCoins @19 (context :Proxy.Context, recipients :List(WalletSendRecipient), coinControl :WalletCoinControl, acceptedFee :Int64) -> (result :SendCoinsResult);
    backupWallet @20 (context :Proxy.Context, dest :Text) -> (result :Bool);
    backupConfigFile @21 (context :Proxy.Context, dest :Text) -> (result :Bool);
    signMessage @22 (context :Proxy.Context, address :Text, message :Text) -> (signatureOut :Text, result :Int32);
    verifyMessage @23 (context :Proxy.Context, address :Text, message :Text, signature :Data) -> (result :Int32);
    getAddresses @24 (context :Proxy.Context) -> (result :List(WalletAddress));
    getAddressLabel @25 (context :Proxy.Context, address :Text) -> (labelOut :Text, result :Bool);
    isMine @26 (context :Proxy.Context, address :Text) -> (result :Bool);
    setAddressBook @27 (context :Proxy.Context, address :Text, label :Text) -> ();
    delAddressBook @28 (context :Proxy.Context, address :Text) -> (result :Bool);
    getNewReceiveAddress @29 (context :Proxy.Context) -> (addressOut :Text, result :Bool);
    getNewReceiveAddressWithLabel @30 (context :Proxy.Context, label :Text) -> (addressOut :Text, result :Bool);
    getUnbookedReceiveAddresses @31 (context :Proxy.Context) -> (result :List(Text));
    handleStatusChanged @32 (context :Proxy.Context, callback :Node.VoidCallback) -> (result :Handler.Handler);
    handleAddressBookChanged @33 (context :Proxy.Context, callback :AddressBookChangedCallback) -> (result :Handler.Handler);
    handleTransactionChanged @34 (context :Proxy.Context, callback :TransactionChangedCallback) -> (result :Handler.Handler);
}

# --- Notification callbacks (interfaces::Wallet::*Fn std::function types) ---
# handleStatusChanged reuses Node.VoidCallback (also ProxyCallback<void()>) so
# the two schemas do not register two capnp interfaces for the same C++ type.

interface AddressBookChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(const std::string&, const std::string&, bool, const std::string&, ChangeType)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, address :Text, label :Text, isMine :Bool, purpose :Text, status :Int32) -> ();
}

interface TransactionChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(const uint256&, ChangeType)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, txHash :Data, status :Int32) -> ();
}

# --- Value DTOs (interfaces:: structs in wallet.h) ---

# COutPoint (primitives/transaction.h): a hash + output index.
struct OutPoint $Proxy.wrap("COutPoint") {
    hash @0 :Data;
    n @1 :UInt32;
}

struct WalletBalances $Proxy.wrap("interfaces::WalletBalances") {
    balance @0 :Int64;
    stake @1 :Int64;
    unconfirmedBalance @2 :Int64 $Proxy.name("unconfirmed_balance");
    immatureBalance @3 :Int64 $Proxy.name("immature_balance");
}

struct WalletLockState $Proxy.wrap("interfaces::WalletLockState") {
    crypted @0 :Bool;
    locked @1 :Bool;
    unlockedForStakingOnly @2 :Bool $Proxy.name("unlocked_for_staking_only");
    stakingOnlyFlag @3 :Bool $Proxy.name("staking_only_flag");
}

struct WalletOutput $Proxy.wrap("interfaces::WalletOutput") {
    outpoint @0 :OutPoint;
    amount @1 :Int64;
    address @2 :Text;
    depth @3 :Int32;
    time @4 :Int64;
    immature @5 :Bool;
}

struct WalletAddress $Proxy.wrap("interfaces::WalletAddress") {
    address @0 :Text;
    label @1 :Text;
    isMine @2 :Bool $Proxy.name("is_mine");
}

# The GUI's coin-selection value. `selected` is a std::set<COutPoint>, marshalled
# as a List(OutPoint) via the type-set hook.
struct WalletCoinControl $Proxy.wrap("interfaces::WalletCoinControl") {
    destChange @0 :Text $Proxy.name("dest_change");
    allowWatchOnly @1 :Bool $Proxy.name("allow_watch_only");
    selected @2 :List(OutPoint);
}

struct CoinControlSummary $Proxy.wrap("interfaces::CoinControlSummary") {
    quantity @0 :Int32;
    amount @1 :Int64;
    fee @2 :Int64;
    afterFee @3 :Int64 $Proxy.name("after_fee");
    bytes @4 :UInt32;
    change @5 :Int64;
    lowOutput @6 :Bool $Proxy.name("low_output");
    dust @7 :Bool;
}

struct WalletSendRecipient $Proxy.wrap("interfaces::WalletSendRecipient") {
    address @0 :Text;
    label @1 :Text;
    amount @2 :Int64;
    message @3 :Text;
    subtractFeeFromAmount @4 :Bool $Proxy.name("subtract_fee_from_amount");
}

struct SendCoinsResult $Proxy.wrap("interfaces::SendCoinsResult") {
    status @0 :Int32;
    fee @1 :Int64;
    txidHex @2 :Text $Proxy.name("txid_hex");
}
