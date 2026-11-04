// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/contract/contract.h"
#include "gridcoin/contract/message.h"
#include "gridcoin/tx_message.h"
#include "gridcoin/voting/builders.h"
#include "init.h"
#include "keystore.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "scheduler.h"
#include "sync.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "util/time.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

using grc_test::EncryptedWallet;
using grc_test::SchedulerForThisCase;
using grc_test::WaitForRelock;

// A scope holds a grant for the thread that made it. It can be neither copied
// nor moved, so the grant cannot be carried to another thread or outlive the
// scope that took it.
static_assert(!std::is_copy_constructible_v<CapabilityScope>);
static_assert(!std::is_move_constructible_v<CapabilityScope>);
static_assert(!std::is_copy_assignable_v<CapabilityScope>);
static_assert(!std::is_move_assignable_v<CapabilityScope>);

const SecureString& Passphrase()
{
    return EncryptedWallet::PASSPHRASE;
}

//! Whether the fixture's key decrypts on the calling thread.
bool KeyReachable(const EncryptedWallet& fx)
{
    CKey key;
    return fx.wallet.GetKey(fx.id, key) && key.IsValid();
}

//! The RPC error code EnsureWalletIsUnlocked() refuses the calling thread with,
//! or nullopt when it lets the thread through.
std::optional<int> EnsureRefusal()
{
    try {
        EnsureWalletIsUnlocked();
    } catch (const UniValue& error) {
        return find_value(error, "code").get_int();
    }

    return std::nullopt;
}

} // namespace

BOOST_AUTO_TEST_SUITE(capability_tests)

BOOST_AUTO_TEST_CASE(capability_ttls_are_pinned)
{
    BOOST_CHECK(CapabilityTtl(CapabilityKind::Send) == std::chrono::seconds(300));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::SignMessage) == std::chrono::seconds(120));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::SignPSGT) == std::chrono::seconds(300));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::Contract) == std::chrono::seconds(300));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::Backup) == std::chrono::seconds(300));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::FileTransfer) == std::chrono::seconds(3600));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::Settings) == std::chrono::seconds(120));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::AutoUnlockSetup) == std::chrono::seconds(120));
    BOOST_CHECK(CapabilityTtl(CapabilityKind::RpcRequest) == std::chrono::seconds(300));
}

BOOST_AUTO_TEST_CASE(failed_passphrase_delay_doubles_to_a_cap)
{
    using std::chrono::milliseconds;

    BOOST_CHECK(FailedPassphraseDelay(1) == milliseconds(500));
    BOOST_CHECK(FailedPassphraseDelay(2) == milliseconds(1000));
    BOOST_CHECK(FailedPassphraseDelay(6) == milliseconds(16000));

    // 250 ms << 7 is 32 s, over the 30 s cap, and the shift stops growing at 7.
    BOOST_CHECK(FailedPassphraseDelay(7) == milliseconds(30000));
    BOOST_CHECK(FailedPassphraseDelay(8) == milliseconds(30000));
    BOOST_CHECK(FailedPassphraseDelay(1000000) == milliseconds(30000));
}

BOOST_AUTO_TEST_CASE(capability_grants_only_inside_scope)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());
    BOOST_CHECK(*id != 0);

    // Issuing it widens nothing.
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!fx.wallet.CanUseKeys());
    BOOST_CHECK(!KeyReachable(fx));

    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(scope.Active());
        BOOST_CHECK(fx.wallet.CanUseKeys());
        BOOST_CHECK(KeyReachable(fx));
        BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    }

    // The grant ended with the scope; the capability itself is still issued.
    BOOST_CHECK(!fx.wallet.CanUseKeys());
    BOOST_CHECK(!KeyReachable(fx));
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    // A scope given no capability is inactive.
    {
        CapabilityScope scope(fx.wallet, std::nullopt, CapabilityKind::Send);
        BOOST_CHECK(!scope.Active());
        BOOST_CHECK(!fx.wallet.CanUseKeys());
    }

    // A standing staking-only unlock is not ended by its capability ending.
    fx.wallet.EndElevation(*id);
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_CASE(capability_other_thread_refused)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());

    CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
    BOOST_REQUIRE(scope.Active());

    // The scope's own thread passes the RPC check.
    BOOST_CHECK(!EnsureRefusal().has_value());

    // Boost.Test assertions are not thread-safe, so the other thread only
    // records what it saw.
    bool other_can_use_keys{true};
    bool other_key_reachable{true};
    std::optional<int> other_refusal;

    std::thread other([&] {
        other_can_use_keys = fx.wallet.CanUseKeys();
        other_key_reachable = KeyReachable(fx);
        other_refusal = EnsureRefusal();
    });
    other.join();

    BOOST_CHECK(!other_can_use_keys);
    BOOST_CHECK(!other_key_reachable);
    BOOST_REQUIRE(other_refusal.has_value());
    BOOST_CHECK_EQUAL(*other_refusal, RPC_WALLET_UNLOCK_NEEDED);

    fx.wallet.EndElevation(*id);
}

