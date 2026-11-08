// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file gui_cache_tests.cpp
//! \brief The node's GUI state cache (node/gui_cache.h): its rate limit, its
//! timer, the parts it recomputes, and the lock rules it keeps.
//!
//! The cases run the cache on fakes passed through its own constructor
//! (test/gui_cache_fakes.h): a Compute whose parts count their calls, a manual
//! clock that moves only when a case moves it, and a run_under_cs_main that takes
//! the real cs_main and counts the timer's acquisitions. An in-scope update is
//! what SetBestChain does: mark the parts dirty and call UpdateLocked under
//! cs_main. The timer cases use short intervals so its real-time waits stay
//! short; whether an update is due is decided by the manual clock alone, so a
//! case controls it exactly.
//!
//! The cache computes nothing while no GUI is subscribed, so every case but
//! the one about that subscribes first (AddSubscriber), as a GUI's
//! GuiState::handleChanged does.
//!

#include "gridcoin/staking/status.h"
#include "interfaces/gui_state.h"
#include "logging.h"
#include "node/gui_cache.h"
#include "sync.h"
#include "test/gui_cache_fakes.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern CWallet* pwalletMain;

using namespace std::chrono_literals;

namespace {

using node::GuiCache;
using interfaces::GuiStateSnapshot;
using gui_cache_test::Deliveries;
using gui_cache_test::FakeCompute;
using gui_cache_test::InScope;
using gui_cache_test::Quotes;
using gui_cache_test::WaitUntil;

//! Timer-case cadence. Short, so the timer's real-time waits stay short.
constexpr auto INTERVAL = 200ms;
constexpr auto TICK = 300ms;

//! Long enough for the timer to have woken, settled and acted at least once
//! if it was going to.
constexpr auto TIMER_CHANCE = TICK + GuiCache::SETTLE + INTERVAL;

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(gui_cache_tests)

BOOST_AUTO_TEST_CASE(update_in_scope_is_rate_limited)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);
    cache.AddSubscriber();

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
    cache.AddSubscriber();

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
    cache.AddSubscriber();

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
    cache.AddSubscriber();

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
    cache.AddSubscriber();

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
    cache.AddSubscriber();

    compute.out_of_sync = true;
    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();

    // Let the timer deliver that update and go idle on its tick.
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));
    BOOST_CHECK(!deliveries.Last().has_mrc);
    std::this_thread::sleep_for(GuiCache::SETTLE + INTERVAL);

    // The time-based test turns as time passes, with no block and no mark: the
    // next tick sees it and the synced parts arrive.
    compute.out_of_sync = false;
    compute.Advance(INTERVAL);
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.has_mrc; }));

    const GuiStateSnapshot snap = deliveries.Last();
    BOOST_CHECK(!snap.out_of_sync);
    BOOST_CHECK(snap.has_etts);
    BOOST_CHECK(snap.has_researcher);
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 1);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(a_miner_pass_recomputes_etts_only_when_staking_flips)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);
    cache.AddSubscriber();

    InScope(cache, GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 1);

    // Miner passes that leave the staking flag as it was change no input of the
    // estimate: the miner part alone runs.
    for (int i = 0; i < 3; ++i) {
        compute.Advance(4000ms);
        InScope(cache, GuiCache::MINER);
    }
    BOOST_CHECK_EQUAL(compute.miner_calls.load(), 4);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 1);

    // The miner starts staking: the estimate follows in the same update.
    compute.staking = true;
    compute.Advance(4000ms);
    InScope(cache, GuiCache::MINER);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 2);
    BOOST_CHECK(cache.Current().staking);

    // A pass while it keeps staking: no estimate.
    compute.Advance(4000ms);
    InScope(cache, GuiCache::MINER);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 2);

    // It stops: the estimate follows again.
    compute.staking = false;
    compute.Advance(4000ms);
    InScope(cache, GuiCache::MINER);
    BOOST_CHECK_EQUAL(compute.etts_calls.load(), 3);
    BOOST_CHECK(!cache.Current().staking);
}

BOOST_AUTO_TEST_CASE(a_failing_part_is_retried_at_the_interval_while_sync_has_turned)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, TICK);
    cache.AddSubscriber();

    compute.out_of_sync = true;
    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    // The node syncs with no block as an interval passes, and the MRC part, one
    // of those that now appear, fails.
    compute.mrc_throws = true;
    compute.out_of_sync = false;
    compute.Advance(INTERVAL);
    BOOST_REQUIRE(WaitUntil([&] { return compute.mrc_calls.load() >= 1; }));

    // While the clock stands still nothing is retried, however long the timer
    // runs: the failure keeps the turn pending, and a retry waits for the
    // interval like any other update.
    std::this_thread::sleep_for(3 * TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 1);

    // An interval later, one retry.
    compute.Advance(INTERVAL);
    BOOST_REQUIRE(WaitUntil([&] { return compute.mrc_calls.load() >= 2; }));
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 2);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 2);

    // The part recovers: the next due update lands, with the synced parts.
    compute.mrc_throws = false;
    compute.Advance(INTERVAL);
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.has_mrc; }));
    BOOST_CHECK_EQUAL(compute.mrc_calls.load(), 3);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(request_update_forces_an_update_off_the_caller_thread)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms);
    cache.AddSubscriber();

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

