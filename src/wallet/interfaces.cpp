// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/wallet.h"

#include "gridcoin/tx_message.h"
#include "hash.h"
#include "init.h"
#include "interfaces/connection_scope.h"
#include "interfaces/handler.h"
#include "interfaces/notification_dispatcher.h"
#include "key_io.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "scheduler.h"
#include "streams.h"
#include "util/strencodings.h"
#include "util/system.h"
#include "util/transfer_file.h"
#include "wallet/coincontrol.h"
#include "net_processing.h"
#include "wallet/transfer_sources.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace interfaces {

//! How long after a successful IPC encryption the core stops by itself, when the
//! GUI's connection has not closed by then.
static constexpr std::chrono::seconds ENCRYPT_SHUTDOWN_FALLBACK{60};

namespace {
std::mutex g_encryption_shutdown_mutex;
std::function<void()> g_encryption_shutdown_fn;
} // namespace

void SetEncryptionShutdownFnForTesting(std::function<void()> fn)
{
    std::lock_guard<std::mutex> lock(g_encryption_shutdown_mutex);
    g_encryption_shutdown_fn = std::move(fn);
}

void RequestShutdownAfterEncryption()
{
    // Copied out so the test function runs outside the leaf mutex.
    std::function<void()> fn;
    {
        std::lock_guard<std::mutex> lock(g_encryption_shutdown_mutex);
        fn = g_encryption_shutdown_fn;
    }

    if (fn) {
        fn();
    } else {
        StartShutdown();
    }
}

namespace {

//! Build the value snapshot of one wallet output. The maturity flag needs
//! cs_main (GetBlocksToMaturity walks the chain), which every caller holds.
WalletOutput MakeWalletOutput(const CWalletTx& wtx, unsigned int n, int depth)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    WalletOutput output;
    output.outpoint = COutPoint(wtx.GetHash(), n);
    output.amount = wtx.vout[n].nValue;

    CTxDestination address;
    if (ExtractDestination(wtx.vout[n].scriptPubKey, address)) {
        output.address = EncodeDestination(address);
    }

    output.depth = depth;
    output.time = wtx.GetTxTime();
    output.immature = wtx.IsCoinStake() && wtx.GetBlocksToMaturity() > 0 && depth > 0;

    return output;
}

//! Marks one wallet interface's transfer as live for as long as it exists.
class LiveTransfer
{
public:
    //! Null when a transfer of the interface owning \p flag is already live.
    static std::unique_ptr<LiveTransfer> Claim(const std::shared_ptr<std::atomic<bool>>& flag)
    {
        if (flag->exchange(true)) return nullptr;
        return std::unique_ptr<LiveTransfer>(new LiveTransfer(flag));
    }

    ~LiveTransfer() { m_flag->store(false); }

    LiveTransfer(const LiveTransfer&) = delete;
    LiveTransfer& operator=(const LiveTransfer&) = delete;

private:
    explicit LiveTransfer(std::shared_ptr<std::atomic<bool>> flag) : m_flag(std::move(flag)) {}

    const std::shared_ptr<std::atomic<bool>> m_flag;
};

//! A source that keeps its wallet interface's transfer live until it is
//! destroyed.
class LiveFileSource final : public FileSource
{
public:
    LiveFileSource(std::unique_ptr<FileSource> inner, std::unique_ptr<LiveTransfer> live)
        : m_live(std::move(live)), m_inner(std::move(inner))
    {
    }

    std::vector<FileEntryInfo> entries() override { return m_inner->entries(); }
    FileChunk read(uint32_t entry, uint32_t max_bytes) override { return m_inner->read(entry, max_bytes); }
    FileTrailer trailer(uint32_t entry) override { return m_inner->trailer(entry); }
    void cancel() override { m_inner->cancel(); }

private:
    // Declared first, so it is destroyed last: another transfer can begin only
    // once the inner source, and with it every snapshot, is gone.
    std::unique_ptr<LiveTransfer> m_live;
    std::unique_ptr<FileSource> m_inner;
};

//! Remove the snapshots in \p entries and empty it.
void RemoveSnapshots(std::vector<wallet::SnapshotEntry>& entries)
{
    for (const wallet::SnapshotEntry& entry : entries) {
        if (entry.path.empty()) continue;

        boost::system::error_code ec;
        fs::remove(entry.path, ec);

        if (ec) {
            LogPrintf("WARNING: %s: could not remove %s: %s", __func__, entry.path.string(), ec.message());
        }
    }

    entries.clear();
}

//! Steps 3 to 5 of Wallet::beginBackup(): refuse what cannot be backed up,
//! check that the transfer directory has room for it, and snapshot each
//! requested item there, in the order wallet, configuration file, settings
//! file. On failure the snapshots made so far are removed and \p entries is
//! left empty.
bool SnapshotBackupItems(const CWallet& wallet, uint8_t items, std::vector<wallet::SnapshotEntry>& entries,
                         std::string& error)
{
    const uint8_t wallet_item = static_cast<uint8_t>(BackupItem::Wallet);
    const uint8_t config_item = static_cast<uint8_t>(BackupItem::Config);
    const uint8_t settings_item = static_cast<uint8_t>(BackupItem::Settings);

    if (items == 0 || (items & ~(wallet_item | config_item | settings_item)) != 0) {
        error = strprintf("unknown backup items 0x%02x", static_cast<unsigned int>(items));
        return false;
    }

    if (items & config_item) {
        std::string reason;
        if (!wallet::ConfigBackupPermitted(reason)) {
            error = reason;
            return false;
        }
    }

    if ((items & wallet_item) && !wallet.fFileBacked) {
        error = "the wallet is not file-backed";
        return false;
    }

    // What each item is copied from. A configuration or settings file that
    // does not exist has nothing to back up; its entry carries a note instead.
    struct Item
    {
        std::string name;
        fs::path source;
        bool is_wallet{false};
        std::string missing_note;
    };

    std::vector<Item> plan;

    if (items & wallet_item) {
        plan.push_back({wallet.strWalletFile, GetDataDir() / wallet.strWalletFile, true, ""});
    }

    if (items & config_item) {
        const fs::path conf = GetConfigFile();
        plan.push_back({conf.filename().string(), conf, false, "no configuration file"});
    }

    if (items & settings_item) {
        fs::path settings;
        if (gArgs.GetSettingsPath(&settings)) {
            plan.push_back({settings.filename().string(), settings, false, "no settings file"});
        } else {
            plan.push_back({GRIDCOIN_SETTINGS_FILENAME, fs::path(), false, "no settings file"});
        }
    }

    uint64_t need = 0;

    for (Item& item : plan) {
        boost::system::error_code ec;

        if (!item.is_wallet && !fs::exists(item.source, ec)) {
            item.source.clear();
            continue;
        }

        const uintmax_t size = fs::file_size(item.source, ec);
        if (!ec) need += size;
    }

    if (!util::HasFreeSpace(wallet::GetTransferDir(), need, error)) return false;

    try {
        for (const Item& item : plan) {
            if (item.source.empty()) {
                entries.push_back({item.name, fs::path(), item.missing_note});
                continue;
            }

            const std::optional<fs::path> snapshot = item.is_wallet ? wallet::SnapshotWallet(wallet, error)
                                                                    : wallet::SnapshotPlainFile(item.source, error);
            if (!snapshot) {
                RemoveSnapshots(entries);
                return false;
            }

            entries.push_back({item.name, *snapshot, ""});
        }
    } catch (const std::exception& e) {
        // In process this runs on the GUI's thread, where an exception would end
        // the application.
        RemoveSnapshots(entries);
        error = strprintf("the backup failed: %s", e.what());
        return false;
    }

    return true;
}

