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
using WalletCoinSource = import "wallet_coin_source.capnp";
using MRC = import "mrc.capnp";
using Voting = import "voting.capnp";
using Researcher = import "researcher.capnp";
using PSGT = import "psgt.capnp";
using SideStake = import "sidestake.capnp";

# Per-process bootstrap interface (interfaces::Init). construct @0 is the
# libmultiprocess lifecycle entry point (ThreadMap exchange); the makeX methods
# hand out the other interface capabilities.
#
# This is no longer a grow-per-migration subset: EVERY virtual on
# interfaces::Init must appear here, and test/lint/lint-serve-init-complete.py
# enforces it. A virtual with no ordinal is not "not migrated yet" -- mpgen
# generates no override for it on ProxyClient<Init>, so the call resolves to the
# base default, returns nullptr, and the capability silently does not exist over
# IPC. The coin channel shipped exactly that way and the multiprocess GUI could
# not start at all. isAuthenticated is the one deliberate omission (the local
# listener asks it before authentication); it is recorded in that lint's UNSERVED
# set with the reason.
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
    makePSGTPoolContext @12 (context :Proxy.Context) -> (result :PSGT.PSGTPoolContext);
    makeSideStakeManager @13 (context :Proxy.Context) -> (result :SideStake.SideStakeManager);
    makeWalletCoinSource @14 (context :Proxy.Context) -> (result :WalletCoinSource.WalletCoinSource);
}

struct BuildInfo $Proxy.wrap("interfaces::BuildInfo") {
    gitCommit @0 :Text $Proxy.name("git_commit");
    builtAt @1 :Text $Proxy.name("built_at");
    schemaMajor @2 :UInt32 $Proxy.name("schema_major");
    schemaMinor @3 :UInt32 $Proxy.name("schema_minor");
    protocolVersion @4 :UInt32 $Proxy.name("protocol_version");
}

# Ordinals renumbered under the schema-major 2 bump (identity model re-based onto
# the wallet UUID; former nodeId/datadir/genesisHash retired). Cap'n Proto requires
# consecutive ordinals from 0, so @0/@1 are reused (both Text) rather than left as
# gaps; safe because nothing persists this struct (live IPC only) and a stale peer
# hard-fails on schema_major before reading it.
struct NodeIdentity $Proxy.wrap("interfaces::NodeIdentity") {
    identityToken @0 :Text $Proxy.name("identity_token");
    network @1 :Text;
}
