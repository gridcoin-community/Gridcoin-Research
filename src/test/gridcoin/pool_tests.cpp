// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/pool.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/researcher.h"
#include "chainparams.h"
#include "key.h"
#include "streams.h"

#include <boost/test/unit_test.hpp>
#include <map>
#include <set>
#include <vector>

extern GRC::MiningPools g_mining_pools;

namespace {

//!
//! \brief Local helper for constructing operator keys and CPIDs that match
//! across multiple test cases. Mirrors the TestKey helper in
//! beacon_tests.cpp without depending on its specific key bytes.
//!
struct PoolTestKey
{
    //!
    //! \brief Create a valid private key for tests. MakeNewKey uses the
    //! RNG, so a fresh random key is generated per call — signatures are
    //! NOT reproducible across runs. Tests that need to verify a
    //! signature must call Private() once and reuse the returned key.
    //!
    static CKey Private()
    {
        CKey key;
        key.MakeNewKey(true);
        return key;
    }

    static GRC::Cpid Cpid()
    {
        return GRC::Cpid::Parse("00010203040506070809101112131415");
    }
};

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(pool_tests)

// -----------------------------------------------------------------------------
// PoolStatus
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pool_status_to_string_covers_all_named_values)
{
    GRC::Pool pool;

    pool.m_status = GRC::PoolStatus::UNKNOWN;
    BOOST_CHECK(!pool.StatusToString().empty());

    pool.m_status = GRC::PoolStatus::PENDING;
    BOOST_CHECK(!pool.StatusToString().empty());

    pool.m_status = GRC::PoolStatus::ACTIVE;
    BOOST_CHECK(!pool.StatusToString().empty());

    pool.m_status = GRC::PoolStatus::DELETED;
    BOOST_CHECK(!pool.StatusToString().empty());
}

// -----------------------------------------------------------------------------
// PoolRegisterPayload
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(register_payload_serialization_round_trip)
{
    CKey private_key = PoolTestKey::Private();
    const CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload original(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(original.Sign(private_key));

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    original.Serialize(stream, GRC::ContractAction::ADD);

    GRC::PoolRegisterPayload decoded;
    decoded.Unserialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK(decoded.m_cpid == original.m_cpid);
    BOOST_CHECK_EQUAL(decoded.m_name, original.m_name);
    BOOST_CHECK_EQUAL(decoded.m_url, original.m_url);
    BOOST_CHECK(decoded.m_operator_key == original.m_operator_key);
    BOOST_CHECK(decoded.m_signature == original.m_signature);
    BOOST_CHECK_EQUAL(decoded.m_version, GRC::PoolRegisterPayload::CURRENT_VERSION);
}

BOOST_AUTO_TEST_CASE(register_payload_well_formed_accepts_valid_add)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD));
}

BOOST_AUTO_TEST_CASE(register_payload_well_formed_rejects_empty_name)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::ADD));
}

BOOST_AUTO_TEST_CASE(register_payload_well_formed_rejects_empty_url_on_add)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::ADD));
}

BOOST_AUTO_TEST_CASE(register_payload_well_formed_allows_empty_url_on_remove)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::REMOVE));
}

BOOST_AUTO_TEST_CASE(register_payload_well_formed_rejects_invalid_operator_key)
{
    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        CPubKey{} /* default-constructed, not valid */);

    // Without a valid operator key, Sign() should not produce a verifiable
    // signature anyway; even forcing a fake signature in shouldn't pass
    // WellFormed because m_operator_key.IsFullyValid() returns false.
    payload.m_signature.assign(72, 0xAA);

    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::ADD));
}

BOOST_AUTO_TEST_CASE(register_payload_well_formed_rejects_signature_size_out_of_band)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);

    payload.m_signature.assign(32, 0xAA); // Below the 64-byte minimum.
    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::ADD));

    payload.m_signature.assign(80, 0xAA); // Above the 73-byte maximum.
    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::ADD));
}

BOOST_AUTO_TEST_CASE(register_payload_verify_signature_round_trip)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    BOOST_CHECK(payload.VerifySignature(public_key));
}

BOOST_AUTO_TEST_CASE(register_payload_verify_signature_rejects_tampered_field)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    payload.m_url = "https://evil-pool-takeover.example/";
    BOOST_CHECK(!payload.VerifySignature(public_key));
}

