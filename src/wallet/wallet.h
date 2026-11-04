// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <stdlib.h>

#include "amount.h"
#include "chain.h"  // cs_main, mapBlockIndex
#include "node/shutdown.h"
#include "consensus/tx_verify.h"
#include "gridcoin/staking/status.h"
#include "validationinterface.h"
#include "key.h"
#include "keystore.h"
#include "primitives/transaction.h"
#include "script.h"
#include "streams.h"
#include "node/ui_interface.h"
#include <util/string.h>
#include "wallet/capability.h"
#include "wallet/generated_type.h"
#include "wallet/transaction.h"
#include "wallet/walletdb.h"
#include <wallet/walletutil.h>
#include "wallet/ismine.h"

extern bool fConfChange;

// Wallet settings, defined in wallet.cpp (moved from main.{h,cpp}, issue
// #3125 C9). nTransactionFee is initialized to MIN_TX_FEE * 10 for block
// version 11; -paytxfee overrides it in AppInit2.
extern int64_t nTransactionFee;
extern int64_t nReserveBalance;
extern int64_t nMinimumInputValue;
extern unsigned int nDerivationMethodIndex;

class CAccountingEntry;
class CWalletTx;
class DeferredRelay;
class CReserveKey;
class COutput;
class CCoinControl;

//! An output the stake miner may use as a kernel, with the time of the block that confirmed it.
struct StakeCandidate
{
    const CWalletTx* tx{nullptr}; //!< The transaction that created the output.
    unsigned int n{0};            //!< The output's index in tx.
    unsigned int block_time{0};   //!< Time of the block that confirmed tx, which the kernel hash commits to.
};

GRC::MinedType GetGeneratedType(const CWallet *wallet, const uint256& tx, unsigned int vout) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//!
//! \brief The amounts \p vecSend pays once \p deduction is taken out of the
//! outputs listed in \p subtract_fee_outputs ("subtract fee from amount").
//!
//! Split evenly, the first listed output (the lowest index) also paying the
//! remainder. std::nullopt when the set is empty, lists an output vecSend does
//! not have, or would leave a listed output with nothing. CreateTransaction
//! builds a subtract-fee transaction's outputs with it and the GUI send path
//! tells that failure from the others with it, so the two cannot disagree.
//!
std::optional<std::vector<int64_t>> SubtractFromOutputs(const std::vector<std::pair<CScript, int64_t>>& vecSend,
                                                        const std::set<size_t>& subtract_fee_outputs,
                                                        int64_t deduction);

//! The burn CreateTransaction adds for a transaction carrying a user message, or 0.
int64_t MessageBurnAmount(const CTransaction& tx);

//! Return the height of the block confirming this state, or -1 if its block
//! is not in mapBlockIndex. The wallet.dat format does not persist confirming-
//! block height (CMerkleTx only stores hashBlock + nIndex), so consumers that
//! need a height look it up from the block index on demand via this helper.
//! Cost is a single unordered_map lookup, roughly 50 ns. Caller must hold
//! cs_main because it touches mapBlockIndex.
inline int GetConfirmedHeight(const TxStateConfirmed& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    auto it = mapBlockIndex.find(state.m_confirmed_block_hash);
    return (it != mapBlockIndex.end()) ? it->second->nHeight : -1;
}

static const unsigned int DEFAULT_KEYPOOL_SIZE = 100;
static const unsigned int DEFAULT_KEYPOOL_SIZE_PRE_HD = 1000;

/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;

    CKeyPool()
    {
        nTime = GetAdjustedTime();
    }

    CKeyPool(const CPubKey& vchPubKeyIn)
    {
        nTime = GetAdjustedTime();
        vchPubKey = vchPubKeyIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            int nVersion = s.GetVersion();
            READWRITE(nVersion);
        }

        READWRITE(nTime);
        READWRITE(vchPubKey);
    }
};

/** Address book data for a destination: the user-facing label plus a purpose tag.
 *
 * Mirrors Bitcoin 0.17's CAddressBookData. The two fields persist as two separate
 * walletdb records ("name" and "purpose") keyed by address and are assembled in memory at
 * load, so this struct is intentionally not serialized as a unit (no SERIALIZE_METHODS).
 *
 * No implicit conversion to std::string is provided: every consumer must read .name
 * explicitly, so the account/label disentanglement surfaces at compile time rather than
 * silently converting.
 */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData() : purpose("unknown") {}
};

/** A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore, public CValidationInterface
{
private:
    bool SelectCoins(int64_t nTargetValue, unsigned int nSpendTime,
                     std::set<std::pair<const CWalletTx*, unsigned int>>& setCoinsRet, int64_t& nValueRet,
                     const CCoinControl* coinControl = nullptr, bool contract = false) const;

    CWalletDB *pwalletdbEncryption GUARDED_BY(cs_wallet);

    // the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion GUARDED_BY(cs_wallet);

    /* the HD chain data model (external chain counters) */
    CHDChain hdChain;

    //! Random per-wallet identity tag (16 bytes), minted once and persisted in
    //! wallet.dat as the "walletuuid" record. Used by the multiprocess handshake
    //! to fingerprint *which wallet* a node is serving (independent of chain state
    //! or datadir path). Not secret, not key-derived. Empty until loaded/minted.
    std::vector<unsigned char> m_wallet_uuid;

    /* the seed phrase record for a phrase-backed HD hierarchy */
    CSeedPhraseData m_seed_phrase_data GUARDED_BY(cs_wallet);

    //! true when m_seed_phrase_data.vchBlob holds the wallet-crypter
    //! ciphertext (i.e. it came from or goes to a "cseedphrase" record)
    //! rather than the plaintext enciphered blob
    bool m_seed_phrase_blob_crypted GUARDED_BY(cs_wallet) = false;

