// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
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
    /** The fraction of rewards taken as fees in an MRC after the zero payment interval. Only consesnus critical
      * at BlockV12Height or above.
      */
    Fraction InitialMRCFeeFractionPostZeroInterval;
    /** The amount of time from the last reward payment to a researcher where submitting an MRC will resort in 100%
      * forfeiture of fees to the staker and/or foundation. Only consensus critical at BlockV12Height or above.
      */
    int64_t MRCZeroPaymentInterval;

    uint256 powLimit;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
