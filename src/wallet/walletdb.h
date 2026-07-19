// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETDB_H
#define BITCOIN_WALLET_WALLETDB_H

#include "wallet/db.h"
#include <key_io.h>

class CKeyPool;
class CAccount;
class CAccountingEntry;

/** Error statuses for the wallet database */
enum DBErrors
{
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_NONCRITICAL_ERROR,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
    DB_NEED_REWRITE
};

/* simple HD chain data model */
class CHDChain
{
public:
    uint32_t nExternalChainCounter;
    CKeyID masterKeyID; //!< master key hash160

    static const int VERSION_HD_BASE = 1;
    static const int CURRENT_VERSION = VERSION_HD_BASE;
    int nVersion;

    CHDChain() { SetNull(); }

    SERIALIZE_METHODS(CHDChain, obj)
    {
        READWRITE(obj.nVersion, obj.nExternalChainCounter, obj.masterKeyID);
    }

    void SetNull()
    {
        nVersion = CHDChain::CURRENT_VERSION;
        nExternalChainCounter = 0;
        masterKeyID.SetNull();
    }
};

/* Stored record for a wallet whose HD hierarchy is backed by a seed phrase
 * (GRC::Mnemonics). Holds the enciphered blob -- whose word encoding IS the
 * phrase, so re-display never needs the phrase password -- the phrase-derived
 * master key id (the anchor for coverage classification, independent of the
 * currently active hdchain), and the non-secret wallet birthday.
 *
 * Like private keys, the blob is written as a "seedphrase" record while the
 * wallet is unencrypted and as a "cseedphrase" record (blob encrypted under
 * the wallet's master keying material) once the wallet is encrypted:
 * otherwise a phrase protected by an empty or weak password would expose the
 * HD master seed to anyone holding an encrypted wallet.dat. */
class CSeedPhraseData
{
public:
    static const int CURRENT_VERSION = 1;
    int nVersion;
    //! Enciphered blob (GRC::Mnemonics::ENCIPHERED_LENGTH bytes), or its
    //! wallet-crypter ciphertext when loaded from a "cseedphrase" record.
    std::vector<unsigned char> vchBlob;
    //! Hash160 of the phrase-derived HD master public key.
    CKeyID masterKeyID;
    //! Wallet birthday recovered from the phrase (unix time, day resolution).
    int64_t nBirthday;

    CSeedPhraseData() { SetNull(); }

    SERIALIZE_METHODS(CSeedPhraseData, obj)
    {
        READWRITE(obj.nVersion, obj.vchBlob, obj.masterKeyID, obj.nBirthday);
    }

    void SetNull()
    {
        nVersion = CSeedPhraseData::CURRENT_VERSION;
        vchBlob.clear();
        masterKeyID.SetNull();
        nBirthday = 0;
    }

    bool IsNull() const { return vchBlob.empty(); }
};

class CKeyMetadata
{
public:
    static const int VERSION_BASIC=1;
    static const int VERSION_WITH_HDDATA=10;
    static const int CURRENT_VERSION=VERSION_WITH_HDDATA;
    int nVersion;
    int64_t nCreateTime; // 0 means unknown
    std::string hdKeypath; //optional HD/bip32 keypath
    CKeyID hdMasterKeyID; //id of the HD masterkey used to derive this key

    CKeyMetadata()
    {
        SetNull();
    }
    CKeyMetadata(int64_t nCreateTime_)
    {
        SetNull();
        nCreateTime = nCreateTime_;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(nCreateTime);
        if (this->nVersion >= VERSION_WITH_HDDATA)
        {
            READWRITE(hdKeypath);
            READWRITE(hdMasterKeyID);
        }
    }

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
        hdKeypath.clear();
        hdMasterKeyID.SetNull();
    }
};


