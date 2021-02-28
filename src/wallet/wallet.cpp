// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "crypter.h"
#include "ui_interface.h"
#include "base58.h"
#include "wallet/coincontrol.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
#include "rpc/server.h"
#include "rpc/client.h"
#include "rpc/protocol.h"
#include <boost/variant/apply_visitor.hpp>
#include <script.h>
#include "main.h"
#include "util.h"
#include <random>
#include "gridcoin/staking/kernel.h"
#include "gridcoin/support/block_finder.h"

using namespace std;

extern bool fQtActive;
extern MilliTimer g_timer;

bool fConfChange;
unsigned int nDerivationMethodIndex;
extern std::atomic<int64_t> g_nTimeBestReceived;

namespace {
struct CompareValueOnly
{
    bool operator()(const pair<int64_t, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<int64_t, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};
} // anonymous namespace

// -----------------------------------------------------------------------------
// Class: CWallet
// -----------------------------------------------------------------------------

const CPubKey& CWallet::MasterPublicKey()
{
    // If the master key changes, add a conditional entry to this method that
    // returns the new key for the appropriate height.

    // 049ac003b3318d9fe28b2830f6a95a2624ce2a69fb0c0c7ac0b513efcc1e93a6a
    // 6e8eba84481155dd82f2f1104e0ff62c69d662b0094639b7106abc5d84f948c0a
    static const CPubKey since_block_0({
        0x04, 0x9a, 0xc0, 0x03, 0xb3, 0x31, 0x8d, 0x9f, 0xe2, 0x8b, 0x28,
        0x30, 0xf6, 0xa9, 0x5a, 0x26, 0x24, 0xce, 0x2a, 0x69, 0xfb, 0x0c,
        0x0c, 0x7a, 0xc0, 0xb5, 0x13, 0xef, 0xcc, 0x1e, 0x93, 0xa6, 0xa6,
        0xe8, 0xeb, 0xa8, 0x44, 0x81, 0x15, 0x5d, 0xd8, 0x2f, 0x2f, 0x11,
        0x04, 0xe0, 0xff, 0x62, 0xc6, 0x9d, 0x66, 0x2b, 0x00, 0x94, 0x63,
        0x9b, 0x71, 0x06, 0xab, 0xc5, 0xd8, 0x4f, 0x94, 0x8c, 0x0a
    });

    return since_block_0;
}

const CBitcoinAddress CWallet::MasterAddress()
{
    CBitcoinAddress master_address;
    master_address.Set(MasterPublicKey().GetID());

    return master_address;
}

CKey CWallet::MasterPrivateKey() const
{
    CKey key_out;

    GetKey(MasterPublicKey().GetID(), key_out);

    return key_out;
}

CPubKey CWallet::GenerateNewKey()
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = key.GetPubKey();

    // Create new metadata
    int64_t nCreationTime =  GetAdjustedTime();
    mapKeyMetadata[pubkey.GetID()] = CKeyMetadata(nCreationTime);
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    if (!AddKey(key))
        throw std::runtime_error("CWallet::GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool CWallet::AddKey(const CKey& key)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata

    CPubKey pubkey = key.GetPubKey();

    if (!CCryptoKeyStore::AddKey(key))
        return false;
    if (!fFileBacked)
        return true;
    if (!IsCrypted())
        return CWalletDB(strWalletFile).WriteKey(pubkey, key.GetPrivKey(), mapKeyMetadata[pubkey.GetID()]);
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey, const vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}

bool CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

// optional setting to unlock wallet for staking only
// serves to disable the trivial sendmoney when OS account compromised
// provides no real security
bool fWalletUnlockStakingOnly = false;

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = CBitcoinAddress(redeemScript.GetID()).ToString();
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %" PRIszu " which exceeds maximum size %i thus can never be redeemed. Do not use address %s.",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    if (!IsLocked())
        return false;

    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        for (auto const& pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
            {
                return true;
            }
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        for (auto &pMasterKey : mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteBestBlock(loc);
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;
    RandAddSeedPerfmon();

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    RAND_bytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey(nDerivationMethodIndex);

    RandAddSeedPerfmon();
    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    RAND_bytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin())
                return false;
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked)
                pwalletdbEncryption->TxnAbort();
            exit(1); //We now probably have half of our keys encrypted in memory, and half not...die and let the user reload their unencrypted wallet.
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit())
                exit(1); //We now have keys encrypted in memory, but no on disk...die to avoid confusion and let the user reload their unencrypted wallet.

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);

    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

CWallet::TxItems CWallet::OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount)
{
    AssertLockHeld(cs_wallet); // mapWallet
    CWalletDB walletdb(strWalletFile);

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-order multimap.
    TxItems txOrdered;

    // Note: maintaining indices in the database of (account,time) --> txid and (account, time) --> acentry
    // would make this much faster for applications that do this a lot.
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txOrdered.insert(make_pair(wtx->nOrderPos, TxPair(wtx, (CAccountingEntry*)0)));
    }
    acentries.clear();
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    for (auto &entry : acentries)
    {
        txOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));
    }

    return txOrdered;
}

void CWallet::WalletUpdateSpent(const CTransaction &tx, bool fBlock, CWalletDB* pwalletdb)
{
    // Anytime a signature is successfully verified, it's proof the outpoint is spent.
    // Update the wallet spent flag if it doesn't know due to wallet.dat being
    // restored from backup or the user making copies of wallet.dat.
    {
        LOCK(cs_wallet);
        for (auto const& txin : tx.vin)
        {
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                CWalletTx& wtx = (*mi).second;
                if (txin.prevout.n >= wtx.vout.size())
                    LogPrintf("WalletUpdateSpent: bad wtx %s", wtx.GetHash().ToString());
                else if (!wtx.IsSpent(txin.prevout.n) && (IsMine(wtx.vout[txin.prevout.n]) != ISMINE_NO))
                {
                    LogPrint(BCLog::LogFlags::VERBOSE, "WalletUpdateSpent found spent coin %s gC %s", FormatMoney(wtx.GetCredit()), wtx.GetHash().ToString());
                    wtx.MarkSpent(txin.prevout.n);
                    wtx.WriteToDisk(pwalletdb);
                    NotifyTransactionChanged(this, txin.prevout.hash, CT_UPDATED);
                }
            }
        }

        if (fBlock)
        {
            uint256 hash = tx.GetHash();
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(hash);
            CWalletTx& wtx = (*mi).second;

            for (auto const& txout : tx.vout)
            {
                if (IsMine(txout) != ISMINE_NO)
                {
                    wtx.MarkUnspent(&txout - &tx.vout[0]);
                    wtx.WriteToDisk(pwalletdb);
                    NotifyTransactionChanged(this, hash, CT_UPDATED);
                }
            }
        }

    }
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        for (auto &item : mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, CWalletDB* pwalletdb)
{
    uint256 hash = wtxIn.GetHash();
    {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
        {
            wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext(pwalletdb);

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (!wtxIn.hashBlock.IsNull())
            {
                auto mapItem = mapBlockIndex.find(wtxIn.hashBlock);
                if (mapItem != mapBlockIndex.end())
                {
                    unsigned int latestNow = wtx.nTimeReceived;
                    unsigned int latestEntry = 0;
                    {
                        // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64_t latestTolerated = latestNow + 300;
                        std::list<CAccountingEntry> acentries;
                        TxItems txOrdered = OrderedTxItems(acentries);
                        for (TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
                        {
                            CWalletTx *const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            CAccountingEntry *const pacentry = (*it).second.second;
                            int64_t nSmartTime;
                            if (pwtx)
                            {
                                nSmartTime = pwtx->nTimeSmart;
                                if (!nSmartTime)
                                    nSmartTime = pwtx->nTimeReceived;
                            }
                            else
                                nSmartTime = pacentry->nTime;
                            if (nSmartTime <= latestTolerated)
                            {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }

                    unsigned int& blocktime = mapItem->second->nTime;
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                }
                else
                {
                    LogPrint(BCLog::LogFlags::VERBOSE, "AddToWallet() : found %s in block %s not in index",
                           wtxIn.GetHash().ToString().substr(0,10),
                           wtxIn.hashBlock.ToString());
                }
            }
        }

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (!wtxIn.hashBlock.IsNull() && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
            fUpdated |= wtx.UpdateSpent(wtxIn.vfSpent);
        }

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk(pwalletdb))
                return false;
        if(!fQtActive)
        {
            // If default receiving address gets used, replace it with a new one
            if (vchDefaultKey.IsValid()) {
                CScript scriptDefaultKey;
                scriptDefaultKey.SetDestination(vchDefaultKey.GetID());
                for (auto const& txout : wtx.vout)
                {
                    if (txout.scriptPubKey == scriptDefaultKey)
                    {
                        CPubKey newDefaultKey;
                        if (GetKeyFromPool(newDefaultKey, false))
                        {
                            SetDefaultKey(newDefaultKey);
                            SetAddressBookName(vchDefaultKey.GetID(), "");
                        }
                    }
                }
            }
        }
        // since AddToWallet is called directly for self-originating transactions, check for consumption of own coins
        WalletUpdateSpent(wtx, (!wtxIn.hashBlock.IsNull()), pwalletdb);

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");
        if (!strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }

    }
    return true;
}

// Add a transaction to the wallet, or update it.
// pblock is optional, but should be provided if the transaction is known to be in a block.
// If fUpdate is true, existing transactions will be updated.
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fFindBlock)
{
    uint256 hash = tx.GetHash();
    {
        LOCK(cs_wallet);
        bool fExisted = mapWallet.count(hash);
        if (fExisted && !fUpdate) return false;

        // Do not flush the wallet here for performance reasons
        // this is safe, as in case of a crash, we rescan the necessary blocks on startup.
        CWalletDB walletdb(strWalletFile, "r+", false);

        if (fExisted || (IsMine(tx) != ISMINE_NO) || IsFromMe(tx))
        {
            CWalletTx wtx(this,tx);
            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(pblock);

            return AddToWallet(wtx, &walletdb);
        }
        else
            WalletUpdateSpent(tx, false, &walletdb);
    }
    return false;
}

bool CWallet::EraseFromWallet(uint256 hash)
{
    if (!fFileBacked)
        return false;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
    }
    return true;
}


isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return IsMine(prev.vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

int64_t CWallet::GetDebit(const CTxIn &txin,const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                 if (IsMine(prev.vout[txin.prevout.n]) & filter)
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    CTxDestination address;

    // TODO : fix handling of 'change' outputs. The assumption is that any
    // payment to a TX_PUBKEYHASH that is mine but isn't in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (ExtractDestination(txout.scriptPubKey, address) && (::IsMine(*this, address) != ISMINE_NO))
    {
        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase() || IsCoinStake())
        {
            // Generated block
            if (!hashBlock.IsNull())
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && !hashBlock.IsNull())
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}


CTxDestination GetCoinstakeDestination(const CWalletTx* wtx,CTxDB& txdb)
{
   // For Coinstakes, extract the address from the input
   for (auto const& txin : wtx->vin)
   {
            COutPoint prevout = txin.prevout;
            CTransaction prev;
            if(txdb.ReadDiskTx(prevout.hash, prev))
            {
                if (prevout.n < prev.vout.size())
                {
                    //Inputs:
                    const CTxOut &vout = prev.vout[prevout.n];
                    CTxDestination address;
                    if (ExtractDestination(vout.scriptPubKey, address))
                    {
                        return address;
                    }
                }
            }
    }
    return CNoDestination();
}


void CWalletTx::GetAmounts(list<COutputEntry>& listReceived, list<COutputEntry>& listSent,
                           int64_t& nFee, string& strSentAccount,
                           const isminefilter& filter) const
{
    nFee = 0;

    listReceived.clear();
    listSent.clear();

    strSentAccount = strFromAccount;

    // This is the same as nDebit > 0, i.e. we sent the transaction.
    bool fIsFromMe = IsFromMe();

    // This will be true if this is a self-transaction.
    bool fIsAllToMe = true;
    for (auto const& txout : vout)
    {
        fIsAllToMe = fIsAllToMe && (pwallet->IsMine(txout) != ISMINE_NO);

        // Once false, no point in continuing.
        if (!fIsAllToMe) break;
    }

    // Used for coinstake rollup.
    int64_t amount = 0;

    bool fIsCoinStake = IsCoinStake();

    // The first output of the coinstake has the same owner as the input.
    bool fIsCoinStakeMine = (fIsCoinStake && pwallet->IsMine(vout[1]) != ISMINE_NO) ? true : false;

    // Compute fee:
    int64_t nDebit = GetDebit(filter);
    // fIsFromMe true means we signed/sent this transaction, we do not record a fee for
    // coinstakes. The fees collected from other transactions in the block are added
    // to the staker's output(s) that are the staker's. Therefore fees only need
    // to be shown for non-coinstake send transactions.
    if (fIsFromMe && !fIsCoinStake)
    {
        int64_t nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.
    for (unsigned int i = 0; i < vout.size(); ++i)
    {
        const CTxOut& txout = vout[i];
        isminetype fIsMine = pwallet->IsMine(txout);
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (fIsFromMe)
        {
            // If not a coinstake, don't report 'change' txouts. Txouts on change addresses for coinstakes
            // must be reported because a change address itself can stake, and there is no "change" on a
            // coinstake.
            if (!fIsCoinStake && pwallet->IsChange(txout)) continue;
        }
        else
        {
            if (fIsMine == ISMINE_NO) continue;
        }

        CTxDestination address;
        COutputEntry output;

        // Send...

        // If the output is not mine and ((output > 1 and a coinstake and the coinstake input, i.e. output 1, is mine)
        // OR (not a coinstake and nDebit > 0, i.e. a normal send transaction)), add the output as a "sent" entry.
        // We exclude coinstake outputs 0 and 1 from sends, because output 0 is empty and output 1 MUST go back to
        // the staker (i.e. is not a send by definition). Notice that for a normal self-transaction, the send and
        // receive details will be suppressed in this block. There is a separate section to deal with self-transactions
        // below.
        if (fIsMine == ISMINE_NO && ((i > 1 && fIsCoinStakeMine) || (!fIsCoinStake && fIsFromMe)))
        {
            if (!ExtractDestination(txout.scriptPubKey, address))
            {
                if (txout.scriptPubKey[0] != OP_RETURN)
                {
                    LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s",
                              this->GetHash().ToString().c_str());
                }

                address = CNoDestination();
            }

            output = {address, txout.nValue, (int) i};
            listSent.push_back(output);
        }

        // Receive...

        // This first section is for rolling up the entire coinstake into one entry.
        // If a coinstake and the coinstake is mine, add all of the outputs and treat as
        // a received entry, regardless of whether they are mine or not, because sidestakes
        // to addresses not mine will be treated separately.
        if (fIsCoinStakeMine)
        {
            // You can't simply use nCredit here, because we specifically are counting ALL outputs,
            // regardless of whether they are mine or not. This is because instead of doing the coinstake
            // as a single "net" entry, we show the whole coinstake AS IF the entire coinstake were back
            // to the staker, and then create separate "send" entries for the sidestakes out to another
            // address that is not mine.
            amount += txout.nValue;

            // If we are on the last output of the coinstake, then push the net amount.
            if (i == vout.size() - 1)
            {
                // We want the destination for the overall coinstake to come from output one,
                // which also matches the input.
                ExtractDestination(vout[1].scriptPubKey, address);

                // For the rolled up coinstake entry, the first output is indicated in the pushed output
                output = {address, amount - nDebit, 1};
                listReceived.push_back(output);
            }
        }

        // If this is my output AND the transaction is not from me, then record the output as received.
        if (fIsMine != ISMINE_NO && !fIsFromMe)
        {
            if (!ExtractDestination(txout.scriptPubKey, address) && txout.scriptPubKey[0] != OP_RETURN)
            {
                LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s",
                          this->GetHash().ToString().c_str());
                address = CNoDestination();
            }

            output = {address, txout.nValue, (int) i};
            listReceived.push_back(output);
        }

        // Self-transactions...

        if (fIsFromMe && fIsAllToMe)
        {
            if (!ExtractDestination(txout.scriptPubKey, address))
            {
                if (txout.scriptPubKey[0] != OP_RETURN)
                {
                    LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s",
                              this->GetHash().ToString().c_str());
                }

                address = CNoDestination();
            }

            // For a self-transaction, the output has to be both a send and a receive. Note that an
            // unfortunate side-effect of this solution for self-transaction listing is that the fee
            // will be reported on both the send and receive transactions in the ListTransactions that
            // normally calls this function, but that is better than simply reporting the receive side only
            // of a self-transaction, which is typically what is done.
            //
            // Also, a mixed transaction where some of the outputs are back to oneself, and others are to
            // other addressees, does not qualify here. Those only the output sends will be reported.
            output = {address, txout.nValue, (int) i};
            listSent.push_back(output);
            listReceived.push_back(output);
        }
    }
}