BOOST_AUTO_TEST_CASE(capability_wrong_kind_inactive)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());

    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Backup);
        BOOST_CHECK(!scope.Active());
        BOOST_CHECK(!fx.wallet.CanUseKeys());
        BOOST_CHECK(!KeyReachable(fx));
    }

    // An id that is no longer issued is inactive. It was issued while the
    // first was live, so it is not the first, and then ended.
    const std::optional<CapabilityId> ended = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(ended.has_value());
    BOOST_REQUIRE(*ended != *id);
    fx.wallet.EndElevation(*ended);

    {
        CapabilityScope scope(fx.wallet, ended, CapabilityKind::Send);
        BOOST_CHECK(!scope.Active());
    }

    // The right kind is still active: the refusals above changed nothing.
    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(scope.Active());
    }

    fx.wallet.EndElevation(*id);
}

BOOST_AUTO_TEST_CASE(capability_expired_inactive)
{
    grc_test::StateGuard guard;

    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    const int64_t now = GetTime();
    SetMockTime(now);

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());

    const int64_t ttl = CapabilityTtl(CapabilityKind::Send).count();

    // Active up to the last second before its deadline.
    SetMockTime(now + ttl - 1);
    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(scope.Active());
    }

    // Inactive from the deadline on.
    SetMockTime(now + ttl);
    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(!scope.Active());
    }

    SetMockTime(now + ttl + 1);
    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(!scope.Active());
        BOOST_CHECK(!fx.wallet.CanUseKeys());
        BOOST_CHECK(!KeyReachable(fx));
    }

    fx.wallet.EndElevation(*id);
}

BOOST_AUTO_TEST_CASE(capability_end_revokes)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());

    {
        CapabilityScope running(fx.wallet, id, CapabilityKind::Send);
        BOOST_REQUIRE(running.Active());

        fx.wallet.EndElevation(*id);

        // A scope already active finishes what it is doing.
        BOOST_CHECK(running.Active());
        BOOST_CHECK(KeyReachable(fx));

        // Every scope opened after the end is inactive, on this thread too.
        CapabilityScope later(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(!later.Active());
    }

    CapabilityScope after(fx.wallet, id, CapabilityKind::Send);
    BOOST_CHECK(!after.Active());
    BOOST_CHECK(!fx.wallet.CanUseKeys());
    BOOST_CHECK(!KeyReachable(fx));
}

BOOST_AUTO_TEST_CASE(capability_wrong_passphrase)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    BOOST_REQUIRE_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    const auto start = std::chrono::steady_clock::now();
    const std::optional<CapabilityId> refused = fx.wallet.BeginElevation(SecureString("wrong"),
                                                                         CapabilityKind::Send);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    BOOST_CHECK(!refused.has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);

    // The refusal waited out the first failure's delay before returning.
    BOOST_CHECK(elapsed >= FailedPassphraseDelay(1));

    // Nothing was widened or issued.
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!fx.wallet.CanUseKeys());

    // The right passphrase is accepted and clears the count.
    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    fx.wallet.EndElevation(*id);
}

