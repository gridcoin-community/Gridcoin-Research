// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Serialization round-trip tests for CTransaction. These verify that
// serialize -> deserialize preserves GetHash() for all transaction
// variants, serving as a safety net for the upcoming CMutableTransaction
// split (PR E in the PSGT sequence).

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <main.h>
#include <streams.h>

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

    CTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1700000000;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"), 0);
    tx.vin[0].nSequence = 0xffffffff;
    tx.vout.emplace_back(50 * COIN, P2PKH(key.GetPubKey().GetID()));

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

    CTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1700000001;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("1111111111111111111111111111111111111111111111111111111111111111"), 3);
    tx.vout.emplace_back(10 * COIN, p2sh);

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

    CTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1700000002;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("2222222222222222222222222222222222222222222222222222222222222222"), 1);
    tx.vout.emplace_back(25 * COIN, multisig);

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

    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000003;
    tx.hashBoinc = "some_legacy_boinc_data_for_testing";
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("3333333333333333333333333333333333333333333333333333333333333333"), 0);
    tx.vout.emplace_back(5 * COIN, P2PKH(key.GetPubKey().GetID()));

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

    CTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1700000004;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("4444444444444444444444444444444444444444444444444444444444444444"), 0);
    tx.vout.emplace_back(100 * COIN, P2PKH(key.GetPubKey().GetID()));

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

    CTransaction tx1;
    tx1.nVersion = 2;
    tx1.nTime = 1700000010;
    tx1.vin.resize(1);
    tx1.vin[0].prevout = COutPoint(uint256S("5555555555555555555555555555555555555555555555555555555555555555"), 0);
    tx1.vout.emplace_back(1 * COIN, P2PKH(key.GetPubKey().GetID()));

    CTransaction tx2;
    tx2.nVersion = 2;
    tx2.nTime = 1700000011; // Different nTime
    tx2.vin.resize(1);
    tx2.vin[0].prevout = COutPoint(uint256S("5555555555555555555555555555555555555555555555555555555555555555"), 0);
    tx2.vout.emplace_back(1 * COIN, P2PKH(key.GetPubKey().GetID()));

    BOOST_CHECK(tx1.GetHash() != tx2.GetHash());
}

BOOST_AUTO_TEST_CASE(coinbase_roundtrip)
{
    CTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1700000020;
    tx.vin.resize(1);
    // Coinbase: prevout is null
    tx.vin[0].prevout.SetNull();
    tx.vin[0].scriptSig = CScript() << 0;
    tx.vout.emplace_back(0, CScript());

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

    CTransaction tx;
    tx.nVersion = 2;
    tx.nTime = 1700000030;

    // Non-null input (required for coinstake, not coinbase)
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("6666666666666666666666666666666666666666666666666666666666666666"), 0);

    // Coinstake: vout[0] is empty marker
    tx.vout.emplace_back(0, CScript());
    tx.vout.emplace_back(100 * COIN, P2PKH(key.GetPubKey().GetID()));

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

BOOST_AUTO_TEST_SUITE_END()
