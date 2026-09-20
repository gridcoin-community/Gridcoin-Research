// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KEYSTORE_H
#define BITCOIN_KEYSTORE_H

#include "crypter.h"
#include "sync.h"
#include <boost/signals2/signal.hpp>

class CScript;
class CScriptID;

/** Read-only interface for key and script access, used by signing code.
 *  Unlike CKeyStore, this has no Add* or enumeration methods.
 */
class SigningProvider
{
public:
    virtual ~SigningProvider() {}
    virtual bool HaveKey(const CKeyID &address) const { return false; }
    virtual bool GetKey(const CKeyID &address, CKey& keyOut) const { return false; }
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const { return false; }
    virtual bool HaveCScript(const CScriptID &hash) const { return false; }
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const { return false; }
};

/** A virtual base class for key stores */
class CKeyStore : public SigningProvider
{
protected:
    mutable CCriticalSection cs_KeyStore;

public:
    virtual ~CKeyStore() {}

    // Add a key to the store.
    virtual bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) =0;
    virtual bool AddKey(const CKey &key);

    // Check whether a key corresponding to a given address is present in the store.
    virtual bool HaveKey(const CKeyID &address) const =0;
    virtual bool GetKey(const CKeyID &address, CKey& keyOut) const =0;
    virtual void GetKeys(std::set<CKeyID> &setAddress) const =0;
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;

    // Support for BIP 0013 : see https://github.com/bitcoin/bips/blob/master/bip-0013.mediawiki
    virtual bool AddCScript(const CScript& redeemScript) =0;
    virtual bool HaveCScript(const CScriptID &hash) const =0;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const =0;
};

typedef std::map<CKeyID, CKey> KeyMap;
typedef std::map<CScriptID, CScript > ScriptMap;

/** Basic key store, that keeps keys in an address->secret map */
class CBasicKeyStore : public CKeyStore
{
protected:
    KeyMap mapKeys GUARDED_BY(cs_KeyStore);
    ScriptMap mapScripts GUARDED_BY(cs_KeyStore);

public:
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    bool HaveKey(const CKeyID &address) const
    {
        bool result;
        {
            LOCK(cs_KeyStore);
            result = (mapKeys.count(address) > 0);
        }
        return result;
    }
    void GetKeys(std::set<CKeyID> &setAddress) const
    {
        setAddress.clear();
        {
            LOCK(cs_KeyStore);
            KeyMap::const_iterator mi = mapKeys.begin();
            while (mi != mapKeys.end())
            {
                setAddress.insert(mi->first);
                mi++;
            }
        }
    }
    bool GetKey(const CKeyID &address, CKey &keyOut) const
    {
        {
            LOCK(cs_KeyStore);
            KeyMap::const_iterator mi = mapKeys.find(address);
            if (mi != mapKeys.end())
            {
                keyOut = mi->second;
                return true;
            }
        }
        return false;
    }
    virtual bool AddCScript(const CScript& redeemScript);
    virtual bool HaveCScript(const CScriptID &hash) const;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const;
};

typedef std::map<CKeyID, std::pair<CPubKey, std::vector<unsigned char> > > CryptedKeyMap;

//! \brief What the current unlock permits.
//!
//! One value, not a lock flag plus a restriction flag. It is written in the same
//! cs_KeyStore section that installs or clears the master key, so no reader can
//! observe an unlocked store carrying the previous unlock's restriction. That
//! pair being separately written is what let a full unlock's cleared restriction
//! survive into a staking-only unlock.
//!
//! Ephemeral by construction: the master key is secure-allocated and never
//! serialized, and no startup path unlocks, so an encrypted wallet begins every
//! run Locked. What wallet.dat persists is the encrypted master key records,
//! which say nothing about scope.
enum class UnlockScope : uint8_t {
    //! No master key. Nothing that needs one may proceed.
    Locked,

    //! Unlocked for staking only. The kernel may be signed; spends may not.
    StakingOnly,

    //! Unlocked without restriction.
    Full,
};

/** Keystore which keeps the private keys encrypted.
 * It derives from the basic key store, which is used if no encryption is active.
 */
class CCryptoKeyStore : public CBasicKeyStore
{
private:
    CryptedKeyMap mapCryptedKeys GUARDED_BY(cs_KeyStore);

    CKeyingMaterial vMasterKey GUARDED_BY(cs_KeyStore);

    //! Written only alongside vMasterKey, under the same lock, so the two cannot
    //! disagree. Locked exactly when vMasterKey is empty.
    UnlockScope m_unlock_scope GUARDED_BY(cs_KeyStore){UnlockScope::Locked};

