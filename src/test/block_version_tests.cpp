// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Tests for ComputeBlockVersion(height): the pure block-header-version ladder
// the staking miner uses to stamp new blocks. This is a follow-up to #2955 /
// #3121, where the miner was never taught to produce v15 blocks at the V15
// gate while AcceptBlock had been taught to require them, so the staker emitted
// v14 blocks its own validator rejected ("reject too old nVersion = 14") and
// the chain self-halted at BlockV15Height. That bug survived the unit tests and
// a 3-agent adversarial review precisely because the version selection was
// buried inline in the stake-assembly path with no unit coverage. These tests
// pin the ladder to the chainparams gate heights and tie the miner default to
// the AcceptBlock version band so the same hole cannot silently re-open.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <miner.h>
#include <primitives/block.h>

namespace {
//! Mirror of the V12..V15 rungs of the two-sided block-version band enforced in
//! AcceptBlock (src/validation.cpp): a block at \p height is accepted only when
//! its version is neither "too old" (below the highest active gate) nor "too
//! new" (at or above a not-yet-active gate). The real band also carries the
//! legacy ProtocolV2 / V8..V11 rungs; they are omitted here because every
//! height these tests exercise sits at or above the V12 gate (the lowest is
//! v13 - 1, still far above the mainnet V12 activation), where all omitted
//! lower gates are already active and thus constant — so the retained rungs
//! classify those heights identically to the full band. Kept parallel to
//! the validator so the assertions below fail loudly if the miner default ever
//! drifts outside the band the network will actually accept; if the tested
//! height set is ever lowered below the V12 gate, restore the missing rungs.
bool AcceptedByVersionBand(int height, int32_t nVersion)
{
    const bool too_old =
        (IsV12Enabled(height) && nVersion < 12)
        || (IsV13Enabled(height) && nVersion < 13)
        || (IsV14Enabled(height) && nVersion < 14)
        || (IsV15Enabled(height) && nVersion < 15);

    const bool too_new =
        (!IsV12Enabled(height) && nVersion >= 12)
        || (!IsV13Enabled(height) && nVersion >= 13)
        || (!IsV14Enabled(height) && nVersion >= 14)
        || (!IsV15Enabled(height) && nVersion >= 15);

    return !too_old && !too_new;
}
} // namespace

BOOST_AUTO_TEST_SUITE(block_version_tests)

// The ladder must map each half-open height band to its version rung, checked
// against the live chainparams gate heights (read, not hard-coded, so the test
// tracks any future re-pinning of the gates).
BOOST_AUTO_TEST_CASE(compute_block_version_ladder_matches_gate_heights)
{
    SelectParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = Params().GetConsensus();

    const int v13 = consensus.BlockV13Height;
    const int v14 = consensus.BlockV14Height;
    const int v15 = GetBlockV15Height();

    // Local copy so the Boost comparison macros, which bind their operands by
    // const reference, do not ODR-use the CBlock::CURRENT_VERSION static (it
    // has an in-class initializer but no out-of-line definition).
    const int current_version = CBlock::CURRENT_VERSION;

    // The bands below are only meaningful if the gates are ordered and V13/V14
    // are finite; mainnet satisfies this (V15 may be inert at INT_MAX).
    BOOST_REQUIRE_LT(0, v13);
    BOOST_REQUIRE_LT(v13, v14);
    BOOST_REQUIRE_LT(v14, v15);

    // Below V13 -> 12.
    BOOST_CHECK_EQUAL(ComputeBlockVersion(v13 - 1), 12);

    // [V13, V14) -> 13, at both ends of the half-open band.
    BOOST_CHECK_EQUAL(ComputeBlockVersion(v13), 13);
    BOOST_CHECK_EQUAL(ComputeBlockVersion(v14 - 1), 13);

    // [V14, V15) -> 14, at both ends of the half-open band.
    BOOST_CHECK_EQUAL(ComputeBlockVersion(v14), 14);
    BOOST_CHECK_EQUAL(ComputeBlockVersion(v15 - 1), 14);

    // >= V15 -> CURRENT_VERSION (not a literal 15). With mainnet V15 inert at
    // INT_MAX this exercises the topmost representable height; once V15 is
    // pinned to a finite value it exercises the real activation height.
    BOOST_CHECK_EQUAL(ComputeBlockVersion(v15), current_version);
}

// The version the miner stamps must fall inside the AcceptBlock band at every
// gate boundary. This is the exact property that failed in #2955: the miner
// produced 14 at a height where the validator required >= 15. Cross-checking
// ComputeBlockVersion against the mirrored band at each boundary turns that
// end-to-end-only failure into a unit tripwire.
BOOST_AUTO_TEST_CASE(miner_version_inside_accept_band_at_gate_boundaries)
{
    SelectParams(CBaseChainParams::MAIN);
    const Consensus::Params& consensus = Params().GetConsensus();

    const int v13 = consensus.BlockV13Height;
    const int v14 = consensus.BlockV14Height;
    const int v15 = GetBlockV15Height();

    for (int height : {v13 - 1, v13, v14 - 1, v14, v15 - 1, v15}) {
        BOOST_CHECK_MESSAGE(
            AcceptedByVersionBand(height, ComputeBlockVersion(height)),
            "miner version " << ComputeBlockVersion(height)
                             << " rejected by AcceptBlock band at height " << height);
    }
}

// Regression guard for the CURRENT_VERSION knob itself: the miner default at a
// V15 height must be at least the AcceptBlock v15 lower bound. If a future edit
// lowered CBlock::CURRENT_VERSION below 15 the staker would once again emit a
// version its own validator rejects, self-halting the chain at the gate.
BOOST_AUTO_TEST_CASE(current_version_tracks_v15_lower_bound)
{
    SelectParams(CBaseChainParams::MAIN);

    // Local copy: the Boost macro binds by const reference (ODR-use) and the
    // CBlock::CURRENT_VERSION static has no out-of-line definition.
    const int current_version = CBlock::CURRENT_VERSION;

    BOOST_CHECK_GE(current_version, 15);
    BOOST_CHECK_GE(ComputeBlockVersion(GetBlockV15Height()), 15);
}

BOOST_AUTO_TEST_SUITE_END()
