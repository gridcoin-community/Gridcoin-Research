// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_MODELFAKES_H
#define BITCOIN_QT_TEST_MODELFAKES_H

#include "interfaces/handler.h"
#include "interfaces/mrc.h"
#include "interfaces/staking.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_tx_source.h"
#include "qt/test/interfacefakes.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

//! Fakes of the interfaces the GUI models read, so a test can build the real
//! ClientModel, WalletModel, ResearcherModel, MRCModel and VotingModel over
//! in-memory values. Beside interfacefakes.h and free of Qt and of the QML
//! module, so the Widgets tests can use them too. Each fake records the thread
//! of the calls a test asks about, and can hold a call at its start on a
//! FakeLatch until the test opens it.
namespace qt_test {

//! A gate a fake waits on until a test opens it; bounded (30 s), so a test
//! that forgets to open it fails instead of hanging.
class FakeLatch
{
public:
    std::atomic<bool> open{false};

    void wait()
    {
        for (int i = 0; i < 30000 && !open; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

//! Fake interfaces::Wallet: settable balances and lock state; records the
//! thread of the last balance read and of the last lock-state read, and counts
//! address-label lookups. Every other method answers with an empty default.
class FakeWallet : public interfaces::Wallet
{
public:
    // Settable values.
    std::atomic<int64_t> m_balance{0};
    std::atomic<int64_t> m_stake{0};
    std::atomic<int64_t> m_unconfirmed_balance{0};
    std::atomic<int64_t> m_immature_balance{0};
    std::atomic<int> m_num_transactions{0};
    interfaces::WalletLockState m_lock_state;
    //! While set, tryGetBalances bows out (returns false).
    bool m_balances_bow_out{false};

    // Optional latches, waited on at the start of the call when set.
    FakeLatch* m_lock_state_latch{nullptr};
    FakeLatch* m_balance_latch{nullptr};

    // Recorded calls.
    std::atomic<std::thread::id> m_balance_thread{};
    std::atomic<std::thread::id> m_lock_state_thread{};
    std::atomic<int> m_address_label_calls{0};

    int64_t getBalance() override
    {
        BalanceRead();
        return m_balance;
    }
    int64_t getStake() override
    {
        BalanceRead();
        return m_stake;
    }
    int64_t getUnconfirmedBalance() override
    {
        BalanceRead();
        return m_unconfirmed_balance;
    }
    int64_t getImmatureBalance() override
    {
        BalanceRead();
        return m_immature_balance;
    }
    bool tryGetBalances(interfaces::WalletBalances& balances) override
    {
        BalanceRead();
        if (m_balances_bow_out) return false;
        balances.balance = m_balance;
        balances.stake = m_stake;
        balances.unconfirmed_balance = m_unconfirmed_balance;
        balances.immature_balance = m_immature_balance;
        return true;
    }
    int getNumTransactions() override { return m_num_transactions; }
    interfaces::WalletLockState getLockState() override
    {
        if (m_lock_state_latch) m_lock_state_latch->wait();
        m_lock_state_thread = std::this_thread::get_id();
        return m_lock_state;
    }
    bool isUnlockedForStakingOnly() override { return false; }
    bool getUnlockStakingOnlyFlag() override { return false; }
    bool encryptWallet(const SecureString& /*passphrase*/) override { return false; }
    bool lockWallet() override { return false; }
    bool restrictToStakingOnly() override { return false; }
    bool elevateWallet(const SecureString& /*passphrase*/) override { return false; }
    bool unlockWallet(const SecureString& /*passphrase*/, bool /*staking_only*/) override { return false; }
    bool changeWalletPassphrase(const SecureString& /*old_passphrase*/,
                                const SecureString& /*new_passphrase*/) override
    {
        return false;
    }
    bool getPubKey(const CKeyID& /*address*/, CPubKey& /*pub_key_out*/) override { return false; }
    bool getKeyFromPool(const std::string& /*label*/, CPubKey& /*pub_key_out*/) override { return false; }
    std::vector<interfaces::WalletOutput> getOutputs(const std::vector<COutPoint>& /*outpoints*/) override
    {
        return {};
    }
    interfaces::CoinControlSummary computeCoinControlSummary(const interfaces::WalletCoinControl& /*selection*/,
                                                             const std::vector<int64_t>& /*recipient_amounts*/,
                                                             bool /*subtract_fee_from_amount*/) override
    {
        return {};
    }
    unsigned int getMaxConsolidationInputs() override { return 0; }
    interfaces::SendCoinsResult sendCoins(const std::vector<interfaces::WalletSendRecipient>& /*recipients*/,
                                          const std::optional<interfaces::WalletCoinControl>& /*coin_control*/,
                                          int64_t /*accepted_fee*/) override
    {
        return {};
    }
    bool backupWallet(const std::string& /*dest*/) override { return false; }
    bool backupConfigFile(const std::string& /*dest*/) override { return false; }
    interfaces::MessageSignStatus signMessage(const std::string& /*address*/, const std::string& /*message*/,
                                              std::string& /*signature_out*/) override
    {
        return {};
    }
    interfaces::MessageVerifyStatus verifyMessage(const std::string& /*address*/, const std::string& /*message*/,
                                                  const std::vector<unsigned char>& /*signature*/) override
    {
        return {};
    }
    std::vector<interfaces::WalletAddress> getAddresses() override { return {}; }
    bool getAddressLabel(const std::string& /*address*/, std::string& /*label_out*/) override
    {
        ++m_address_label_calls;
        return false;
    }
    bool isMine(const std::string& /*address*/) override { return false; }
    void setAddressBook(const std::string& /*address*/, const std::string& /*label*/) override {}
    bool delAddressBook(const std::string& /*address*/) override { return false; }
    bool getNewReceiveAddress(std::string& /*address_out*/) override { return false; }
    bool getNewReceiveAddressWithLabel(const std::string& /*label*/, std::string& /*address_out*/) override
    {
        return false;
    }
    std::vector<std::string> getUnbookedReceiveAddresses() override { return {}; }

    // The subscription callbacks are dropped, as in interfacefakes.h.
    std::unique_ptr<interfaces::Handler> handleStatusChanged(StatusChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleAddressBookChanged(AddressBookChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleTransactionChanged(TransactionChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

private:
    //! Every balance read: waits on the balance latch, then records its thread.
    void BalanceRead()
    {
        if (m_balance_latch) m_balance_latch->wait();
        m_balance_thread = std::this_thread::get_id();
    }
};

//! Fake interfaces::WalletTxSource: serves canned rows per view and records
//! the view registrations and limits, with the calling thread of each kind of
//! call. The records are held under a mutex: setViewLimit can run on a worker
//! thread while getRows runs on the GUI thread.
class FakeWalletTxSource : public interfaces::WalletTxSource
{
public:
    //! Waited on at the start of setViewLimit when set.
    FakeLatch* m_set_view_limit_latch{nullptr};

    //! The rows getRows returns for a view.
    void setRows(int view_id, std::vector<TransactionRecord> rows)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rows[view_id] = std::move(rows);
    }

    int registerViewCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_register_view_calls;
    }
    int unregisterViewCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_unregister_view_calls;
    }
    //! The views unregisterView was called for, in order.
    std::vector<int> unregisteredViews() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_unregistered_views;
    }
    int setViewLimitCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_set_view_limit_calls;
    }
    //! The limits of the setViewLimit calls, in order.
    std::vector<int> viewLimits() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_view_limits;
    }
    //! The views of the setViewLimit calls, in order.
    std::vector<int> viewLimitViews() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_view_limit_views;
    }
    std::thread::id registerViewThread() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_register_view_thread;
    }
    std::thread::id getRowsThread() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_get_rows_thread;
    }
    std::thread::id setViewLimitThread() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_set_view_limit_thread;
    }
    std::thread::id unregisterViewThread() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_unregister_view_thread;
    }
    //! Clears every recorded call (the canned rows stay).
    void clearRecords()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_register_view_calls = 0;
        m_unregister_view_calls = 0;
        m_set_view_limit_calls = 0;
        m_unregistered_views.clear();
        m_view_limits.clear();
        m_view_limit_views.clear();
    }

    void registerView(int /*view_id*/, GRC::FilterSpec /*filter*/, int /*sort_column*/, int /*sort_order*/) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_register_view_calls;
        m_register_view_thread = std::this_thread::get_id();
    }
    void unregisterView(int view_id) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_unregister_view_calls;
        m_unregistered_views.push_back(view_id);
        m_unregister_view_thread = std::this_thread::get_id();
    }
    void setViewSort(int /*view_id*/, int /*sort_column*/, int /*sort_order*/) override {}
    void setViewFilter(int /*view_id*/, GRC::FilterSpec /*filter*/) override {}
    void setViewLimit(int view_id, int limit) override
    {
        if (m_set_view_limit_latch) m_set_view_limit_latch->wait();
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_set_view_limit_calls;
        m_view_limit_views.push_back(view_id);
        m_view_limits.push_back(limit);
        m_set_view_limit_thread = std::this_thread::get_id();
    }
    GRC::RowsResult getRows(int view_id, int /*first*/, int /*count*/) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_get_rows_thread = std::this_thread::get_id();
        GRC::RowsResult result;
        const auto it = m_rows.find(view_id);
        if (it != m_rows.end()) result.records = it->second;
        result.total_accepted = static_cast<int>(result.records.size());
        return result;
    }
    GRC::RowsResult getAllRows(int view_id) override { return getRows(view_id, 0, -1); }
    int rowForKey(int /*view_id*/, const uint256& /*hash*/, int /*idx*/) override { return -1; }
    GRC::WalletTxDetail getRowDetail(const uint256& /*hash*/, int /*idx*/) override { return {}; }
    void prime(bool /*limit_enabled*/, int64_t /*limit_time*/) override {}
    std::vector<GRC::WalletEvent> drainEvents(std::size_t /*max_batch*/) override { return {}; }
    void noteAddressBookChanged(const std::string& /*address*/, const std::string& /*label*/) override {}