public:
    /// Main wallet lock.
    /// This lock protects all the fields added by CWallet
    ///   except for:
    ///      fFileBacked (immutable after instantiation)
    ///      strWalletFile (immutable after instantiation)
    mutable CCriticalSection cs_wallet;

    bool fFileBacked;
    std::string strWalletFile;

    std::set<int64_t> setKeyPool GUARDED_BY(cs_wallet);
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata GUARDED_BY(cs_wallet);


    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys GUARDED_BY(cs_wallet);
    unsigned int nMasterKeyMaxID GUARDED_BY(cs_wallet);

    CWallet()
    {
        LOCK(cs_wallet);

        SetNull();
    }

    CWallet(std::string strWalletFileIn)
    {
        LOCK(cs_wallet);

        SetNull();

        strWalletFile = strWalletFileIn;
        fFileBacked = true;
    }

    //!
    //! \brief Get the output address controlled by the master private key used
    //! to verify administrative contracts.
    //!
    //! \param height The block height which the master key is valid for.
    //!
    //! \return Address as calculated from the master public key.
    //!
    static const CTxDestination MasterAddress(int height);

    void SetNull() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        nWalletVersion = wallet::FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = nullptr;
        nOrderPosNext = 0;
        nTimeFirstKey = 0;
    }

    std::map<uint256, CWalletTx> mapWallet GUARDED_BY(cs_wallet);
    int64_t nOrderPosNext GUARDED_BY(cs_wallet);
    std::map<uint256, int> mapRequestCount GUARDED_BY(cs_wallet);

    std::map<CTxDestination, CAddressBookData> mapAddressBook GUARDED_BY(cs_wallet);

    //! Maps spent outpoints to the transaction that spends them.
    //! Multimap because reorgs can temporarily create duplicates.
    std::multimap<COutPoint, uint256> mapTxSpends GUARDED_BY(cs_wallet);

    //! Hash of the last block the wallet processed. Updated by
    //! blockConnected/blockDisconnected for incremental sync & reorg detection.
    uint256 m_last_block_processed GUARDED_BY(cs_wallet);
    int m_last_block_processed_height GUARDED_BY(cs_wallet) = 0;

    //! Which block's connect displaced each transaction the wallet is holding
    //! conflicted, keyed by txid. Read by TakeConflictedBy when that block is
    //! disconnected.
    //!
    //! Deliberately transient. The on-disk wallet format carries one hashBlock
    //! slot per transaction, holding the null hash for an in-mempool entry
    //! (and CONFLICTED_HASH_SENTINEL for the inactive state),
    //! and transaction.h reserves the sentinel range so a future state stays
    //! downgrade-safe -- writing a real block hash into that slot is exactly
    //! the failure the range exists to prevent, since an older client would
    //! read the transaction as confirmed in that block. A restart therefore
    //! starts empty, and a conflict recorded before it is not re-evaluated.
    //! An entry leaves only when its block is disconnected, so the map holds
    //! one record per displaced own transaction for the rest of the session.
    //! That is the intended scope: a reorg is a within-session event, and the
    //! conflicts worth re-evaluating are the ones this session recorded.
    std::map<uint256, uint256> m_conflicting_block GUARDED_BY(cs_wallet);

    CPubKey vchDefaultKey GUARDED_BY(cs_wallet);
    int64_t nTimeFirstKey GUARDED_BY(cs_wallet);

    // check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum wallet::WalletFeature wf) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        AssertLockHeld(cs_wallet);
        return wallet::IsFeatureSupported(nWalletVersion, wf);
    }

    void AvailableCoinsForStaking(std::vector<StakeCandidate>& vCoins, unsigned int nSpendTime, int64_t& nBalanceOut,
                                  bool fMiner = false) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    bool SelectCoinsForStaking(unsigned int nSpendTime, std::vector<StakeCandidate>& vCoinsRet,
                               GRC::MinerStatus::ErrorFlags& not_staking_error, int64_t& balance_out, bool fMiner = false) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed = true, const CCoinControl* coinControl = nullptr,
                        bool fIncludeStakingCoins = false) const;
    bool SelectCoinsMinConf(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins,
                            std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;
    bool SelectSmallestCoins(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins,
                             std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;
    //! \brief Coin selection for contract transactions: prefer the smallest UTXOs
    //! (dust consolidation) but fall back to normal target-based selection when
    //! the smallest-first set would exceed a standard-tx-safe input budget, so a
    //! large burn on a fragmented wallet cannot produce a >MAX_STANDARD_TX_SIZE
    //! transaction (issue #3120). Takes \p vCoins so it is unit-testable in
    //! isolation, mirroring SelectCoinsMinConf / SelectSmallestCoins.
    bool SelectContractCoins(int64_t nTargetValue, unsigned int nSpendTime, std::vector<COutput> vCoins,
                             std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;

    // keystore implementation
    // Generate a new key
    CPubKey GenerateNewKey();
    // Adds a key to the store, and saves it to disk.
    bool AddKey(const CKey& key) override;
    // Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key) { return CCryptoKeyStore::AddKey(key); }
    // Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &metadata);

    bool LoadMinVersion(int nVersion) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        AssertLockHeld(cs_wallet);
        nWalletVersion = nVersion;
        return true;
    }

    // Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret) override;
    // Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript) override;
    bool LoadCScript(const CScript& redeemScript);

    //! \brief Is this wallet unlocked, but for staking only?
    //!
    //! Replaces the former fWalletUnlockStakingOnly global, which lived beside
    //! the lock state rather than in it. The scope answers both questions at
    //! once, so this can never read "unrestricted" on a wallet that was just
    //! unlocked for staking.
    bool IsUnlockedForStakingOnly() const
    {
        return GetUnlockScope() == UnlockScope::StakingOnly;
    }

    //! \brief May a key be created on the calling thread?
    //!
    //! A new key is derived from the HD seed or made fresh and written to the
    //! store, and an unlock for staking only withholds both. True when the
    //! wallet is unencrypted or fully unlocked, or unlocked for staking only
    //! with an ElevationGrant for it live on this thread. False when it is
    //! locked, grant or not: there is no master key to make a key with.
    //!
    //! That last case is deliberate, and it is where this differs from
    //! CanUseKeys(). A grant can outlive the unlock it was taken under, when a
    //! timed relock lands while a CapabilityScope is open. CanUseKeys() then
    //! still answers true, which is harmless because reading a key fails
    //! cleanly with no master key installed. Making one is not, because
    //! GenerateNewKey() would throw.
    //!
    //! The key pool is topped up, and a key made when the pool is empty, only
    //! while this holds. Drawing a key the pool already holds creates nothing,
    //! and is allowed whatever the scope.
    bool CanCreateKeys() const;

    //! \brief May the calling thread use the wallet's private keys now?
    //!
    //! True when the wallet-wide unlock is Full (an unencrypted wallet reads
    //! Full), or when a CapabilityScope for this wallet is active on the
    //! calling thread. A capability changes nothing wallet-wide, so
    //! GetUnlockScope() and IsUnlockedForStakingOnly() still report the
    //! standing unlock, and every other thread is still refused.
    //!
    //! This is what a check that means "may this operation sign or spend now"
    //! asks. It does not tell a locked wallet apart: a scope opened before a
    //! timed relock keeps answering true after it, while no key is installed.
    //! A caller that reports locked and staking-only differently tests
    //! IsLocked() first.
    bool CanUseKeys() const;

    //! Why a passphrase attempt failed. Defined with Unlock() below.
    enum class UnlockFailure;

    //! \brief Issue a capability, which authorises one operation of \p kind and
    //! is proved by the wallet passphrase.
    //!
    //! The capability lasts CapabilityTtl(kind) unless EndElevation() ends it
    //! first. It authorises nothing by itself; a CapabilityScope opened with
    //! its id on the operation's thread does. By the wallet's state:
    //!  - unencrypted: an id is issued without a check, so callers need not
    //!    tell the cases apart. There is nothing to grant.
    //!  - Full: the passphrase is checked against the installed master key and
    //!    the unlock is left as it is. It is checked although Full permits
    //!    everything, because the wallet can be narrowed to staking only while
    //!    the capability lasts.
    //!  - StakingOnly: the passphrase is checked the same way. The wallet-wide
    //!    scope is NOT changed.
    //!  - Locked: the wallet is unlocked for staking only, timed to relock
    //!    after the capability's lifetime, and EndElevation() locks it again.
    //!    Capabilities issued while that unlock lasts rely on it too. It is
    //!    locked when the last of them ends, or when the relock armed for the
    //!    capability that made it fires, whichever comes first: a later one's
    //!    longer lifetime does not move that relock. A capability that outlives
    //!    the unlock then fails closed, since no key can be read once the
    //!    wallet is locked.
    //!
    //! An encrypted wallet's attempt first waits for its turn, as Unlock()
    //! does. nullopt for a wrong passphrase. That refusal returns only after
    //! the FailedPassphraseDelay() for the failures counted so far, slept
    //! without holding cs_wallet, so the caller must not hold it either. Also
    //! nullopt, with no delay, when a locked wallet's unlock cannot arm its
    //! relock, and when the attempt is given up while it waits for its turn
    //! (AwaitPassphraseTurn()).
    //!
    //! \p failure_out, when given, says why none was issued: Passphrase,
    //! RelockUnavailable, or Abandoned for an attempt given up while it waited,
    //! which for a caller with no cancel token means a shutdown. None when one
    //! was issued.
    std::optional<CapabilityId> BeginElevation(const SecureString& passphrase, CapabilityKind kind,
                                               UnlockFailure* failure_out = nullptr)
        LOCKS_EXCLUDED(cs_wallet, m_capability_mutex);

    //! \brief End a capability.
    //!
    //! Every CapabilityScope opened with \p id afterwards is inactive; one
    //! already open finishes its operation. A no-op for an id that is unknown
    //! or already ended. Takes neither cs_main nor cs_wallet: the record is
    //! removed under the capability mutex, which is released before a wallet
    //! the capability unlocked is locked again, and that takes cs_KeyStore
    //! alone.
    void EndElevation(CapabilityId id) LOCKS_EXCLUDED(m_capability_mutex);

    //! \brief Issue a capability into the IPC slot.
    //!
    //! The IPC server serves one client at a time, so the process keeps one IPC
    //! capability. Whatever the slot held is ended, whether or not this one is
    //! issued, and false leaves the slot empty. Otherwise as BeginElevation().
    //!
    //! \p abandoned, when set, is asked in the same capability-mutex section
    //! that installs the capability. When it answers true (the client the
    //! capability is for has gone away), the slot is left exactly as it is, the
    //! new capability is ended once the mutex is released, and false is
    //! returned. It runs under the capability mutex, so it must not lock
    //! anything; a lock-free read is what it is for.
    bool BeginIpcElevation(const SecureString& passphrase, CapabilityKind kind,
                           const std::function<bool()>& abandoned = {})
        LOCKS_EXCLUDED(cs_wallet, m_capability_mutex);

    //! \brief End the IPC slot's capability, if it holds one, as EndElevation()
    //! does. Also called when the IPC client disconnects.
    void EndIpcElevation() LOCKS_EXCLUDED(m_capability_mutex);

    //! The capability in the IPC slot, or nullopt when the slot is empty.
    std::optional<CapabilityId> IpcCapability() const LOCKS_EXCLUDED(m_capability_mutex);

    //! How many passphrase attempts have failed in a row, across Unlock(),
    //! BeginElevation() and ChangeWalletPassphrase(). An accepted passphrase
    //! resets it to 0.
    unsigned int FailedUnlockCount() const { return m_failed_unlocks.load(); }

    //! \brief Unlock with an explicit scope and an optional duration.
    //!
    //! \p relock_after, when set, is how long this unlock lasts. The deadline
    //! is stored with the key and the scope, and a one-shot relock is armed on
    //! the scheduler against this unlock's epoch, so setting a duration and
    //! arming its timer are ONE operation that cannot be half-done.
    //!
    //! Fails if a duration is asked for and no scheduler is RUNNING, rather
    //! than unlocking without the timer: a caller that asked for sixty seconds
    //! and silently got forever is the failure this design exists to remove.
    //!
    //! It is reachable in production, which an earlier version of this note
    //! denied. The startup half is now closed -- AppInit2 constructs the
    //! scheduler BEFORE it starts the RPC server. A walletpassphrase during
    //! SHUTDOWN can still land where nothing would run the relock, and that one
    //! does not matter: the unlock does not survive the process (closed #3388).
    //!
    //! The decision is taken when the relock is ARMED, not before: the key
    //! derivation in between takes a calibrated few hundred milliseconds, and
    //! the scheduler can stop inside it. If the arming is refused, or the
    //! deadline has already passed by then, the wallet is LOCKED and this
    //! returns false -- never left unlocked without a timer.
    //! Why an unlock failed, for a caller that must not report every failure as
    //! a bad passphrase.
    enum class UnlockFailure {
        None,

        //! The passphrase did not decrypt the master key -- or the wallet was
        //! not in a state that can be unlocked at all.
        Passphrase,

        //! The passphrase was RIGHT, but the relock could not be armed, or the
        //! deadline had already passed by the time the key was installed. The
        //! wallet is locked; granting the unlock would have meant granting it
        //! with nothing to end it.
        RelockUnavailable,

        //! Another unlock got there first. Two callers can pass the "is it
        //! locked?" test before either takes cs_wallet, and the second would
        //! otherwise replace the first unlock's scope, deadline and epoch --
        //! retiring the relock armed for it and silently giving the wallet a
        //! different lifetime than the first caller was told.
        AlreadyUnlocked,

        //! The attempt was given up while it waited for its turn, because a
        //! shutdown was requested or its caller cancelled it
        //! (PassphraseAttemptCancel). The passphrase was not checked.
        Abandoned,
    };

    //! The attempt first waits for its turn (AwaitPassphraseTurn()), and one
    //! given up while it waits fails with UnlockFailure::Abandoned. A wrong
    //! passphrase (UnlockFailure::Passphrase) is counted, and the refusal
    //! returns only after the FailedPassphraseDelay() for the failures counted
    //! so far. The key is derived, and every wait slept, without cs_wallet
    //! held, so the caller must not hold it either. A success resets the count.
    bool Unlock(const SecureString& strWalletPassphrase, UnlockScope scope,
                std::optional<std::chrono::seconds> relock_after,
                UnlockFailure* failure_out = nullptr) LOCKS_EXCLUDED(cs_wallet);

    //! \brief Unlock with an explicit scope.
    //!
    //! Defaults to Full, which is what an unlock that does not say otherwise has
    //! always meant (walletpassphrase without its third argument). The scope is
    //! installed with the master key, so the wallet is never briefly unlocked
    //! carrying the previous unlock's restriction. A wrong passphrase is
    //! counted and delayed as by the overload above.
    bool Unlock(const SecureString& strWalletPassphrase, UnlockScope scope = UnlockScope::Full)
        LOCKS_EXCLUDED(cs_wallet);

    //! \brief Narrow a full unlock to staking only, keeping its deadline.
    //!
    //! Takes cs_wallet before narrowing, which is what serialises it against a
    //! spend. CreateTransaction reads the scope inside LOCK2(cs_main,
    //! cs_wallet) and holds cs_wallet for the whole build, so narrowing under
    //! cs_KeyStore alone could land just after that read and the build would
    //! sign anyway. With this, either the spend finishes first or the builder
    //! sees the restriction.
    //!
    //! False when there is nothing to narrow, which covers BOTH a locked wallet
    //! and an unencrypted one; do not read false as "it was locked".
    bool RestrictToStakingOnly();

    //! The wallet's unlock is left exactly as it was, whether or not the
    //! change succeeds: a wallet unlocked for staking only stays so, with the
    //! same deadline and relock, a full unlock stays full, and a locked wallet
    //! is never opened. The old passphrase is proved against the keys without
    //! its master key being installed, and every key is derived without
    //! cs_wallet held.
    //!
    //! A wrong old passphrase is counted and delayed as by Unlock(), and the
    //! caller must not hold cs_wallet. False, with nothing changed, when the
    //! attempt is given up while it waits for its turn. A successful change
    //! ends every capability: they were proved with the old passphrase.
    //!
    //! \p failure_out, when given, is Abandoned for an attempt given up while
    //! it waited, Passphrase for a wrong old passphrase, and None otherwise.
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase,
                                UnlockFailure* failure_out = nullptr) LOCKS_EXCLUDED(cs_wallet);

    //! Ends every capability issued before it, since those were issued with no
    //! passphrase to check.
    bool EncryptWallet(const SecureString& strWalletPassphrase);

private:
    //! One issued capability.
    struct CapabilityRecord {
        CapabilityKind kind{CapabilityKind::Send};

        //! When it expires, on the mockable clock CapabilityScope reads.
        std::chrono::seconds deadline{0};

        //! The epoch of the unlock that ending this capability may lock again.
        //! That is the unlock it made on a locked wallet, or the one another
        //! capability made, when that unlock was in progress as it was issued.
        //! nullopt when the wallet was unlocked by other means, which ending it
        //! leaves alone.
        std::optional<uint64_t> relock_epoch;
    };

    //! Guards the issued capabilities and the IPC slot. A leaf: no other lock
    //! is taken, and nothing is called out to, while it is held.
    mutable Mutex m_capability_mutex;

    std::map<CapabilityId, CapabilityRecord> m_capabilities GUARDED_BY(m_capability_mutex);
    std::optional<CapabilityId> m_ipc_capability GUARDED_BY(m_capability_mutex);

    //! Guards the passphrase attempts' turns. A leaf: no other lock is taken
    //! while it is held, and nothing is called but the lock-free reads of the
    //! shutdown flag and of the attempt's cancel token.
    mutable Mutex m_passphrase_turn_mutex;

    //! Raised whenever a turn ends, so that the attempt now served looks again.
    std::condition_variable m_passphrase_turn_ended;

    //! Consecutive failed passphrase attempts, across Unlock(),
    //! BeginElevation() and ChangeWalletPassphrase(). Process-wide in effect:
    //! there is one wallet. Written under m_passphrase_turn_mutex; atomic so
    //! that FailedUnlockCount() can read it without the mutex.
    std::atomic<unsigned int> m_failed_unlocks{0};

    //! Passphrase attempts are served in the order they ask: each takes the
    //! next ticket, and waits until its ticket is the one served.
    uint64_t m_next_passphrase_ticket GUARDED_BY(m_passphrase_turn_mutex){0};
    uint64_t m_served_passphrase_ticket GUARDED_BY(m_passphrase_turn_mutex){0};

    //! Tickets given up before they were served, passed over when they are.
    std::set<uint64_t> m_abandoned_passphrase_tickets GUARDED_BY(m_passphrase_turn_mutex);

    //! The served attempt checks its passphrase no sooner than this: the end of
    //! the delay the last refusal earned.
    std::chrono::steady_clock::time_point m_next_passphrase_check GUARDED_BY(m_passphrase_turn_mutex){};

    //! Whether \p id is issued, for \p kind, and before its deadline. What
    //! CapabilityScope checks before it grants.
    bool IsCapabilityLive(CapabilityId id, CapabilityKind kind) const LOCKS_EXCLUDED(m_capability_mutex);

    friend class CapabilityScope;
    friend struct PassphraseTurnTestPeer;

    //! Unlock() without the turn, and without counting or delaying a wrong
    //! passphrase, reporting the epoch of the unlock it installed through
    //! \p epoch_out. It takes cs_wallet only to copy the master key records
    //! and to install the key, never across the derivation, so a caller that
    //! does not hold cs_wallet keeps it free while the key is derived.
    bool UnlockWithoutDelay(const SecureString& strWalletPassphrase, UnlockScope scope,
                            std::optional<std::chrono::seconds> relock_after,
                            UnlockFailure* failure_out, uint64_t* epoch_out);

    //! Decrypt the master key with \p passphrase into \p master_key_out. False
    //! when the passphrase decrypts no master key record. The key derivation
    //! runs without cs_wallet held.
    bool DecryptMasterKey(const SecureString& passphrase, CKeyingMaterial& master_key_out) const
        LOCKS_EXCLUDED(cs_wallet);

    //! \brief One passphrase attempt's turn, from AwaitPassphraseTurn().
    //!
    //! Held() is false when the attempt was given up while it waited, and it
    //! must then check no passphrase. Otherwise the attempt reports its check
    //! with Accepted() or Refused(). A turn that ends without either, because
    //! the attempt never checked the passphrase or did not get as far as
    //! saying how the check went, is handed on unused, so the attempts behind
    //! it are never held up by it.
    class PassphraseTurn
    {
    public:
        PassphraseTurn(CWallet& wallet, std::optional<uint64_t> ticket) : m_wallet(wallet), m_ticket(ticket) {}
        ~PassphraseTurn();

        PassphraseTurn(const PassphraseTurn&) = delete;
        PassphraseTurn& operator=(const PassphraseTurn&) = delete;
        PassphraseTurn(PassphraseTurn&&) = delete;
        PassphraseTurn& operator=(PassphraseTurn&&) = delete;

        bool Held() const { return m_ticket.has_value(); }

        //! The passphrase was accepted: the count is reset, and the next
        //! attempt is checked at once.
        void Accepted() LOCKS_EXCLUDED(m_wallet.m_passphrase_turn_mutex);

        //! The passphrase was refused. It is counted, every later check waits
        //! out its FailedPassphraseDelay(), and this returns once that delay
        //! is over, or sooner when a shutdown is requested or the attempt is
        //! cancelled (PassphraseAttemptCancel); the attempts behind it wait the
        //! whole delay all the same. Never call it under cs_wallet.
        void Refused() LOCKS_EXCLUDED(m_wallet.cs_wallet, m_wallet.m_passphrase_turn_mutex);

    private:
        CWallet& m_wallet;

        //! The ticket whose turn this is, until the turn ends.
        std::optional<uint64_t> m_ticket;

        //! End the turn: serve the next ticket, and wake the attempts waiting.
        void End() LOCKS_EXCLUDED(m_wallet.m_passphrase_turn_mutex);
    };

    //! \brief Wait for this passphrase attempt's turn.
    //!
    //! Attempts are served one at a time, in the order they ask, so callers on
    //! several threads have their passphrases checked as one caller's would
    //! be. The attempt served waits out the delay the last refusal earned
    //! before its passphrase is checked, and no later attempt overtakes it, so
    //! its wait is bounded by the attempts ahead of it, each at most a check
    //! and the longest FailedPassphraseDelay().
    //!
    //! The wait ends with no turn held when a shutdown is requested, or when
    //! the calling thread's attempt is cancelled (PassphraseAttemptCancel). The
    //! attempt is then given up: its passphrase is not checked, nothing is
    //! counted, and its place passes to the attempt behind it. Never call it
    //! under cs_wallet.
    PassphraseTurn AwaitPassphraseTurn() LOCKS_EXCLUDED(cs_wallet, m_passphrase_turn_mutex);

    //! Serve the ticket after \p ticket, or, when \p ticket is not yet the one
    //! served, mark it to be passed over when its turn comes.
    void EndPassphraseTicket(uint64_t ticket) EXCLUSIVE_LOCKS_REQUIRED(m_passphrase_turn_mutex);

    //! ChangeWalletPassphrase() without the turn or the delay. \p
    //! passphrase_accepted is set once the old passphrase has been checked:
    //! true when it opened a master key record and that key opens the keys,
    //! false when it did not. It takes
    //! cs_wallet only to copy the master key records and to write the new one.
    bool ChangeWalletPassphraseWithoutDelay(const SecureString& strOldWalletPassphrase,
                                            const SecureString& strNewWalletPassphrase,
                                            std::optional<bool>& passphrase_accepted) LOCKS_EXCLUDED(cs_wallet);

public:
    void GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_wallet);


    /** Increment the next transaction order id
        @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB* pwalletdb = nullptr);

    // Use hash-based lookup and value-based storage to avoid dangling pointers
    // when mapWallet reallocates or acentries goes out of scope.
    // Stores transaction hash + optional accounting entry copy (not a pointer!)
    typedef std::pair<uint256, std::optional<CAccountingEntry>> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;

    /** Get the wallet's activity log
        @return multimap of ordered transactions and accounting entries
        @warning Transaction hashes must be looked up in mapWallet before use
     */
    TxItems OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount = "");

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, CWalletDB *pwalletdb) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /** Record in mapTxSpends that \p tx spends each of its inputs. A row already present is not
     *  duplicated, and a coinbase spends nothing and records nothing. Every wallet transaction that
     *  is not abandoned must have its rows, so every path that adds a transaction with inputs to
     *  mapWallet calls this.
     *  AbandonTransaction removes an abandoned transaction's rows and the load does not restore
     *  them, so a reader that must reach abandoned transactions has to walk mapWallet. */
    void AddToSpends(const CTransaction& tx, const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    bool EraseFromWallet(uint256 hash);

    /** Central entry point for updating wallet transaction state.
     *  Called by validation callbacks; handles mapWallet, mapTxSpends,
     *  balance updates, UI notifications, and persistence.
     *
     *  cs_main is required because the producer-side NotifyTransactionChanged
     *  handler (walletmodel.cpp) reads mapBlockIndex / pindexBest under this
     *  lock for coinstake/coinbase visibility decisions.
     */
    bool SyncTransaction(const CTransactionRef& ptx,
                        const TxState& state,
                        bool update_tx = true,
                        bool rescanning_old_block = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Legacy overload — wraps CTransaction in a shared_ptr.
    bool SyncTransaction(const CTransaction& tx,
                        const TxState& state,
                        bool update_tx = true) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        return SyncTransaction(MakeTransactionRef(tx), state, update_tx, false);
    }

    // -- CValidationInterface callbacks ---------------------------------------
    // Invoked by the validation/mempool layer through GetMainSignals() with
    // cs_main held. Lock-order rule for all of these: callers must hold cs_main
    // and must NOT hold cs_wallet. Most acquire cs_wallet internally;
    // ChainStateFlushed is the exception -- it only writes the wallet DB and
    // takes no cs_wallet. Override the (UpperCamelCase) CValidationInterface
    // virtuals.
    void TransactionAddedToMempool(const CTransactionRef& tx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main) LOCKS_EXCLUDED(cs_wallet);
    void BlockConnected(const CBlock& block, int height) override EXCLUSIVE_LOCKS_REQUIRED(cs_main) LOCKS_EXCLUDED(cs_wallet);
    void TransactionRemovedFromMempool(const CTransactionRef& tx,
                                      MemPoolRemovalReason reason) override EXCLUSIVE_LOCKS_REQUIRED(cs_main) LOCKS_EXCLUDED(cs_wallet);
    //! A conflicting tx replaced this one in the mempool; erase the defunct copy.
    void TransactionReplacedInMempool(const CTransactionRef& tx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main) LOCKS_EXCLUDED(cs_wallet);
    void BlockDisconnected(const CBlock& block, int height) override EXCLUSIVE_LOCKS_REQUIRED(cs_main) LOCKS_EXCLUDED(cs_wallet);
    //! Persist the best-block locator so a restored wallet can detect its
    //! position. Writes wallet DB only; takes no cs_wallet.
    void ChainStateFlushed(const CBlockLocator& locator) override EXCLUSIVE_LOCKS_REQUIRED(cs_main) LOCKS_EXCLUDED(cs_wallet);

    //!
    //! \brief Note that a block's connect displaced these transactions from the
    //! mempool.
    //!
    //! A block that spends an input of a pooled transaction evicts it through
    //! CTxMemPool::removeConflicts. remove() emits no validation signal, so the
    //! wallet is never told: the entry keeps TxStateInMempool and only
    //! GetDepthInMainChain notices, turning depth 0 into -1 because the mempool
    //! no longer holds it. Nothing anywhere records which block did it, so when
    //! that block is disconnected the transaction stays at -1 for the rest of
    //! the session -- rendered Conflicted by every depth-based reader, and
    //! absent from the local mempool -- though the reason it was displaced is
    //! gone.
    //!
    //! Hashes that are not the wallet's are ignored.
    //!
    //! \param hashes The transactions removeConflicts took out.
    //! \param block_hash The block being connected.
    //!
    void RecordConflictedBy(const std::vector<uint256>& hashes, const uint256& block_hash)
        LOCKS_EXCLUDED(cs_wallet);

    //!
    //! \brief Take the transactions a block's connect had displaced, forgetting
    //! the records.
    //!
    //! The disconnect asks for its own hash back and re-offers what it gets to
    //! the mempool, after the resurrect loop so a transaction that legitimately
    //! still holds the outpoint gets it first. A refusal is the ordinary
    //! answer, and leaves the wallet exactly as it was; it succeeds when
    //! whatever displaced the transaction went with the block, which is always
    //! the case for a coinstake.
    //!
    //! Only a transaction the wallet still holds and the mempool does not is
    //! returned: one that has since confirmed, been abandoned, or found its way
    //! back into the pool was resolved by another path.
    //!
    //! \param block_hash The block being disconnected.
    //!
    //! \return The transactions to re-offer, in no particular order.
    //!
    std::vector<CTransaction> TakeConflictedBy(const uint256& block_hash) LOCKS_EXCLUDED(cs_wallet);

    // -- Conflict tracking & abandonment -------------------------------------
    /** Return txids that spend the same inputs as @p txid. */
    std::set<uint256> GetConflicts(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    /** True if the transaction was explicitly abandoned by the user. */
    bool IsAbandoned(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    /** Mark an unconfirmed transaction (and its descendants) as abandoned.
     *  Acquires cs_wallet internally (unlike GetConflicts/IsAbandoned, which
     *  require the caller to hold it); cs_wallet is recursive so a caller that
     *  already holds it may still call this.
     *  Requires cs_main because the NotifyTransactionChanged signal handler
     *  reads mapBlockIndex / pindexBest via decomposeTransaction. */
    //! Abandon an unconfirmed transaction that is not in the mempool, along with any
    //! wallet descendants, and release the inputs each of them consumed.
    //!
    //! \p inputs_released, when supplied, receives the number of this wallet's
    //! outputs made spendable again across the whole cascade.
    bool AbandonTransaction(const uint256& txid, unsigned int* inputs_released = nullptr)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /** @deprecated Use SyncTransaction or BlockConnected/TransactionAddedToMempool instead.
     *  Public compatibility wrapper with the legacy signature. */
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock,
                                  bool fUpdate = false, bool fFindBlock = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

private:
    /** Add tx to wallet if it involves our addresses. Called by SyncTransaction. */
    bool AddToWalletIfInvolvingMe(const CTransactionRef& ptx,
                                  const TxState& state,
                                  bool fUpdate) EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_wallet);

public:
    void WalletUpdateSpent(const CTransaction &tx, bool fBlock, CWalletDB* pwalletdb);
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    int ScanForMRCRequests(CBlockIndex* pindexStart, CBlockIndex* pindexEnd, bool fUpdate = false);
    void ReacceptWalletTransactions();


    //!
    //! \brief This method resends wallet transactions that have not been confirmed on the chain. The original implementation
    //! was based on old Bitcoin code and was really bad. This new revision adapts some of the ideas from the Bitcoin Core
    //! current master (~v22), but to straighten everything out requires a full port of the newer Bitcoin wallet code.
    //!
    //! \param fForce
    //!
    //! Re-announce own transactions that are unconfirmed and no longer in the
    //! mempool. An inactive one that is not abandoned is offered back to the
    //! mempool first and announced only if accepted (#3382); a coinbase or
    //! coinstake never is. Returns how many were relayed, including those; the
    //! rate-limit and sync guards that fForce bypasses return 0 without looking
    //! at the wallet.
    unsigned int ResendWalletTransactions(bool fForce = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    int64_t GetBalance() const;
    int64_t GetUnconfirmedBalance() const;
    int64_t GetImmatureBalance() const;
    int64_t GetStake() const;

    //! The four balances above, as those four functions return them.
    struct Balances
    {
        int64_t balance{0};
        int64_t stake{0};
        int64_t unconfirmed{0};
        int64_t immature{0};
    };

    //!
    //! \brief All four balances in one pass over the wallet, with one depth
    //! lookup per transaction (more only for a transaction of ours less than
    //! three blocks deep, whose dependencies IsTrusted() walks). Equal to
    //! calling GetBalance(), GetStake(), GetUnconfirmedBalance() and
    //! GetImmatureBalance() under one hold of the locks, at a fraction of the
    //! cost on a large wallet.
    //!
    Balances GetBalances() const;
    int64_t GetNewMint() const;
    bool FundTransaction(CTransaction& tx, int64_t& nFeeRet, int& nChangePosInOut,
                         std::string& strFailReason, const CCoinControl* coinControl);
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t>>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey,
                           int64_t& nFeeRet, int& nChangePosRet, const CCoinControl* coinControl = nullptr,
                           bool change_back_to_input_address = false, const StakingKeyAccess* renewal_access = nullptr,
                           const std::set<size_t>& subtract_fee_outputs = {});
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t>>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey,
                           int64_t& nFeeRet, const CCoinControl* coinControl = nullptr, bool change_back_to_input_address = false,
                           const StakingKeyAccess* renewal_access = nullptr, const std::set<size_t>& subtract_fee_outputs = {});
    //! nEnforcedMinFee, when nonzero, sets a fee floor above the normal size-based fee
    //! (e.g. splitunspent's per-piece fee). The fee can still rise above it if required.
    //!
    //! Note the asymmetry below is deliberate, not an oversight: only the
    //! overloads the automated renewal actually uses carry renewal_access. The
    //! setCoins overload without nChangePosRet is reached only from
    //! consolidateunspent, splitunspent and sweepuncoveredcoins, all
    //! user-initiated spends that must stay refused (each calls
    //! EnsureWalletIsUnlocked first, so they are already refused before
    //! reaching the builder), so giving it the parameter would only widen the
    //! ways to reach the renewal's build.
    //!
    //! \p renewal_access is the automated beacon renewal's, and only
    //! Researcher::RunRenewBeaconJob() can make one. That job runs hourly on the
    //! wallets a researcher unlocks for staking, and a refusal would let the
    //! beacon expire at six months. With it the builder:
    //!  - builds while the wallet is unlocked for staking only;
    //!  - pays the minimum fee for the transaction's size. When a pass's fee is
    //!    above the minimum for the coins it selected, it keeps those coins and
    //!    pays their minimum;
    //!  - sends any change back to the address of one of its inputs, so it
    //!    draws no key from the pool;
    //!  - checks the built transaction with GRC::IsRenewalShape() before it
    //!    signs anything, and fails if it is not that shape;
    //!  - signs through a StakingSigningProvider made with the access.
    //! Every other caller passes nullptr and is refused while the wallet is
    //! unlocked for staking only.
    //!
    //! It is NOT "contract transactions are harmless". The GUI contract surface
    //! has no unlock guard of its own and addkey gates only on IsLocked(), so
    //! widening this to all contracts would re-open the very paths this check
    //! exists to close.
    //!
    //! \p subtract_fee_outputs lists the indices into \p vecSend of the outputs
    //! that pay the fee and any message burn ("subtract fee from amount"). The
    //! two are split evenly across them and the first listed index also pays
    //! the remainder, so the inputs need cover only the amounts in vecSend and
    //! the sender is debited exactly those. Empty, the default, keeps the fee
    //! and burn on top of the outputs. The same asymmetry applies: only the
    //! overloads on the GUI send path carry it, since that is its one caller.
    //! A non-empty set with \p renewal_access is refused: the renewal's outputs
    //! and burn are fixed, and GRC::IsRenewalShape() checks them.
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t>>& vecSend, std::set<std::pair<const CWalletTx*,unsigned int>>& setCoins,
                           CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, int& nChangePosRet,
                           const CCoinControl* coinControl = nullptr, bool change_back_to_input_address = false,
                           int64_t nEnforcedMinFee = 0, const StakingKeyAccess* renewal_access = nullptr,
                           const std::set<size_t>& subtract_fee_outputs = {});
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t>>& vecSend, std::set<std::pair<const CWalletTx*,unsigned int>>& setCoins,
                           CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl = nullptr,
                           bool change_back_to_input_address = false, int64_t nEnforcedMinFee = 0);
    bool CreateTransaction(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet,
                           const CCoinControl* coinControl = nullptr, bool change_back_to_input_address = false);
    //! Commit a transaction and announce it.
    //!
    //! \param relay When non-null, the announcement is QUEUED here instead of
    //! being made before returning. Callers that hold cs_wallet across this call
    //! must pass one that outlives their own lock: this function releasing its
    //! own guard is not enough if the caller still holds the wallet lock, which
    //! is the case for WalletImpl::sendCoins (#3391).
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, DeferredRelay* relay = nullptr);

    //! \param relay Forwarded to CommitTransaction. When null, SendMoney queues
    //! the announcement itself and makes it once its own lock is released. A
    //! caller holding cs_wallet across this call should supply one that
    //! outlives its own guard, so the announcement is not made under the
    //! wallet lock (#3391). Announcing under it is safe (the lock rule on
    //! CConnman::ForEachNode, #3443); the queue only shortens the hold.
    std::string SendMoney(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew,
                          DeferredRelay* relay = nullptr);
    std::string SendMoneyToDestination(const CTxDestination &address, int64_t nValue, CWalletTx& wtxNew,
                                       DeferredRelay* relay = nullptr);

    bool NewKeyPool();
    bool TopUpKeyPool(unsigned int nSize = 0);
    int64_t AddReserveKey(const CKeyPool& keypool);
    //! \p top_up false takes from the pool as it stands, creating no key.
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool top_up = true);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex);
    bool GetKeyFromPool(CPubKey &key, bool fAllowReuse=true);
    int64_t GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const;


    std::set< std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, int64_t> GetAddressBalances();

    isminetype IsMine(const CTxIn& txin) const;
    int64_t GetDebit(const CTxIn& txin, const isminefilter& filter=(ISMINE_SPENDABLE|ISMINE_WATCH_ONLY)) const;
    isminetype IsMine(const CTxOut& txout) const
    {
        return ::IsMine(*this, txout.scriptPubKey);
    }
    int64_t GetCredit(const CTxOut& txout, const isminefilter& filter=(ISMINE_WATCH_ONLY|ISMINE_SPENDABLE)) const
    {
        if (!MoneyRange(txout.nValue))
            throw std::runtime_error("CWallet::GetCredit() : value out of range");
        return ((IsMine(txout) != ISMINE_NO) ? txout.nValue : 0);
    }
    bool IsChange(const CTxOut& txout) const;
    int64_t GetChange(const CTxOut& txout) const
    {
        if (!MoneyRange(txout.nValue))
            throw std::runtime_error("CWallet::GetChange() : value out of range");
        return (IsChange(txout) ? txout.nValue : 0);
    }
    isminetype IsMine(const CTransaction& tx) const
    {
        for (auto const& txout : tx.vout) {
            isminetype fIsMine = IsMine(txout);
            if ((fIsMine != ISMINE_NO) && txout.nValue >= nMinimumInputValue)
                return fIsMine;
        }
        return ISMINE_NO;
    }
    bool IsFromMe(const CTransaction& tx) const
    {
        return (GetDebit(tx) > 0);
    }
    int64_t GetDebit(const CTransaction& tx, const isminefilter& filter=(ISMINE_SPENDABLE|ISMINE_WATCH_ONLY)) const
    {
        int64_t nDebit = 0;
        for (auto const& txin : tx.vin)
        {
            nDebit += GetDebit(txin, filter);
            if (!MoneyRange(nDebit))
                throw std::runtime_error("CWallet::GetDebit() : value out of range");
        }
        return nDebit;
     }
    int64_t GetCredit(const CTransaction& tx) const
    {
        int64_t nCredit = 0;
        for (auto const& txout : tx.vout)
        {
            nCredit += GetCredit(txout);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWallet::GetCredit() : value out of range");
        }
        return nCredit;
    }
    int64_t GetChange(const CTransaction& tx) const
    {
        int64_t nChange = 0;
        for (auto const& txout : tx.vout)
        {
            nChange += GetChange(txout);
            if (!MoneyRange(nChange))
                throw std::runtime_error("CWallet::GetChange() : value out of range");
        }
        return nChange;
    }

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);

    bool SetAddressBookName(const CTxDestination& address, const std::string& strName);

    bool SetAddressBookPurpose(const CTxDestination& address, const std::string& strPurpose);

    bool DelAddressBookName(const CTxDestination& address);

    void Inventory(const uint256 &hash)
    {
        {
            LOCK(cs_wallet);
            std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
            if (mi != mapRequestCount.end())
                mi->second++;
        }
    }

    unsigned int GetKeyPoolSize() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        AssertLockHeld(cs_wallet); // setKeyPool
        return setKeyPool.size();
    }

    bool GetTransaction(const uint256 &hashTx, CWalletTx& wtx);

    bool SetDefaultKey(const CPubKey &vchPubKey);

    // signify that a particular wallet feature is now used.
    bool SetMinVersion(enum wallet::WalletFeature, CWalletDB* pwalletdbIn = nullptr);

    // get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }

    void FixSpentCoins(int& nMismatchSpent, int64_t& nBalanceInQuestion, bool fCheckOnly = false);

    //! Release spent flags recorded by transactions that are NOT in the active
    //! chain -- spends the wallet remembers but the chain has not (yet) replayed.
    //!
    //! Needed because vfSpent is persisted in wallet.dat while the block data is
    //! not. Reset or roll back the chain under a populated wallet and the wallet
    //! still believes in every spend it ever saw, including ones now far above the
    //! tip, so those coins are invisible and the balance under-reports for the
    //! whole resync. FixSpentCoins cannot repair this: it skips any transaction
    //! missing from the tx index (wallet.cpp), and immediately after a reset that
    //! is every transaction in the wallet.
    //!
    //! Cheap by construction, unlike FixSpentCoins: no CTxDB and no per-transaction
    //! index reads. It needs only a mapBlockIndex lookup per wallet transaction
    //! (~50ns, see TxStateConfirmed in wallet/transaction.h) plus mapWallet lookups,
    //! and writes only the entries it actually corrects. A wallet whose transactions
    //! are all in the active chain -- every ordinary startup -- does no writes at all.
    //!
    //! Replay re-establishes the flags naturally: when a spend's block is connected
    //! again, AddToWalletIfInvolvingMe marks its inputs spent as it always did. So
    //! the balance tracks the chain honestly at every height instead of reading
    //! low until the resync finishes.
    //!
    //! \param nReleased        Number of outputs whose spent flag was cleared.
    //! \param nAmountReleased  Their total value.
    //! \param fCheckOnly       Report only; change nothing.
    void ReleaseSpendsNotInActiveChain(int& nReleased, int64_t& nAmountReleased, bool fCheckOnly = false);
    void DisableTransaction(const CTransaction &tx);

    //! Clear the spent bits on the parent outputs \p tx consumed, so the coins are
    //! selectable again. Returns how many of this wallet's outputs were released.
    //!
    //! Abandoning a transaction erases its mapTxSpends rows but does not touch the
    //! parents' vfSpent bits -- and it is vfSpent, not mapTxSpends, that gates
    //! AvailableCoins and GetAvailableCredit. Without this the coins stay locked and
    //! the balance stays depressed until FixSpentCoins runs at startup or via
    //! repairwallet, which is what made abandontransaction's promise that inputs
    //! "can be respent" untrue. Mirrors the reversal BlockDisconnected performs.
    //!
    //! \p pwalletdb may be null for a wallet that is not file-backed. Takes the
    //! caller's database session rather than opening its own, so a cascade writes
    //! through one handle.
    //!
    //! An output another wallet transaction still spends -- one the wallet still
    //! tags in-mempool (pooled or evicted), unrecognized, or confirmed on the
    //! active chain -- keeps its bit: vfSpent does not record which transaction
    //! set it, so it is not this one's alone to clear. So does an output the pool
    //! holds a spend of, or the tx index records spent, whether or not the wallet
    //! holds that spend.
    //!
    //! Fires CT_UPDATED once for each parent whose output it released.
    unsigned int ReleaseTransactionInputs(const CTransaction& tx, CWalletDB* pwalletdb)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs_wallet);

    //!
    //! \brief Get the time that the wallet last created a backup.
    //!
    //! \return Timestamp of the backup in seconds. Zero if the wallet never
    //! created a backup before.
    //!
    int64_t GetLastBackupTime() const;

    //!
    //! \brief Save the time that the wallet last created a backup.
    //!
    //! \param backup_time Timestamp of the backup in seconds.
    //!
    void StoreLastBackupTime(const int64_t backup_time);

    /** Address book entry changed.
     * @note Emitted with cs_wallet NOT held: the emitters snapshot name/purpose into locals
     *       inside the lock scope and fire the signal after releasing it, so the by-reference
     *       string arguments stay valid for the duration of the call. A subscriber must not
     *       assume cs_wallet is held in its handler.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination &address, const std::string &label, bool isMine, const std::string &purpose, ChangeType status)> NotifyAddressBookChanged;

    /** Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx, ChangeType status)> NotifyTransactionChanged;

    /** A bulk wallet-transaction mutation pass (rescan / reaccept) completed
     *  without emitting per-transaction NotifyTransactionChanged signals.
     *  Subscribers that maintain derived transaction or coin state should
     *  fully resynchronize from the wallet.
     *  @note Lock state at emission varies by path (a rescan nested inside
     *        ReacceptWalletTransactions fires under cs_main + cs_wallet; the
     *        top-level emissions fire with neither held). Subscribers must not
     *        assume either and should defer heavy resync work rather than
     *        perform it inline.
     */
    boost::signals2::signal<void ()> NotifyTransactionsBulkChanged;

    /* Set the HD chain model (chain child index counters) */
    bool SetHDChain(const CHDChain& chain, bool memonly);
    const CHDChain& GetHDChain() { return hdChain; }

    //! Load the persisted wallet UUID (called by CWalletDB during LoadWallet). No
    //! disk write.
    void LoadWalletUuid(const std::vector<unsigned char>& uuid) { m_wallet_uuid = uuid; }

    //! The per-wallet identity tag (16 bytes), or empty if none has been minted
    //! (e.g. a mockable-chain wallet, or a write failure at mint time). Immutable
    //! after load; safe to read without cs_wallet.
    std::vector<unsigned char> GetWalletUuid() const { return m_wallet_uuid; }

    /* Load the seed phrase record during wallet db load (memory only) */
    bool LoadSeedPhraseData(const CSeedPhraseData& data, bool crypted);

    /* Set and persist the seed phrase record. The blob in `data` must be the
     * plaintext enciphered blob; when the wallet is encrypted it is stored
     * under the wallet's master keying material instead (requires unlock). */
    bool SetSeedPhraseData(const CSeedPhraseData& data);

    /* Retrieve the plaintext enciphered blob for phrase re-display
     * (requires unlock when the wallet is encrypted) */
    bool GetSeedPhraseBlob(CKeyingMaterial& blob_out) const;

    /* Returns true if the HD hierarchy is backed by a seed phrase */
    bool HasSeedPhrase() const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        return !m_seed_phrase_data.IsNull();
    }

    /* Returns true if the stored blob is wallet-crypter-encrypted */
    bool SeedPhraseBlobCrypted() const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        return m_seed_phrase_blob_crypted;
    }

    /* Non-secret seed phrase record fields (master key id, birthday) */
    const CSeedPhraseData& GetSeedPhraseData() const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        return m_seed_phrase_data;
    }

    //! Coverage class of a wallet key against the seed phrase master: covered
    //! by the phrase (the phrase-derived master itself or HD-derived from it),
    //! derived from a prior (retired) HD master, or a legacy non-HD key. The
    //! latter two remain usable but cannot be recovered from the phrase.
    enum class SeedPhraseKeyClass { COVERED, PRIOR_HD, LEGACY };

    //! Precondition: the wallet must have a seed phrase record
    //! (HasSeedPhrase()); classifying against an unset master key id is
    //! meaningless. All callers gate on it.
    SeedPhraseKeyClass ClassifySeedPhraseKey(const CKeyID& key_id) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /* Returns true if HD is enabled */
    bool IsHDEnabled() const;

    /* Generates a new HD master key (will not be activated) */
    CPubKey GenerateNewHDMasterKey();

    /* Derives a new HD master key (will not be activated) */
    CPubKey DeriveNewMasterHDKey(const CKey& key);

    /* Set the current HD master key (will reset the chain child index counters) */
    bool SetHDMasterKey(const CPubKey& key);

    /** Upgrade the wallet */
    bool UpgradeWallet(int version, std::string& error);
};

