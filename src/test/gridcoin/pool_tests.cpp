// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/pool.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/registry.h"
#include "gridcoin/researcher.h"
#include "chainparams.h"
#include "key.h"
#include "main.h"
#include "primitives/transaction.h"
#include "streams.h"
#include "sync.h"
#include "util/system.h"

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

//!
//! \brief Wrap a pool contract in a CTransaction with a unique hash.
//!
//! `nonce` differentiates the txid (via the input prevout index) so a chain of
//! contracts on the same CPID gets distinct m_hash / m_previous_hash links,
//! exactly as real on-chain contracts would.
//!
CTransaction MakePoolTx(GRC::Contract contract, uint32_t nonce)
{
    CMutableTransaction mtx;
    mtx.nTime = 1500000000 + nonce;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256(), nonce);
    mtx.vContracts.push_back(std::move(contract));
    return CTransaction(mtx);
}

//!
//! \brief Drive a single pool-contract transaction through the real contract
//! dispatcher (GRC::ApplyContracts -> Dispatcher::Apply), the same path
//! ConnectBlock uses. This is what exercises the OPEN dispatch wiring — calling
//! PoolRegistry::Open directly would bypass the Dispatcher and miss the bug the
//! blocker fix targets.
//!
void DispatchApply(const CTransaction& tx, const CBlockIndex* pindex)
{
    GRC::RegistryBookmarks bookmarks;
    bool found_contract = false;
    GRC::ApplyContracts(tx, pindex, bookmarks, found_contract);
}

//!
//! \brief Fixture that leaves the global PoolRegistry in its clean booted
//! (builtins-only) state before and after each lifecycle test, so a failure
//! mid-test can't pollute the shared singleton for later cases.
//!
struct PoolLifecycleFixture
{
    PoolLifecycleFixture()  { Restore(); }
    ~PoolLifecycleFixture() { Restore(); }

    // Leave the registry booted-clean and V15 inert (some tests force
    // -blockv15height=0 to exercise the validation gate; reset it so the
    // override never leaks into another case via the global gArgs).
    static void Restore()
    {
        // "2147483647" == INT_MAX, the same V15-inert sentinel the chainparams
        // default uses. A string literal avoids the locale-dependent
        // integer-to-string conversion flagged by lint-locale-dependence.sh.
        gArgs.ForceSetArg("-blockv15height", "2147483647");
        LOCK(cs_main);
        GRC::GetPoolRegistry().Reset();
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
    BOOST_REQUIRE(original.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

    BOOST_CHECK(payload.VerifySignature(public_key, GRC::ContractAction::ADD, uint256{}));
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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

    payload.m_url = "https://evil-pool-takeover.example/";
    BOOST_CHECK(!payload.VerifySignature(public_key, GRC::ContractAction::ADD, uint256{}));
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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

    BOOST_CHECK(!payload.VerifySignature(other_pubkey, GRC::ContractAction::ADD, uint256{}));
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
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

    // Pass a default-constructed (invalid) public key — VerifySignature
    // must reject without crashing.
    BOOST_CHECK(!payload.VerifySignature(CPubKey{}, GRC::ContractAction::ADD, uint256{}));
}

BOOST_AUTO_TEST_CASE(register_payload_signature_binds_action_and_previous_hash)
{
    // Replay defense: the operator signature must commit to the contract action
    // AND the predecessor state, so a captured payload+signature can't be
    // replayed with the action flipped (REMOVE to de-list an active pool) or
    // against a different predecessor (a stale ADD knocking a pool to PENDING).
    CKey private_key = PoolTestKey::Private();
    const CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);

    const uint256 prev = GRC::PoolRegistry::BuiltinSeedHash(PoolTestKey::Cpid());

    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, prev));

    // Verifies only against the exact (action, predecessor) it was signed for.
    BOOST_CHECK(payload.VerifySignature(public_key, GRC::ContractAction::ADD, prev));

    // Action flipped to REMOVE → reject.
    BOOST_CHECK(!payload.VerifySignature(public_key, GRC::ContractAction::REMOVE, prev));

    // Different predecessor state → reject.
    BOOST_CHECK(!payload.VerifySignature(public_key, GRC::ContractAction::ADD, uint256{}));
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
    // Q3 shadow check: pre-V15, ActivePoolsAtHeight must yield the same SET of
    // (CPID, name, URL) as the legacy g_mining_pools list. Order is incidental —
    // membership is what AVW consumes (the consumer drops the result into an
    // unordered_set), so this compares as a set, not a sequence. If this
    // assertion fires, grandfathering has diverged from the hardcoded list and
    // AVW will fork the chain on the next pre-V15 poll.
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
    const int retention = GetPendingPoolRetention();

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
    const int retention = GetPendingPoolRetention();
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
    BOOST_CHECK_EQUAL(GetPendingPoolRetention(), 28800);
}

