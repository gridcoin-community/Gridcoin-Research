// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pubkey.h>

#include <secp256k1.h>

namespace
{
/* Global secp256k1_context object used for verification. */
secp256k1_context* secp256k1_context_verify = nullptr;
} // namespace

/* static */ int ECCVerifyHandle::refcount = 0;

ECCVerifyHandle::ECCVerifyHandle()
{
    if (refcount == 0) {
        assert(secp256k1_context_verify == nullptr);
        secp256k1_context_verify = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
        assert(secp256k1_context_verify != nullptr);
    }
    refcount++;
}

ECCVerifyHandle::~ECCVerifyHandle()
{
    refcount--;
    if (refcount == 0) {
        assert(secp256k1_context_verify != nullptr);
        secp256k1_context_destroy(secp256k1_context_verify);
        secp256k1_context_verify = nullptr;
    }
}

const secp256k1_context* GetVerifyContext() {
    return secp256k1_context_verify;
}