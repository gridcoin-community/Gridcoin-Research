# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xf1e2d3c4b5a69788;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/wallet_tx_source.h");
$Proxy.includeTypes("ipc/capnp/wallet_tx_source-types.h");

# Mirrors interfaces::WalletTxSource (src/interfaces/wallet_tx_source.h), the
# windowed transaction-table event channel. Pure-abstract, so every method is
# schema'd (else ProxyClient<WalletTxSource> stays abstract). Only value types
# cross: uint256 as Data (common-types.h); enums (TransactionRecord::Type,
# TransactionStatus::Status, MinedType, WalletTxDetail::Finality/Source/
# AmountForm) as their integer values; and the WalletEventPayload std::variant
# as a discriminated struct (WalletEventPayloadWire + type-variant.h).
interface WalletTxSource $Proxy.wrap("interfaces::WalletTxSource") {
    destroy @0 (context :Proxy.Context) -> ();

    registerView @1 (context :Proxy.Context, viewId :Int32, filter :FilterSpec, sortColumn :Int32, sortOrder :Int32) -> ();
    unregisterView @2 (context :Proxy.Context, viewId :Int32) -> ();
    setViewSort @3 (context :Proxy.Context, viewId :Int32, sortColumn :Int32, sortOrder :Int32) -> ();
    setViewFilter @4 (context :Proxy.Context, viewId :Int32, filter :FilterSpec) -> ();
    setViewLimit @5 (context :Proxy.Context, viewId :Int32, limit :Int32) -> ();
    getRows @6 (context :Proxy.Context, viewId :Int32, first :Int32, count :Int32) -> (result :RowsResult);
    getAllRows @7 (context :Proxy.Context, viewId :Int32) -> (result :RowsResult);
    rowForKey @8 (context :Proxy.Context, viewId :Int32, hash :Data, idx :Int32) -> (result :Int32);
    getRowDetail @9 (context :Proxy.Context, hash :Data, idx :Int32) -> (result :WalletTxDetail);
    prime @10 (context :Proxy.Context, limitEnabled :Bool, limitTime :Int64) -> ();
    drainEvents @11 (context :Proxy.Context, maxBatch :UInt64) -> (result :List(WalletEvent));
    noteAddressBookChanged @12 (context :Proxy.Context, address :Text, label :Text) -> ();
}

# --- Filter / sort parameter (GRC::FilterSpec) ---

struct FilterSpec $Proxy.wrap("GRC::FilterSpec") {
    dateFrom @0 :Int64 $Proxy.name("date_from");
    dateTo @1 :Int64 $Proxy.name("date_to");
    typeMask @2 :UInt32 $Proxy.name("type_mask");
    addressSubstr @3 :Text $Proxy.name("address_substr");
    minAmount @4 :Int64 $Proxy.name("min_amount");
    limitRows @5 :Int32 $Proxy.name("limit_rows");
    showInactive @6 :Bool $Proxy.name("show_inactive");
    showOrphans @7 :Bool $Proxy.name("show_orphans");
}

# --- Transaction record (TransactionRecord + nested TransactionStatus) ---

struct TransactionStatus $Proxy.wrap("TransactionStatus") {
    countsForBalance @0 :Bool;
    sortKey @1 :Text;
    maturesIn @2 :Int32 $Proxy.name("matures_in");
    status @3 :Int32;
    generatedType @4 :Int32 $Proxy.name("generated_type");
    depth @5 :Int64;
    openFor @6 :Int64 $Proxy.name("open_for");
    curNumBlocks @7 :Int32 $Proxy.name("cur_num_blocks");
}

struct TransactionRecord $Proxy.wrap("TransactionRecord") {
    hash @0 :Data;
    time @1 :Int64;
    type @2 :Int32;
    address @3 :Text;
    debit @4 :Int64;
    credit @5 :Int64;
    vout @6 :UInt32;
    idx @7 :Int32;
    status @8 :TransactionStatus;
    label @9 :Text;
}

# --- Windowed read result (GRC::RowsResult) ---

struct RowsResult $Proxy.wrap("GRC::RowsResult") {
    records @0 :List(TransactionRecord);
    totalAccepted @1 :Int32 $Proxy.name("total_accepted");
    epoch @2 :UInt64;
    highWater @3 :UInt64 $Proxy.name("high_water");
}

# --- Event payloads (GRC::*Payload) ---

struct RowsInsertedPayload $Proxy.wrap("GRC::RowsInsertedPayload") {
    position @0 :Int32;
    records @1 :List(TransactionRecord);
    viewId @2 :Int32;
    epoch @3 :UInt64;
}

struct RowsRemovedPayload $Proxy.wrap("GRC::RowsRemovedPayload") {
    position @0 :Int32;
    count @1 :Int32;
    viewId @2 :Int32;
    epoch @3 :UInt64;
}

struct ChainTipChangedPayload $Proxy.wrap("GRC::ChainTipChangedPayload") {
    height @0 :Int32;
    bestTime @1 :Int64 $Proxy.name("best_time");
}

