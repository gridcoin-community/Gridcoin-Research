// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include <primitives/transaction.h>
#include "net.h"
#include "sync.h"
#include "validationinterface.h"

#include <memory>

class CTransaction;
class CConnman;
class CScheduler;
class BanMan;

//! Declared in chain.h; redeclared here for the UpdatedBlockTip lock
//! annotation (the gridcoin/staking/chain_trust.h idiom) without pulling the
//! chain-state header into the net layer.
extern CCriticalSection cs_main;

// Relay a transaction to peers, caching its serialized form in mapRelay so it
// can be served from the getdata loop (moved from net.h, issue #2558 PR 2b).
void RelayTransaction(const CTransaction& tx, const uint256& hash);
void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss);

//! Drop a transaction's cached serialization from relay memory.
//!
//! The getdata loop serves from mapRelay *before* consulting the mempool, and
//! entries linger there for 15 minutes with expiry as their only removal path.
//! Removing a transaction from the mempool therefore does not stop this node
//! from handing it out: a peer whose getdata is still queued would be served
//! the cached copy. Callers that cancel a transaction outright must purge it
//! here as well, or the cancellation only appears to have taken effect.
void RemoveFromRelayMemory(const uint256& hash);

//! Re-announce the node's own transactions still awaiting initial broadcast.
//! Intended to be driven periodically by the scheduler (never from SendMessages;
//! see the definition for the lock-order rationale).
void ResendUnbroadcastTransactions();

//! \brief One entry in the orphan transaction pool.
//!
//! Declared here, rather than privately in net_processing.cpp, so that the tests
//! that drive the pool name the same type. A private copy in a test satisfies
//! ODR only for as long as the two stay token-identical, and the failure when
//! they diverge is a silent type mismatch across translation units.
struct COrphanTx {
    CTransaction tx;
    int64_t time_received;
};

//! \brief How long an orphan transaction may sit before it is swept.
//!
//! Published for the same reason as COrphanTx: a test asserting the boundary
//! must assert against the value the implementation actually uses.
static constexpr int64_t ORPHAN_TX_EXPIRE_SECONDS = 20 * 60;

//! \brief Sweep orphan transactions past ORPHAN_TX_EXPIRE_SECONDS.
//!
//! The orphan pool's count limit is applied only when a new orphan is inserted,
//! and an orphan is otherwise erased only when its parent arrives. A peer that
//! fills the pool and then stops sending therefore leaves it full indefinitely.
//! This gives reclamation a driver that does not depend on further traffic.
//!
//! Takes cs_main. Safe from the scheduler thread, which holds no per-node locks.
//!
//! \return the number of orphans swept.
//! \param now  Current adjusted time; taken as a parameter rather than read
//!             internally, matching OrphanBlockManager::EraseExpired and
//!             PSGTPool::EraseExpired, and so that it is testable without
//!             mocking the clock for the sweep itself.
unsigned int ExpireOrphanTransactions(const int64_t now);

//! Relay a pooled PSGT revision (#2910) to peers on PSGT_PROTO_VERSION or
//! later. The object itself is served from the PSGT pool by the getdata loop.
void RelayPSGT(const uint256& revision_hash);

//! Message-processing manager (issue #2558 PR 8a). Abstract interface; the
//! implementation (PeerManagerImpl) lives in net_processing.cpp and also
//! implements NetEventsInterface (ProcessMessages/SendMessages). The
//! peer-misbehavior tracking moved onto it in PR 8b (was the free
//! GetMisbehaviorAddr/MisbehavingAddr/ClearMisbehaviorForSubnet of PR 2c).
//! ThreadMessageHandler drives it through g_peerman; CConnman gains a
//! NetEventsInterface* in PR 8c. It is additionally a CValidationInterface
//! subscriber (registered in init.cpp) so relay work hangs off validation
//! signals like Bitcoin's net_processing (issue #3125 C8, the
//! PeerManager-as-subscriber half of #3030 workstream B3).
class PeerManager : public NetEventsInterface, public CValidationInterface
{
public:
    static std::unique_ptr<PeerManager> make(CConnman& connman, BanMan* banman);
    virtual ~PeerManager() {}

    //! Start the recurring scheduled tasks (shell in PR 8a; populated later).
    virtual void StartScheduledTasks(CScheduler& scheduler) = 0;

    //! CValidationInterface: relay the new best-block inventory to peers
    //! (moved from AcceptBlock, issue #3125 C8). Redeclared public here --
    //! the base declares it protected -- so tests can drive the handler
    //! through g_peerman directly (the node/psgt_pool.h pattern).
    virtual void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork,
                                 bool fInitialDownload) override EXCLUSIVE_LOCKS_REQUIRED(cs_main) = 0;

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

    //! \brief Queue an inventory request on this peer.
    //!
    //! Was CNode::AskFor. The per-inventory request-time map it consults lives
    //! in net_processing (it is request tracking, which is this layer's job, and
    //! net.h must not depend on this header). Applies the two-minute per-item
    //! backoff and pushes onto the peer's own mapAskFor priority queue.
    virtual void AskFor(CNode* pnode, const CInv& inv) = 0;

    //! \brief Forget that an inventory item was requested.
    //!
    //! Call when the item arrives or the request is abandoned, so it stops being
    //! re-asked. Absence is meaningful: SendMessages treats a missing entry as a
    //! satisfied request and will not re-arm it.
    virtual void ForgetInventoryRequest(const CInv& inv) = 0;

    //! \brief Clear an inventory item's backoff without forgetting it.
    //!
    //! Leaves the entry present with a zero request time, so the next AskFor
    //! sends immediately instead of two minutes out. Distinct from
    //! ForgetInventoryRequest because presence is load-bearing (see above).
    virtual void ResetInventoryRequestBackoff(const CInv& inv) = 0;
};

extern std::unique_ptr<PeerManager> g_peerman;

//! \brief Capacity of the inventory-request map behind PeerManager::AskFor.
//!
//! The map itself is private to net_processing.cpp. This exists so the bound is
//! still assertable: an unbounded map here is the regression that motivated
//! keeping limitedmap at all (a peer announcing inventory it never serves grows
//! state charged to every peer, since entries are only removed on receipt).
size_t InventoryRequestMapCapacity();

#endif // BITCOIN_NET_PROCESSING_H
