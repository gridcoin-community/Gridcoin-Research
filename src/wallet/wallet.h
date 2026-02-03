// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <stdlib.h>

#include "amount.h"
#include "consensus/tx_verify.h"
#include "gridcoin/staking/status.h"
#include "main.h"
#include "key.h"
#include "keystore.h"
#include "primitives/transaction.h"
#include "script.h"
#include "streams.h"
#include "node/ui_interface.h"
#include <util/string.h>
#include "wallet/generated_type.h"
#include "wallet/transaction.h"
#include "wallet/walletdb.h"
#include <wallet/walletutil.h>
#include "wallet/ismine.h"

extern bool fWalletUnlockStakingOnly;
extern bool fConfChange;
class CAccountingEntry;
class CWalletTx;
class CReserveKey;
class COutput;
class CCoinControl;

GRC::MinedType GetGeneratedType(const CWallet *wallet, const uint256& tx, unsigned int vout);

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

/** A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore
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

    //!
    //! \brief Get the imported master private key used to sign administrative
    //! contracts.
    //!
    //! A master key holder uses the master private key to sign administrative
    //! contracts, such as project whitelist additions and removals. All nodes
    //! verify the contracts using the embedded public key that corresponds to
    //! this private key.
    //!
    //! Key holders need to import the private key into the wallet before they
    //! can sign administrative contracts. The following RPC command imports a
    //! private key and assigns to it the optional label of "master":
    //!
    //!    importprivkey <hex_key_string> master
    //!
    //! Note that this private key differs from the wallet keystore's "master"
    //! key which the wallet uses to encrypt the private keys in storage.
    //!
    //! \param height The block height which the master key is valid for.
    //!
    //! \return An empty key when no master key imported.
    //!
    CKey MasterPrivateKey(int height) const;

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

    std::map<CTxDestination, std::string> mapAddressBook GUARDED_BY(cs_wallet);

    /**
     * Transaction Spending Tracker (mapTxSpends)
     *
     * WHY THIS EXISTS:
     * Essential for detecting transaction conflicts (double-spends) and tracking
     * which transactions spend which outputs. When two transactions attempt to
     * spend the same output, we need to quickly identify the conflict.
     *
     * STRUCTURE:
     * - Key: COutPoint (txid + vout index) - identifies a specific output
     * - Value: txid - the transaction that spends this output
     * - multimap because during reorgs, temporarily multiple txs may reference same output
     *
     * USAGE PATTERNS:
     * 1. When transaction added: Record all inputs in mapTxSpends
     * 2. When checking conflicts: Look up each input to see if already spent
     * 3. When transaction removed: Clean up spending records
     *
     * EXAMPLE:
     *   Transaction A (txid=aaa) creates output 0
     *   Transaction B (txid=bbb) spends A:0 as input
     *   → mapTxSpends[COutPoint(aaa, 0)] = bbb
     *
     * THREAD SAFETY: Protected by cs_wallet
     */
    std::multimap<COutPoint, uint256> mapTxSpends GUARDED_BY(cs_wallet);

    /**
     * Last Block Processing Markers (m_last_block_processed)
     *
     * WHY THIS EXISTS:
     * Tracks which block the wallet last processed to enable incremental
     * updates and proper reorg handling. Without this, we couldn't tell if
     * we're seeing a new block or reprocessing an old one.
     *
     * USAGE:
     * - Updated when blockConnected() or blockDisconnected() is called
     * - Used to detect gaps in block processing
     * - Helps determine if we need to rescan
     * - Critical for proper reorg detection
     *
     * REORG SCENARIO:
     *   Chain was: A -> B -> C (m_last_block_processed = C)
     *   Reorg to:  A -> B -> D -> E
     *   1. blockDisconnected(C) called (m_last_block_processed = B)
     *   2. blockConnected(D) called (m_last_block_processed = D)
     *   3. blockConnected(E) called (m_last_block_processed = E)
     *
     * THREAD SAFETY: Protected by cs_wallet
     */
    uint256 m_last_block_processed GUARDED_BY(cs_wallet);
    int m_last_block_processed_height GUARDED_BY(cs_wallet) = 0;

    CPubKey vchDefaultKey GUARDED_BY(cs_wallet);
    int64_t nTimeFirstKey GUARDED_BY(cs_wallet);

    // check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum wallet::WalletFeature wf) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        AssertLockHeld(cs_wallet);
        return wallet::IsFeatureSupported(nWalletVersion, wf);
    }

    void AvailableCoinsForStaking(std::vector<COutput>& vCoins, unsigned int nSpendTime, int64_t& nBalanceOut) const;
    bool SelectCoinsForStaking(unsigned int nSpendTime, std::vector<std::pair<const CWalletTx*,unsigned int> >& vCoinsRet,
                               GRC::MinerStatus::ErrorFlags& not_staking_error, int64_t& balance_out, bool fMiner = false) const;
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed = true, const CCoinControl* coinControl = nullptr,
                        bool fIncludeStakingCoins = false) const;
    bool SelectCoinsMinConf(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins,
                            std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;
    bool SelectSmallestCoins(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins,
                             std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;

    // keystore implementation
    // Generate a new key
    CPubKey GenerateNewKey();
    // Adds a key to the store, and saves it to disk.
    bool AddKey(const CKey& key);
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
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    // Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const;


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
    bool AddToWallet(const CWalletTx& wtxIn, CWalletDB *pwalletdb);
    bool EraseFromWallet(uint256 hash);

    /**
     * SyncTransaction: Unified Transaction State Synchronization
     *
     * WHY THIS EXISTS:
     * This is THE central entry point for updating wallet state when transactions
     * change state. Previously, transaction updates were scattered across multiple
     * functions. SyncTransaction consolidates all updates into one place, ensuring
     * consistency and reducing bugs.
     *
     * RESPONSIBILITIES:
     * 1. Add/update transaction in mapWallet
     * 2. Update mapTxSpends for conflict tracking
     * 3. Trigger wallet balance updates
     * 4. Emit UI notifications
     * 5. Persist changes to wallet.dat
     *
     * WHEN CALLED:
     * - transactionAddedToMempool() → calls with TxStateInMempool
     * - blockConnected() → calls with TxStateConfirmed for each tx in block
     * - blockDisconnected() → calls with TxStateInMempool or removes tx
     * - transactionRemovedFromMempool() → calls with TxStateInactive if conflicted
     *
     * STATE TRANSITIONS HANDLED:
     * 1. New tx → mempool: SyncTransaction(tx, TxStateInMempool{})
     * 2. Mempool → confirmed: SyncTransaction(tx, TxStateConfirmed{hash, height, pos})
     * 3. Confirmed → mempool (reorg): SyncTransaction(tx, TxStateInMempool{})
     * 4. Mempool → conflicted: SyncTransaction(tx, TxStateInactive{false})
     * 5. Mempool → abandoned: SyncTransaction(tx, TxStateInactive{true})
     *
     * PARAMETERS EXPLAINED:
     * @param ptx               Transaction being synchronized (shared_ptr for efficiency)
     * @param state             New state for the transaction (see wallet/transaction.h)
     * @param update_tx         If true, update existing transaction; if false, only add new
     * @param rescanning_old_block  If true, we're rescanning (affects UI notifications)
     *
     * @return true if wallet was modified, false if transaction not relevant to wallet
     *
     * DESIGN NOTE - Why shared_ptr:
     * Using CTransactionRef (shared_ptr) avoids copying large transactions. The
     * validation layer holds transactions in shared_ptr form, so we accept that
     * directly rather than forcing a copy.
     *
     * THREAD SAFETY: Requires cs_wallet lock (checked at runtime via EXCLUSIVE_LOCKS_REQUIRED)
     */
    bool SyncTransaction(const CTransactionRef& ptx,
                        const wallet::TxState& state,
                        bool update_tx = true,
                        bool rescanning_old_block = false) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Legacy overload accepting CTransaction
     *
     * WHY THIS EXISTS:
     * Some older code still works with CTransaction directly rather than
     * CTransactionRef. This overload converts on the fly to avoid forcing
     * a large refactoring of existing code.
     *
     * NOTE: Creates a shared_ptr internally, so has some overhead. Prefer
     * the CTransactionRef version when possible.
     */
    bool SyncTransaction(const CTransaction& tx,
                        const wallet::TxState& state,
                        bool update_tx = true)
    {
        return SyncTransaction(MakeTransactionRef(tx), state, update_tx, false);
    }

    /**
     * ========================================================================
     * VALIDATION INTERFACE CALLBACKS
     * ========================================================================
     *
     * OVERVIEW:
     * These methods are called by the validation layer to notify the wallet
     * of chain state changes. They form the "glue" between chain validation
     * and wallet state management.
     *
     * DESIGN PATTERN:
     * This follows Bitcoin Core's validation interface pattern. The validation
     * layer doesn't know about wallets directly - instead it calls registered
     * callbacks. This keeps validation and wallet code decoupled.
     *
     * CALL SEQUENCE DURING NORMAL BLOCK:
     * 1. Validation accepts block to chain
     * 2. For each tx in block: transactionRemovedFromMempool(tx, BLOCK)
     * 3. blockConnected(block, height)
     * 4. Wallet processes each tx via SyncTransaction
     *
     * CALL SEQUENCE DURING REORG:
     * 1. Old blocks disconnected: blockDisconnected(old_block, height)
     * 2. New blocks connected: blockConnected(new_block, height)
     * 3. Orphaned txs return to mempool: transactionAddedToMempool(tx)
     *
     * THREAD SAFETY:
     * All callbacks require cs_wallet lock. The validation layer must acquire
     * this lock before calling. This prevents race conditions between chain
     * updates and wallet operations.
     */

    /**
     * transactionAddedToMempool: Transaction entered mempool
     *
     * WHEN CALLED:
     * - New transaction broadcast and accepted to mempool
     * - During reorg, when confirmed tx returns to mempool
     *
     * RESPONSIBILITIES:
     * - Call SyncTransaction with TxStateInMempool
     * - Update balance if tx involves our addresses
     * - Notify UI of new pending transaction
     *
     * WHY SEPARATE FROM blockConnected:
     * Mempool transactions need special handling - they're unconfirmed and
     * can be evicted. We track them differently from confirmed transactions.
     */
    void transactionAddedToMempool(const CTransactionRef& tx) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * blockConnected: New block added to active chain
     *
     * WHEN CALLED:
     * - New block validated and connected to tip
     * - During reorg, when alternative chain becomes active
     *
     * RESPONSIBILITIES:
     * - Update m_last_block_processed
     * - For each tx in block: Call SyncTransaction with TxStateConfirmed
     * - Mark conflicting mempool txs as inactive
     * - Update wallet balance and confirmations
     * - Notify UI of confirmed transactions
     *
     * CRITICAL ORDERING:
     * Must process transactions in block order (not random) to maintain
     * consistent state during wallet rescans.
     */
    void blockConnected(const CBlock& block, int height) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * transactionRemovedFromMempool: Transaction left mempool
     *
     * WHEN CALLED:
     * - Transaction confirmed in block (reason = BLOCK)
     * - Transaction conflicted with confirmed tx (reason = CONFLICT)
     * - Transaction evicted due to mempool limits (reason = EXPIRY or SIZELIMIT)
     * - Transaction replaced by higher fee (reason = REPLACED)
     *
     * RESPONSIBILITIES:
     * - If CONFLICT: Mark tx as inactive (conflicted)
     * - If BLOCK: No action needed (blockConnected handles confirmation)
     * - If EXPIRY/SIZELIMIT: Remove from wallet if not ours
     * - Update UI to reflect removal
     *
     * WHY WE NEED reason PARAMETER:
     * Different removal reasons require different handling. Conflicted txs
     * should be marked inactive, but confirmed txs are handled by blockConnected.
     */
    void transactionRemovedFromMempool(const CTransactionRef& tx,
                                      MemPoolRemovalReason reason) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * blockDisconnected: Block removed from active chain (reorg)
     *
     * WHEN CALLED:
     * - During blockchain reorganization
     * - When a block becomes invalid
     * - During chain rollback (rare)
     *
     * RESPONSIBILITIES:
     * - Update m_last_block_processed to parent block
     * - For each tx in disconnected block:
     *   * If tx still valid: Return to mempool (TxStateInMempool)
     *   * If tx now conflicted: Mark inactive (TxStateInactive)
     * - Revert wallet balance changes from this block
     * - Notify UI of confirmation count changes
     *
     * CRITICAL:
     * Must handle reorgs correctly to avoid lost transactions or double-spends.
     * This is one of the most complex wallet scenarios.
     */
    void blockDisconnected(const CBlock& block, int height) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * ========================================================================
     * TRANSACTION CONFLICT TRACKING AND ABANDONMENT
     * ========================================================================
     *
     * OVERVIEW:
     * These methods handle scenarios where transactions compete for the same
     * inputs (conflicts) or where users give up on unconfirmed transactions
     * (abandonment).
     */

    /**
     * GetConflicts: Find transactions conflicting with given transaction
     *
     * WHY THIS EXISTS:
     * When two transactions spend the same input, only one can confirm. We
     * need to identify conflicts to:
     * 1. Mark losing transaction as inactive/conflicted
     * 2. Show user why their transaction failed
     * 3. Free up inputs for new transactions
     *
     * HOW IT WORKS:
     * 1. Look up each input of txid in mapTxSpends
     * 2. Find all OTHER transactions spending those same inputs
     * 3. Return set of conflicting transaction IDs
     *
     * EXAMPLE:
     *   TxA and TxB both try to spend output X:0
     *   If TxA confirms, GetConflicts(TxB) returns {TxA}
     *   TxB is then marked conflicted/inactive
     *
     * THREAD SAFETY: Requires cs_wallet lock
     */
    std::set<uint256> GetConflicts(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * IsAbandoned: Check if transaction was explicitly abandoned
     *
     * WHY THIS EXISTS:
     * Users need to know if they intentionally gave up on a transaction.
     * Abandoned transactions:
     * - Won't be rebroadcast
     * - Free up their inputs for reuse
     * - Show differently in UI
     *
     * USAGE:
     * Called by RPC commands (listtransactions, gettransaction) to determine
     * transaction display status.
     *
     * IMPLEMENTATION:
     * Checks if tx state is TxStateInactive with abandoned=true
     *
     * THREAD SAFETY: Requires cs_wallet lock
     */
    bool IsAbandoned(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * AbandonTransaction: Explicitly give up on unconfirmed transaction
     *
     * WHY THIS EXISTS:
     * Sometimes transactions get "stuck" (low fee, network issues, etc). Rather
     * than wait forever, users can abandon them to:
     * 1. Free up inputs for new transactions
     * 2. Stop wallet from rebroadcasting
     * 3. Clear up UI with stuck pending transactions
     *
     * SAFETY CHECKS:
     * - Transaction must be unconfirmed (can't abandon confirmed tx!)
     * - Transaction must exist in wallet
     * - Must not have conflicting confirmed transaction
     *
     * WHAT IT DOES:
     * 1. Marks transaction state as TxStateInactive{abandoned=true}
     * 2. Stops rebroadcasting this transaction
     * 3. Frees up inputs for use in new transactions
     * 4. Updates wallet balance (removes pending credit)
     * 5. Notifies UI of status change
     *
     * REORG HANDLING:
     * If abandoned tx later appears in a block (edge case), we'll re-activate it.
     * The abandoned flag is a user preference, not a permanent state.
     *
     * RPC INTERFACE:
     * Exposed via abandontransaction RPC command
     *
     * @param txid Transaction to abandon
     * @return true if successfully abandoned, false if can't abandon (confirmed, etc)
     *
     * THREAD SAFETY: Requires cs_wallet lock
     */
    bool AbandonTransaction(const uint256& txid) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

private:
    /**
     * Add transaction to wallet if it involves this wallet's addresses
     * Internal helper called by SyncTransaction
     * This is the new implementation using TxState
     */
    bool AddToWalletIfInvolvingMe(const CTransactionRef& ptx,
                                  const wallet::TxState& state,
                                  bool fUpdate) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

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
    void ResendWalletTransactions(bool fForce = false);
    int64_t GetBalance() const;
    int64_t GetUnconfirmedBalance() const;
    int64_t GetImmatureBalance() const;
    int64_t GetStake() const;
    int64_t GetNewMint() const;
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t>>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey,
                           int64_t& nFeeRet, const CCoinControl* coinControl = nullptr, bool change_back_to_input_address = false);
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t>>& vecSend, std::set<std::pair<const CWalletTx*,unsigned int>>& setCoins,
                           CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl = nullptr,
                           bool change_back_to_input_address = false);
    bool CreateTransaction(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet,
                           const CCoinControl* coinControl = nullptr, bool change_back_to_input_address = false);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);

    std::string SendMoney(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);
    std::string SendMoneyToDestination(const CTxDestination &address, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);

    bool NewKeyPool();
    bool TopUpKeyPool(unsigned int nSize = 0);
    int64_t AddReserveKey(const CKeyPool& keypool);
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool);
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
    void SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);

    bool SetAddressBookName(const CTxDestination& address, const std::string& strName);

    bool DelAddressBookName(const CTxDestination& address);

    void UpdatedTransaction(const uint256 &hashTx);

    void PrintWallet(const CBlock& block);

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
    void DisableTransaction(const CTransaction &tx);

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
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination &address, const std::string &label, bool isMine, ChangeType status)> NotifyAddressBookChanged;

    /** Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx, ChangeType status)> NotifyTransactionChanged;

    /* Set the HD chain model (chain child index counters) */
    bool SetHDChain(const CHDChain& chain, bool memonly);
    const CHDChain& GetHDChain() { return hdChain; }

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
        if (!fShutdown)
            ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey);
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

