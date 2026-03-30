// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <keystore.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <psgt.h>
#include <script/interpreter.h>
#include <script/sign.h>
#include <script/standard.h>
#include <streams.h>
#include <util/strencodings.h>

BOOST_AUTO_TEST_SUITE(psgt_tests)

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

// Build a fake previous transaction that pays to the given key.
static CTransaction MakePrevTx(const CKey& key, CAmount amount)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000000;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull(); // coinbase-like
    mtx.vin[0].scriptSig = CScript() << 0;
    mtx.vout.push_back(CTxOut(amount, P2PKH(key.GetPubKey().GetID())));
    return CTransaction(mtx);
}

// ---------------------------------------------------------------------------
// Test 1: Create a PSGT, serialize, deserialize, verify round-trip.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(serialize_deserialize_roundtrip)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 50 * COIN);

    // Build unsigned spending transaction.
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000100;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(49 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;

    // Serialize.
    std::vector<unsigned char> data = SerializePSGT(psgt);
    BOOST_CHECK(!data.empty());

    // Check magic bytes.
    BOOST_CHECK(data.size() >= 5);
    BOOST_CHECK_EQUAL(data[0], 0x70); // 'p'
    BOOST_CHECK_EQUAL(data[1], 0x73); // 's'
    BOOST_CHECK_EQUAL(data[2], 0x67); // 'g'
    BOOST_CHECK_EQUAL(data[3], 0x74); // 't'
    BOOST_CHECK_EQUAL(data[4], 0xff);

    // Base64 round-trip.
    std::string b64 = EncodeBase64(data.data(), data.size());
    PartiallySignedTransaction psgt2;
    std::string error;
    BOOST_CHECK(DecodeRawPSGT(psgt2, b64, error));

    // Unsigned tx must match.
    BOOST_CHECK(psgt2.tx.GetHash() == psgt.tx.GetHash());
    BOOST_CHECK_EQUAL(psgt2.inputs.size(), 1u);
    BOOST_CHECK_EQUAL(psgt2.outputs.size(), 1u);

    // UTXO must survive round-trip.
    BOOST_CHECK(!psgt2.inputs[0].non_witness_utxo.IsNull());
    BOOST_CHECK(psgt2.inputs[0].non_witness_utxo.GetHash() == prevTx.GetHash());
}

// ---------------------------------------------------------------------------
// Test 2: Sign a PSGT input with a keystore.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(sign_p2pkh_input)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 10 * COIN);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000200;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(9 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;

    // Not signed yet.
    BOOST_CHECK(!PSGTInputSigned(psgt.inputs[0]));

    // Create a keystore with the signing key.
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    // Sign.
    BOOST_CHECK(SignPSGTInput(keystore, psgt, 0));
    BOOST_CHECK(PSGTInputSigned(psgt.inputs[0]));
    BOOST_CHECK(!psgt.inputs[0].final_script_sig.empty());
}

// ---------------------------------------------------------------------------
// Test 3: Finalize and extract a signed PSGT.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(finalize_and_extract)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 5 * COIN);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000300;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(4 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;

    CBasicKeyStore keystore;
    keystore.AddKey(key);
    BOOST_CHECK(SignPSGTInput(keystore, psgt, 0));

    // Finalize.
    CMutableTransaction result;
    BOOST_CHECK(FinalizeAndExtractPSGT(psgt, result));

    // The result should have a non-empty scriptSig.
    BOOST_CHECK(!result.vin[0].scriptSig.empty());

    // The scriptSig should verify.
    CTransaction signedTx(result);
    BOOST_CHECK(VerifyScript(
        signedTx.vin[0].scriptSig,
        prevTx.vout[0].scriptPubKey,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        signedTx, 0));
}

// ---------------------------------------------------------------------------
// Test 4: Combine two PSGTs.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(combine_psgts)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 20 * COIN);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000400;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(19 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    // PSGT 1: has the UTXO but no signature.
    PartiallySignedTransaction psgt1(mtx);
    psgt1.inputs[0].non_witness_utxo = prevTx;

    // PSGT 2: has the signature.
    PartiallySignedTransaction psgt2(mtx);
    CBasicKeyStore keystore;
    keystore.AddKey(key);
    psgt2.inputs[0].non_witness_utxo = prevTx;
    SignPSGTInput(keystore, psgt2, 0);

    // Combine.
    PartiallySignedTransaction merged;
    BOOST_CHECK(CombinePSGTs(merged, {psgt1, psgt2}));

    // Merged should have both UTXO and signature.
    BOOST_CHECK(!merged.inputs[0].non_witness_utxo.IsNull());
    BOOST_CHECK(PSGTInputSigned(merged.inputs[0]));

    // Should finalize.
    CMutableTransaction result;
    BOOST_CHECK(FinalizeAndExtractPSGT(merged, result));
}

