// Copyright (c) 2015-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_TEST_GRIDCOIN_H
#define GRIDCOIN_TEST_TEST_GRIDCOIN_H

#include <main.h>
#include <random.h>
#include <wallet/wallet.h>

extern FastRandomContext g_insecure_rand_ctx;

static inline uint32_t InsecureRand32() { return g_insecure_rand_ctx.rand32(); }
static inline uint256 InsecureRand256() { return g_insecure_rand_ctx.rand256(); }
static inline uint64_t InsecureRandBits(int bits) { return g_insecure_rand_ctx.randbits(bits); }
static inline uint64_t InsecureRandRange(uint64_t range) { return g_insecure_rand_ctx.randrange(range); }
static inline bool InsecureRandBool() { return g_insecure_rand_ctx.randbool(); }
static inline std::vector<unsigned char> InsecureRandBytes(size_t len) { return g_insecure_rand_ctx.randbytes(len); }

// Enable BOOST_CHECK_EQUAL for enum class types
namespace std {
template <typename T>
std::ostream& operator<<(typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e)
{
    return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

template <typename T>
std::ostream& operator<<(typename std::enable_if_t<std::is_same_v<T, std::byte>, std::ostream>& stream, const std::vector<T>& v) {
    return stream << HexStr(v);
}
} // namespace std

#endif
