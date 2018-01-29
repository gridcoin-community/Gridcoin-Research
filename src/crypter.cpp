// Copyright (c) 2009-2012 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <vector>
#include <string>
#ifdef WIN32
#include <windows.h>
#endif

#include "crypter.h"
#include "scrypt.h"


unsigned char chKeyGridcoin[256];
unsigned char chIVGridcoin[256];
bool fKeySetGridcoin;
std::string getHardwareID();
std::string RetrieveMd5(std::string s1);

bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
    {
        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
                          (unsigned char *)&strKeyData[0], strKeyData.size(), nRounds, chKey, chIV);
    }

    if (nDerivationMethod == 1)
    {
        // Passphrase conversion
        uint256 scryptHash = scrypt_salted_multiround_hash((const void*)strKeyData.c_str(), strKeyData.size(), &chSalt[0], 8, nRounds);

        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
                          (unsigned char *)&scryptHash, sizeof scryptHash, nRounds, chKey, chIV);
        OPENSSL_cleanse(&scryptHash, sizeof scryptHash);
    }


    if (i != (int)WALLET_CRYPTO_KEY_SIZE)
    {
        OPENSSL_cleanse(&chKey, sizeof chKey);
        OPENSSL_cleanse(&chIV, sizeof chIV);
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_KEY_SIZE)
        return false;

    memcpy(&chKey[0], &chNewKey[0], sizeof chKey);
    memcpy(&chIV[0], &chNewIV[0], sizeof chIV);

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext)
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

    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKey, chIV);
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext)
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
 
    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKey, chIV);
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








bool LoadGridKey(std::string gridkey, std::string salt)
{
    const char* chGridKey = gridkey.c_str();
    const char* chSalt = salt.c_str();
    OPENSSL_cleanse(chKeyGridcoin, sizeof(chKeyGridcoin));
    OPENSSL_cleanse(chIVGridcoin, sizeof(chIVGridcoin));
    EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(),(unsigned char *)chSalt,
        (unsigned char *)chGridKey, 
        strlen(chGridKey), 1,
        chKeyGridcoin, chIVGridcoin);
    fKeySetGridcoin = true;
    return true;
}







bool GridEncrypt(std::vector<unsigned char> vchPlaintext, std::vector<unsigned char> &vchCiphertext)
{
    LoadGridKey("gridcoin","cqiuehEJ2Tqdov");
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext.resize(nCLen);
    bool fOk = true;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
        throw std::runtime_error("Error allocating cipher context");

    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKeyGridcoin, chIVGridcoin);
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);
    if (!fOk) return false;
    vchCiphertext.resize(nCLen + nFLen);
    return true;
}


bool GridDecrypt(const std::vector<unsigned char>& vchCiphertext,std::vector<unsigned char>& vchPlaintext)
{
    LoadGridKey("gridcoin","cqiuehEJ2Tqdov");
    int nLen = vchCiphertext.size();
    vchPlaintext.resize(nLen); //plain text will be smaller or eq than ciphertext
    int nPLen = nLen, nFLen = 0;
    bool fOk = true;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
        throw std::runtime_error("Error allocating cipher context");
    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKeyGridcoin, chIVGridcoin);
    if (fOk) fOk = EVP_DecryptUpdate(ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (&vchPlaintext[0])+nPLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);
    if (!fOk) return false;
    vchPlaintext.resize(nPLen + nFLen);
    return true;
}





bool GridEncryptWithSalt(std::vector<unsigned char> vchPlaintext, std::vector<unsigned char> &vchCiphertext, std::string salt)
{
    LoadGridKey("gridcoin",salt);
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char> (nCLen);
    bool fOk = true;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
        throw std::runtime_error("Error allocating cipher context");
   
    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKeyGridcoin, chIVGridcoin);
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);
    if (!fOk) return false;
    vchCiphertext.resize(nCLen + nFLen);
    return true;
}