BOOST_AUTO_TEST_CASE(register_payload_verify_signature_rejects_other_key)
{
    CKey private_key = PoolTestKey::Private();
    CKey different_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();
    CPubKey other_pubkey = different_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    BOOST_CHECK(!payload.VerifySignature(other_pubkey));
}

BOOST_AUTO_TEST_CASE(register_payload_verify_signature_rejects_default_key)
{
    CKey private_key = PoolTestKey::Private();
    CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(payload.Sign(private_key));

    // Pass a default-constructed (invalid) public key — VerifySignature
    // must reject without crashing.
    BOOST_CHECK(!payload.VerifySignature(CPubKey{}));
}

BOOST_AUTO_TEST_CASE(register_payload_contract_type_is_pool_register)
{
    GRC::PoolRegisterPayload payload;
    BOOST_CHECK(payload.ContractType() == GRC::ContractType::POOL_REGISTER);
}

BOOST_AUTO_TEST_CASE(register_payload_burn_amount_is_100_grc)
{
    // Pool registration is a heavier on-chain commitment than a beacon
    // (operator trusted with BOINC aggregation, infrastructure, policy
    // honored over time). 100 GRC matches the REGISTRATION_BURN
    // constant; if a future PR retunes it, this test becomes a
    // documentation tripwire alongside doc/consensus.md §11. Symmetric
    // for ADD and REMOVE — the spam control on REMOVE is the
    // existing-key signature check, not the burn.
    GRC::PoolRegisterPayload payload;
    BOOST_CHECK_EQUAL(payload.RequiredBurnAmount(), 100 * COIN);
    BOOST_CHECK_EQUAL(GRC::PoolRegisterPayload::REGISTRATION_BURN, 100 * COIN);
}

// -----------------------------------------------------------------------------
// PoolApprovePayload
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(approve_payload_serialization_round_trip)
{
    GRC::PoolApprovePayload original(PoolTestKey::Cpid());

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    original.Serialize(stream, GRC::ContractAction::ADD);

    GRC::PoolApprovePayload decoded;
    decoded.Unserialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK(decoded.m_cpid == original.m_cpid);
    BOOST_CHECK_EQUAL(decoded.m_version, GRC::PoolApprovePayload::CURRENT_VERSION);
}

BOOST_AUTO_TEST_CASE(approve_payload_well_formed_accepts_add_and_remove)
{
    GRC::PoolApprovePayload payload(PoolTestKey::Cpid());

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD));
    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::REMOVE));
}

BOOST_AUTO_TEST_CASE(approve_payload_well_formed_rejects_unknown_action)
{
    GRC::PoolApprovePayload payload(PoolTestKey::Cpid());

    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::UNKNOWN));
}

BOOST_AUTO_TEST_CASE(approve_payload_well_formed_accepts_open_with_valid_auth_key)
{
    CKey op_priv = PoolTestKey::Private();
    GRC::PoolApprovePayload payload(PoolTestKey::Cpid(), op_priv.GetPubKey());

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::OPEN));
}

BOOST_AUTO_TEST_CASE(approve_payload_well_formed_rejects_open_with_invalid_auth_key)
{
    // Finding N: an OPEN payload with a default-constructed (invalid)
    // m_authorized_operator_key would silently do nothing — the IsBuiltin
    // guard's auth.IsValid() check would reject every claim attempt. Fail
    // fast at the WellFormed gate.
    GRC::PoolApprovePayload payload(PoolTestKey::Cpid(), CPubKey{} /* invalid */);

    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::OPEN));
}

BOOST_AUTO_TEST_CASE(approve_payload_serialization_round_trip_open_includes_auth_key)
{
    CKey op_priv = PoolTestKey::Private();
    GRC::PoolApprovePayload original(PoolTestKey::Cpid(), op_priv.GetPubKey());

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    original.Serialize(stream, GRC::ContractAction::OPEN);

    GRC::PoolApprovePayload decoded;
    decoded.Unserialize(stream, GRC::ContractAction::OPEN);

    BOOST_CHECK(decoded.m_cpid == original.m_cpid);
    BOOST_CHECK(decoded.m_authorized_operator_key == original.m_authorized_operator_key);
}