void CWalletTx::GetAccountAmounts(const string& strAccount, int64_t& nReceived,
                                  int64_t& nSent, int64_t& nFee, const isminefilter& filter) const
{
    nReceived = nSent = nFee = 0;

    int64_t allFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;
    GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
    if (strAccount == strSentAccount)
    {
        for (auto const& s : listSent)
            nSent += s.amount;
        nFee = allFee;
    }
    {
        LOCK(pwallet->cs_wallet);
        for (auto const& r : listReceived)
        {
            if (pwallet->mapAddressBook.count(r.destination))
            {
                map<CTxDestination, string>::const_iterator mi = pwallet->mapAddressBook.find(r.destination);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second == strAccount)
                    nReceived += r.amount;
            }
            else if (strAccount.empty())
            {
                nReceived += r.amount;
            }
        }
    }
}

void CWalletTx::AddSupportingTransactions(CTxDB& txdb)
{
    vtxPrev.clear();

    const int COPY_DEPTH = 3;
    if (SetMerkleBranch() < COPY_DEPTH)
    {
        vector<uint256> vWorkQueue;
        for (auto const& txin : vin)
            vWorkQueue.push_back(txin.prevout.hash);

        // This critsect is OK because txdb is already open
        {
            LOCK(pwallet->cs_wallet);
            map<uint256, const CMerkleTx*> mapWalletPrev;
            set<uint256> setAlreadyDone;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hash = vWorkQueue[i];
                if (setAlreadyDone.count(hash))
                    continue;
                setAlreadyDone.insert(hash);

                CMerkleTx tx;
                map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(hash);
                if (mi != pwallet->mapWallet.end())
                {
                    tx = (*mi).second;
                    for (auto const& txWalletPrev : (*mi).second.vtxPrev)
                        mapWalletPrev[txWalletPrev.GetHash()] = &txWalletPrev;
                }
                else if (mapWalletPrev.count(hash))
                {
                    tx = *mapWalletPrev[hash];
                }
                else if (txdb.ReadDiskTx(hash, tx))
                {
                    ;
                }
                else
                {
                    LogPrintf("ERROR: AddSupportingTransactions() : unsupported transaction");
                    continue;
                }

                int nDepth = tx.SetMerkleBranch();
                vtxPrev.push_back(tx);

                if (nDepth < COPY_DEPTH)
                {
                    for (auto const& txin : tx.vin)
                        vWorkQueue.push_back(txin.prevout.hash);
                }
            }
        }
    }

    reverse(vtxPrev.begin(), vtxPrev.end());
}

bool CWalletTx::WriteToDisk(CWalletDB *pwalletdb)
{
    return pwalletdb->WriteTx(GetHash(), *this);
}

// Scan the block chain (starting in pindexStart) for transactions
// from or to us. If fUpdate is true, found transactions that already
// exist in the wallet will be updated.
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int ret = 0;

    CBlockIndex* pindex = pindexStart;
    {
        LOCK2(cs_main, cs_wallet);
        while (pindex)
        {
            // no need to read and scan block, if block was created before
            // our wallet birthday (as adjusted for block time variability)
            if (nTimeFirstKey && (pindex->nTime < (nTimeFirstKey - 7200))) {
                pindex = pindex->pnext;
                continue;
            }

            CBlock block;
            block.ReadFromDisk(pindex, true);
            for (auto const& tx : block.vtx)
            {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                    ret++;
            }
            pindex = pindex->pnext;
        }
    }
    return ret;
}

void CWallet::ReacceptWalletTransactions()
{
    CTxDB txdb("r");
    bool fRepeat = true;
    while (fRepeat)
    {
        LOCK2(cs_main, cs_wallet);
        fRepeat = false;
        vector<CDiskTxPos> vMissingTx;
        for (auto &item : mapWallet)
        {
            CWalletTx& wtx = item.second;
            if ((wtx.IsCoinBase() && wtx.IsSpent(0)) || (wtx.IsCoinStake() && wtx.IsSpent(1)))
                continue;

            CTxIndex txindex;
            bool fUpdated = false;
            if (txdb.ReadTxIndex(wtx.GetHash(), txindex))
            {
                // Update fSpent if a tx got spent somewhere else by a copy of wallet.dat
                if (txindex.vSpent.size() != wtx.vout.size())
                {
                    LogPrintf("ERROR: ReacceptWalletTransactions() : txindex.vSpent.size() %" PRIszu " != wtx.vout.size() %" PRIszu, txindex.vSpent.size(), wtx.vout.size());
                    continue;
                }
                for (unsigned int i = 0; i < txindex.vSpent.size(); i++)
                {
                    if (wtx.IsSpent(i))
                        continue;
                    if (!txindex.vSpent[i].IsNull() && (IsMine(wtx.vout[i]) != ISMINE_NO))
                    {
                        wtx.MarkSpent(i);
                        fUpdated = true;
                        vMissingTx.push_back(txindex.vSpent[i]);
                    }
                }
                if (fUpdated)
                {
                    LogPrintf("ReacceptWalletTransactions found spent coin %s gC %s", FormatMoney(wtx.GetCredit()), wtx.GetHash().ToString());
                    wtx.MarkDirty();

                    CWalletDB walletdb(strWalletFile);

                    wtx.WriteToDisk(&walletdb);
                }
            }
            else
            {
                // Re-accept any txes of ours that aren't already in a block
                if (!(wtx.IsCoinBase() || wtx.IsCoinStake()))
                    wtx.AcceptWalletTransaction(txdb);
            }
        }
        if (!vMissingTx.empty())
        {
            // TODO : optimize this to scan just part of the block chain?
            if (ScanForWalletTransactions(pindexGenesisBlock))
                fRepeat = true;  // Found missing transactions: re-do re-accept.
        }
    }
}


void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    // Nodes erase version 1 transactions from the mempool at the
    // block version 11 threshold to prepare for version 2. If we
    // still have unconfirmed version 1 transactions removed from
    // the pool when the transition occurred, we can't switch the
    // format to version 2 because we need to re-sign these which
    // may change the properties of the transaction in a way that
    // requires the consent of the user. Log a message instead so
    // that the user can take action if needed:
    //
    if (nVersion == 1)
    {
        if (IsCoinBase() || IsCoinStake())
        {
            return;
        }

        const uint256 hash = GetHash();

        if (!txdb.ContainsTx(hash))
        {
            LogPrintf(
                "WARNING: %s: unable to resend legacy version 1 tx %s",
                __func__,
                hash.ToString());
        }

        return;
    }

    for (auto const& tx : vtxPrev)
    {
        if (!(tx.IsCoinBase() || tx.IsCoinStake()))
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
                RelayTransaction((CTransaction)tx, hash);
        }
    }

    if (!(IsCoinBase() || IsCoinStake()))
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            LogPrint(BCLog::LogFlags::NOISY, "Relaying wtx %s", hash.ToString().substr(0,10));
            RelayTransaction((CTransaction)*this, hash);
        }
    }
}

void CWalletTx::RelayWalletTransaction()
{
   CTxDB txdb("r");
   RelayWalletTransaction(txdb);
}

void CWallet::ResendWalletTransactions(bool fForce)
{
    if (!fForce)
    {
        // Do this infrequently and randomly to avoid giving away
        // that these are our transactions.
        static int64_t nNextTime;
        if ( GetAdjustedTime() < nNextTime)
            return;
        bool fFirst = (nNextTime == 0);
        nNextTime =  GetAdjustedTime() + GetRand(30 * 60);
        if (fFirst)
            return;

        // Only do it if there's been a new block since last time
        static int64_t nLastTime;
        if (g_nTimeBestReceived < nLastTime)
            return;
        nLastTime =  GetAdjustedTime();
    }

    // Rebroadcast any of our txes that aren't in a block yet
    CTxDB txdb("r");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        for (auto &item : mapWallet)
        {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (fForce || g_nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60)
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }
        for (auto const &item : mapSorted)
        {
            CWalletTx& wtx = *item.second;
            if (wtx.CheckTransaction()) {
                wtx.RelayWalletTransaction(txdb);
            } else {
                LogPrintf("ResendWalletTransactions() : CheckTransaction failed for transaction %s", wtx.GetHash().ToString());
            }
        }
    }
}






