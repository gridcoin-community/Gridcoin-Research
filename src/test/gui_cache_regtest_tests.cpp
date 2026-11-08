// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file gui_cache_regtest_tests.cpp
//! \brief The GUI state cache on a real regtest chain, with the production
//! compute: SetBestChain keeps it at the tip in its own scope and never waits on
//! the cache's timer.
//!
//! A separate suite from gui_cache_tests because the chain fixture attaches per
//! suite (test/chain_setup.h, LIFETIME).
//!

#include "chain.h"
#include "interfaces/gui_state.h"
#include "node/gui_cache.h"
#include "primitives/block.h"
#include "sync.h"
#include "test/chain_setup.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace {

//! The bound on a wait that should end well before it. It only stops a broken
//! cache from hanging the run.
constexpr auto WAIT_LIMIT = 30s;

//! The case's update interval: short, so it mines an interval apart quickly.
constexpr auto INTERVAL = 500ms;

//!
//! \brief Holds the cache's deliveries at a gate while it is shut. A held
//! delivery keeps the timer thread waiting inside it, holding no lock, which is
//! how a case shows that connecting blocks never waits for the timer.
//!
class GatedDeliveries
{
public:
    node::GuiCache::DeliverFn Make()
    {
        return [this](const interfaces::GuiStateSnapshot& snap) {
            WAIT_LOCK(m_mutex, lock);
            m_held = true;
            m_cv.notify_all();
            while (!m_open) m_cv.wait(lock);
            m_heights.push_back(snap.height);
            m_cv.notify_all();
        };
    }

    void Open()
    {
        {
            LOCK(m_mutex);
            m_open = true;
        }
        m_cv.notify_all();
    }

    //! Wait until a delivery is waiting at the gate.
    bool WaitUntilHeld()
    {
        const auto deadline = std::chrono::steady_clock::now() + WAIT_LIMIT;
        WAIT_LOCK(m_mutex, lock);
        while (!m_held) {
            if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout) return false;
        }
        return true;
    }

    //! Wait until a snapshot at \p height has passed the gate.
    bool WaitForHeight(int height, std::chrono::steady_clock::time_point deadline)
    {
        WAIT_LOCK(m_mutex, lock);
        while (true) {
            for (int delivered : m_heights) {
                if (delivered == height) return true;
            }
            if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout) return false;
        }
    }

    size_t Passed()
    {
        LOCK(m_mutex);
        return m_heights.size();
    }

private:
    Mutex m_mutex;
    std::condition_variable m_cv;
    bool m_open GUARDED_BY(m_mutex){false};
    bool m_held GUARDED_BY(m_mutex){false};
    std::vector<int> m_heights GUARDED_BY(m_mutex);
};

//!
//! \brief Publishes a cache as the node's for a case. At any exit it opens the
//! gate, so the timer can finish what it holds, then unpublishes and stops it.
//!
class PublishedCache
{
public:
    PublishedCache(std::unique_ptr<node::GuiCache> cache, GatedDeliveries& gate) : m_gate(gate)
    {
        node::StartGuiCache(std::move(cache));
    }

    ~PublishedCache()
    {
        m_gate.Open();
        node::StopGuiCache();
    }

    PublishedCache(const PublishedCache&) = delete;
    PublishedCache& operator=(const PublishedCache&) = delete;

private:
    GatedDeliveries& m_gate;
};

//! The tip, and the cache's height, right after one block was connected.
struct AfterBlock
{
    int tip = -1;
    int cached = -1;
};

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(gui_cache_regtest_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

BOOST_AUTO_TEST_CASE(setbestchain_keeps_the_cache_at_the_tip_without_waiting_on_the_timer)
{
    GatedDeliveries gate;
    {
        // A GUI is subscribed from the start, as the monolithic GUI is: the
        // cache computes nothing without one.
        auto subscribed = std::make_unique<node::GuiCache>(node::MakeProductionCompute(), gate.Make(),
                                                           INTERVAL, 30000ms);
        subscribed->AddSubscriber();
        PublishedCache published(std::move(subscribed), gate);

        node::GuiCache* cache = node::GetGuiCache();
        BOOST_REQUIRE(cache);

        // The timer's first update reaches the shut gate and waits there.
        BOOST_REQUIRE(gate.WaitUntilHeld());

        // Five blocks through the production path, mined on another thread so
        // that a SetBestChain waiting on the timer would show as a timeout here
        // rather than a hung run. The first four come an interval apart, so each
        // one's update is due and is made in SetBestChain's own scope. The fifth
        // follows the fourth at once, so its update is normally not due, and
        // the timer has to catch it.
        auto mined = std::async(std::launch::async, [cache] {
            std::pair<std::string, std::vector<AfterBlock>> result;

            for (int i = 0; i < 5; ++i) {
                if (i < 4) std::this_thread::sleep_for(INTERVAL);

                CBlock block;
                if (!grc_test::CreateAndProcessBlock(block, result.first)) return result;

                LOCK(cs_main);
                result.second.push_back(AfterBlock{nBestHeight, cache->Current().height});
            }

            return result;
        });

        const bool finished = mined.wait_for(WAIT_LIMIT) == std::future_status::ready;
        if (!finished) gate.Open();
        BOOST_REQUIRE_MESSAGE(finished, "connecting blocks waited on the GUI state cache's timer");

        const auto last_block = std::chrono::steady_clock::now();
        const auto [err, after] = mined.get();
        BOOST_REQUIRE_MESSAGE(err.empty(), err);
        BOOST_REQUIRE_EQUAL(after.size(), 5U);

        // The first four updates were made in scope while the timer waited at
        // the gate: the cache followed the tip with nothing delivered.
        for (size_t i = 0; i < 4; ++i) {
            BOOST_CHECK_EQUAL(after[i].cached, after[i].tip);
        }
        BOOST_CHECK_EQUAL(gate.Passed(), 0U);

        // Once the timer can move, the tip reaches the GUI within one interval
        // of the last block. The bound adds the timer's settle (node/gui_cache.h)
        // and slack for a loaded machine.
        const int tip = after.back().tip;
        gate.Open();
        BOOST_CHECK(gate.WaitForHeight(tip, last_block + INTERVAL + node::GuiCache::SETTLE + 2s));
        BOOST_CHECK_EQUAL(cache->Current().height, tip);
    }

    // Stopped and unpublished: SetBestChain no longer reaches it.
    BOOST_CHECK(node::GetGuiCache() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
