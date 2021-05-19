// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/** (POS/POR) enums for CoinStake Transactions -- We should never get unknown but just in case!*/
enum MinedType
{
    UNKNOWN = 0,
    POS = 1,
    POR = 2,
    ORPHANED = 3,
    POS_SIDE_STAKE_RCV = 4,
    POR_SIDE_STAKE_RCV = 5,
    POS_SIDE_STAKE_SEND = 6,
    POR_SIDE_STAKE_SEND = 7,
    SUPERBLOCK = 8
};
