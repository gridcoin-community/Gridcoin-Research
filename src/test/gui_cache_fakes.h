// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_GUI_CACHE_FAKES_H
#define GRIDCOIN_TEST_GUI_CACHE_FAKES_H

//!
//! \file gui_cache_fakes.h
//! \brief Fakes for the node's GUI state cache (node/gui_cache.h), shared by its
//! own cases (gui_cache_tests) and the GuiState interface's (interfaces_tests).
//!
//! FakeCompute is a Compute whose parts count their calls, on a manual clock that
//! moves only when a case moves it, with a run_under_cs_main that takes the real
//! cs_main and counts the timer's acquisitions. Deliveries and Quotes record what
//! the cache delivers, and on which thread. InScope is what SetBestChain does:
//! mark the parts dirty and call UpdateLocked under cs_main.
//!

#include "amount.h"
#include "interfaces/gui_state.h"
#include "interfaces/mrc.h"
#include "node/gui_cache.h"
#include "node/ui_interface.h"
#include "sync.h"
#include "util/threadnames.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace gui_cache_test {

using namespace std::chrono_literals;

//! The bound on a wait that should end almost at once. It only stops a broken
//! cache from hanging the run.
constexpr auto WAIT_LIMIT = 10s;

//! The internal name of the cache's timer thread.
constexpr char TIMER_THREAD[] = "grc-guistate";

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
    std::atomic<int> quote_calls{0};

    //! Calls of run_under_cs_main: only the timer makes them.
    std::atomic<int> cs_main_runs{0};

    //! The thread the chain part last ran on.
    std::atomic<std::thread::id> chain_thread{};

    //! What the chain and miner parts report.
    std::atomic<int> height{0};
    std::atomic<bool> staking{false};
    std::atomic<double> coin_weight{0};

    //! The MRC part throws while set.
    std::atomic<bool> mrc_throws{false};

    //! The quote throws while set.
    std::atomic<bool> quote_throws{false};

    //! What the last quote read of the snapshot it was handed.
    std::atomic<uint64_t> quote_saw_sequence{0};
    std::atomic<bool> quote_saw_researcher{false};

    std::atomic<bool> out_of_sync{false};
    std::atomic<int64_t> clock_ms{1000000};

    //! Run inside the chain part, with cs_main held. Set before the cache is
    //! constructed.
    std::function<void()> chain_hook;

    node::GuiCache::Compute Make()
    {
        node::GuiCache::Compute compute;

        compute.chain = [this](interfaces::GuiStateSnapshot& snap) {
            ++chain_calls;
            chain_thread = std::this_thread::get_id();
            snap.height = height.load();
            if (chain_hook) chain_hook();
        };
        compute.balances = [this](interfaces::GuiStateSnapshot& snap) {
            ++balances_calls;
            snap.has_balances = true;
        };
        compute.miner = [this](interfaces::GuiStateSnapshot& snap) {
            ++miner_calls;
            snap.staking = staking.load();
            snap.coin_weight = coin_weight.load();
        };
        compute.mrc = [this](interfaces::GuiStateSnapshot& snap) {
            ++mrc_calls;
            if (mrc_throws) throw std::runtime_error("the MRC part failed");
            snap.has_mrc = true;
        };
        compute.etts = [this](interfaces::GuiStateSnapshot& snap) {
            ++etts_calls;
            snap.has_etts = true;
        };
        compute.researcher = [this](interfaces::GuiStateSnapshot& snap) {
            ++researcher_calls;
            snap.has_researcher = true;
        };
        // The quote reports the boost it was asked for as its fee.
        compute.mrc_quote = [this](CAmount fee_boost, const interfaces::GuiStateSnapshot& current) {
            ++quote_calls;
            quote_saw_sequence = current.sequence;
            quote_saw_researcher = current.has_researcher;
            if (quote_throws) throw std::runtime_error("the quote failed");
            interfaces::MRCSnapshot quote;
            quote.fee = fee_boost;
            return quote;
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
//! \brief Records what the cache delivers, the thread that delivered it, and
//! whether that thread held any lock while it did. Its Make() serves as the
//! cache's DeliverFn and as a GuiState::ChangedFn.
//!
class Deliveries
{
public:
    std::function<void(const interfaces::GuiStateSnapshot&)> Make()
    {
        return [this](const interfaces::GuiStateSnapshot& snap) {
            // Checked before this recorder takes its own lock. Without
            // DEBUG_LOCKORDER the check always passes.
            const bool no_lock_held = LockStackEmpty();
            {
                LOCK(m_mutex);
                m_delivered.push_back(snap);
                m_threads.push_back(util::ThreadGetInternalName());
                if (!no_lock_held) ++m_delivered_under_a_lock;
            }
            m_cv.notify_all();
        };
    }

    //! Wait until a delivered snapshot satisfies \p pred.
    bool WaitFor(const std::function<bool(const interfaces::GuiStateSnapshot&)>& pred)
    {
        const auto deadline = std::chrono::steady_clock::now() + WAIT_LIMIT;
        WAIT_LOCK(m_mutex, lock);
        while (true) {
            for (const interfaces::GuiStateSnapshot& snap : m_delivered) {
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

    interfaces::GuiStateSnapshot Last()
    {
        LOCK(m_mutex);
        return m_delivered.empty() ? interfaces::GuiStateSnapshot{} : m_delivered.back();
    }

    //! The internal name of the thread of each delivery, in order.
    std::vector<std::string> Threads()
    {
        LOCK(m_mutex);
        return m_threads;
    }

    int DeliveredUnderALock()
    {
        LOCK(m_mutex);
        return m_delivered_under_a_lock;
    }

private:
    Mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<interfaces::GuiStateSnapshot> m_delivered GUARDED_BY(m_mutex);
    std::vector<std::string> m_threads GUARDED_BY(m_mutex);
    int m_delivered_under_a_lock GUARDED_BY(m_mutex){0};
};

//!
//! \brief Records the MRC quotes the cache answers, the thread that answered
//! each, and whether that thread held any lock while it did. Its Make() serves
//! as the cache's QuoteFn and as a GuiState::MRCQuoteFn.
//!
class Quotes
{
public:
    struct Answer {
        uint64_t request_id = 0;
        interfaces::MRCSnapshot quote;
        std::string thread;
    };

    std::function<void(uint64_t, const interfaces::MRCSnapshot&)> Make()
    {
        return [this](uint64_t request_id, const interfaces::MRCSnapshot& quote) {
            const bool no_lock_held = LockStackEmpty();
            {
                LOCK(m_mutex);
                m_answers.push_back(Answer{request_id, quote, util::ThreadGetInternalName()});
                if (!no_lock_held) ++m_answered_under_a_lock;
            }
            m_cv.notify_all();
        };
    }

    int AnsweredUnderALock()
    {
        LOCK(m_mutex);
        return m_answered_under_a_lock;
    }

    //! Wait until at least \p count answers have arrived.
    bool WaitForCount(size_t count)
    {
        const auto deadline = std::chrono::steady_clock::now() + WAIT_LIMIT;
        WAIT_LOCK(m_mutex, lock);
        while (m_answers.size() < count) {
            if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout) return false;
        }
        return true;
    }

    std::vector<Answer> Answers()
    {
        LOCK(m_mutex);
        return m_answers;
    }

private:
    Mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<Answer> m_answers GUARDED_BY(m_mutex);
    int m_answered_under_a_lock GUARDED_BY(m_mutex){0};
};

//!
//! \brief Publishes a cache on a FakeCompute as the node's for a case,
//! delivering through the uiInterface signals as init.cpp's does, and
//! unpublishes and stops it at any exit. Declare the recorders its deliveries
//! reach before it, so its timer is joined before they go.
//!
class PublishedFakeCache
{
public:
    PublishedFakeCache(FakeCompute& compute, std::chrono::milliseconds interval, std::chrono::milliseconds tick)
    {
        node::StartGuiCache(std::make_unique<node::GuiCache>(
            compute.Make(),
            [](const interfaces::GuiStateSnapshot& snapshot) { uiInterface.GuiStateChanged(snapshot); },
            interval, tick,
            [](uint64_t request_id, const interfaces::MRCSnapshot& quote) {
                uiInterface.GuiStateMRCQuote(request_id, quote);
            }));
    }

    ~PublishedFakeCache() { node::StopGuiCache(); }

    PublishedFakeCache(const PublishedFakeCache&) = delete;
    PublishedFakeCache& operator=(const PublishedFakeCache&) = delete;

    node::GuiCache& Cache() { return *node::GetGuiCache(); }
};

//! What SetBestChain does: mark the parts and update under the cs_main it holds.
inline void InScope(node::GuiCache& cache, uint32_t parts)
{
    LOCK(cs_main);
    cache.MarkDirty(parts);
    cache.UpdateLocked(false);
}

//! A Boost check cannot run on another thread, so the cases wait for flags
//! the other threads set.
inline bool WaitUntil(const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + WAIT_LIMIT;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(5ms);
    }
    return true;
}

} // namespace gui_cache_test

#endif // GRIDCOIN_TEST_GUI_CACHE_FAKES_H