//! Pure selection math for sweepuncoveredcoins, factored out for unit
//! testing. Given candidate output values sorted ascending, a smallest-first
//! batch of select_count outputs is chosen; while that batch cannot pay
//! fee_ceiling, the largest unselected candidate is swapped in for the
//! smallest selected one. Returns the number of swaps; the resulting batch
//! is values[swaps, select_count) plus values[size - swaps, size).
std::size_t SweepSwapCount(const std::vector<int64_t>& values, std::size_t select_count,
                           int64_t fee_ceiling);

/** A key allocated from the key pool. */
class CReserveKey
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    CPubKey vchPubKey;
public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        if (!ShutdownInProgress())
            ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey);
    //! Reserve the oldest key in the key pool as the pool stands: no top-up
    //! first, and no default key when the pool is empty, so no key is created.
    //! False when the pool is empty. KeepKey() keeps it; ReturnKey(), or the end
    //! of this object, puts it back in the pool.
    bool GetReservedKeyWithoutTopUp(CPubKey& pubkey);
    void KeepKey();
};


typedef std::map<std::string, std::string> mapValue_t;


static void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO : calculate elsewhere
        return;
    }

    nOrderPos = 0;
    if (!ParseInt64(mapValue["n"], &nOrderPos))
    {
        error("%s: nOrderPos cannot be parsed: %s.", __func__, mapValue["n"]);
    }
}