BOOST_AUTO_TEST_CASE(failed_unlocks_count_with_failed_elevations)
{
    EncryptedWallet fx;
    BOOST_REQUIRE(fx.wallet.IsLocked());

    // Both Unlock() overloads count a wrong passphrase.
    BOOST_CHECK(!fx.wallet.Unlock(SecureString("wrong"), UnlockScope::Full));
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);

    CWallet::UnlockFailure failure = CWallet::UnlockFailure::None;
    BOOST_CHECK(!fx.wallet.Unlock(SecureString("wrong"), UnlockScope::StakingOnly, std::nullopt, &failure));
    BOOST_CHECK(failure == CWallet::UnlockFailure::Passphrase);
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 2U);

    // A refusal that is not about the passphrase is not counted.
    BOOST_REQUIRE(fx.wallet.Unlock(Passphrase(), UnlockScope::StakingOnly));
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    BOOST_CHECK(!fx.wallet.Unlock(SecureString("wrong"), UnlockScope::Full, std::nullopt, &failure));
    BOOST_CHECK(failure == CWallet::UnlockFailure::AlreadyUnlocked);
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
}

BOOST_AUTO_TEST_CASE(capability_on_locked_wallet_relocks)
{
    EncryptedWallet fx;
    SchedulerForThisCase scheduler;
    BOOST_REQUIRE(fx.wallet.IsLocked());

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());

    // Unlocked for staking only, timed to the capability, and no wider.
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::StakingOnly);
    BOOST_CHECK(fx.wallet.GetUnlockDeadline().has_value());
    BOOST_CHECK(!fx.wallet.CanUseKeys());
    BOOST_CHECK(!KeyReachable(fx));

    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_REQUIRE(scope.Active());
        BOOST_CHECK(KeyReachable(fx));
    }

    fx.wallet.EndElevation(*id);
    BOOST_CHECK(fx.wallet.IsLocked());
}

BOOST_AUTO_TEST_CASE(capabilities_sharing_an_unlock_relock_when_the_last_ends)
{
    EncryptedWallet fx;
    SchedulerForThisCase scheduler;
    BOOST_REQUIRE(fx.wallet.IsLocked());

    const std::optional<CapabilityId> first = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(first.has_value());
    BOOST_REQUIRE(fx.wallet.IsUnlockedForStakingOnly());

    // The second finds the unlock the first made, and relies on it.
    const std::optional<CapabilityId> second = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Contract);
    BOOST_REQUIRE(second.has_value());
    BOOST_CHECK(*first != *second);

    fx.wallet.EndElevation(*first);
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    {
        CapabilityScope scope(fx.wallet, second, CapabilityKind::Contract);
        BOOST_REQUIRE(scope.Active());
        BOOST_CHECK(KeyReachable(fx));
    }

    fx.wallet.EndElevation(*second);
    BOOST_CHECK(fx.wallet.IsLocked());
}

BOOST_AUTO_TEST_CASE(capability_full_wallet_unchanged)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::Full);

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::Full);

    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(scope.Active());
        BOOST_CHECK(KeyReachable(fx));
    }

    fx.wallet.EndElevation(*id);
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::Full);
    BOOST_CHECK(fx.wallet.CanUseKeys());

    // The passphrase is still checked: a capability outlives the standing
    // unlock if the wallet is narrowed to staking only, so one issued for a
    // wrong passphrase would then grant what the passphrase guards.
    BOOST_CHECK(!fx.wallet.BeginElevation(SecureString("wrong"), CapabilityKind::Send).has_value());
    BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::Full);
}

BOOST_AUTO_TEST_CASE(capability_unencrypted_wallet_is_uniform)
{
    CWallet plain;
    BOOST_REQUIRE(!plain.IsCrypted());

    // Any passphrase: there is none to check, and callers need not tell.
    const std::optional<CapabilityId> id = plain.BeginElevation(SecureString("anything"), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());
    BOOST_CHECK_EQUAL(plain.FailedUnlockCount(), 0U);

    {
        CapabilityScope scope(plain, id, CapabilityKind::Send);
        BOOST_CHECK(scope.Active());
        BOOST_CHECK(plain.CanUseKeys());
    }

    plain.EndElevation(*id);
    BOOST_CHECK(plain.CanUseKeys());
    BOOST_CHECK(plain.GetUnlockScope() == UnlockScope::Full);
}

