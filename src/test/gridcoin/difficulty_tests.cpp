// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "chainparams.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/kernel.h"
#include "rpc/blockchain.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

#include <cmath>
#include <limits>
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

BOOST_AUTO_TEST_CASE(timestamp_window_classifies_both_eras)
{
    const int64_t P = 1790000000, N = 1790000100;   // parent stamp, receiver adjusted time
    // Before v15: [parent - 128, now + 128].
    BOOST_CHECK(CheckBlockTime(P - 128, P, N, false) == BlockTimeCheck::OK);
    BOOST_CHECK(CheckBlockTime(P - 129, P, N, false) == BlockTimeCheck::TOO_EARLY);
    BOOST_CHECK(CheckBlockTime(N + 128, P, N, false) == BlockTimeCheck::OK);
    BOOST_CHECK(CheckBlockTime(N + 129, P, N, false) == BlockTimeCheck::TOO_FAR_AHEAD);
    // From v15: [parent - 48, now + 48]; (now + 48, now + 128] unpenalized; beyond that DoS.
    BOOST_CHECK(CheckBlockTime(P - 48, P, N, true) == BlockTimeCheck::OK);
    BOOST_CHECK(CheckBlockTime(P - 49, P, N, true) == BlockTimeCheck::TOO_EARLY);
    BOOST_CHECK(CheckBlockTime(N + 48, P, N, true) == BlockTimeCheck::OK);
    BOOST_CHECK(CheckBlockTime(N + 49, P, N, true) == BlockTimeCheck::AHEAD_UNPENALIZED);
    BOOST_CHECK(CheckBlockTime(N + 128, P, N, true) == BlockTimeCheck::AHEAD_UNPENALIZED);
    BOOST_CHECK(CheckBlockTime(N + 129, P, N, true) == BlockTimeCheck::TOO_FAR_AHEAD);
    // The past bound is checked first: a stamp that is both too early and too far ahead is TOO_EARLY.
    BOOST_CHECK(CheckBlockTime(P - 1000, P + 2000, N, true) == BlockTimeCheck::TOO_EARLY);
}

BOOST_FIXTURE_TEST_CASE(kernel_target_eases_only_from_v15_and_only_past_300s, grc_test::StateGuard)
{
    LOCK(cs_main);
    SelectParams(CBaseChainParams::MAIN);
    const unsigned int bits = 0x1c194e41;
    CBlockIndex* parent = BuildChain({1790000000, 1790000090}, bits);
    const arith_uint256 base = FromCompact(bits);

    {
        grc_test::V15HeightGuard below(parent->nHeight + 2);
        BOOST_CHECK(GRC::GetKernelTarget(bits, parent->GetBlockTime() + 812, parent) == base);
    }
    {
        grc_test::V15HeightGuard at(parent->nHeight + 1);
        BOOST_CHECK(GRC::GetKernelTarget(bits, parent->GetBlockTime() + 300, parent) == base);
        BOOST_CHECK(GRC::GetKernelTarget(bits, parent->GetBlockTime() + 812, parent) == GRC::EaseTarget(base, 812));
        BOOST_CHECK(GRC::GetKernelTarget(bits, parent->GetBlockTime() + 812, parent) > base);
        // A stamp before the parent (negative elapsed time) is never eased. Unsigned arithmetic would wrap here.
        BOOST_CHECK(GRC::GetKernelTarget(bits, parent->GetBlockTime() - 16, parent) == base);
    }
}

BOOST_FIXTURE_TEST_CASE(kernel_target_on_a_limit_base_is_never_harder, grc_test::StateGuard)
{
    LOCK(cs_main);
    SelectParams(CBaseChainParams::MAIN);
    // A chain sitting at the proof-of-stake limit (regtest, testnet, a fresh chain).
    const unsigned int bits = GRC::GetProofOfStakeLimit().GetCompact();
    CBlockIndex* parent = BuildChain({1790000000, 1790000090}, bits);
    const arith_uint256 base = FromCompact(bits);
    grc_test::V15HeightGuard at(parent->nHeight + 1);

    for (const int64_t e : {-48, 0, 300, 301, 812, 5000, 100000}) {
        const arith_uint256 target = GRC::GetKernelTarget(bits, parent->GetBlockTime() + e, parent);
        BOOST_CHECK_MESSAGE(target >= base && target <= GRC::GetProofOfStakeLimit(), "e = " << e);
    }
}

