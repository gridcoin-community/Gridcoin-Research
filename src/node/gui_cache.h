// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_GUI_CACHE_H
#define GRIDCOIN_NODE_GUI_CACHE_H

#include "amount.h"
#include "interfaces/gui_state.h"
#include "interfaces/mrc.h"
#include "sync.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <thread>

extern CCriticalSection cs_main;

namespace node {

//! The fewest milliseconds between two updates of the node's GUI state cache, for
//! every trigger but a GUI request: the old GUI loop's MODEL_UPDATE_DELAY. During a
//! sync that is one update every four seconds; once synced, about one a block.
static constexpr std::chrono::milliseconds GUI_CACHE_MIN_INTERVAL{4000};

//! How often the cache's timer wakes with nothing pending, to catch the
//! time-based out-of-sync test turning without a block.
static constexpr std::chrono::milliseconds GUI_CACHE_TICK{30000};

//!
//! \brief The node's copy of the state the GUI shows that needs cs_main to
//! compute (interfaces::GuiStateSnapshot), kept fresh by the node and pushed to
//! the GUI, so the GUI never takes cs_main to refresh it.
//!
//! The work needs cs_main anyway, so it is done where cs_main is already held:
//! SetBestChain calls UpdateLocked() in its own scope after every tip change, and
//! the call is a no-op unless the interval has passed since the last update. A
//! timer thread catches what that leaves pending (the last block of a burst, a
//! trigger that is not a block, a GUI request, the out-of-sync test turning) and
//! takes cs_main for it only then. Every snapshot is delivered from the timer
//! thread, holding no lock. So during a sync, when SetBestChain keeps the cache
//! fresh, the timer finds nothing due and never takes cs_main.
//!
//! Each part of the snapshot is recomputed only when one of its inputs changed
//! since the last update (MarkDirty); the other parts are kept. MRC, ETTS and
//! RESEARCHER are computed only while the node is synced; while it is out of
//! sync they are absent.
//!
//! The cache works only while a GUI is subscribed (AddSubscriber; a GUI's
//! interfaces::GuiState::handleChanged counts as one while its Handler lives).
//! With none, UpdateLocked returns after its lock-free check and the timer
//! computes nothing and takes no cs_main; the marks just accumulate. The
//! subscription that ends that (the first) asks for a forced update
//! (RequestUpdate), so a GUI that attaches gets a computed snapshot within one
//! timer cycle. The monolithic GUI subscribes when it starts, so it always has
//! one; a daemon computes only while a multiprocess GUI is attached, and one
//! with no GUI pays nothing for the cache.
//!
//! A GUI may also ask for the MRC figures at a fee boost (RequestMRCQuote). The
//! timer serves the latest such request under cs_main, not held to the
//! interval, and answers it through its own fn (QuoteFn) with the request's id.
//! Quotes are user-paced, so they do not wait for a subscriber.
//!
//! Lock invariants (a deadlock is a matter of order; cs_main being recursive
//! does not prevent one):
//!  1. Every lock an update takes is taken after cs_main, and each such pair
//!     already occurs in the node (SetBestChain's wallet, mempool and
//!     miner-status work; the researcher and MRC snapshots the GUI takes today),
//!     so no new order is added. The cache's own two locks are leaves.
//!  2. The snapshot's leaf lock is never held while another lock is taken.
//!  3. The timer never holds its wake mutex while taking cs_main, and nothing
//!     takes cs_main while holding the wake mutex.
//!  4. Nothing is delivered while any lock is held.
//!  5. A handler of a signal emitted under another lock only marks (MarkDirty):
//!     an atomic or and the wake notify. A GUI's subscription and request calls
//!     only count, record and notify, too.
//!
class GuiCache
{
public:
    //! The parts of the snapshot, by the inputs that change them. A tip changes
    //! CHAIN, BALANCES, MRC, ETTS and RESEARCHER. A MINER update whose staking
    //! flag flips recomputes ETTS too.
    enum Part : uint32_t { CHAIN = 1, BALANCES = 2, MINER = 4, MRC = 8, ETTS = 16, RESEARCHER = 32, ALL = 63 };

    //!
    //! \brief How the cache computes, reads its clock and takes cs_main.
    //! Production passes MakeProductionCompute(); tests pass fakes.
    //!
    struct Compute {
        //! Fill one part of the snapshot, including its present flag where it has
        //! one, keeping the rest. Each runs with cs_main held by the caller: it may
        //! re-enter cs_main, and it takes cs_wallet and other locks after it. The
        //! cache sets out_of_sync before any part runs, and runs RESEARCHER before
        //! MRC, so a part may read both.
        std::function<void(interfaces::GuiStateSnapshot&)> chain, balances, miner, mrc, etts, researcher;

