// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <keystore.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <psgt.h>
#include <script/interpreter.h>
#include <script/sign.h>
#include <script/standard.h>
#include <streams.h>
#include <test/psgt_test_helpers.h>
#include <util/strencodings.h>
#include <version.h>

BOOST_AUTO_TEST_SUITE(psgt_tests)

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
// Test 12b: P2SH-wrapped 2-of-3 multisig — two signers, each holding only one
// key, sign independently; combine, finalize, extract, and verify the script.
// This is the end-to-end path the Multisign (PSGT) GUI dialog drives: a P2SH
// multisig address (as addmultisigaddress produces) where no single wallet can
// complete the spend, so the combine step is genuinely exercised.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(multisig_p2sh_combine_roundtrip)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();
    CKey key3 = MakeKey();

    // 2-of-3 redeem script wrapped in P2SH.
    CScript redeem;
    redeem << OP_2
           << key1.GetPubKey() << key2.GetPubKey() << key3.GetPubKey()
           << OP_3 << OP_CHECKMULTISIG;
    CScript scriptPubKey;
    scriptPubKey << OP_HASH160 << CScriptID(redeem) << OP_EQUAL;

    // Previous tx pays to the P2SH address.
    CMutableTransaction prevMtx;
    prevMtx.nVersion = 2;
    prevMtx.nTime = 1700001600;
    prevMtx.vin.resize(1);
    prevMtx.vin[0].prevout.SetNull();
    prevMtx.vin[0].scriptSig = CScript() << 0;
    prevMtx.vout.push_back(CTxOut(10 * COIN, scriptPubKey));
    CTransaction prevTx(prevMtx);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700001700;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(9 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    // Signer 1 holds only key1.
    PartiallySignedTransaction psgt1(mtx);
    psgt1.inputs[0].non_witness_utxo = prevTx;
    psgt1.inputs[0].redeem_script = redeem;
    CBasicKeyStore ks1;
    ks1.AddKey(key1);
    BOOST_CHECK(SignPSGTInput(ks1, psgt1, 0));
    BOOST_CHECK_EQUAL(psgt1.inputs[0].partial_sigs.size(), 1u);

    // Signer 2 holds only key3 (any 2 of the 3 keys suffice).
    PartiallySignedTransaction psgt2(mtx);
    psgt2.inputs[0].non_witness_utxo = prevTx;
    psgt2.inputs[0].redeem_script = redeem;
    CBasicKeyStore ks2;
    ks2.AddKey(key3);
    BOOST_CHECK(SignPSGTInput(ks2, psgt2, 0));
    BOOST_CHECK_EQUAL(psgt2.inputs[0].partial_sigs.size(), 1u);

    // The real GUI flow exchanges PSGTs as base64 between co-signers, so a
    // co-signer's redeem_script (auto-filled from their wallet) must survive
    // serialization for the combining party to finalize. Round-trip signer 2's
    // PSGT through base64 before combining and confirm the field is preserved.
    {
        std::vector<unsigned char> ser = SerializePSGT(psgt2);
        std::string b64 = EncodeBase64(ser.data(), ser.size());
        PartiallySignedTransaction psgt2_decoded;
        std::string error;
        BOOST_REQUIRE(DecodeRawPSGT(psgt2_decoded, b64, error));
        BOOST_CHECK(psgt2_decoded.inputs[0].redeem_script == redeem);
        BOOST_CHECK_EQUAL(psgt2_decoded.inputs[0].partial_sigs.size(), 1u);
        psgt2 = psgt2_decoded;
    }

    // Neither single signer's PSGT can finalize on its own (1 of 2 required).
    {
        PartiallySignedTransaction lone = psgt1;
        CMutableTransaction tmp;
        BOOST_CHECK(!FinalizeAndExtractPSGT(lone, tmp));
    }

    // Combine the two independently-signed PSGTs, then finalize and extract.
    PartiallySignedTransaction merged;
    BOOST_REQUIRE(CombinePSGTs(merged, {psgt1, psgt2}));
    BOOST_REQUIRE_EQUAL(merged.inputs[0].partial_sigs.size(), 2u);

    CMutableTransaction result;
    BOOST_REQUIRE(FinalizeAndExtractPSGT(merged, result));

    // The extracted scriptSig (OP_0 <sig> <sig> <redeemScript>) must satisfy the
    // P2SH output under standard flags (which include SCRIPT_VERIFY_P2SH).
    CTransaction signedTx(result);
    BOOST_CHECK(VerifyScript(
        signedTx.vin[0].scriptSig, scriptPubKey,
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

// ---------------------------------------------------------------------------
// Test 15: HD keypaths survive serialize/deserialize round-trip.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(hd_keypaths_roundtrip)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 5 * COIN);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.nTime = 1700001800;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prevTx.GetHash(), 0);
    mtx.vout.push_back(CTxOut(4 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

    PartiallySignedTransaction psgt(mtx);
    psgt.inputs[0].non_witness_utxo = prevTx;

    // Manually populate HD keypath for the input.
    KeyOriginInfo info;
    info.fingerprint[0] = 0xDE; info.fingerprint[1] = 0xAD;
    info.fingerprint[2] = 0xBE; info.fingerprint[3] = 0xEF;
    info.path = {0x80000000, 0x80000000, 0x80000005}; // m/0'/0'/5'
    psgt.inputs[0].hd_keypaths[key.GetPubKey()] = info;

    // Also add an HD keypath for the output.
    CKey outKey = MakeKey();
    KeyOriginInfo outInfo;
    outInfo.fingerprint[0] = 0xCA; outInfo.fingerprint[1] = 0xFE;
    outInfo.fingerprint[2] = 0xBA; outInfo.fingerprint[3] = 0xBE;
    outInfo.path = {0x80000000, 0x80000000, 0x8000002A}; // m/0'/0'/42'
    psgt.outputs[0].hd_keypaths[outKey.GetPubKey()] = outInfo;

    // Serialize and deserialize.
    std::vector<unsigned char> data = SerializePSGT(psgt);
    std::string b64 = EncodeBase64(data.data(), data.size());

    PartiallySignedTransaction psgt2;
    std::string error;
    BOOST_CHECK(DecodeRawPSGT(psgt2, b64, error));

    // Input HD keypaths should survive.
    BOOST_CHECK_EQUAL(psgt2.inputs[0].hd_keypaths.size(), 1u);
    auto it = psgt2.inputs[0].hd_keypaths.find(key.GetPubKey());
    BOOST_CHECK(it != psgt2.inputs[0].hd_keypaths.end());
    BOOST_CHECK_EQUAL(it->second.fingerprint[0], 0xDE);
    BOOST_CHECK_EQUAL(it->second.fingerprint[3], 0xEF);
    BOOST_CHECK_EQUAL(it->second.path.size(), 3u);
    BOOST_CHECK_EQUAL(it->second.path[2], 0x80000005u);

    // Output HD keypaths should survive.
    BOOST_CHECK_EQUAL(psgt2.outputs[0].hd_keypaths.size(), 1u);
    auto it2 = psgt2.outputs[0].hd_keypaths.find(outKey.GetPubKey());
    BOOST_CHECK(it2 != psgt2.outputs[0].hd_keypaths.end());
    BOOST_CHECK_EQUAL(it2->second.fingerprint[0], 0xCA);
    BOOST_CHECK_EQUAL(it2->second.path[2], 0x8000002Au);
}

// ---------------------------------------------------------------------------
// AnalyzePSGT tests.
// ---------------------------------------------------------------------------

// Test: unsigned P2PKH input — signer's turn, missing sig and pubkey,
// fee/size/min-fee all computable.
BOOST_AUTO_TEST_CASE(analyze_p2pkh_unsigned)
{
    CKey key = MakeKey();
    CKeyID keyid = key.GetPubKey().GetID();
    CTransaction prevTx = MakePrevTx(key, 10 * COIN);
    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 9 * COIN);

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_REQUIRE_EQUAL(analysis.inputs.size(), 1u);
    const PSGTInputAnalysis& ia = analysis.inputs[0];
    BOOST_CHECK(ia.has_utxo);
    BOOST_CHECK(!ia.is_final);
    BOOST_CHECK(ia.next == PSGTRole::SIGNER);
    BOOST_REQUIRE_EQUAL(ia.missing_sigs.size(), 1u);
    BOOST_CHECK(ia.missing_sigs[0] == keyid);
    // No hd_keypaths entry, so the full pubkey is unknown to the PSGT.
    BOOST_REQUIRE_EQUAL(ia.missing_pubkeys.size(), 1u);
    BOOST_CHECK(ia.missing_pubkeys[0] == keyid);
    BOOST_CHECK(!ia.missing_redeem_script);

    BOOST_CHECK(analysis.next == PSGTRole::SIGNER);
    BOOST_CHECK(analysis.error.empty());

    BOOST_REQUIRE(analysis.fee.has_value());
    BOOST_CHECK_EQUAL(*analysis.fee, 1 * COIN);

    // Estimated size = unsigned size + dummy P2PKH scriptSig (107 bytes).
    BOOST_REQUIRE(analysis.estimated_final_size.has_value());
    const unsigned int unsigned_size =
        ::GetSerializeSize(CTransaction(psgt.tx), SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_EQUAL(*analysis.estimated_final_size, unsigned_size + 107);

    BOOST_REQUIRE(analysis.min_required_fee.has_value());
    BOOST_CHECK_EQUAL(*analysis.min_required_fee,
        GetMinFee(CTransaction(psgt.tx), 1000, GMF_SEND, *analysis.estimated_final_size));

    // With a pubkey present in hd_keypaths, only the signature is missing.
    psgt.inputs[0].hd_keypaths[key.GetPubKey()] = KeyOriginInfo();
    analysis = AnalyzePSGT(psgt);
    BOOST_CHECK(analysis.inputs[0].missing_pubkeys.empty());
    BOOST_CHECK_EQUAL(analysis.inputs[0].missing_sigs.size(), 1u);
}

// Test: input without UTXO data — updater's turn, no fee or size estimate.
BOOST_AUTO_TEST_CASE(analyze_no_utxo)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 10 * COIN);
    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 9 * COIN);
    psgt.inputs[0].non_witness_utxo = CTransaction();

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_REQUIRE_EQUAL(analysis.inputs.size(), 1u);
    BOOST_CHECK(!analysis.inputs[0].has_utxo);
    BOOST_CHECK(analysis.inputs[0].next == PSGTRole::UPDATER);
    BOOST_CHECK(analysis.next == PSGTRole::UPDATER);
    BOOST_CHECK(!analysis.fee.has_value());
    BOOST_CHECK(!analysis.estimated_final_size.has_value());
    BOOST_CHECK(!analysis.min_required_fee.has_value());
}

// Test: provided UTXO does not match the input's prevout hash — treated as
// missing (stricter than SignPSGTInput, which skips the hash check).
BOOST_AUTO_TEST_CASE(analyze_wrong_utxo_hash)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 10 * COIN);
    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 9 * COIN);
    // Substitute a different transaction as the claimed UTXO.
    psgt.inputs[0].non_witness_utxo = MakePrevTx(MakeKey(), 10 * COIN);

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_REQUIRE_EQUAL(analysis.inputs.size(), 1u);
    BOOST_CHECK(!analysis.inputs[0].has_utxo);
    BOOST_CHECK(analysis.inputs[0].next == PSGTRole::UPDATER);
    BOOST_CHECK(!analysis.fee.has_value());
}