//! In-process Wallet implementation: thin wrappers over CWallet. Balance
//! queries lock internally (CWallet takes LOCK2(cs_main, cs_wallet) itself),
//! so callers never hold core locks. Signal bridges convert the wallet's own
//! signal payloads to value types before they cross the boundary.
class WalletImpl : public Wallet
{
public:
    explicit WalletImpl(CWallet* wallet) : m_wallet(wallet) {}

    ~WalletImpl() override
    {
        if (m_connection_scope && m_closing_hook != 0) m_connection_scope->removeCloseHook(m_closing_hook);
    }

    WalletImpl(const WalletImpl&) = delete;
    WalletImpl& operator=(const WalletImpl&) = delete;

    int64_t getBalance() override { return m_wallet->GetBalance(); }

    int64_t getStake() override { return m_wallet->GetStake(); }

    int64_t getUnconfirmedBalance() override { return m_wallet->GetUnconfirmedBalance(); }

    int64_t getImmatureBalance() override { return m_wallet->GetImmatureBalance(); }

    CAmount getTransactionFeeRate() override
    {
        // The rate per kB CreateTransaction pays: the configured rate, or the
        // minimum fee of a send when that is higher.
        return std::max<CAmount>(nTransactionFee, GetBaseFee(CTransaction(), GMF_SEND));
    }

    bool tryGetBalances(WalletBalances& balances) override
    {
        // The balance scan iterates the wallet's full mapWallet and becomes
        // expensive on large wallets, so refresh-path callers must be able to
        // bow out cleanly when the core is holding the locks (e.g. during a
        // wallet rescan) instead of stalling the GUI thread. GetBalances()
        // re-locks recursively. The TRY_LOCK only covers contention: once the
        // locks are taken the scan runs to the end, which is why it is one pass.
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            return false;
        }
        TRY_LOCK(m_wallet->cs_wallet, lockWallet);
        if (!lockWallet) {
            return false;
        }

        const CWallet::Balances totals = m_wallet->GetBalances();
        balances.balance = totals.balance;
        balances.stake = totals.stake;
        balances.unconfirmed_balance = totals.unconfirmed;
        balances.immature_balance = totals.immature;