        //! The MRC figures at a fee boost, for a GUI's quote request: the MRC
        //! part's trial claim at that boost. Runs with cs_main held by the caller
        //! (the timer). \p current is the cache's snapshot, whose researcher part
        //! gives the eligibility, as it does for the MRC part.
        std::function<interfaces::MRCSnapshot(CAmount fee_boost, const interfaces::GuiStateSnapshot& current)>
            mrc_quote;

        //! OutOfSyncByAge(). Lock-free: the timer calls it holding nothing.
        std::function<bool()> out_of_sync;

        //! A steady clock, in milliseconds.
        std::function<int64_t()> now_ms;

        //! Run the function with cs_main held: production is LOCK(cs_main); f().
        std::function<void(const std::function<void()>&)> run_under_cs_main;
    };

    //! Receives every new snapshot, on the timer thread, holding no lock. The
    //! sequences it sees never decrease but may repeat: an update that lands
    //! while a delivery copies the snapshot is delivered then and again after.
    using DeliverFn = std::function<void(const interfaces::GuiStateSnapshot&)>;

    //! Receives each quote answer (RequestMRCQuote), on the timer thread,
    //! holding no lock, with the id of the request it answers.
    using QuoteFn = std::function<void(uint64_t request_id, const interfaces::MRCSnapshot&)>;

    //! How long the timer waits, once an update looks due to it, before it takes
    //! cs_main for it. An update SetBestChain is about to make in its own scope
    //! (it marks first, then updates) lands within that time, and then the timer
    //! finds nothing due. So the mark of an arriving block never sends the timer
    //! for cs_main, and during a sync, while blocks come faster than this, the end
    //! of each interval goes to SetBestChain, not to the timer. A GUI request does
    //! not wait.
    static constexpr std::chrono::milliseconds SETTLE{250};

    //! \param min_interval The fewest milliseconds between two updates, for every
    //! trigger but RequestUpdate() (GUI_CACHE_MIN_INTERVAL in production).
    //! \param tick How long the timer sleeps with nothing pending
    //! (GUI_CACHE_TICK in production).
    //! \param deliver_quote Receives the quote answers; none, and they go
    //! nowhere.
    GuiCache(Compute compute, DeliverFn deliver, std::chrono::milliseconds min_interval,
             std::chrono::milliseconds tick, QuoteFn deliver_quote = {});

    //! Stops the timer (Stop()).
    ~GuiCache();

    GuiCache(const GuiCache&) = delete;
    GuiCache& operator=(const GuiCache&) = delete;

    //! Recompute the marked parts and swap the new snapshot in, if an update is
    //! due: a GUI is subscribed, marks are pending or the out-of-sync test has
    //! turned, and the interval has passed since the last update; or \p force
    //! with a GUI subscribed. Not due, it returns at once, having taken no lock.
    //! Never takes cs_main itself, and never delivers: it leaves the delivery to
    //! the timer and wakes it. An exception from a part propagates, the snapshot is
    //! left as it was, and the parts stay marked for the next due update.
    void UpdateLocked(bool force) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Mark \p parts as changed. Any thread, under any locks: an atomic or, and
    //! a wake of the timer when nothing was pending before. Nothing else.
    void MarkDirty(uint32_t parts);

    //! The GUI asks for a fresh snapshot: every part is marked and the timer
    //! makes a forced update, taking cs_main on its own thread, then delivers it.
    //! A request made while one is pending joins it. One the timer takes while no
    //! GUI is subscribed is dropped: no one would see its answer. Never blocks.
    void RequestUpdate();

    //! Count a GUI subscribed to the deliveries. Returns whether it is the
    //! first: that GUI's GuiState then asks for a forced update
    //! (RequestUpdate), since nothing was computed while there was none, and
    //! that request wakes the timer. Counting wakes nothing, so the timer sees
    //! the subscriber and the request together and makes one forced update. The
    //! marks left pending meanwhile are otherwise taken by the next due update.
    //! Never blocks.
    bool AddSubscriber();

    //! A subscribed GUI has gone (its Handler was disconnected or destroyed):
    //! once per AddSubscriber. With none left, the cache stops computing.
    void RemoveSubscriber();

    //! The GUI asks for the MRC figures at \p fee_boost. The timer computes them
    //! under cs_main (Compute::mrc_quote), not held to the interval, and answers
    //! through the QuoteFn with \p request_id. One quote is pending at a time: a
    //! request made before the timer takes the pending one replaces it, so only
    //! the latest is answered. Never blocks.
    void RequestMRCQuote(CAmount fee_boost, uint64_t request_id);