struct RowsResetPayload $Proxy.wrap("GRC::RowsResetPayload") {
    viewId @0 :Int32;
    epoch @1 :UInt64;
    total @2 :Int32;
}

struct RowCountChangedPayload $Proxy.wrap("GRC::RowCountChangedPayload") {
    viewId @0 :Int32;
    epoch @1 :UInt64;
    totalAccepted @2 :Int32 $Proxy.name("total_accepted");
}

struct RowsChangedPayload $Proxy.wrap("GRC::RowsChangedPayload") {
    viewId @0 :Int32;
    epoch @1 :UInt64;
    first @2 :Int32;
    count @3 :Int32;
    # Appended (new ordinal, so old peers simply do not see it): the changed rows
    # sampled at emission. Consumers apply these directly instead of calling
    # getRows() when the event lands -- see GRC::RowsChangedPayload for why
    # re-fetching at apply time was both stale and a round trip per change.
    records @4 :List(TransactionRecord);
}

# WalletEventPayload = std::variant<the six payloads>, marshalled by type-variant.h
# as a discriminated struct: `which` (the active alternative index) plus one
# field per alternative, in the SAME order as the C++ std::variant. Synthetic
# wire type -- no $Proxy.wrap.
struct WalletEventPayloadWire {
    which @0 :UInt16;
    rowsInserted @1 :RowsInsertedPayload;
    rowsRemoved @2 :RowsRemovedPayload;
    chainTipChanged @3 :ChainTipChangedPayload;
    rowsReset @4 :RowsResetPayload;
    rowCountChanged @5 :RowCountChangedPayload;
    rowsChanged @6 :RowsChangedPayload;
}

struct WalletEvent $Proxy.wrap("GRC::WalletEvent") {
    seqno @0 :UInt64;
    emitTimeUs @1 :Int64 $Proxy.name("emit_time_us");
    payload @2 :WalletEventPayloadWire;
}

# --- Transaction detail (GRC::WalletTxDetail + nested DebitOut / InputInfo) ---

# Generic string pair for WalletTxDetail.stake_info (vector<pair<string,string>>).
struct StrPair {
    first @0 :Text;
    second @1 :Text;
}

struct DebitOut $Proxy.wrap("GRC::WalletTxDetail::DebitOut") {
    hasAddress @0 :Bool $Proxy.name("has_address");
    address @1 :Text;
    label @2 :Text;
    amount @3 :Int64;
}

struct InputInfo $Proxy.wrap("GRC::WalletTxDetail::InputInfo") {
    hasAddress @0 :Bool $Proxy.name("has_address");
    address @1 :Text;
    amount @2 :Int64;
    isMine @3 :Bool $Proxy.name("is_mine");
}

struct WalletTxDetail $Proxy.wrap("GRC::WalletTxDetail") {
    found @0 :Bool;
    finality @1 :Int32;
    finalityValue @2 :Int64 $Proxy.name("finality_value");
    depth @3 :Int32;
    requests @4 :Int32;
    time @5 :Int64;
    source @6 :Int32;
    generatedType @7 :Int32 $Proxy.name("generated_type");
    fromValue @8 :Text $Proxy.name("from_value");
    hasOwnCreditLine @9 :Bool $Proxy.name("has_own_credit_line");
    ownCreditAddress @10 :Text $Proxy.name("own_credit_address");
    ownCreditLabel @11 :Text $Proxy.name("own_credit_label");
    hasTo @12 :Bool $Proxy.name("has_to");
    toValue @13 :Text $Proxy.name("to_value");
    toLabel @14 :Text $Proxy.name("to_label");
    amountForm @15 :Int32 $Proxy.name("amount_form");
    unmatured @16 :Int64;
    inMainChain @17 :Bool $Proxy.name("in_main_chain");
    maturesIn @18 :Int32 $Proxy.name("matures_in");
    debits @19 :List(DebitOut);
    toSelf @20 :Bool $Proxy.name("to_self");
    selfValue @21 :Int64 $Proxy.name("self_value");
    hasFee @22 :Bool $Proxy.name("has_fee");
    fee @23 :Int64;
    mixedDebits @24 :List(Int64) $Proxy.name("mixed_debits");
    mixedCredits @25 :List(Int64) $Proxy.name("mixed_credits");
    net @26 :Int64;
    mapMessage @27 :Text $Proxy.name("map_message");
    mapComment @28 :Text $Proxy.name("map_comment");
    txid @29 :Text;
    hasBlockHash @30 :Bool $Proxy.name("has_block_hash");
    blockHash @31 :Text $Proxy.name("block_hash");
    contractMessage @32 :Text $Proxy.name("contract_message");
    isGenerated @33 :Bool $Proxy.name("is_generated");
    stakeInfo @34 :List(StrPair) $Proxy.name("stake_info");
    verboseDebits @35 :List(Int64) $Proxy.name("verbose_debits");
    verboseCredits @36 :List(Int64) $Proxy.name("verbose_credits");
    txDump @37 :Text $Proxy.name("tx_dump");
    inputs @38 :List(InputInfo);
}