/** A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

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

    /**
     * New transaction state tracking
     * Replaces reliance on hashBlock for state determination
     */
    wallet::TxState m_state;

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
        m_state = wallet::TxStateUnrecognized{}; // Default to unrecognized
    }

    /**
     * Get transaction state of specific type
     * @return Pointer to state if type matches, nullptr otherwise
     */
    template<typename T>
    const T* state() const {
        return std::get_if<T>(&m_state);
    }

    template<typename T>
    T* state() {
        return std::get_if<T>(&m_state);
    }

    /**
     * Check if transaction is confirmed
     */
    bool isConfirmed() const {
        return std::holds_alternative<wallet::TxStateConfirmed>(m_state);
    }

    /**
     * Check if transaction is in mempool
     */
    bool isInMempool() const {
        return std::holds_alternative<wallet::TxStateInMempool>(m_state);
    }

    /**
     * Check if transaction is inactive/conflicted
     */
    bool isInactive() const {
        return std::holds_alternative<wallet::TxStateInactive>(m_state);
    }

    /**
     * Check if transaction state is unrecognized (old format)
     */
    bool isUnrecognized() const {
        return std::holds_alternative<wallet::TxStateUnrecognized>(m_state);
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

            // Sync m_state to legacy fields for backward compatibility
            // Older wallet versions rely on hashBlock/nIndex
            if (const auto* conf = state<wallet::TxStateConfirmed>()) {
                hashBlock = conf->m_confirmed_block_hash;
                nIndex = conf->m_position_in_block;
            } else {
                hashBlock = uint256();  // Clear hashBlock for unconfirmed/inactive
                nIndex = -1;
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
        }

        // Direct m_state serialization (version-gated)
        // This replaces the buggy mapValue-based state storage
        if (s.GetVersion() >= wallet::FEATURE_TRANSACTION_STATES) {
            // New format: Deserialize m_state directly using variant deserialization
            if (!ser_action.ForRead()) {
                // WRITING: Serialize the current m_state
                wallet::SerializeTxState(s, m_state);
            } else {
                // READING: Deserialize the state and assign it
                // This restores the actual transaction state from wallet.dat
                wallet::TxState temp_state;
                wallet::UnserializeTxState(s, temp_state);
                m_state = temp_state;  // Assign the deserialized state
            }
        } else {
            // Legacy format: hashBlock is already deserialized above
            // Migrate from hashBlock to proper state based on whether block is set
            if (ser_action.ForRead()) {
                // If hashBlock is set, transaction was confirmed in the old format
                if (!hashBlock.IsNull()) {
                    m_state = wallet::TxStateConfirmed(hashBlock, -1, nIndex);
                } else {
                    // hashBlock is null, transaction was unconfirmed
                    m_state = wallet::TxStateUnrecognized{};
                }
            }
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
        // Must wait until coinbase is safely deep enough in the chain before valuing it
        if ((IsCoinBase() || IsCoinStake()) && GetBlocksToMaturity() > 0)
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
		int nMinConfirmsRequiredToSendGRC = 3;
        // Quick answer in most cases
        if (!IsFinalTx(*this))
            return false;
        int nDepth = GetDepthInMainChain();
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

    void RelayWalletTransaction(CTxDB& txdb);
    void RelayWalletTransaction();

    bool RevalidateTransaction(CTxDB& txdb);

    GRC::MinedType GetGeneratedType(uint32_t vout_offset) const
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
