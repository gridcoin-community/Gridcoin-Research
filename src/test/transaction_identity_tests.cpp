// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Serialization round-trip tests for CTransaction. These verify that
// serialize -> deserialize preserves GetHash() for all transaction
// variants, serving as a safety net for the CMutableTransaction split.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <main.h>
#include <streams.h>
#include <gridcoin/contract/contract.h>

BOOST_AUTO_TEST_SUITE(transaction_identity_tests)

static CKey MakeKey()
{
    CKey key;
    key.MakeNewKey(true);
    return key;
}

static CScript P2PKH(const CKeyID& id)
{
    CScript s;
    s << OP_DUP << OP_HASH160 << id << OP_EQUALVERIFY << OP_CHECKSIG;
    return s;
}

BOOST_AUTO_TEST_CASE(standard_p2pkh_roundtrip)
{
    CKey key = MakeKey();

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000000;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"), 0);
    mtx.vin[0].nSequence = 0xffffffff;
    mtx.vout.emplace_back(50 * COIN, P2PKH(key.GetPubKey().GetID()));

    CTransaction tx(mtx);
    uint256 hash_before = tx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2;
    ss >> tx2;

    BOOST_CHECK(tx2.GetHash() == hash_before);
    BOOST_CHECK(tx == tx2);
    BOOST_CHECK_EQUAL(tx2.nVersion, 2);
    BOOST_CHECK_EQUAL(tx2.nTime, 1700000000u);
    BOOST_CHECK_EQUAL(tx2.vin.size(), 1u);
    BOOST_CHECK_EQUAL(tx2.vout.size(), 1u);
    BOOST_CHECK_EQUAL(tx2.vout[0].nValue, 50 * COIN);
}

BOOST_AUTO_TEST_CASE(p2sh_roundtrip)
{
    CKey key = MakeKey();

    // Build a P2SH output wrapping a P2PKH redeem script
    CScript redeemScript = P2PKH(key.GetPubKey().GetID());
    CScriptID scriptID(redeemScript);
    CScript p2sh;
    p2sh << OP_HASH160 << scriptID << OP_EQUAL;

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000001;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("1111111111111111111111111111111111111111111111111111111111111111"), 3);
    mtx.vout.emplace_back(10 * COIN, p2sh);

    CTransaction tx(mtx);
    uint256 hash_before = tx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2;
    ss >> tx2;

    BOOST_CHECK(tx2.GetHash() == hash_before);
    BOOST_CHECK(tx == tx2);
}

BOOST_AUTO_TEST_CASE(multisig_roundtrip)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();

    CScript multisig;
    multisig << OP_1 << key1.GetPubKey() << key2.GetPubKey() << OP_2 << OP_CHECKMULTISIG;

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000002;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("2222222222222222222222222222222222222222222222222222222222222222"), 1);
    mtx.vout.emplace_back(25 * COIN, multisig);

    CTransaction tx(mtx);
    uint256 hash_before = tx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2;
    ss >> tx2;

    BOOST_CHECK(tx2.GetHash() == hash_before);
    BOOST_CHECK(tx == tx2);
}

BOOST_AUTO_TEST_CASE(v1_hashboinc_roundtrip)
{
    CKey key = MakeKey();

    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.nTime = 1700000003;
    mtx.hashBoinc = "some_legacy_boinc_data_for_testing";
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("3333333333333333333333333333333333333333333333333333333333333333"), 0);
    mtx.vout.emplace_back(5 * COIN, P2PKH(key.GetPubKey().GetID()));

    CTransaction tx(mtx);
    uint256 hash_before = tx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2;
    ss >> tx2;

    BOOST_CHECK(tx2.GetHash() == hash_before);
    BOOST_CHECK(tx == tx2);
    BOOST_CHECK_EQUAL(tx2.nVersion, 1);
    BOOST_CHECK_EQUAL(tx2.hashBoinc, "some_legacy_boinc_data_for_testing");
}

BOOST_AUTO_TEST_CASE(v2_empty_contracts_roundtrip)
{
    CKey key = MakeKey();

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000004;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("4444444444444444444444444444444444444444444444444444444444444444"), 0);
    mtx.vout.emplace_back(100 * COIN, P2PKH(key.GetPubKey().GetID()));

    CTransaction tx(mtx);

    // v2 with empty vContracts — verify the contract vector serializes correctly
    BOOST_CHECK(tx.vContracts.empty());

    uint256 hash_before = tx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2;
    ss >> tx2;

    BOOST_CHECK(tx2.GetHash() == hash_before);
    BOOST_CHECK(tx == tx2);
    BOOST_CHECK(tx2.vContracts.empty());
}