BOOST_AUTO_TEST_CASE(approve_payload_serialization_add_excludes_auth_key_field)
{
    // The conditional serialization in PoolApprovePayload::SerializationOp
    // means ADD/REMOVE wire-shape is (version, cpid) = 20 bytes. An OPEN
    // payload extends to (version, cpid, pubkey) = 53 bytes. Setting an
    // auth key on an ADD payload should NOT change the serialized size
    // (key omitted on the wire).
    CKey op_priv = PoolTestKey::Private();
    GRC::PoolApprovePayload with_auth(PoolTestKey::Cpid(), op_priv.GetPubKey());
    GRC::PoolApprovePayload without_auth(PoolTestKey::Cpid());

    CDataStream stream_with(SER_NETWORK, PROTOCOL_VERSION);
    CDataStream stream_without(SER_NETWORK, PROTOCOL_VERSION);
    with_auth.Serialize(stream_with, GRC::ContractAction::ADD);
    without_auth.Serialize(stream_without, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(stream_with.size(), stream_without.size());
    BOOST_CHECK_EQUAL(stream_with.size(), 4u + 16u); // version (uint32) + cpid (16 bytes)
}

BOOST_AUTO_TEST_CASE(approve_payload_contract_type_is_pool_approve)
{
    GRC::PoolApprovePayload payload;
    BOOST_CHECK(payload.ContractType() == GRC::ContractType::POOL_APPROVE);
}

// -----------------------------------------------------------------------------
// Contract::RequiresMasterKey alignment
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pool_register_does_not_require_master_key)
{
    GRC::Contract contract = GRC::MakeContract<GRC::PoolRegisterPayload>(
        GRC::ContractAction::ADD,
        GRC::PoolRegisterPayload(
            PoolTestKey::Cpid(),
            "grcpool.com",
            "https://grcpool.com/",
            PoolTestKey::Private().GetPubKey()));

    BOOST_CHECK(!contract.RequiresMasterKey());
}

BOOST_AUTO_TEST_CASE(pool_approve_requires_master_key)
{
    GRC::Contract approve_add = GRC::MakeContract<GRC::PoolApprovePayload>(
        GRC::ContractAction::ADD,
        GRC::PoolApprovePayload(PoolTestKey::Cpid()));

    GRC::Contract approve_remove = GRC::MakeContract<GRC::PoolApprovePayload>(
        GRC::ContractAction::REMOVE,
        GRC::PoolApprovePayload(PoolTestKey::Cpid()));

    BOOST_CHECK(approve_add.RequiresMasterKey());
    BOOST_CHECK(approve_remove.RequiresMasterKey());
}

// -----------------------------------------------------------------------------
// PoolRegistry singleton + in-memory query API
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pool_registry_is_active_pool_returns_false_for_unknown_cpid)
{
    // The registry starts empty; lookups for an unregistered CPID return
    // false without touching LevelDB.
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    BOOST_CHECK(!registry.IsActivePool(PoolTestKey::Cpid()));
}

BOOST_AUTO_TEST_CASE(pool_registry_is_active_pool_name_returns_false_for_unknown_name)
{
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    BOOST_CHECK(!registry.IsActivePoolName("never-registered.example"));
}

BOOST_AUTO_TEST_CASE(pool_registry_active_pools_contains_grandfathered_builtins)
{
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    // The registry boots with the 5 builtin pools seeded by the
    // constructor (plan §3). All 5 are ACTIVE. (Registry::Add path
    // exercising via real contracts requires ContractContext +
    // CBlockIndex mocking — tracked as follow-up work in the test
    // plan / PR body.)
    const std::vector<GRC::Pool> active = registry.ActivePools();
    BOOST_CHECK_EQUAL(active.size(), GRC::PoolRegistry::BuiltinPoolSeeds().size());
}

