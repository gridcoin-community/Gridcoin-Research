// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/staking/spam.h"
#include "sync.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <cstring>

namespace {
//! A proof hash with bytes of our choosing right after it. The slot a proof
//! hash maps to must depend on its 32 bytes alone, so the same hash has to be
//! found whatever follows it in memory. Aligned like size_t so that a word-wise
//! read of the hash cannot fault instead of returning a wrong slot.
struct alignas(alignof(size_t)) Holder
{
    uint256 hash;
    unsigned char trailing[32];
};

static_assert(offsetof(Holder, trailing) == sizeof(uint256),
              "the trailing bytes must sit directly after the hash");
} // namespace

BOOST_AUTO_TEST_SUITE(seen_stakes_tests)

//! Remember a proof, then look it up through copies of it that differ only in
//! the bytes after the hash. Each trailing byte value gives the trailing words
//! a different sum, so a slot computed from those bytes would differ between
//! the copies and the lookups would miss, bar a chance collision in 2048 slots.
BOOST_AUTO_TEST_CASE(a_remembered_proof_is_found_whatever_follows_it_in_memory)
{
    LOCK(cs_main);

    GRC::SeenStakes seen;
    const uint256 proof = uint256S("5f6e8d2c1b0a99887766554433221100ffeeddccbbaa99887766554433221100");

    Holder remembered;
    remembered.hash = proof;
    std::memset(remembered.trailing, 0xC3, sizeof(remembered.trailing));
    seen.Remember(remembered.hash);

    for (int fill = 1; fill <= 8; ++fill) {
        Holder lookup;
        lookup.hash = proof;
        std::memset(lookup.trailing, fill, sizeof(lookup.trailing));

        BOOST_CHECK_MESSAGE(seen.ContainsProof(lookup.hash),
                            "proof not found with trailing bytes 0x" << std::hex << fill);
    }
}

//! The table holds nothing it was not given.
BOOST_AUTO_TEST_CASE(an_unseen_proof_is_not_found)
{
    LOCK(cs_main);

    GRC::SeenStakes seen;
    BOOST_CHECK(!seen.ContainsProof(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")));
}

BOOST_AUTO_TEST_SUITE_END()
