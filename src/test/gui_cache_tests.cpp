// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file gui_cache_tests.cpp
//! \brief The node's GUI state cache (node/gui_cache.h): its rate limit, its
//! timer, the parts it recomputes, and the lock rules it keeps.
//!
//! The cases run the cache on fakes passed through its own constructor: a
//! Compute whose parts count their calls, a manual clock that moves only when a
//! case moves it, and a run_under_cs_main that takes the real cs_main and counts
//! the timer's acquisitions. An in-scope update is what SetBestChain does: mark
//! the parts dirty and call UpdateLocked under cs_main. The timer cases use short
//! intervals so its real-time waits stay short; whether an update is due is
//! decided by the manual clock alone, so a case controls it exactly.
//!

#include "gridcoin/staking/status.h"
#include "interfaces/gui_state.h"
#include "logging.h"
#include "node/gui_cache.h"
#include "sync.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

extern CWallet* pwalletMain;

using namespace std::chrono_literals;

namespace {

using node::GuiCache;
using interfaces::GuiStateSnapshot;

//! The bound on a wait that should end almost at once. It only stops a broken
//! cache from hanging the run.
constexpr auto WAIT_LIMIT = 10s;

//! Timer-case cadence. Short, so the timer's real-time waits stay short.
constexpr auto INTERVAL = 200ms;
constexpr auto TICK = 300ms;

//! Long enough for the timer to have woken, settled and acted at least once
//! if it was going to.
constexpr auto TIMER_CHANCE = TICK + GuiCache::SETTLE + INTERVAL;

//!
//! \brief A Compute whose parts count their calls and fill only what a case
//! reads, on a manual clock.
//!
class FakeCompute
{
public:
    std::atomic<int> chain_calls{0};
    std::atomic<int> balances_calls{0};
    std::atomic<int> miner_calls{0};
    std::atomic<int> mrc_calls{0};
    std::atomic<int> etts_calls{0};
    std::atomic<int> researcher_calls{0};

    //! Calls of run_under_cs_main: only the timer makes them.
    std::atomic<int> cs_main_runs{0};

    //! The thread the chain part last ran on.
    std::atomic<std::thread::id> chain_thread{};

    //! What the chain part reports.
    std::atomic<int> height{0};

    std::atomic<bool> out_of_sync{false};
    std::atomic<int64_t> clock_ms{1000000};

    //! Run inside the chain part, with cs_main held. Set before the cache is
    //! constructed.
    std::function<void()> chain_hook;

    GuiCache::Compute Make()
    {
        GuiCache::Compute compute;

        compute.chain = [this](GuiStateSnapshot& snap) {
            ++chain_calls;
            chain_thread = std::this_thread::get_id();
            snap.height = height.load();
            if (chain_hook) chain_hook();
        };
        compute.balances = [this](GuiStateSnapshot& snap) {
            ++balances_calls;
            snap.has_balances = true;
        };
        compute.miner = [this](GuiStateSnapshot&) { ++miner_calls; };
        compute.mrc = [this](GuiStateSnapshot& snap) {
            ++mrc_calls;
            snap.has_mrc = true;
        };
        compute.etts = [this](GuiStateSnapshot& snap) {
            ++etts_calls;
            snap.has_etts = true;
        };
        compute.researcher = [this](GuiStateSnapshot& snap) {
            ++researcher_calls;
            snap.has_researcher = true;
        };
        compute.out_of_sync = [this] { return out_of_sync.load(); };
        compute.now_ms = [this] { return clock_ms.load(); };
        compute.run_under_cs_main = [this](const std::function<void()>& fn) {
            ++cs_main_runs;
            LOCK(cs_main);
            fn();
        };

        return compute;
    }

    void Advance(std::chrono::milliseconds by) { clock_ms += by.count(); }
};

//!
//! \brief Records what the cache delivers, and whether the delivering thread
//! held any lock while it did.
//!
class Deliveries
{
public:
    GuiCache::DeliverFn Make()
    {
        return [this](const GuiStateSnapshot& snap) {
            // Checked before this recorder takes its own lock. Without
            // DEBUG_LOCKORDER the check always passes.
            const bool no_lock_held = LockStackEmpty();
            {
                LOCK(m_mutex);
                m_delivered.push_back(snap);
                if (!no_lock_held) ++m_delivered_under_a_lock;
            }
            m_cv.notify_all();
        };
    }

