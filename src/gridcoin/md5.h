// Copyright (c) 2023 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_MD5_H
#define GRIDCOIN_MD5_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define MD5_CBLOCK 64
#define MD5_DIGEST_LENGTH 16

typedef struct MD5_CTX {
  uint32_t h[4];
  uint32_t Nl, Nh;
  uint8_t data[MD5_CBLOCK];
  unsigned num;
} MD5_CTX;

uint8_t *MD5(const uint8_t *data, size_t len, uint8_t out[MD5_DIGEST_LENGTH]);

#if defined(__cplusplus)
}
#endif

#endif // GRIDCOIN_MD5_H
