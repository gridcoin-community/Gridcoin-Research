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
    /** Block height at which poll v3 contract payloads are valid */
    int PollV3Height;
    /** Block height at which project v2 contracts are allowed */
    int ProjectV2Height;
    /**
     * @brief Block height at which superblock v3 contracts are allowed/required
     */
    int SuperblockV3Height;
    /** The fraction of rewards taken as fees in an MRC after the zero payment interval. Only consesnus critical
      * at BlockV12Height or above.
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

    int64_t StandardContractReplayLookback;

    uint256 powLimit;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