// Test: P2SH 1-of-2 multisig — signer's turn until enough partial sigs,
// then finalizer's turn.
BOOST_AUTO_TEST_CASE(analyze_multisig_partial)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();
    CScript redeem = Multisig1of2(key1, key2);
    CTransaction prevTx = MakePrevTxScript(P2SH(CScriptID(redeem)), 10 * COIN);

    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 9 * COIN);
    psgt.inputs[0].redeem_script = redeem;

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_REQUIRE_EQUAL(analysis.inputs.size(), 1u);
    BOOST_CHECK(analysis.inputs[0].has_utxo);
    BOOST_CHECK(analysis.inputs[0].next == PSGTRole::SIGNER);
    BOOST_CHECK_EQUAL(analysis.inputs[0].missing_sigs.size(), 2u);
    BOOST_CHECK(analysis.next == PSGTRole::SIGNER);

    // Estimated scriptSig: OP_0 + 1 dummy sig + redeem script push.
    BOOST_REQUIRE(analysis.estimated_final_size.has_value());
    const unsigned int unsigned_size =
        ::GetSerializeSize(CTransaction(psgt.tx), SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_EQUAL(*analysis.estimated_final_size,
        unsigned_size + 1 + 73 + redeem.size() + 1);

    // Sign with one key — 1-of-2 is now satisfiable: finalizer's turn.
    CBasicKeyStore keystore;
    keystore.AddKey(key1);
    keystore.AddCScript(redeem);
    BOOST_REQUIRE(SignPSGTInput(keystore, psgt, 0));
    BOOST_REQUIRE_EQUAL(psgt.inputs[0].partial_sigs.size(), 1u);

    analysis = AnalyzePSGT(psgt);
    BOOST_CHECK(analysis.inputs[0].next == PSGTRole::FINALIZER);
    BOOST_CHECK(analysis.inputs[0].missing_sigs.empty());
    BOOST_CHECK(analysis.next == PSGTRole::FINALIZER);
}