//////////////////////////////////////////////////////////////////////////////
//
// Actions
//


int64_t CWallet::GetBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted() && (pcoin->IsConfirmed() || pcoin->fFromMe))
                nTotal += pcoin->GetAvailableCredit();
        }
    }

    return nTotal;
}

int64_t CWallet::GetUnconfirmedBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!IsFinalTx(*pcoin) || (!pcoin->IsConfirmed() && !pcoin->fFromMe && pcoin->IsInMainChain()))
                nTotal += pcoin->GetAvailableCredit();
        }
    }
    return nTotal;
}

int64_t CWallet::GetImmatureBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& pcoin = (*it).second;
            if (pcoin.IsCoinBase() && pcoin.GetBlocksToMaturity() > 0 && pcoin.IsInMainChain())
                nTotal += GetCredit(pcoin);
        }
    }
    return nTotal;
}

// populate vCoins with vector of spendable COutputs
void CWallet::AvailableCoins(vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl *coinControl, bool fIncludeStakedCoins) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
			int nDepth = pcoin->GetDepthInMainChain();

			if (!fIncludeStakedCoins)
			{
				if (!IsFinalTx(*pcoin))
					continue;

				if (fOnlyConfirmed && !pcoin->IsTrusted())
					continue;

				if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
					continue;

				if(pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0)
					continue;

				if (nDepth < 0)
					continue;
			}
			else
			{
				if (nDepth < 1) continue;
			}

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
			{
                if ((!(pcoin->IsSpent(i)) && (IsMine(pcoin->vout[i]) != ISMINE_NO) && pcoin->vout[i].nValue >= nMinimumInputValue &&
                   (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
	     	 	   || (fIncludeStakedCoins && pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0))
				   {
				        vCoins.push_back(COutput(pcoin, i, nDepth));
				   }
			}

        }
    }
}

// A lock must be taken on cs_main before calling this function.
void CWallet::AvailableCoinsForStaking(vector<COutput>& vCoins, unsigned int nSpendTime, int64_t& balance_out) const
{

    vCoins.clear();
    {
        AssertLockHeld(cs_main);
        LOCK(cs_wallet);

        std::string function = __func__;
        function += ": ";

        unsigned int transactions = 0;
        unsigned int txns_w_avail_outputs = 0;

        for (const auto& it : mapWallet)
        {
            const CWalletTx* pcoin = &it.second;

            // Track number of transactions processed for instrumentation purposes.
            ++transactions;

            int nDepth = pcoin->GetDepthInMainChain();
            std::vector<std::pair<const CWalletTx*, int>> possible_vCoins;

            // Do the balance computation here after the GetDepthInMainChain() call.
            // This avoids the expensive IsTrusted() and IsConfirmed() calls in the GetBalance() function, which each
            // have a call to GetDepthInMainChain(). We also want to use a slightly different standard for the balance
            // calculation here, to include recently staked amounts. The number here should be equal or very close to
            // the "Total" field on the GUI overview screen. This is the proper number to use to be able to do the
            // efficiency calculations.
            if (nDepth > 0 || (pcoin->fFromMe && (pcoin->AreDependenciesConfirmed() || pcoin->IsCoinStake())))
            {
                for (unsigned int i = 0; i < pcoin->vout.size(); ++i)
                {
                    if (!(pcoin->IsSpent(i))
                            && (IsMine(pcoin->vout[i]) != ISMINE_NO)
                            && pcoin->vout[i].nValue > 0)
                    {
                        balance_out += pcoin->vout[i].nValue;
                        possible_vCoins.push_back(std::make_pair(pcoin, i));
                    }
                }
            }

            // If there are no possible (pre-qualified) outputs, continue, so we avoid the expensive GetDepthInMainChain()
            // call.
            if (possible_vCoins.empty()) continue;

            // Filtering by tx timestamp instead of block timestamp may give false positives but never false negatives
            if (pcoin->nTime + nStakeMinAge > nSpendTime) continue;

            // We avoid GetBlocksToMaturity(), because that also calls GetDepthInMainChain(), so the older code,
            // to get nDepth, still had to call GetDepthInMainChain(), so that meant it was called twice for EVERY
            // every transaction in the wallet. Wasteful.
            int blocks_to_maturity = 0;

            // If coinbase or coinstake, blocks_to_maturity must be 0. (This means a minimum depth of
            // nCoinbaseMaturity + 10.
            if (pcoin->IsCoinBase() || pcoin->IsCoinStake())
            {
                blocks_to_maturity = std::max(0, (nCoinbaseMaturity + 10) - nDepth);

                if (blocks_to_maturity > 0) continue;
            }
            // If regular transaction, then must be at depth of 1 or more.
            else
            {
                if (nDepth < 1) continue;
            }

            bool available_output = false;

            for (const auto& iter : possible_vCoins)
            {
                // We need to respect the nMinimumInputValue parameter and include only those outputs that pass.
                if (iter.first->vout[iter.second].nValue >= nMinimumInputValue)
                {
                    vCoins.push_back(COutput(iter.first, iter.second, nDepth));
                    available_output = true;
                }
            }

            // If the transaction has one or more available outputs that have passed the requirements,
            // increment the counter.
            if (available_output) ++txns_w_avail_outputs;
        }

        g_timer.GetElapsedTime(function
                               + "transactions = "
                               + std::to_string(transactions)
                               + ", txns_w_avail_outputs = "
                               + std::to_string(txns_w_avail_outputs)
                               + ", balance = "
                               + std::to_string(balance_out)
                               , "miner");
    }
}

static void ApproximateBestSubset(vector<pair<int64_t, pair<const CWalletTx*,unsigned int> > >vValue, int64_t nTotalLower, int64_t nTargetValue,
                                  vector<char>& vfBest, int64_t& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    seed_insecure_rand();

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        int64_t nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                //The solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng fast. We do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (nPass == 0 ? insecure_rand()&1 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

// ppcoin: total coins staked (non-spendable until maturity)
int64_t CWallet::GetStake() const
{
    int64_t nTotal = 0;
    LOCK2(cs_main, cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

int64_t CWallet::GetNewMint() const
{
    int64_t nTotal = 0;
    LOCK2(cs_main, cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

// This comparator is needed since std::sort alone cannot sort COutput
struct smallestcoincomp
{
    bool operator() (const COutput a, const COutput b)
    {
        const CWalletTx* acoin = a.tx;
        const CWalletTx* bcoin = b.tx;

        return (acoin->vout[a.i].nValue < bcoin->vout[b.i].nValue);
    }
};

bool CWallet::SelectCoinsMinConf(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, vector<COutput> vCoins, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<int64_t, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<int64_t>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<int64_t, pair<const CWalletTx*,unsigned int> > > vValue;
    int64_t nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    for (auto output : vCoins)
    {
        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe() ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;

        // Follow the timestamp rules
        if (pcoin->nTime > nSpendTime)
            continue;

        int64_t n = pcoin->vout[i].nValue;

        pair<int64_t,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n == nTargetValue)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        }
        else if (n < nTargetValue + CENT)
        {
            vValue.push_back(coin);
            nTotalLower += n;
        }
        else if (n < coinLowestLarger.first)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    vector<char> vfBest;
    int64_t nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + CENT)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + CENT, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + CENT) || coinLowestLarger.first <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && GetBoolArg("-printpriority"))
        {
            //// debug print
            LogPrintf("SelectCoins() best subset: ");
            for (unsigned int i = 0; i < vValue.size(); i++)
                if (vfBest[i])
                    LogPrintf("%s ", FormatMoney(vValue[i].first));
            LogPrintf("total %s", FormatMoney(nBest));
        }
    }

    return true;
}

bool CWallet::SelectSmallestCoins(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, vector<COutput> vCoins, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    sort(vCoins.begin(), vCoins.end(), smallestcoincomp());

    for (auto output : vCoins) {
        const CWalletTx* const pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe() ? nConfMine : nConfTheirs)) {
            continue;
        }

        // Follow the timestamp rules
        if (pcoin->nTime > nSpendTime) {
            continue;
        }

        setCoinsRet.emplace(pcoin, output.i);
        nValueRet += pcoin->vout[output.i].nValue;

        if (nValueRet >= nTargetValue) {
            return true;
        }
    }

    return false;
}

