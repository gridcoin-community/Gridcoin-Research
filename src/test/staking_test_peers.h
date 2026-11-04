// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_STAKING_TEST_PEERS_H
#define GRIDCOIN_TEST_STAKING_TEST_PEERS_H

#include "init.h"
#include "key.h"
#include "keystore.h"
#include "node/shutdown.h"
#include "scheduler.h"
#include "support/allocators/secure.h"
#include "sync.h"
#include "util/time.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <memory>
#include <thread>

// The two test peers make tokens that elsewhere only the named factories and
// CapabilityScope may make. They must sit at global scope, because the friend
// declarations in keystore.h name ::StakingKeyAccessTestPeer and
// ::ElevationGrantTestPeer. test/lint/lint-staking-key-access.py confines them
// to src/test/.
struct StakingKeyAccessTestPeer
{
    static StakingKeyAccess Make() { return StakingKeyAccess(); }
};

struct ElevationGrantTestPeer
{
    static ElevationGrant Make(const CCryptoKeyStore& store) { return ElevationGrant(store); }

    static std::unique_ptr<ElevationGrant> MakeHeld(const CCryptoKeyStore& store)
    {
        return std::unique_ptr<ElevationGrant>(new ElevationGrant(store));
    }
};

namespace grc_test {

//! Swaps a wallet into pwalletMain for the life of a case, as wallet_tests does.
struct SwapMainWallet
{
    CWallet* m_saved{nullptr};

    explicit SwapMainWallet(CWallet* replacement) : m_saved(pwalletMain) { pwalletMain = replacement; }
    ~SwapMainWallet() { pwalletMain = m_saved; }

    SwapMainWallet(const SwapMainWallet&) = delete;
    SwapMainWallet& operator=(const SwapMainWallet&) = delete;
};

//! An in-memory wallet holding one key added before encryption, left locked,
//! and swapped into pwalletMain for its lifetime.
//!
//! In-memory rather than file-backed: EncryptWallet on a file-backed wallet
//! rewrites wallet.dat, which the mocked Berkeley DB of this binary refuses.
struct EncryptedWallet
{
    static inline const SecureString PASSPHRASE{"staking-key-access-test"};

    CWallet wallet;
    SwapMainWallet swap{&wallet};
    CKey key;
    CKeyID id;

    EncryptedWallet()
    {
        key.MakeNewKey(true);
        id = key.GetPubKey().GetID();

        {
            LOCK(wallet.cs_wallet);
            BOOST_REQUIRE(wallet.AddKey(key));
        }

        BOOST_REQUIRE(wallet.EncryptWallet(PASSPHRASE));
        wallet.Lock();
        BOOST_REQUIRE(wallet.IsLocked());
    }

    ~EncryptedWallet()
    {
        wallet.Lock();
    }

    EncryptedWallet(const EncryptedWallet&) = delete;
    EncryptedWallet& operator=(const EncryptedWallet&) = delete;

    void UnlockFor(UnlockScope scope)
    {
        wallet.Lock();
        BOOST_REQUIRE(wallet.Unlock(PASSPHRASE, scope));
        BOOST_REQUIRE(wallet.GetUnlockScope() == scope);
    }
};

//! Stand up a running scheduler for the duration of a case and put the previous
//! one back however the case exits, so a failed assertion cannot strand the
//! rest of the binary without one.
//!
//! The handle is published as soon as the object exists and does NOT wait for
//! the service thread to enter serviceQueue. It does not need to: arming asks
//! only whether the scheduler has been told to stop, so a task enqueued before
//! the thread comes up is run when it gets there. Waiting on
//! AreThreadsServicingQueue() here would be the racy version of this.
//!
//! Declare it AFTER the wallet its tasks touch, so it is destroyed first: its
//! destructor joins the service thread, which may still be inside a relock. A
//! case sees the relock through IsLocked(), and LockIfCurrent() locks before it
//! raises NotifyStatusChanged, so a wallet destroyed first is destroyed under
//! that call (ThreadSanitizer reported it as a race on the signal's mutex).
struct SchedulerForThisCase {
    std::unique_ptr<CScheduler> m_saved;
    CScheduler* m_saved_handle;
    std::thread m_service;

    SchedulerForThisCase()
        : m_saved(std::move(g_scheduler))
        , m_saved_handle(g_scheduler_handle.load(std::memory_order_acquire))
    {
        g_scheduler = std::make_unique<CScheduler>();
        g_scheduler_handle.store(g_scheduler.get(), std::memory_order_release);
        m_service = std::thread([] { g_scheduler->serviceQueue(); });
    }

    ~SchedulerForThisCase()
    {
        g_scheduler_handle.store(m_saved_handle, std::memory_order_release);
        g_scheduler->stop();
        if (m_service.joinable()) m_service.join();
        g_scheduler = std::move(m_saved);
    }
};

//! Spin until the scheduled relock has run, or give up. The service thread runs
//! the task, so the wait is for another thread rather than for time.
inline void WaitForRelock(const CWallet& wallet)
{
    for (int i = 0; i < 200 && !wallet.IsLocked(); ++i) {
        UninterruptibleSleep(std::chrono::milliseconds(10));
    }
}

//! Requests a shutdown while it lives, and clears the request however the case
//! exits, so that the rest of the binary runs without one.
struct ShutdownRequestedForThisCase
{
    ShutdownRequestedForThisCase() { SetShutdownRequested(); }
    ~ShutdownRequestedForThisCase() { AbortShutdown(); }

    ShutdownRequestedForThisCase(const ShutdownRequestedForThisCase&) = delete;
    ShutdownRequestedForThisCase& operator=(const ShutdownRequestedForThisCase&) = delete;
};

} // namespace grc_test

#endif // GRIDCOIN_TEST_STAKING_TEST_PEERS_H