// -----------------------------------------------------------------------------
// Lifecycle: apply / revert through the real contract dispatcher
//
// These drive contracts through GRC::ApplyContracts / GRC::RevertContracts (the
// ConnectBlock / DisconnectBlock path) rather than poking the registry directly,
// so they exercise Dispatcher::Apply / Dispatcher::Revert — including the OPEN
// dispatch wiring whose absence was the blocker on the 2026-06-14 review.
// -----------------------------------------------------------------------------

BOOST_FIXTURE_TEST_CASE(open_apply_revert_round_trips_a_builtin, PoolLifecycleFixture)
{
    LOCK(cs_main);

    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();
    const GRC::Cpid cpid =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);

    // Booted state: ACTIVE builtin seed, no operator key, no authorization.
    {
        GRC::Pool_ptr seed = registry.Try(cpid);
        BOOST_REQUIRE(seed);
        BOOST_REQUIRE(registry.IsBuiltin(cpid));
        BOOST_CHECK(seed->m_status == GRC::PoolStatus::ACTIVE);
        BOOST_CHECK(!seed->m_authorized_operator_key.IsValid());
    }

    const CKey operator_key = PoolTestKey::Private();
    const CPubKey authorized = operator_key.GetPubKey();

    CBlockIndex pindex;
    pindex.nHeight = 5'000'000;

    GRC::Contract open = GRC::MakeContract<GRC::PoolApprovePayload>(
        GRC::ContractAction::OPEN, cpid, authorized);
    const CTransaction open_tx = MakePoolTx(std::move(open), 1);

    // Apply OPEN through the dispatcher. BEFORE the blocker fix, Dispatcher::Apply
    // dropped OPEN entirely, so the authorization below was never recorded.
    DispatchApply(open_tx, &pindex);

    {
        GRC::Pool_ptr opened = registry.Try(cpid);
        BOOST_REQUIRE(opened);
        BOOST_CHECK(opened->m_authorized_operator_key == authorized);
        BOOST_CHECK_EQUAL(opened->m_authorization_height, pindex.nHeight);
        // OPEN preserves status and builtin-ness.
        BOOST_CHECK(opened->m_status == GRC::PoolStatus::ACTIVE);
        BOOST_CHECK(registry.IsBuiltin(cpid));
    }

    // Revert the OPEN block (reorg). Apply/Revert must be symmetric: the seed
    // must be restored, NOT erased. BEFORE the fix, Revert popped an entry Apply
    // never pushed and the null-previous-hash builtin seed was lost.
    GRC::RevertContracts(open_tx, &pindex);

    {
        GRC::Pool_ptr restored = registry.Try(cpid);
        BOOST_REQUIRE(restored); // seed survived the reorg
        BOOST_CHECK(registry.IsBuiltin(cpid));
        BOOST_CHECK(restored->m_status == GRC::PoolStatus::ACTIVE);
        BOOST_CHECK(!restored->m_authorized_operator_key.IsValid());
    }
}

BOOST_FIXTURE_TEST_CASE(approve_remove_on_builtin_reverts_to_active_seed, PoolLifecycleFixture)
{
    LOCK(cs_main);

    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();
    const GRC::Cpid cpid =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);

    CBlockIndex pindex;
    pindex.nHeight = 5'000'000;

    GRC::Contract remove = GRC::MakeContract<GRC::PoolApprovePayload>(
        GRC::ContractAction::REMOVE, cpid);
    const CTransaction remove_tx = MakePoolTx(std::move(remove), 7);

    DispatchApply(remove_tx, &pindex);

    {
        GRC::Pool_ptr deleted = registry.Try(cpid);
        BOOST_REQUIRE(deleted);
        BOOST_CHECK(deleted->m_status == GRC::PoolStatus::DELETED);
        BOOST_CHECK(registry.IsBuiltin(cpid)); // builtin-ness is permanent
    }

    GRC::RevertContracts(remove_tx, &pindex);

    {
        GRC::Pool_ptr restored = registry.Try(cpid);
        BOOST_REQUIRE(restored);
        BOOST_CHECK(restored->m_status == GRC::PoolStatus::ACTIVE);
        BOOST_CHECK(registry.IsBuiltin(cpid));
    }
}