// Test: P2SH input with unknown redeem script — updater's turn; fee is still
// known (the UTXO is present) but the final size is not estimable.
BOOST_AUTO_TEST_CASE(analyze_p2sh_missing_redeem)
{
    CKey key1 = MakeKey();
    CKey key2 = MakeKey();
    CScript redeem = Multisig1of2(key1, key2);
    CTransaction prevTx = MakePrevTxScript(P2SH(CScriptID(redeem)), 10 * COIN);

    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 9 * COIN);

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_REQUIRE_EQUAL(analysis.inputs.size(), 1u);
    BOOST_CHECK(analysis.inputs[0].has_utxo);
    BOOST_CHECK(analysis.inputs[0].missing_redeem_script);
    BOOST_CHECK(analysis.inputs[0].next == PSGTRole::UPDATER);
    BOOST_CHECK(analysis.next == PSGTRole::UPDATER);
    BOOST_REQUIRE(analysis.fee.has_value());
    BOOST_CHECK_EQUAL(*analysis.fee, 1 * COIN);
    BOOST_CHECK(!analysis.estimated_final_size.has_value());
}

// Test: fully signed input — extractor's turn; the actual final scriptSig is
// used for the size estimate.
BOOST_AUTO_TEST_CASE(analyze_final_extractor)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 10 * COIN);
    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 9 * COIN);

    CBasicKeyStore keystore;
    keystore.AddKey(key);
    BOOST_REQUIRE(SignPSGTInput(keystore, psgt, 0));
    BOOST_REQUIRE(PSGTInputSigned(psgt.inputs[0]));

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_REQUIRE_EQUAL(analysis.inputs.size(), 1u);
    BOOST_CHECK(analysis.inputs[0].is_final);
    BOOST_CHECK(analysis.inputs[0].next == PSGTRole::EXTRACTOR);
    BOOST_CHECK(analysis.inputs[0].missing_sigs.empty());
    BOOST_CHECK(analysis.next == PSGTRole::EXTRACTOR);

    // The estimate uses the actual final scriptSig, so it must equal the
    // size of the extracted, fully-signed transaction.
    CMutableTransaction final_tx = psgt.tx;
    final_tx.vin[0].scriptSig = psgt.inputs[0].final_script_sig;
    BOOST_REQUIRE(analysis.estimated_final_size.has_value());
    BOOST_CHECK_EQUAL(*analysis.estimated_final_size,
        ::GetSerializeSize(CTransaction(final_tx), SER_NETWORK, PROTOCOL_VERSION));
}