private:
    mutable std::mutex m_mutex;
    std::map<int, std::vector<TransactionRecord>> m_rows;
    int m_register_view_calls{0};
    int m_unregister_view_calls{0};
    int m_set_view_limit_calls{0};
    std::vector<int> m_unregistered_views;
    std::vector<int> m_view_limits;
    std::vector<int> m_view_limit_views;
    std::thread::id m_register_view_thread{};
    std::thread::id m_get_rows_thread{};
    std::thread::id m_set_view_limit_thread{};
    std::thread::id m_unregister_view_thread{};
};

//! Fake interfaces::StakingStatus: settable staking flag, weights, errors and
//! estimated time to stake (seconds). ClientModel's status-bar and miner
//! warnings and its network weight read through it.
class FakeStakingStatus : public interfaces::StakingStatus
{
public:
    std::atomic<bool> m_staking{false};
    std::atomic<double> m_coin_weight{0.0};
    std::atomic<double> m_network_weight{0.0};
    std::atomic<double> m_etts{0.0};

    //! The errors getErrors returns.
    void setErrors(std::string errors)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errors = std::move(errors);
    }

    bool isStaking() override { return m_staking; }
    std::string getErrors() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_errors;
    }
    double getCoinWeight() override { return m_coin_weight; }
    double getNetworkWeight() override { return m_network_weight; }
    std::optional<double> tryGetNetworkWeight() override { return m_network_weight.load(); }
    std::optional<double> tryGetEstimatedTimeToStake() override { return m_etts.load(); }

