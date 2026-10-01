// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "chainparams.h"
#include "gridcoin/staking/difficulty.h"
#include "rpc/blockchain.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"

#include <boost/test/unit_test.hpp>

#include <vector>

namespace {
struct RetargetVector { unsigned int parent; int64_t t; unsigned int expected; };
struct EaseVector { unsigned int base; int64_t e; unsigned int expected; };

const RetargetVector RETARGET_VECTORS[] = {
    {0x1c194e41, -48, 0x1c1325e3},
    {0x1c194e41, -16, 0x1c146d93},
    {0x1c194e41, 0, 0x1c151993},
    {0x1c194e41, 16, 0x1c15cb29},
    {0x1c194e41, 64, 0x1c180340},
    {0x1c194e41, 90, 0x1c194e41},
    {0x1c194e41, 91, 0x1c195b66},
    {0x1c194e41, 128, 0x1c1b53ee},
    {0x1c194e41, 300, 0x1c26ad3e},
    {0x1c194e41, 600, 0x1c46e5e7},
    {0x1c194e41, 1290, 0x1d011dc5},
    {0x1c194e41, 1463, 0x1d01955a},
    {0x1c194e41, 2500, 0x1d0cdda4},
    {0x1c194e41, 2501, 0x1d0cdda4},
    {0x1c194e41, 10000, 0x1d0cdda4},
    {0x1c0f9a5c, -48, 0x1c0bce67},
    {0x1c0f9a5c, -16, 0x1c0c9873},
    {0x1c0f9a5c, 0, 0x1c0d0280},
    {0x1c0f9a5c, 16, 0x1c0d6fff},
    {0x1c0f9a5c, 64, 0x1c0ece45},
    {0x1c0f9a5c, 90, 0x1c0f9a5c},
    {0x1c0f9a5c, 91, 0x1c0fa277},
    {0x1c0f9a5c, 128, 0x1c10d98c},
    {0x1c0f9a5c, 300, 0x1c17d8e7},
    {0x1c0f9a5c, 600, 0x1c2bb6dc},
    {0x1c0f9a5c, 1290, 0x1d00b033},
    {0x1c0f9a5c, 1463, 0x1d00f9ee},
    {0x1c0f9a5c, 2500, 0x1d07eecb},
    {0x1c0f9a5c, 2501, 0x1d07eecb},
    {0x1c0f9a5c, 10000, 0x1d07eecb},
    {0x1d00ffff, -48, 0x1d00c1b4},
    {0x1d00ffff, -16, 0x1d00cea7},
    {0x1d00ffff, 0, 0x1d00d573},
    {0x1d00ffff, 16, 0x1d00dc77},
    {0x1d00ffff, 64, 0x1d00f2ea},
    {0x1d00ffff, 90, 0x1d00ffff},
    {0x1d00ffff, 91, 0x1d010083},
    {0x1d00ffff, 128, 0x1d011473},
    {0x1d00ffff, 300, 0x1d018742},
    {0x1d00ffff, 600, 0x1d02cd37},
    {0x1d00ffff, 1290, 0x1d0b4ae4},
    {0x1d00ffff, 1463, 0x1d10049f},
    {0x1d00ffff, 2500, 0x1e008226},
    {0x1d00ffff, 2501, 0x1e008226},
    {0x1d00ffff, 10000, 0x1e008226},
    {0x1e0fffff, -48, 0x1e0c1b4f},
    {0x1e0fffff, -16, 0x1e0cea7f},
    {0x1e0fffff, 0, 0x1e0d573f},
    {0x1e0fffff, 16, 0x1e0dc787},
    {0x1e0fffff, 64, 0x1e0f2eb7},
    {0x1e0fffff, 90, 0x1e0fffff},
    {0x1e0fffff, 91, 0x1e0fffff},
    {0x1e0fffff, 128, 0x1e0fffff},
    {0x1e0fffff, 300, 0x1e0fffff},
    {0x1e0fffff, 600, 0x1e0fffff},
    {0x1e0fffff, 1290, 0x1e0fffff},
    {0x1e0fffff, 1463, 0x1e0fffff},
    {0x1e0fffff, 2500, 0x1e0fffff},
    {0x1e0fffff, 2501, 0x1e0fffff},
    {0x1e0fffff, 10000, 0x1e0fffff},
};
const EaseVector EASE_VECTORS[] = {
    {0x1c194e41, -16, 0x1c194e41},
    {0x1c194e41, 0, 0x1c194e41},
    {0x1c194e41, 300, 0x1c194e41},
    {0x1c194e41, 316, 0x1c1a6db5},
    {0x1c194e41, 556, 0x1c329c82},
    {0x1c194e41, 812, 0x1c653904},
    {0x1c194e41, 4240, 0x1e0fffff},
    {0x1c194e41, 14128, 0x1e0fffff},
    {0x1c194e41, 100000, 0x1e0fffff},
    {0x1e0fffff, -16, 0x1e0fffff},
    {0x1e0fffff, 0, 0x1e0fffff},
    {0x1e0fffff, 300, 0x1e0fffff},
    {0x1e0fffff, 316, 0x1e0fffff},
    {0x1e0fffff, 556, 0x1e0fffff},
    {0x1e0fffff, 812, 0x1e0fffff},
    {0x1e0fffff, 4240, 0x1e0fffff},
    {0x1e0fffff, 14128, 0x1e0fffff},
    {0x1e0fffff, 100000, 0x1e0fffff},
};

arith_uint256 FromCompact(unsigned int c) { arith_uint256 v; v.SetCompact(c); return v; }

//! A linear chain of proof-of-stake block indexes with the given stamps and nBits. Returns the tip. The entries
//! live in mapBlockIndex; the StateGuard fixture erases them afterwards.
CBlockIndex* BuildChain(const std::vector<int64_t>& times, unsigned int bits) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CBlockIndex* prev = nullptr;
    for (size_t i = 0; i < times.size(); ++i) {
        CBlockIndex* idx = GRC::MockBlockIndex::InsertBlockIndex(InsecureRand256());
        idx->nHeight = 4000000 + static_cast<int>(i);
        idx->nTime = static_cast<unsigned int>(times[i]);
        idx->nBits = bits;
        idx->SetProofOfStake();
        idx->pprev = prev;
        if (prev) prev->pnext = idx;
        prev = idx;
    }
    return prev;
}
} // namespace

