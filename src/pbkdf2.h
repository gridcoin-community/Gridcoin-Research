// Copyright (c) 2013 NovaCoin Developers

#ifndef BITCOIN_PBKDF2_H
#define BITCOIN_PBKDF2_H

#include <crypto/hmac_sha256.h>

#include <stdint.h>

void
PBKDF2_SHA256(const uint8_t * passwd, size_t passwdlen, const uint8_t * salt,
    size_t saltlen, uint64_t c, uint8_t * buf, size_t dkLen);

#endif // BITCOIN_PBKDF2_H