    //! Wait until a delivered snapshot satisfies \p pred.
    bool WaitFor(const std::function<bool(const GuiStateSnapshot&)>& pred)
    {
        const auto deadline = std::chrono::steady_clock::now() + WAIT_LIMIT;
        WAIT_LOCK(m_mutex, lock);
        while (true) {
            for (const GuiStateSnapshot& snap : m_delivered) {
                if (pred(snap)) return true;
            }
            if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout) return false;
        }
    }

    size_t Count()
    {
        LOCK(m_mutex);
        return m_delivered.size();
    }

    GuiStateSnapshot Last()
    {
        LOCK(m_mutex);
        return m_delivered.empty() ? GuiStateSnapshot{} : m_delivered.back();
    }

    int DeliveredUnderALock()
    {
        LOCK(m_mutex);
        return m_delivered_under_a_lock;
    }

private:
    Mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<GuiStateSnapshot> m_delivered GUARDED_BY(m_mutex);
    int m_delivered_under_a_lock GUARDED_BY(m_mutex){0};
};

//! What SetBestChain does: mark the parts and update under the cs_main it holds.
void InScope(GuiCache& cache, uint32_t parts)
{
    LOCK(cs_main);
    cache.MarkDirty(parts);
    cache.UpdateLocked(false);
}

//! A Boost check cannot run on another thread, so the cases wait for flags
//! the other threads set.
bool WaitUntil(const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + WAIT_LIMIT;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(5ms);
    }
    return true;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(gui_cache_tests)

BOOST_AUTO_TEST_CASE(update_in_scope_is_rate_limited)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);

    // Fifty blocks inside one interval: only the first computes.
    {
        LOCK(cs_main);
        for (int i = 0; i < 50; ++i) {
            cache.MarkDirty(GuiCache::CHAIN);
            cache.UpdateLocked(false);
            compute.Advance(10ms);
        }
    }

    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 1);

    // The interval has passed since that update: the next block computes again.
    compute.Advance(4000ms);
    InScope(cache, GuiCache::CHAIN);

    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_CHECK_EQUAL(cache.Current().sequence, 2U);

    // No timer ran, so nothing took cs_main for the cache and nothing was
    // delivered.
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 0);
    BOOST_CHECK_EQUAL(deliveries.Count(), 0U);
}

BOOST_AUTO_TEST_CASE(timer_catches_the_last_block_of_a_burst)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, TICK);

    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();

    // Ten blocks inside one interval with a rising height. The first is due and
    // updates in scope; the other nine find it not due and leave their marks.
    compute.Advance(INTERVAL);
    for (int height = 1; height <= 10; ++height) {
        compute.height = height;
        InScope(cache, GuiCache::CHAIN);
        compute.Advance(5ms);
    }

    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_CHECK_EQUAL(cache.Current().height, 1);

    // Inside the interval the timer leaves the pending marks alone.
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 0);

    // Then no more blocks: once the interval passes, the timer takes cs_main
    // once and delivers the last height.
    compute.Advance(INTERVAL);
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.height == 10; }));

    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 1);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 3);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(timer_does_not_take_cs_main_when_nothing_is_due)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, TICK);

    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();

    // A block arrives as each interval passes, so every update is made in scope
    // and the timer finds nothing due, over three intervals and several ticks.
    // SetBestChain marks, then updates, and the mark wakes the timer with the
    // update due but not yet made. That gap is normally microseconds, but a
    // loaded machine can stretch it: 20 ms here, well inside the timer's settle.
    for (int i = 0; i < 3; ++i) {
        compute.Advance(INTERVAL);
        {
            LOCK(cs_main);
            cache.MarkDirty(GuiCache::CHAIN);
            std::this_thread::sleep_for(20ms);
            cache.UpdateLocked(false);
        }
        std::this_thread::sleep_for(TIMER_CHANCE);
    }

    // The timer delivered the in-scope updates without taking cs_main.
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 4; }));
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 0);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 4);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(untouched_parts_are_not_recomputed)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);

    // The first update computes every part: the cache starts empty.
    InScope(cache, GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.balances_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.miner_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.researcher_calls.load(), 1);

    // A miner change alone recomputes the miner part and keeps the others.
    compute.Advance(4000ms);
    InScope(cache, GuiCache::MINER);

    BOOST_CHECK_EQUAL(compute.miner_calls.load(), 2);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.balances_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.researcher_calls.load(), 1);

    const GuiStateSnapshot snap = cache.Current();
    BOOST_CHECK_EQUAL(snap.sequence, 2U);
    BOOST_CHECK(snap.has_balances);
    BOOST_CHECK(snap.has_mrc);
    BOOST_CHECK(snap.has_etts);
    BOOST_CHECK(snap.has_researcher);
}