// -----------------------------------------------------------------------------
// Grandfathered builtin pools (issue #1783 / plan §3, §3.5, §6, Q3, Q4)
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(builtin_pools_match_g_mining_pools_pre_v15)
{
    // Q3 shadow check: pre-V15, ActivePoolsAtHeight must yield exactly the
    // same set of pool CPIDs as the legacy g_mining_pools list, in the same
    // order. If this assertion fires, grandfathering has diverged from the
    // hardcoded list and AVW will fork the chain on the next pre-V15 poll.
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    // height 0 is sufficient: the seeds are all at m_height == 0, so the
    // chain-walk in ActivePoolsAtHeight stops on the seed for every CPID.
    const std::vector<GRC::Pool> registry_pools = registry.ActivePoolsAtHeight(0);
    const std::vector<GRC::MiningPool> legacy_pools = g_mining_pools.GetMiningPools();

    BOOST_REQUIRE_EQUAL(registry_pools.size(), legacy_pools.size());

    // The legacy list defines the canonical ordering. The registry's
    // ActivePoolsAtHeight returns entries in std::map<Cpid, ...> iteration
    // order (lexicographic by CPID bytes), which is NOT the same as the
    // legacy push-order. So compare as sets rather than as sequences —
    // matching CPID set with matching names/URLs is the consensus-relevant
    // invariant; ordering is incidental.
    std::map<GRC::Cpid, std::pair<std::string, std::string>> legacy_map;
    for (const GRC::MiningPool& p : legacy_pools) {
        legacy_map[p.m_cpid] = {p.m_name, p.m_url};
    }

    for (const GRC::Pool& p : registry_pools) {
        auto it = legacy_map.find(p.m_cpid);
        BOOST_REQUIRE(it != legacy_map.end());
        BOOST_CHECK_EQUAL(p.m_name, it->second.first);
        BOOST_CHECK_EQUAL(p.m_url, it->second.second);
    }
}

BOOST_AUTO_TEST_CASE(is_builtin_recognizes_every_seeded_cpid)
{
    // §3.5 protection relies on IsBuiltin recognising every seeded CPID.
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    for (const auto& seed : GRC::PoolRegistry::BuiltinPoolSeeds()) {
        const GRC::Cpid cpid = GRC::Cpid::Parse(seed.cpid_hex);
        BOOST_CHECK(registry.IsBuiltin(cpid));
    }

    // Sanity: a non-builtin test CPID should not register as builtin.
    BOOST_CHECK(!registry.IsBuiltin(PoolTestKey::Cpid()));
}

BOOST_AUTO_TEST_CASE(builtin_seed_hash_is_deterministic_and_unique)
{
    // The sentinel hash must be deterministic so that on-chain
    // m_previous_hash chains can terminate on it across runs / restarts.
    // It must also be unique per CPID so two builtin chains never alias.
    std::set<uint256> seen;

    for (const auto& seed : GRC::PoolRegistry::BuiltinPoolSeeds()) {
        const GRC::Cpid cpid = GRC::Cpid::Parse(seed.cpid_hex);

        const uint256 a = GRC::PoolRegistry::BuiltinSeedHash(cpid);
        const uint256 b = GRC::PoolRegistry::BuiltinSeedHash(cpid);
        BOOST_CHECK(a == b); // deterministic across calls

        BOOST_CHECK(seen.insert(a).second); // unique vs. all prior seeds
    }
}

BOOST_AUTO_TEST_CASE(grandfathered_builtins_have_invalid_operator_key)
{
    // The Path 2 sticky-claim semantic relies on builtins having an
    // invalid operator key by default — that's what sends them down
    // Path 2 instead of Path 1 until a real claim takes hold (plan
    // §3.5, finding K).
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    for (const auto& seed : GRC::PoolRegistry::BuiltinPoolSeeds()) {
        const GRC::Cpid cpid = GRC::Cpid::Parse(seed.cpid_hex);
        GRC::Pool_ptr entry = registry.Try(cpid);
        BOOST_REQUIRE(entry);
        BOOST_CHECK(!entry->m_operator_key.IsValid());
        BOOST_CHECK(entry->m_status == GRC::PoolStatus::ACTIVE);
        BOOST_CHECK_EQUAL(entry->m_height, 0);
        // Auth fields default to "no authorization."
        BOOST_CHECK(!entry->m_authorized_operator_key.IsValid());
        BOOST_CHECK_EQUAL(entry->m_authorization_height, -1);
    }
}

