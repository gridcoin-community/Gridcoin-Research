# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xb5bee761e6259756;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/gui_state.h");
$Proxy.includeTypes("ipc/capnp/gui_state-types.h");

using Handler = import "handler.capnp";
using Mrc = import "mrc.capnp";
using PSGT = import "psgt.capnp";
using Researcher = import "researcher.capnp";
using Wallet = import "wallet.capnp";

# Mirrors interfaces::GuiState (src/interfaces/gui_state.h): the state the GUI
# shows that needs cs_main, computed by the node's GUI state cache and pushed to
# the GUI, and the GUI's requests of it. Its own file, not node.capnp: the
# snapshot nests the MRC, researcher, PSGT and wallet structs, and mrc, researcher
# and wallet.capnp import node.capnp, so node.capnp cannot import them back.
# CAmount crosses as Int64.
interface GuiState $Proxy.wrap("interfaces::GuiState") {
    destroy @0 (context :Proxy.Context) -> ();

    current @1 (context :Proxy.Context) -> (result :GuiStateSnapshot);
    handleChanged @2 (context :Proxy.Context, callback :ChangedCallback) -> (result :Handler.Handler);
    requestMRCQuote @3 (context :Proxy.Context, feeBoost :Int64, requestId :UInt64) -> ();
    handleMRCQuote @4 (context :Proxy.Context, callback :MRCQuoteCallback) -> (result :Handler.Handler);
    requestUpdate @5 (context :Proxy.Context) -> ();
}

# The node calls these back on its cache's timer thread. Each carries the whole
# value: unlike the payload-free refetch triggers, a push is the data itself.
interface ChangedCallback $Proxy.wrap("ProxyCallback<std::function<void(const interfaces::GuiStateSnapshot&)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, snapshot :GuiStateSnapshot) -> ();
}

interface MRCQuoteCallback $Proxy.wrap("ProxyCallback<std::function<void(uint64_t, const interfaces::MRCSnapshot&)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, requestId :UInt64, snapshot :Mrc.MRCSnapshot) -> ();
}

# Every interfaces::GuiStateSnapshot member, by its C++ name. The three parts
# with a present flag take capnp names other than their C++ ones
# (walletBalances, mrcSnapshot, researcherSnapshot): mpgen reads a field `hasX`
# beside a field `x` as x's presence flag and maps no member to it (mergeFields
# in mp/gen.cpp), so the C++ flag it names would never cross. These flags are
# plain members beside plain values, and each must cross as itself.
struct GuiStateSnapshot $Proxy.wrap("interfaces::GuiStateSnapshot") {
    sequence @0 :UInt64;
    height @1 :Int32;
    tipTime @2 :Int64 $Proxy.name("tip_time");
    difficulty @3 :Float64;
    initialDownload @4 :Bool $Proxy.name("initial_download");
    outOfSync @5 :Bool $Proxy.name("out_of_sync");
    peersHeight @6 :Int32 $Proxy.name("peers_height");
    netWeight @7 :Float64 $Proxy.name("net_weight");
    staking @8 :Bool;
    coinWeight @9 :Float64 $Proxy.name("coin_weight");
    minerErrors @10 :Text $Proxy.name("miner_errors");
    hasBalances @11 :Bool $Proxy.name("has_balances");
    walletBalances @12 :Wallet.WalletBalances $Proxy.name("balances");
    psgtStatus @13 :PSGT.PSGTPoolStatus $Proxy.name("psgt_status");
    hasMrc @14 :Bool $Proxy.name("has_mrc");
    mrcSnapshot @15 :Mrc.MRCSnapshot $Proxy.name("mrc");
    hasEtts @16 :Bool $Proxy.name("has_etts");
    ettsSeconds @17 :Float64 $Proxy.name("etts_seconds");
    hasResearcher @18 :Bool $Proxy.name("has_researcher");
    researcherSnapshot @19 :Researcher.ResearcherSnapshot $Proxy.name("researcher");
}
