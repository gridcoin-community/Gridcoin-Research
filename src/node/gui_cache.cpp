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
#include "interfaces/wallet.h"
#include "logging.h"
#include "util/threadnames.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

extern CWallet* pwalletMain;

namespace node {

GuiScanThread::GuiScanThread(std::string name)
    : m_name(std::move(name))
{
}

GuiScanThread::~GuiScanThread()
{
    RequestStop();
    Join();
}

void GuiScanThread::Start()
{
    LOCK(m_thread_mutex);

    if (m_thread.joinable()) return;

    // m_stopping is not read here, so this lock is held alone. A Start after
    // RequestStop() starts a thread that runs no job: Post has refused every
    // job since, RequestStop() dropped the rest, and Loop() returns on
    // m_stopping before it takes one. The next Join() joins it.

    // As the timer's: nothing may escape a std::thread body. Each job contains
    // its own failures; anything reaching here stops this thread, so say so.
    m_thread = std::thread([this] {
        util::ThreadRename(std::string{m_name});

        try {
            Loop();
        } catch (const std::exception& e) {
            LogPrintf("ERROR: GuiScanThread: %s stopped after an unhandled exception: %s. "
                      "It will not run again until restart.", m_name, e.what());
        } catch (...) {
            LogPrintf("ERROR: GuiScanThread: %s stopped after an unknown unhandled exception. "
                      "It will not run again until restart.", m_name);
        }

        // A thread that has stopped takes no more jobs: refuse them, as after
        // RequestStop(), so a caller learns it at once. No job lets an
        // exception reach here; this is the guard if one ever does.
        {
            std::deque<std::function<void()>> dropped;
            {
                LOCK(m_mutex);
                m_stopping = true;
                dropped.swap(m_jobs);
            }
        }
    });
}

bool GuiScanThread::Post(std::function<void()> job)
{
    {
        LOCK(m_mutex);
        if (m_stopping) return false;
        m_jobs.push_back(std::move(job));
    }
    m_cv.notify_one();
    return true;
}

void GuiScanThread::RequestStop()
{
    // The dropped jobs are destroyed once the lock is released, so no capture's
    // destructor runs under it.
    std::deque<std::function<void()>> dropped;
    {
        LOCK(m_mutex);
        m_stopping = true;
        dropped.swap(m_jobs);
    }
    m_cv.notify_all();
}

void GuiScanThread::Join()
{
    LOCK(m_thread_mutex);

    // A job that stops the cache runs on this thread, which cannot join
    // itself; the destructor joins it.
    if (m_thread.joinable() && m_thread.get_id() != std::this_thread::get_id()) {
        m_thread.join();
    }
}

std::size_t GuiScanThread::Queued() const
{
    LOCK(m_mutex);
    return m_jobs.size();
}

void GuiScanThread::Loop()
{
    while (true) {
        std::function<void()> job;

        {
            WAIT_LOCK(m_mutex, lock);

            while (m_jobs.empty() && !m_stopping) m_cv.wait(lock);

            if (m_stopping) return;

            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }

        // Holding no lock: the job takes what it needs.
        job();
    }
}

GuiCache::GuiCache(Compute compute, DeliverFn deliver, std::chrono::milliseconds min_interval,
                   std::chrono::milliseconds tick, QuoteFn deliver_quote, UnbookedFn deliver_unbooked)
    : m_compute(std::move(compute))
    , m_deliver(std::move(deliver))
    , m_min_interval(min_interval)
    , m_tick(tick)
    , m_deliver_quote(std::move(deliver_quote))
    , m_deliver_unbooked(std::move(deliver_unbooked))
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

