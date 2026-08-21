# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xa0eb30f0c12f8b70;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/wallet_coin_source.h");
$Proxy.includeTypes("ipc/capnp/wallet_coin_source-types.h");

# COutPoint already has a wrapped representation in wallet.capnp; reuse it
# rather than declaring a second capnp struct wrapping the same C++ type.
using Wallet = import "wallet.capnp";

# Mirrors interfaces::WalletCoinSource (src/interfaces/wallet_coin_source.h),
# the windowed coin-control selection channel and coin-side sibling of
# WalletTxSource. Pure-abstract, so every method is schema'd (else
# ProxyClient<WalletCoinSource> stays abstract and the factory below cannot
# return one). Only value types cross: COutPoint as Wallet.OutPoint; the
# CoinViewMode enum as its integer value; and the WalletCoinEventPayload
# std::variant as a discriminated struct (WalletCoinEventPayloadWire +
# type-variant.h).
interface WalletCoinSource $Proxy.wrap("interfaces::WalletCoinSource") {
    destroy @0 (context :Proxy.Context) -> ();

    registerView @1 (context :Proxy.Context, viewId :Int32, mode :Int32, sortColumn :Int32, sortOrder :Int32) -> ();
    unregisterView @2 (context :Proxy.Context, viewId :Int32) -> ();
    setViewMode @3 (context :Proxy.Context, viewId :Int32, mode :Int32) -> ();
    setViewSort @4 (context :Proxy.Context, viewId :Int32, sortColumn :Int32, sortOrder :Int32) -> ();
    getRows @5 (context :Proxy.Context, viewId :Int32, first :Int32, count :Int32) -> (result :CoinRowsResult);
    getGroups @6 (context :Proxy.Context, viewId :Int32, first :Int32, count :Int32) -> (result :CoinGroupsResult);
    getGroupRows @7 (context :Proxy.Context, viewId :Int32, groupAddress :Text, first :Int32, count :Int32) -> (result :CoinRowsResult);
    getGroupDirectory @8 (context :Proxy.Context) -> (result :List(CoinGroupInfo));
    reconcileSelection @9 (context :Proxy.Context, selection :List(Wallet.OutPoint)) -> (result :List(Wallet.OutPoint));
    setSelected @10 (context :Proxy.Context, outpoint :Wallet.OutPoint, selected :Bool) -> (result :CoinSelectionUpdate);
    selectGroup @11 (context :Proxy.Context, groupAddress :Text, selected :Bool) -> (result :CoinBulkSelectionResult);
    selectAll @12 (context :Proxy.Context, selected :Bool) -> (result :CoinBulkSelectionResult);
    applyValueFilter @13 (context :Proxy.Context, lessOrEqual :Bool, value :Int64, maxInputs :UInt32) -> (result :CoinBulkSelectionResult);
    reloadAndSnapshot @14 (context :Proxy.Context) -> (result :CoinGroupsResult);
    drainEvents @15 (context :Proxy.Context, maxBatch :UInt64) -> (result :List(WalletCoinEvent));
    consumeNeedsResync @16 (context :Proxy.Context) -> (result :Bool);
    noteAddressBookChanged @17 (context :Proxy.Context, address :Text, label :Text) -> ();
}

# --- Records and aggregates (GRC::CoinRecord / GRC::CoinGroupInfo) ---

struct CoinRecord $Proxy.wrap("GRC::CoinRecord") {
    outpoint @0 :Wallet.OutPoint;
    amount @1 :Int64;
    address @2 :Text;
    groupAddress @3 :Text $Proxy.name("group_address");
    label @4 :Text;
    time @5 :Int64;
    blockHeight @6 :Int32 $Proxy.name("block_height");
    depth @7 :Int32;
    isChange @8 :Bool $Proxy.name("is_change");
}

struct CoinGroupInfo $Proxy.wrap("GRC::CoinGroupInfo") {
    address @0 :Text;
    label @1 :Text;
    totalAmount @2 :Int64 $Proxy.name("total_amount");
    outputCount @3 :Int32 $Proxy.name("output_count");
    directOutputCount @4 :Int32 $Proxy.name("direct_output_count");
    selectedCount @5 :Int32 $Proxy.name("selected_count");
    selectedAmount @6 :Int64 $Proxy.name("selected_amount");
}

# --- Windowed read results (GRC::CoinRowsResult / GRC::CoinGroupsResult) ---