BOOST_FIXTURE_TEST_CASE(kernel_target_is_monotone_over_long_outages, grc_test::StateGuard)
{
    LOCK(cs_main);
    SelectParams(CBaseChainParams::MAIN);
    const unsigned int bits = 0x1c194e41;
    CBlockIndex* parent = BuildChain({1790000000, 1790000090}, bits);
    const arith_uint256 base = FromCompact(bits);
    grc_test::V15HeightGuard at(parent->nHeight + 1);

    // From a stamp 48 s before the parent to a six-hour outage, in 16 s steps: never below the base, never zero,
    // never above the limit, non-decreasing, and at the limit by the end.
    arith_uint256 previous = base;
    for (int64_t e = -48; e <= 6 * 60 * 60; e += 16) {
        const arith_uint256 target = GRC::GetKernelTarget(bits, parent->GetBlockTime() + e, parent);
        BOOST_REQUIRE_MESSAGE(target >= previous && target >= base && target <= GRC::GetProofOfStakeLimit(),
                              "e = " << e);
        previous = target;
    }
    BOOST_CHECK(previous == GRC::GetProofOfStakeLimit());
}

BOOST_FIXTURE_TEST_CASE(stake_time_is_masked_and_guarded_from_v15, grc_test::StateGuard)
{
    LOCK(cs_main);
    SelectParams(CBaseChainParams::MAIN);
    CBlockIndex* parent = BuildChain({1790000000, 1790000096}, 0x1c194e41);   // parent stamp on the 16 s grid
    const int64_t P = parent->GetBlockTime();

    {
        grc_test::V15HeightGuard below(parent->nHeight + 2);
        BOOST_CHECK_EQUAL(GRC::GetStakeTime(parent, P + 21), P + 21);             // unchanged below v15
    }

    grc_test::V15HeightGuard at(parent->nHeight + 1);
    BOOST_CHECK_EQUAL(GRC::GetStakeTime(parent, P + 21), GRC::MaskStakeTime(P + 21));
    // A clock behind the parent by more than 48 s is lifted to parent - 48 (masked parent stamps are on the grid).
    BOOST_CHECK_EQUAL(GRC::GetStakeTime(parent, P - 200), P - 48);

    for (int64_t now = P - 500; now <= P + 500; now += 7) {
        const int64_t stamp = GRC::GetStakeTime(parent, now);
        BOOST_CHECK_EQUAL(stamp, GRC::MaskStakeTime(stamp));
        // The stamp always passes the past bound. It passes the node's own validation whenever the clock is at most
        // 96 s behind the parent; further behind, the node's own window rejects it, so the miner must skip.
        BOOST_CHECK(P - stamp <= GRC::V15_PAST_TIME_BOUND);
        const bool accepted = CheckBlockTime(stamp, P, now, true) == BlockTimeCheck::OK;
        BOOST_CHECK_MESSAGE(accepted == (now >= P - 2 * GRC::V15_PAST_TIME_BOUND), "now = P + " << now - P);
    }
}

// A zero mantissa encodes a target of zero. GetBlockDifficulty() returns
// +infinity for it, the value dividing by the mantissa gave, whatever the
// exponent, and CheckBlock's difficulty ceiling compares against that.
BOOST_AUTO_TEST_CASE(a_zero_mantissa_is_an_unbounded_difficulty)
{
    for (const unsigned int exponent : {0x00u, 0x01u, 0x1cu, 0x1du, 0x1eu, 0x20u, 0xffu}) {
        const double difficulty = GRC::GetBlockDifficulty(exponent << 24);
        BOOST_CHECK_MESSAGE(std::isinf(difficulty) && difficulty > 0,
                            "exponent " << exponent << " gave " << difficulty);
    }

    // A non-zero mantissa still divides: the minimum difficulty is exactly 1.
    BOOST_CHECK_EQUAL(GRC::GetBlockDifficulty(0x1d00ffff), 1.0);
    BOOST_CHECK_EQUAL(GRC::GetBlockDifficulty(0x1e00ffff), 1.0 / 256.0);
}

// MaskStakeTime() clears the low four bits for both types it is used with:
// block and transaction times (uint32_t) and adjusted time (int64_t), negative
// values included. These are the values the mask gave when it was ~int(15)
// converted to the stamp's type.
BOOST_AUTO_TEST_CASE(mask_stake_time_clears_the_low_four_bits)
{
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(uint32_t{0}), uint32_t{0});
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(uint32_t{15}), uint32_t{0});
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(uint32_t{1790000021}), uint32_t{1790000016});
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(std::numeric_limits<uint32_t>::max()), uint32_t{0xfffffff0});

    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(int64_t{1790000021}), int64_t{1790000016});
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(int64_t{-1}), int64_t{-16});
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(int64_t{-17}), int64_t{-32});
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(std::numeric_limits<int64_t>::max()),
                      std::numeric_limits<int64_t>::max() - 15);
    BOOST_CHECK_EQUAL(GRC::MaskStakeTime(std::numeric_limits<int64_t>::min()),
                      std::numeric_limits<int64_t>::min());
}

BOOST_AUTO_TEST_SUITE_END()