/** Access to the wallet database (wallet.dat) */
class CWalletDB : public CDB
{
public:
    CWalletDB(const std::string& strFilename, const char* pszMode = "r+", bool flush_on_close = true)
        : CDB(strFilename, pszMode, flush_on_close)
    {
    }
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
public:
    bool WriteName(const std::string& strAddress, const std::string& strName);

    bool EraseName(const std::string& strAddress);

    bool WritePurpose(const std::string& strAddress, const std::string& strPurpose);

    bool ErasePurpose(const std::string& strAddress);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool WriteTx(uint256 hash, const CWalletTx& wtx)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("tx"), hash));
    }

    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta)
    {
        nWalletDBUpdated++;

        if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
            return false;

        return Write(std::make_pair(std::string("key"), std::vector<unsigned char>(vchPubKey.begin(), vchPubKey.end())), vchPrivKey, false);
    }

    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta)
    {
        nWalletDBUpdated++;
        bool fEraseUnencryptedKey = true;

        if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
            return false;

        if (!Write(std::make_pair(std::string("ckey"), std::vector<unsigned char>(vchPubKey.begin(), vchPubKey.end())), vchCryptedSecret, false))
            return false;
        if (fEraseUnencryptedKey)
        {
            Erase(std::make_pair(std::string("key"), std::vector<unsigned char>(vchPubKey.begin(), vchPubKey.end())));
            Erase(std::make_pair(std::string("wkey"), std::vector<unsigned char>(vchPubKey.begin(), vchPubKey.end())));
        }
        return true;
    }

    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
    }

    bool WriteCScript(const uint160& hash, const CScript& redeemScript)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("cscript"), hash), redeemScript, false);
    }

    bool WriteOrderPosNext(int64_t nOrderPosNext)
    {
        nWalletDBUpdated++;
        return Write(std::string("orderposnext"), nOrderPosNext);
    }

    bool WriteDefaultKey(const CPubKey& vchPubKey)
    {
        nWalletDBUpdated++;
        return Write(std::string("defaultkey"), std::vector<unsigned char>(vchPubKey.begin(), vchPubKey.end()));
    }

    bool ReadPool(int64_t nPool, CKeyPool& keypool)
    {
        return Read(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool WritePool(int64_t nPool, const CKeyPool& keypool)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool ErasePool(int64_t nPool)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("pool"), nPool));
    }

    bool WriteMinVersion(int nVersion)
    {
        return Write(std::string("minversion"), nVersion);
    }

    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);

    bool ReadBackupTime(int64_t& out_backup_time)
    {
        return Read(std::string("backuptime"), out_backup_time);
    }

    bool WriteBackupTime(const int64_t backup_time)
    {
        nWalletDBUpdated++;
        return Write(std::string("backuptime"), backup_time);
    }

    template<typename T>
    bool WriteAttribute(const std::string& attribute, const T& value)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("attribute"), attribute), value);
    }

    template<typename T>
    bool ReadAttribute(const std::string& attribute, T& value)
    {
        return Read(std::make_pair(std::string("attribute"), attribute), value);
    }
private:
    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
public:
    bool WriteAccountingEntry(const CAccountingEntry& acentry);
    int64_t GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors ReorderTransactions(CWallet*);
	DBErrors FindWalletTx(CWallet* pwallet, std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx);

	DBErrors ZapWalletTx(CWallet* pwallet, std::vector<CWalletTx>& vWtx);

    DBErrors LoadWallet(CWallet* pwallet);
    static bool Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys);
    static bool Recover(CDBEnv& dbenv, std::string filename);

    //! write the hdchain model (external chain child index counter)
    bool WriteHDChain(const CHDChain& chain);

    //! write the seed phrase record with a plaintext blob (unencrypted wallet)
    bool WriteSeedPhrase(const CSeedPhraseData& data);

    //! write the seed phrase record with a wallet-crypter-encrypted blob;
    //! erases any plaintext "seedphrase" record
    bool WriteCryptedSeedPhrase(const CSeedPhraseData& data);
};

#endif // BITCOIN_WALLET_WALLETDB_H