struct CoinRowsResult $Proxy.wrap("GRC::CoinRowsResult") {
    records @0 :List(CoinRecord);
    totalAccepted @1 :Int32 $Proxy.name("total_accepted");
    epoch @2 :UInt64;
    highWater @3 :UInt64 $Proxy.name("high_water");
}

struct CoinGroupsResult $Proxy.wrap("GRC::CoinGroupsResult") {
    groups @0 :List(CoinGroupInfo);
    totalGroups @1 :Int32 $Proxy.name("total_groups");
    epoch @2 :UInt64;
    highWater @3 :UInt64 $Proxy.name("high_water");
}

# --- Selection results (GRC::CoinSelectionUpdate / CoinBulkSelectionResult) ---

struct CoinSelectionUpdate $Proxy.wrap("GRC::CoinSelectionUpdate") {
    applied @0 :Bool;
    group @1 :CoinGroupInfo;
}

struct CoinBulkSelectionResult $Proxy.wrap("GRC::CoinBulkSelectionResult") {
    added @0 :List(Wallet.OutPoint);
    removed @1 :List(Wallet.OutPoint);
    culled @2 :Bool;
}

# --- Event payloads (GRC::Coin*Payload) ---

struct CoinRowsInsertedPayload $Proxy.wrap("GRC::CoinRowsInsertedPayload") {
    viewId @0 :Int32 $Proxy.name("view_id");
    epoch @1 :UInt64;
    scope @2 :Text;
    position @3 :Int32;
    records @4 :List(CoinRecord);
}

struct CoinRowsRemovedPayload $Proxy.wrap("GRC::CoinRowsRemovedPayload") {
    viewId @0 :Int32 $Proxy.name("view_id");
    epoch @1 :UInt64;
    scope @2 :Text;
    position @3 :Int32;
    count @4 :Int32;
    outpoints @5 :List(Wallet.OutPoint);
}

struct CoinRowsChangedPayload $Proxy.wrap("GRC::CoinRowsChangedPayload") {
    viewId @0 :Int32 $Proxy.name("view_id");
    epoch @1 :UInt64;
    scope @2 :Text;
    first @3 :Int32;
    count @4 :Int32;
}

struct CoinGroupsInsertedPayload $Proxy.wrap("GRC::CoinGroupsInsertedPayload") {
    viewId @0 :Int32 $Proxy.name("view_id");
    epoch @1 :UInt64;
    position @2 :Int32;
    groups @3 :List(CoinGroupInfo);
}

struct CoinGroupsRemovedPayload $Proxy.wrap("GRC::CoinGroupsRemovedPayload") {
    viewId @0 :Int32 $Proxy.name("view_id");
    epoch @1 :UInt64;
    position @2 :Int32;
    count @3 :Int32;
}

struct CoinGroupsChangedPayload $Proxy.wrap("GRC::CoinGroupsChangedPayload") {
    viewId @0 :Int32 $Proxy.name("view_id");
    epoch @1 :UInt64;
    first @2 :Int32;
    count @3 :Int32;
}

struct CoinResetPayload $Proxy.wrap("GRC::CoinResetPayload") {
    viewId @0 :Int32 $Proxy.name("view_id");
    epoch @1 :UInt64;
}

struct CoinDepthRefreshPayload $Proxy.wrap("GRC::CoinDepthRefreshPayload") {
    height @0 :Int32;
}

# WalletCoinEventPayload = std::variant<the eight payloads>, marshalled by
# type-variant.h as a discriminated struct: `which` (the active alternative
# index) plus one field per alternative, in the SAME order as the C++
# std::variant. Synthetic wire type -- no $Proxy.wrap.
struct WalletCoinEventPayloadWire {
    which @0 :UInt16;
    coinRowsInserted @1 :CoinRowsInsertedPayload;
    coinRowsRemoved @2 :CoinRowsRemovedPayload;
    coinRowsChanged @3 :CoinRowsChangedPayload;
    coinGroupsInserted @4 :CoinGroupsInsertedPayload;
    coinGroupsRemoved @5 :CoinGroupsRemovedPayload;
    coinGroupsChanged @6 :CoinGroupsChangedPayload;
    coinReset @7 :CoinResetPayload;
    coinDepthRefresh @8 :CoinDepthRefreshPayload;
}

struct WalletCoinEvent $Proxy.wrap("GRC::WalletCoinEvent") {
    seqno @0 :UInt64;
    emitTimeUs @1 :Int64 $Proxy.name("emit_time_us");
    payload @2 :WalletCoinEventPayloadWire;
}
