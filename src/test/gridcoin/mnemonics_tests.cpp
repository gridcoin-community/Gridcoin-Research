// Copyright (c) 2024-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <gridcoin/mnemonics.h>
#include <key.h>
#include <util/strencodings.h>

#include <test/test_gridcoin.h>

#include <algorithm>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace GRC::Mnemonics;

namespace {

SecureString ToSecure(const std::string& str)
{
    return SecureString(str.begin(), str.end());
}

std::vector<std::byte> BlobWithWordIndex(unsigned word, unsigned index)
{
    // Build a 33-byte blob whose 11-bit group at position `word` equals
    // `index` and whose other groups are zero.
    std::vector<std::byte> blob(ENCIPHERED_LENGTH, std::byte{0});
    for (unsigned b = 0; b < WORDLIST_BIT_LENGTH; ++b) {
        const unsigned bit = word * WORDLIST_BIT_LENGTH + b;
        const unsigned value = (index >> (WORDLIST_BIT_LENGTH - 1 - b)) & 1u;
        blob[bit / 8] |= std::byte(value << (7 - bit % 8));
    }
    return blob;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(mnemonics_tests)

BOOST_AUTO_TEST_CASE(encode_produces_known_words_at_wordlist_boundaries)
{
    // All-zero blob: every 11-bit group is 0 -> the first wordlist entry.
    const std::vector<std::byte> zeroes(ENCIPHERED_LENGTH, std::byte{0});
    SecureString phrase = EncodeSeedPhrase(zeroes);
    BOOST_CHECK_EQUAL(std::string(phrase.begin(), phrase.end()),
                      "abandon abandon abandon abandon abandon abandon abandon abandon "
                      "abandon abandon abandon abandon abandon abandon abandon abandon "
                      "abandon abandon abandon abandon abandon abandon abandon abandon");

    // All-ones blob: every group is 2047 -> the last wordlist entry.
    const std::vector<std::byte> ones(ENCIPHERED_LENGTH, std::byte{0xFF});
    phrase = EncodeSeedPhrase(ones);
    BOOST_CHECK_EQUAL(std::string(phrase.begin(), phrase.end()),
                      "zoo zoo zoo zoo zoo zoo zoo zoo "
                      "zoo zoo zoo zoo zoo zoo zoo zoo "
                      "zoo zoo zoo zoo zoo zoo zoo zoo");

    // Spot checks at the ends of the wordlist.
    SecureString one_word = EncodeSeedPhrase(BlobWithWordIndex(0, 1));
    BOOST_CHECK_EQUAL(std::string(one_word.begin(), one_word.end()).substr(0, 8), "ability ");
    one_word = EncodeSeedPhrase(BlobWithWordIndex(0, 2046));
    BOOST_CHECK_EQUAL(std::string(one_word.begin(), one_word.end()).substr(0, 5), "zone ");
}

BOOST_AUTO_TEST_CASE(encode_decode_roundtrip_covers_every_word_index)
{
    // Exercise all 2048 wordlist entries through encode -> decode, at varying
    // word positions.
    for (unsigned index = 0; index < 2048; ++index) {
        const std::vector<std::byte> blob = BlobWithWordIndex(index % WORD_COUNT, index);
        std::vector<std::byte> decoded(ENCIPHERED_LENGTH);
        BOOST_REQUIRE(DecodeSeedPhrase(EncodeSeedPhrase(blob), decoded));
        BOOST_REQUIRE(blob == decoded);
    }
}

BOOST_AUTO_TEST_CASE(encode_decode_roundtrip_random_blobs)
{
    for (int i = 0; i < 100; ++i) {
        const std::vector<unsigned char> random = InsecureRandBytes(ENCIPHERED_LENGTH);
        const std::vector<std::byte> blob(reinterpret_cast<const std::byte*>(random.data()),
                                          reinterpret_cast<const std::byte*>(random.data()) + random.size());

        std::vector<std::byte> decoded(ENCIPHERED_LENGTH);
        BOOST_REQUIRE(DecodeSeedPhrase(EncodeSeedPhrase(blob), decoded));
        BOOST_REQUIRE(blob == decoded);
    }
}

BOOST_AUTO_TEST_CASE(decode_rejects_malformed_phrases)
{
    std::vector<std::byte> blob(ENCIPHERED_LENGTH);

    // Empty phrase.
    BOOST_CHECK(!DecodeSeedPhrase(SecureString{}, blob));

    // A word not in the wordlist.
    const std::vector<std::byte> zeroes(ENCIPHERED_LENGTH, std::byte{0});
    SecureString phrase = EncodeSeedPhrase(zeroes);
    SecureString corrupted = phrase;
    corrupted.replace(0, 7, "zzzzzzz");
    BOOST_CHECK(!DecodeSeedPhrase(corrupted, blob));

    // Too few words (drop the last word).
    SecureString truncated(phrase.begin(), phrase.begin() + phrase.rfind(' '));
    BOOST_CHECK(!DecodeSeedPhrase(truncated, blob));

    // Too many words.
    SecureString extended = phrase;
    extended += " abandon";
    BOOST_CHECK(!DecodeSeedPhrase(extended, blob));

    // Garbage.
    BOOST_CHECK(!DecodeSeedPhrase(ToSecure("not a seed phrase"), blob));

    // Empty word segments: leading, doubled and lone separators must be
    // rejected without scanning an empty candidate.
    SecureString leading = phrase;
    leading.insert(leading.begin(), ' ');
    BOOST_CHECK(!DecodeSeedPhrase(leading, blob));

    SecureString doubled = phrase;
    doubled.insert(doubled.find(' '), 1, ' ');
    BOOST_CHECK(!DecodeSeedPhrase(doubled, blob));

    BOOST_CHECK(!DecodeSeedPhrase(ToSecure(" "), blob));

    // Oversized input is rejected up front: no valid phrase exceeds
    // WORD_COUNT * (LONGEST_WORD_LENGTH + 1) characters.
    SecureString oversized(WORD_COUNT * 9 + 1, 'a');
    BOOST_CHECK(!DecodeSeedPhrase(oversized, blob));

    // A single trailing separator marks the end of the last word and is
    // tolerated.
    SecureString trailing = phrase;
    trailing += ' ';
    BOOST_CHECK(DecodeSeedPhrase(trailing, blob));
}

BOOST_AUTO_TEST_CASE(build_and_parse_fixed_vector)
{
    // Fixed vector pinning the complete scheme: scrypt KDF, AEAD, truncated
    // tag, bit packing and wordlist. Any change to any layer changes the
    // phrase and must bump the outer version instead.
    const std::vector<unsigned char> entropy = ParseHex("000102030405060708090a0b0c0d0e0f");
    const std::vector<unsigned char> salt = ParseHex("aabbccddee");
    const SecureString password = ToSecure("gridcoin");
    const uint16_t birthday_days = 4242;

    const SecureString phrase = BuildSeedPhrase(MakeByteSpan(entropy), birthday_days,
                                                MakeByteSpan(salt), password);

    BOOST_CHECK_EQUAL(std::string(phrase.begin(), phrase.end()),
                      "absent fiction veteran rookie this display improve clock "
                      "gift same dove apple purpose usual engine survey "
                      "tenant farm basket aim leisure joke hurry help");

    CKey key;
    int64_t birthday = 0;
    BOOST_REQUIRE(ParseSeedPhrase(phrase, password, key, &birthday));
    BOOST_CHECK(key.IsValid());
    // SHA256 of the entropy (inner version 0), verified against an
    // independent implementation.
    BOOST_CHECK_EQUAL(HexStr(Span{key.begin(), key.size()}),
                      "be45cb2605bf36bebde684841a28f0fd43c69850a3dce5fedba69928ee3a8991");
    BOOST_CHECK_EQUAL(birthday, BIRTHDAY_EPOCH + int64_t{birthday_days} * 24 * 60 * 60);

    // The same parts with a different password produce a different phrase,
    // and the original phrase fails to parse under the wrong password.
    const SecureString other = BuildSeedPhrase(MakeByteSpan(entropy), birthday_days,
                                               MakeByteSpan(salt), ToSecure("Gridcoin"));
    BOOST_CHECK(phrase != other);
    BOOST_CHECK(!ParseSeedPhrase(phrase, ToSecure("Gridcoin"), key));
    BOOST_CHECK(!ParseSeedPhrase(phrase, SecureString{}, key));
}

BOOST_AUTO_TEST_CASE(generate_and_parse_roundtrip)
{
    for (const std::string& password_str : {std::string{}, std::string{"correct horse"}}) {
        const SecureString password = ToSecure(password_str);

        CKey generated;
        const SecureString phrase = GenerateSeedPhrase(password, generated);
        BOOST_REQUIRE(generated.IsValid());

        // 24 words.
        const std::string str(phrase.begin(), phrase.end());
        BOOST_CHECK_EQUAL(std::count(str.begin(), str.end(), ' '), WORD_COUNT - 1);

        CKey parsed;
        int64_t birthday = 0;
        BOOST_REQUIRE(ParseSeedPhrase(phrase, password, parsed, &birthday));
        BOOST_CHECK(parsed.IsValid());
        BOOST_CHECK(generated == parsed);

        // The birthday is now, at day resolution.
        BOOST_CHECK(birthday >= BIRTHDAY_EPOCH);
        BOOST_CHECK(birthday <= GetTime());
        BOOST_CHECK(GetTime() - birthday <= 2 * 24 * 60 * 60);

        // Wrong password fails to authenticate.
        BOOST_CHECK(!ParseSeedPhrase(phrase, ToSecure("wrong"), parsed));

        // A corrupted word fails: swap the first word for a different valid word.
        SecureString corrupted = phrase;
        const SecureString::size_type first_space = corrupted.find(' ');
        corrupted.replace(0, first_space, str.compare(0, first_space, "zoo") == 0 ? "zebra" : "zoo");
        BOOST_CHECK(!ParseSeedPhrase(corrupted, password, parsed));
    }
}

BOOST_AUTO_TEST_SUITE_END()
