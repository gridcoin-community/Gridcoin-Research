// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "validationinterface.h"

#include "scheduler.h"
#include "sync.h"

#include <boost/signals2/signal.hpp>

#include <cassert>
#include <future>
#include <unordered_map>
#include <utility>

//! Per-subscriber boost::signals2 connections, scoped so that destroying this
//! struct (on Unregister) automatically disconnects the subscriber.
struct ValidationInterfaceConnections {
    boost::signals2::scoped_connection UpdatedBlockTip;
    boost::signals2::scoped_connection TransactionAddedToMempool;
    boost::signals2::scoped_connection TransactionRemovedFromMempool;
    boost::signals2::scoped_connection TransactionReplacedInMempool;
    boost::signals2::scoped_connection BlockConnected;
    boost::signals2::scoped_connection BlockDisconnected;
    boost::signals2::scoped_connection ChainStateFlushed;
};

struct MainSignalsInstance {
private:
    Mutex m_mutex;
    //! List of subscribers (kept so we can disconnect them on Unregister/Clear).
    std::unordered_map<CValidationInterface*, ValidationInterfaceConnections> m_map GUARDED_BY(m_mutex);

public:
    boost::signals2::signal<void (const CBlockIndex*, const CBlockIndex*, bool fInitialDownload)> UpdatedBlockTip;
    boost::signals2::signal<void (const CTransactionRef&)> TransactionAddedToMempool;
    boost::signals2::signal<void (const CTransactionRef&, MemPoolRemovalReason)> TransactionRemovedFromMempool;
    boost::signals2::signal<void (const CTransactionRef&)> TransactionReplacedInMempool;
    boost::signals2::signal<void (const CBlock&, int height)> BlockConnected;
    boost::signals2::signal<void (const CBlock&, int height)> BlockDisconnected;
    boost::signals2::signal<void (const CBlockLocator&)> ChainStateFlushed;

    //! Serializes background callbacks on the g_scheduler thread.
    SingleThreadedSchedulerClient m_schedulerClient;

    explicit MainSignalsInstance(CScheduler& scheduler) : m_schedulerClient(&scheduler) {}

    void Register(CValidationInterface* callbacks)
    {
        LOCK(m_mutex);
        auto inserted = m_map.emplace(callbacks, ValidationInterfaceConnections{});
        if (!inserted.second) return;
        inserted.first->second.UpdatedBlockTip = UpdatedBlockTip.connect(
            std::bind(&CValidationInterface::UpdatedBlockTip, callbacks,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        inserted.first->second.TransactionAddedToMempool = TransactionAddedToMempool.connect(
            std::bind(&CValidationInterface::TransactionAddedToMempool, callbacks, std::placeholders::_1));
        inserted.first->second.TransactionRemovedFromMempool = TransactionRemovedFromMempool.connect(
            std::bind(&CValidationInterface::TransactionRemovedFromMempool, callbacks,
                      std::placeholders::_1, std::placeholders::_2));
        inserted.first->second.TransactionReplacedInMempool = TransactionReplacedInMempool.connect(
            std::bind(&CValidationInterface::TransactionReplacedInMempool, callbacks, std::placeholders::_1));
        inserted.first->second.BlockConnected = BlockConnected.connect(
            std::bind(&CValidationInterface::BlockConnected, callbacks,
                      std::placeholders::_1, std::placeholders::_2));
        inserted.first->second.BlockDisconnected = BlockDisconnected.connect(
            std::bind(&CValidationInterface::BlockDisconnected, callbacks,
                      std::placeholders::_1, std::placeholders::_2));
        inserted.first->second.ChainStateFlushed = ChainStateFlushed.connect(
            std::bind(&CValidationInterface::ChainStateFlushed, callbacks, std::placeholders::_1));
    }

    void Unregister(CValidationInterface* callbacks)
    {
        LOCK(m_mutex);
        m_map.erase(callbacks);
    }

    void Clear()
    {
        LOCK(m_mutex);
        m_map.clear();
    }
};

static CMainSignals g_signals;

CMainSignals::CMainSignals() = default;
CMainSignals::~CMainSignals() = default;

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void CMainSignals::RegisterBackgroundSignalScheduler(CScheduler& scheduler)
{
    assert(!m_internals);
    m_internals = std::make_unique<MainSignalsInstance>(scheduler);
}

void CMainSignals::UnregisterBackgroundSignalScheduler()
{
    m_internals.reset(nullptr);
}

void CMainSignals::FlushBackgroundCallbacks()
{
    if (m_internals) {
        m_internals->m_schedulerClient.EmptyQueue();
    }
}

size_t CMainSignals::CallbacksPending()
{
    if (!m_internals) return 0;
    return m_internals->m_schedulerClient.CallbacksPending();
}

void RegisterValidationInterface(CValidationInterface* callbacks)
{
    if (g_signals.m_internals) {
        g_signals.m_internals->Register(callbacks);
    }
}

void UnregisterValidationInterface(CValidationInterface* callbacks)
{
    if (g_signals.m_internals) {
        g_signals.m_internals->Unregister(callbacks);
    }
}

void UnregisterAllValidationInterfaces()
{
    if (g_signals.m_internals) {
        g_signals.m_internals->Clear();
    }
}

void CallFunctionInValidationInterfaceQueue(std::function<void()> func)
{
    // The background scheduler is only present between RegisterBackgroundSignalScheduler()
    // (AppInit2) and UnregisterBackgroundSignalScheduler() (shutdown). Outside that window
    // -- early init, unit tests, or late shutdown -- there is no queue and therefore no prior
    // background callbacks to order against, so run synchronously rather than dereferencing a
    // null m_internals (which would crash).
    if (!g_signals.m_internals) {
        func();
        return;
    }
    g_signals.m_internals->m_schedulerClient.AddToProcessQueue(std::move(func));
}

void SyncWithValidationInterfaceQueue()
{
    // Block until the validation interface queue has caught up.
    std::promise<void> promise;
    CallFunctionInValidationInterfaceQueue([&promise] {
        promise.set_value();
    });
    promise.get_future().wait();
}

// Emission is synchronous: the slots run on the calling thread, preserving the
// caller's locks (cs_main -> signals -> cs_wallet). The background scheduler
// queue above is reserved for CallFunctionInValidationInterfaceQueue and a
// future async migration.

void CMainSignals::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload)
{
    if (!m_internals) return;
    m_internals->UpdatedBlockTip(pindexNew, pindexFork, fInitialDownload);
}

void CMainSignals::TransactionAddedToMempool(const CTransactionRef& tx)
{
    if (!m_internals) return;
    m_internals->TransactionAddedToMempool(tx);
}

void CMainSignals::TransactionRemovedFromMempool(const CTransactionRef& tx, MemPoolRemovalReason reason)
{
    if (!m_internals) return;
    m_internals->TransactionRemovedFromMempool(tx, reason);
}

void CMainSignals::TransactionReplacedInMempool(const CTransactionRef& tx)
{
    if (!m_internals) return;
    m_internals->TransactionReplacedInMempool(tx);
}

void CMainSignals::BlockConnected(const CBlock& block, int height)
{
    if (!m_internals) return;
    m_internals->BlockConnected(block, height);
}

void CMainSignals::BlockDisconnected(const CBlock& block, int height)
{
    if (!m_internals) return;
    m_internals->BlockDisconnected(block, height);
}

void CMainSignals::ChainStateFlushed(const CBlockLocator& locator)
{
    if (!m_internals) return;
    m_internals->ChainStateFlushed(locator);
}