BOOST_AUTO_TEST_SUITE(difficulty_tests)

BOOST_AUTO_TEST_CASE(retarget_matches_reference_vectors)
{
    for (const auto& v : RETARGET_VECTORS) {
        BOOST_TEST_INFO("parent=" << std::hex << v.parent << " t=" << std::dec << v.t);
        BOOST_CHECK_EQUAL(GRC::RetargetLogLinear(FromCompact(v.parent), v.t).GetCompact(), v.expected);
    }
}

BOOST_AUTO_TEST_CASE(easing_matches_reference_vectors)
{
    for (const auto& v : EASE_VECTORS) {
        BOOST_TEST_INFO("base=" << std::hex << v.base << " e=" << std::dec << v.e);
        BOOST_CHECK_EQUAL(GRC::EaseTarget(FromCompact(v.base), v.e).GetCompact(), v.expected);
    }
}

BOOST_AUTO_TEST_CASE(retarget_at_90s_is_identity_and_monotone_in_t)
{
    const arith_uint256 base = FromCompact(0x1c194e41);
    BOOST_CHECK(GRC::RetargetLogLinear(base, 90) == base);
    arith_uint256 prev = GRC::RetargetLogLinear(base, -48);
    for (int64_t t = -47; t <= 3000; ++t) {
        const arith_uint256 cur = GRC::RetargetLogLinear(base, t);
        BOOST_CHECK(cur >= prev);
        prev = cur;
    }
    // Cap: everything at or beyond 2500 gives the same target.
    BOOST_CHECK(GRC::RetargetLogLinear(base, 2500) == GRC::RetargetLogLinear(base, 1000000));
}

BOOST_AUTO_TEST_CASE(retarget_extreme_negative_spacing_is_bounded)
{
    // Valid chains keep spacing at -128 s or above; anything far below is clamped rather than overflowing int64.
    const arith_uint256 base = FromCompact(0x1c194e41);
    BOOST_CHECK(GRC::RetargetLogLinear(base, -1000000000000LL) == GRC::RetargetLogLinear(base, -GRC::V15_SPACING_CAP));
    BOOST_CHECK(GRC::RetargetLogLinear(base, -1000000000000LL) > 0);
}

BOOST_AUTO_TEST_CASE(retarget_never_wraps_or_exceeds_the_limit)
{
    const arith_uint256& limit = GRC::GetProofOfStakeLimit();
    for (int64_t t = -48; t <= 10000; t += 7) {
        const arith_uint256 r = GRC::RetargetLogLinear(limit, t);
        BOOST_CHECK(r > 0);
        BOOST_CHECK(r <= limit);
    }
    BOOST_CHECK(GRC::RetargetLogLinear(limit, 1463) == limit);   // first wrap point of the unchecked order
}

