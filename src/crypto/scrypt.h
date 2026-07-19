// Copyright (c) 2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CRYPTO_SCRYPT_H
#define GRIDCOIN_CRYPTO_SCRYPT_H

#include <span.h>

#include <cstddef>

/**
 * The scrypt password-based key derivation function as specified by RFC 7914,
 * with parameterizable cost factors.
 *
 * This is distinct from the legacy fixed-parameter (N=1024, r=1, p=1) scrypt
 * hasher in src/scrypt.{h,cpp}, which dates from the proof-of-work era and is
 * retained for the wallet crypter. New uses of scrypt as a KDF (e.g. the seed
 * phrase scheme) should use this implementation: it is endian-correct on all
 * platforms and supports the memory-hard parameter ranges recommended for
 * password hashing.
 *
 * \param pass   The password.
 * \param salt   The salt.
 * \param N      CPU/memory cost. Must be a power of two, greater than 1.
 * \param r      Block size factor. Memory usage is roughly 128 * r * N bytes.
 * \param p      Parallelization factor (lanes are computed sequentially).
 * \param out    Receives the derived key; any length > 0 is permitted.
 *
 * \throws std::invalid_argument if the parameters are outside the RFC 7914
 * domain (including r * p >= 2^30) or would require more than 4 GiB of
 * scratch memory. Validation is unconditional, not assert()-based.
 */
void ScryptRFC7914(Span<const std::byte> pass, Span<const std::byte> salt,
                   unsigned int N, unsigned int r, unsigned int p,
                   Span<std::byte> out);

#endif // GRIDCOIN_CRYPTO_SCRYPT_H