bool CWallet::SelectCoins(int64_t nTargetValue, unsigned int nSpendTime, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet, const CCoinControl* coinControl, bool contract) const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl, false);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected())
    {
        for (auto const& out : vCoins)
        {
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    if (contract) {
        LogPrint(BCLog::LogFlags::ESTIMATEFEE, "INFO %s: Contract is included so SelectSmallestCoins will be used.", __func__);

        return (SelectSmallestCoins(nTargetValue, nSpendTime, 1, 10, vCoins, setCoinsRet, nValueRet) ||
                SelectSmallestCoins(nTargetValue, nSpendTime, 1, 1, vCoins, setCoinsRet, nValueRet)  ||
                SelectSmallestCoins(nTargetValue, nSpendTime, 0, 1, vCoins, setCoinsRet, nValueRet));
    }

    return (SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 10, vCoins, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 1, 1, vCoins, setCoinsRet, nValueRet)  ||
            SelectCoinsMinConf(nTargetValue, nSpendTime, 0, 1, vCoins, setCoinsRet, nValueRet));
}

/* Select coins from wallet for staking
//
// All wallet based information to be checked here and sent to miner as requested by this function
// 1) Check if we have a balance
// 2) Check if we have a balance after the reserve is applied to consider staking with
// 3) Check if we have coins eligible to stake
// 4) Iterate through the wallet of stakable utxos and return them to miner if we can stake with them
//
// Formula Stakable = ((SPENDABLE - RESERVED) > UTXO)
*/
bool CWallet::SelectCoinsForStaking(unsigned int nSpendTime, std::vector<pair<const CWalletTx*,unsigned int> >& vCoinsRet,
                                    GRC::MinerStatus::ReasonNotStakingCategory& not_staking_error,
                                    int64_t& balance_out,
                                    bool fMiner) const
{
    std::string function = __func__;
    function += ": ";

    vector<COutput> vCoins;

    // The balance is now calculated INSIDE of AvailableCoinsForStaking while iterating through wallet map
    // and reported back out to maintain compatibility with overall MinerStatus fields, which all are retained
    // but some really not necessary, and also provide the miner with the balance for staking efficiency calculations.
    // It may seem odd to reverse the flow from the original code, but the original code called the GetBalance()
    // function under the impression that call was cheap. It is not. It iterates through the entire wallet map to
    // compute the balance to do the cutoff at the balance level here. Old wallets can have 100000 transactions or more,
    // most of which are spent. For example, a testnet wallet used as a sidestaking target had 210000 map entries.
    // If the cutoff at the balance level passes then the old code went to AvailableCoinsForStaking, where we went
    // through the map AGAIN. Silly. Just go through the map once, do all of the required work there, and then get
    // the balance_out as a by-product.
    // For that 210000 transaction wallet, all of these changes have reduced the time in the miner loop from >750 msec
    // down to < 450 msec.
    AvailableCoinsForStaking(vCoins, nSpendTime, balance_out);

    int64_t BalanceToConsider = balance_out;

    // Check if we have a spendable balance. (This is not strictly necessary but retained for legacy purposes.)
    if (BalanceToConsider <= 0)
    {
        if (fMiner) not_staking_error = GRC::MinerStatus::NO_COINS;

        return false;
    }
    // Check if we have a balance to stake with after the reserve is applied. (This is not strictly necessary
    // but retained for legacy purposes.)
    BalanceToConsider -= nReserveBalance;

    if (BalanceToConsider <= 0)
    {
        if (fMiner) not_staking_error = GRC::MinerStatus::ENTIRE_BALANCE_RESERVED;

        return false;
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::MINER) && fMiner)
        LogPrintf("SelectCoinsForStaking: Balance considered for staking %.8f", BalanceToConsider / (double) COIN);

    // These two blocks below comprise the only truly required test. The others above are maintained for legacy purposes.
    if (vCoins.empty())
    {
        if (fMiner) not_staking_error = GRC::MinerStatus::NO_MATURE_COINS;

        return false;
    }

    // Iterate through the wallet of stakable utxos and return them to miner if we can stake with them. I would like
    // to get rid of this iteration too, but unfortunately, we need the computed balance for the test.
    vCoinsRet.clear();

    for (const COutput& output : vCoins)
    {
        const CWalletTx *pcoin = output.tx;
        int i = output.i;

        // If the Spendable balance is more then utxo value it is classified as able to stake
        if (BalanceToConsider >= pcoin->vout[i].nValue)
        {
            if (LogInstance().WillLogCategory(BCLog::LogFlags::MINER) && fMiner)
            {
                LogPrintf("SelectCoinsForStaking: UTXO=%s (BalanceToConsider=%.8f >= Value=%.8f)",
                          pcoin->vout[i].GetHash().ToString(),
                          BalanceToConsider / (double) COIN,
                          pcoin->vout[i].nValue / (double) COIN);
            }

            vCoinsRet.push_back(make_pair(pcoin, i));
        }
     }

    // Check if we have any utxos to send back at this point and if not the reasoning behind this
    if (vCoinsRet.empty())
    {
        if (fMiner) not_staking_error = GRC::MinerStatus::NO_UTXOS_AVAILABLE_DUE_TO_RESERVE;

        return false;
    }

    g_timer.GetTimes(function + "select loop", "miner");

    // Randomize the vector order to keep PoS truly a roll of dice in which utxo has a chance to stake first
    if (fMiner)
    {
        unsigned int seed = static_cast<unsigned int>(GetAdjustedTime());

        std::shuffle(vCoinsRet.begin(), vCoinsRet.end(), std::default_random_engine(seed));
    }

    g_timer.GetTimes(function + "shuffle", "miner");

    return true;
}

