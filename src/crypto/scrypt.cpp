// Copyright (c) 2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
//
// RFC 7914 scrypt. The Salsa20/8 core below follows the public domain
// implementation lineage of Colin Percival (Tarsnap) as adapted by ArtForz
// and pooler; see src/scrypt.cpp for the original notice.

#include <crypto/scrypt.h>

#include <crypto/common.h>
#include <crypto/hmac_sha256.h>
#include <support/cleanse.h>

#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace {

//! B = Salsa20/8(B ^ Bx). Operates on 16 native 32-bit words.
void xor_salsa8(uint32_t B[16], const uint32_t Bx[16])
{
    uint32_t x00, x01, x02, x03, x04, x05, x06, x07, x08, x09, x10, x11, x12, x13, x14, x15;

    x00 = (B[0] ^= Bx[0]);
    x01 = (B[1] ^= Bx[1]);
    x02 = (B[2] ^= Bx[2]);
    x03 = (B[3] ^= Bx[3]);
    x04 = (B[4] ^= Bx[4]);
    x05 = (B[5] ^= Bx[5]);
    x06 = (B[6] ^= Bx[6]);
    x07 = (B[7] ^= Bx[7]);
    x08 = (B[8] ^= Bx[8]);
    x09 = (B[9] ^= Bx[9]);
    x10 = (B[10] ^= Bx[10]);
    x11 = (B[11] ^= Bx[11]);
    x12 = (B[12] ^= Bx[12]);
    x13 = (B[13] ^= Bx[13]);
    x14 = (B[14] ^= Bx[14]);
    x15 = (B[15] ^= Bx[15]);

    const auto R = [](uint32_t a, int b) -> uint32_t { return (a << b) | (a >> (32 - b)); };

    for (int i = 0; i < 8; i += 2) {
        /* Operate on columns. */
        x04 ^= R(x00 + x12, 7);  x09 ^= R(x05 + x01, 7);
        x14 ^= R(x10 + x06, 7);  x03 ^= R(x15 + x11, 7);

        x08 ^= R(x04 + x00, 9);  x13 ^= R(x09 + x05, 9);
        x02 ^= R(x14 + x10, 9);  x07 ^= R(x03 + x15, 9);

        x12 ^= R(x08 + x04, 13); x01 ^= R(x13 + x09, 13);
        x06 ^= R(x02 + x14, 13); x11 ^= R(x07 + x03, 13);

        x00 ^= R(x12 + x08, 18); x05 ^= R(x01 + x13, 18);
        x10 ^= R(x06 + x02, 18); x15 ^= R(x11 + x07, 18);

        /* Operate on rows. */
        x01 ^= R(x00 + x03, 7);  x06 ^= R(x05 + x04, 7);
        x11 ^= R(x10 + x09, 7);  x12 ^= R(x15 + x14, 7);

        x02 ^= R(x01 + x00, 9);  x07 ^= R(x06 + x05, 9);
        x08 ^= R(x11 + x10, 9);  x13 ^= R(x12 + x15, 9);

        x03 ^= R(x02 + x01, 13); x04 ^= R(x07 + x06, 13);
        x09 ^= R(x08 + x11, 13); x14 ^= R(x13 + x12, 13);

        x00 ^= R(x03 + x02, 18); x05 ^= R(x04 + x07, 18);
        x10 ^= R(x09 + x08, 18); x15 ^= R(x14 + x13, 18);
    }

    B[0] += x00;
    B[1] += x01;
    B[2] += x02;
    B[3] += x03;
    B[4] += x04;
    B[5] += x05;
    B[6] += x06;
    B[7] += x07;
    B[8] += x08;
    B[9] += x09;
    B[10] += x10;
    B[11] += x11;
    B[12] += x12;
    B[13] += x13;
    B[14] += x14;
    B[15] += x15;
}

//! PBKDF2-HMAC-SHA256 with an iteration count of one, the only count scrypt
//! uses (RFC 7914 sections 2 and 6).
void PBKDF2_SHA256_C1(Span<const std::byte> pass, Span<const std::byte> salt, Span<std::byte> out)
{
    const unsigned char* pass_data = reinterpret_cast<const unsigned char*>(pass.data());
    const unsigned char* salt_data = reinterpret_cast<const unsigned char*>(salt.data());

    unsigned char block[CHMAC_SHA256::OUTPUT_SIZE];
    uint32_t block_index = 1;

    size_t remaining = out.size();
    std::byte* dest = out.data();
    while (remaining > 0) {
        unsigned char index_be[4];
        WriteBE32(index_be, block_index);

        CHMAC_SHA256(pass_data, pass.size())
            .Write(salt_data, salt.size())
            .Write(index_be, sizeof(index_be))
            .Finalize(block);

        const size_t chunk = remaining < sizeof(block) ? remaining : sizeof(block);
        for (size_t i = 0; i < chunk; ++i) {
            dest[i] = static_cast<std::byte>(block[i]);
        }

        dest += chunk;
        remaining -= chunk;
        ++block_index;
    }

    memory_cleanse(block, sizeof(block));
}

