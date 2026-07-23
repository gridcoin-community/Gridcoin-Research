# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xe079c305988de2da;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/init.h");
$Proxy.includeTypes("ipc/capnp/init-types.h");

using Node = import "node.capnp";
using Staking = import "staking.capnp";

# Per-process bootstrap interface (interfaces::Init). construct @0 is the
# libmultiprocess lifecycle entry point (ThreadMap exchange); the makeX methods
# hand out the other interface capabilities. Only the subset whose return types
# are schema'd so far is exposed (grow-per-migration): the remaining factories
# (makeWallet / makeMRC / ...) are added as their interfaces get schemas.
interface Init $Proxy.wrap("interfaces::Init") {
    construct @0 (threadMap :Proxy.ThreadMap) -> (threadMap :Proxy.ThreadMap);
    isCoreReady @1 (context :Proxy.Context) -> (result :Bool);
    makeNode @2 (context :Proxy.Context) -> (result :Node.Node);
    makeStakingStatus @3 (context :Proxy.Context) -> (result :Staking.StakingStatus);
}
