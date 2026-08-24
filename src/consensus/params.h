// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include "util.h"

namespace Consensus {

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;

    /** Block height at which protocol v2 becomes active */
    int ProtocolV2Height;
    /** Block height at which research age is enabled */
    int ResearchAgeHeight;
    /** Block height at which v8 blocks are created after */
    int BlockV8Height;
    /** Block height at which v9 blocks are created */
    int BlockV9Height;
    /** Block height at which v9 tally becomes active (3 hours after v9) */
    int BlockV9TallyHeight;
    /** Block height at which v10 blocks are created */
    int BlockV10Height;
    /** Block height at which v11 blocks are created */
    int BlockV11Height;
    /** Block height at which v12 blocks are created */
    int BlockV12Height;
    /** Block height at which v13 blocks are created */
    int BlockV13Height;
    /** Block height at which v14 blocks are created (CLTV + CSV + BIP68) */
    int BlockV14Height;
    /** Block height at which v15 blocks are created. Default
      * std::numeric_limits<int>::max() means the v15 hard-fork machinery
      * (including on-chain pool registration, issue #1783) is inert until a
      * follow-up release pins the activation height by maintainer/community
      * decision. See doc/consensus.md.
      */
    int BlockV15Height;
    /** Retention window (in blocks) after which a PENDING pool registration
      * or a POOL_APPROVE OPEN pre-authorization is treated as expired for
      * query and takeover-defense purposes. Pure query-time check, no state
      * mutation at the expiration boundary — reorg-safe by construction.
      * 28800 blocks is ~30 days at mainnet ~90s spacing. Consensus-affecting:
      * nodes with differing values will disagree on POOL_REGISTER admission
      * across expiration boundaries and fork. Override via the hidden
      * -pendingpoolretention arg (isolated-testnet / regtest only — see
      * init.cpp). See doc/consensus.md §11.
      */
    int PendingPoolRetention;
    /** Grace period in blocks after BlockV14Height before peers on the old
      * protocol version are disconnected. Network-specific to allow testnet
      * a longer window when the fork has already passed before deployment.
      */
    int ProtocolVersionGracePeriod;
    /** Block height at which poll v3 contract payloads are valid */
    int PollV3Height;
    /** Block height at which project v2 contracts are allowed */
    int ProjectV2Height;
    /** Block height at which poll multi-address eligibility claims are required */
    int PollMultiAddressHeight;
    /** Block height at which project v4 contracts are allowed */
    int ProjectV4Height;
    /** Height at which the benefit of the doubt logic is enabled for autogreylist evaluation */
    int AutoGreylistAuditHeight;
    /** Height at which Whitelist::Snapshot deep-copies project entries before applying the
      * auto-greylist overlay. Set to std::numeric_limits<int>::max() on main/testnet until
      * activation is scheduled; overridable via -autogreylistdeepcopyheight for testnet
      * rollout. */
    int AutoGreylistDeepCopyHeight;
    /** Height at which the scraper stops emitting a (spurious zero) total-credit entry for a project
      * whose user-statistics export returned no usable records this cycle (the "no_records" condition).
      * Before this height the legacy behavior is preserved (a no_records project still gets a zero entry
      * in ProjectsAllCpidTotalCredits) so historical superblocks remain bit-identical; at/after it the
      * entry is omitted, so the auto-greylist baseline records nullopt (benefit-of-doubt) instead of a
      * hard zero that collapses WAS. Set to std::numeric_limits<int>::max() on main/testnet until
      * activation is scheduled; overridable via -autogreylisttotalcreditfixheight for testnet rollout. */
    int AutoGreylistTotalCreditFixHeight;
    /**
      * @brief Single activation height for the remaining AutoGreylist correctness batch. Covers
      * (a) separation of the pending (candidate/tip-anchored) greylist state from the authoritative
      * (committed-superblock) state, (b) treating a chain-resident zero project total credit as
      * missing data rather than a real observation, and (c) the walker corrections that follow from
      * (a). Batched behind ONE height deliberately: these activate together in the release, and a
      * partial combination is a configuration nobody tests. AutoGreylistTotalCreditFixHeight is a
      * different thing -- a scraper-EMIT gate that stops new spurious zeros being written; zeros
      * already recorded in the chain are permanent and keep corrupting the WAS window as they
      * transit it (one landing on the j=40 endpoint inflates the 40-SB average and collapses WAS;
      * one at j<=7 inflates the 7-SB average and can mask a genuine greylist).
      *
      * MUST NOT be set below AutoGreylistDeepCopyHeight. Before the deep-copy gate the Snapshot
      * overlay writes through to the registry and only ever promotes to AUTO_GREYLISTED -- there is
      * no demotion arm, and ReinitFromDisk is conditioned on the deep-copy crossing -- so a project
      * spuriously greylisted before that gate cannot heal. Co-activation satisfies this: within
      * Quorum::PushSuperblock, ReinitFromDisk runs before the AutoGreylist Refresh. By procedure the
      * deep-copy height is never set above the others.
      *
      * TBD: set coincident with BlockV15Height when v15 is scheduled. std::numeric_limits<int>::max()
      * on main/testnet until then; kept as its own field only so it can be driven independently
      * during testing. Overridable via -autogreylistredesignheight for testnet rollout. */
    int AutoGreylistRedesignHeight;
    /**
      * @brief The default GRC paid for a constant block reward.
      *
      * Note that the GRC paid for CBR can be specified by an administrative protocol entry with the key name "blockreward1" for
      * V13+ blocks. The value is specified in HALFORDS.
      */
    int64_t DefaultConstantBlockReward;
    /**
      * @brief The minimum GRC that can be set by administrative contract for a constant block reward (clamp floor). This is valid
      * for block v13+. Note that this is typed as int64_t rather than CAmount to avoid the extra include.
      */
    int64_t ConstantBlockRewardFloor;
    /**
      * @brief The maximum GRC that can be set by administrative contract for a constant block reward (clamp ceiling). This is valid
      * for block v13+. Note that this is typed as int64_t rather than CAmount to avoid the extra include.
      */
    int64_t ConstantBlockRewardCeiling;
    /**
      * @brief Block height at which superblock v3 contracts are allowed/required
      */
    int SuperblockV3Height;
    /** The fraction of rewards taken as fees in an MRC after the zero payment interval. Only consesnus critical
      * at BlockV12Height or above. Note that this is typed as int64_t rather than CAmount to avoid the extra include.
      */
    Fraction InitialMRCFeeFractionPostZeroInterval;
    /** The amount of time from the last reward payment to a researcher where submitting an MRC will resort in 100%
      * forfeiture of fees to the staker and/or foundation. Only consensus critical at BlockV12Height or above.
      */
    int64_t MRCZeroPaymentInterval;
    /**
     * @brief The maximum allocation (as a Fraction) that can be used by all of the mandatory sidestakes
     */
    Fraction MaxMandatorySideStakeTotalAlloc;
    /**
      * @brief The multiplier applied to network magnitude to determine the rate of accrual. Nominally 1/4 from Fern onwards.
      *
      * Note that the magnitude unit can be set by an administrative protocol entry with the key name "magnitudeunit" for
      * V13+ blocks. The value is specified as a whole number or fraction. For example, 0.25 would be "1/4", 5 would be "5".
      */
    Fraction DefaultMagnitudeUnit;
    /**
      * @brief The maximum magnitude unit allowed to be specified. This is an upper clamp that is set at 5.
      */
    Fraction MaxMagnitudeUnit;
    /**
     * @brief This is the minimum allowed magnitude weight factor as a fraction. Applicable for block v13+
     */
    Fraction MinMagnitudeWeightFactor;
    /**
      * @brief The multiplier applied to (money supply / network magnitude) to scale the network magnitude into equivalent GRC
      * for purposes of computing voting weight. Nominally 1 / 5.67 from Fern onwards.
      *
      * The magnitude weight factor can be set by an administrative protocol entry with the key name "magnitudeweightfactor" for
      * V13+ blocks. The value is specified as a whole number or fraction. For example, 1 / 5.67 would be "100/567", 2 would be "2".
      */
    Fraction DefaultMagnitudeWeightFactor;
    /**
     * @brief This is the maximum allowed magnitude weight factor as a fraction. Applicable for block v13+.
     */
    Fraction MaxMagnitudeWeightFactor;
    /** The "standard" contract replay lookback for those contract types that do not have a registry db.
      */
    int64_t StandardContractReplayLookback;
    /**
      * "standard" scrypt target limit for proof of work, results in 0,000244140625 proof-of-work difficulty.
      * Equivalent to ~arith_uint256() >> 20 or 1e0fffff in compact notation.
      */
    uint256 powLimit;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
