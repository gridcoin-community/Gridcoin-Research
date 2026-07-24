# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0x9a8b7c6d5e4f3021;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/psgt.h");
$Proxy.includeTypes("ipc/capnp/psgt-types.h");

# Mirrors interfaces::PSGTPoolContext (src/interfaces/psgt.h), the PSGT pool +
# multisig-workbench boundary. PSGTBytes (std::vector<unsigned char>, the
# serialized PSGT) crosses as Data; enum members (PSGTSignStatus, PSGTRelevance,
# PSGTInput{Amount,Sig}State, PSGTPoolAddStatus, PSGTPoolRejectReason) cross as
# their integer values; CAmount as Int64. signPoolEntry declares its input
# (image_hex) before its outputs (error, txid), so the params-then-results
# fields bind to the C++ arguments in order.
interface PSGTPoolContext $Proxy.wrap("interfaces::PSGTPoolContext") {
    destroy @0 (context :Proxy.Context) -> ();

    entries @1 (context :Proxy.Context) -> (result :List(PSGTPoolRow));
    signPoolEntry @2 (context :Proxy.Context, imageHex :Text) -> (error :Text, txid :Text, result :Int32);
    removePoolEntry @3 (context :Proxy.Context, imageHex :Text) -> (result :Bool);
    poolStatus @4 (context :Proxy.Context) -> (result :PSGTPoolStatus);
    poolEntryPsgt @5 (context :Proxy.Context, imageHex :Text) -> (result :Data);
    describePSGT @6 (context :Proxy.Context, psgt :Data) -> (result :PSGTDescription);
    signPSGT @7 (context :Proxy.Context, psgt :Data) -> (result :PSGTSignResult);
    combinePSGTs @8 (context :Proxy.Context, psgts :List(Data)) -> (result :PSGTCombineResult);
    submitPSGTToPool @9 (context :Proxy.Context, psgt :Data) -> (result :PSGTSubmitResult);
    finalizeToRawTxHex @10 (context :Proxy.Context, psgt :Data) -> (result :PSGTFinalizeResult);
    decodePSGT @11 (context :Proxy.Context, bytes :Data) -> (result :PSGTDecodeResult);
    walletHasSignature @12 (context :Proxy.Context, psgt :Data) -> (result :Bool);
    walletMustSignRevision @13 (context :Proxy.Context, revisionHex :Text) -> (result :Bool);
}

struct PSGTPoolStatus $Proxy.wrap("interfaces::PSGTPoolStatus") {
    active @0 :Bool;
    v15Enabled @1 :Bool $Proxy.name("v15_enabled");
    outOfSync @2 :Bool $Proxy.name("out_of_sync");
}

struct PSGTPoolRow $Proxy.wrap("interfaces::PSGTPoolRow") {
    imageHex @0 :Text $Proxy.name("image_hex");
    imageAddress @1 :Text $Proxy.name("image_address");
    revisionHex @2 :Text $Proxy.name("revision_hex");
    destination @3 :Text;
    amount @4 :Int64;
    validSigs @5 :Int32 $Proxy.name("valid_sigs");
    sigsRequired @6 :Int32 $Proxy.name("sigs_required");
    sigsTotal @7 :Int32 $Proxy.name("sigs_total");
    timeReceived @8 :Int64 $Proxy.name("time_received");
    inPool @9 :Bool $Proxy.name("in_pool");
    relevance @10 :Int32;
}

struct PSGTInputInfo $Proxy.wrap("interfaces::PSGTInputInfo") {
    prevoutHash @0 :Text $Proxy.name("prevout_hash");
    prevoutN @1 :UInt32 $Proxy.name("prevout_n");
    hasMetadata @2 :Bool $Proxy.name("has_metadata");
    amountState @3 :Int32 $Proxy.name("amount_state");
    amount @4 :Int64;
    sigState @5 :Int32 $Proxy.name("sig_state");
    partialSigCount @6 :Int32 $Proxy.name("partial_sig_count");
    hasRedeem @7 :Bool $Proxy.name("has_redeem");
    p2shAddress @8 :Text $Proxy.name("p2sh_address");
    p2shHashHex @9 :Text $Proxy.name("p2sh_hash_hex");
    isMultisig @10 :Bool $Proxy.name("is_multisig");
    multisigM @11 :Int32 $Proxy.name("multisig_m");
    multisigN @12 :Int32 $Proxy.name("multisig_n");
}

struct PSGTOutputInfo $Proxy.wrap("interfaces::PSGTOutputInfo") {
    isStandard @0 :Bool $Proxy.name("is_standard");
    destinations @1 :List(Text);
    nRequired @2 :Int32 $Proxy.name("n_required");
    amount @3 :Int64;
}

struct PSGTDescription $Proxy.wrap("interfaces::PSGTDescription") {
    valid @0 :Bool;
    error @1 :Text;
    txid @2 :Text;
    version @3 :Int32;
    time @4 :UInt32;
    lockTime @5 :UInt32 $Proxy.name("lock_time");
    inputs @6 :List(PSGTInputInfo);
    outputs @7 :List(PSGTOutputInfo);
    signedInputCount @8 :Int32 $Proxy.name("signed_input_count");
    inputCount @9 :Int32 $Proxy.name("input_count");
    complete @10 :Bool;
}

struct PSGTSignResult $Proxy.wrap("interfaces::PSGTSignResult") {
    psgt @0 :Data;
    ok @1 :Bool;
    complete @2 :Bool;
    signedNow @3 :Int32 $Proxy.name("signed_now");
    needsData @4 :Int32 $Proxy.name("needs_data");
    couldNotSign @5 :Int32 $Proxy.name("could_not_sign");
    error @6 :Text;
}

struct PSGTCombineResult $Proxy.wrap("interfaces::PSGTCombineResult") {
    psgt @0 :Data;
    ok @1 :Bool;
    error @2 :Text;
}

struct PSGTSubmitResult $Proxy.wrap("interfaces::PSGTSubmitResult") {
    decoded @0 :Bool;
    validated @1 :Bool;
    addStatus @2 :Int32 $Proxy.name("add_status");
    rejectReason @3 :Int32 $Proxy.name("reject_reason");
    rejectText @4 :Text $Proxy.name("reject_text");
    revisionHex @5 :Text $Proxy.name("revision_hex");
    error @6 :Text;
    accepted @7 :Bool;
}

struct PSGTFinalizeResult $Proxy.wrap("interfaces::PSGTFinalizeResult") {
    rawTxHex @0 :Text $Proxy.name("raw_tx_hex");
    complete @1 :Bool;
    error @2 :Text;
}

struct PSGTDecodeResult $Proxy.wrap("interfaces::PSGTDecodeResult") {
    psgt @0 :Data;
    ok @1 :Bool;
    error @2 :Text;
}