// Test: outputs exceed inputs — error reported with a negative fee; and a
// PSGT with no inputs is the creator's problem.
BOOST_AUTO_TEST_CASE(analyze_negative_fee_and_empty)
{
    CKey key = MakeKey();
    CTransaction prevTx = MakePrevTx(key, 10 * COIN);
    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 11 * COIN);

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_CHECK(!analysis.error.empty());
    BOOST_REQUIRE(analysis.fee.has_value());
    BOOST_CHECK_EQUAL(*analysis.fee, -1 * COIN);

    // Empty PSGT: nothing to analyze, back to the creator.
    PartiallySignedTransaction empty;
    analysis = AnalyzePSGT(empty);
    BOOST_CHECK(analysis.inputs.empty());
    BOOST_CHECK(analysis.next == PSGTRole::CREATOR);
    BOOST_CHECK(analysis.error.empty());
}

// Test: when an input error and the negative-fee condition hold at once, the
// per-input message recorded first is not overwritten by the fee check.
BOOST_AUTO_TEST_CASE(analyze_error_precedence)
{
    // A bare OP_TRUE matches no script template: non-standard. The UTXO
    // itself is still known, so the fee gets computed -- and is negative,
    // since the spend pays out more than the input carries.
    CTransaction prevTx = MakePrevTxScript(CScript() << OP_TRUE, 1 * COIN);
    PartiallySignedTransaction psgt = MakeSpendPSGT(prevTx, 2 * COIN);

    PSGTAnalysis analysis = AnalyzePSGT(psgt);

    BOOST_REQUIRE_EQUAL(analysis.inputs.size(), 1u);
    BOOST_CHECK(analysis.inputs[0].has_utxo);
    BOOST_CHECK(analysis.inputs[0].next == PSGTRole::UPDATER);
    BOOST_CHECK(!analysis.estimated_final_size.has_value());

    BOOST_REQUIRE(analysis.fee.has_value());
    BOOST_CHECK_EQUAL(*analysis.fee, -1 * COIN);

    BOOST_CHECK_EQUAL(analysis.error,
        "Input 0 spends a non-standard or unspendable output");
}

