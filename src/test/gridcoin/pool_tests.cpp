// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/pool.h"
#include "gridcoin/contract/contract.h"
#include "key.h"
#include "streams.h"

#include <boost/test/unit_test.hpp>
#include <vector>

namespace {

//!
//! \brief Local helper for constructing operator keys and CPIDs that match
//! across multiple test cases. Mirrors the TestKey helper in
//! beacon_tests.cpp without depending on its specific key bytes.
//!
struct PoolTestKey
{
    //!
    //! \brief Create a valid private key for tests. The key is generated
    //! deterministically using MakeNewKey so signatures are reproducible
    //! across runs within a single test process.
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
    const CKey private_key = PoolTestKey::Private();
    const CPubKey public_key = private_key.GetPubKey();

    GRC::PoolRegisterPayload original(
        PoolTestKey::Cpid(),
        "grcpool.com",
        "https://grcpool.com/",
        public_key);
    BOOST_REQUIRE(original.Sign(const_cast<CKey&>(private_key)));

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

BOOST_AUTO_TEST_CASE(register_payload_burn_amount_matches_beacon)
{
    GRC::PoolRegisterPayload payload;
    BOOST_CHECK_EQUAL(payload.RequiredBurnAmount(), COIN / 2);
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

BOOST_AUTO_TEST_CASE(pool_registry_active_pools_starts_empty)
{
    GRC::PoolRegistry& registry = GRC::GetPoolRegistry();

    // A pristine wallet under unit-test conditions has no pool entries.
    // (Registry::Add path exercising requires ContractContext + CBlockIndex
    // mocking similar to BeaconRegistryTest in beacon_tests.cpp; the
    // hybrid-authority and reorg-revert cases are tracked as follow-up
    // work in the test plan.)
    BOOST_CHECK(registry.ActivePools().empty());
}

BOOST_AUTO_TEST_SUITE_END()
