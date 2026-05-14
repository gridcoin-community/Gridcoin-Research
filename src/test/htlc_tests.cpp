// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <crypto/sha256.h>
#include "htlc.h"
#include "key.h"
#include "main.h"
#include "script.h"

using namespace std;

extern uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);

BOOST_AUTO_TEST_SUITE(htlc_tests)

BOOST_AUTO_TEST_CASE(htlc_script_roundtrip)
{
    // Test that CreateHTLCScript and ParseHTLCScript are inverses
    CKey receiver_key, sender_key;
    receiver_key.MakeNewKey(true);
    sender_key.MakeNewKey(true);

    CPubKey receiver_pubkey = receiver_key.GetPubKey();
    CPubKey sender_pubkey = sender_key.GetPubKey();

    // Create a hash from some preimage
    unsigned char preimage[] = "test preimage data here!";
    vector<unsigned char> hash(32);
    CSHA256().Write(preimage, sizeof(preimage) - 1).Finalize(hash.data());

    int64_t timeout = 1700000000; // example Unix timestamp (Nov 2023)

    CScript script = CreateHTLCScript(hash, receiver_pubkey, sender_pubkey, timeout);

    // Parse it back
    vector<unsigned char> parsed_hash;
    CPubKey parsed_receiver, parsed_sender;
    int64_t parsed_timeout;

    BOOST_CHECK(ParseHTLCScript(script, parsed_hash, parsed_receiver, parsed_sender, parsed_timeout));
    BOOST_CHECK(parsed_hash == hash);
    BOOST_CHECK(parsed_receiver == receiver_pubkey);
    BOOST_CHECK(parsed_sender == sender_pubkey);
    BOOST_CHECK_EQUAL(parsed_timeout, timeout);
}

BOOST_AUTO_TEST_CASE(htlc_claim_path_succeeds)
{
    CKey receiver_key, sender_key;
    receiver_key.MakeNewKey(true);
    sender_key.MakeNewKey(true);

    // Create preimage and hash
    unsigned char preimage_data[] = "secret preimage";
    vector<unsigned char> preimage(preimage_data, preimage_data + sizeof(preimage_data) - 1);
    vector<unsigned char> hash(32);
    CSHA256().Write(preimage.data(), preimage.size()).Finalize(hash.data());

    int64_t timeout = 1700000000;

    CScript redeemScript = CreateHTLCScript(hash, receiver_key.GetPubKey(),
                                            sender_key.GetPubKey(), timeout);

    // Create P2SH scriptPubKey
    CScript scriptPubKey;
    scriptPubKey << OP_HASH160;
    CScriptID scriptID = redeemScript.GetID();
    scriptPubKey << vector<unsigned char>(scriptID.begin(), scriptID.end());
    scriptPubKey << OP_EQUAL;

    // Create spending transaction
    CMutableTransaction txSpend;
    txSpend.nVersion = 2;
    txSpend.nTime = GetAdjustedTime();
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vin[0].prevout.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].nSequence = 0xfffffffe;
    txSpend.vout.resize(1);
    txSpend.vout[0].nValue = 1 * COIN;

    // Sign with receiver's key
    uint256 sighash = SignatureHash(redeemScript, CTransaction(txSpend), 0, SIGHASH_ALL);
    vector<unsigned char> vchSig;
    receiver_key.Sign(sighash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    // Build claim scriptSig
    txSpend.vin[0].scriptSig = CreateHTLCClaimScript(vchSig, preimage, redeemScript);

    // Verify
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    BOOST_CHECK(VerifyScript(txSpend.vin[0].scriptSig, scriptPubKey, flags, CTransaction(txSpend), 0));
}

