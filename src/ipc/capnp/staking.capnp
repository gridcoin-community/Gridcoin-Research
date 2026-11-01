# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xd973814d231e38d7;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/staking.h");
$Proxy.includeTypes("ipc/capnp/staking-types.h");

# Mirrors interfaces::StakingStatus (src/interfaces/staking.h). The try* methods
# return std::optional<double>, expressed here as a (result, hasResult) pair.
interface StakingStatus $Proxy.wrap("interfaces::StakingStatus") {
    destroy @0 (context :Proxy.Context) -> ();
    isStaking @1 (context :Proxy.Context) -> (result :Bool);
    getErrors @2 (context :Proxy.Context) -> (result :Text);
    getCoinWeight @3 (context :Proxy.Context) -> (result :Float64);
    getNetworkWeight @4 (context :Proxy.Context) -> (result :Float64);
    tryGetNetworkWeight @5 (context :Proxy.Context) -> (result :Float64, hasResult :Bool);
    tryGetEstimatedTimeToStake @6 (context :Proxy.Context) -> (result :Float64, hasResult :Bool);
}