// ---------------------------------------------------------------------------
// Phase II groundwork (#2910): bytes decoder, image helpers, partial-sig
// verification.
// ---------------------------------------------------------------------------

// A funded 2-of-3 P2SH multisig spend with the redeem script attached --
// the shape the PSGT pool operates on.
struct Multisig2of3PSGT
{
    CKey k1, k2, k3;
    CScript redeem;
    PartiallySignedTransaction psgt;

    Multisig2of3PSGT()
        : k1(MakeKey()), k2(MakeKey()), k3(MakeKey())
        , redeem(MultisigScript(2, {k1.GetPubKey(), k2.GetPubKey(), k3.GetPubKey()}))
    {
        CTransaction prevTx = MakePrevTxScript(P2SH(CScriptID(redeem)), 10 * COIN);
        psgt = MakeSpendPSGT(prevTx, 9 * COIN);
        psgt.inputs[0].redeem_script = redeem;
    }

    void Sign(const CKey& key)
    {
        CBasicKeyStore keystore;
        keystore.AddKey(key);
        keystore.AddCScript(redeem);
        BOOST_REQUIRE(SignPSGTInput(keystore, psgt, 0));
    }
};

// Test: DecodePSGTBytes accepts what SerializePSGT produced, agrees with the
// base64 path, and rejects corrupted magic.
BOOST_AUTO_TEST_CASE(decode_bytes_matches_base64)
{
    Multisig2of3PSGT fixture;
    fixture.Sign(fixture.k1);

    const std::vector<unsigned char> data = SerializePSGT(fixture.psgt);

    PartiallySignedTransaction from_bytes;
    PartiallySignedTransaction from_base64;
    std::string error;

    BOOST_REQUIRE_MESSAGE(DecodePSGTBytes(from_bytes, data, error), error);
    BOOST_REQUIRE_MESSAGE(
        DecodeRawPSGT(from_base64, EncodeBase64(data.data(), data.size()), error), error);

    BOOST_CHECK(SerializePSGT(from_bytes) == data);
    BOOST_CHECK(SerializePSGT(from_base64) == data);

    std::vector<unsigned char> bad_magic = data;
    bad_magic[0] ^= 0x01;
    PartiallySignedTransaction rejected;
    BOOST_CHECK(!DecodePSGTBytes(rejected, bad_magic, error));
    BOOST_CHECK_EQUAL(error, "Invalid PSGT magic bytes");
}