static void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = ToString(nOrderPos);
}

struct COutputEntry
{
     CTxDestination destination;
     int64_t amount;
     int vout;
};

// Moved here from main.h (issue #3125, workstream C3): only CWalletTx below
// derives from CMerkleTx, mirroring Bitcoin's relocation into the wallet.
/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
protected:
    virtual int GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const;
public:
    uint256 hashBlock;
    int nIndex;

    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    virtual ~CMerkleTx() = default;

    void Init()
    {
        hashBlock.SetNull();
        nIndex = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        std::vector<uint256> dummy_vector1; //!< Used to be vMerkleBranch
        READWRITEAS(CTransaction, *this);
        READWRITE(hashBlock);
        READWRITE(dummy_vector1);
        READWRITE(nIndex);
    }

    int SetMerkleBranch(const CBlock* pblock = nullptr);

    // Return depth of transaction in blockchain:
    // -1  : not in blockchain, and not in memory pool (conflicted transaction)
    //  0  : in memory pool, waiting to be included in a block
    // >=1 : this many blocks deep in the main chain
    int GetDepthInMainChain(CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChainINTERNAL(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    //! GetBlocksToMaturity() for a depth the caller has already looked up.
    int GetBlocksToMaturity(int depth) const;
    bool AcceptToMemoryPool();
};

/** A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

    // Uses m_state instead of legacy hashBlock/nIndex for depth calculation.
    int GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Canonical transaction state (replaces legacy hashBlock-based inference).
    //! Private: every mutation must go through SetTxState(), which keeps the
    //! legacy hashBlock/nIndex fields synced. m_state is the single source of truth.
    TxState m_state;

public:
    std::vector<CMerkleTx> vtxPrev;
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived;  // time received by this node
    unsigned int nTimeSmart;
    char fFromMe;
    std::string strFromAccount;
    std::vector<char> vfSpent; // which outputs are already spent
    int64_t nOrderPos;  // position in ordered transaction list

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fAvailableCreditCached;
	mutable bool fWatchDebitCached;
	mutable bool fWatchCreditCached;
    mutable bool fChangeCached;
    mutable int64_t nDebitCached;
    mutable int64_t nCreditCached;
    mutable int64_t nAvailableCreditCached;
	mutable int64_t nWatchDebitCached;
	mutable int64_t nWatchCreditCached;
    mutable int64_t nChangeCached;

    CWalletTx()
    {
        Init(nullptr);
    }

    CWalletTx(const CWallet* pwalletIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        vtxPrev.clear();
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        vfSpent.clear();
        fDebitCached = false;
        fCreditCached = false;
        fAvailableCreditCached = false;
		fWatchDebitCached = false;
		fWatchCreditCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nAvailableCreditCached = 0;
		nWatchDebitCached = 0;
		nWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
        m_state = TxStateUnrecognized{}; // Default to unrecognized
    }

    //! Get state if it matches type T, else nullptr.
    template<typename T>
    const T* state() const {
        return std::get_if<T>(&m_state);
    }

    template<typename T>
    T* state() {
        return std::get_if<T>(&m_state);
    }

    bool isConfirmed() const {
        return std::holds_alternative<TxStateConfirmed>(m_state);
    }

    bool isInMempool() const {
        return std::holds_alternative<TxStateInMempool>(m_state);
    }

    bool isInactive() const {
        return std::holds_alternative<TxStateInactive>(m_state);
    }

    bool isUnrecognized() const {
        return std::holds_alternative<TxStateUnrecognized>(m_state);
    }

    //! Derive legacy hashBlock/nIndex from m_state.
    //! @deprecated These fields exist only for backward-compatible serialization
    //! and legacy code paths. m_state is the single source of truth.
    //! Call this after any m_state mutation to keep legacy fields in sync.
    void SyncLegacyFromState()
    {
        hashBlock = TxStateSerializedBlockHash(m_state);
        nIndex = TxStateSerializedIndex(m_state);
    }

    //! Read-only access to the canonical transaction state.
    const TxState& GetState() const { return m_state; }

    //! Set the canonical transaction state. This is the only supported way to
    //! mutate state from outside the class: it assigns m_state and immediately
    //! syncs the legacy hashBlock/nIndex fields, so callers can never forget.
    void SetTxState(const TxState& new_state)
    {
        m_state = new_state;
        SyncLegacyFromState();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        char fSpent = false;

        if (ser_action.ForRead()) {
            Init(nullptr);
        } else {
            mapValue["fromaccount"] = strFromAccount;

            std::string str;
            for (auto const& f : vfSpent)
            {
                str += (f ? '1' : '0');
                if (f)
                    fSpent = true;
            }

            mapValue["spent"] = str;

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart) {
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
            }
        }

        READWRITEAS(CMerkleTx, *this);
        READWRITE(vtxPrev);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead()) {
            strFromAccount = mapValue["fromaccount"];

            if (mapValue.count("spent")) {
                for (auto const& c :  mapValue["spent"]) {
                    vfSpent.push_back(c != '0');
                }
            } else {
                vfSpent.assign(vout.size(), fSpent);
            }

            ReadOrderPos(nOrderPos, mapValue);

            if (!mapValue.count("timesmart") || !ParseUInt32(mapValue["timesmart"], &nTimeSmart)) {
                nTimeSmart = 0;
            }

            // Reconstruct m_state from the legacy hashBlock/nIndex fields
            // that CMerkleTx deserialized. Works for both old and new wallets.
            // TxStateConfirmed does not carry a height — the wallet.dat format
            // does not persist one. Consumers that need a height call
            // GetConfirmedHeight(state) which looks it up from mapBlockIndex
            // on demand (single ~50 ns unordered_map lookup).
            m_state = MigrateFromLegacyHashBlock(hashBlock, nIndex);
        }

        // Clean up temporary mapValue entries
        if (ser_action.ForRead()) {
            mapValue.erase("fromaccount");
            mapValue.erase("version");
            mapValue.erase("spent");
            mapValue.erase("n");
            mapValue.erase("timesmart");
        }
    }

    // marks certain txout's as spent
    // returns true if any update took place
    bool UpdateSpent(const std::vector<char>& vfNewSpent)
    {
        bool fReturn = false;
        for (unsigned int i = 0; i < vfNewSpent.size(); i++)
        {
            if (i == vfSpent.size())
                break;

            if (vfNewSpent[i] && !vfSpent[i])
            {
                vfSpent[i] = true;
                fReturn = true;
                fAvailableCreditCached = false;
            }
        }
        return fReturn;
    }

    // make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
		fWatchDebitCached = false;
		fWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    void MarkSpent(unsigned int nOut)
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::MarkSpent() : nOut out of range");
        vfSpent.resize(vout.size());
        if (!vfSpent[nOut])
        {
            vfSpent[nOut] = true;
            fAvailableCreditCached = false;
        }
    }

    void MarkUnspent(unsigned int nOut)
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::MarkUnspent() : nOut out of range");
        vfSpent.resize(vout.size());
        if (vfSpent[nOut])
        {
            vfSpent[nOut] = false;
            fAvailableCreditCached = false;
        }
    }

    bool IsSpent(unsigned int nOut) const
    {
        if (nOut >= vout.size())
            throw std::runtime_error("CWalletTx::IsSpent() : nOut out of range");
        if (nOut >= vfSpent.size())
            return false;
        return (!!vfSpent[nOut]);
    }

    int64_t GetDebit(const isminefilter& filter=(ISMINE_SPENDABLE|ISMINE_WATCH_ONLY)) const
    {
		if (vin.empty())
            return 0;

		 int64_t debit = 0;
         if(filter & ISMINE_SPENDABLE)
         {
             if (fDebitCached)
                 debit += nDebitCached;
             else
             {
                 nDebitCached = pwallet->GetDebit(*this, ISMINE_SPENDABLE);
                 fDebitCached = true;
                 debit += nDebitCached;
             }
         }
         if(filter & ISMINE_WATCH_ONLY)
         {
             if(fWatchDebitCached)
                 debit += nWatchDebitCached;
             else
             {
                 nWatchDebitCached = pwallet->GetDebit(*this, ISMINE_WATCH_ONLY);
                 fWatchDebitCached = true;
                 debit += nWatchDebitCached;
             }
         }
         return debit;

    }

    int64_t GetCredit(bool fUseCache=true) const
    {
        // Must wait until coinbase is safely deep enough in the chain before valuing it
        if ((IsCoinBase() || IsCoinStake()) && GetBlocksToMaturity() > 0)
            return 0;

        // GetBalance can assume transactions in mapWallet won't change
        if (fUseCache && fCreditCached)
            return nCreditCached;
        nCreditCached = pwallet->GetCredit(*this);
        fCreditCached = true;
        return nCreditCached;
    }

    int64_t GetAvailableCredit(bool fUseCache=true) const
    {
        return GetAvailableCredit(GetBlocksToMaturity(), fUseCache);
    }

    //! GetAvailableCredit() for a GetBlocksToMaturity() the caller has already
    //! computed.
    int64_t GetAvailableCredit(int blocks_to_maturity, bool fUseCache) const
    {
        // Must wait until coinbase is safely deep enough in the chain before valuing it
        if ((IsCoinBase() || IsCoinStake()) && blocks_to_maturity > 0)
            return 0;

        if (fUseCache && fAvailableCreditCached)
            return nAvailableCreditCached;

        int64_t nCredit = 0;
        for (unsigned int i = 0; i < vout.size(); i++)
        {
            if (!IsSpent(i))
            {
                const CTxOut &txout = vout[i];
                // SAFETY CHECK: Ensure pwallet is valid before dereferencing
                if (pwallet)
                {
                    nCredit += pwallet->GetCredit(txout);
                    if (!MoneyRange(nCredit))
                        throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
                }
            }
        }

        nAvailableCreditCached = nCredit;
        fAvailableCreditCached = true;
        return nCredit;
    }


    int64_t GetChange() const
    {
        if (fChangeCached)
            return nChangeCached;
        nChangeCached = pwallet->GetChange(*this);
        fChangeCached = true;
        return nChangeCached;
    }

    void GetAmounts(std::list<COutputEntry>& listReceived, std::list<COutputEntry>& listSent, int64_t& nFee, std::string& strSentAccount,
        const isminefilter& filter=(ISMINE_SPENDABLE|ISMINE_WATCH_ONLY)) const;


    void GetAccountAmounts(const std::string& strAccount, int64_t& nReceived,
                              int64_t& nSent, int64_t& nFee, const isminefilter& filter=(ISMINE_SPENDABLE|ISMINE_WATCH_ONLY)) const;
    bool IsFromMe(const isminefilter& filter=(ISMINE_SPENDABLE|ISMINE_WATCH_ONLY)) const
    {
        return (GetDebit(filter) > 0);
    }
    bool IsConfirmed() const
    {
        return GetDepthInMainChain() >= 10;
    }

    bool AreDependenciesConfirmed() const
    {
        // If no confirmations but it's from us, we can still
        // consider it confirmed if all dependencies are confirmed
        std::map<uint256, const CMerkleTx*> mapPrev;
        std::vector<const CMerkleTx*> vWorkQueue;
        vWorkQueue.reserve(vtxPrev.size()+1);
        vWorkQueue.push_back(this);
        for (unsigned int i = 0; i < vWorkQueue.size(); i++)
        {
            const CMerkleTx* ptx = vWorkQueue[i];

            if (!IsFinalTx(*ptx))
                return false;
            int nPDepth = ptx->GetDepthInMainChain();
            if (nPDepth >= 1)
                continue;
            if (nPDepth < 0)
                return false;
            if (!pwallet->IsFromMe(*ptx))
                return false;

            if (mapPrev.empty())
            {
                for (auto const& tx : vtxPrev)
                    mapPrev[tx.GetHash()] = &tx;
            }

            for (auto const& txin : ptx->vin)
            {
                if (!mapPrev.count(txin.prevout.hash))
                    return false;
                vWorkQueue.push_back(mapPrev[txin.prevout.hash]);
            }
        }

        return true;
    }

    bool IsTrusted() const
    {
        // Quick answer in most cases
        if (!IsFinalTx(*this))
            return false;
        return IsTrusted(GetDepthInMainChain());
    }

    //! IsTrusted() for a GetDepthInMainChain() the caller has already looked up.
    bool IsTrusted(int nDepth) const
    {
        int nMinConfirmsRequiredToSendGRC = 3;
        if (!IsFinalTx(*this))
            return false;
        if (nDepth >= nMinConfirmsRequiredToSendGRC)
            return true;
        if (nDepth < 0)
            return false;
        if (fConfChange || !IsFromMe()) // using wtx's cached debit
            return false;

        // If no confirmations but it's from us, we can still
        // consider it confirmed if all dependencies are confirmed

        return AreDependenciesConfirmed();
    }

    bool WriteToDisk(CWalletDB *pwalletdb);

    int64_t GetTxTime() const;
    int GetRequestCount() const;

    void AddSupportingTransactions(CTxDB& txdb);

    bool AcceptWalletTransaction(CTxDB& txdb);
    bool AcceptWalletTransaction();

    //! Queue this transaction (and its unindexed parents) for announcement.
    //! Touches no network state. Returns false when the transaction is inactive
    //! and nothing was queued.
    //!
    //! This is the overload to use while holding cs_wallet: let the caller's
    //! DeferredRelay outlive the lock, because announcing under cs_wallet closes
    //! an ABBA cycle with a peer's cs_inventory (#3391).
    bool QueueRelay(CTxDB& txdb, DeferredRelay& relay) const;

    //! Announce this transaction (and its unindexed parents) to peers. Returns
    //! false when the transaction is inactive and nothing was relayed.
    //!
    //! Announces before returning, so the caller must NOT hold cs_wallet; use
    //! QueueRelay if it does.
    bool RelayWalletTransaction(CTxDB& txdb);
    bool RelayWalletTransaction();

    //! What ResendWalletTransactions may do with a transaction it is about to
    //! re-announce. INPUTS_UNAVAILABLE is not a verdict on the transaction: an
    //! input is in neither the transaction index nor the mempool, which is what a
    //! child reads after its parent was evicted with it.
    enum class RevalidateResult { VALID, INPUTS_UNAVAILABLE, INVALID };

    RevalidateResult RevalidateTransaction(CTxDB& txdb) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    GRC::MinedType GetGeneratedType(uint32_t vout_offset) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        return ::GetGeneratedType(pwallet, GetHash(), vout_offset);
    }
};




class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn;
    }

    std::string ToString() const
    {
        return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString().substr(0,10).c_str(), i, nDepth, FormatMoney(tx->vout[i].nValue));
    }

    void print() const
    {
        LogPrintf("%s", ToString());
    }
};




/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //// todo: add something to note what created it (user, getnewaddress, change)
    ////   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires=0)
    {
        nTimeCreated = (nExpires ? GetAdjustedTime() : 0);
        nTimeExpires = nExpires;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            int nVersion = s.GetVersion();
            READWRITE(nVersion);
        }

        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(strComment);
    }
};






/** Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            int nVersion = s.GetVersion();
            READWRITE(nVersion);
        }

        READWRITE(vchPubKey);
    }
};



/** Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    int64_t nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64_t nOrderPos;  // position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        CAccountingEntry& me = *const_cast<CAccountingEntry*>(this);
        int nVersion = s.GetVersion();

        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }

        // Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(strOtherAccount);

        if (!ser_action.ForRead()) {
            WriteOrderPos(nOrderPos, me.mapValue);

            if (!(mapValue.empty() && _ssExtra.empty())) {
                CDataStream ss(s.GetType(), nVersion);
                ss.write(AsBytes(Span<const char>{"\0", 1}));
                ss << mapValue;
                ss.write(MakeByteSpan(_ssExtra));
                me.strComment.append(ss.str());
            }
        }

        READWRITE(strComment);

        size_t nSepPos = strComment.find("\0", 0, 1);

        if (ser_action.ForRead()) {
            me.mapValue.clear();

            if (std::string::npos != nSepPos) {
                CDataStream ss(
                    Span((std::byte*)&(strComment.begin() + nSepPos + 1)[0], (std::byte*)&strComment.end()[0]),
                    s.GetType(),
                    nVersion);

                ss >> me.mapValue;
                me._ssExtra = std::vector<char>((char*)&ss.begin()[0], (char*)&ss.end()[0]);
            }

            ReadOrderPos(me.nOrderPos, me.mapValue);
        }

        if (std::string::npos != nSepPos) {
            me.strComment.erase(nSepPos);
        }

        me.mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

#endif // BITCOIN_WALLET_WALLET_H
