// Copyright (c) 2009-2012 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <vector>
#include <string>

#include "crypter.h"
#include "scrypt.h"

bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
    {
        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
                          (unsigned char *)&strKeyData[0], strKeyData.size(), nRounds, vchKey.data(), vchIV.data());
    }

    if (nDerivationMethod == 1)
    {
        // Passphrase conversion
        uint256 scryptHash = scrypt_salted_multiround_hash((const void*)strKeyData.c_str(), strKeyData.size(), &chSalt[0], 8, nRounds);

        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
                          (unsigned char *)&scryptHash, sizeof scryptHash, nRounds, vchKey.data(), vchIV.data());
        memory_cleanse(&scryptHash, sizeof scryptHash);
    }


    if (i != (int)WALLET_CRYPTO_KEY_SIZE)
    {
        memory_cleanse(vchKey.data(), vchKey.size());
        memory_cleanse(vchIV.data(), vchIV.size());
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_KEY_SIZE)
        return false;

    memcpy(vchKey.data(), chNewKey.data(), chNewKey.size());
    memcpy(vchIV.data(), chNewIV.data(), chNewIV.size());

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext) const
{
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char> (nCLen);

    bool fOk = true;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
        throw std::runtime_error("Error allocating cipher context");

    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, vchKey.data(), vchIV.data());
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext) const
{
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    vchPlaintext = CKeyingMaterial(nPLen);

    bool fOk = true;

    EVP_CIPHER_CTX *ctx= EVP_CIPHER_CTX_new();
    if(!ctx)
        throw std::runtime_error("Error allocating cipher context");

    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, vchKey.data(), vchIV.data());
    if (fOk) fOk = EVP_DecryptUpdate(ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (&vchPlaintext[0])+nPLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    vchPlaintext.resize(nPLen + nFLen);
    return true;
}


bool EncryptSecret(CKeyingMaterial& vMasterKey, const CSecret &vchPlaintext, const uint256& nIV, std::vector<unsigned char> &vchCiphertext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt((CKeyingMaterial)vchPlaintext, vchCiphertext);
}

bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CSecret& vchPlaintext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, *((CKeyingMaterial*)&vchPlaintext));
}