BOOST_FIXTURE_TEST_CASE(register_on_nonbuiltin_reverts_cleanly_without_touching_builtins,
                        PoolLifecycleFixture)
{
    LOCK(cs_main);

    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    const GRC::Cpid nonbuiltin = PoolTestKey::Cpid();
    BOOST_REQUIRE(!registry.IsBuiltin(nonbuiltin));
    BOOST_REQUIRE(!registry.Try(nonbuiltin)); // not present at boot

    const GRC::Cpid builtin =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);
    const size_t builtin_count = GRC::PoolRegistry::BuiltinPoolSeeds().size();

    CBlockIndex pindex;
    pindex.nHeight = 5'000'000;

    CKey operator_key = PoolTestKey::Private();
    GRC::PoolRegisterPayload payload(
        nonbuiltin, "testpool", "https://test.example/", operator_key.GetPubKey());
    BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, uint256{}));

    GRC::Contract reg = GRC::MakeContract<GRC::PoolRegisterPayload>(
        GRC::ContractAction::ADD, std::move(payload));
    const CTransaction reg_tx = MakePoolTx(std::move(reg), 3);

    DispatchApply(reg_tx, &pindex);

    {
        GRC::Pool_ptr pending = registry.Try(nonbuiltin);
        BOOST_REQUIRE(pending);
        BOOST_CHECK(pending->m_status == GRC::PoolStatus::PENDING);
        // Builtins untouched.
        BOOST_CHECK_EQUAL(registry.ActivePools().size(), builtin_count);
        BOOST_CHECK(registry.IsActivePool(builtin));
    }

    GRC::RevertContracts(reg_tx, &pindex);

    {
        // First contract for this CPID had a null previous hash → revert erases
        // the CPID outright, leaving only the grandfathered builtins.
        BOOST_CHECK(!registry.Try(nonbuiltin));
        BOOST_CHECK(registry.IsActivePool(builtin));
        BOOST_CHECK_EQUAL(registry.ActivePools().size(), builtin_count);
    }
}

BOOST_FIXTURE_TEST_CASE(takeover_register_rejected_operator_register_accepted, PoolLifecycleFixture)
{
    // Drives the takeover defense through the authoritative consensus path
    // (GRC::BlockValidateContracts -> Dispatcher::BlockValidate ->
    // ValidateAtHeight -> VerifyRegisterAuth) with V15 forced active. An
    // interloper's POOL_REGISTER on a CPID already held by a valid operator key
    // must be rejected; a payload signed by the existing operator key must be
    // accepted. Also exercises the action+predecessor signature binding inside
    // the verifier. BlockValidateContracts is used (not ValidateContracts) so
    // the activation height comes from the block index we control rather than
    // the global nBestHeight, which is unset under the test harness.
    gArgs.ForceSetArg("-blockv15height", "0"); // V15 active at every height >= 0

    LOCK(cs_main);
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    const GRC::Cpid cpid = PoolTestKey::Cpid(); // non-builtin
    BOOST_REQUIRE(!registry.IsBuiltin(cpid));

    CBlockIndex pindex;
    pindex.nHeight = 100; // >= BlockV15Height(0), so the V15 gate passes

    // Establish a live, operator-claimed entry (ACTIVE, valid key).
    CKey operator_key = PoolTestKey::Private();
    GRC::Pool entry(cpid, "legitpool", "https://legit.example/", operator_key.GetPubKey());
    entry.m_status = GRC::PoolStatus::ACTIVE;
    entry.m_height = 50;
    entry.m_hash = GRC::PoolRegistry::BuiltinSeedHash(cpid); // any deterministic non-null hash
    registry.SeedForTests(entry);

    const uint256 prev_hash = entry.m_hash; // predecessor the new contract chains onto

    // Interloper: signs with their OWN key (the takeover attempt), even using
    // the correct action + predecessor so only the key differs.
    {
        CKey attacker = PoolTestKey::Private();
        GRC::PoolRegisterPayload payload(cpid, "evilpool", "https://evil.example/",
                                         attacker.GetPubKey());
        BOOST_REQUIRE(payload.Sign(attacker, GRC::ContractAction::ADD, prev_hash));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 21);

        int dos = 0;
        BOOST_CHECK(!GRC::BlockValidateContracts(&pindex, tx, dos)); // rejected: sig != existing key
    }

    // Legitimate operator: signs with the existing operator key -> accepted
    // (routine update / rotation).
    {
        GRC::PoolRegisterPayload payload(cpid, "legitpool", "https://legit.example/",
                                         operator_key.GetPubKey());
        BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, prev_hash));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 22);

        int dos = 0;
        BOOST_CHECK(GRC::BlockValidateContracts(&pindex, tx, dos)); // accepted: operator-signed
    }
}