BOOST_AUTO_TEST_CASE(easing_is_one_below_threshold_and_monotone_to_the_limit)
{
    const arith_uint256& limit = GRC::GetProofOfStakeLimit();
    for (unsigned int bits : {0x1c194e41u, limit.GetCompact()}) {
        const arith_uint256 base = FromCompact(bits);
        BOOST_CHECK(GRC::EaseTarget(base, -48) == base);
        BOOST_CHECK(GRC::EaseTarget(base, 0) == base);
        BOOST_CHECK(GRC::EaseTarget(base, 300) == base);
        arith_uint256 prev = base;
        for (int64_t e = 300; e <= 400000; e += 16) {
            const arith_uint256 cur = GRC::EaseTarget(base, e);
            BOOST_CHECK(cur >= prev);      // monotone non-decreasing
            BOOST_CHECK(cur >= base);      // never harder than the base
            BOOST_CHECK(cur <= limit);     // never above the limit
            BOOST_CHECK(cur > 0);          // never zero
            prev = cur;
        }
        BOOST_CHECK(GRC::EaseTarget(base, 400000) == limit);
    }
}

BOOST_AUTO_TEST_CASE(scale_exp2_handles_extreme_exponents)
{
    const arith_uint256& limit = GRC::GetProofOfStakeLimit();
    const arith_uint256 base = FromCompact(0x1c194e41);
    BOOST_CHECK(GRC::ScaleTargetExp2(base, 0, limit) == base);
    BOOST_CHECK(GRC::ScaleTargetExp2(base, int64_t{300} << 16, limit) == limit);   // shift >= 256: overflow
    BOOST_CHECK(GRC::ScaleTargetExp2(base, -(int64_t{300} << 16), limit) == arith_uint256(1)); // underflow -> 1
    BOOST_CHECK(GRC::ScaleTargetExp2(base, 65536, limit) == base * 2);                // exactly one doubling
}

BOOST_FIXTURE_TEST_CASE(next_target_switches_to_log_linear_exactly_at_v15, grc_test::StateGuard)
{
    LOCK(cs_main);
    SelectParams(CBaseChainParams::MAIN);
    const unsigned int bits = 0x1c194e41;
    // Parent spacing 16 s: the old rule gives (810 + 32) / 990, the log-linear rule 2^(2 (16 - 90) / (990 ln 2)).
    CBlockIndex* tip = BuildChain({1790000000, 1790000016, 1790000032}, bits);

    {
        grc_test::V15HeightGuard below(tip->nHeight + 2);   // the next block (tip + 1) is below v15
        arith_uint256 old_expected = FromCompact(bits);
        old_expected *= 810 + 2 * 16;
        old_expected /= arith_uint256(990);
        BOOST_CHECK_EQUAL(GRC::GetNextTargetRequired(tip), old_expected.GetCompact());
    }
    {
        grc_test::V15HeightGuard at(tip->nHeight + 1);      // the next block is the first v15 block
        BOOST_CHECK_EQUAL(GRC::GetNextTargetRequired(tip), GRC::RetargetLogLinear(FromCompact(bits), 16).GetCompact());
    }
}

BOOST_FIXTURE_TEST_CASE(next_target_v15_uses_signed_parent_spacing, grc_test::StateGuard)
{
    LOCK(cs_main);
    SelectParams(CBaseChainParams::MAIN);
    const unsigned int bits = 0x1c194e41;
    // Parent stamped 32 s before its own parent: the old rule treats that as 90 s, the v15 rule uses -32.
    CBlockIndex* tip = BuildChain({1790000064, 1790000080, 1790000048}, bits);
    grc_test::V15HeightGuard at(0);
    BOOST_CHECK_EQUAL(GRC::GetNextTargetRequired(tip), GRC::RetargetLogLinear(FromCompact(bits), -32).GetCompact());
}

BOOST_FIXTURE_TEST_CASE(next_target_v15_ignores_the_difficulty_valve, grc_test::StateGuard)
{
    LOCK(cs_main);
    SelectParams(CBaseChainParams::MAIN);
    // Difficulty 2^24, above the 900000 valve, with the chain tip as pindexBest so that the valve reads it.
    const unsigned int bits = 0x1a00ffff;
    CBlockIndex* tip = BuildChain({1790000000, 1790000016, 1790000032}, bits);
    pindexBest = tip;
    BOOST_REQUIRE_GT(GRC::GetCurrentDifficulty(), 900000.0);

    {
        grc_test::V15HeightGuard below(tip->nHeight + 2);
        BOOST_CHECK_EQUAL(GRC::GetNextTargetRequired(tip), GRC::GetProofOfStakeLimit().GetCompact());
    }
    {
        grc_test::V15HeightGuard at(tip->nHeight + 1);
        BOOST_CHECK_EQUAL(GRC::GetNextTargetRequired(tip), GRC::RetargetLogLinear(FromCompact(bits), 16).GetCompact());
    }
}

BOOST_AUTO_TEST_SUITE_END()
