// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLET_H
#define BITCOIN_WALLET_H

#include <string>
#include <vector>
#include <set>
#include <stdlib.h>

#include "amount.h"
#include "gridcoin/staking/status.h"
#include "main.h"
#include "key.h"
#include "keystore.h"
#include "script.h"
#include "streams.h"
#include "ui_interface.h"
#include "wallet/walletdb.h"
#include "wallet/ismine.h"

extern bool fWalletUnlockStakingOnly;
extern bool fConfChange;
class CAccountingEntry;
class CWalletTx;
class CReserveKey;
class COutput;
class CCoinControl;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)
    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys
    FEATURE_LATEST = 60000
};

/** (POS/POR) enums for CoinStake Transactions -- We should never get unknown but just in case!*/
enum MinedType
{
    UNKNOWN = 0,
    POS = 1,
    POR = 2,
    ORPHANED = 3,
    POS_SIDE_STAKE_RCV = 4,
    POR_SIDE_STAKE_RCV = 5,
    POS_SIDE_STAKE_SEND = 6,
    POR_SIDE_STAKE_SEND = 7,
    SUPERBLOCK = 8
};

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
    bool SelectCoins(int64_t nTargetValue, unsigned int nSpendTime, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet, const CCoinControl *coinControl=NULL, bool contract = false) const;

    CWalletDB *pwalletdbEncryption;

    // the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    // the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

public:
    /// Main wallet lock.
    /// This lock protects all the fields added by CWallet
    ///   except for:
    ///      fFileBacked (immutable after instantiation)
    ///      strWalletFile (immutable after instantiation)
    mutable CCriticalSection cs_wallet;

    bool fFileBacked;
    std::string strWalletFile;

    std::set<int64_t> setKeyPool;
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;


    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    CWallet()
    {
        SetNull();
    }

    CWallet(std::string strWalletFileIn)
    {
        SetNull();

        strWalletFile = strWalletFileIn;
        fFileBacked = true;
    }

    //!
    //! \brief Get the public key used to verify administrative contracts.
    //!
    //! The wallet embeds the master public key so that every node can verify
    //! the authenticity of administrative contracts like a project whitelist
    //! addition or removal. The master key holders must import a private key
    //! that corresponds to this public key before they can sign contracts.
    //!
    //! \return A \c CPubKey object containing the master public key.
    //!
    static const CPubKey& MasterPublicKey();

    //!
    //! \brief Get the output address controlled by the master private key used
    //! to verify administrative contracts.
    //!
    //! \return Address as calculated from the master public key.
    //!
    static const CBitcoinAddress MasterAddress();

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
    //! \return An empty key when no master key imported.
    //!
    CKey MasterPrivateKey() const;

    void SetNull()
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nTimeFirstKey = 0;
    }

    std::map<uint256, CWalletTx> mapWallet;
    int64_t nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<CTxDestination, std::string> mapAddressBook;

    CPubKey vchDefaultKey;
    int64_t nTimeFirstKey;

    // check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; }

    void AvailableCoinsForStaking(std::vector<COutput>& vCoins, unsigned int nSpendTime, int64_t& nBalanceOut) const;
    bool SelectCoinsForStaking(unsigned int nSpendTime, std::vector<std::pair<const CWalletTx*,unsigned int> >& vCoinsRet,
                               GRC::MinerStatus::ReasonNotStakingCategory& not_staking_error, int64_t& balance_out, bool fMiner = false) const;
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl=NULL, bool fIncludeStakingCoins=false) const;
    bool SelectCoinsMinConf(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;
    bool SelectSmallestCoins(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;

    // keystore implementation
    // Generate a new key
    CPubKey GenerateNewKey();
    // Adds a key to the store, and saves it to disk.
    bool AddKey(const CKey& key);
    // Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key) { return CCryptoKeyStore::AddKey(key); }
    // Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &metadata);

    bool LoadMinVersion(int nVersion) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }

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
    int64_t IncOrderPosNext(CWalletDB *pwalletdb = NULL);

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;

    /** Get the wallet's activity log
        @return multimap of ordered transactions and accounting entries
        @warning Returned pointers are *only* valid within the scope of passed acentries
     */
    TxItems OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount = "");

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, CWalletDB *pwalletdb);
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate = false, bool fFindBlock = false);
    bool EraseFromWallet(uint256 hash);
    void WalletUpdateSpent(const CTransaction &tx, bool fBlock, CWalletDB* pwalletdb);
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    void ReacceptWalletTransactions();
    void ResendWalletTransactions(bool fForce = false);
    int64_t GetBalance() const;
    int64_t GetUnconfirmedBalance() const;
    int64_t GetImmatureBalance() const;
    int64_t GetStake() const;
    int64_t GetNewMint() const;
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl *coinControl=NULL);
    bool CreateTransaction(const std::vector<std::pair<CScript, int64_t> >& vecSend, std::set<std::pair<const CWalletTx*,unsigned int>>& setCoins, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl *coinControl=NULL);
    bool CreateTransaction(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl *coinControl=NULL);
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
    std::vector<std::pair<CBitcoinAddress, CBitcoinSecret>> GetAllPrivateKeys(std::string& sError) const;


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
                (*mi).second++;
        }
    }

    unsigned int GetKeyPoolSize()
    {
        AssertLockHeld(cs_wallet); // setKeyPool
        return setKeyPool.size();
    }

    bool GetTransaction(const uint256 &hashTx, CWalletTx& wtx);

    bool SetDefaultKey(const CPubKey &vchPubKey);

    // signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    // change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

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
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
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
        Init(NULL);
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
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        char fSpent = false;

        if (ser_action.ForRead()) {
            Init(NULL);
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

            nTimeSmart = mapValue.count("timesmart")
                ? (unsigned int)atoi64(mapValue["timesmart"])
                : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("version");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
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
                nCredit += pwallet->GetCredit(txout);
                if (!MoneyRange(nCredit))
                    throw std::runtime_error("CWalletTx::GetAvailableCredit() : value out of range");
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
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                me.strComment.append(ss.str());
            }
        }

        READWRITE(strComment);

        size_t nSepPos = strComment.find("\0", 0, 1);

        if (ser_action.ForRead()) {
            me.mapValue.clear();

            if (std::string::npos != nSepPos) {
                CDataStream ss(
                    std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()),
                    s.GetType(),
                    nVersion);

                ss >> me.mapValue;
                me._ssExtra = std::vector<char>(ss.begin(), ss.end());
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

MinedType GetGeneratedType(const CWallet *wallet, const uint256& tx, unsigned int vout);
#endif