BOOST_AUTO_TEST_CASE(synced_parts_absent_while_out_of_sync)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);

    // Out of sync from the start: the synced parts are never computed.
    compute.out_of_sync = true;
    InScope(cache, GuiCache::ALL);

    GuiStateSnapshot snap = cache.Current();
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 0);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 0);
    BOOST_CHECK_EQUAL(compute.researcher_calls.load(), 0);
    BOOST_CHECK(snap.out_of_sync);
    BOOST_CHECK(!snap.has_mrc);
    BOOST_CHECK(!snap.has_etts);
    BOOST_CHECK(!snap.has_researcher);

    // The always parts are computed regardless.
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 1);
    BOOST_CHECK(snap.has_balances);

    // Synced, they appear.
    compute.out_of_sync = false;
    compute.Advance(4000ms);
    InScope(cache, GuiCache::ALL);

    snap = cache.Current();
    BOOST_CHECK(!snap.out_of_sync);
    BOOST_CHECK(snap.has_mrc);
    BOOST_CHECK(snap.has_etts);
    BOOST_CHECK(snap.has_researcher);

    // Out of sync again: they go absent and their computes are not called.
    compute.out_of_sync = true;
    compute.Advance(4000ms);
    InScope(cache, GuiCache::ALL);

    snap = cache.Current();
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.researcher_calls.load(), 1);
    BOOST_CHECK(snap.out_of_sync);
    BOOST_CHECK(!snap.has_mrc);
    BOOST_CHECK(!snap.has_etts);
    BOOST_CHECK(!snap.has_researcher);
}

BOOST_AUTO_TEST_CASE(periodic_tick_flips_synced_parts_without_a_block)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, TICK);

    compute.out_of_sync = true;
    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();

    // Let the timer deliver that update and go idle on its tick.
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));
    BOOST_CHECK(!deliveries.Last().has_mrc);
    std::this_thread::sleep_for(GuiCache::SETTLE + INTERVAL);

    // The time-based test turns, with no block, no mark and no clock movement:
    // the next tick sees it and the synced parts arrive.
    compute.out_of_sync = false;
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.has_mrc; }));

    const GuiStateSnapshot snap = deliveries.Last();
    BOOST_CHECK(!snap.out_of_sync);
    BOOST_CHECK(snap.has_etts);
    BOOST_CHECK(snap.has_researcher);
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 1);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(request_update_forces_an_update_off_the_caller_thread)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);

    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    // Inside the interval (the clock does not move), a request still updates,
    // on the timer thread, and the answer is delivered.
    cache.RequestUpdate();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 2; }));

    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 1);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_CHECK(compute.chain_thread.load() != std::this_thread::get_id());

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(sequence_increases_when_height_decreases)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, TICK);

    BOOST_CHECK_EQUAL(cache.Current().sequence, 0U);

    compute.height = 100;
    InScope(cache, GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(cache.Current().sequence, 1U);
    BOOST_CHECK_EQUAL(cache.Current().height, 100);

    // A reorg to a shorter chain: the height goes down, the sequence goes up.
    compute.Advance(INTERVAL);
    compute.height = 90;
    InScope(cache, GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(cache.Current().sequence, 2U);
    BOOST_CHECK_EQUAL(cache.Current().height, 90);

    cache.StartTimer();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 2; }));
    BOOST_CHECK_EQUAL(deliveries.Last().height, 90);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(delivery_holds_no_lock)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, TICK);

    // One delivery of each origin: an in-scope update, a request, and the
    // timer's own update at the end of an interval.
    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    cache.RequestUpdate();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 2; }));

    cache.MarkDirty(GuiCache::MINER);
    compute.Advance(INTERVAL);
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 3; }));

    cache.Stop();

    BOOST_CHECK_EQUAL(deliveries.DeliveredUnderALock(), 0);
}