bool GridDecryptWithSalt(const std::vector<unsigned char>& vchCiphertext,std::vector<unsigned char>& vchPlaintext, std::string salt)
{
    LoadGridKey("gridcoin",salt);
    int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;
    bool fOk = true;

    // Allocate data for the plaintext string. This is always equal to lower
    // than the length of the encrypted string. Stray data is discarded
    // after successfully decrypting.
    vchPlaintext.resize(nLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(!ctx)
        throw std::runtime_error("Error allocating cipher context");

    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKeyGridcoin, chIVGridcoin);    
    if (fOk) fOk = EVP_DecryptUpdate(ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (&vchPlaintext[0])+nPLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);
    if (!fOk) return false;

    vchPlaintext.resize(nPLen + nFLen);
    return true;
}






char FromUnsigned( unsigned char ch )
{
  return static_cast< char >( ch );
}

std::string UnsignedVectorToString(std::vector< unsigned char > v)
{
    std::string s;
    s.reserve( v.size() );
    std::transform( v.begin(), v.end(), back_inserter( s ), FromUnsigned );
    return s;
}


std::string AdvancedCrypt(std::string boinchash)
{

    try 
    {
       std::vector<unsigned char> vchSecret( boinchash.begin(), boinchash.end() );
       std::vector<unsigned char> vchCryptedSecret;
       GridEncrypt(vchSecret, vchCryptedSecret);
       std::string encrypted = EncodeBase64(UnsignedVectorToString(vchCryptedSecret));
       return encrypted;
    } 
    catch (std::exception &e) 
    {
        printf("Error while encrypting %s",boinchash.c_str());
        return "";
    }
    catch(...)
    {
        printf("Error while encrypting 2.");
        return "";
    }
              
}

std::string AdvancedDecrypt(std::string boinchash_encrypted)
{
    try{
       std::string pre_encrypted_boinchash = DecodeBase64(boinchash_encrypted);
       std::vector<unsigned char> vchCryptedSecret(pre_encrypted_boinchash.begin(),pre_encrypted_boinchash.end());
       std::vector<unsigned char> vchPlaintext;
       GridDecrypt(vchCryptedSecret,vchPlaintext);
       std::string decrypted = UnsignedVectorToString(vchPlaintext);
       return decrypted;
    } catch (std::exception &e) 
    {
        printf("Error while decrypting %s",boinchash_encrypted.c_str());
        return "";
    }
    catch(...)
    {
        printf("Error while decrypting 2.");
        return "";
    }
}
     

std::string AdvancedCryptWithHWID(std::string data)
{
    std::string HWID = getHardwareID();
    std::string enc = "";
    std::string salt = HWID;
    for (unsigned int i = 0; i < 9; i++)
    {
        std::string old_salt = salt;
        salt = RetrieveMd5(old_salt);
    }
    enc = AdvancedCryptWithSalt(data,salt);
    return enc;
}

std::string AdvancedDecryptWithHWID(std::string data)
{
    std::string HWID = getHardwareID();
    std::string salt = HWID;
    for (unsigned int i = 0; i < 9; i++)
    {
        std::string old_salt = salt;
        salt = RetrieveMd5(old_salt);
    }
    std::string dec = AdvancedDecryptWithSalt(data,salt);
    return dec;
}

std::string AdvancedCryptWithSalt(std::string boinchash, std::string salt)
{
    try 
    {
       std::vector<unsigned char> vchSecret( boinchash.begin(), boinchash.end() );
       std::vector<unsigned char> vchCryptedSecret;
       GridEncryptWithSalt(vchSecret, vchCryptedSecret,salt);
       std::string encrypted = EncodeBase64(UnsignedVectorToString(vchCryptedSecret));

       return encrypted;
    } catch (std::exception &e) 
    {
        printf("Error while encrypting %s",boinchash.c_str());
        return "";
    }
}

std::string AdvancedDecryptWithSalt(std::string boinchash_encrypted, std::string salt)
{
    try{
       std::string pre_encrypted_boinchash = DecodeBase64(boinchash_encrypted);
       std::vector<unsigned char> vchCryptedSecret(pre_encrypted_boinchash.begin(),pre_encrypted_boinchash.end());
       std::vector<unsigned char> vchPlaintext;
       GridDecryptWithSalt(vchCryptedSecret,vchPlaintext,salt);
       std::string decrypted = UnsignedVectorToString(vchPlaintext);
       return decrypted;
    } catch (std::exception &e) 
    {
        printf("Error while decrypting %s",boinchash_encrypted.c_str());
        return "";
    }
}