// -----------------------------------------------------------------------------
// IsPendingExpired / IsAuthorizationExpired pure-function tests
//
// These verify the height-comparison logic without setting up registry state.
// The full BlockValidate / lifecycle tests that exercise expired-PENDING
// transparency, OPEN-authorization expiry, sticky-claim rejection, etc., need
// ContractContext + CBlockIndex mocking and ride along with the deferred
// test-infrastructure PR (same pattern as the rework's existing deferrals).
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(is_pending_expired_only_fires_for_pending_status)
{
    GRC::Pool entry;
    entry.m_status = GRC::PoolStatus::ACTIVE;
    entry.m_height = 100;
    // Well past the default retention window — would expire if status mattered.
    BOOST_CHECK(!GRC::PoolRegistry::IsPendingExpired(entry, 100 + 1'000'000));

    entry.m_status = GRC::PoolStatus::DELETED;
    BOOST_CHECK(!GRC::PoolRegistry::IsPendingExpired(entry, 100 + 1'000'000));

    entry.m_status = GRC::PoolStatus::PENDING;
    BOOST_CHECK(GRC::PoolRegistry::IsPendingExpired(entry, 100 + 1'000'000));
}

BOOST_AUTO_TEST_CASE(is_pending_expired_height_boundary)
{
    const int retention = GRC::GetPendingPoolRetention();

    GRC::Pool entry;
    entry.m_status = GRC::PoolStatus::PENDING;
    entry.m_height = 100;

    // At and below the boundary: not expired.
    BOOST_CHECK(!GRC::PoolRegistry::IsPendingExpired(entry, 100 + retention));
    BOOST_CHECK(!GRC::PoolRegistry::IsPendingExpired(entry, 100 + retention - 1));

    // Above the boundary: expired.
    BOOST_CHECK(GRC::PoolRegistry::IsPendingExpired(entry, 100 + retention + 1));
}

BOOST_AUTO_TEST_CASE(is_pending_expired_handles_seed_height_sentinel)
{
    // Builtin seeds use m_height == 0 with status ACTIVE. PENDING with a
    // negative height (defensive — never happens in practice) is treated
    // as not-expired so we don't fire on uninitialized memory.
    GRC::Pool entry;
    entry.m_status = GRC::PoolStatus::PENDING;
    entry.m_height = -1;
    BOOST_CHECK(!GRC::PoolRegistry::IsPendingExpired(entry, 1'000'000));
}

BOOST_AUTO_TEST_CASE(is_authorization_expired_only_fires_with_valid_auth_key)
{
    GRC::Pool entry;
    entry.m_authorization_height = 100;
    // No auth key — never expired (nothing to expire).
    BOOST_CHECK(!entry.m_authorized_operator_key.IsValid());
    BOOST_CHECK(!GRC::PoolRegistry::IsAuthorizationExpired(entry, 100 + 1'000'000));
}

BOOST_AUTO_TEST_CASE(is_authorization_expired_height_boundary)
{
    const int retention = GRC::GetPendingPoolRetention();
    CKey op_priv = PoolTestKey::Private();

    GRC::Pool entry;
    entry.m_authorized_operator_key = op_priv.GetPubKey();
    entry.m_authorization_height = 100;

    BOOST_CHECK(!GRC::PoolRegistry::IsAuthorizationExpired(entry, 100 + retention));
    BOOST_CHECK(!GRC::PoolRegistry::IsAuthorizationExpired(entry, 100 + retention - 1));
    BOOST_CHECK(GRC::PoolRegistry::IsAuthorizationExpired(entry, 100 + retention + 1));
}

BOOST_AUTO_TEST_CASE(is_authorization_expired_handles_sentinel_height)
{
    // Sentinel m_authorization_height == -1 means "no authorization ever
    // set"; the auth-key-validity check should already short-circuit, but
    // belt-and-suspenders.
    CKey op_priv = PoolTestKey::Private();

    GRC::Pool entry;
    entry.m_authorized_operator_key = op_priv.GetPubKey();
    entry.m_authorization_height = -1;
    BOOST_CHECK(!GRC::PoolRegistry::IsAuthorizationExpired(entry, 1'000'000));
}

BOOST_AUTO_TEST_CASE(pending_pool_retention_default_is_28800)
{
    // Sanity: the default retention should match what the plan and
    // chainparams.cpp declare. If a future commit retunes this, the test
    // becomes a documentation tripwire.
    //
    // GetPendingPoolRetention() reads the -pendingpoolretention override
    // when set; in the unit-test harness no override is in play.
    BOOST_CHECK_EQUAL(GRC::GetPendingPoolRetention(), 28800);
}

BOOST_AUTO_TEST_SUITE_END()