private:
    std::mutex m_mutex;
    std::string m_errors;
};

//! Fake interfaces::VotingManager: a settable current poll title, with the
//! thread of the last currentPollTitle() call recorded and the calls counted;
//! everything else answers with an empty default.
class FakeVotingManager : public interfaces::VotingManager
{
public:
    std::atomic<std::thread::id> m_current_poll_title_thread{};
    std::atomic<int> m_current_poll_title_calls{0};

    void setCurrentPollTitle(std::string title)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_current_poll_title = std::move(title);
    }

    std::vector<interfaces::PollTableItem> buildPollTable(int /*filter_flags*/) override { return {}; }
    std::vector<interfaces::PollTypeMeta> getPollTypes() override { return {}; }
    bool pollV3Enabled() override { return false; }
    CAmount estimatePollFee() override { return 0; }
    std::string currentPollTitle() override
    {
        m_current_poll_title_thread = std::this_thread::get_id();
        ++m_current_poll_title_calls;
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_current_poll_title;
    }
    int64_t latestActivePollTime() override { return 0; }
    interfaces::VotingSubmitResult submitPoll(const interfaces::PollSubmission& /*poll*/) override { return {}; }
    interfaces::VotingSubmitResult submitVote(const std::string& /*poll_txid*/,
                                              const std::vector<uint8_t>& /*choice_offsets*/) override
    {
        return {};
    }
    std::unique_ptr<interfaces::Handler> handleNewPollReceived(interfaces::NewPollReceivedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
    std::unique_ptr<interfaces::Handler> handleNewVoteReceived(interfaces::NewVoteReceivedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }

private:
    std::mutex m_mutex;
    std::string m_current_poll_title;
};