BOOST_AUTO_TEST_CASE(capability_relock_during_operation)
{
    EncryptedWallet fx;
    SchedulerForThisCase scheduler;

    // A standing staking-only unlock with its relock armed.
    BOOST_REQUIRE(fx.wallet.Unlock(Passphrase(), UnlockScope::StakingOnly, std::chrono::seconds(60)));
    BOOST_REQUIRE(fx.wallet.IsUnlockedForStakingOnly());

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());

    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_REQUIRE(scope.Active());
        BOOST_REQUIRE(KeyReachable(fx));

        // The relock lands in the middle of the operation.
        g_scheduler->MockForward(std::chrono::seconds(61));
        WaitForRelock(fx.wallet);
        BOOST_REQUIRE(fx.wallet.IsLocked());

        // The operation fails cleanly: no key is installed to reach, none can
        // be made, and nothing was widened.
        BOOST_CHECK(!KeyReachable(fx));
        BOOST_CHECK(!fx.wallet.CanCreateKeys());
        BOOST_CHECK(fx.wallet.GetUnlockScope() == UnlockScope::Locked);
    }

    BOOST_CHECK(fx.wallet.IsLocked());
    BOOST_CHECK(!fx.wallet.CanUseKeys());

    // The capability was not the one that unlocked, so ending it does nothing.
    fx.wallet.EndElevation(*id);
    BOOST_CHECK(fx.wallet.IsLocked());
}

BOOST_AUTO_TEST_CASE(ipc_slot_dropped)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    BOOST_REQUIRE(!fx.wallet.IpcCapability().has_value());

    BOOST_REQUIRE(fx.wallet.BeginIpcElevation(Passphrase(), CapabilityKind::Send));
    const std::optional<CapabilityId> id = fx.wallet.IpcCapability();
    BOOST_REQUIRE(id.has_value());

    {
        CapabilityScope scope(fx.wallet, fx.wallet.IpcCapability(), CapabilityKind::Send);
        BOOST_CHECK(scope.Active());
        BOOST_CHECK(KeyReachable(fx));
    }

    fx.wallet.EndIpcElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());

    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::Send);
        BOOST_CHECK(!scope.Active());
    }

    // Ending an empty slot is a no-op.
    fx.wallet.EndIpcElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
}

BOOST_AUTO_TEST_CASE(ipc_slot_holds_one_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    BOOST_REQUIRE(fx.wallet.BeginIpcElevation(Passphrase(), CapabilityKind::Send));
    const std::optional<CapabilityId> first = fx.wallet.IpcCapability();
    BOOST_REQUIRE(first.has_value());

    // A second begin replaces the first, which ends.
    BOOST_REQUIRE(fx.wallet.BeginIpcElevation(Passphrase(), CapabilityKind::SignMessage));
    const std::optional<CapabilityId> second = fx.wallet.IpcCapability();
    BOOST_REQUIRE(second.has_value());
    BOOST_CHECK(*first != *second);

    {
        CapabilityScope scope(fx.wallet, first, CapabilityKind::Send);
        BOOST_CHECK(!scope.Active());
    }

    // A refused begin empties the slot too, so nothing earlier stays usable.
    BOOST_CHECK(!fx.wallet.BeginIpcElevation(SecureString("wrong"), CapabilityKind::Send));
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());

    {
        CapabilityScope scope(fx.wallet, second, CapabilityKind::SignMessage);
        BOOST_CHECK(!scope.Active());
    }
}

