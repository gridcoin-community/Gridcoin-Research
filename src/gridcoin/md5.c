/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <gridcoin/md5.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define CRYPTO_load_u32_le(data) (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24)
#define CRYPTO_store_u32_le(dst, src) (dst)[0] = (src & 0xFF); (dst)[1] = (src & 0xFF00) >> 8; (dst)[2] = (src & 0xFF0000) >> 16; (dst)[3] = (src & 0xFF000000) >> 24
#define CRYPTO_load_u32_be(data) (uint32_t)data[3] | ((uint32_t)data[2] << 8) | ((uint32_t)data[1] << 16) | ((uint32_t)data[0] << 24)
#define CRYPTO_store_u32_be(dst, src) (dst)[3] = (src & 0xFF); (dst)[2] = (src & 0xFF00) >> 8; (dst)[1] = (src & 0xFF0000) >> 16; (dst)[0] = (src & 0xFF000000) >> 24

static inline uint32_t CRYPTO_rotl_u32(uint32_t value, int shift) {
#if defined(_MSC_VER)
  return _rotl(value, shift);
#else
  return (value << shift) | (value >> ((-shift) & 31));
#endif
}

// This is a generic 32-bit "collector" for message digest algorithms. It
// collects input character stream into chunks of 32-bit values and invokes the
// block function that performs the actual hash calculations.
//
// To make use of this mechanism, the hash context should be defined with the
// following parameters.
//
//     typedef struct <name>_state_st {
//       uint32_t h[<chaining length> / sizeof(uint32_t)];
//       uint32_t Nl, Nh;
//       uint8_t data[<block size>];
//       unsigned num;
//       ...
//     } <NAME>_CTX;
//
// <chaining length> is the output length of the hash in bytes, before
// any truncation (e.g. 64 for SHA-224 and SHA-256, 128 for SHA-384 and
// SHA-512).
//
// |h| is the hash state and is updated by a function of type
// |crypto_md32_block_func|. |data| is the partial unprocessed block and has
// |num| bytes. |Nl| and |Nh| maintain the number of bits processed so far.

// A crypto_md32_block_func should incorporate |num_blocks| of input from |data|
// into |state|. It is assumed the caller has sized |state| and |data| for the
// hash function.
typedef void (*crypto_md32_block_func)(uint32_t *state, const uint8_t *data,
                                       size_t num_blocks);

// crypto_md32_update adds |len| bytes from |in| to the digest. |data| must be a
// buffer of length |block_size| with the first |*num| bytes containing a
// partial block. This function combines the partial block with |in| and
// incorporates any complete blocks into the digest state |h|. It then updates
// |data| and |*num| with the new partial block and updates |*Nh| and |*Nl| with
// the data consumed.
static inline void crypto_md32_update(crypto_md32_block_func block_func,
                                      uint32_t *h, uint8_t *data,
                                      size_t block_size, unsigned *num,
                                      uint32_t *Nh, uint32_t *Nl,
                                      const uint8_t *in, size_t len) {
  if (len == 0) {
    return;
  }

  uint32_t l = *Nl + (((uint32_t)len) << 3);
  if (l < *Nl) {
    // Handle carries.
    (*Nh)++;
  }
  *Nh += (uint32_t)(len >> 29);
  *Nl = l;

  size_t n = *num;
  if (n != 0) {
    if (len >= block_size || len + n >= block_size) {
      memcpy(data + n, in, block_size - n);
      block_func(h, data, 1);
      n = block_size - n;
      in += n;
      len -= n;
      *num = 0;
      // Keep |data| zeroed when unused.
      memset(data, 0, block_size);
    } else {
      memcpy(data + n, in, len);
      *num += (unsigned)len;
      return;
    }
  }

  n = len / block_size;
  if (n > 0) {
    block_func(h, in, n);
    n *= block_size;
    in += n;
    len -= n;
  }

  if (len != 0) {
    *num = (unsigned)len;
    memcpy(data, in, len);
  }
}

// crypto_md32_final incorporates the partial block and trailing length into the
// digest state |h|. The trailing length is encoded in little-endian if
// |is_big_endian| is zero and big-endian otherwise. |data| must be a buffer of
// length |block_size| with the first |*num| bytes containing a partial block.
// |Nh| and |Nl| contain the total number of bits processed. On return, this
// function clears the partial block in |data| and
// |*num|.
//
// This function does not serialize |h| into a final digest. This is the
// responsibility of the caller.
static inline void crypto_md32_final(crypto_md32_block_func block_func,
                                     uint32_t *h, uint8_t *data,
                                     size_t block_size, unsigned *num,
                                     uint32_t Nh, uint32_t Nl,
                                     int is_big_endian) {
  // |data| always has room for at least one byte. A full block would have
  // been consumed.
  size_t n = *num;
  assert(n < block_size);
  data[n] = 0x80;
  n++;

  // Fill the block with zeros if there isn't room for a 64-bit length.
  if (n > block_size - 8) {
    memset(data + n, 0, block_size - n);
    n = 0;
    block_func(h, data, 1);
  }
  memset(data + n, 0, block_size - 8 - n);

  // Append a 64-bit length to the block and process it.
  if (is_big_endian) {
    CRYPTO_store_u32_be(data + block_size - 8, Nh);
    CRYPTO_store_u32_be(data + block_size - 4, Nl);
  } else {
    CRYPTO_store_u32_le(data + block_size - 8, Nl);
    CRYPTO_store_u32_le(data + block_size - 4, Nh);
  }
  block_func(h, data, 1);
  *num = 0;
  memset(data, 0, block_size);
}