BOOST_FIXTURE_TEST_CASE(reset_reseeds_builtins, PoolLifecycleFixture)
{
    LOCK(cs_main);

    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();
    const size_t builtin_count = GRC::PoolRegistry::BuiltinPoolSeeds().size();

    // Seed an extra non-builtin entry so we can confirm Reset clears real state.
    GRC::Pool extra;
    extra.m_cpid = PoolTestKey::Cpid();
    extra.m_name = "ephemeral";
    extra.m_status = GRC::PoolStatus::ACTIVE;
    extra.m_height = 4'000'000;
    registry.SeedForTests(extra);
    BOOST_REQUIRE(registry.Try(PoolTestKey::Cpid()));

    registry.Reset();

    // The non-builtin entry is gone; the grandfathered builtins are back. Before
    // the Major-2 fix, Reset left the registry EMPTY (no reseed), which would
    // regress IsActivePool* and diverge AVW for a -clearallregistryhistory node.
    BOOST_CHECK(!registry.Try(PoolTestKey::Cpid()));
    BOOST_CHECK_EQUAL(registry.ActivePools().size(), builtin_count);

    const GRC::Cpid builtin =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);
    BOOST_CHECK(registry.IsActivePool(builtin));
    BOOST_CHECK(registry.IsBuiltin(builtin));
}

BOOST_AUTO_TEST_CASE(register_payload_well_formed_rejects_open)
{
    // BLOCKER (2026-06-22): POOL_REGISTER has no OPEN semantics. PoolRegistry::Open
    // no-ops for POOL_REGISTER while PoolRegistry::Revert processes it
    // unconditionally, so a valid REGISTER+OPEN would apply as a no-op but revert
    // as a mutation and diverge nodes on a reorg. WellFormed must reject OPEN
    // (and UNKNOWN), mirroring PoolApprovePayload::WellFormed's default branch,
    // while still accepting ADD and REMOVE.
    CKey private_key = PoolTestKey::Private();

    GRC::PoolRegisterPayload payload(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        private_key.GetPubKey());

    // Signature size only needs to land in the accepted band; the digest's
    // action doesn't affect WellFormed.
    BOOST_REQUIRE(payload.Sign(private_key, GRC::ContractAction::ADD, uint256{}));

    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::ADD));
    BOOST_CHECK(payload.WellFormed(GRC::ContractAction::REMOVE));
    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::OPEN));
    BOOST_CHECK(!payload.WellFormed(GRC::ContractAction::UNKNOWN));
}