//! Fake interfaces::MRC: empty defaults.
class FakeMRC : public interfaces::MRC
{
public:
    interfaces::MRCSnapshot snapshot(CAmount /*fee_boost*/, bool /*wallet_locked*/,
                                     bool /*researcher_eligible*/) override
    {
        return {};
    }
    interfaces::MRCSubmitResult submit(CAmount /*fee_boost*/, bool /*wallet_locked*/) override { return {}; }
    bool isOutOfSync() override { return false; }
    std::unique_ptr<interfaces::Handler> handleMRCChanged(interfaces::MRCChangedFn /*fn*/) override
    {
        return interfaces::MakeCleanupHandler([] {});
    }
};

//! A FakeWallet whose lock state, passphrase, unlock calls and send results
//! are scripted, and which logs every call of the six methods the unlock,
//! restore and send paths make (with the calling thread), so a test can check
//! which calls were made, in which order and on which thread. Every member can
//! be used from a test thread while another thread calls the fake. A call can
//! be held at its start on a FakeLatch, and each of the six methods can be
//! scripted to throw.
class ScriptedWallet : public FakeWallet
{
public:
    enum class Method { GetLockState, UnlockWallet, ElevateWallet, LockWallet, RestrictToStakingOnly, SendCoins, Op };

    //! One logged call. The passphrase, its capacity and the staking flag are
    //! set for unlockWallet and elevateWallet; the recipients, the coin control
    //! and the accepted fee for sendCoins.
    struct Call {
        Method method;
        std::thread::id thread;
        bool staking_only{false};
        std::size_t capacity{0};
        std::string passphrase;
        std::vector<interfaces::WalletSendRecipient> recipients;
        std::optional<interfaces::WalletCoinControl> coin_control;
        int64_t accepted_fee{0};
    };

    //! What a method with a throw switch set throws, after logging.
    enum class ThrowKind { None, Runtime, NonStd };

    // Optional latches, waited on when set: getLockState at its start,
    // unlockWallet after logging, sendCoins from the call m_send_latch_from on
    // (zero-based).
    std::atomic<FakeLatch*> m_read_latch{nullptr};
    std::atomic<FakeLatch*> m_unlock_latch{nullptr};
    std::atomic<FakeLatch*> m_send_latch{nullptr};
    std::atomic<int> m_send_latch_from{0};

    //! Logs an Op entry with the calling thread: a test's posted operation
    //! calls it, so the log shows where the operation ran among the wallet
    //! calls.
    void recordOp()
    {
        Call call{};
        call.method = Method::Op;
        call.thread = std::this_thread::get_id();
        Log(std::move(call));
    }

    //! Every call so far, in order.
    std::vector<Call> log() const
    {
        std::lock_guard<std::mutex> lock(m_log_mutex);
        return m_log;
    }
    int lockStateCalls() const { return Count(Method::GetLockState); }
    int unlockCalls() const { return Count(Method::UnlockWallet); }
    int elevateCalls() const { return Count(Method::ElevateWallet); }
    int lockCalls() const { return Count(Method::LockWallet); }
    int restrictCalls() const { return Count(Method::RestrictToStakingOnly); }
    int sendCalls() const { return Count(Method::SendCoins); }
    int opCalls() const { return Count(Method::Op); }

