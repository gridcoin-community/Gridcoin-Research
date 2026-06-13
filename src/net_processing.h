// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include "net.h"

class CTransaction;

// Relay a transaction to peers, caching its serialized form in mapRelay so it
// can be served from the getdata loop (moved from net.h, issue #2558 PR 2b).
void RelayTransaction(const CTransaction& tx, const uint256& hash);
void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss);

// Peer-misbehavior tracking (moved from CNode, issue #2558 PR 2c). The score
// map lives in net_processing.cpp; CNode::Misbehaving/GetMisbehavior forward
// here. ClearMisbehaviorForSubnet is registered as BanMan's clear callback.
int GetMisbehaviorAddr(const CAddress& addr);
bool MisbehavingAddr(const CAddress& addr, int howmuch);
unsigned int ClearMisbehaviorForSubnet(const CSubNet& sub_net);

bool ProcessMessages(CNode* pfrom) EXCLUSIVE_LOCKS_REQUIRED(pfrom->cs_vRecvMsg);
// Self-managed locking: called from ThreadMessageHandler2 in net.cpp with
// cs_main and pto->cs_vSend held by TRY_LOCK, but the function body acquires
// cs_main again internally for each section it needs. The current pattern
// is recursive-safe but TSA cannot model the recursive-acquire correctly,
// so the function intentionally has no EXCLUSIVE_LOCKS_REQUIRED annotation.
bool SendMessages(CNode* pto, bool fSendTrickle);

#endif // BITCOIN_NET_PROCESSING_H