    // if fUseCrypto is true, mapKeys must be empty
    // if fUseCrypto is false, vMasterKey must be empty
    bool fUseCrypto;

    bool fDecryptionThoroughlyChecked;

protected:
    bool SetCrypted();

    // will encrypt previously unencrypted keys
    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn);

    bool Unlock(const CKeyingMaterial& vMasterKeyIn, UnlockScope scope);

    //! Encrypt/decrypt an arbitrary non-key wallet secret (e.g. the seed
    //! phrase blob) under the store's keying material. The store must be
    //! crypted and unlocked. The IV must be deterministic and unique to the
    //! secret, mirroring the per-key pubkey-hash IV used for private keys.
    bool EncryptSecretWithMasterKey(const CKeyingMaterial& plaintext, const uint256& iv,
                                    std::vector<unsigned char>& ciphertext_out) const;
    bool DecryptSecretWithMasterKey(const std::vector<unsigned char>& ciphertext, const uint256& iv,
                                    CKeyingMaterial& plaintext_out) const;

public:
    CCryptoKeyStore()
        : fUseCrypto(false)
        , fDecryptionThoroughlyChecked(false)
    {
    }

    bool IsCrypted() const
    {
        return fUseCrypto;
    }

    //! \brief What the current unlock permits.
    //!
    //! An unencrypted wallet has nothing to restrict, so it reads Full: every
    //! caller asking "may this proceed" gets yes, which is what it got before
    //! there was a scope at all.
    UnlockScope GetUnlockScope() const
    {
        if (!IsCrypted())
            return UnlockScope::Full;

        LOCK(cs_KeyStore);
        return m_unlock_scope;
    }

    bool IsLocked() const
    {
        return GetUnlockScope() == UnlockScope::Locked;
    }

    //! \brief Narrow the current unlock to staking only.
    //!
    //! Narrowing only: it can remove permission, never add it, so unlike
    //! Unlock() it needs no passphrase -- the master key is already installed
    //! and stays installed. Returns false when there is nothing to narrow,
    //! which covers BOTH a locked wallet and an unencrypted one; a caller must
    //! not read false as "it was locked". A no-op on one already restricted.
    //!
    //! This exists so an elevation taken for one operation can be handed back.
    //! Locking instead would silently stop staking on a wallet the user had
    //! deliberately left staking.
    bool RestrictToStakingOnly()
    {
        // False on an UNENCRYPTED wallet as well as a locked one: there is
        // nothing to restrict in either case. A caller must not read false as
        // "the wallet was locked".
        if (!IsCrypted()) return false;

        {
            LOCK(cs_KeyStore);
            if (m_unlock_scope == UnlockScope::Locked) return false;
            if (m_unlock_scope == UnlockScope::StakingOnly) return true;

            m_unlock_scope = UnlockScope::StakingOnly;
        }

        // Announce it, as Lock() and Unlock() do. This changes exactly the bit
        // WalletLockState publishes over IPC, and a subscriber that refreshes
        // cached lock state on the status signal would otherwise miss the
        // transition. Emitted outside the lock, matching the other two.
        NotifyStatusChanged(this);
        return true;
    }

    bool Lock();

    virtual bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    bool HaveKey(const CKeyID &address) const
    {
        {
            LOCK(cs_KeyStore);
            if (!IsCrypted())
                return CBasicKeyStore::HaveKey(address);
            return mapCryptedKeys.count(address) > 0;
        }
        return false;
    }
    bool GetKey(const CKeyID &address, CKey& keyOut) const;
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;
    void GetKeys(std::set<CKeyID> &setAddress) const
    {
        // The lock covers the IsCrypted() test as well as the walk below, so the
        // branch and the container it selects cannot disagree. cs_KeyStore is
        // recursive, so delegating to the base -- which takes it again -- is fine.
        // This mirrors HaveKey above.
        LOCK(cs_KeyStore);
        if (!IsCrypted())
        {
            CBasicKeyStore::GetKeys(setAddress);
            return;
        }
        setAddress.clear();
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        while (mi != mapCryptedKeys.end())
        {
            setAddress.insert(mi->first);
            mi++;
        }
    }

    /* Wallet status (encrypted, locked) changed.
     * Note: Called without locks held.
     */
    boost::signals2::signal<void (CCryptoKeyStore* wallet)> NotifyStatusChanged;
};

/** Checks if a CKey is in the given SigningProvider compressed or otherwise*/
bool HaveKey(const SigningProvider& store, const CKey& key);

#endif