BOOST_AUTO_TEST_CASE(capability_end_idempotent_and_lock_free)
{
    EncryptedWallet fx;
    SchedulerForThisCase scheduler;

    // Unknown ids, and ending one twice, change nothing.
    fx.wallet.EndElevation(0);
    fx.wallet.EndElevation(12345);
    BOOST_CHECK(fx.wallet.IsLocked());

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(id.has_value());
    BOOST_REQUIRE(fx.wallet.IsUnlockedForStakingOnly());

    // The relock that ends a capability begun on a locked wallet announces the
    // change. At that point the ending thread must hold no lock: neither the
    // capability mutex, nor cs_wallet or cs_main. The lock stack is tracked in
    // the lock-order build; elsewhere this reads empty. Only the ends are
    // watched.
    std::atomic<int> notified{0};
    std::atomic<bool> held_a_lock{false};

    const auto watch = [&] {
        return fx.wallet.NotifyStatusChanged.connect([&](CCryptoKeyStore*) {
            if (!LockStackEmpty()) held_a_lock = true;
            ++notified;
        });
    };

    {
        const boost::signals2::scoped_connection connection = watch();

        fx.wallet.EndElevation(*id);
        BOOST_CHECK(fx.wallet.IsLocked());
        BOOST_CHECK_EQUAL(notified.load(), 1);
        BOOST_CHECK(!held_a_lock.load());

        fx.wallet.EndElevation(*id);
        BOOST_CHECK(fx.wallet.IsLocked());
        BOOST_CHECK_EQUAL(notified.load(), 1);
    }

    // Ending takes neither cs_main nor cs_wallet: it completes on another
    // thread while this one holds both.
    const std::optional<CapabilityId> held = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    BOOST_REQUIRE(held.has_value());
    BOOST_REQUIRE(fx.wallet.IsUnlockedForStakingOnly());

    {
        const boost::signals2::scoped_connection connection = watch();

        std::future<void> ending;
        bool completed{false};
        {
            LOCK2(cs_main, fx.wallet.cs_wallet);

            ending = std::async(std::launch::async, [&fx, &held] { fx.wallet.EndElevation(*held); });
            completed = ending.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
        }
        ending.wait();

        BOOST_CHECK(completed);
        BOOST_CHECK(fx.wallet.IsLocked());
        BOOST_CHECK_EQUAL(notified.load(), 2);
        BOOST_CHECK(!held_a_lock.load());
    }
}