BOOST_FIXTURE_TEST_CASE(captured_register_add_replayed_as_remove_rejected, PoolLifecycleFixture)
{
    // 2026-06-22 review (test-gap): assert the action binding END-TO-END through
    // the consensus path (BlockValidateContracts -> Dispatcher::BlockValidate ->
    // ValidateAtHeight -> VerifyRegisterAuth), not just the VerifySignature
    // primitive. A capture of a valid operator-signed POOL_REGISTER ADD, replayed
    // with the action byte flipped to REMOVE without re-signing, must be rejected
    // because the signature is bound to the action; and a correctly REMOVE-signed
    // withdrawal by the same operator must be accepted (the path the new
    // withdrawpool RPC drives).
    gArgs.ForceSetArg("-blockv15height", "0"); // V15 active at every height >= 0

    LOCK(cs_main);
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    const GRC::Cpid cpid = PoolTestKey::Cpid(); // non-builtin
    BOOST_REQUIRE(!registry.IsBuiltin(cpid));

    CBlockIndex pindex;
    pindex.nHeight = 100; // >= BlockV15Height(0), so the V15 gate passes

    // Establish a live, operator-claimed entry (ACTIVE, valid key).
    CKey operator_key = PoolTestKey::Private();
    GRC::Pool entry(cpid, "legitpool", "https://legit.example/", operator_key.GetPubKey());
    entry.m_status = GRC::PoolStatus::ACTIVE;
    entry.m_height = 50;
    entry.m_hash = GRC::PoolRegistry::BuiltinSeedHash(cpid); // deterministic non-null hash
    registry.SeedForTests(entry);

    const uint256 prev_hash = entry.m_hash; // predecessor the contract chains onto

    // Capture a valid ADD signed by the operator, then ship it as REMOVE without
    // re-signing. The signature covers the ADD digest, so the REMOVE digest
    // VerifyRegisterAuth recomputes won't match -> rejected at consensus. (A
    // pure VerifySignature assertion would catch the primitive; this proves the
    // dispatch/validate layer actually enforces it.)
    {
        GRC::PoolRegisterPayload captured(cpid, "legitpool", "https://legit.example/",
                                          operator_key.GetPubKey());
        BOOST_REQUIRE(captured.Sign(operator_key, GRC::ContractAction::ADD, prev_hash));

        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::REMOVE, std::move(captured)); // action flipped, sig unchanged
        const CTransaction tx = MakePoolTx(std::move(c), 31);

        int dos = 0;
        BOOST_CHECK(!GRC::BlockValidateContracts(&pindex, tx, dos)); // action binding holds
    }

    // The legitimate operator self-withdrawal: signs REMOVE over the same
    // predecessor with the registered operator key -> accepted.
    {
        GRC::PoolRegisterPayload payload(cpid, "legitpool", "https://legit.example/",
                                         operator_key.GetPubKey());
        BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::REMOVE, prev_hash));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::REMOVE, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 32);

        int dos = 0;
        BOOST_CHECK(GRC::BlockValidateContracts(&pindex, tx, dos)); // operator withdrawal accepted
    }
}

BOOST_FIXTURE_TEST_CASE(builtin_path2_claim_requires_matching_open_authorization, PoolLifecycleFixture)
{
    // R1 (adversarial pre-review): drive a GRANDFATHERED BUILTIN slot through the
    // consensus path (BlockValidateContracts -> ValidateAtHeight ->
    // VerifyRegisterAuth Path 2) — the sticky takeover defense that governs who
    // may claim the 5 builtin grcpool CPIDs (real magnitude / AVW). The existing
    // takeover test uses a NON-builtin CPID and so never exercises Path 2.
    gArgs.ForceSetArg("-blockv15height", "0"); // V15 active at every height >= 0

    LOCK(cs_main);
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    const GRC::Cpid cpid =
        GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);
    BOOST_REQUIRE(registry.IsBuiltin(cpid));

    CBlockIndex pindex;
    pindex.nHeight = 100; // >= BlockV15Height(0), so the V15 gate passes

    CKey operator_key = PoolTestKey::Private();
    const CPubKey authorized = operator_key.GetPubKey();

    // (a) No OPEN authorization yet: an unauthorized POOL_REGISTER ADD on a
    // builtin slot must be rejected (the core sticky guard).
    {
        const uint256 prev = registry.Try(cpid)->m_hash; // current seed hash
        GRC::PoolRegisterPayload payload(cpid, "claimer", "https://claimer.example/", authorized);
        BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, prev));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 41);

        int dos = 0;
        BOOST_CHECK(!GRC::BlockValidateContracts(&pindex, tx, dos)); // no OPEN -> rejected
    }

    // Foundation issues a POOL_APPROVE OPEN authorizing `authorized`.
    {
        GRC::Contract open = GRC::MakeContract<GRC::PoolApprovePayload>(
            GRC::ContractAction::OPEN, cpid, authorized);
        const CTransaction open_tx = MakePoolTx(std::move(open), 42);
        DispatchApply(open_tx, &pindex);
        GRC::Pool_ptr opened = registry.Try(cpid);
        BOOST_REQUIRE(opened);
        BOOST_REQUIRE(opened->m_authorized_operator_key == authorized);
    }

    const uint256 prev_after_open = registry.Try(cpid)->m_hash;

    // (b) OPEN exists but a DIFFERENT key tries to claim -> rejected.
    {
        CKey wrong = PoolTestKey::Private();
        GRC::PoolRegisterPayload payload(cpid, "imposter", "https://imposter.example/",
                                         wrong.GetPubKey());
        BOOST_REQUIRE(payload.Sign(wrong, GRC::ContractAction::ADD, prev_after_open));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 43);

        int dos = 0;
        BOOST_CHECK(!GRC::BlockValidateContracts(&pindex, tx, dos)); // key != OPEN auth -> rejected
    }

    // (c) The authorized key claims -> accepted.
    {
        GRC::PoolRegisterPayload payload(cpid, "claimer", "https://claimer.example/", authorized);
        BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, prev_after_open));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 44);

        int dos = 0;
        BOOST_CHECK(GRC::BlockValidateContracts(&pindex, tx, dos)); // authorized -> accepted
    }

    // (d) Expired OPEN: validate the same authorized claim at a height past the
    // retention window -> rejected (authorization no longer live). State was
    // never mutated above (BlockValidateContracts only validates), so the OPEN
    // authorization recorded at pindex.nHeight is still present.
    {
        CBlockIndex late;
        late.nHeight = registry.Try(cpid)->m_authorization_height + GetPendingPoolRetention() + 1;

        GRC::PoolRegisterPayload payload(cpid, "claimer", "https://claimer.example/", authorized);
        BOOST_REQUIRE(payload.Sign(operator_key, GRC::ContractAction::ADD, prev_after_open));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 45);

        int dos = 0;
        BOOST_CHECK(!GRC::BlockValidateContracts(&late, tx, dos)); // OPEN expired -> rejected
    }
}