// Test: image = CScriptID(redeem_script) for a trustworthy P2SH multisig
// input; non-P2SH inputs and untrustworthy redeem scripts have none.
BOOST_AUTO_TEST_CASE(image_and_multisig_params)
{
    Multisig2of3PSGT fixture;

    const std::optional<CScriptID> image = GetPSGTImage(fixture.psgt);
    BOOST_REQUIRE(image.has_value());
    BOOST_CHECK(*image == CScriptID(fixture.redeem));

    int required = 0;
    int total = 0;
    BOOST_REQUIRE(GetPSGTMultisigParams(fixture.psgt, required, total));
    BOOST_CHECK_EQUAL(required, 2);
    BOOST_CHECK_EQUAL(total, 3);

    // A P2PKH input has no image.
    CKey plain = MakeKey();
    PartiallySignedTransaction p2pkh = MakeSpendPSGT(MakePrevTx(plain, 10 * COIN), 9 * COIN);
    BOOST_CHECK(!GetPSGTInputImage(p2pkh, 0).has_value());
    BOOST_CHECK(!GetPSGTImage(p2pkh).has_value());

    // A redeem script that does not hash to the funded script hash is not
    // trusted, so the input has no image.
    Multisig2of3PSGT lying;
    lying.psgt.inputs[0].redeem_script = Multisig1of2(MakeKey(), MakeKey());
    BOOST_CHECK(!GetPSGTInputImage(lying.psgt, 0).has_value());
}

// Test: a multi-input spend from the SAME multisig shares one image; inputs
// from different multisig arrangements do not.
BOOST_AUTO_TEST_CASE(image_multi_input)
{
    CKey k1 = MakeKey();
    CKey k2 = MakeKey();
    CKey k3 = MakeKey();
    const CScript redeem_a = MultisigScript(2, {k1.GetPubKey(), k2.GetPubKey(), k3.GetPubKey()});
    const CScript redeem_b = Multisig1of2(MakeKey(), MakeKey());

    // Distinct amounts so the two funding transactions hash differently.
    CTransaction prev1 = MakePrevTxScript(P2SH(CScriptID(redeem_a)), 10 * COIN);
    CTransaction prev2 = MakePrevTxScript(P2SH(CScriptID(redeem_a)), 20 * COIN);
    CTransaction prev_b = MakePrevTxScript(P2SH(CScriptID(redeem_b)), 30 * COIN);

    const auto make_two_input_psgt = [](const CTransaction& first, const CTransaction& second,
                                        const CScript& first_redeem, const CScript& second_redeem) {
        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.nTime = 1700002000;
        mtx.vin.resize(2);
        mtx.vin[0].prevout = COutPoint(first.GetHash(), 0);
        mtx.vin[1].prevout = COutPoint(second.GetHash(), 0);
        mtx.vout.push_back(CTxOut(25 * COIN, P2PKH(MakeKey().GetPubKey().GetID())));

        PartiallySignedTransaction psgt(mtx);
        psgt.inputs[0].non_witness_utxo = first;
        psgt.inputs[0].redeem_script = first_redeem;
        psgt.inputs[1].non_witness_utxo = second;
        psgt.inputs[1].redeem_script = second_redeem;
        return psgt;
    };

    // Same arrangement on both inputs: one image.
    PartiallySignedTransaction same = make_two_input_psgt(prev1, prev2, redeem_a, redeem_a);
    const std::optional<CScriptID> image = GetPSGTImage(same);
    BOOST_REQUIRE(image.has_value());
    BOOST_CHECK(*image == CScriptID(redeem_a));

    // Mixed arrangements: both inputs have an image, the PSGT does not.
    PartiallySignedTransaction mixed = make_two_input_psgt(prev1, prev_b, redeem_a, redeem_b);
    BOOST_CHECK(GetPSGTInputImage(mixed, 0).has_value());
    BOOST_CHECK(GetPSGTInputImage(mixed, 1).has_value());
    BOOST_CHECK(!GetPSGTImage(mixed).has_value());
}

