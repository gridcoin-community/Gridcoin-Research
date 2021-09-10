// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_FEES_H
#define BITCOIN_POLICY_FEES_H

#include "amount.h"
#include "primitives/transaction.h"
#include "consensus/consensus.h"

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
