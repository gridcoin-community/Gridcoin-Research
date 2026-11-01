# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xb2c3d4e5f6071829;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/voting.h");
$Proxy.includeTypes("ipc/capnp/voting-types.h");

using Handler = import "handler.capnp";

# Mirrors interfaces::VotingManager (src/interfaces/voting.h), the voting
# boundary. The DTOs carry enum values as their raw ints (the C++ members are
# already int, not the enum types); CAmount crosses as Int64. PollTableItem's
# std::optional<bool> validated is a `validated` field paired with `hasValidated`
# (the libmultiprocess presence-companion convention -- the has field is skipped
# as a member and marks validated FIELD_OPTIONAL).
interface VotingManager $Proxy.wrap("interfaces::VotingManager") {
    destroy @0 (context :Proxy.Context) -> ();

    buildPollTable @1 (context :Proxy.Context, filterFlags :Int32) -> (result :List(PollTableItem));
    getPollTypes @2 (context :Proxy.Context) -> (result :List(PollTypeMeta));
    pollV3Enabled @3 (context :Proxy.Context) -> (result :Bool);
    estimatePollFee @4 (context :Proxy.Context) -> (result :Int64);
    currentPollTitle @5 (context :Proxy.Context) -> (result :Text);
    latestActivePollTime @6 (context :Proxy.Context) -> (result :Int64);
    submitPoll @7 (context :Proxy.Context, poll :PollSubmission) -> (result :VotingSubmitResult);
    submitVote @8 (context :Proxy.Context, pollTxid :Text, choiceOffsets :Data) -> (result :VotingSubmitResult);
    handleNewPollReceived @9 (context :Proxy.Context, callback :NewPollReceivedCallback) -> (result :Handler.Handler);
    handleNewVoteReceived @10 (context :Proxy.Context, callback :NewVoteReceivedCallback) -> (result :Handler.Handler);
}

# --- Notification callbacks (interfaces::VotingManager::*Fn) ---

interface NewPollReceivedCallback $Proxy.wrap("ProxyCallback<std::function<void(int64_t)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, pollTime :Int64) -> ();
}

interface NewVoteReceivedCallback $Proxy.wrap("ProxyCallback<std::function<void(std::string)>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, pollTxid :Text) -> ();
}

# --- Value DTOs (interfaces:: structs in voting.h) ---

struct PollTypeMeta $Proxy.wrap("interfaces::PollTypeMeta") {
    type @0 :Int32;
    name @1 :Text;
    description @2 :Text;
    minDurationDays @3 :Int32 $Proxy.name("min_duration_days");
    requiredFields @4 :List(Text) $Proxy.name("required_fields");
}

struct PollChoiceResult $Proxy.wrap("interfaces::PollChoiceResult") {
    label @0 :Text;
    votes @1 :Float64;
    weight @2 :UInt64;
}

struct PollAdditionalField $Proxy.wrap("interfaces::PollAdditionalField") {
    name @0 :Text;
    value @1 :Text;
    required @2 :Bool;
}

struct PollSelfVoteResponse $Proxy.wrap("interfaces::PollSelfVoteResponse") {
    choiceOffset @0 :UInt8 $Proxy.name("choice_offset");
    weight @1 :UInt64;
}

struct PollTableItem $Proxy.wrap("interfaces::PollTableItem") {
    txid @0 :Text;
    payloadVersion @1 :UInt32 $Proxy.name("payload_version");
    typeStr @2 :Text $Proxy.name("type_str");
    title @3 :Text;
    question @4 :Text;
    url @5 :Text;
    startTime @6 :Int64 $Proxy.name("start_time");
    expiration @7 :Int64;
    durationDays @8 :UInt32 $Proxy.name("duration_days");
    weightType @9 :Int32 $Proxy.name("weight_type");
    weightTypeStr @10 :Text $Proxy.name("weight_type_str");
    responseType @11 :Text $Proxy.name("response_type");
    topAnswer @12 :Text $Proxy.name("top_answer");
    totalVotes @13 :UInt32 $Proxy.name("total_votes");
    totalWeight @14 :UInt64 $Proxy.name("total_weight");
    activeWeight @15 :UInt64 $Proxy.name("active_weight");
    votePercentAvw @16 :Float64 $Proxy.name("vote_percent_avw");
    validated @17 :Bool;
    hasValidated @18 :Bool;
    finished @19 :Bool;
    multipleChoice @20 :Bool $Proxy.name("multiple_choice");
    additionalFields @21 :List(PollAdditionalField) $Proxy.name("additional_fields");
    choices @22 :List(PollChoiceResult);
    selfVoted @23 :Bool $Proxy.name("self_voted");
    selfVoteResponses @24 :List(PollSelfVoteResponse) $Proxy.name("self_vote_responses");
}

struct PollSubmission $Proxy.wrap("interfaces::PollSubmission") {
    type @0 :Int32;
    title @1 :Text;
    durationDays @2 :Int32 $Proxy.name("duration_days");
    question @3 :Text;
    url @4 :Text;
    weightType @5 :Int32 $Proxy.name("weight_type");
    responseType @6 :Int32 $Proxy.name("response_type");
    choices @7 :List(Text);
    additionalFields @8 :List(PollAdditionalField) $Proxy.name("additional_fields");
}

struct VotingSubmitResult $Proxy.wrap("interfaces::VotingSubmitResult") {
    status @0 :Int32;
    txid @1 :Text;
    error @2 :Text;
}
