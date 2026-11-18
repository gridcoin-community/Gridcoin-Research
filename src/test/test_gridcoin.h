// Copyright (c) 2015-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_TEST_GRIDCOIN_H
#define GRIDCOIN_TEST_TEST_GRIDCOIN_H

#include <random.h>
#include <wallet/wallet.h>

#include <string>

extern FastRandomContext g_insecure_rand_ctx;

static inline uint32_t InsecureRand32() { return g_insecure_rand_ctx.rand32(); }
static inline uint256 InsecureRand256() { return g_insecure_rand_ctx.rand256(); }
static inline uint64_t InsecureRandBits(int bits) { return g_insecure_rand_ctx.randbits(bits); }
static inline uint64_t InsecureRandRange(uint64_t range) { return g_insecure_rand_ctx.randrange(range); }
static inline bool InsecureRandBool() { return g_insecure_rand_ctx.randbool(); }
static inline std::vector<unsigned char> InsecureRandBytes(size_t len) { return g_insecure_rand_ctx.randbytes(len); }

namespace grc_test {
//! The seed for a RANDOM_CTX_SEED value: the value read as hex, or a fresh
//! random seed when it is unset, empty or reads as zero.
uint256 SeedFromEnv(const char* value);

//! The run's seed, read from RANDOM_CTX_SEED and printed when the tests start.
const uint256& TestRngSeed();

//! The seed g_insecure_rand_ctx restarts from at the start of the test case
//! or suite with this full name: a hash of the run's seed and the name.
uint256 CaseRngSeed(const std::string& full_name);
} // namespace grc_test

#endif
