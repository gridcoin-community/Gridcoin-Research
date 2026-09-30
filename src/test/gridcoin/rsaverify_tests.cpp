// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/crypto/rsaverify.h"
#include "test/gridcoin/rsa_test_vectors.h"
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>
#include <string>
#include <vector>

using rsa_test_vectors::g_test_message;
using rsa_test_vectors::g_test_pem_pubkey;
using rsa_test_vectors::g_test_signature_hex;

BOOST_AUTO_TEST_SUITE(rsaverify_tests)

BOOST_AUTO_TEST_CASE(it_verifies_a_valid_rsa_sha512_signature)
{
    std::vector<uint8_t> message(g_test_message.begin(), g_test_message.end());
    std::vector<uint8_t> signature = ParseHex(g_test_signature_hex);

    BOOST_CHECK(GRC::VerifyRSASHA512(message, signature, g_test_pem_pubkey));
}

BOOST_AUTO_TEST_CASE(it_rejects_a_tampered_message)
{
    std::string tampered = g_test_message;
    tampered[0] = '2'; // Change account ID from "19100117" to "29100117"

    std::vector<uint8_t> message(tampered.begin(), tampered.end());
    std::vector<uint8_t> signature = ParseHex(g_test_signature_hex);

    BOOST_CHECK(!GRC::VerifyRSASHA512(message, signature, g_test_pem_pubkey));
}

BOOST_AUTO_TEST_CASE(it_rejects_a_tampered_signature)
{
    std::vector<uint8_t> message(g_test_message.begin(), g_test_message.end());
    std::vector<uint8_t> signature = ParseHex(g_test_signature_hex);

    // Flip a byte in the signature.
    if (!signature.empty()) {
        signature[0] ^= 0xFF;
    }

    BOOST_CHECK(!GRC::VerifyRSASHA512(message, signature, g_test_pem_pubkey));
}

BOOST_AUTO_TEST_CASE(it_rejects_an_empty_signature)
{
    std::vector<uint8_t> message(g_test_message.begin(), g_test_message.end());
    std::vector<uint8_t> empty_sig;

    BOOST_CHECK(!GRC::VerifyRSASHA512(message, empty_sig, g_test_pem_pubkey));
}

BOOST_AUTO_TEST_CASE(it_rejects_an_invalid_pem_key)
{
    std::vector<uint8_t> message(g_test_message.begin(), g_test_message.end());
    std::vector<uint8_t> signature = ParseHex(g_test_signature_hex);

    BOOST_CHECK(!GRC::VerifyRSASHA512(message, signature, "not a valid PEM key"));
}

BOOST_AUTO_TEST_CASE(it_rejects_an_empty_message)
{
    std::vector<uint8_t> empty_message;
    std::vector<uint8_t> signature = ParseHex(g_test_signature_hex);

    // Signature was computed for a non-empty message, so empty message should fail.
    BOOST_CHECK(!GRC::VerifyRSASHA512(empty_message, signature, g_test_pem_pubkey));
}

BOOST_AUTO_TEST_CASE(it_gets_the_same_der_for_the_same_key_in_different_pem_text)
{
    const std::vector<uint8_t> der = GRC::GetPublicKeyDER(g_test_pem_pubkey);

    BOOST_CHECK(!der.empty());
    BOOST_CHECK(der == GRC::GetPublicKeyDER(rsa_test_vectors::g_test_pem_pubkey_rewrapped));
    BOOST_CHECK(der != GRC::GetPublicKeyDER(rsa_test_vectors::g_test_pem_pubkey_2));
}

BOOST_AUTO_TEST_CASE(it_gets_no_der_for_an_invalid_pem_key)
{
    BOOST_CHECK(GRC::GetPublicKeyDER("not a valid PEM key").empty());
    BOOST_CHECK(GRC::GetPublicKeyDER("").empty());
}

BOOST_AUTO_TEST_SUITE_END()