        return true;
    }

    int getNumTransactions() override
    {
        return WITH_LOCK(m_wallet->cs_wallet,
                         return static_cast<int>(m_wallet->mapWallet.size()));
    }

    WalletLockState getLockState() override
    {
        // One round trip for the split build, and now two reads rather than
        // three: the scope answers locked and the restriction together, so
        // those two can no longer be caught disagreeing.
        //
        // Deliberately does NOT take cs_wallet: getEncryptionStatus() is polled
        // from the GUI thread and must not stall behind a long cs_wallet holder
        // (the O(N) updateWallet path). GetUnlockScope() takes cs_KeyStore,
        // which IsLocked() always did, so the profile is unchanged.
        //
        // A concurrent transition can still land between the crypted read and
        // the scope read. That is harmless: every pair is a valid status and
        // the next poll corrects it.
        const bool crypted = m_wallet->IsCrypted();
        const UnlockScope scope = m_wallet->GetUnlockScope();

        WalletLockState state;
        state.crypted = crypted;
        state.locked = (scope == UnlockScope::Locked);
        state.unlocked_for_staking_only = (scope == UnlockScope::StakingOnly);
        state.staking_only_flag = state.unlocked_for_staking_only;
        return state;
    }

    bool isUnlockedForStakingOnly() override
    {
        return m_wallet->GetUnlockScope() == UnlockScope::StakingOnly;
    }

    bool getUnlockStakingOnlyFlag() override
    {
        return m_wallet->GetUnlockScope() == UnlockScope::StakingOnly;
    }

    bool encryptWallet(const SecureString& passphrase) override
    {
        if (!m_wallet->EncryptWallet(passphrase)) return false;

        // Served on an IPC connection, the GUI cannot restart the core, and the
        // wallet database still holds plaintext residue until the core stops, as
        // the RPC encryptwallet makes it stop. The core stops when this
        // connection closes (the GUI closes it after telling the user), and the
        // scheduler stops it after a delay if the GUI never does. In process
        // (no scope) the monolithic GUI quits itself.
        if (m_connection_scope) {
            m_connection_scope->addCloseHook(&RequestShutdownAfterEncryption);

            // A null handle means shutdown has begun: nothing to add.
            if (CScheduler* scheduler = g_scheduler_handle.load(std::memory_order_acquire)) {
                // A refusal means the scheduler is stopping, which is shutdown
                // already under way, so the result is not needed.
                (void)scheduler->scheduleFromNowIfRunning(&RequestShutdownAfterEncryption,
                                                          ENCRYPT_SHUTDOWN_FALLBACK);
            }
        }

        return true;
    }

    bool lockWallet() override { return m_wallet->Lock(); }
    bool restrictToStakingOnly() override { return m_wallet->RestrictToStakingOnly(); }

    bool elevateWallet(const SecureString& passphrase) override
    {
        // A client that still asks for this predates beginElevation(), and the
        // handshake's schema major keeps such a client from connecting. Refused
        // without reading the passphrase, so it is not counted as an attempt.
        LogPrintf("WARNING: %s: refused. elevateWallet is retired; the client begins a "
                  "capability for each operation instead.", __func__);
        return false;
    }

    bool beginElevation(const SecureString& passphrase, CapabilityKind kind) override
    {
        if (!IsKnownCapabilityKind(kind)) {
            LogPrintf("WARNING: %s: refused a capability of unknown kind %u", __func__,
                      static_cast<unsigned int>(kind));
            return false;
        }

        // The connection this wallet is served on may go away while the
        // passphrase is checked. Its scope reads closed from the moment its
        // teardown begins, before the slot hook ends the IPC capability (an
        // exchange under the capability mutex), and the begin asks the scope in
        // the same capability-mutex section that installs. So either the install
        // comes before the hook's exchange, and the hook ends it before the slot
        // can pass to another GUI, or it comes after, and nothing is installed:
        // the slot, and any capability the next GUI has begun, are left alone.
        // In process (the monolithic build) there is no connection and no check.
        // A wait for its turn ends, unchecked, when the connection closes.
        const std::optional<PassphraseAttemptCancel> given_up = GiveUpWhenConnectionCloses();

        bool begun = false;
        try {
            begun = m_wallet->BeginIpcElevation(passphrase, kind, m_connection_closed);
        } catch (const std::exception& e) {
            // Nothing the client could use is left behind: a capability this
            // call may have issued before the throw is ended with the slot. Once
            // the connection has closed the slot is left alone, since it may be
            // the next GUI's, and the slot hook ended what was this one's.
            LogPrintf("ERROR: %s: no capability was begun: %s", __func__, e.what());
            m_wallet->EndIpcElevation(m_connection_closed);
            return false;
        }
        return begun;
    }

    // Ends this connection's capability. Once the connection has closed the
    // slot is left alone: it may hold the next GUI's capability by then.
    void endElevation() override { m_wallet->EndIpcElevation(m_connection_closed); }

    bool unlockWallet(const SecureString& passphrase, bool staking_only) override
    {
        // A wait for its turn ends, unchecked, when the connection closes.
        const std::optional<PassphraseAttemptCancel> given_up = GiveUpWhenConnectionCloses();

        // One call. The scope is installed with the master key, so there is no
        // window in which the wallet is unlocked while still carrying the
        // previous unlock's restriction -- which, since nothing cleared that
        // restriction on lock, could be the permissive one.
        return m_wallet->Unlock(passphrase,
                                staking_only ? UnlockScope::StakingOnly : UnlockScope::Full);
    }

    bool changeWalletPassphrase(const SecureString& old_passphrase,
                                const SecureString& new_passphrase) override
    {
        // No preliminary lock. ChangeWalletPassphrase leaves the unlock as it
        // is, so locking here would silently end a staking-only unlock and its
        // timer that the RPC path keeps. It also takes cs_wallet itself, so
        // holding it here would buy nothing. A wait for its turn ends, unchecked,
        // when the connection closes.
        const std::optional<PassphraseAttemptCancel> given_up = GiveUpWhenConnectionCloses();
        return m_wallet->ChangeWalletPassphrase(old_passphrase, new_passphrase);
    }

    // The core no longer writes to a path the client names. A client that
    // still asks predates beginBackup(), and the handshake's schema major keeps
    // such a client from connecting.
    bool backupWallet(const std::string& /*dest*/) override
    {
        LogPrintf("WARNING: %s: refused. IPC: use the transfer channel.", __func__);
        return false;
    }

    bool backupConfigFile(const std::string& /*dest*/) override
    {
        LogPrintf("WARNING: %s: refused. IPC: use the transfer channel.", __func__);
        return false;
    }

    std::unique_ptr<FileSource> beginBackup(uint8_t items, std::string& error_out) override
    {
        error_out.clear();

        // One transfer at a time on this interface. The claim is made here and
        // held by the source returned, so two calls cannot both pass it.
        std::unique_ptr<LiveTransfer> live = LiveTransfer::Claim(m_transfer_live);
        if (!live) {
            error_out = "a transfer is already in progress";
            return nullptr;
        }

        // A copy of the wallet file is the keys, so the snapshots are made under
        // the Backup capability the client began. An unencrypted or fully
        // unlocked wallet needs none. The slot is read once, so the capability
        // ended below is the one this call ran under.
        const std::optional<CapabilityId> id = m_wallet->IpcCapability(m_connection_closed);
        CapabilityScope scope(*m_wallet, id, CapabilityKind::Backup);
        if (!m_wallet->CanUseKeys()) {
            error_out = "Enter the wallet passphrase to back up the wallet.";
            return nullptr;
        }

        std::vector<wallet::SnapshotEntry> entries;
        const bool made = SnapshotBackupItems(*m_wallet, items, entries, error_out);

        // A capability authorises one operation, and this attempt was it,
        // whether the snapshots were made or not. Ended now, so the source holds
        // none. Only when this call ran under it: on an unencrypted or fully
        // unlocked wallet the gate passes without one, and a capability of
        // another kind the client holds stays for its own operation. Only this
        // call's capability is ended: one begun into the slot meanwhile stays,
        // and once the connection has closed the slot is left alone.
        if (scope.Active()) m_wallet->EndIpcElevationIf(*id, m_connection_closed);

        if (!made) return nullptr;

        return std::make_unique<LiveFileSource>(wallet::MakeSnapshotSource(std::move(entries), m_connection_scope),
                                                std::move(live));
    }

    MessageSignStatus signMessage(const std::string& address, const std::string& message,
                                  std::string& signature_out) override
    {
        const CTxDestination dest = DecodeDestination(address);
        const CKeyID* key_id = std::get_if<CKeyID>(&dest);
        if (!key_id) {
            return MessageSignStatus::KeyNotAvailable;
        }

        // The key is read under the capability the client began for this. On
        // a wallet unlocked for staking only there is none otherwise, and the
        // keystore refuses the key.
        CapabilityScope scope(*m_wallet, m_wallet->IpcCapability(m_connection_closed), CapabilityKind::SignMessage);

        try {
            CKey key;
            if (!WITH_LOCK(m_wallet->cs_wallet, return m_wallet->GetKey(*key_id, key))) {
                return MessageSignStatus::KeyNotAvailable;
            }

            CDataStream ss(SER_GETHASH, 0);
            ss << strMessageMagic;
            ss << message;

            std::vector<unsigned char> signature;
            if (!key.SignCompact(Hash(ss), signature)) {
                return MessageSignStatus::SigningFailed;
            }

            signature_out = EncodeBase64(signature.data(), signature.size());
            return MessageSignStatus::OK;
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: %s", __func__, e.what());
            return MessageSignStatus::SigningFailed;
        }
    }

    MessageVerifyStatus verifyMessage(const std::string& address, const std::string& message,
                                      const std::vector<unsigned char>& signature) override
    {
        const CTxDestination dest = DecodeDestination(address);

        CDataStream ss(SER_GETHASH, 0);
        ss << strMessageMagic;
        ss << message;

        CPubKey pubkey;
        if (!pubkey.RecoverCompact(Hash(ss), signature)) {
            return MessageVerifyStatus::RecoverFailed;
        }

        if (CTxDestination(pubkey.GetID()) != dest) {
            return MessageVerifyStatus::AddressMismatch;
        }

        return MessageVerifyStatus::OK;
    }

    std::vector<WalletAddress> getAddresses() override
    {
        std::vector<WalletAddress> result;

        LOCK(m_wallet->cs_wallet);
        result.reserve(m_wallet->mapAddressBook.size());
        for (const auto& item : m_wallet->mapAddressBook) {
            const CTxDestination& dest = item.first;
            WalletAddress entry;
            entry.address = EncodeDestination(dest);
            entry.label = item.second.name;
            entry.is_mine = IsMine(*m_wallet, dest) != ISMINE_NO;
            result.push_back(std::move(entry));
        }

        return result;
    }

    bool getAddressLabel(const std::string& address, std::string& label_out) override
    {
        const CTxDestination dest = DecodeDestination(address);
        if (!IsValidDestination(dest)) {
            return false;
        }

        LOCK(m_wallet->cs_wallet);
        const auto it = m_wallet->mapAddressBook.find(dest);
        if (it == m_wallet->mapAddressBook.end()) {
            return false;
        }

        label_out = it->second.name;
        return true;
    }

    bool isMine(const std::string& address) override
    {
        const CTxDestination dest = DecodeDestination(address);
        if (!IsValidDestination(dest)) {
            return false;
        }

        LOCK(m_wallet->cs_wallet);
        return IsMine(*m_wallet, dest) != ISMINE_NO;
    }

    void setAddressBook(const std::string& address, const std::string& label) override
    {
        const CTxDestination dest = DecodeDestination(address);
        if (!IsValidDestination(dest)) {
            return;
        }

        // SetAddressBookName takes cs_wallet itself and fires NotifyAddressBookChanged.
        m_wallet->SetAddressBookName(dest, label);
    }

    bool delAddressBook(const std::string& address) override
    {
        const CTxDestination dest = DecodeDestination(address);
        if (!IsValidDestination(dest)) {
            return false;
        }

        // DelAddressBookName takes cs_wallet itself and fires NotifyAddressBookChanged.
        return m_wallet->DelAddressBookName(dest);
    }

    bool getNewReceiveAddress(std::string& address_out) override
    {
        // The reserved key stays node-side and only its encoded address is
        // returned.
        //
        // A fresh key or nothing (fAllowReuse=false): an empty pool reports
        // false rather than handing back the wallet's default key, which the
        // user already has. The address book draws first with no capability;
        // on false, for a wallet locked or unlocked for staking only, it asks
        // for the passphrase and draws again under a Send capability (the one
        // under which a send refills the pool for its change key), which lets
        // the pool refill.
        CapabilityScope scope(*m_wallet, m_wallet->IpcCapability(m_connection_closed), CapabilityKind::Send);

        try {
            CPubKey new_key;
            if (!m_wallet->GetKeyFromPool(new_key, false)) {
                return false;
            }

            address_out = EncodeDestination(new_key.GetID());
            return true;
        } catch (const std::exception& e) {
            // A refill can throw when the wallet locks while it runs.
            LogPrintf("ERROR: %s: %s", __func__, e.what());
            return false;
        }
    }

    bool getNewReceiveAddressWithLabel(const std::string& label, std::string& address_out) override
    {
        // fAllowReuse=false to match the researcher pool page's former
        // getKeyFromPool(..., false) call: a deliberately fresh receive key.
        //
        // That page is part of the researcher wizard. It draws first with no
        // capability, and on false asks for the passphrase and draws again
        // under a Contract capability, which lets the pool refill.
        CapabilityScope scope(*m_wallet, m_wallet->IpcCapability(m_connection_closed), CapabilityKind::Contract);

        try {
            CPubKey new_key;
            if (!m_wallet->GetKeyFromPool(new_key, false)) {
                return false;
            }

            if (!label.empty()) {
                m_wallet->SetAddressBookName(new_key.GetID(), label);
            }

            address_out = EncodeDestination(new_key.GetID());
            return true;
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: %s", __func__, e.what());
            return false;
        }
    }

    std::vector<std::string> getUnbookedReceiveAddresses() override
    {
        std::vector<std::string> result;

        // cs_wallet is recursive, so it covers both GetAddressBalances (which
        // re-locks internally) and the mapAddressBook membership test below.
        LOCK(m_wallet->cs_wallet);
        const std::map<CTxDestination, int64_t> balances = m_wallet->GetAddressBalances();
        for (const auto& [dest, balance] : balances) {
            if (balance > 0 && m_wallet->mapAddressBook.count(dest) == 0) {
                result.push_back(EncodeDestination(dest));
            }
        }

        return result;
    }

    bool getPubKey(const CKeyID& address, CPubKey& pub_key_out) override
    {
        return m_wallet->GetPubKey(address, pub_key_out);
    }

    bool getKeyFromPool(const std::string& label, CPubKey& pub_key_out) override
    {
        if (!m_wallet->GetKeyFromPool(pub_key_out, false)) {
            return false;
        }

        if (!label.empty()) {
            m_wallet->SetAddressBookName(pub_key_out.GetID(), label);
        }

        return true;
    }

    std::vector<WalletOutput> getOutputs(const std::vector<COutPoint>& outpoints) override
    {
        std::vector<WalletOutput> outputs;

        LOCK2(cs_main, m_wallet->cs_wallet);

        for (const COutPoint& outpoint : outpoints) {
            auto it = m_wallet->mapWallet.find(outpoint.hash);
            if (it == m_wallet->mapWallet.end()) continue;
            if (outpoint.n >= it->second.vout.size()) continue;
            int depth = it->second.GetDepthInMainChain();
            if (depth < 0) continue;
            outputs.push_back(MakeWalletOutput(it->second, outpoint.n, depth));
        }

        return outputs;
    }

    CoinControlSummary computeCoinControlSummary(const WalletCoinControl& selection,
                                                 const std::vector<int64_t>& recipient_amounts,
                                                 bool subtract_fee_from_amount) override
    {
        CoinControlSummary summary;

        // Recipient side: pay amount, low-output flag, and the dummy outputs
        // GetMinFee's dust check inspects. Mirrors the pre-migration
        // updateLabels() exactly (the dummy script content is irrelevant to
        // the fee; only the count and nValue matter).
        CAmount pay_amount = 0;
        CMutableTransaction tx_dummy;
        for (const int64_t amount : recipient_amounts) {
            pay_amount += amount;
            if (amount > 0) {
                if (amount < CENT) {
                    summary.low_output = true;
                }
                tx_dummy.vout.push_back(CTxOut(amount, (CScript)std::vector<unsigned char>(24, 0)));
            }
        }

        CAmount amount = 0;
        unsigned int bytes_inputs = 0;
        unsigned int quantity = 0;

        LOCK2(cs_main, m_wallet->cs_wallet);

        for (const COutPoint& outpoint : selection.selected) {
            auto it = m_wallet->mapWallet.find(outpoint.hash);
            if (it == m_wallet->mapWallet.end()) continue;
            if (outpoint.n >= it->second.vout.size()) continue;
            if (it->second.GetDepthInMainChain() < 0) continue; // skip vanished/conflicted

            quantity++;
            amount += it->second.vout[outpoint.n].nValue;

            // Byte estimate per input from pubkey compression (148 compressed
            // else 180; 148 in every error/non-pubkeyhash case), matching the
            // former GUI-side getOutputs()+getPubKey() loop.
            CTxDestination address;
            if (ExtractDestination(it->second.vout[outpoint.n].scriptPubKey, address)) {
                const CKeyID* key_id = std::get_if<CKeyID>(&address);
                CPubKey pubkey;
                if (key_id && m_wallet->GetPubKey(*key_id, pubkey)) {
                    bytes_inputs += (pubkey.IsCompressed() ? 148 : 180);
                } else {
                    bytes_inputs += 148;
                }
            } else {
                bytes_inputs += 148;
            }
        }

        summary.quantity = static_cast<int>(quantity);
        summary.amount = amount;

        if (quantity > 0) {
            // Always assume +1 output for change here.
            summary.bytes = bytes_inputs
                + ((recipient_amounts.size() > 0 ? recipient_amounts.size() + 1 : 2) * 34) + 10;

            const CAmount fee = nTransactionFee * (1 + (int64_t)summary.bytes / 1000);
            const CAmount min_fee = GetMinFee(CTransaction(tx_dummy), 1000, GMF_SEND, summary.bytes);
            summary.fee = std::max(fee, min_fee);

            if (pay_amount > 0) {
                // When subtracting fee from amount, the fee is absorbed by the
                // recipients rather than coming from the change output.
                summary.change = subtract_fee_from_amount
                    ? amount - pay_amount
                    : amount - summary.fee - pay_amount;

                // A sub-cent change output isn't worth creating: absorb it into
                // the fee. The fee INCREASES by the change removed (fee + change),
                // matching CreateTransaction's behaviour -- the pre-migration GUI
                // code set fee = change here, dropping the already-computed
                // sub-cent base fee and under-reporting the fee by that amount.
                if (summary.fee < CENT && summary.change > 0 && summary.change < CENT) {
                    summary.fee += summary.change;
                    summary.change = 0;
                }

                if (summary.change == 0) {
                    summary.bytes -= 34;
                }
            }

            summary.after_fee = amount - summary.fee;
            if (summary.after_fee < 0) {
                summary.after_fee = 0;
            }
        }

        return summary;
    }

    unsigned int getMaxConsolidationInputs() override { return GetMaxInputsForConsolidationTxn(); }

    SendCoinsResult sendCoins(const std::vector<WalletSendRecipient>& recipients,
                              const std::optional<WalletCoinControl>& coin_control,
                              int64_t accepted_fee) override
    {
        // The whole flow runs under the Send capability the client began: the
        // build signs, and a change key can refill the key pool.
        CapabilityScope scope(*m_wallet, m_wallet->IpcCapability(m_connection_closed), CapabilityKind::Send);

        // Nothing may escape to the caller: in the single-process build that is
        // the GUI thread, where an exception ends the application. What can
        // throw here is the build (a key-pool refill for the change key when
        // the wallet locks while it runs), before anything is committed; the
        // steps after the commit catch their own.
        try {
            return SendCoinsUnderScope(recipients, coin_control, accepted_fee);
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: the send failed: %s", __func__, e.what());
            return {SendCoinsStatus::TransactionCreationFailed};
        }
    }

    // NOTE: like every other uiInterface forwarder (see node/interfaces.cpp and
    // gridcoin/interfaces.cpp), these must be wrapped in ForwardNotify (which
    // carries the GuardNotify catch). They are not merely GUI conveniences:
    // CWallet emits them from inside state transitions that are only
    // half-complete at the emission point. CWallet::SetAddressBookName, for
    // example, updates mapAddressBook, emits NotifyAddressBookChanged, and only THEN
    // writes the name to the wallet database -- so an unguarded throw here (in
    // multiprocess, when the GUI has gone away) unwinds before the write and loses
    // the label permanently. DelAddressBookName has the mirror-image hazard.
    // boost::signals2 also stops calling the remaining slots once one throws, which
    // would silently deprive the node-side WalletTxSource of the same notification.
    std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) override
    {
        return MakeSignalHandler(m_wallet->NotifyStatusChanged.connect(
            interfaces::ForwardNotify([fn = std::move(fn)](CCryptoKeyStore*) { fn(); })));
    }

    std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) override
    {
        return MakeSignalHandler(m_wallet->NotifyAddressBookChanged.connect(
            interfaces::ForwardNotify([fn = std::move(fn)](CWallet*,
                                 const CTxDestination& address,
                                 const std::string& label,
                                 bool is_mine,
                                 const std::string& purpose,
                                 ChangeType status) {
                fn(EncodeDestination(address), label, is_mine, purpose, status);
            })));
    }

    std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) override
    {
        return MakeSignalHandler(m_wallet->NotifyTransactionChanged.connect(
            interfaces::ForwardNotify([fn = std::move(fn)](CWallet*, const uint256& tx_hash, ChangeType status) {
                fn(tx_hash, status);
            })));
    }

    void bindConnectionScope(std::shared_ptr<ConnectionScope> scope) override
    {
        // Bound again, the earlier connection's hook goes, so that connection's
        // close no longer reaches this wallet and the hook is not left behind.
        if (m_connection_scope && m_closing_hook != 0) m_connection_scope->removeCloseHook(m_closing_hook);
        m_closing_hook = 0;

        m_connection_scope = std::move(scope);
        m_connection_closed = ConnectionClosedCheck(m_connection_scope);
        if (m_connection_scope) {
            // Runs now, and returns 0, if the connection has already closed.
            m_closing_hook = m_connection_scope->addCloseHook([closing = m_connection_closing] { *closing = true; });
        }
    }

