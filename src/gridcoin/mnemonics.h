// Copyright (c) 2024-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_MNEMONICS_H
#define GRIDCOIN_MNEMONICS_H

#include <key.h>
#include <span.h>
#include <support/allocators/secure.h>

#include <cstdint>

namespace GRC {
namespace Mnemonics {

//! \brief Gridcoin's versioned seed phrase scheme.
//!
//! A seed phrase is 24 words from the BIP39 English wordlist encoding a
//! 33-byte blob (24 * 11 bits = 264 bits, with no padding):
//!
//!   outer version (1) || salt (5) || ciphertext (19) || tag (8)
//!
//! The ciphertext is the ChaCha20Poly1305 (RFC 8439) encryption -- with the
//! tag truncated to 8 bytes -- of:
//!
//!   inner version (1) || wallet birthday in days, LE (2) || entropy (16)
//!
//! under a key derived from the user's password with scrypt (RFC 7914,
//! N=32768/r=8/p=1), a zero nonce (the key is used exactly once per salt),
//! and the outer version plus salt as additional authenticated data.
//!
//! The outer version fixes the decoding format, wordlist, KDF and cipher; the
//! inner version fixes how keys are derived from the entropy. Both are 0.
//! Unlike BIP39, a wrong password fails authentication explicitly rather than
//! silently deriving a different wallet.

//! \brief Number of bits encoded per word; the wordlist has 2^11 = 2048 words.
constexpr unsigned WORDLIST_BIT_LENGTH = 11;

//! \brief Length of the entropy the phrase protects.
constexpr unsigned ENTROPY_LENGTH = 16;

//! \brief plaintext: 1 byte inner version || 2 byte birthday || 16 byte entropy.
constexpr unsigned PLAINTEXT_LENGTH = 1 + 2 + ENTROPY_LENGTH;

//! \brief Length of the KDF salt, which doubles as the AEAD's authenticated data.
constexpr unsigned SALT_LENGTH = 5;

//! \brief Length of the (truncated) Poly1305 authentication tag.
constexpr unsigned TAG_LENGTH = 8;

//! \brief enciphered: outer version || salt || ciphertext || tag = 33 bytes.
constexpr unsigned ENCIPHERED_LENGTH = 1 + SALT_LENGTH + PLAINTEXT_LENGTH + TAG_LENGTH;

//! \brief 33 bytes = 264 bits = exactly 24 words.
constexpr unsigned WORD_COUNT = ENCIPHERED_LENGTH * 8 / WORDLIST_BIT_LENGTH;
static_assert(WORD_COUNT * WORDLIST_BIT_LENGTH == ENCIPHERED_LENGTH * 8,
              "the enciphered blob must pack into whole words with no padding");

//! \brief Epoch for the 16-bit wallet birthday (in days): the mainnet genesis
//! block time. 16 bits of days covers about 179 years.
constexpr int64_t BIRTHDAY_EPOCH = 1413149999;

//! \brief scrypt cost parameters for the password KDF under outer version 0
//! (the aezeed parameters; roughly 32 MiB of scratch memory).
constexpr unsigned SCRYPT_N = 32768;
constexpr unsigned SCRYPT_R = 8;
constexpr unsigned SCRYPT_P = 1;

//! \brief Convert a seed phrase to the 33-byte enciphered blob.
//!
//! \param seed_phrase Words separated by single spaces. A single trailing
//!                    separator is tolerated; empty segments (leading or
//!                    doubled separators) are rejected.
//! \param data_out    Receives the blob; must be ENCIPHERED_LENGTH bytes.
//!
//! \return False if any word is not in the wordlist or the phrase does not
//! contain exactly WORD_COUNT words.
bool DecodeSeedPhrase(const SecureString& seed_phrase, Span<std::byte> data_out);

//! \brief Convert a 33-byte enciphered blob to a seed phrase.
//!
//! \param data_in The blob; must be ENCIPHERED_LENGTH bytes.
SecureString EncodeSeedPhrase(Span<const std::byte> data_in);

//! \brief Deterministically construct a seed phrase from its parts. Exposed
//! for tests and fixed vectors; use GenerateSeedPhrase() to create one.
//!
//! \warning The AEAD nonce is fixed at zero on the premise that a
//! password+salt pair keys exactly one encryption. Reusing a salt with the
//! same password for different plaintexts reuses the keystream; callers must
//! supply a fresh random salt per phrase, as GenerateSeedPhrase() does.
//!
//! \param entropy       ENTROPY_LENGTH bytes of entropy.
//! \param birthday_days Days since BIRTHDAY_EPOCH at wallet creation.
//! \param salt          SALT_LENGTH bytes of KDF salt.
//! \param password      Password protecting the phrase. May be empty.
SecureString BuildSeedPhrase(Span<const std::byte> entropy, uint16_t birthday_days,
                             Span<const std::byte> salt, const SecureString& password);

//! \brief Generate a new random seed phrase and the key it protects.
//!
//! \param password Password protecting the phrase. May be empty.
//! \param key_out  Receives the derived key (SHA256 of the entropy under
//!                 inner version 0), intended as the wallet's HD master seed.
SecureString GenerateSeedPhrase(const SecureString& password, CKey& key_out);

//! \brief Recover the key protected by a seed phrase.
//!
//! \param seed_phrase  The 24-word phrase.
//! \param password     The password it was generated with.
//! \param key_out      Receives the derived key on success.
//! \param birthday_out If non-null, receives the wallet birthday as a Unix
//!                     timestamp (day resolution) for bounding rescans.
//!
//! \return False if the phrase is malformed, the password is wrong (the tag
//! fails to authenticate), or a version byte is unknown.
bool ParseSeedPhrase(const SecureString& seed_phrase, const SecureString& password,
                     CKey& key_out, int64_t* birthday_out = nullptr);

} // namespace Mnemonics
} // namespace GRC

#endif // GRIDCOIN_MNEMONICS_H
