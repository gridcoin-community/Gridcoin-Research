// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_FEES_H
#define BITCOIN_POLICY_FEES_H

#include "amount.h"
#include "primitives/transaction.h"

enum GetMinFee_mode
{
    GMF_BLOCK,
    GMF_RELAY,
    GMF_SEND,
};

CAmount GetBaseFee(const CTransaction& tx, enum GetMinFee_mode mode=GMF_BLOCK);

CAmount GetMinFee(const CTransaction& tx, unsigned int nBlockSize=1, enum GetMinFee_mode mode=GMF_BLOCK, unsigned int nBytes = 0);

#endif // BITCOIN_POLICY_FEES_H