BOOST_AUTO_TEST_CASE(htlc_refund_path_succeeds)
{
    CKey receiver_key, sender_key;
    receiver_key.MakeNewKey(true);
    sender_key.MakeNewKey(true);

    unsigned char preimage_data[] = "secret preimage";
    vector<unsigned char> hash(32);
    CSHA256().Write(preimage_data, sizeof(preimage_data) - 1).Finalize(hash.data());

    int64_t timeout = 500; // block height based (< LOCKTIME_THRESHOLD)

    CScript redeemScript = CreateHTLCScript(hash, receiver_key.GetPubKey(),
                                            sender_key.GetPubKey(), timeout);

    // P2SH scriptPubKey
    CScript scriptPubKey;
    scriptPubKey << OP_HASH160;
    CScriptID scriptID = redeemScript.GetID();
    scriptPubKey << vector<unsigned char>(scriptID.begin(), scriptID.end());
    scriptPubKey << OP_EQUAL;

    // Create spending transaction with locktime at timeout
    CMutableTransaction txSpend;
    txSpend.nVersion = 2;
    txSpend.nTime = GetAdjustedTime();
    txSpend.nLockTime = timeout; // CLTV requires nLockTime >= script timeout
    txSpend.vin.resize(1);
    txSpend.vin[0].prevout.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].nSequence = 0xfffffffe; // non-final to enable nLockTime
    txSpend.vout.resize(1);
    txSpend.vout[0].nValue = 1 * COIN;

    // Sign with sender's key
    uint256 sighash = SignatureHash(redeemScript, CTransaction(txSpend), 0, SIGHASH_ALL);
    vector<unsigned char> vchSig;
    sender_key.Sign(sighash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    // Build refund scriptSig
    txSpend.vin[0].scriptSig = CreateHTLCRefundScript(vchSig, redeemScript);

    // Verify
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    BOOST_CHECK(VerifyScript(txSpend.vin[0].scriptSig, scriptPubKey, flags, CTransaction(txSpend), 0));
}

BOOST_AUTO_TEST_CASE(htlc_wrong_preimage_fails)
{
    CKey receiver_key, sender_key;
    receiver_key.MakeNewKey(true);
    sender_key.MakeNewKey(true);

    unsigned char preimage_data[] = "secret preimage";
    vector<unsigned char> hash(32);
    CSHA256().Write(preimage_data, sizeof(preimage_data) - 1).Finalize(hash.data());

    int64_t timeout = 1700000000;

    CScript redeemScript = CreateHTLCScript(hash, receiver_key.GetPubKey(),
                                            sender_key.GetPubKey(), timeout);

    CScript scriptPubKey;
    scriptPubKey << OP_HASH160;
    CScriptID scriptID = redeemScript.GetID();
    scriptPubKey << vector<unsigned char>(scriptID.begin(), scriptID.end());
    scriptPubKey << OP_EQUAL;

    CMutableTransaction txSpend;
    txSpend.nVersion = 2;
    txSpend.nTime = GetAdjustedTime();
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vin[0].prevout.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].nSequence = 0xfffffffe;
    txSpend.vout.resize(1);
    txSpend.vout[0].nValue = 1 * COIN;

    uint256 sighash = SignatureHash(redeemScript, CTransaction(txSpend), 0, SIGHASH_ALL);
    vector<unsigned char> vchSig;
    receiver_key.Sign(sighash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    // Use WRONG preimage
    vector<unsigned char> wrong_preimage = {0x00, 0x01, 0x02, 0x03};
    txSpend.vin[0].scriptSig = CreateHTLCClaimScript(vchSig, wrong_preimage, redeemScript);

    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    BOOST_CHECK(!VerifyScript(txSpend.vin[0].scriptSig, scriptPubKey, flags, CTransaction(txSpend), 0));
}