    //! Sets the lock state (crypted, locked, unlocked for staking only).
    void setLockState(bool crypted, bool locked, bool staking_only)
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_lock_state = interfaces::WalletLockState{};
        m_lock_state.crypted = crypted;
        m_lock_state.locked = locked;
        m_lock_state.unlocked_for_staking_only = staking_only;
    }
    interfaces::WalletLockState lockState() const
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return m_lock_state;
    }
    //! After the next getLockState, the state becomes `next`; that read
    //! itself returns the state before the change.
    void flipAfterNextRead(interfaces::WalletLockState next)
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_flip = next;
    }

    void setPassphrase(std::string passphrase)
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_passphrase = std::move(passphrase);
    }
    std::string passphrase() const
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return m_passphrase;
    }

    //! A scripted result makes restrictToStakingOnly return it and leave the
    //! state alone; std::nullopt restores the unscripted behaviour.
    void scriptRestrict(std::optional<bool> result)
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_restrict_result = result;
    }

    //! The results the next sendCoins calls return, in order.
    void scriptSend(std::vector<interfaces::SendCoinsResult> results)
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_send_results.assign(results.begin(), results.end());
    }

    //! Makes `method` throw after logging: a std::runtime_error carrying
    //! `text`, or a non-standard exception. ThrowKind::None clears it.
    void setThrow(Method method, ThrowKind kind, std::string text = {})
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_throws[method] = std::make_pair(kind, std::move(text));
    }

    //! Logs, waits on the read latch, reads the state, then applies a
    //! one-shot change set by flipAfterNextRead. It does not call
    //! FakeWallet::getLockState.
    interfaces::WalletLockState getLockState() override
    {
        Call call{};
        call.method = Method::GetLockState;
        call.thread = std::this_thread::get_id();
        Log(std::move(call));
        if (FakeLatch* latch = m_read_latch.load()) latch->wait();
        MaybeThrow(Method::GetLockState);
        std::lock_guard<std::mutex> lock(m_state_mutex);
        const interfaces::WalletLockState state = m_lock_state;
        if (m_flip) {
            m_lock_state = *m_flip;
            m_flip.reset();
        }
        return state;
    }

    //! The right passphrase without staking_only unlocks fully; with
    //! staking_only it leaves the state alone and returns whether the
    //! passphrase is right; a wrong one returns false.
    bool unlockWallet(const SecureString& passphrase, bool staking_only) override
    {
        Call call{};
        call.method = Method::UnlockWallet;
        call.thread = std::this_thread::get_id();
        call.staking_only = staking_only;
        call.capacity = passphrase.capacity();
        call.passphrase.assign(passphrase.begin(), passphrase.end());
        const std::string given = call.passphrase;
        Log(std::move(call));
        if (FakeLatch* latch = m_unlock_latch.load()) latch->wait();
        MaybeThrow(Method::UnlockWallet);
        std::lock_guard<std::mutex> lock(m_state_mutex);
        const bool right = given == m_passphrase;
        if (!right) return false;
        if (staking_only) return true;
        m_lock_state.crypted = true;
        m_lock_state.locked = false;
        m_lock_state.unlocked_for_staking_only = false;
        return true;
    }

    //! Widens a staking-only unlock given the right passphrase; false on a
    //! locked wallet and in every other case.
    bool elevateWallet(const SecureString& passphrase) override
    {
        Call call{};
        call.method = Method::ElevateWallet;
        call.thread = std::this_thread::get_id();
        call.capacity = passphrase.capacity();
        call.passphrase.assign(passphrase.begin(), passphrase.end());
        const std::string given = call.passphrase;
        Log(std::move(call));
        MaybeThrow(Method::ElevateWallet);
        std::lock_guard<std::mutex> lock(m_state_mutex);
        const bool staking_only =
            m_lock_state.crypted && !m_lock_state.locked && m_lock_state.unlocked_for_staking_only;
        if (!staking_only || given != m_passphrase) return false;
        m_lock_state.unlocked_for_staking_only = false;
        return true;
    }

    //! Locks a crypted wallet (true); an unencrypted one is left alone
    //! (false). It does not call the FakeWallet base.
    bool lockWallet() override
    {
        Call call{};
        call.method = Method::LockWallet;
        call.thread = std::this_thread::get_id();
        Log(std::move(call));
        MaybeThrow(Method::LockWallet);
        std::lock_guard<std::mutex> lock(m_state_mutex);
        if (!m_lock_state.crypted) return false;
        m_lock_state.locked = true;
        m_lock_state.unlocked_for_staking_only = false;
        return true;
    }

    //! A scripted result is returned as it is; unscripted, a fully unlocked
    //! wallet is narrowed to staking only (true), and anything else is false.
    bool restrictToStakingOnly() override
    {
        Call call{};
        call.method = Method::RestrictToStakingOnly;
        call.thread = std::this_thread::get_id();
        Log(std::move(call));
        MaybeThrow(Method::RestrictToStakingOnly);
        std::lock_guard<std::mutex> lock(m_state_mutex);
        if (m_restrict_result) return *m_restrict_result;
        const bool full = m_lock_state.crypted && !m_lock_state.locked && !m_lock_state.unlocked_for_staking_only;
        if (!full) return false;
        m_lock_state.unlocked_for_staking_only = true;
        return true;
    }

    //! Returns the next scripted result; past the end of the script, a default
    //! SendCoinsResult (TransactionCreationFailed).
    interfaces::SendCoinsResult sendCoins(const std::vector<interfaces::WalletSendRecipient>& recipients,
                                          const std::optional<interfaces::WalletCoinControl>& coin_control,
                                          int64_t accepted_fee) override
    {
        Call call{};
        call.method = Method::SendCoins;
        call.thread = std::this_thread::get_id();
        call.recipients = recipients;
        call.coin_control = coin_control;
        call.accepted_fee = accepted_fee;
        const int index = Log(std::move(call));
        FakeLatch* latch = m_send_latch.load();
        if (latch && index >= m_send_latch_from.load()) latch->wait();
        MaybeThrow(Method::SendCoins);
        std::lock_guard<std::mutex> lock(m_state_mutex);
        if (m_send_results.empty()) return interfaces::SendCoinsResult{};
        interfaces::SendCoinsResult result = m_send_results.front();
        m_send_results.pop_front();
        return result;
    }