    // With no GUI subscribed nobody would see the update, so none is made, not
    // even a forced one; the marks stay for the first subscriber's.
    if (!HasSubscribers() || (!force && !IsDue(now))) {
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

            // A miner pass is signalled about every eight seconds whether or not
            // anything changed. When it is all that was marked, and it left the
            // snapshot as it was, there is nothing to swap or to deliver: the
            // work is dropped. Never for a forced update, which answers a
            // request, and never for a snapshot not yet computed. A turn of the
            // out-of-sync test adds parts above, so it is never dropped here.
            if (!force && parts == MINER && m_gated_out_of_sync.load() == out_of_sync) {
                const interfaces::GuiStateSnapshot kept = Current();

                if (kept.sequence != 0 && kept.out_of_sync == next.out_of_sync
                    && kept.staking == next.staking
                    && kept.coin_weight == next.coin_weight
                    && kept.miner_errors == next.miner_errors) {
                    return;
                }
            }
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

bool GuiCache::AddSubscriber()
{
    // Wakes nothing: the first subscriber's request (RequestUpdate) does, after
    // this count, so the timer it wakes sees both and makes one forced update.
    return m_subscribers.fetch_add(1) == 0;
}

void GuiCache::RemoveSubscriber()
{
    m_subscribers.fetch_sub(1);
}

void GuiCache::RequestMRCQuote(CAmount fee_boost, uint64_t request_id)
{
    {
        LOCK(m_wake_mutex);
        m_quote = QuoteRequest{fee_boost, request_id};
        m_wake = true;
    }
    m_wake_cv.notify_one();
}

void GuiCache::RequestUnbookedReceiveAddresses(uint64_t request_id)
{
    // A walk takes every request queued when it starts, so a walk is posted
    // only when none is queued: a flood keeps at most one queued and one
    // running. Once the cache has stopped, the post is refused and the
    // request gets no answer.
    bool post;
    {
        LOCK(m_unbooked_mutex);
        m_unbooked.push_back(request_id);
        post = !std::exchange(m_unbooked_posted, true);
    }

    if (post && !m_picker_scan.Post([this] { ServeUnbooked(); })) {
        LOCK(m_unbooked_mutex);
        m_unbooked_posted = false;
    }
}

std::size_t GuiCache::QueuedPickerJobsForTest() const
{
    return m_picker_scan.Queued();
}

bool GuiCache::RequestCoinReload(CoinJob job)
{
    return m_coin_scan.Post([this, job = std::move(job)] { RunCoinJob(job); });
}

void GuiCache::StartTimer()
{
    // The scan threads first, before the timer's lock, so no lock of the
    // cache's is held while another is taken. One started after Stop()
    // runs no job: its Loop() returns on m_stopping.
    m_picker_scan.Start();
    m_coin_scan.Start();

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

    // Both scan threads are asked first, so neither starts a job while the
    // other is joined. Each then ends only the job it has started.
    m_picker_scan.RequestStop();
    m_coin_scan.RequestStop();
    m_picker_scan.Join();
    m_coin_scan.Join();

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

bool GuiCache::HasSubscribers() const
{
    return m_subscribers.load() > 0;
}

bool GuiCache::IsPending() const
{
    return HasSubscribers() && (m_dirty.load() != 0 || SyncTurned());
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

interfaces::MRCSnapshot GuiCache::QuoteLocked(CAmount fee_boost)
{
    AssertLockHeld(cs_main);

    try {
        interfaces::GuiStateSnapshot current = Current();

        // The cache's researcher part gives the eligibility while the cache keeps
        // it: a GUI subscribed and the snapshot computed. Otherwise the part is
        // computed here, for this quote alone, behind the same sync gate, and
        // nothing is swapped into the cache.
        if (!HasSubscribers() || current.sequence == 0) {
            current.out_of_sync = m_compute.out_of_sync();
            current.has_researcher = false;
            current.researcher = {};

            if (!current.out_of_sync) m_compute.researcher(current);
        }

        return m_compute.mrc_quote(fee_boost, current);
    } catch (const std::exception& e) {
        LogPrintf("ERROR: %s: the MRC quote failed: %s", __func__, e.what());
        return FailedQuote(e.what());
    } catch (...) {
        LogPrintf("ERROR: %s: the MRC quote failed with an unknown exception", __func__);
        return FailedQuote("unknown exception");
    }
}

void GuiCache::AnswerQuote(uint64_t request_id, const interfaces::MRCSnapshot& answer)
{
    if (m_stop.load()) return;

    try {
        if (m_deliver_quote) m_deliver_quote(request_id, answer);
    } catch (const std::exception& e) {
        LogPrintf("ERROR: %s: delivering the MRC quote failed: %s", __func__, e.what());
    } catch (...) {
        LogPrintf("ERROR: %s: delivering the MRC quote failed with an unknown exception", __func__);
    }
}

void GuiCache::ServeUnbooked()
{
    std::vector<uint64_t> request_ids;
    {
        LOCK(m_unbooked_mutex);
        m_unbooked_posted = false;
        request_ids.swap(m_unbooked);
    }

    if (request_ids.empty()) return;

    std::vector<std::string> addresses;
    std::string error;

    // The walk takes cs_main and cs_wallet itself, so this thread holds no lock
    // around it, and nothing on the timer waits in line behind it.
    try {
        if (!m_compute.unbooked) throw std::runtime_error("this cache does not compute them");

        addresses = m_compute.unbooked();
    } catch (const std::exception& e) {
        addresses.clear();
        error = e.what();
        if (error.empty()) error = "the walk failed";
    } catch (...) {
        addresses.clear();
        error = "unknown exception";
    }

    if (!error.empty()) {
        LogPrintf("ERROR: %s: the unbooked receive addresses could not be listed: %s", __func__, error);
    }

    // Holding no lock, as a quote is answered. One walk serves every request
    // taken with it.
    for (const uint64_t request_id : request_ids) {
        if (m_stop.load()) return;

        try {
            if (m_deliver_unbooked) m_deliver_unbooked(request_id, addresses, error);
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: delivering the unbooked receive addresses failed: %s", __func__, e.what());
        } catch (...) {
            LogPrintf("ERROR: %s: delivering the unbooked receive addresses failed with an unknown exception",
                      __func__);
        }
    }
}

void GuiCache::RunCoinJob(const CoinJob& job)
{
    // The rebuild takes cs_main and cs_wallet itself and releases them before
    // the job returns. Its reference is dropped here, after that, so a source
    // whose last owner has gone is destroyed on this thread holding no lock.
    std::shared_ptr<void> keepalive;

    try {
        keepalive = job();
    } catch (const std::exception& e) {
        LogPrintf("ERROR: %s: a coin-list rebuild failed: %s", __func__, e.what());
    } catch (...) {
        LogPrintf("ERROR: %s: a coin-list rebuild failed with an unknown exception", __func__);
    }
}

interfaces::MRCSnapshot GuiCache::FailedQuote(const std::string& what)
{
    interfaces::MRCSnapshot answer;
    answer.create_error = true;
    answer.create_error_msg = "The MRC figures could not be computed: " + what;

    return answer;
}

bool GuiCache::TakeRequest()
{
    return m_request.exchange(false) && HasSubscribers();
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
        std::optional<QuoteRequest> quote;

        {
            WAIT_LOCK(m_wake_mutex, lock);

            if (!m_wake && !m_stop) {
                m_wake_cv.wait_for(lock, NextWait());
            }

            if (m_stop) return;

            m_wake = false;
            quote = std::exchange(m_quote, std::nullopt);
        }

        // The wake mutex is released. From here the timer takes it again only to
        // wait, takes cs_main only inside run_under_cs_main, and delivers holding
        // nothing.
        Deliver();

        // A request no GUI is subscribed to see answered is dropped.
        bool request = TakeRequest();

        // With nothing asked for, the timer acts only on an update it finds due,
        // and waits SETTLE first: an update SetBestChain is making in its own
        // scope lands in that time, and then there is nothing left to do. A
        // request made meanwhile (the first subscriber's, say) is answered by
        // this same update, forced, so it costs no second pass. A quote made
        // meanwhile has set the wake, and is served on the next turn.
        if (!request && !quote) {
            if (!IsDue(m_compute.now_ms())) continue;

            if (!Settle()) return;

            Deliver();

            request = TakeRequest();

            if (!request && !IsDue(m_compute.now_ms())) continue;
        }

        // One cs_main hold for the update and the quote, the update first, so a
        // quote reads the snapshot it leaves. The update is forced for a request,
        // and for a quote while the snapshot was never computed; otherwise it is
        // made only if due, and one that fails is retried when it is due again,
        // an interval after the attempt. A quote is answered whatever fails
        // here, so the GUI's wait for it always ends: QuoteLocked answers its
        // own failure, and an exception that reaches the catch below came before
        // the quote was computed.
        interfaces::MRCSnapshot answer;

        try {
            // run_under_cs_main holds cs_main around this lambda, which the
            // thread-safety analyzer cannot see through std::function.
            m_compute.run_under_cs_main([&]() NO_THREAD_SAFETY_ANALYSIS {
                AssertLockHeld(cs_main);

                try {
                    UpdateLocked(request || (quote && Current().sequence == 0));
                } catch (const std::exception& e) {
                    LogPrintf("ERROR: TimerLoop: the GUI state update failed: %s", e.what());
                } catch (...) {
                    LogPrintf("ERROR: TimerLoop: the GUI state update failed with an unknown exception");
                }

                if (quote) answer = QuoteLocked(quote->fee_boost);
            });
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: the GUI state update failed: %s", __func__, e.what());
            if (quote) answer = FailedQuote(e.what());
        } catch (...) {
            LogPrintf("ERROR: %s: the GUI state update failed with an unknown exception", __func__);
            if (quote) answer = FailedQuote("unknown exception");
        }

        Deliver();

        if (quote) AnswerQuote(quote->request_id, answer);
    }
}

namespace {
//! Whether the researcher part of \p snap makes the wallet eligible for an MRC,
//! derived as MRCModel::refresh derives it.
bool ResearcherEligible(const interfaces::GuiStateSnapshot& snap)
{
    return snap.has_researcher
        && snap.researcher.has_active_beacon
        && !snap.researcher.configured_for_noncruncher_mode
        && !snap.researcher.detected_pool_mode;
}
} // anonymous namespace

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

        snap.mrc = GRC::BuildMRCSnapshot(pwalletMain, /*fee_boost=*/0, /*no_sign=*/true, ResearcherEligible(snap));
        snap.has_mrc = true;
    };

    // The same trial claim at a GUI's fee boost (MRC::snapshot's boosted rerun),
    // unsigned and quiet as the MRC part's. Eligibility comes from the cache's
    // researcher part, as the MRC page takes it from its researcher model. Out of
    // sync, BuildMRCSnapshot says so and computes nothing more.
    compute.mrc_quote = [](CAmount fee_boost, const interfaces::GuiStateSnapshot& current) NO_THREAD_SAFETY_ANALYSIS {
        AssertLockHeld(cs_main);

        if (!pwalletMain) return interfaces::MRCSnapshot{};

        return GRC::BuildMRCSnapshot(pwalletMain, fee_boost, /*no_sign=*/true, ResearcherEligible(current));
    };

    // The "add existing receive address" picker's list, on the cache's picker
    // thread: Wallet's getUnbookedReceiveAddresses itself, whose LOCK2 takes
    // cs_main and cs_wallet for the walk. Shutdown() stops the cache before it
    // deletes pwalletMain.
    compute.unbooked = []() {
        if (!pwalletMain) throw std::runtime_error("no wallet is loaded");

        return interfaces::MakeWallet(pwalletMain)->getUnbookedReceiveAddresses();
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

bool ScheduleCoinReload(GuiCache::CoinJob job)
{
    GuiCache* const cache = GetGuiCache();

    return cache && cache->RequestCoinReload(std::move(job));
}

} // namespace node
