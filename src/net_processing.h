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
#include <utility>
#include <vector>

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

//! Announcements held back until the caller's wallet lock is released (#3391).
//!
//! Every relay entry point below reaches CConnman::RelayInventory ->
//! ForEachNode -> CNode::PushInventory, which takes that peer's cs_inventory.
//! SendMessages takes cs_inventory and then asks the wallet whether a
//! transaction is ours, taking cs_wallet. Announcing while cs_wallet is held
//! therefore closes an ABBA cycle: the announcing thread walks every node and
//! eventually reaches the one the message handler is servicing.
//!
//! Declare one BEFORE the lock guard it must outlive, queue while the lock is
//! held, and it announces when it goes out of scope -- after the guard above it
//! has unlocked. That ordering is the whole point, so the declaration must come
//! first; destruction runs in reverse order of construction.
//!
//! \code
//!     DeferredRelay relay;                 // flushes last
//!     LOCK2(cs_main, pwalletMain->cs_wallet);
//!     ...
//!     relay.AddTransaction(tx, tx.GetHash());
//!     return SomeResult;                   // unlocks, then announces
//! \endcode
//!
//! Queuing is what makes this usable in functions with many exit paths: every
//! return and every throw flushes the same way, with no per-exit bookkeeping.
//!
//! SCOPE. Callers using this do not form a closed set, and nothing here should
//! be read as claiming they do. The set was enumerated six times by four
//! methods while #3391 was being fixed -- direct relay sites, the
//! TransactionAddedToMempool signal path, callers of CommitTransaction, callers
//! of SendMoney, and a transitive walk -- and every pass found sites the
//! previous one missed; the GRC::SendContract callers are known to be
//! unconverted today. One of the paths reaches RelayPSGT through a
//! CValidationInterface subscriber list populated at runtime, which no static
//! walk can see at all.
//!
//! So this is a hold-time cleanup, NOT the reason the cs_wallet/cs_inventory
//! cycle is broken. That comes from SendMessages resolving its wallet answer
//! before taking cs_inventory, which makes cs_inventory a leaf: a leaf cannot
//! close a cycle however many cs_wallet -> cs_inventory edges remain. Anything
//! that wants the class closed properly should give the announcement to the
//! scheduler, the way ResendUnbroadcastTransactions already does, rather than
//! threading this queue through another call level.
class DeferredRelay
{
public:
    DeferredRelay() = default;
    ~DeferredRelay();

    DeferredRelay(const DeferredRelay&) = delete;
    DeferredRelay& operator=(const DeferredRelay&) = delete;

    //! Queue a transaction announcement. The transaction is copied, so the
    //! caller's object may go out of scope before the flush.
    void AddTransaction(const CTransaction& tx, const uint256& hash);

    //! Queue a pooled PSGT revision announcement.
    void AddPSGT(const uint256& revision_hash);

    //! Announce everything queued and empty the queue. Called by the
    //! destructor; public so a caller that wants the announcements at a
    //! specific point can force them, having released its locks.
    void Flush();

    bool empty() const { return m_txs.empty() && m_psgts.empty(); }

private:
    std::vector<std::pair<CTransaction, uint256>> m_txs;
    std::vector<uint256> m_psgts;
};

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
    //!
    //! Lifetime contract for the null guards at external call sites
    //! (scraper_net, chainman): g_peerman outlives every calling thread --
    //! StopNode() joins the net and scraper threads before init resets the
    //! pointer -- so `if (g_peerman)` there is defensive, never load-bearing.
    //! A change to shutdown ordering must revisit those guards: a null here
    //! silently drops a request rather than failing.
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