int MD5_Init(MD5_CTX *md5) {
  memset(md5, 0, sizeof(MD5_CTX));
  md5->h[0] = 0x67452301UL;
  md5->h[1] = 0xefcdab89UL;
  md5->h[2] = 0x98badcfeUL;
  md5->h[3] = 0x10325476UL;
  return 1;
}

static void md5_block_data_order(uint32_t *state, const uint8_t *data,
                                 size_t num);

void MD5_Transform(MD5_CTX *c, const uint8_t data[MD5_CBLOCK]) {
  md5_block_data_order(c->h, data, 1);
}

int MD5_Update(MD5_CTX *c, const void *data, size_t len) {
  crypto_md32_update(&md5_block_data_order, c->h, c->data, MD5_CBLOCK, &c->num,
                     &c->Nh, &c->Nl, data, len);
  return 1;
}

int MD5_Final(uint8_t out[MD5_DIGEST_LENGTH], MD5_CTX *c) {
  crypto_md32_final(&md5_block_data_order, c->h, c->data, MD5_CBLOCK, &c->num,
                    c->Nh, c->Nl, /*is_big_endian=*/0);

  CRYPTO_store_u32_le(out, c->h[0]);
  CRYPTO_store_u32_le(out + 4, c->h[1]);
  CRYPTO_store_u32_le(out + 8, c->h[2]);
  CRYPTO_store_u32_le(out + 12, c->h[3]);
  return 1;
}

uint8_t *MD5(const uint8_t *data, size_t len, uint8_t out[MD5_DIGEST_LENGTH]) {
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, data, len);
  MD5_Final(out, &ctx);

  return out;
}

// As pointed out by Wei Dai <weidai@eskimo.com>, the above can be
// simplified to the code below.  Wei attributes these optimizations
// to Peter Gutmann's SHS code, and he attributes it to Rich Schroeppel.
#define F(b, c, d) ((((c) ^ (d)) & (b)) ^ (d))
#define G(b, c, d) ((((b) ^ (c)) & (d)) ^ (c))
#define H(b, c, d) ((b) ^ (c) ^ (d))
#define I(b, c, d) (((~(d)) | (b)) ^ (c))

#define R0(a, b, c, d, k, s, t)            \
  do {                                     \
    (a) += ((k) + (t) + F((b), (c), (d))); \
    (a) = CRYPTO_rotl_u32(a, s);           \
    (a) += (b);                            \
  } while (0)

#define R1(a, b, c, d, k, s, t)            \
  do {                                     \
    (a) += ((k) + (t) + G((b), (c), (d))); \
    (a) = CRYPTO_rotl_u32(a, s);           \
    (a) += (b);                            \
  } while (0)

#define R2(a, b, c, d, k, s, t)            \
  do {                                     \
    (a) += ((k) + (t) + H((b), (c), (d))); \
    (a) = CRYPTO_rotl_u32(a, s);           \
    (a) += (b);                            \
  } while (0)

#define R3(a, b, c, d, k, s, t)            \
  do {                                     \
    (a) += ((k) + (t) + I((b), (c), (d))); \
    (a) = CRYPTO_rotl_u32(a, s);           \
    (a) += (b);                            \
  } while (0)

#ifndef MD5_ASM
#ifdef X
#undef X
#endif


