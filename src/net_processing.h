// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include "net.h"

bool ProcessMessages(CNode* pfrom) EXCLUSIVE_LOCKS_REQUIRED(pfrom->cs_vRecvMsg);
// Self-managed locking: called from ThreadMessageHandler2 in net.cpp with
// cs_main and pto->cs_vSend held by TRY_LOCK, but the function body acquires
// cs_main again internally for each section it needs. The current pattern
// is recursive-safe but TSA cannot model the recursive-acquire correctly,
// so the function intentionally has no EXCLUSIVE_LOCKS_REQUIRED annotation.
bool SendMessages(CNode* pto, bool fSendTrickle);

#endif // BITCOIN_NET_PROCESSING_H
