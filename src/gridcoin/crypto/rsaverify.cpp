// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/crypto/rsaverify.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>

namespace GRC {

bool VerifyRSASHA512(
    const std::vector<uint8_t>& message,
    const std::vector<uint8_t>& signature,
    const std::string& pem_pubkey)
{
    if (message.empty() || signature.empty() || pem_pubkey.empty()) {
        return false;
    }

    // Parse the PEM public key via a memory BIO.
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(
        BIO_new_mem_buf(pem_pubkey.data(), static_cast<int>(pem_pubkey.size())),
        BIO_free);

    if (!bio) {
        return false;
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(
        PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr),
        EVP_PKEY_free);

    if (!pkey) {
        return false;
    }

    // Create and initialize the digest verification context.
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(
        EVP_MD_CTX_new(),
        EVP_MD_CTX_free);

    if (!ctx) {
        return false;
    }

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha512(), nullptr, pkey.get()) != 1) {
        return false;
    }

    if (EVP_DigestVerifyUpdate(ctx.get(), message.data(), message.size()) != 1) {
        return false;
    }

    return EVP_DigestVerifyFinal(ctx.get(), signature.data(), signature.size()) == 1;
}

std::vector<uint8_t> GetPublicKeyDER(const std::string& pem_pubkey)
{
    if (pem_pubkey.empty()) {
        return {};
    }

    std::unique_ptr<BIO, decltype(&BIO_free)> bio(
        BIO_new_mem_buf(pem_pubkey.data(), static_cast<int>(pem_pubkey.size())),
        BIO_free);

    if (!bio) {
        return {};
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(
        PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr),
        EVP_PKEY_free);

    if (!pkey) {
        return {};
    }

    const int length = i2d_PUBKEY(pkey.get(), nullptr);

    if (length <= 0) {
        return {};
    }

    std::vector<uint8_t> der(length);
    unsigned char* out = der.data();

    if (i2d_PUBKEY(pkey.get(), &out) != length) {
        return {};
    }

    return der;
}

} // namespace GRC