// ---------------------------------------------------------------------------
// Test 5: Unfinalizable PSGT (missing signature).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(incomplete_psgt_not_finalizable)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 1 * COIN);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000500;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(COIN / 2, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    // Don't sign — just set UTXO.
    psgt.inputs[0].non_witness_utxo = prevTx;

    CMutableTransaction result;
    BOOST_CHECK(!FinalizeAndExtractPSGT(psgt, result));
}

// ---------------------------------------------------------------------------
// Test 6: Mismatched transactions cannot be combined.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(combine_mismatched_fails)
{
    CKey key = MakeKey();

    CMutableTransaction mtx1;
    mtx1.nVersion = 2;
    mtx1.nTime = 1700000600;
    mtx1.vin.resize(1);
    mtx1.vin[0].prevout = COutPoint(uint256S("aaaa"), 0);
    mtx1.vout.push_back(CTxOut(1 * COIN, P2PKH(key.GetPubKey().GetID())));

    CMutableTransaction mtx2;
    mtx2.nVersion = 2;
    mtx2.nTime = 1700000601; // Different time -> different hash.
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout = COutPoint(uint256S("bbbb"), 0);
    mtx2.vout.push_back(CTxOut(1 * COIN, P2PKH(key.GetPubKey().GetID())));

    PartiallySignedTransaction psgt1(mtx1);
    PartiallySignedTransaction psgt2(mtx2);

    PartiallySignedTransaction merged;
    BOOST_CHECK(!CombinePSGTs(merged, {psgt1, psgt2}));
}

// ---------------------------------------------------------------------------
// Test 7: converttopsgt preserves existing scriptSigs as final_script_sig.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(convert_preserves_scriptsig)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 3 * COIN);

    // Build and sign a transaction normally.
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000700;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(2 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    CBasicKeyStore keystore;
    keystore.AddKey(key);
    BOOST_CHECK(SignSignature(keystore, prevTx.vout[0].scriptPubKey, mtx, 0));

    CScript savedSig = mtx.vin[0].scriptSig;
    BOOST_CHECK(!savedSig.empty());

    // Convert to PSGT: the scriptSig should be stored as final_script_sig
    // and the unsigned tx should have empty scriptSigs.
    CMutableTransaction mtxCopy(mtx);
    std::vector<CScript> savedScripts;
    for (auto& vin : mtxCopy.vin)
    {
        savedScripts.push_back(vin.scriptSig);
        vin.scriptSig.clear();
    }

    PartiallySignedTransaction psgt(mtxCopy);
    for (unsigned int i = 0; i < savedScripts.size(); ++i)
    {
        if (!savedScripts[i].empty())
            psgt.inputs[i].final_script_sig = savedScripts[i];
    }

    BOOST_CHECK(PSGTInputSigned(psgt.inputs[0]));
    BOOST_CHECK(psgt.inputs[0].final_script_sig == savedSig);

    // Should finalize since it already has the scriptSig.
    CMutableTransaction result;
    BOOST_CHECK(FinalizeAndExtractPSGT(psgt, result));
    BOOST_CHECK(result.vin[0].scriptSig == savedSig);
}

// ---------------------------------------------------------------------------
// Test 8: Gridcoin nTime preserved through PSGT round-trip.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ntime_preserved)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700000800;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("cccc"), 0);
    mtx.vout.push_back(CTxOut(1 * COIN, CScript() << OP_TRUE));

    PartiallySignedTransaction psgt(mtx);

    std::vector<unsigned char> data = SerializePSGT(psgt);
    std::string b64 = EncodeBase64(data.data(), data.size());

    PartiallySignedTransaction psgt2;
    std::string error;
    BOOST_CHECK(DecodeRawPSGT(psgt2, b64, error));
    BOOST_CHECK_EQUAL(psgt2.tx.nTime, 1700000800u);
}

// ---------------------------------------------------------------------------
// Test 9: Invalid base64 is rejected.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(invalid_base64_rejected)
{
    PartiallySignedTransaction psgt;
    std::string error;
    BOOST_CHECK(!DecodeRawPSGT(psgt, "not-valid-base64!!!", error));
}

