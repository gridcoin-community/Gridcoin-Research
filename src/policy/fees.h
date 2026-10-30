// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_FEES_H
#define BITCOIN_POLICY_FEES_H

#include "amount.h"
#include "primitives/transaction.h"
#include "consensus/consensus.h"

#include <cstdint>
#include <utility>

//!
//! \brief A fee over a transaction's serialized size, ordered as the exact ratio.
//!
//! The miner ranks block candidates by fee rate. It used to work the rate out
//! as fee / (size / 1000.0) in a double, which ties rates a double cannot tell
//! apart and does not round the same on every floating-point unit: 32-bit x86
//! builds use the x87 unit, whose 80-bit intermediates round differently from
//! the 64-bit doubles of SSE2 and ARM. This compares the ratio exactly, in
//! integers, and gives the same order on every platform.
//!
//! Each rate is split into whole satoshi per byte, rounded down, and a
//! remainder in [0, size). Two rates compare on the whole parts, then on the
//! remainders multiplied by the other side's size. Each remainder is below its
//! own size, so both products fit in uint64_t for any two sizes, and no fee,
//! negative ones included, can overflow the comparison. The size must not be
//! zero: the split divides by it.
//!
struct FeeRate
{
    CAmount fee = 0;
    unsigned int size = 1; //!< Serialized size in bytes. Must not be zero.

    bool operator<(const FeeRate& other) const
    {
        const auto [whole, remainder] = Split();
        const auto [other_whole, other_remainder] = other.Split();

        if (whole != other_whole) return whole < other_whole;

        return static_cast<uint64_t>(remainder) * other.size
            < static_cast<uint64_t>(other_remainder) * size;
    }

    bool operator>(const FeeRate& other) const { return other < *this; }

    //! Equal as ratios: 1 satoshi over 2 bytes equals 2 over 4.
    bool operator==(const FeeRate& other) const { return !(*this < other) && !(other < *this); }
    bool operator!=(const FeeRate& other) const { return !(*this == other); }

private:
    //! Whole satoshi per byte, rounded toward negative infinity, and the
    //! remainder in [0, size) that goes with it.
    std::pair<CAmount, CAmount> Split() const
    {
        CAmount whole = fee / static_cast<CAmount>(size);
        CAmount remainder = fee % static_cast<CAmount>(size);

        if (remainder < 0) {
            --whole;
            remainder += size;
        }

        return {whole, remainder};
    }
};

enum GetMinFee_mode
{
    GMF_BLOCK,
    GMF_RELAY,
    GMF_SEND,
};

inline CAmount GetBaseFee(const CTransaction& tx, enum GetMinFee_mode mode = GMF_BLOCK)
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE

    CAmount nBaseFee = 0;

    // Written this way to silence the duplicate branch warning on advanced compilers while MIN_RELAY_TX_FEE
    // and MIN_TX_FEE are equal.
    if (mode != GMF_RELAY)
    {
        nBaseFee = MIN_TX_FEE;
    }
    else if (mode == GMF_RELAY)
    {
        nBaseFee = MIN_RELAY_TX_FEE;
    }

    // For block version 11 onwards, which corresponds to CTransaction::CURRENT_VERSION 2,
    // a multiplier is used on top of MIN_TX_FEE and MIN_RELAY_TX_FEE
    if (tx.nVersion >= 2)
    {
        nBaseFee *= 10;
    }

    return nBaseFee;
}

inline CAmount GetMinFee(const CTransaction& tx, unsigned int nBlockSize = 1, enum GetMinFee_mode mode=GMF_BLOCK, unsigned int nBytes = 0)
{
    CAmount nBaseFee = GetBaseFee(tx, mode);

    unsigned int nNewBlockSize = nBlockSize + nBytes;
    CAmount nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee)
    {
        for (auto const& txout : tx.vout)
            if (txout.nValue < CENT)
                nMinFee = nBaseFee;
    }

    // Raise the price as the block approaches full
    if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2)
    {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return MAX_MONEY;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

#endif // BITCOIN_POLICY_FEES_H
