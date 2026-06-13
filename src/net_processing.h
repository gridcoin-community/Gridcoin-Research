// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include "net.h"

#include <memory>

class CTransaction;
class CConnman;
class CScheduler;

// Relay a transaction to peers, caching its serialized form in mapRelay so it
// can be served from the getdata loop (moved from net.h, issue #2558 PR 2b).
void RelayTransaction(const CTransaction& tx, const uint256& hash);
void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss);

// Peer-misbehavior tracking (moved from CNode, issue #2558 PR 2c). The score
// map lives in net_processing.cpp; CNode::Misbehaving/GetMisbehavior forward
// here. ClearMisbehaviorForSubnet is registered as BanMan's clear callback.
// (These collapse into PeerManager in PR 8b.)
int GetMisbehaviorAddr(const CAddress& addr);
bool MisbehavingAddr(const CAddress& addr, int howmuch);
unsigned int ClearMisbehaviorForSubnet(const CSubNet& sub_net);

//! Message-processing manager (issue #2558 PR 8a). Abstract interface; the
//! implementation (PeerManagerImpl) lives in net_processing.cpp and also
//! implements NetEventsInterface (ProcessMessages/SendMessages). The peer-level
//! API (Misbehaving, ...) collapses onto this in PR 8b. ThreadMessageHandler
//! drives it through g_peerman; CConnman gains a NetEventsInterface* in PR 8c.
class PeerManager : public NetEventsInterface
{
public:
    static std::unique_ptr<PeerManager> make(CConnman& connman);
    virtual ~PeerManager() {}

    //! Start the recurring scheduled tasks (shell in PR 8a; populated later).
    virtual void StartScheduledTasks(CScheduler& scheduler) = 0;
};

extern std::unique_ptr<PeerManager> g_peerman;

#endif // BITCOIN_NET_PROCESSING_H