// ---------------------------------------------------------------------------
// Test 10: Wrong magic bytes rejected.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(wrong_magic_rejected)
{
    // Encode some data with wrong magic.
    std::vector<unsigned char> data = {0x70, 0x73, 0x62, 0x74, 0xff}; // "psbt" not "psgt"
    data.push_back(0x00); // global separator
    std::string b64 = EncodeBase64(data.data(), data.size());

    PartiallySignedTransaction psgt;
    std::string error;
    BOOST_CHECK(!DecodeRawPSGT(psgt, b64, error));
    BOOST_CHECK(error.find("magic") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Test 11: 2-of-3 multisig — sign with both keys, finalize succeeds.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(multisig_2of3_full_sign)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();
    CKey key3 = MakeKey();

    // Build a 2-of-3 multisig script.
    CScript multisig;
    multisig << OP_2
             << key1.GetPubKey() << key2.GetPubKey() << key3.GetPubKey()
             << OP_3 << OP_CHECKMULTISIG;

    // Build a previous tx that pays to the multisig.
    CMutableTransaction prevMtx;
    prevMtx.nVersion = 2;
    prevMtx.nTime = 1700001000;
    prevMtx.vin.resize(1);
    prevMtx.vin[0].prevout.SetNull();
    prevMtx.vin[0].scriptSig = CScript() << 0;
    prevMtx.vout.push_back(CTxOut(10 * COIN, multisig));
    CTransaction prevTx(prevMtx);

    // Build the spending transaction.
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700001100;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(9 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;

    // Sign with key1 only.
    CBasicKeyStore ks1;
    ks1.AddKey(key1);
    BOOST_CHECK(SignPSGTInput(ks1, psgt, 0));
    BOOST_CHECK_EQUAL(psgt.inputs[0].partial_sigs.size(), 1u);
    BOOST_CHECK(!PSGTInputSigned(psgt.inputs[0])); // Not finalized yet.

    // Sign with key2.
    CBasicKeyStore ks2;
    ks2.AddKey(key2);
    BOOST_CHECK(SignPSGTInput(ks2, psgt, 0));
    BOOST_CHECK_EQUAL(psgt.inputs[0].partial_sigs.size(), 2u);

    // Finalize — should succeed with 2 of 3.
    BOOST_CHECK(FinalizePSGT(psgt));
    BOOST_CHECK(PSGTInputSigned(psgt.inputs[0]));

    // Extract and verify.
    CMutableTransaction result;
    BOOST_CHECK(FinalizeAndExtractPSGT(psgt, result));
    BOOST_CHECK(!result.vin[0].scriptSig.empty());

    CTransaction signedTx(result);
    BOOST_CHECK(VerifyScript(
        signedTx.vin[0].scriptSig, multisig,
        STANDARD_SCRIPT_VERIFY_FLAGS, signedTx, 0));
}

// ---------------------------------------------------------------------------
// Test 12: 2-of-3 multisig — combine two independently-signed PSGTs.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(multisig_combine_partial)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();
    CKey key3 = MakeKey();

    CScript multisig;
    multisig << OP_2
             << key1.GetPubKey() << key2.GetPubKey() << key3.GetPubKey()
             << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction prevMtx;
    prevMtx.nVersion = 2;
    prevMtx.nTime = 1700001200;
    prevMtx.vin.resize(1);
    prevMtx.vin[0].prevout.SetNull();
    prevMtx.vin[0].scriptSig = CScript() << 0;
    prevMtx.vout.push_back(CTxOut(10 * COIN, multisig));
    CTransaction prevTx(prevMtx);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700001300;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(9 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    // Signer 1 signs independently.
    PartiallySignedTransaction psgt1(mtx);
    psgt1.inputs[0].non_witness_utxo = prevTx;
    CBasicKeyStore ks1;
    ks1.AddKey(key1);
    BOOST_CHECK(SignPSGTInput(ks1, psgt1, 0));
    BOOST_CHECK_EQUAL(psgt1.inputs[0].partial_sigs.size(), 1u);

    // Signer 2 signs independently.
    PartiallySignedTransaction psgt2(mtx);
    psgt2.inputs[0].non_witness_utxo = prevTx;
    CBasicKeyStore ks2;
    ks2.AddKey(key3); // Using key3 to show any 2 of 3 works.
    BOOST_CHECK(SignPSGTInput(ks2, psgt2, 0));
    BOOST_CHECK_EQUAL(psgt2.inputs[0].partial_sigs.size(), 1u);

    // Combine.
    PartiallySignedTransaction merged;
    BOOST_CHECK(CombinePSGTs(merged, {psgt1, psgt2}));
    BOOST_CHECK_EQUAL(merged.inputs[0].partial_sigs.size(), 2u);

    // Finalize and verify.
    BOOST_CHECK(FinalizePSGT(merged));
    CMutableTransaction result;
    BOOST_CHECK(FinalizeAndExtractPSGT(merged, result));

    CTransaction signedTx(result);
    BOOST_CHECK(VerifyScript(
        signedTx.vin[0].scriptSig, multisig,
        STANDARD_SCRIPT_VERIFY_FLAGS, signedTx, 0));
}

// ---------------------------------------------------------------------------
// Test 13: Incomplete multisig — only 1 of 2 required sigs, finalize fails.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(multisig_incomplete_fails)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();
    CKey key3 = MakeKey();

    CScript multisig;
    multisig << OP_2
             << key1.GetPubKey() << key2.GetPubKey() << key3.GetPubKey()
             << OP_3 << OP_CHECKMULTISIG;

    CMutableTransaction prevMtx;
    prevMtx.nVersion = 2;
    prevMtx.nTime = 1700001400;
    prevMtx.vin.resize(1);
    prevMtx.vin[0].prevout.SetNull();
    prevMtx.vin[0].scriptSig = CScript() << 0;
    prevMtx.vout.push_back(CTxOut(10 * COIN, multisig));
    CTransaction prevTx(prevMtx);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700001500;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(9 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;

    // Sign with only key1 (need 2).
    CBasicKeyStore ks1;
    ks1.AddKey(key1);
    BOOST_CHECK(SignPSGTInput(ks1, psgt, 0));
    BOOST_CHECK_EQUAL(psgt.inputs[0].partial_sigs.size(), 1u);

    // Finalize should fail — only 1 of 2 required.
    BOOST_CHECK(!FinalizePSGT(psgt));
    BOOST_CHECK(!PSGTInputSigned(psgt.inputs[0]));
}

// ---------------------------------------------------------------------------
// Test 14: Multisig partial sigs survive serialize/deserialize round-trip.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(multisig_partial_sigs_roundtrip)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();

    CScript multisig;
    multisig << OP_2
             << key1.GetPubKey() << key2.GetPubKey()
             << OP_2 << OP_CHECKMULTISIG;

    CMutableTransaction prevMtx;
    prevMtx.nVersion = 2;
    prevMtx.nTime = 1700001600;
    prevMtx.vin.resize(1);
    prevMtx.vin[0].prevout.SetNull();
    prevMtx.vin[0].scriptSig = CScript() << 0;
    prevMtx.vout.push_back(CTxOut(5 * COIN, multisig));
    CTransaction prevTx(prevMtx);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700001700;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(4 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;

    // Sign with key1 only.
    CBasicKeyStore ks1;
    ks1.AddKey(key1);
    BOOST_CHECK(SignPSGTInput(ks1, psgt, 0));
    BOOST_CHECK_EQUAL(psgt.inputs[0].partial_sigs.size(), 1u);

    // Serialize and deserialize.
    std::vector<unsigned char> data = SerializePSGT(psgt);
    std::string b64 = EncodeBase64(data.data(), data.size());

    PartiallySignedTransaction psgt2;
    std::string error;
    BOOST_CHECK(DecodeRawPSGT(psgt2, b64, error));

    // Partial sigs should survive round-trip.
    BOOST_CHECK_EQUAL(psgt2.inputs[0].partial_sigs.size(), 1u);

    // Sign with key2 on the deserialized PSGT.
    CBasicKeyStore ks2;
    ks2.AddKey(key2);
    BOOST_CHECK(SignPSGTInput(ks2, psgt2, 0));
    BOOST_CHECK_EQUAL(psgt2.inputs[0].partial_sigs.size(), 2u);

    // Finalize and verify.
    BOOST_CHECK(FinalizePSGT(psgt2));
    CMutableTransaction result;
    BOOST_CHECK(FinalizeAndExtractPSGT(psgt2, result));

    CTransaction signedTx(result);
    BOOST_CHECK(VerifyScript(
        signedTx.vin[0].scriptSig, multisig,
        STANDARD_SCRIPT_VERIFY_FLAGS, signedTx, 0));
}

BOOST_AUTO_TEST_SUITE_END()