//! scryptBlockMix (RFC 7914 section 4). B is 32 * r words; Y is scratch of the
//! same size.
void BlockMix(uint32_t* B, uint32_t* Y, unsigned int r)
{
    const unsigned int blocks = 2 * r; // 64-byte (16-word) sub-blocks

    // X starts as the last sub-block of B.
    uint32_t X[16];
    for (int i = 0; i < 16; ++i) {
        X[i] = B[(blocks - 1) * 16 + i];
    }

    // X = Salsa(X ^ B[i]); interleave even/odd sub-blocks into the output
    // order Y[0], Y[2], ..., Y[1], Y[3], ... directly.
    for (unsigned int i = 0; i < blocks; ++i) {
        xor_salsa8(X, &B[i * 16]);
        const unsigned int dest = (i / 2) + (i & 1) * r;
        for (int j = 0; j < 16; ++j) {
            Y[dest * 16 + j] = X[j];
        }
    }

    for (unsigned int i = 0; i < blocks * 16; ++i) {
        B[i] = Y[i];
    }

    memory_cleanse(X, sizeof(X));
}

//! scryptROMix (RFC 7914 section 5). B is 32 * r words; V is scratch of
//! N * 32 * r words; Y is scratch of 32 * r words.
void ROMix(uint32_t* B, uint32_t* V, uint32_t* Y, unsigned int N, unsigned int r)
{
    const unsigned int words = 32 * r;

    for (unsigned int i = 0; i < N; ++i) {
        for (unsigned int j = 0; j < words; ++j) {
            V[i * words + j] = B[j];
        }
        BlockMix(B, Y, r);
    }

    for (unsigned int i = 0; i < N; ++i) {
        // Integerify: the first 8 bytes of the last 64-byte sub-block as a
        // little-endian integer. The words are already little-endian decoded.
        const uint64_t idx = (uint64_t{B[words - 16]} | (uint64_t{B[words - 15]} << 32)) & (N - 1);
        for (unsigned int j = 0; j < words; ++j) {
            B[j] ^= V[idx * words + j];
        }
        BlockMix(B, Y, r);
    }
}

} // anonymous namespace

void ScryptRFC7914(Span<const std::byte> pass, Span<const std::byte> salt,
                   unsigned int N, unsigned int r, unsigned int p,
                   Span<std::byte> out)
{
    // Enforce the RFC 7914 section 2 parameter domain unconditionally rather
    // than with assert(): out-of-domain values would otherwise overflow the
    // size arithmetic below. r * p < 2^30 is the RFC's own constraint. The
    // scratch cap bounds the AGGREGATE allocation -- B (128 * r * p) plus V
    // (128 * r * N) plus the lane and Y buffers (128 * r each), i.e.
    // 128 * r * (N + p + 2) bytes -- using division so that no product in the
    // validation itself can wrap. N and p are bounded by the r * p and
    // power-of-two checks before they are summed.
    constexpr uint64_t MAX_SCRATCH_BYTES = uint64_t{1} << 32; // 4 GiB

    if (N <= 1 || (N & (N - 1)) != 0 || r == 0 || p == 0
        || uint64_t{r} * p >= (uint64_t{1} << 30)
        || uint64_t{128} * r > MAX_SCRATCH_BYTES / (uint64_t{N} + p + 2)
        || out.size() == 0) {
        throw std::invalid_argument("ScryptRFC7914: invalid parameters");
    }

    const size_t lane_bytes = 128 * static_cast<size_t>(r);
    const size_t lane_words = lane_bytes / 4;

    // B = PBKDF2-SHA256(pass, salt, c=1, p * 128 * r)
    std::vector<std::byte> B(p * lane_bytes);
    PBKDF2_SHA256_C1(pass, salt, MakeWritableByteSpan(B));

    std::vector<uint32_t> lane(lane_words);
    std::vector<uint32_t> V(static_cast<size_t>(N) * lane_words);
    std::vector<uint32_t> Y(lane_words);

    for (unsigned int i = 0; i < p; ++i) {
        std::byte* lane_start = B.data() + i * lane_bytes;

        // Decode the lane to native words (scrypt is little-endian).
        for (size_t j = 0; j < lane_words; ++j) {
            lane[j] = ReadLE32(reinterpret_cast<const unsigned char*>(lane_start) + 4 * j);
        }

        ROMix(lane.data(), V.data(), Y.data(), N, r);

        for (size_t j = 0; j < lane_words; ++j) {
            WriteLE32(reinterpret_cast<unsigned char*>(lane_start) + 4 * j, lane[j]);
        }
    }

    // out = PBKDF2-SHA256(pass, B, c=1, dkLen)
    PBKDF2_SHA256_C1(pass, MakeByteSpan(B), out);

    memory_cleanse(B.data(), B.size());
    memory_cleanse(lane.data(), lane.size() * sizeof(uint32_t));
    memory_cleanse(V.data(), V.size() * sizeof(uint32_t));
    memory_cleanse(Y.data(), Y.size() * sizeof(uint32_t));
}