private:
    //! On a wallet served over IPC, the cancel token for a passphrase attempt
    //! made on this thread: set once the connection closes, so an attempt still
    //! waiting for its turn is given up with its passphrase unchecked (nothing
    //! counted, no delay) rather than checked for a client that has gone. None in
    //! process, where the caller sets its own token (the GUI's dialog does) and
    //! this must not replace it.
    std::optional<PassphraseAttemptCancel> GiveUpWhenConnectionCloses() const
    {
        if (!m_connection_scope) return std::nullopt;
        return std::optional<PassphraseAttemptCancel>(std::in_place, *m_connection_closing);
    }

    //! sendCoins() without its capability scope and exception boundary.
    SendCoinsResult SendCoinsUnderScope(const std::vector<WalletSendRecipient>& recipients,
                                        const std::optional<WalletCoinControl>& coin_control,
                                        int64_t accepted_fee);

    CWallet* m_wallet;
    //! The IPC connection this wallet is served on (bindConnectionScope), or null
    //! in process. Set once, before the wallet is handed out, and only read after.
    //! Nothing is called on it when the wallet is destroyed: what registers a hook
    //! in it removes the hook itself.
    std::shared_ptr<ConnectionScope> m_connection_scope{nullptr};
    //! Whether that connection has closed, asked under the wallet's capability
    //! mutex wherever this wallet begins, reads or ends the IPC capability.
    //! Empty in process, where there is no connection and nothing to check.
    std::function<bool()> m_connection_closed{};
    //! Set by a close hook on that connection, registered in
    //! bindConnectionScope() and removed when this wallet is bound again or
    //! destroyed; the cancel token of the passphrase attempts this wallet makes
    //! (GiveUpWhenConnectionCloses).
    const std::shared_ptr<std::atomic<bool>> m_connection_closing{std::make_shared<std::atomic<bool>>(false)};
    ConnectionScope::HookId m_closing_hook{0};
    //! Whether a transfer this interface began is live (LiveTransfer). Shared
    //! with that transfer, which may outlive the interface.
    const std::shared_ptr<std::atomic<bool>> m_transfer_live{std::make_shared<std::atomic<bool>>(false)};
};

