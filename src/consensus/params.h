// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "uint256.h"

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
};
} // namespace Consensus