static void md5_block_data_order(uint32_t *state, const uint8_t *data,
                                 size_t num) {
  uint32_t A, B, C, D;
  uint32_t XX0, XX1, XX2, XX3, XX4, XX5, XX6, XX7, XX8, XX9, XX10, XX11, XX12,
      XX13, XX14, XX15;
#define X(i) XX##i

  A = state[0];
  B = state[1];
  C = state[2];
  D = state[3];

  for (; num--;) {
    X(0) = CRYPTO_load_u32_le(data);
    data += 4;
    X(1) = CRYPTO_load_u32_le(data);
    data += 4;
    // Round 0
    R0(A, B, C, D, X(0), 7, 0xd76aa478L);
    X(2) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(D, A, B, C, X(1), 12, 0xe8c7b756L);
    X(3) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(C, D, A, B, X(2), 17, 0x242070dbL);
    X(4) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(B, C, D, A, X(3), 22, 0xc1bdceeeL);
    X(5) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(A, B, C, D, X(4), 7, 0xf57c0fafL);
    X(6) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(D, A, B, C, X(5), 12, 0x4787c62aL);
    X(7) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(C, D, A, B, X(6), 17, 0xa8304613L);
    X(8) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(B, C, D, A, X(7), 22, 0xfd469501L);
    X(9) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(A, B, C, D, X(8), 7, 0x698098d8L);
    X(10) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(D, A, B, C, X(9), 12, 0x8b44f7afL);
    X(11) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(C, D, A, B, X(10), 17, 0xffff5bb1L);
    X(12) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(B, C, D, A, X(11), 22, 0x895cd7beL);
    X(13) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(A, B, C, D, X(12), 7, 0x6b901122L);
    X(14) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(D, A, B, C, X(13), 12, 0xfd987193L);
    X(15) = CRYPTO_load_u32_le(data);
    data += 4;
    R0(C, D, A, B, X(14), 17, 0xa679438eL);
    R0(B, C, D, A, X(15), 22, 0x49b40821L);
    // Round 1
    R1(A, B, C, D, X(1), 5, 0xf61e2562L);
    R1(D, A, B, C, X(6), 9, 0xc040b340L);
    R1(C, D, A, B, X(11), 14, 0x265e5a51L);
    R1(B, C, D, A, X(0), 20, 0xe9b6c7aaL);
    R1(A, B, C, D, X(5), 5, 0xd62f105dL);
    R1(D, A, B, C, X(10), 9, 0x02441453L);
    R1(C, D, A, B, X(15), 14, 0xd8a1e681L);
    R1(B, C, D, A, X(4), 20, 0xe7d3fbc8L);
    R1(A, B, C, D, X(9), 5, 0x21e1cde6L);
    R1(D, A, B, C, X(14), 9, 0xc33707d6L);
    R1(C, D, A, B, X(3), 14, 0xf4d50d87L);
    R1(B, C, D, A, X(8), 20, 0x455a14edL);
    R1(A, B, C, D, X(13), 5, 0xa9e3e905L);
    R1(D, A, B, C, X(2), 9, 0xfcefa3f8L);
    R1(C, D, A, B, X(7), 14, 0x676f02d9L);
    R1(B, C, D, A, X(12), 20, 0x8d2a4c8aL);
    // Round 2
    R2(A, B, C, D, X(5), 4, 0xfffa3942L);
    R2(D, A, B, C, X(8), 11, 0x8771f681L);
    R2(C, D, A, B, X(11), 16, 0x6d9d6122L);
    R2(B, C, D, A, X(14), 23, 0xfde5380cL);
    R2(A, B, C, D, X(1), 4, 0xa4beea44L);
    R2(D, A, B, C, X(4), 11, 0x4bdecfa9L);
    R2(C, D, A, B, X(7), 16, 0xf6bb4b60L);
    R2(B, C, D, A, X(10), 23, 0xbebfbc70L);
    R2(A, B, C, D, X(13), 4, 0x289b7ec6L);
    R2(D, A, B, C, X(0), 11, 0xeaa127faL);
    R2(C, D, A, B, X(3), 16, 0xd4ef3085L);
    R2(B, C, D, A, X(6), 23, 0x04881d05L);
    R2(A, B, C, D, X(9), 4, 0xd9d4d039L);
    R2(D, A, B, C, X(12), 11, 0xe6db99e5L);
    R2(C, D, A, B, X(15), 16, 0x1fa27cf8L);
    R2(B, C, D, A, X(2), 23, 0xc4ac5665L);
    // Round 3
    R3(A, B, C, D, X(0), 6, 0xf4292244L);
    R3(D, A, B, C, X(7), 10, 0x432aff97L);
    R3(C, D, A, B, X(14), 15, 0xab9423a7L);
    R3(B, C, D, A, X(5), 21, 0xfc93a039L);
    R3(A, B, C, D, X(12), 6, 0x655b59c3L);
    R3(D, A, B, C, X(3), 10, 0x8f0ccc92L);
    R3(C, D, A, B, X(10), 15, 0xffeff47dL);
    R3(B, C, D, A, X(1), 21, 0x85845dd1L);
    R3(A, B, C, D, X(8), 6, 0x6fa87e4fL);
    R3(D, A, B, C, X(15), 10, 0xfe2ce6e0L);
    R3(C, D, A, B, X(6), 15, 0xa3014314L);
    R3(B, C, D, A, X(13), 21, 0x4e0811a1L);
    R3(A, B, C, D, X(4), 6, 0xf7537e82L);
    R3(D, A, B, C, X(11), 10, 0xbd3af235L);
    R3(C, D, A, B, X(2), 15, 0x2ad7d2bbL);
    R3(B, C, D, A, X(9), 21, 0xeb86d391L);

    A = state[0] += A;
    B = state[1] += B;
    C = state[2] += C;
    D = state[3] += D;
  }
}
#undef X
#endif

#undef F
#undef G
#undef H
#undef I
#undef R0
#undef R1
#undef R2
#undef R3
#undef CRYPTO_load_u32_le
#undef CRYPTO_store_u32_le
#undef CRYPTO_load_u32_be
#undef CRYPTO_store_u32_be