BOOST_AUTO_TEST_CASE(htlc_wrong_key_fails)
{
    CKey receiver_key, sender_key, wrong_key;
    receiver_key.MakeNewKey(true);
    sender_key.MakeNewKey(true);
    wrong_key.MakeNewKey(true);

    unsigned char preimage_data[] = "secret preimage";
    vector<unsigned char> preimage(preimage_data, preimage_data + sizeof(preimage_data) - 1);
    vector<unsigned char> hash(32);
    CSHA256().Write(preimage.data(), preimage.size()).Finalize(hash.data());

    int64_t timeout = 1700000000;

    CScript redeemScript = CreateHTLCScript(hash, receiver_key.GetPubKey(),
                                            sender_key.GetPubKey(), timeout);

    CScript scriptPubKey;
    scriptPubKey << OP_HASH160;
    CScriptID scriptID = redeemScript.GetID();
    scriptPubKey << vector<unsigned char>(scriptID.begin(), scriptID.end());
    scriptPubKey << OP_EQUAL;

    CMutableTransaction txSpend;
    txSpend.nVersion = 2;
    txSpend.nTime = GetAdjustedTime();
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vin[0].prevout.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].nSequence = 0xfffffffe;
    txSpend.vout.resize(1);
    txSpend.vout[0].nValue = 1 * COIN;

    // Sign with WRONG key
    uint256 sighash = SignatureHash(redeemScript, CTransaction(txSpend), 0, SIGHASH_ALL);
    vector<unsigned char> vchSig;
    wrong_key.Sign(sighash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    txSpend.vin[0].scriptSig = CreateHTLCClaimScript(vchSig, preimage, redeemScript);

    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    BOOST_CHECK(!VerifyScript(txSpend.vin[0].scriptSig, scriptPubKey, flags, CTransaction(txSpend), 0));
}

BOOST_AUTO_TEST_CASE(htlc_early_refund_fails)
{
    CKey receiver_key, sender_key;
    receiver_key.MakeNewKey(true);
    sender_key.MakeNewKey(true);

    unsigned char preimage_data[] = "secret preimage";
    vector<unsigned char> hash(32);
    CSHA256().Write(preimage_data, sizeof(preimage_data) - 1).Finalize(hash.data());

    int64_t timeout = 500; // block height 500

    CScript redeemScript = CreateHTLCScript(hash, receiver_key.GetPubKey(),
                                            sender_key.GetPubKey(), timeout);

    CScript scriptPubKey;
    scriptPubKey << OP_HASH160;
    CScriptID scriptID = redeemScript.GetID();
    scriptPubKey << vector<unsigned char>(scriptID.begin(), scriptID.end());
    scriptPubKey << OP_EQUAL;

    // Create spending tx with nLockTime BEFORE timeout
    CMutableTransaction txSpend;
    txSpend.nVersion = 2;
    txSpend.nTime = GetAdjustedTime();
    txSpend.nLockTime = 400; // before timeout of 500
    txSpend.vin.resize(1);
    txSpend.vin[0].prevout.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].nSequence = 0xfffffffe;
    txSpend.vout.resize(1);
    txSpend.vout[0].nValue = 1 * COIN;

    uint256 sighash = SignatureHash(redeemScript, CTransaction(txSpend), 0, SIGHASH_ALL);
    vector<unsigned char> vchSig;
    sender_key.Sign(sighash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    txSpend.vin[0].scriptSig = CreateHTLCRefundScript(vchSig, redeemScript);

    // CLTV should fail because nLockTime (400) < timeout (500)
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
    BOOST_CHECK(!VerifyScript(txSpend.vin[0].scriptSig, scriptPubKey, flags, CTransaction(txSpend), 0));
}

BOOST_AUTO_TEST_CASE(htlc_parse_invalid_script)
{
    // Parsing a non-HTLC script should return false
    CScript notHTLC;
    notHTLC << OP_DUP << OP_HASH160;

    vector<unsigned char> hash;
    CPubKey receiver, sender;
    int64_t timeout;

    BOOST_CHECK(!ParseHTLCScript(notHTLC, hash, receiver, sender, timeout));
}

BOOST_AUTO_TEST_SUITE_END()