BOOST_AUTO_TEST_CASE(unlock_derives_without_holding_cs_wallet)
{
    EncryptedWallet fx;
    BOOST_REQUIRE(fx.wallet.IsLocked());

    // A long derivation, at an iteration count set here rather than scaled
    // from the calibrated one: five million iterations take a second or more
    // on any machine.
    //
    // The crypted master key is also cut short by a byte, so that no key
    // decrypts it: CBC decryption refuses a ciphertext that is not a whole
    // number of blocks. A wrong key would otherwise pass the padding check
    // about once in 255 tries, and the attempt would then take cs_wallet again
    // to install it, which this thread holds. As it is, the unlock below is
    // refused as soon as the derivation ends, without taking cs_wallet, and
    // the refusal is counted before its delay is slept.
    {
        LOCK(fx.wallet.cs_wallet);
        BOOST_REQUIRE(!fx.wallet.mapMasterKeys.empty());

        for (auto& entry : fx.wallet.mapMasterKeys) {
            entry.second.nDeriveIterations = 5000000;

            BOOST_REQUIRE(!entry.second.vchCryptedKey.empty());
            entry.second.vchCryptedKey.pop_back();
        }
    }

    std::atomic<bool> began{false};
    std::future<bool> unlock = std::async(std::launch::async, [&fx, &began] {
        began = true;
        return fx.wallet.Unlock(SecureString("wrong"), UnlockScope::Full);
    });

    for (int i = 0; i < 6000 && !began; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    BOOST_REQUIRE(began);

    // This thread takes cs_wallet while the attempt is not yet counted, and
    // holds it. The attempt can then be refused, and so counted, only if it
    // derives without cs_wallet. The attempt takes cs_wallet briefly to copy
    // the records first, at once when it begins. In the rare case that this
    // thread still took it before that, the attempt waits for it and nothing
    // is counted while it is held, so the round is given up for another. A
    // round lasts several times the derivation on a loaded machine, so that an
    // attempt that is only slow is not mistaken for one that waits.
    bool refused_while_held{false};
    bool waited_for_the_lock{false};

    for (int round = 0; round < 3 && !refused_while_held; ++round) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        const auto asked = std::chrono::steady_clock::now();
        LOCK(fx.wallet.cs_wallet);

        if (std::chrono::steady_clock::now() - asked >= std::chrono::milliseconds(500)) {
            waited_for_the_lock = true;
        }

        // Counted before this thread held the lock, so nothing is left to show.
        if (fx.wallet.FailedUnlockCount() != 0) break;

        for (int i = 0; i < 2000 && fx.wallet.FailedUnlockCount() == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        refused_while_held = fx.wallet.FailedUnlockCount() == 1;
    }

    BOOST_CHECK(!unlock.get());

    // The attempt finished deriving while this thread held cs_wallet, and
    // taking cs_wallet never waited for a derivation.
    BOOST_CHECK(refused_while_held);
    BOOST_CHECK(!waited_for_the_lock);
    BOOST_CHECK(fx.wallet.IsLocked());
}

BOOST_AUTO_TEST_CASE(an_attempt_waits_out_a_concurrent_refusal)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    // Two refusals first, so that the next one earns a 2 s delay, long beside a
    // key derivation even on a loaded machine.
    BOOST_REQUIRE(!fx.wallet.BeginElevation(SecureString("wrong"), CapabilityKind::Send).has_value());
    BOOST_REQUIRE(!fx.wallet.BeginElevation(SecureString("wrong"), CapabilityKind::Send).has_value());
    BOOST_REQUIRE_EQUAL(fx.wallet.FailedUnlockCount(), 2U);

    std::future<std::optional<CapabilityId>> refused = std::async(std::launch::async, [&fx] {
        return fx.wallet.BeginElevation(SecureString("wrong"), CapabilityKind::Send);
    });

    // Until the third attempt has been refused. It is then waiting out its
    // delay on its own thread.
    for (int i = 0; i < 1000 && fx.wallet.FailedUnlockCount() < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    BOOST_REQUIRE_EQUAL(fx.wallet.FailedUnlockCount(), 3U);

    // An attempt made meanwhile, from another caller, waits for the same
    // delay before its passphrase is checked, so concurrent callers get no
    // more attempts than one caller would. Most of the 2 s, with slack.
    const auto asked = std::chrono::steady_clock::now();
    const std::optional<CapabilityId> accepted = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    const auto waited = std::chrono::steady_clock::now() - asked;

    BOOST_CHECK(accepted.has_value());
    BOOST_CHECK(waited >= std::chrono::milliseconds(1000));
    BOOST_CHECK(!refused.get().has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
}

BOOST_AUTO_TEST_CASE(a_queued_attempt_waits_out_failures_made_while_it_waited)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    // A short derivation, at an iteration count set here, so that each check
    // is brief beside the half-second turns. No passphrase opens the records
    // with it, so every attempt below is refused.
    {
        LOCK(fx.wallet.cs_wallet);
        BOOST_REQUIRE(!fx.wallet.mapMasterKeys.empty());

        for (auto& entry : fx.wallet.mapMasterKeys) {
            entry.second.nDeriveIterations = 1000;
        }
    }

    // Three attempts start together while nothing has failed, so each claims
    // a turn spaced by the first failure's delay. The first is refused at
    // once, and the turns of the other two come round after the count has
    // grown.
    std::promise<void> go;
    const std::shared_future<void> released = go.get_future().share();
    std::vector<std::future<std::optional<CapabilityId>>> attempts;

    for (int i = 0; i < 3; ++i) {
        attempts.push_back(std::async(std::launch::async, [&fx, released] {
            released.wait();
            return fx.wallet.BeginElevation(SecureString("wrong"), CapabilityKind::Send);
        }));
    }

    // When the count was first seen at 1, 2 and 3, which is when the first,
    // second and third passphrase checks failed, as closely as polling tells.
    std::array<std::chrono::steady_clock::time_point, 3> failed_at{};
    unsigned int seen{0};

    go.set_value();

    for (int i = 0; i < 60000 && seen < failed_at.size(); ++i) {
        const unsigned int count = fx.wallet.FailedUnlockCount();
        const auto now = std::chrono::steady_clock::now();

        while (seen < count && seen < failed_at.size()) {
            failed_at[seen++] = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (auto& attempt : attempts) {
        BOOST_CHECK(!attempt.get().has_value());
    }

    BOOST_REQUIRE_EQUAL(seen, 3U);
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 3U);

    // Each check came no sooner after the failure before it than the delay
    // that failure earned, as when one caller makes the attempts in turn.
    // Lower bounds only: a loaded machine makes every gap longer.
    BOOST_CHECK(failed_at[1] - failed_at[0] >= FailedPassphraseDelay(1));
    BOOST_CHECK(failed_at[2] - failed_at[1] >= FailedPassphraseDelay(2));
}

BOOST_AUTO_TEST_CASE(a_failed_passphrase_change_counts_and_waits)
{
    EncryptedWallet fx;
    const SecureString changed("capability-test-changed");

    const auto asked = std::chrono::steady_clock::now();
    BOOST_CHECK(!fx.wallet.ChangeWalletPassphrase(SecureString("wrong"), changed));
    const auto waited = std::chrono::steady_clock::now() - asked;

    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 1U);
    BOOST_CHECK(waited >= FailedPassphraseDelay(1));

    // The right passphrase changes it and clears the count.
    BOOST_CHECK(fx.wallet.ChangeWalletPassphrase(Passphrase(), changed));
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);
    BOOST_CHECK(fx.wallet.IsLocked());
}