BOOST_AUTO_TEST_CASE(stop_joins_while_a_compute_runs)
{
    FakeCompute compute;
    Deliveries deliveries;

    Mutex latch_mutex;
    std::condition_variable latch_cv;
    bool released = false; // guarded by latch_mutex
    std::atomic<bool> armed{false};
    std::atomic<bool> entered{false};

    compute.chain_hook = [&] {
        if (!armed) return;
        entered = true;
        WAIT_LOCK(latch_mutex, lock);
        while (!released) latch_cv.wait(lock);
    };

    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);

    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    // A request puts the timer inside a compute, which blocks on the latch.
    armed = true;
    cache.RequestUpdate();
    BOOST_REQUIRE(WaitUntil([&] { return entered.load(); }));

    std::atomic<bool> stop_returned{false};
    std::thread stopper([&] {
        cache.Stop();
        stop_returned = true;
    });

    // Stop waits for the update in flight.
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK(!stop_returned);

    {
        LOCK(latch_mutex);
        released = true;
    }
    latch_cv.notify_all();
    stopper.join();
    BOOST_CHECK(stop_returned);

    // Nothing is delivered once Stop has returned, whatever is asked of it.
    const size_t delivered = deliveries.Count();
    armed = false;
    cache.MarkDirty(GuiCache::ALL);
    cache.RequestUpdate();
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(deliveries.Count(), delivered);

    // Stop is idempotent.
    cache.Stop();
}

#ifdef DEBUG_LOCKORDER
BOOST_AUTO_TEST_CASE(lock_order_all_paths_at_once)
{
    BOOST_REQUIRE(pwalletMain);

    // The lock-order checker logs each report and goes on (DEBUG_LOCKORDER
    // reports never abort), so count the reports it logs.
    std::atomic<int> reports{0};
    const auto callback = LogInstance().PushBackCallback([&reports](const std::string& line) {
        if (line.find("POTENTIAL DEADLOCK DETECTED") != std::string::npos) ++reports;
    });

    // The production lock shapes: the timer's updates take the real cs_main,
    // the balances part takes the real wallet lock inside it, and the miner part
    // takes the miner-status lock through its accessor. The clock is the real
    // steady clock, with a short interval so every path runs many times.
    GuiCache::Compute compute;
    compute.chain = [](GuiStateSnapshot& snap) { ++snap.height; };
    compute.balances = [](GuiStateSnapshot& snap) {
        LOCK(pwalletMain->cs_wallet);
        snap.has_balances = true;
    };
    compute.miner = [](GuiStateSnapshot& snap) { snap.staking = g_miner_status.StakingActive(); };
    compute.mrc = [](GuiStateSnapshot& snap) {
        LOCK(pwalletMain->cs_wallet);
        snap.has_mrc = true;
    };
    compute.etts = [](GuiStateSnapshot& snap) {
        LOCK(pwalletMain->cs_wallet);
        snap.has_etts = true;
    };
    compute.researcher = [](GuiStateSnapshot& snap) { snap.has_researcher = true; };
    compute.out_of_sync = [] { return false; };
    compute.now_ms = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    compute.run_under_cs_main = [](const std::function<void()>& fn) {
        LOCK(cs_main);
        fn();
    };

    Deliveries deliveries;
    GuiCache cache(std::move(compute), deliveries.Make(), 5ms, 20ms);
    cache.StartTimer();

    // A signal emitted under another lock: its handler only marks.
    CCriticalSection emitter_cs;

    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // SetBestChain's in-scope updates.
    threads.emplace_back([&] {
        while (running) {
            InScope(cache, GuiCache::CHAIN | GuiCache::BALANCES | GuiCache::MRC | GuiCache::ETTS
                               | GuiCache::RESEARCHER);
            std::this_thread::sleep_for(1ms);
        }
    });
    // The GUI's on-demand requests.
    threads.emplace_back([&] {
        while (running) {
            cache.RequestUpdate();
            std::this_thread::sleep_for(3ms);
        }
    });
    // The GUI's reads.
    threads.emplace_back([&] {
        while (running) {
            (void)cache.Current();
        }
    });
    // Wallet and other emitters marking under their own locks.
    threads.emplace_back([&] {
        while (running) {
            {
                LOCK(pwalletMain->cs_wallet);
                cache.MarkDirty(GuiCache::BALANCES | GuiCache::MRC | GuiCache::ETTS);
            }
            {
                LOCK(emitter_cs);
                cache.MarkDirty(GuiCache::MINER | GuiCache::ETTS);
            }
            std::this_thread::sleep_for(1ms);
        }
    });

    std::this_thread::sleep_for(2s);
    running = false;
    for (std::thread& thread : threads) thread.join();
    cache.Stop();

    LogInstance().DeleteCallback(callback);

    BOOST_CHECK_EQUAL(reports.load(), 0);
    BOOST_CHECK_EQUAL(deliveries.DeliveredUnderALock(), 0);

    // Every path ran: the cache updated and delivered many times.
    BOOST_CHECK_GT(cache.Current().sequence, 10U);
    BOOST_CHECK_GT(deliveries.Count(), 10U);
}
#endif // DEBUG_LOCKORDER

BOOST_AUTO_TEST_SUITE_END()
