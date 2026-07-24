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
using Wallet = import "wallet.capnp";
using WalletTxSource = import "wallet_tx_source.capnp";
using MRC = import "mrc.capnp";
using Voting = import "voting.capnp";
using Researcher = import "researcher.capnp";

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

    # Connect handshake (doc/multiprocess_design.md section 4.3). authenticate is
    # the first call; the node serves build/identity info (and the makeX
    # capabilities) only to an authenticated peer.
    authenticate @4 (context :Proxy.Context, cookie :Text) -> (result :Bool);
    getBuildInfo @5 (context :Proxy.Context) -> (result :BuildInfo);
    getIdentity @6 (context :Proxy.Context) -> (result :NodeIdentity);
    makeWallet @7 (context :Proxy.Context) -> (result :Wallet.Wallet);
    makeWalletTxSource @8 (context :Proxy.Context) -> (result :WalletTxSource.WalletTxSource);
    makeMRC @9 (context :Proxy.Context) -> (result :MRC.MRC);
    makeVotingManager @10 (context :Proxy.Context) -> (result :Voting.VotingManager);
    makeResearcherContext @11 (context :Proxy.Context) -> (result :Researcher.ResearcherContext);
}

struct BuildInfo $Proxy.wrap("interfaces::BuildInfo") {
    gitCommit @0 :Text $Proxy.name("git_commit");
    builtAt @1 :Text $Proxy.name("built_at");
    schemaMajor @2 :UInt32 $Proxy.name("schema_major");
    schemaMinor @3 :UInt32 $Proxy.name("schema_minor");
    protocolVersion @4 :UInt32 $Proxy.name("protocol_version");
}

struct NodeIdentity $Proxy.wrap("interfaces::NodeIdentity") {
    nodeId @0 :Text $Proxy.name("node_id");
    network @1 :Text;
    datadir @2 :Text;
    genesisHash @3 :Text $Proxy.name("genesis_hash");
}
