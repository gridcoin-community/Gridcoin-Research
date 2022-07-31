// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_GENERATED_TYPE_H
#define BITCOIN_WALLET_GENERATED_TYPE_H

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
    SUPERBLOCK = 8,
    MRC_RCV = 9,
    MRC_SEND = 10
};

#endif // BITCOIN_WALLET_GENERATED_TYPE_H