BOOST_FIXTURE_TEST_CASE(expired_pending_nonbuiltin_superseded_by_fresh_key, PoolLifecycleFixture)
{
    // R5 (adversarial pre-review): doc/consensus.md §11.3.1 promises a fresh
    // POOL_REGISTER ADD from ANY key may supersede an EXPIRED PENDING on a
    // NON-builtin slot (one the Foundation never ratified), while a still-fresh
    // PENDING stays protected by takeover defense. Exercise that documented
    // consensus consequence through BlockValidateContracts, not just the
    // IsPendingExpired predicate in isolation.
    gArgs.ForceSetArg("-blockv15height", "0");

    LOCK(cs_main);
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    const GRC::Cpid cpid = PoolTestKey::Cpid(); // non-builtin
    BOOST_REQUIRE(!registry.IsBuiltin(cpid));

    // A PENDING entry held by an incumbent operator key, recorded at height H.
    const int H = 1000;
    CKey incumbent = PoolTestKey::Private();
    GRC::Pool entry(cpid, "pendingpool", "https://pending.example/", incumbent.GetPubKey());
    entry.m_status = GRC::PoolStatus::PENDING;
    entry.m_height = H;
    entry.m_hash = GRC::PoolRegistry::BuiltinSeedHash(cpid); // deterministic non-null hash
    registry.SeedForTests(entry);

    const uint256 prev = entry.m_hash;
    const int retention = GetPendingPoolRetention();
    CKey newcomer = PoolTestKey::Private(); // a DIFFERENT operator

    // Still-fresh PENDING (height H+1): the foreign key is rejected — takeover
    // defense protects the unexpired PENDING.
    {
        CBlockIndex fresh;
        fresh.nHeight = H + 1;
        GRC::PoolRegisterPayload payload(cpid, "newpool", "https://new.example/",
                                         newcomer.GetPubKey());
        BOOST_REQUIRE(payload.Sign(newcomer, GRC::ContractAction::ADD, prev));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 51);

        int dos = 0;
        BOOST_CHECK(!GRC::BlockValidateContracts(&fresh, tx, dos)); // fresh PENDING protected
    }

    // Past the retention window (height H+retention+1): the expired PENDING is
    // treated as absent, so the foreign key is accepted on the first-claim path.
    {
        CBlockIndex expired;
        expired.nHeight = H + retention + 1;
        GRC::PoolRegisterPayload payload(cpid, "newpool", "https://new.example/",
                                         newcomer.GetPubKey());
        BOOST_REQUIRE(payload.Sign(newcomer, GRC::ContractAction::ADD, prev));
        GRC::Contract c = GRC::MakeContract<GRC::PoolRegisterPayload>(
            GRC::ContractAction::ADD, std::move(payload));
        const CTransaction tx = MakePoolTx(std::move(c), 52);

        int dos = 0;
        BOOST_CHECK(GRC::BlockValidateContracts(&expired, tx, dos)); // expired PENDING superseded
    }
}

BOOST_AUTO_TEST_SUITE_END()