// Test: VerifyPSGTPartialSigs verifies every signature cryptographically and
// reports per-input signer sets; any invalid or foreign entry fails the whole
// PSGT.
BOOST_AUTO_TEST_CASE(verify_partial_sigs)
{
    Multisig2of3PSGT fixture;
    fixture.Sign(fixture.k1);

    std::vector<std::set<CKeyID>> valid;
    std::string error;

    BOOST_REQUIRE_MESSAGE(VerifyPSGTPartialSigs(fixture.psgt, valid, error), error);
    BOOST_REQUIRE_EQUAL(valid.size(), 1u);
    BOOST_CHECK_EQUAL(valid[0].size(), 1u);
    BOOST_CHECK(valid[0].count(fixture.k1.GetPubKey().GetID()));

    // Second signer: both keys now verify.
    fixture.Sign(fixture.k2);
    BOOST_REQUIRE_MESSAGE(VerifyPSGTPartialSigs(fixture.psgt, valid, error), error);
    BOOST_CHECK_EQUAL(valid[0].size(), 2u);
    BOOST_CHECK(valid[0].count(fixture.k2.GetPubKey().GetID()));

    // A corrupted signature poisons the whole PSGT (CombinePSGTs would merge
    // it silently; the pool must not).
    {
        PartiallySignedTransaction corrupted = fixture.psgt;
        auto& sig = corrupted.inputs[0].partial_sigs[fixture.k1.GetPubKey().GetID()];
        sig[sig.size() / 2] ^= 0x01;
        BOOST_CHECK(!VerifyPSGTPartialSigs(corrupted, valid, error));
    }

    // An entry keyed by a non-member key fails.
    {
        PartiallySignedTransaction foreign = fixture.psgt;
        foreign.inputs[0].partial_sigs[MakeKey().GetPubKey().GetID()] =
            fixture.psgt.inputs[0].partial_sigs.begin()->second;
        BOOST_CHECK(!VerifyPSGTPartialSigs(foreign, valid, error));
    }

    // Signatures without a verifiable signing context (no matching previous
    // transaction) fail rather than being skipped.
    {
        PartiallySignedTransaction no_utxo = fixture.psgt;
        no_utxo.inputs[0].non_witness_utxo = CTransaction();
        BOOST_CHECK(!VerifyPSGTPartialSigs(no_utxo, valid, error));
    }
}

// Test: PSGTSignedBy is keyed to the provider's own keys and tolerant of
// other signers' garbage.
BOOST_AUTO_TEST_CASE(psgt_signed_by)
{
    Multisig2of3PSGT fixture;
    fixture.Sign(fixture.k1);

    CBasicKeyStore holder1;
    holder1.AddKey(fixture.k1);
    CBasicKeyStore holder2;
    holder2.AddKey(fixture.k2);

    BOOST_CHECK(PSGTSignedBy(holder1, fixture.psgt));
    BOOST_CHECK(!PSGTSignedBy(holder2, fixture.psgt));

    // k2 signs; now both holders pass.
    fixture.Sign(fixture.k2);
    BOOST_CHECK(PSGTSignedBy(holder2, fixture.psgt));

    // Corrupt k1's signature: holder1 no longer passes (presence under an
    // owned key id is not enough), holder2 is unaffected.
    auto& sig = fixture.psgt.inputs[0].partial_sigs[fixture.k1.GetPubKey().GetID()];
    sig[sig.size() / 2] ^= 0x01;
    BOOST_CHECK(!PSGTSignedBy(holder1, fixture.psgt));
    BOOST_CHECK(PSGTSignedBy(holder2, fixture.psgt));
}

BOOST_AUTO_TEST_SUITE_END()
