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
class BanMan;

// Relay a transaction to peers, caching its serialized form in mapRelay so it
// can be served from the getdata loop (moved from net.h, issue #2558 PR 2b).
void RelayTransaction(const CTransaction& tx, const uint256& hash);
void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss);

//! Message-processing manager (issue #2558 PR 8a). Abstract interface; the
//! implementation (PeerManagerImpl) lives in net_processing.cpp and also
//! implements NetEventsInterface (ProcessMessages/SendMessages). The
//! peer-misbehavior tracking moved onto it in PR 8b (was the free
//! GetMisbehaviorAddr/MisbehavingAddr/ClearMisbehaviorForSubnet of PR 2c).
//! ThreadMessageHandler drives it through g_peerman; CConnman gains a
//! NetEventsInterface* in PR 8c.
class PeerManager : public NetEventsInterface
{
public:
    static std::unique_ptr<PeerManager> make(CConnman& connman, BanMan* banman);
    virtual ~PeerManager() {}

    //! Start the recurring scheduled tasks (shell in PR 8a; populated later).
    virtual void StartScheduledTasks(CScheduler& scheduler) = 0;

    //! Score misbehavior against an address; returns true if it triggered a ban.
    //! The per-address score, its linear decay, and the ban escalation moved
    //! here from net_processing's file scope in PR 8b. (Gridcoin keeps the
    //! CAddress-keyed map rather than Bitcoin master's NodeId-keyed form.)
    virtual bool Misbehaving(const CAddress& addr, int howmuch) = 0;

    //! Current (decayed) misbehavior score for an address.
    virtual int GetMisbehaviorScore(const CAddress& addr) = 0;

    //! Clear misbehavior scores for all addresses matching sub_net (registered
    //! as BanMan's clear callback). Returns the number of entries cleared.
    virtual unsigned int ClearMisbehaviorForSubnet(const CSubNet& sub_net) = 0;
};

extern std::unique_ptr<PeerManager> g_peerman;

#endif // BITCOIN_NET_PROCESSING_H
