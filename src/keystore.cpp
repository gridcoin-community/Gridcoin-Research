// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "keystore.h"
#include "script.h"

bool CKeyStore::GetPubKey(const CKeyID &address, CPubKey &vchPubKeyOut) const
{
    CKey key;
    if (!GetKey(address, key))
        return false;
    vchPubKeyOut = key.GetPubKey();
    return true;
}

bool CBasicKeyStore::AddKey(const CKey& key)
{
    CSecret secret(key.begin(), key.end());
    {
        LOCK(cs_KeyStore);
        mapKeys[key.GetPubKey().GetID()] = make_pair(secret, key.IsCompressed());
    }
    return true;
}

bool CBasicKeyStore::AddCScript(const CScript& redeemScript)
{
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
        return error("CBasicKeyStore::AddCScript() : redeemScripts > %i bytes are invalid", MAX_SCRIPT_ELEMENT_SIZE);

    {
        LOCK(cs_KeyStore);
        mapScripts[redeemScript.GetID()] = redeemScript;
    }
    return true;
}

bool CBasicKeyStore::HaveCScript(const CScriptID& hash) const
{
    bool result;
    {
        LOCK(cs_KeyStore);
        result = (mapScripts.count(hash) > 0);
    }
    return result;
}


bool CBasicKeyStore::GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const
{
    {
        LOCK(cs_KeyStore);
        ScriptMap::const_iterator mi = mapScripts.find(hash);
        if (mi != mapScripts.end())
        {
            redeemScriptOut = mi->second;
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::SetCrypted()
{
    {
        LOCK(cs_KeyStore);
        if (fUseCrypto)
            return true;
        if (!mapKeys.empty())
            return false;
        fUseCrypto = true;
    }
    return true;
}

bool CCryptoKeyStore::Lock()
{
    if (!SetCrypted())
        return false;

    {
        LOCK(cs_KeyStore);
        vMasterKey.clear();
    }

    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        bool keyPass = false;
        bool keyFail = false;

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi)
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            CKey key;
            if (!DecryptKey(vMasterKeyIn, vchCryptedSecret, vchPubKey, key))
            {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }
        if (keyPass && keyFail)
        {
            error("%s: The wallet is probably corrupted: Some keys decrypt but not all.", __func__);
            assert(false);
        }
        // div72: The memset below is necessary because of a GCC(seen on 12.1.0) bug where
        // keyPass and !keyPass can both evaluate to true at the same time when the value
        // of keyPass is other than 1.
        static_assert(sizeof(bool) == 1);
        if (keyPass) std::memset(&keyPass, 1, sizeof(bool));
        if (keyFail || !keyPass)
            return false;
        vMasterKey = vMasterKeyIn;
        fDecryptionThoroughlyChecked = true;
    }
    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::AddKey(const CKey& key)
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::AddKey(key);

        if (IsLocked())
            return false;

        std::vector<unsigned char> vchCryptedSecret;
        CPubKey vchPubKey = key.GetPubKey();
        CSecret secret(key.begin(), key.end());
        if (!EncryptSecret(vMasterKey, secret, vchPubKey.GetHash(), vchCryptedSecret))
            return false;

        if (!AddCryptedKey(key.GetPubKey(), vchCryptedSecret))
            return false;
    }
    return true;
}


bool CCryptoKeyStore::AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    }
    return true;
}

bool CCryptoKeyStore::GetKey(const CKeyID &address, CKey& keyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetKey(address, keyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end())
        {
            const CPubKey& vchPubKey = mi->second.first;
            const std::vector<unsigned char>& vchCryptedSecret = mi->second.second;
            CSecret vchSecret;
            if (!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
                return false;
            if (vchSecret.size() != 32)
                return false;
            keyOut.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
            return keyOut.IsValid();
        }
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CKeyStore::GetPubKey(address, vchPubKeyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end())
        {
            vchPubKeyOut = mi->second.first;
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!mapCryptedKeys.empty() || IsCrypted())
            return false;

        fUseCrypto = true;
        for(KeyMap::value_type& mKey : mapKeys)
        {
            CKey key;
            key.Set(mKey.second.first.begin(), mKey.second.first.end(), mKey.second.second);
            if (!key.IsValid())
                return false;
            const CPubKey vchPubKey = key.GetPubKey();
            std::vector<unsigned char> vchCryptedSecret;
            if (!EncryptSecret(vMasterKeyIn, mKey.second.first, vchPubKey.GetHash(), vchCryptedSecret))
                return false;
            if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
                return false;
        }
        mapKeys.clear();
    }
    return true;
}