bool CWallet::CreateTransaction(const vector<pair<CScript, int64_t> >& vecSend, set<pair<const CWalletTx*,unsigned int>>& setCoins_in,
                                CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl)
{

    int64_t nValueOut = 0;
    int64_t message_fee = 0;
    set<pair<const CWalletTx*,unsigned int>> setCoins_out;

    bool provided_coin_set = !setCoins_in.empty();

    for (auto const& s : vecSend)
    {
        if (nValueOut < 0)
            return error("%s: invalid output value: %" PRId64, __func__, nValueOut);
        nValueOut += s.second;
    }

    if (vecSend.empty() || nValueOut < 0)
        return error("%s: invalid output value: %" PRId64, __func__, nValueOut);

    // Add the burn fee for a transaction with a custom user message:
    if (!wtxNew.vContracts.empty()
        && wtxNew.vContracts[0].m_type == GRC::ContractType::MESSAGE)
    {
        message_fee = wtxNew.vContracts[0].RequiredBurnAmount();
        nValueOut += message_fee;
    }

    wtxNew.BindWallet(this);

    {
        LOCK2(cs_main, cs_wallet);

        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            nFeeRet = nTransactionFee;
            while (true)
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                setCoins_out.clear();
                wtxNew.fFromMe = true;

                int64_t nTotalValue = nValueOut + nFeeRet;
                // dPriority is not currently used. Commented out.
                //double dPriority = 0;

                // vouts to the payees
                for (auto const& s : vecSend)
                    wtxNew.vout.emplace_back(s.second, s.first);

                // Add the burn fee for a transaction with a custom user message:
                if (message_fee > 0)
                {
                    wtxNew.vout.emplace_back(message_fee, CScript() << OP_RETURN);
                }

                int64_t nValueIn = 0;

                // If provided coin set is empty, choose coins to use.
                if (!provided_coin_set)
                {
                    // If the transaction contains a contract, we want to select the
                    // smallest UTXOs available:
                    //
                    // TODO: make this configurable for users that wish to avoid the
                    // privacy issues caused by lumping inputs into one transaction.
                    //
                    const bool contract = (!coinControl || !coinControl->HasSelected())
                        && !wtxNew.vContracts.empty()
                        && wtxNew.vContracts[0].m_type != GRC::ContractType::MESSAGE;

                    // Notice that setCoins_out is that set PRODUCED by SelectCoins. Tying this to the input
                    // parameter of CreateTransaction was a major bug here before. It is now separated.
                    if (!SelectCoins(nTotalValue, wtxNew.nTime, setCoins_out, nValueIn, coinControl, contract)) {
                        return error("%s: Failed to select coins", __func__);
                    }

                    if (LogInstance().WillLogCategory(BCLog::LogFlags::ESTIMATEFEE))
                    {
                        CAmount setcoins_total = 0;

                        for (const auto& output: setCoins_out)
                        {
                            setcoins_total += output.first->vout[output.second].nValue;
                        }

                        LogPrintf("INFO %s: Just after SelectCoins: "
                                 "nTotalValue = %s, nValueIn = %s, nValueOut = %s, setCoins total = %s.",
                                  __func__,
                                  FormatMoney(nTotalValue),
                                  FormatMoney(nValueIn),
                                  FormatMoney(nValueOut),
                                  FormatMoney(setcoins_total));
                    }

                    // Compute dPriority on automatically selected coins.
                    /*
                    for (auto const& input : setCoins_out)
                    {
                        int64_t nCredit = input.first->vout[input.second].nValue;
                        dPriority += (double) nCredit * input.first->GetDepthInMainChain();
                    }
                    */
                }
                else
                {
                    // Add up input value for the provided set of coins, and also compute dPriority. (dPriority is
                    // commented out because it is not actually used right now.
                    for (auto const& input : setCoins_in)
                    {
                        int64_t nCredit = input.first->vout[input.second].nValue;
                        //dPriority += (double) nCredit * input.first->GetDepthInMainChain();
                        nValueIn += nCredit;
                    }
                }

                int64_t nChange = nValueIn - nValueOut - nFeeRet;

                // Note: In the case where CreateTransaction is called with a provided input set of coins,
                // if the nValueIn of those coins is sufficient to cover the minimum nTransactionFee that starts
                // the while loop, it will pass the first iteration. If the size of the transaction causes the nFeeRet
                // to elevate and a second pass shows that the nValueOut + required fee is greater than that available
                // i.e. negative change, then the loop is exited with an error. The reasoning for this is that
                // in the case of no provided coin set, SelectTransaction above will be given the chance to modify its
                // selection to cover the increased fees, hopefully converging on an appropriate solution. In the case
                // of a provided set of inputs, that set is immutable for this transaction, so no point in continuing.
                if (provided_coin_set && nChange < 0)
                {
                    return error("%s: Total value of inputs, %s, cannot cover the transaction fees of %s. "
                                 "CreateTransaction aborted.",
                                 __func__,
                                 FormatMoney(nValueIn),
                                 FormatMoney(nFeeRet));
                }

                LogPrint(BCLog::LogFlags::ESTIMATEFEE, "INFO %s: Before CENT test: nValueIn = %s, nValueOut = %s, "
                        "nChange = %s, nFeeRet = %s.",
                         __func__,
                         FormatMoney(nValueIn),
                         FormatMoney(nValueOut),
                         FormatMoney(nChange),
                         FormatMoney(nFeeRet));

                // if sub-cent change is required, the fee must be raised to at least GetBaseFee
                // or until nChange becomes zero
                // NOTE: this depends on the exact behaviour of GetMinFee
                if (nFeeRet < wtxNew.GetBaseFee() && nChange > 0 && nChange < CENT)
                {
                    int64_t nMoveToFee = min(nChange, wtxNew.GetBaseFee() - nFeeRet);
                    nChange -= nMoveToFee;
                    nFeeRet += nMoveToFee;

                    LogPrint(BCLog::LogFlags::ESTIMATEFEE, "INFO %s: After CENT limit adjustment: nChange = %s, "
                             "nFeeRet = %s",
                             __func__,
                             FormatMoney(nChange),
                             FormatMoney(nFeeRet));
                }

                if (nChange > 0)
                {
                    // Fill a vout to ourself
                    // TODO : pass in scriptChange instead of reservekey so
                    // change transaction isn't always pay-to-bitcoin-address
                    CScript scriptChange;

                    // coin control: send change to custom address
                    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
                        scriptChange.SetDestination(coinControl->destChange);

                    // no coin control: send change to newly generated address
                    else
                    {
                        // Note: We use a new key here to keep it from being obvious which side is the change.
                        //  The drawback is that by not reusing a previous key, the change may be lost if a
                        //  backup is restored, if the backup doesn't have the new private key for the change.
                        //  If we reused the old key, it would be possible to add code to look for and
                        //  rediscover unknown transactions that were written with keys of ours to recover
                        //  post-backup change.

                        // Reserve a new key pair from key pool
                        CPubKey vchPubKey;
                        assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked

                        scriptChange.SetDestination(vchPubKey.GetID());
                    }

                    // Insert change txn at random position:
                    vector<CTxOut>::iterator position = wtxNew.vout.begin()+GetRandInt(wtxNew.vout.size());
                    wtxNew.vout.insert(position, CTxOut(nChange, scriptChange));
                }
                else
                {
                    reservekey.ReturnKey();
                }

                if (setCoins_in.size())
                {
                    // Fill vin from provided inputs
                    for (auto const& coin : setCoins_in)
                    {
                        wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));
                    }

                    // Sign
                    int nIn = 0;
                    for (auto const& coin : setCoins_in)
                        if (!SignSignature(*this, *coin.first, wtxNew, nIn++)) {
                            return error("%s: Failed to sign tx", __func__);
                        }
                }
                else // use setCoins_out from SelectCoins as the inputs
                {
                    // Fill vin from provided inputs
                    for (auto const& coin : setCoins_out)
                    {
                        wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));
                    }

                    // Sign
                    int nIn = 0;
                    for (auto const& coin : setCoins_out)
                        if (!SignSignature(*this, *coin.first, wtxNew, nIn++))
                        {
                            return error("%s: Failed to sign tx", __func__);
                        }
                }

                // Limit size
                unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK, PROTOCOL_VERSION);
                if (nBytes >= MAX_STANDARD_TX_SIZE) {
                    return error("%s: tx size %d greater than standard %d", __func__, nBytes, MAX_STANDARD_TX_SIZE);
                }

                // dPriority is not currently used.
                //dPriority /= nBytes;

                // Check that enough fee is included
                int64_t nPayFee = nTransactionFee * (1 + (int64_t)nBytes / 1000);
                int64_t nMinFee = wtxNew.GetMinFee(1000, GMF_SEND, nBytes);

                LogPrint(BCLog::LogFlags::ESTIMATEFEE, "INFO %s: nTransactionFee = %s, nBytes = %" PRId64 ", nPayFee = %s"
                         ", nMinFee = %s, nFeeRet = %s.",
                         __func__,
                         FormatMoney(nTransactionFee),
                         nBytes,
                         FormatMoney(nPayFee),
                         FormatMoney(nMinFee),
                         FormatMoney(nFeeRet));

                if (nFeeRet < max(nPayFee, nMinFee))
                {
                    nFeeRet = max(nPayFee, nMinFee);
                    continue;
                }

                LogPrint(BCLog::LogFlags::ESTIMATEFEE, "INFO %s: FINAL nValueIn = %s, nChange = %s, nTransactionFee = %s,"
                         " nBytes = %" PRId64 ", nPayFee = %s, nMinFee = %s, nFeeRet = %s.",
                         __func__,
                         FormatMoney(nValueIn),
                         FormatMoney(nChange),
                         FormatMoney(nTransactionFee),
                         nBytes,
                         FormatMoney(nPayFee),
                         FormatMoney(nMinFee),
                         FormatMoney(nFeeRet));

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

bool CWallet::CreateTransaction(const vector<pair<CScript, int64_t> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey,
    int64_t& nFeeRet, const CCoinControl* coinControl)
{
    // Initialize setCoins empty to let CreateTransaction choose via SelectCoins...
    set<pair<const CWalletTx*,unsigned int>> setCoins;

    return CreateTransaction(vecSend, setCoins, wtxNew, reservekey, nFeeRet, coinControl);
}




bool CWallet::CreateTransaction(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl)
{
    vector< pair<CScript, int64_t> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, coinControl);
}

// Call after CreateTransaction unless you want to abort
bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey)
{
    if(fDevbuildCripple)
    {
        return error("CommitTransaction(): Development build restrictions in effect");
    }
    {
        LOCK2(cs_main, cs_wallet);
        LogPrint(BCLog::LogFlags::VERBOSE, "CommitTransaction:\n%s", wtxNew.ToString());
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile,"r+") : NULL;

            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew, pwalletdb);

            // Mark old coins as spent
            set<CWalletTx*> setCoins;
            for (auto const& txin : wtxNew.vin)
            {
                CWalletTx &coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                coin.MarkSpent(txin.prevout.n);
                coin.WriteToDisk(pwalletdb);
                NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast
        if (!wtxNew.AcceptToMemoryPool())
        {
            // This must not fail. The transaction has already been signed and recorded.
            LogPrintf("CommitTransaction() : Error: Transaction not valid");
            return false;
        }
        wtxNew.RelayWalletTransaction();
    }
    return true;
}




