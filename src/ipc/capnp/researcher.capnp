# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xa3b4c5d6e7f80912;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/researcher.h");
$Proxy.includeTypes("ipc/capnp/researcher-types.h");

using Handler = import "handler.capnp";
using Node = import "node.capnp";

# Mirrors interfaces::ResearcherContext (src/interfaces/researcher.h). Enum
# members (BeaconStatus, ResearcherProjectRow::WhitelistStatus/ErrorKind,
# ResearcherMode) cross as their integer values; CAmount as Int64. trySnapshot's
# std::optional<ResearcherSnapshot> return uses the boxed struct's own
# nullability (as wallet.capnp's optional coin-control param does). The two
# field-level optionals (accrual_near_limit, gdpr_controls) are primitives, so
# they use the presence-companion convention (a hasX field mpgen skips as a
# member). The four notification callbacks reuse node.capnp's callback interfaces
# so the shared C++ ProxyCallback types are registered once.
interface ResearcherContext $Proxy.wrap("interfaces::ResearcherContext") {
    destroy @0 (context :Proxy.Context) -> ();

    snapshot @1 (context :Proxy.Context) -> (result :ResearcherSnapshot);
    trySnapshot @2 (context :Proxy.Context) -> (result :ResearcherSnapshot);
    outOfSync @3 (context :Proxy.Context) -> (result :Bool);
    hasV3CapableProjects @4 (context :Proxy.Context) -> (result :Bool);
    projects @5 (context :Proxy.Context, extended :Bool) -> (result :List(ResearcherProjectRow));
    whitelistProjects @6 (context :Proxy.Context) -> (result :List(WhitelistProject));
    maxProjectNameLength @7 (context :Proxy.Context) -> (result :Int32);
    maxProjectUrlLength @8 (context :Proxy.Context) -> (result :Int32);
    v3CapableProjects @9 (context :Proxy.Context) -> (result :List(WhitelistProject));
    switchMode @10 (context :Proxy.Context, mode :Int32, email :Text) -> (result :Bool);
    advertiseBeacon @11 (context :Proxy.Context) -> (result :BeaconAdvertiseResult);
    generateBeaconKeyForV3 @12 (context :Proxy.Context) -> (result :Text);
    advertiseBeaconV3 @13 (context :Proxy.Context, ownershipProofXml :Text) -> (result :BeaconAdvertiseResult);
    reload @14 (context :Proxy.Context) -> ();
    handleResearcherChanged @15 (context :Proxy.Context, callback :Node.VoidCallback) -> (result :Handler.Handler);
    handleBeaconChanged @16 (context :Proxy.Context, callback :Node.VoidCallback) -> (result :Handler.Handler);
    handleAccrualChanged @17 (context :Proxy.Context, callback :Node.VoidCallback) -> (result :Handler.Handler);
    handleBlocksChanged @18 (context :Proxy.Context, callback :Node.NotifyBlocksChangedCallback) -> (result :Handler.Handler);
    activePools @19 (context :Proxy.Context) -> (result :List(PoolRow));
}

struct ResearcherSnapshot $Proxy.wrap("interfaces::ResearcherSnapshot") {
    cpid @0 :Text;
    hasCpid @1 :Bool $Proxy.name("has_cpid");
    hasSplitCpid @2 :Bool $Proxy.name("has_split_cpid");
    hasRac @3 :Bool $Proxy.name("has_rac");
    email @4 :Text;
    magnitude @5 :Float64;
    magnitudeText @6 :Text $Proxy.name("magnitude_text");
    hasMagnitude @7 :Bool $Proxy.name("has_magnitude");
    accrual @8 :Int64;
    accrualNearLimit @9 :Int64 $Proxy.name("accrual_near_limit");
    hasAccrualNearLimit @10 :Bool;
    configuredForNoncruncherMode @11 :Bool $Proxy.name("configured_for_noncruncher_mode");
    detectedPoolMode @12 :Bool $Proxy.name("detected_pool_mode");
    outOfSync @13 :Bool $Proxy.name("out_of_sync");
    miningStatus @14 :Text $Proxy.name("mining_status");
    hasEligibleProjects @15 :Bool $Proxy.name("has_eligible_projects");
    hasPoolProjects @16 :Bool $Proxy.name("has_pool_projects");
    actionNeeded @17 :Bool $Proxy.name("action_needed");
    beaconStatus @18 :Int32 $Proxy.name("beacon_status");
    beaconPresent @19 :Bool $Proxy.name("beacon_present");
    pendingBeaconPresent @20 :Bool $Proxy.name("pending_beacon_present");
    hasActiveBeacon @21 :Bool $Proxy.name("has_active_beacon");
    hasPendingBeacon @22 :Bool $Proxy.name("has_pending_beacon");
    hasRenewableBeacon @23 :Bool $Proxy.name("has_renewable_beacon");
    beaconExpired @24 :Bool $Proxy.name("beacon_expired");
    needsBeaconAuth @25 :Bool $Proxy.name("needs_beacon_auth");
    beaconAddress @26 :Text $Proxy.name("beacon_address");
    beaconVerificationCode @27 :Text $Proxy.name("beacon_verification_code");
    beaconError @28 :Text $Proxy.name("beacon_error");
    cachedBeaconPubkeyHex @29 :Text $Proxy.name("cached_beacon_pubkey_hex");
    beaconTimestamp @30 :Int64 $Proxy.name("beacon_timestamp");
    beaconAge @31 :Int64 $Proxy.name("beacon_age");
    timeToBeaconExpiration @32 :Int64 $Proxy.name("time_to_beacon_expiration");
    timeToPendingBeaconExpiration @33 :Int64 $Proxy.name("time_to_pending_beacon_expiration");
    boincDataDir @34 :Text $Proxy.name("boinc_data_dir");
    isV14Enabled @35 :Bool $Proxy.name("is_v14_enabled");
}

struct ResearcherProjectRow $Proxy.wrap("interfaces::ResearcherProjectRow") {
    whitelisted @0 :Int32;
    errorKind @1 :Int32 $Proxy.name("error_kind");
    gdprControls @2 :Bool $Proxy.name("gdpr_controls");
    hasGdprControls @3 :Bool;
    name @4 :Text;
    cpid @5 :Text;
    magnitude @6 :Float64;
    rac @7 :Float64;
    errorMessage @8 :Text $Proxy.name("error_message");
}

struct BeaconAdvertiseResult $Proxy.wrap("interfaces::BeaconAdvertiseResult") {
    status @0 :Int32;
}

struct WhitelistProject $Proxy.wrap("interfaces::WhitelistProject") {
    name @0 :Text;
    url @1 :Text;
}

struct PoolRow $Proxy.wrap("interfaces::PoolRow") {
    name @0 :Text;
    url @1 :Text;
}
