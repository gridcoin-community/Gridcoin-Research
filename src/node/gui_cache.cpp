// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "node/gui_cache.h"

#include "chain.h"
#include "chainparams.h"
#include "gridcoin/mrc.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/status.h"
#include "interfaces/researcher.h"
#include "logging.h"
#include "util/threadnames.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <exception>
#include <utility>

extern CWallet* pwalletMain;

namespace node {

GuiCache::GuiCache(Compute compute, DeliverFn deliver, std::chrono::milliseconds min_interval,
                   std::chrono::milliseconds tick)
    : m_compute(std::move(compute))
    , m_deliver(std::move(deliver))
    , m_min_interval(min_interval)
    , m_tick(tick)
{
}

GuiCache::~GuiCache()
{
    Stop();
}

void GuiCache::UpdateLocked(bool force)
{
    AssertLockHeld(cs_main);

    const int64_t now = m_compute.now_ms();

    if (!force && !IsDue(now)) {
        return;
    }

    // Read the time-based test once, so the whole snapshot agrees with it. When
    // it has turned since the last update, the chain part (which reports it)
    // and the synced parts (which appear or go) are recomputed whatever was
    // marked. A turn is due like a mark: once the interval has passed (IsDue).
    const bool out_of_sync = m_compute.out_of_sync();

    uint32_t parts = m_dirty.exchange(0);

    if (out_of_sync != m_gated_out_of_sync.load()) {
        parts |= CHAIN | SYNCED_PARTS;
    }

    // Nothing to do only when forced (an update in scope took a request's marks
    // after it was made, and that update's delivery answers it), or when the
    // out-of-sync test turned back between the due check and the read above.
    if (parts == 0) {
        return;
    }

    // Recorded before the parts run, so a part that throws is retried at the
    // cache's pace and not on every block.
    m_last_update_ms.store(now);

    interfaces::GuiStateSnapshot next = Current();
    next.out_of_sync = out_of_sync;

    try {
        if (parts & CHAIN) m_compute.chain(next);
        if (parts & BALANCES) m_compute.balances(next);

        if (parts & MINER) {
            const bool was_staking = next.staking;
            m_compute.miner(next);

            // A miner pass changes no input of the staking-time estimate, but
            // whether the miner stakes is one: recompute it when that flips.
            if (next.staking != was_staking) parts |= ETTS;
        }

        if (out_of_sync) {
            // Meaningless until synced: absent, and not computed.
            next.has_researcher = false;
            next.researcher = {};
            next.has_mrc = false;
            next.mrc = {};
            next.has_etts = false;
            next.etts_seconds = 0;
        } else {
            // The researcher part first: the MRC part reads its eligibility.
            if (parts & RESEARCHER) m_compute.researcher(next);
            if (parts & MRC) m_compute.mrc(next);
            if (parts & ETTS) m_compute.etts(next);
        }
    } catch (...) {
        MarkDirty(parts);
        throw;
    }

    next.sequence += 1;
    m_gated_out_of_sync.store(out_of_sync);

    {
        LOCK(m_snapshot_mutex);
        std::swap(m_snapshot, next);
    }

    m_delivery_pending.store(true);
    Notify();
}

void GuiCache::MarkDirty(uint32_t parts)
{
    // The timer's next wake depends on whether anything is pending, not on what,
    // so only the mark that makes something pending wakes it.
    if (m_dirty.fetch_or(parts) == 0 && parts != 0) {
        Notify();
    }
}

void GuiCache::RequestUpdate()
{
    m_dirty.fetch_or(ALL);
    m_request.store(true);
    Notify();
}

void GuiCache::StartTimer()
{
    LOCK(m_timer_mutex);

    if (m_timer.joinable() || m_stop.load()) {
        return;
    }

    // Nothing may escape a std::thread body: that is std::terminate. TimerLoop
    // contains a failed update or delivery; anything reaching here stops the
    // timer, so say so.
    m_timer = std::thread([this] {
        util::ThreadRename("grc-guistate");

        try {
            TimerLoop();
        } catch (const std::exception& e) {
            LogPrintf("ERROR: GuiCache: the GUI state timer stopped after an unhandled exception: %s. "
                      "The GUI state will not update until restart.", e.what());
        } catch (...) {
            LogPrintf("ERROR: GuiCache: the GUI state timer stopped after an unknown unhandled exception. "
                      "The GUI state will not update until restart.");
        }
    });
}

void GuiCache::Stop()
{
    {
        LOCK(m_wake_mutex);
        m_stop = true;
        m_wake = true;
    }
    m_wake_cv.notify_all();

    LOCK(m_timer_mutex);

    // A delivery that stops the cache runs on the timer, which cannot join
    // itself; the destructor joins it.
    if (m_timer.joinable() && m_timer.get_id() != std::this_thread::get_id()) {
        m_timer.join();
    }
}

interfaces::GuiStateSnapshot GuiCache::Current() const
{
    LOCK(m_snapshot_mutex);
    return m_snapshot;
}

bool GuiCache::IsPending() const
{
    return m_dirty.load() != 0 || SyncTurned();
}

bool GuiCache::IsDue(int64_t now_ms) const
{
    if (!IsPending()) {
        return false;
    }

    const int64_t last = m_last_update_ms.load();

    return last == NEVER || now_ms - last >= m_min_interval.count();
}

bool GuiCache::SyncTurned() const
{
    return m_compute.out_of_sync() != m_gated_out_of_sync.load();
}

std::chrono::milliseconds GuiCache::NextWait() const
{
    if (!IsPending()) {
        return m_tick;
    }

    const int64_t last = m_last_update_ms.load();

    if (last == NEVER) {
        return std::chrono::milliseconds{0};
    }

    // The time left in the interval. Never longer than the tick, so the
    // out-of-sync test is read at least that often.
    const int64_t left = last + m_min_interval.count() - m_compute.now_ms();

    return std::chrono::milliseconds{std::clamp<int64_t>(left, 0, m_tick.count())};
}

void GuiCache::Notify()
{
    {
        LOCK(m_wake_mutex);
        m_wake = true;
    }
    m_wake_cv.notify_one();
}

void GuiCache::Deliver()
{
    if (m_stop.load() || !m_delivery_pending.exchange(false)) {
        return;
    }

    const interfaces::GuiStateSnapshot snapshot = Current();

    try {
        if (m_deliver) m_deliver(snapshot);
    } catch (const std::exception& e) {
        LogPrintf("ERROR: %s: delivering the GUI state failed: %s", __func__, e.what());
    } catch (...) {
        LogPrintf("ERROR: %s: delivering the GUI state failed with an unknown exception", __func__);
    }
}

bool GuiCache::Settle()
{
    const auto until = std::chrono::steady_clock::now() + SETTLE;

    WAIT_LOCK(m_wake_mutex, lock);

    // A wake does not end the wait: it is most likely the in-scope update the
    // wait is for. m_wake stays set, so the loop looks again at once after it.
    while (!m_stop) {
        if (m_wake_cv.wait_until(lock, until) == std::cv_status::timeout) break;
    }

    return !m_stop;
}

void GuiCache::TimerLoop()
{
    while (true) {
        {
            WAIT_LOCK(m_wake_mutex, lock);

            if (!m_wake && !m_stop) {
                m_wake_cv.wait_for(lock, NextWait());
            }

            if (m_stop) return;

            m_wake = false;
        }

        // The wake mutex is released. From here the timer takes it again only to
        // wait, takes cs_main only inside run_under_cs_main, and delivers holding
        // nothing.
        Deliver();

        const bool request = m_request.exchange(false);

        if (!request) {
            if (!IsDue(m_compute.now_ms())) continue;

            // An update SetBestChain is making in its own scope lands in this
            // time, and then there is nothing left for the timer to do.
            if (!Settle()) return;

            Deliver();

            if (!IsDue(m_compute.now_ms())) continue;
        }

        try {
            // run_under_cs_main holds cs_main around this lambda, which the
            // thread-safety analyzer cannot see through std::function. Only a
            // request is forced: anything else is due, and an update that fails
            // is retried when it is due again, an interval after the attempt.
            m_compute.run_under_cs_main([this, request]() NO_THREAD_SAFETY_ANALYSIS {
                AssertLockHeld(cs_main);
                UpdateLocked(request);
            });
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: the GUI state update failed: %s", __func__, e.what());
        } catch (...) {
            LogPrintf("ERROR: %s: the GUI state update failed with an unknown exception", __func__);
        }

        Deliver();
    }
}

GuiCache::Compute MakeProductionCompute()
{
    GuiCache::Compute compute;

    // Every part runs with cs_main held by the caller (GuiCache::UpdateLocked),
    // and all but the miner's need it. The thread-safety analyzer cannot see that
    // through std::function, so each that needs it asserts it instead. Each reads
    // its values the way the interface the GUI calls today reads them.

    // Node::getNumBlocks, getLastBlockTime, getDifficulty, isInitialBlockDownload
    // and getNumBlocksOfPeers, StakingStatus::getNetworkWeight, and
    // PSGTPoolContext::poolStatus with the cache's out-of-sync reading.
    compute.chain = [](interfaces::GuiStateSnapshot& snap) NO_THREAD_SAFETY_ANALYSIS {
        AssertLockHeld(cs_main);

        snap.height = nBestHeight;
        snap.tip_time = pindexBest ? pindexBest->GetBlockTime() : 0;
        snap.difficulty = GRC::GetCurrentDifficulty();
        snap.initial_download = IsInitialBlockDownload();
        snap.peers_height = GetNumBlocksOfPeers();
        snap.net_weight = GRC::GetEstimatedNetworkWeight();

        snap.psgt_status.v15_enabled = IsV15Enabled(nBestHeight);
        snap.psgt_status.out_of_sync = snap.out_of_sync;
        snap.psgt_status.active = snap.psgt_status.v15_enabled && !snap.psgt_status.out_of_sync;
    };

    // Wallet::tryGetBalances, without the try: one pass over the wallet.
    compute.balances = [](interfaces::GuiStateSnapshot& snap) NO_THREAD_SAFETY_ANALYSIS {
        AssertLockHeld(cs_main);

        if (!pwalletMain) {
            snap.has_balances = false;
            snap.balances = {};
            return;
        }

        const CWallet::Balances totals = pwalletMain->GetBalances();
        snap.balances.balance = totals.balance;
        snap.balances.stake = totals.stake;
        snap.balances.unconfirmed_balance = totals.unconfirmed;
        snap.balances.immature_balance = totals.immature;
        snap.has_balances = true;
    };

    // StakingStatus::isStaking, getCoinWeight and getErrors: the miner-status
    // lock, or an atomic. No cs_main.
    compute.miner = [](interfaces::GuiStateSnapshot& snap) {
        snap.staking = g_miner_status.StakingActive();
        snap.coin_weight = g_miner_status.GetSearchReport().CoinWeight();
        snap.miner_errors = g_miner_status.FormatErrors();
    };

    // ResearcherContext::snapshot itself; its cs_main re-enters the one held.
    compute.researcher = [](interfaces::GuiStateSnapshot& snap) NO_THREAD_SAFETY_ANALYSIS {
        AssertLockHeld(cs_main);

        if (!pwalletMain) {
            snap.has_researcher = false;
            snap.researcher = {};
            return;
        }

        snap.researcher = interfaces::MakeResearcherContext(pwalletMain)->snapshot();
        snap.has_researcher = true;
    };

    // MRC::snapshot's body, unboosted and unsigned: showing a quote needs no
    // signature, and a submit builds and signs its own claim. Researcher
    // eligibility is derived as MRCModel::refresh derives it, from the researcher
    // part, which the cache runs first.
    compute.mrc = [](interfaces::GuiStateSnapshot& snap) NO_THREAD_SAFETY_ANALYSIS {
        AssertLockHeld(cs_main);

        if (!pwalletMain) {
            snap.has_mrc = false;
            snap.mrc = {};
            return;
        }

        const bool researcher_eligible = snap.has_researcher
                && snap.researcher.has_active_beacon
                && !snap.researcher.configured_for_noncruncher_mode
                && !snap.researcher.detected_pool_mode;

        snap.mrc = GRC::BuildMRCSnapshot(pwalletMain, /*fee_boost=*/0, /*no_sign=*/true, researcher_eligible);
        snap.has_mrc = true;
    };

    // StakingStatus::tryGetEstimatedTimeToStake, without the try. Seconds.
    compute.etts = [](interfaces::GuiStateSnapshot& snap) NO_THREAD_SAFETY_ANALYSIS {
        AssertLockHeld(cs_main);

        if (!pwalletMain) {
            snap.has_etts = false;
            snap.etts_seconds = 0;
            return;
        }

        snap.etts_seconds = GRC::GetEstimatedTimetoStake();
        snap.has_etts = true;
    };

    compute.out_of_sync = [] { return OutOfSyncByAge(); };

    compute.now_ms = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch()).count();
    };

    compute.run_under_cs_main = [](const std::function<void()>& fn) {
        LOCK(cs_main);
        fn();
    };

    return compute;
}

namespace {
//! Owns the node's cache. Written only by StartGuiCache().
std::unique_ptr<GuiCache> g_gui_cache;

//! What GetGuiCache() returns: published after the timer starts, cleared before
//! it stops.
std::atomic<GuiCache*> g_gui_cache_handle{nullptr};
} // anonymous namespace

GuiCache* GetGuiCache()
{
    return g_gui_cache_handle.load(std::memory_order_acquire);
}

void StartGuiCache(std::unique_ptr<GuiCache> cache)
{
    StopGuiCache();

    g_gui_cache = std::move(cache);
    g_gui_cache->StartTimer();
    g_gui_cache_handle.store(g_gui_cache.get(), std::memory_order_release);
}

void StopGuiCache()
{
    GuiCache* cache = g_gui_cache_handle.exchange(nullptr, std::memory_order_acq_rel);

    if (cache) {
        cache->Stop();
    }
}

} // namespace node