string CWallet::SendMoney(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    int64_t nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        LogPrintf("SendMoney() : %s", strError);
        return strError;
    }
    if (fWalletUnlockStakingOnly)
    {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        LogPrintf("SendMoney() : %s", strError);
        return strError;
    }
    // 12-9-2015 Ensure user has confirmed balance before sending coins

    if (!CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds  "), FormatMoney(nFeeRequired));
        else
            strError = _("Error: Transaction creation failed  ");
        LogPrintf("SendMoney() : %s", strError);
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}

string CWallet::SendMoneyToDestination(const CTxDestination& address, int64_t nValue, CWalletTx& wtxNew, bool fAskFee)
{
    // Check amount
    if (nValue <= 0)        return _("Invalid amount");

    if (nValue + nTransactionFee > GetBalance())
        return _("Insufficient funds");

    // Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address);

    return SendMoney(scriptPubKey, nValue, wtxNew, fAskFee);
}


DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    fFirstRunRet = false;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    NewThread(ThreadFlushWalletDB, &strWalletFile);

    LogPrintf("LoadWallet: started wallet flush thread.");

    return DB_LOAD_OK;
}



DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    DBErrors nZapWalletTxRet = CWalletDB(strWalletFile,"cr+").ZapWalletTx(this, vWtx);
    if (nZapWalletTxRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapWalletTxRet != DB_LOAD_OK)
        return nZapWalletTxRet;

    return DB_LOAD_OK;
}



bool CWallet::SetAddressBookName(const CTxDestination& address, const string& strName)
{
    bool fUpdated = false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<CTxDestination, std::string>::iterator mi = mapAddressBook.find(address);
        fUpdated = mi != mapAddressBook.end();
        mapAddressBook[address] = strName;
    }
    NotifyAddressBookChanged(this, address, strName, (::IsMine(*this, address) != ISMINE_NO),
                             (fUpdated ? CT_UPDATED : CT_NEW) );
    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).WriteName(CBitcoinAddress(address).ToString(), strName);
}

bool CWallet::DelAddressBookName(const CTxDestination& address)
{
    {
        LOCK(cs_wallet); // mapAddressBook

        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", (::IsMine(*this, address) != ISMINE_NO), CT_DELETED);

    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).EraseName(CBitcoinAddress(address).ToString());
}


void CWallet::PrintWallet(const CBlock& block)
{
    {
        LOCK(cs_wallet);
        if (block.IsProofOfWork() && mapWallet.count(block.vtx[0].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[0].GetHash()];
            LogPrintf("    mine:  %d  %d  %" PRId64 "", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
        }
        if (block.IsProofOfStake() && mapWallet.count(block.vtx[1].GetHash()))
        {
            CWalletTx& wtx = mapWallet[block.vtx[1].GetHash()];
            LogPrintf("    stake: %d  %d  %" PRId64 "", wtx.GetDepthInMainChain(), wtx.GetBlocksToMaturity(), wtx.GetCredit());
         }

    }
    LogPrintf("");
}

bool CWallet::GetTransaction(const uint256 &hashTx, CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
        {
            wtx = (*mi).second;
            return true;
        }
    }
    return false;
}

bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (fFileBacked)
    {
        if (!CWalletDB(strWalletFile).WriteDefaultKey(vchPubKey))
            return false;
    }
    vchDefaultKey = vchPubKey;
    return true;
}

//
// Mark old keypool keys as used,
// and generate all new keys
//
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        for (auto const& nIndex : setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64_t nKeys = max(GetArg("-keypool", 100), (int64_t)0);
        for (int i = 0; i < nKeys; i++)
        {
            int64_t nIndex = i+1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey()));
            setKeyPool.insert(nIndex);
        }
        LogPrintf("CWallet::NewKeyPool wrote %" PRId64 " new keys", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool(unsigned int nSize)
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize;
        if (nSize > 0)
            nTargetSize = nSize;
        else
            nTargetSize = max(GetArg("-keypool", 100), (int64_t)0);

        while (setKeyPool.size() < (nTargetSize + 1))
        {
            int64_t nEnd = 1;
            if (!setKeyPool.empty())
                nEnd = *(--setKeyPool.end()) + 1;
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey())))
                throw runtime_error("TopUpKeyPool() : writing generated key failed");
            setKeyPool.insert(nEnd);
            LogPrint(BCLog::LogFlags::NOISY, "keypool added key %" PRId64 ", size=%" PRIszu, nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool() : read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool() : unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && GetBoolArg("-printkeypool"))
            LogPrintf("keypool reserve %" PRId64, nIndex);
    }
}