SendCoinsResult WalletImpl::SendCoinsUnderScope(const std::vector<WalletSendRecipient>& recipients,
                                                const std::optional<WalletCoinControl>& coin_control,
                                                int64_t accepted_fee)
{
    // Refuse a staking-only wallet here, before the balance checks below can
    // answer first: a staking-only wallet is the reason regardless of the
    // balance. The builder refuses too, which is what makes the restriction
    // structural, and the failure branch after the build re-checks for a
    // narrowing that lands between this check and the build.
    //
    // Nothing else on this path guards the unlock: this layer is what the GUI
    // uses, and EnsureWalletIsUnlocked is RPC-only. A capability scope open on
    // this thread lets the send through.
    if (m_wallet->IsUnlockedForStakingOnly() && !m_wallet->CanUseKeys()) {
        return {SendCoinsStatus::WalletUnlockedForStakingOnly};
    }

    // Reconstruct the wallet-side CCoinControl from the boundary value type
    // (the raw class does not cross the interface).
    CCoinControl ctrl;
    const CCoinControl* coin_control_ptr = nullptr;
    if (coin_control) {
        if (!coin_control->dest_change.empty()) {
            ctrl.destChange = DecodeDestination(coin_control->dest_change);
        }
        ctrl.fAllowWatchOnly = coin_control->allow_watch_only;
        for (const COutPoint& outpoint : coin_control->selected) {
            ctrl.Select(outpoint);
        }
        coin_control_ptr = &ctrl;
    }

    // Defensive guards at the trust boundary. The GUI pre-checks both
    // conditions, but the node side must not rely on the client: an empty
    // recipient list would index recipients[0] below, and an undecodable
    // address becomes CNoDestination, which CScript::SetDestination renders
    // as an EMPTY script — CreateTransaction would fund that
    // anyone-can-spend output.
    if (recipients.empty()) {
        return {SendCoinsStatus::TransactionCreationFailed};
    }

    for (const WalletSendRecipient& rcp : recipients) {
        if (!IsValidDestination(DecodeDestination(rcp.address))) {
            return {SendCoinsStatus::InvalidAddress};
        }
    }

    // Amounts likewise: a non-positive or out-of-range amount must not reach
    // the balance arithmetic below. Checking MoneyRange on the running total
    // after every add keeps the arithmetic itself defined: the total is at
    // most MAX_MONEY before an add and each addend is at most MAX_MONEY, so
    // no intermediate sum can approach INT64_MAX.
    int64_t total = 0;
    for (const WalletSendRecipient& rcp : recipients) {
        if (rcp.amount <= 0 || !MoneyRange(rcp.amount)) {
            return {SendCoinsStatus::InvalidAmount};
        }
        total += rcp.amount;
        if (!MoneyRange(total)) {
            return {SendCoinsStatus::InvalidAmount};
        }
    }

    // Locked across scan AND dereference: COutput carries raw pointers into
    // mapWallet, and the old GUI-side code's unlocked window between
    // AvailableCoins returning and this loop reading out.tx let a concurrent
    // wallet-transaction erasure free the pointed-at CWalletTx.
    int64_t nBalance = 0;
    {
        LOCK2(cs_main, m_wallet->cs_wallet);

        std::vector<COutput> vCoins;
        m_wallet->AvailableCoins(vCoins, true, coin_control_ptr, false);

        for (auto const& out : vCoins)
            nBalance += out.tx->vout[out.i].nValue;
    }

    bool fAnySubtractFeeFromAmount = false;
    for (const WalletSendRecipient& rcp : recipients)
    {
        if (rcp.subtract_fee_from_amount)
        {
            fAnySubtractFeeFromAmount = true;
            break;
        }
    }

    if (total > nBalance)
    {
        return {SendCoinsStatus::AmountExceedsBalance};
    }

    if (!fAnySubtractFeeFromAmount && (total + nTransactionFee) > nBalance)
    {
        return {SendCoinsStatus::AmountWithFeeExceedsBalance, nTransactionFee};
    }

    CWalletTx wtx;

    if (fAnySubtractFeeFromAmount)
    {
        wtx.mapValue["subtractFeeFromAmount"] = "1";
    }

    // Set inside the send scope, reported only once the payment has actually
    // committed: a warning logged before the send would claim a payment that a
    // later failure never made.
    bool message_discarded = false;

    // Declared before the send scope so it announces after that scope unlocks.
    // This scope holds cs_wallet all the way through CommitTransaction, so
    // CommitTransaction releasing its own guard does not help here -- the relay
    // path takes a peer's cs_inventory and SendMessages takes cs_inventory then
    // cs_wallet (#3391). This is the GUI send path that first exposed that cycle.
    DeferredRelay relay;

    {
        LOCK2(cs_main, m_wallet->cs_wallet);

        if (!recipients[0].message.empty()) {
            // Inside this scope deliberately, not before it. nBestHeight is
            // GUARDED_BY(cs_main), and reading it under a lock of its own would
            // still leave the decision free to go stale before CreateTransaction
            // below acts on it: the tip could cross the disable height in the gap,
            // and the contract attached on the strength of the old answer would
            // then be rejected by CheckContracts -- failing the whole payment
            // rather than merely dropping the message. Deciding and building under
            // one lock means both see the same tip.
            //
            // The two send RPCs make the same check already holding
            // LOCK2(cs_main, cs_wallet) taken at the top of each, so this is the
            // same shape rather than a different rule.
            //
            // nBestHeight + 1 is the first height this transaction could be mined
            // at, which is what CheckContracts will judge it by.
            if (!IsMessageContractEnabled(nBestHeight + 1)) {
                // The enforcement half of the GUI's message handling. The send
                // page also hides the field, but that is presentation: the field
                // can be stale, the GUI can be an older build, and the request
                // need not come from the GUI at all.
                //
                // The two halves can legitimately disagree, which is why both
                // exist. GetMessageContractDisableHeight() consults gArgs, and in
                // a multiprocess run the GUI process has its own -- a developer
                // passing -messagecontractdisableheight to the node alone leaves
                // the GUI still offering the field, and this catches it. On
                // mainnet the height comes from compiled-in chainparams and the
                // two cannot diverge.
                message_discarded = true;
            } else {
                CMutableTransaction mtx;
                mtx.vContracts.emplace_back(GRC::MakeContract<GRC::TxMessage>(
                    GRC::ContractAction::ADD,
                    recipients[0].message));
                static_cast<CTransaction&>(wtx) = CTransaction(std::move(mtx));
            }
        }

        // Sendmany. The recipients with "subtract fee from amount" pay the fee
        // and any message burn between them, inside CreateTransaction, so the
        // sender is debited exactly the amounts entered and one call settles
        // the fee: the coin-selection target no longer moves with it.
        std::vector<std::pair<CScript, int64_t>> vecSend;
        std::set<size_t> subtract_fee_outputs;
        for (const WalletSendRecipient& rcp : recipients) {
            CScript scriptPubKey;
            scriptPubKey.SetDestination(DecodeDestination(rcp.address));
            if (rcp.subtract_fee_from_amount) {
                subtract_fee_outputs.insert(vecSend.size());
            }
            vecSend.push_back(std::make_pair(scriptPubKey, rcp.amount));
        }

        CReserveKey keyChange(m_wallet);
        int64_t nFeeRequired = 0;
        bool fCreated = m_wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, coin_control_ptr,
                                                    /*change_back_to_input_address=*/false,
                                                    /*renewal_access=*/nullptr,
                                                    subtract_fee_outputs);

        if (!fCreated)
        {
            // Re-read the scope, here, where cs_wallet IS held. The early check
            // above is not under it, and RestrictToStakingOnly takes cs_wallet,
            // so a narrowing can land between the two: the builder then refuses
            // correctly but this branch would report a generic creation failure
            // and send the user to look at their balance. First, because a
            // staking-only wallet is the reason regardless of what the fee
            // arithmetic below would say.
            if (m_wallet->IsUnlockedForStakingOnly() && !m_wallet->CanUseKeys()) {
                return {SendCoinsStatus::WalletUnlockedForStakingOnly};
            }

            // The fee and burn at which the build stopped leave nothing of a
            // subtract-fee recipient's amount. The same arithmetic the build
            // used, so this names that failure and no other. Reported as the
            // sum: with a message, the burn alone can be what tips it over.
            const int64_t nSubtracted = nFeeRequired + MessageBurnAmount(wtx);

            if (!subtract_fee_outputs.empty() && !SubtractFromOutputs(vecSend, subtract_fee_outputs, nSubtracted))
            {
                return {SendCoinsStatus::FeeExceedsSubtractedAmount, nSubtracted};
            }

            if (!fAnySubtractFeeFromAmount && (total + nFeeRequired) > nBalance)
            {
                return {SendCoinsStatus::AmountWithFeeExceedsBalance, nFeeRequired};
            }
            return {SendCoinsStatus::TransactionCreationFailed};
        }

        // Fee-confirmation gate, replacing the ThreadSafeAskFee modal that
        // used to block right here — inside LOCK2, across arbitrary user
        // think-time (doc/multiprocess_design.md section 4.5). Same
        // thresholds the old GUI handler applied: auto-accept a fee below
        // the base fee or within the configured transaction fee. A larger
        // fee needs the user's explicit acceptance: unless the caller
        // already accepted at least this much, return without committing —
        // this scope's locks release and keyChange's destructor returns the
        // reserved key to the pool. The caller re-invokes with the accepted
        // fee, the transaction is recreated from current wallet state, and
        // any fee that meanwhile grew past the accepted amount lands back
        // here, so no unaccepted fee is ever committed.
        // Threshold on the actual transaction rather than the dummy the old
        // GUI handler used (it had no wtx in scope; GetBaseFee is
        // version-sensitive, and today wtx carries the same default version
        // as a fresh CMutableTransaction, so this is behavior-identical).
        int64_t nMinFee = GetBaseFee(wtx, GMF_SEND);
        if (nFeeRequired >= nMinFee && nFeeRequired > nTransactionFee && nFeeRequired > accepted_fee)
        {
            return {SendCoinsStatus::FeeConfirmationRequired, nFeeRequired};
        }

        if (!m_wallet->CommitTransaction(wtx, keyChange, &relay))
        {
            return {SendCoinsStatus::TransactionCommitFailed};
        }
    }

    if (message_discarded) {
        LogPrintf("WARNING: %s: MESSAGE contracts are disabled from height %d; the message was "
                  "discarded and the payment sent without it.",
                  __func__, GetMessageContractDisableHeight());
    }

    // Add addresses / update labels that we've sent to the address book.
    //
    // Two phases, deliberately: decide what needs writing under cs_wallet, then
    // release it and do the writing. SetAddressBookName() emits
    // NotifyAddressBookChanged (wallet.cpp), and although it does so outside its
    // OWN lock scope, calling it from inside this one would put the emission back
    // under cs_wallet. In the multiprocess build that emission is a blocking proxy
    // call into the GUI, so a front end that is hung rather than dead would pin
    // the node's wallet lock for as long as it stays hung -- stalling the stake
    // miner and every other wallet operation behind it.
    //
    // THIS IS NOT A TOCTOU, and the split should not be "fixed" back. Splitting a
    // check from its write across a dropped lock is normally exactly that bug, so
    // here is why this instance is not:
    //
    //   The VALUE WRITTEN DOES NOT DEPEND ON THE VALUE READ. We write rcp.label,
    //   which comes from the caller's send request and is fixed before the lock is
    //   ever taken. The read decides only WHETHER to write, never WHAT. A classic
    //   TOCTOU needs the write to be derived from -- or conditional on the
    //   continued truth of -- the observation (read balance, then spend it; stat a
    //   path, then open it). Neither holds here.
    //
    // Both interleavings against a concurrent writer produce exactly what the old
    // per-recipient locking produced:
    //
    //   * We decided to write, someone else writes in between -> we still write
    //     the user's label. Under the old lock that writer simply landed before or
    //     after us; last write wins either way, and it is the same last write.
    //   * We decided to skip (label already matched), someone else changes it ->
    //     we skip. The old version skipped too, and that writer's value landed
    //     regardless. Identical end state.
    //
    // There is no ordering the dropped lock admits that the held lock excluded.
    // The only thing genuinely given up is that the address-book map may be
    // observed mid-update by an unrelated reader between the two phases -- which
    // was already true, because SetAddressBookName takes and releases cs_wallet
    // per call, so the old loop was never one atomic transaction across recipients
    // either.
    //
    // The payment is committed by now, so a failure to record the labels must
    // not reach sendCoins' exception boundary, which would report a payment
    // that was made as one that was not. It is logged, and the send reports OK.
    try {
        std::vector<std::pair<CTxDestination, std::string>> address_book_updates;

        {
            LOCK(m_wallet->cs_wallet);

            for (const WalletSendRecipient& rcp : recipients) {
                CTxDestination dest = DecodeDestination(rcp.address);

                auto mi = m_wallet->mapAddressBook.find(dest);

                // Check if we have a new address or an updated label
                if (mi == m_wallet->mapAddressBook.end() || mi->second.name != rcp.label)
                {
                    address_book_updates.emplace_back(std::move(dest), rcp.label);
                }
            }
        }

        for (const auto& [dest, label] : address_book_updates) {
            m_wallet->SetAddressBookName(dest, label);
        }
    } catch (const std::exception& e) {
        LogPrintf("ERROR: %s: the payment %s was sent, but its address book labels were not "
                  "recorded: %s", __func__, wtx.GetHash().GetHex(), e.what());
    }

    SendCoinsResult result;
    result.status = SendCoinsStatus::OK;
    result.txid_hex = wtx.GetHash().GetHex();

    return result;
}

} // namespace

std::unique_ptr<Wallet> MakeWallet(CWallet* wallet)
{
    return std::make_unique<WalletImpl>(wallet);
}

} // namespace interfaces
