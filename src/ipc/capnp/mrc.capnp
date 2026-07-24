# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xc1d2e3f405162738;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/mrc.h");
$Proxy.includeTypes("ipc/capnp/mrc-types.h");

using Handler = import "handler.capnp";
using Node = import "node.capnp";

# Mirrors interfaces::MRC (src/interfaces/mrc.h), the Manual Research Claim
# boundary. CAmount is int64_t and crosses as Int64. handleMRCChanged reuses
# Node.VoidCallback (also ProxyCallback<std::function<void()>>) so the two
# schemas do not register two capnp interfaces for the same C++ type.
interface MRC $Proxy.wrap("interfaces::MRC") {
    destroy @0 (context :Proxy.Context) -> ();

    snapshot @1 (context :Proxy.Context, feeBoost :Int64, walletLocked :Bool, researcherEligible :Bool) -> (result :MRCSnapshot);
    submit @2 (context :Proxy.Context, feeBoost :Int64, walletLocked :Bool) -> (result :MRCSubmitResult);
    isOutOfSync @3 (context :Proxy.Context) -> (result :Bool);
    handleMRCChanged @4 (context :Proxy.Context, callback :Node.VoidCallback) -> (result :Handler.Handler);
}

struct MRCSnapshot $Proxy.wrap("interfaces::MRCSnapshot") {
    outOfSync @0 :Bool $Proxy.name("out_of_sync");
    blockHeight @1 :Int32 $Proxy.name("block_height");
    blockVersionValid @2 :Bool $Proxy.name("block_version_valid");
    outputLimit @3 :Int32 $Proxy.name("output_limit");
    createError @4 :Bool $Proxy.name("create_error");
    createErrorMsg @5 :Text $Proxy.name("create_error_msg");
    boostedFeeError @6 :Bool $Proxy.name("boosted_fee_error");
    boostedFeeErrorMsg @7 :Text $Proxy.name("boosted_fee_error_msg");
    reward @8 :Int64;
    minFee @9 :Int64 $Proxy.name("min_fee");
    fee @10 :Int64;
    queueLength @11 :Int32 $Proxy.name("queue_length");
    pos @12 :Int32;
    foundInQueue @13 :Bool $Proxy.name("found_in_queue");
    queueHeadFee @14 :Int64 $Proxy.name("queue_head_fee");
    queueTailFee @15 :Int64 $Proxy.name("queue_tail_fee");
    queuePayLimitFee @16 :Int64 $Proxy.name("queue_pay_limit_fee");
}

struct MRCSubmitResult $Proxy.wrap("interfaces::MRCSubmitResult") {
    success @0 :Bool;
    error @1 :Text;
    submittedHeight @2 :Int32 $Proxy.name("submitted_height");
    researchSubsidy @3 :Int64 $Proxy.name("research_subsidy");
}