int64_t CWallet::AddReserveKey(const CKeyPool& keypool)
{
    {
        LOCK2(cs_main, cs_wallet);
        CWalletDB walletdb(strWalletFile);

        int64_t nIndex = 1 + *(--setKeyPool.end());
        if (!walletdb.WritePool(nIndex, keypool))
            throw runtime_error("AddReserveKey() : writing added key failed");
        setKeyPool.insert(nIndex);
        return nIndex;
    }
    return -1;
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    if (fFileBacked)
    {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
    LogPrint(BCLog::LogFlags::VERBOSE, "keypool keep %" PRId64, nIndex);
}

void CWallet::ReturnKey(int64_t nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
    LogPrint(BCLog::LogFlags::VERBOSE, "keypool return %" PRId64, nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result, bool fAllowReuse)
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex == -1)
        {
            if (fAllowReuse && vchDefaultKey.IsValid())
            {
                result = vchDefaultKey;
                return true;
            }
            if (IsLocked()) return false;
            result = GenerateNewKey();
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return  GetAdjustedTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

std::map<CTxDestination, int64_t> CWallet::GetAddressBalances()
{
    map<CTxDestination, int64_t> balances;

    {
        LOCK(cs_wallet);
        for (auto walletEntry : mapWallet)
        {
            CWalletTx *pcoin = &walletEntry.second;

            if (!IsFinalTx(*pcoin) || !pcoin->IsTrusted())
                continue;

            if ((pcoin->IsCoinBase() || pcoin->IsCoinStake()) && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe() ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                CTxDestination addr;
                if (IsMine(pcoin->vout[i]) == ISMINE_NO)
                    continue;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                int64_t n = pcoin->IsSpent(i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set< set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    set< set<CTxDestination> > groupings;
    set<CTxDestination> grouping;

    for (auto walletEntry : mapWallet)
    {
        CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0 && (IsMine(pcoin->vin[0]) != ISMINE_NO))
        {
            // group all input addresses with each other
            for (auto const& txin : pcoin->vin)
            {
                CTxDestination address;
                if(!ExtractDestination(mapWallet[txin.prevout.hash].vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
            }

            // group change with input addresses
            for (auto const& txout : pcoin->vout)
                if (IsChange(txout))
                {
                    CWalletTx tx = mapWallet[pcoin->vin[0].prevout.hash];
                    CTxDestination txoutAddr;
                    if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                        continue;
                    grouping.insert(txoutAddr);
                }
            groupings.insert(grouping);
            grouping.clear();
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i]) != ISMINE_NO)
            {
                CTxDestination address;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set< set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    map< CTxDestination, set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    for (auto const& grouping : groupings)
    {
        // make a set of all the groups hit by this new group
        set< set<CTxDestination>* > hits;
        map< CTxDestination, set<CTxDestination>* >::iterator it;
        for (auto const& address : grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        set<CTxDestination>* merged = new set<CTxDestination>(grouping);
        for (auto const& hit : hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for (auto const& element : *merged)
            setmap[element] = merged;
    }

    set< set<CTxDestination> > ret;
    for (auto const& uniqueGrouping : uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

// ppcoin: check 'spent' consistency between wallet and txindex
// ppcoin: fix wallet spent state according to txindex
void CWallet::FixSpentCoins(int& nMismatchFound, int64_t& nBalanceInQuestion, bool fCheckOnly)
{
    nMismatchFound = 0;
    nBalanceInQuestion = 0;

    LOCK(cs_wallet);
    vector<CWalletTx*> vCoins;
    vCoins.reserve(mapWallet.size());
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        vCoins.push_back(&(*it).second);

    CWalletDB walletdb(strWalletFile);

    CTxDB txdb("r");
    for (auto const& pcoin : vCoins)
    {
        // Find the corresponding transaction index
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(pcoin->GetHash(), txindex))
            continue;
        for (unsigned int n=0; n < pcoin->vout.size(); n++)
        {
            if ((IsMine(pcoin->vout[n]) != ISMINE_NO) && pcoin->IsSpent(n) && (txindex.vSpent.size() <= n || txindex.vSpent[n].IsNull()))
            {
                LogPrintf("FixSpentCoins found lost coin %s gC %s[%d], %s",
                    FormatMoney(pcoin->vout[n].nValue).c_str(), pcoin->GetHash().ToString().c_str(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkUnspent(n);
                    pcoin->WriteToDisk(&walletdb);
                }
            }
            else if ((IsMine(pcoin->vout[n]) != ISMINE_NO) && !pcoin->IsSpent(n) && (txindex.vSpent.size() > n && !txindex.vSpent[n].IsNull()))
            {
                LogPrintf("FixSpentCoins found spent coin %s gC %s[%d], %s",
                    FormatMoney(pcoin->vout[n].nValue).c_str(), pcoin->GetHash().ToString().c_str(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkSpent(n);
                    pcoin->WriteToDisk(&walletdb);
                }
            }
        }
    }
}

// ppcoin: disable transaction (only for coinstake)
void CWallet::DisableTransaction(const CTransaction &tx)
{
    if (!tx.IsCoinStake() || !IsFromMe(tx))
        return; // only disconnecting coinstake requires marking input unspent

    LOCK(cs_wallet);

    CWalletDB walletdb(strWalletFile);

    for (auto const& txin : tx.vin)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size() && (IsMine(prev.vout[txin.prevout.n]) != ISMINE_NO))
            {
                prev.MarkUnspent(txin.prevout.n);
                prev.WriteToDisk(&walletdb);
            }
        }
    }
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            if (pwallet->vchDefaultKey.IsValid()) {
                LogPrintf("CReserveKey::GetReservedKey(): Warning: Using default key instead of a new key, top up your keypool!");
                vchPubKey = pwallet->vchDefaultKey;
            } else
                return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    for (auto const& id : setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes() : read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes() : unknown key in key pool");
        setAddress.insert(keyID);
    }
}

std::vector<std::pair<CBitcoinAddress, CBitcoinSecret>> CWallet::GetAllPrivateKeys(std::string& sError) const
{
    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);

    std::vector<std::pair<CBitcoinAddress, CBitcoinSecret>> res;
    // Get Privaty Keys from mapAddressBook
    for (auto const& item : mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        isminetype fMine = ::IsMine(*this, address.Get());
        if (fMine != ISMINE_NO)
        {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
            {
                LogPrintf("GetAllPrivateKeys: During private key backup, Address %s does not refer to a key", address.ToString());
            }
            else
            {
                bool IsCompressed;
                CKey vchSecret;
                if (!GetKey(keyID, vchSecret))
                {
                    LogPrintf("GetAllPrivateKeys: During private key backup, Private key for address %s is not known", address.ToString());
                }
                else
                {
                    CSecret secret = vchSecret.GetSecret(IsCompressed);
                    CBitcoinSecret privateKey(secret, IsCompressed);
                    res.push_back(std::make_pair(address, privateKey));
                }
            }
        }
    }
    // Get Private Keys from KeyPool
    for (auto const& id : setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
        {
            // Important to know.
            sError = "GetAllPrivateKeys: Failed to read pool";
        }
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();

        if (!HaveKey(keyID))
        {
            LogPrintf("GetAllPrivateKeys: Unknown key in key pool");
        }
        else
        {
            bool IsCompressed;
            CKey vchSecret;
            //CSecret vchSecret;
            if (!GetKey(keyID, vchSecret))
            {
                LogPrintf("GetAllPrivateKeys: During Private Key Backup, Private key for address %s is not known", keyID.ToString());
            }
            else
            {
                CSecret secret = vchSecret.GetSecret(IsCompressed);
                CBitcoinAddress address(keyID);
                CBitcoinSecret privateKey(secret, IsCompressed);
                res.push_back(std::make_pair(address, privateKey));
            }
        }
    }
    return res;
}

void CWallet::UpdatedTransaction(const uint256 &hashTx)
{
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
    }
}

void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const {
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndex *pindexMax = GRC::BlockFinder().FindByHeight(std::max(0, nBestHeight - 144)); // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    GetKeys(setKeys);
    for (auto const&keyid : setKeys) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }
    setKeys.clear();

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {
        // iterate over all wallet transactions...
        const CWalletTx &wtx = (*it).second;
        BlockMap::iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && blit->second->IsInMainChain()) {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            for (auto const& txout : wtx.vout) {
                // iterate over all their outputs
                ::ExtractAffectedKeys(*this, txout.scriptPubKey, vAffected);
                for (auto const& keyid : vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = blit->second;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
        mapKeyBirth[it->first] = it->second->nTime - 7200; // block times can be 2h off
}

int64_t CWallet::GetLastBackupTime() const
{
    int64_t out_backup_time = 0;
    CWalletDB(strWalletFile).ReadBackupTime(out_backup_time);

    return out_backup_time;
}

void CWallet::StoreLastBackupTime(const int64_t backup_time)
{
    CWalletDB(strWalletFile).WriteBackupTime(backup_time);
}

MinedType GetGeneratedType(const CWallet *wallet, const uint256& tx, unsigned int vout)
{
    CWalletTx wallettx;
    uint256 hashblock;

    if (!GetTransaction(tx, wallettx, hashblock))
        return MinedType::ORPHANED;

    BlockMap::iterator mi = mapBlockIndex.find(hashblock);

    if (mi == mapBlockIndex.end())
        return MinedType::UNKNOWN;

    CBlockIndex* blkindex = (*mi).second;

    // If we are calling GetGeneratedType, this is a transaction
    // that corresponds (is integral to) the block. We check whether
    // the block is a superblock, and if so we set the MinedType to
    // SUPERBLOCK if vout is 1 as that should override the others here.
    if (vout == 1 && blkindex->IsSuperblock())
    {
        return MinedType::SUPERBLOCK;
    }

    // Basic CoinStake Support
    if (wallettx.vout.size() == 2)
    {
        if (blkindex->ResearchSubsidy() == 0)
            return MinedType::POS;

        else
            return MinedType::POR;
    }

    // Side/Split Stake Support
    else if (wallettx.vout.size() >= 3)
    {
        // The first output of the coinstake has the same owner as the input.
        bool fIsCoinStakeMine = (wallet->IsMine(wallettx.vout[1]) != ISMINE_NO) ? true : false;
        bool fIsOutputMine = (wallet->IsMine(wallettx.vout[vout]) != ISMINE_NO) ? true : false;

        // If output 1 is mine and the pubkey (address) for the output is the same as
        // output 1, it is a split stake return from my stake.
        if (fIsCoinStakeMine && wallettx.vout[vout].scriptPubKey == wallettx.vout[1].scriptPubKey)
        {
            if (blkindex->ResearchSubsidy() == 0)
                return MinedType::POS;

            else
                return MinedType::POR;
        }
        else
        {
            // If the coinstake is mine...
            if (fIsCoinStakeMine)
            {
                // ... you can sidestake back to yourself...
                if (fIsOutputMine)
                {
                    if (blkindex->ResearchSubsidy() == 0)
                        return MinedType::POS_SIDE_STAKE_RCV;
                    else
                        return MinedType::POR_SIDE_STAKE_RCV;
                }
                // ... or the output is not mine, then this must be a
                // sidestake sent to someone else.
                else
                {
                    if (blkindex->ResearchSubsidy() == 0)
                        return MinedType::POS_SIDE_STAKE_SEND;
                    else
                        return MinedType::POR_SIDE_STAKE_SEND;
                }
            }
            // otherwise, the coinstake return is not mine... (i.e. someone else...)
            else
            {
                // ... but the output is mine, then this must be a
                // received sidestake from the staker.
                if (fIsOutputMine)
                {
                    if (blkindex->ResearchSubsidy() == 0)
                        return MinedType::POS_SIDE_STAKE_RCV;

                    else
                        return MinedType::POR_SIDE_STAKE_RCV;
                }

                // the asymmetry is that the case when neither the first coinstake output
                // nor the selected output are mine, then this coinstake is irrelevant.
            }
        }
    }

    return MinedType::UNKNOWN;
}
