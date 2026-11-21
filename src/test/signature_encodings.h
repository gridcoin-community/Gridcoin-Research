// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_SIGNATURE_ENCODINGS_H
#define GRIDCOIN_TEST_SIGNATURE_ENCODINGS_H

#include "arith_uint256.h"
#include "uint256.h"
#include "util/strencodings.h"

#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace grc_test {

//! The DER sequence of the two integers of a signature, with its length in the short form. Each integer is given as
//! the bytes of the DER integer.
inline std::vector<unsigned char> SignatureSequenceOf(const std::vector<unsigned char>& r,
                                                      const std::vector<unsigned char>& s)
{
    std::vector<unsigned char> sequence{0x30, static_cast<unsigned char>(4 + r.size() + s.size())};

    for (const std::vector<unsigned char>* integer : {&r, &s}) {
        sequence.push_back(0x02);
        sequence.push_back(static_cast<unsigned char>(integer->size()));
        sequence.insert(sequence.end(), integer->begin(), integer->end());
    }

    return sequence;
}

//! Byte strings made from \p signature that do not have the form CKey::Sign() writes. \p signature has to be as
//! CKey::Sign() wrote it. Each is given with a name for a test's messages.
inline std::vector<std::pair<std::string, std::vector<unsigned char>>> OtherEncodingsOf(
    const std::vector<unsigned char>& signature)
{
    // A signature as CKey::Sign() writes it is the byte 0x30, the length of what follows, and two integers. Each
    // integer is the byte 0x02, its length and its bytes.
    BOOST_REQUIRE(signature.size() >= 8);
    BOOST_REQUIRE(signature[0] == 0x30);
    BOOST_REQUIRE(static_cast<size_t>(signature[1]) == signature.size() - 2);
    BOOST_REQUIRE(signature[2] == 0x02);

    const size_t r_size = signature[3];

    BOOST_REQUIRE(signature.size() > 6 + r_size);
    BOOST_REQUIRE(signature[4 + r_size] == 0x02);

    const size_t s_size = signature[5 + r_size];

    BOOST_REQUIRE(signature.size() == 6 + r_size + s_size);

    const std::vector<unsigned char> r(signature.begin() + 4, signature.begin() + 4 + r_size);
    const std::vector<unsigned char> s(signature.begin() + 6 + r_size, signature.end());

    BOOST_REQUIRE(SignatureSequenceOf(r, s) == signature);

    std::vector<std::pair<std::string, std::vector<unsigned char>>> encodings;

    std::vector<unsigned char> with_a_byte_after(signature);
    with_a_byte_after.push_back(0x00);
    encodings.emplace_back("variation 1", with_a_byte_after);

    std::vector<unsigned char> with_a_long_form_length{0x30, 0x81};
    with_a_long_form_length.insert(with_a_long_form_length.end(), signature.begin() + 1, signature.end());
    encodings.emplace_back("variation 2", with_a_long_form_length);

    std::vector<unsigned char> with_another_length(signature);
    with_another_length[1] ^= 0x01;
    encodings.emplace_back("variation 3", with_another_length);

    std::vector<unsigned char> padded_r{0x00};
    padded_r.insert(padded_r.end(), r.begin(), r.end());
    encodings.emplace_back("variation 4", SignatureSequenceOf(padded_r, s));

    // Another second integer. Its first bit is required to be set below, and its DER integer then begins with a
    // zero byte.
    const arith_uint256 order =
        UintToArith256(uint256S("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"));

    std::vector<unsigned char> other_s = ParseHex(ArithToUint256(order - UintToArith256(uint256S(HexStr(s)))).GetHex());

    BOOST_REQUIRE(other_s.size() == 32);
    BOOST_REQUIRE((other_s[0] & 0x80) != 0);

    other_s.insert(other_s.begin(), 0x00);
    encodings.emplace_back("variation 5", SignatureSequenceOf(r, other_s));

    return encodings;
}

} // namespace grc_test

#endif // GRIDCOIN_TEST_SIGNATURE_ENCODINGS_H