BOOST_AUTO_TEST_CASE(a_passphrase_change_ends_every_capability)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    const SecureString changed("capability-test-changed");

    const std::optional<CapabilityId> id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::FileTransfer);
    BOOST_REQUIRE(id.has_value());
    BOOST_REQUIRE(fx.wallet.BeginIpcElevation(Passphrase(), CapabilityKind::Send));

    BOOST_REQUIRE(fx.wallet.ChangeWalletPassphrase(Passphrase(), changed));

    // The unlock is as it was; what the old passphrase issued is not.
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!fx.wallet.IpcCapability().has_value());
    {
        CapabilityScope scope(fx.wallet, id, CapabilityKind::FileTransfer);
        BOOST_CHECK(!scope.Active());
        BOOST_CHECK(!KeyReachable(fx));
    }

    // The new passphrase issues capabilities as before.
    const std::optional<CapabilityId> renewed = fx.wallet.BeginElevation(changed, CapabilityKind::Send);
    BOOST_REQUIRE(renewed.has_value());
    {
        CapabilityScope scope(fx.wallet, renewed, CapabilityKind::Send);
        BOOST_CHECK(scope.Active());
        BOOST_CHECK(KeyReachable(fx));
    }

    fx.wallet.EndElevation(*renewed);
}

BOOST_AUTO_TEST_CASE(capability_lets_a_staking_only_wallet_send)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    const std::string locked = _("Error: Wallet locked, unable to create transaction  ");
    const std::string staking_only = _("Error: Wallet unlocked for staking only, unable to create transaction.");

    CScript destination;
    destination.SetDestination(fx.id);

    const auto send = [&] {
        CWalletTx wtx;
        return fx.wallet.SendMoney(destination, CENT, wtx);
    };

    const auto send_contract = [] {
        LOCK(cs_main);
        return GRC::SendContract(GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, "capability")).second;
    };

    const auto poll_build_error = [&] {
        try {
            LOCK(cs_main);
            GRC::PollBuilder().BuildContractTx(&fx.wallet, 2);
        } catch (const std::exception& e) {
            return std::string(e.what());
        }

        return std::string();
    };

    // Refused for the scope without a capability.
    BOOST_CHECK_EQUAL(send(), staking_only);
    BOOST_CHECK_EQUAL(send_contract(), staking_only);
    BOOST_CHECK_EQUAL(poll_build_error(), staking_only);
    BOOST_CHECK(EnsureRefusal().has_value());

    const std::optional<CapabilityId> send_id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Send);
    const std::optional<CapabilityId> contract_id = fx.wallet.BeginElevation(Passphrase(), CapabilityKind::Contract);
    BOOST_REQUIRE(send_id.has_value() && contract_id.has_value());

    // With one, each goes past the scope check to the next refusal, which on
    // this empty wallet is about its funds or its content.
    {
        CapabilityScope scope(fx.wallet, send_id, CapabilityKind::Send);
        BOOST_REQUIRE(scope.Active());

        const std::string result = send();
        BOOST_CHECK(result != staking_only);
        BOOST_CHECK(result != locked);
        BOOST_CHECK(!result.empty());

        BOOST_CHECK(!EnsureRefusal().has_value());
    }

    {
        CapabilityScope scope(fx.wallet, contract_id, CapabilityKind::Contract);
        BOOST_REQUIRE(scope.Active());

        BOOST_CHECK_EQUAL(send_contract(), _("Balance too low to create a contract."));

        const std::string poll_error = poll_build_error();
        BOOST_CHECK(poll_error != staking_only);
        BOOST_CHECK(!poll_error.empty());
    }

    fx.wallet.EndElevation(*send_id);
    fx.wallet.EndElevation(*contract_id);
}

BOOST_AUTO_TEST_SUITE_END()