private:
    //! Appends `call` to the log and returns how many calls of its method came
    //! before it. The log lock is released before the caller waits on a latch.
    int Log(Call call)
    {
        std::lock_guard<std::mutex> lock(m_log_mutex);
        int before = 0;
        for (const Call& logged : m_log) {
            if (logged.method == call.method) ++before;
        }
        m_log.push_back(std::move(call));
        return before;
    }

    int Count(Method method) const
    {
        std::lock_guard<std::mutex> lock(m_log_mutex);
        int count = 0;
        for (const Call& logged : m_log) {
            if (logged.method == method) ++count;
        }
        return count;
    }

    void MaybeThrow(Method method)
    {
        ThrowKind kind = ThrowKind::None;
        std::string text;
        {
            std::lock_guard<std::mutex> lock(m_state_mutex);
            const auto it = m_throws.find(method);
            if (it == m_throws.end()) return;
            kind = it->second.first;
            text = it->second.second;
        }
        if (kind == ThrowKind::Runtime) throw std::runtime_error(text);
        if (kind == ThrowKind::NonStd) throw 1;
    }

    mutable std::mutex m_log_mutex;
    std::vector<Call> m_log;

    //! Guards FakeWallet::m_lock_state and every scripted value below.
    mutable std::mutex m_state_mutex;
    std::optional<interfaces::WalletLockState> m_flip;
    std::string m_passphrase;
    std::optional<bool> m_restrict_result;
    std::deque<interfaces::SendCoinsResult> m_send_results;
    std::map<Method, std::pair<ThrowKind, std::string>> m_throws;
};

} // namespace qt_test

#endif // BITCOIN_QT_TEST_MODELFAKES_H