    //! Start the timer thread ("grc-guistate"). Once; not after Stop().
    void StartTimer();

    //! Stop and join the timer. Idempotent. Nothing is delivered after it
    //! returns. Must not be called holding cs_main: the timer may be waiting for
    //! it.
    void Stop();

    //! The current snapshot, copied under the leaf lock alone.
    interfaces::GuiStateSnapshot Current() const;

private:
    //! m_last_update_ms before the first update.
    static constexpr int64_t NEVER = std::numeric_limits<int64_t>::min();

    //! The parts that exist only while the node is synced.
    static constexpr uint32_t SYNCED_PARTS = MRC | ETTS | RESEARCHER;

    //! A GUI's pending quote request.
    struct QuoteRequest {
        CAmount fee_boost = 0;
        uint64_t request_id = 0;
    };

    const Compute m_compute;
    const DeliverFn m_deliver;
    const std::chrono::milliseconds m_min_interval;
    const std::chrono::milliseconds m_tick;
    const QuoteFn m_deliver_quote;

    //! The GUIs subscribed to the deliveries (AddSubscriber). None, and the
    //! cache computes nothing.
    std::atomic<int> m_subscribers{0};

    //! Parts marked since the last update. All at first: the cache starts empty.
    std::atomic<uint32_t> m_dirty{ALL};

    //! m_compute.now_ms() at the last update, or NEVER.
    std::atomic<int64_t> m_last_update_ms{NEVER};

    //! The out-of-sync state the last update gated the synced parts with. True
    //! at first, as the empty snapshot has them absent.
    std::atomic<bool> m_gated_out_of_sync{true};

    //! A GUI request is pending.
    std::atomic<bool> m_request{false};

    //! An update swapped in a snapshot the timer has not delivered yet.
    std::atomic<bool> m_delivery_pending{false};

    //! Set by Stop(), read lock-free by the delivery and, under the wake mutex,
    //! by the timer's waits.
    std::atomic<bool> m_stop{false};

    //! The leaf lock: held only to copy or swap the snapshot, taking nothing.
    mutable Mutex m_snapshot_mutex;
    interfaces::GuiStateSnapshot m_snapshot GUARDED_BY(m_snapshot_mutex);

    //! The timer's wake. A leaf: nothing is taken while it is held.
    Mutex m_wake_mutex;
    std::condition_variable m_wake_cv;
    bool m_wake GUARDED_BY(m_wake_mutex){false};

    //! The latest quote request the timer has not taken yet.
    std::optional<QuoteRequest> m_quote GUARDED_BY(m_wake_mutex);

    //! Serializes starting and joining the timer.
    Mutex m_timer_mutex;
    std::thread m_timer GUARDED_BY(m_timer_mutex);

    //! A GUI is subscribed (lock-free).
    bool HasSubscribers() const;

    //! A GUI is subscribed, and marks are pending or the out-of-sync test has
    //! turned (lock-free).
    bool IsPending() const;

    //! Pending, and the interval has passed since the last update (lock-free).
    bool IsDue(int64_t now_ms) const;

    //! The out-of-sync test has turned since the last update (lock-free).
    bool SyncTurned() const;

    //! How long the timer may sleep before something it has to act on is due.
    std::chrono::milliseconds NextWait() const;

    //! Wake the timer.
    void Notify();

    //! Deliver the pending snapshot, if any. The timer only, holding no lock.
    void Deliver();

    //! Compute \p quote under cs_main and answer it holding no lock. The timer
    //! only.
    void ServeQuote(const QuoteRequest& quote);

    //! Wait SETTLE, or until Stop(). False when stopped.
    bool Settle();

    void TimerLoop();
};

//! The production compute: each part reads the node's state with cs_main held,
//! the clock is the steady clock, and run_under_cs_main takes cs_main.
GuiCache::Compute MakeProductionCompute();

//! The node's GUI state cache: null before StartGuiCache() and after
//! StopGuiCache().
GuiCache* GetGuiCache();

//! Make \p cache the node's, start its timer and publish it, so SetBestChain
//! updates it. The node owns it from here.
void StartGuiCache(std::unique_ptr<GuiCache> cache);

//! Unpublish the node's cache and stop its timer. The object stays alive until
//! the next StartGuiCache() or exit, because a trigger already running on
//! another thread may still mark it. Must not be called holding cs_main.
void StopGuiCache();

} // namespace node

#endif // GRIDCOIN_NODE_GUI_CACHE_H