BOOST_AUTO_TEST_CASE(nothing_is_computed_without_a_subscriber)
{
    FakeCompute compute;
    Deliveries deliveries;

    // A long tick, so that only a wake can send the timer to work within the
    // case's waits.
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, 30000ms);
    cache.StartTimer();

    // No GUI subscribed: blocks an interval apart compute nothing in scope, and
    // the timer, which the request wakes, takes no cs_main: not for the parts
    // the blocks leave marked, nor for the request, which no one would see
    // answered and which it drops.
    for (int i = 0; i < 3; ++i) {
        compute.Advance(INTERVAL);
        InScope(cache, GuiCache::ALL);
        std::this_thread::sleep_for(TIMER_CHANCE);
    }
    cache.RequestUpdate();
    std::this_thread::sleep_for(TIMER_CHANCE);

    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 0);
    BOOST_CHECK_EQUAL(compute.researcher_calls.load(), 0);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 0);
    BOOST_CHECK_EQUAL(cache.Current().sequence, 0U);
    BOOST_CHECK_EQUAL(deliveries.Count(), 0U);

    // Two GUIs subscribe; only the first subscription says it is the first.
    // The next due update, a block in scope, computes what was left marked
    // meanwhile: all of it.
    BOOST_CHECK(cache.AddSubscriber());
    BOOST_CHECK(!cache.AddSubscriber());
    InScope(cache, GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 1);
    BOOST_CHECK_EQUAL(compute.researcher_calls.load(), 1);
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    // One of the two leaves: computing goes on.
    cache.RemoveSubscriber();
    compute.Advance(INTERVAL);
    InScope(cache, GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 2; }));

    // The last leaves: computing stops, in scope and on the timer, though the
    // block leaves its mark and the interval passes. Counted from the timer at
    // rest: a machine that stalls the update above for longer than the settle
    // lets the timer take cs_main for it once (gui_cache.h, SETTLE).
    cache.RemoveSubscriber();
    std::this_thread::sleep_for(TIMER_CHANCE);
    const int runs = compute.cs_main_runs.load();

    compute.Advance(INTERVAL);
    InScope(cache, GuiCache::CHAIN);
    std::this_thread::sleep_for(TIMER_CHANCE);
    compute.Advance(INTERVAL);
    std::this_thread::sleep_for(TIMER_CHANCE);

    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), runs);
    BOOST_CHECK_EQUAL(cache.Current().sequence, 2U);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(a_quote_is_answered_on_the_timer_inside_the_interval)
{
    FakeCompute compute;
    Deliveries deliveries;
    Quotes quotes;
    GuiCache cache(compute.Make(), deliveries.Make(), 4000ms, 30000ms, quotes.Make());
    cache.AddSubscriber();

    InScope(cache, GuiCache::CHAIN);
    cache.StartTimer();
    BOOST_REQUIRE(deliveries.WaitFor([](const GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    // Inside the interval (the clock does not move), a quote is still computed,
    // at the boost asked, on the timer thread under cs_main, and answered with
    // its request's id. It updates no part of the snapshot.
    cache.RequestMRCQuote(25, 7);
    BOOST_REQUIRE(quotes.WaitForCount(1));

    // And a second one at once: quotes are not held to the interval either.
    cache.RequestMRCQuote(30, 8);
    BOOST_REQUIRE(quotes.WaitForCount(2));

    const std::vector<Quotes::Answer> answers = quotes.Answers();
    BOOST_CHECK_EQUAL(answers[0].request_id, 7U);
    BOOST_CHECK_EQUAL(answers[0].quote.fee, 25);
    BOOST_CHECK_EQUAL(answers[0].thread, gui_cache_test::TIMER_THREAD);
    BOOST_CHECK_EQUAL(answers[1].request_id, 8U);
    BOOST_CHECK_EQUAL(answers[1].quote.fee, 30);
    BOOST_CHECK_EQUAL(compute.quote_calls.load(), 2);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), 2);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 1);
    BOOST_CHECK_EQUAL(cache.Current().sequence, 1U);
    BOOST_CHECK_EQUAL(quotes.AnsweredUnderALock(), 0);

    cache.Stop();
}

BOOST_AUTO_TEST_CASE(sequence_increases_when_height_decreases)
{
    FakeCompute compute;
    Deliveries deliveries;
    GuiCache cache(compute.Make(), deliveries.Make(), INTERVAL, TICK);
    cache.AddSubscriber();

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
    cache.AddSubscriber();

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
    cache.AddSubscriber();

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

    // The production lock shapes: the timer's updates and quotes take the real
    // cs_main, the balances part and the quote take the real wallet lock inside
    // it, and the miner part takes the miner-status lock through its accessor.
    // The clock is the real steady clock, with a short interval so every path
    // runs many times.
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
    compute.mrc_quote = [](CAmount fee_boost, const GuiStateSnapshot&) {
        LOCK(pwalletMain->cs_wallet);
        interfaces::MRCSnapshot quote;
        quote.fee = fee_boost;
        return quote;
    };
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
    Quotes quotes;
    GuiCache cache(std::move(compute), deliveries.Make(), 5ms, 20ms, quotes.Make());
    cache.AddSubscriber();
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
    // The GUI's on-demand requests: updates, and MRC quotes.
    threads.emplace_back([&] {
        uint64_t request_id = 0;
        while (running) {
            cache.RequestUpdate();
            cache.RequestMRCQuote(/*fee_boost=*/1, ++request_id);
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
    BOOST_CHECK_EQUAL(quotes.AnsweredUnderALock(), 0);

    // Every path ran: the cache updated, delivered and quoted many times.
    BOOST_CHECK_GT(cache.Current().sequence, 10U);
    BOOST_CHECK_GT(deliveries.Count(), 10U);
    BOOST_CHECK_GT(quotes.Answers().size(), 10U);
}
#endif // DEBUG_LOCKORDER

BOOST_AUTO_TEST_SUITE_END()
