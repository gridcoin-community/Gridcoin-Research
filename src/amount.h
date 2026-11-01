// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_AMOUNT_H
#define BITCOIN_AMOUNT_H

#include <stdint.h>

/** Amount in halfords (Can be negative) */
typedef int64_t CAmount;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

/** No amount larger than this (in Halford) is valid.
 *
 * Note that this constant is *not* the total money supply, which in Gridcoin
 * currently happens to be less than 2,000,000,000 GRC for various reasons, but
 * rather a sanity check. As this sanity check is used by consensus-critical
 * validation code, the exact value of the MAX_MONEY constant is consensus
 * critical; in unusual circumstances like an overflow bug that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
static const CAmount MAX_MONEY = 2000000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

//! Floor for the post-split coinstake UTXO size, in whole GRC (not Halfords) to
//! match the -minstakesplitvalue config entry. It is a hard clamp, not just a
//! default: miner.cpp takes max(-minstakesplitvalue, this), so a smaller
//! configured value is raised to it. Not a consensus or per-network parameter
//! (same on every chain), so it lives in this stateless header rather than
//! chainparams or miner.h -- which also lets the GUI options dialog/model read
//! it without pulling in the mining engine's headers.
static const int64_t MIN_STAKE_SPLIT_VALUE_GRC = 800;

#endif //  BITCOIN_AMOUNT_H
