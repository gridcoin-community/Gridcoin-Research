// Copyright (c) 2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_MNEMONICS_H
#define GRIDCOIN_MNEMONICS_H

#include <span.h>
#include <support/allocators/secure.h>

namespace GRC {
namespace Mnemonics {

// Wordlist has 2048 words = 2 ^ 11
static constexpr unsigned WORDLIST_BIT_LENGTH = 11;

// plaintext: 1 byte inner version || 2 byte timestamp || 16 bytes of entropy = 19 bytes
static constexpr unsigned PLAINTEXT_LENGTH = 19;

// enciphered: 1 byte outer version || 19 bytes ciphertext || 16 bytes tag || 8 bytes salt / nonce = 44 bytes = 32 words
static constexpr unsigned ENCIPHERED_LENGTH = 44;
static constexpr unsigned WORD_COUNT = 32;

bool DecodeSeedPhrase(const SecureString& seed_phrase, Span<std::byte> data_out);
SecureString EncodeSeedPhrase(Span<const std::byte> data_in);

} // namespace Mnemonics
} // namespace GRC

#endif // GRIDCOIN_MNEMONICS_H
