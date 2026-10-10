// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Known answers for the legacy scrypt in src/scrypt.cpp (N=1024, r=1, p=1). It is
// a different implementation from crypto/scrypt.h, which crypto_tests covers with
// the RFC 7914 vectors. scrypt_salted_multiround_hash derives wallet encryption
// keys (crypter.cpp), so its output must never change. The expected values were
// produced by the implementation as it stood before its scratchpads stopped being
// thread_local.

#include <scrypt.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>
#include <string>

BOOST_AUTO_TEST_SUITE(scrypt_tests)

namespace {
const std::string kPass = "password";
const std::array<unsigned char, 8> kSalt = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
} // namespace

BOOST_AUTO_TEST_CASE(scrypt_hash_known_answer)
{
    const std::string input = "abc";
    BOOST_CHECK_EQUAL(scrypt_hash(input.data(), input.size()).GetHex(),
                      "1965d70dd8e5ddc425725e7695430ec845f509459dc4edd299cda8b7c3c152e6");
}

BOOST_AUTO_TEST_CASE(scrypt_blockhash_known_answer)
{
    std::array<unsigned char, 80> header;
    for (size_t i = 0; i < header.size(); ++i) header[i] = static_cast<unsigned char>(i);
    BOOST_CHECK_EQUAL(scrypt_blockhash(header.data()).GetHex(),
                      "d2a91baa45263cfd16c4fef0fb0776382d0e011ec70530496ef91d801a0a54bc");
}

BOOST_AUTO_TEST_CASE(scrypt_salted_hash_known_answer)
{
    BOOST_CHECK_EQUAL(scrypt_salted_hash(kPass.data(), kPass.size(), kSalt.data(), kSalt.size()).GetHex(),
                      "12ad02b3ff8e738de4a5c1bf219a21327e43a1a85f876e5c767ddfaf85bb85c5");
}

BOOST_AUTO_TEST_CASE(scrypt_salted_multiround_hash_known_answer)
{
    BOOST_CHECK_EQUAL(scrypt_salted_multiround_hash(kPass.data(), kPass.size(), kSalt.data(), kSalt.size(), 3).GetHex(),
                      "ce3733665ffb95b4d53d629f4ff185a1d6d628a0a363a29444a87f5c287619ad");
}

BOOST_AUTO_TEST_CASE(scrypt_salted_multiround_hash_is_chained_salted_hashes)
{
    // One round is the salted hash; each further round re-salts with the previous result.
    const uint256 one = scrypt_salted_hash(kPass.data(), kPass.size(), kSalt.data(), kSalt.size());
    BOOST_CHECK_EQUAL(scrypt_salted_multiround_hash(kPass.data(), kPass.size(), kSalt.data(), kSalt.size(), 1).GetHex(), one.GetHex());

    uint256 chained = one;
    for (int round = 1; round < 3; ++round) {
        chained = scrypt_salted_hash(kPass.data(), kPass.size(), chained.begin(), 32);
    }
    BOOST_CHECK_EQUAL(scrypt_salted_multiround_hash(kPass.data(), kPass.size(), kSalt.data(), kSalt.size(), 3).GetHex(), chained.GetHex());
}

BOOST_AUTO_TEST_SUITE_END()