BOOST_AUTO_TEST_CASE(ntime_affects_hash)
{
    CKey key = MakeKey();

    CMutableTransaction mtx1;
    mtx1.nVersion = 2;
    mtx1.nTime = 1700000010;
    mtx1.vin.resize(1);
    mtx1.vin[0].prevout = COutPoint(uint256S("5555555555555555555555555555555555555555555555555555555555555555"), 0);
    mtx1.vout.emplace_back(1 * COIN, P2PKH(key.GetPubKey().GetID()));

    CMutableTransaction mtx2;
    mtx2.nVersion = 2;
    mtx2.nTime = 1700000011; // Different nTime
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout = COutPoint(uint256S("5555555555555555555555555555555555555555555555555555555555555555"), 0);
    mtx2.vout.emplace_back(1 * COIN, P2PKH(key.GetPubKey().GetID()));

    CTransaction tx1(mtx1);
    CTransaction tx2(mtx2);

    BOOST_CHECK(tx1.GetHash() != tx2.GetHash());
}

BOOST_AUTO_TEST_CASE(coinbase_roundtrip)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000020;
    mtx.vin.resize(1);
    // Coinbase: prevout is null
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 0;
    mtx.vout.emplace_back(0, CScript());

    CTransaction tx(mtx);

    BOOST_CHECK(tx.IsCoinBase());

    uint256 hash_before = tx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2;
    ss >> tx2;

    BOOST_CHECK(tx2.GetHash() == hash_before);
    BOOST_CHECK(tx == tx2);
    BOOST_CHECK(tx2.IsCoinBase());
}

BOOST_AUTO_TEST_CASE(coinstake_roundtrip)
{
    CKey key = MakeKey();

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000030;

    // Non-null input (required for coinstake, not coinbase)
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("6666666666666666666666666666666666666666666666666666666666666666"), 0);

    // Coinstake: vout[0] is empty marker
    mtx.vout.emplace_back(0, CScript());
    mtx.vout.emplace_back(100 * COIN, P2PKH(key.GetPubKey().GetID()));

    CTransaction tx(mtx);

    BOOST_CHECK(tx.IsCoinStake());

    uint256 hash_before = tx.GetHash();

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    CTransaction tx2;
    ss >> tx2;

    BOOST_CHECK(tx2.GetHash() == hash_before);
    BOOST_CHECK(tx == tx2);
    BOOST_CHECK(tx2.IsCoinStake());
}

BOOST_AUTO_TEST_CASE(v1_legacy_contract_getcontracts_is_idempotent)
{
    // Build a v1 transaction with a legacy beacon contract embedded in
    // hashBoinc. The hashBoinc string must satisfy GRC::Contract::Detect()
    // (must contain "<MT>" and not be a superblock).
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.nTime = 1700000000;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S(
        "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"), 0);
    mtx.vin[0].nSequence = 0xffffffff;
    mtx.vout.emplace_back(0, CScript());
    mtx.hashBoinc =
        "<MT>beacon</MT>"
        "<MK>test</MK>"
        "<MV>test</MV>"
        "<MA>A</MA>"
        "<MPK></MPK>";

    const CTransaction tx(std::move(mtx));

    BOOST_REQUIRE_EQUAL(tx.nVersion, 1);
    BOOST_REQUIRE(tx.vContracts.empty());
    BOOST_REQUIRE(GRC::Contract::Detect(tx.hashBoinc));

    // First call parses-and-caches.
    const std::vector<GRC::Contract>& contracts1 = tx.GetContracts();
    BOOST_CHECK_EQUAL(contracts1.size(), 1u);
    BOOST_CHECK(contracts1[0].m_type == GRC::ContractType::BEACON);

    // Subsequent calls must return the same cached vector unchanged. A
    // regression of the m_legacy_parsed_contracts.empty() guard would
    // grow the vector and trip ContextualCheckTransaction's DoS-100
    // path (tx.GetContracts().size() > 1).
    BOOST_CHECK_EQUAL(tx.GetContracts().size(), 1u);
    BOOST_CHECK_EQUAL(tx.GetContracts().size(), 1u);

    // Same backing storage on every call (cache returned, not re-parsed).
    BOOST_CHECK_EQUAL(&contracts1, &tx.GetContracts());
}

BOOST_AUTO_TEST_SUITE_END()
