// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_GRIDCOIN_RSA_TEST_VECTORS_H
#define GRIDCOIN_TEST_GRIDCOIN_RSA_TEST_VECTORS_H

#include <string>

//! RSA-SHA512 test vectors shared by the ownership proof tests.
namespace rsa_test_vectors {

// RSA-2048 test key pair generated for unit testing.
// Private key is NOT included; only the public key is needed for verification.
inline const std::string g_test_pem_pubkey =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA3iZqXyed5wFMz/NDvSH0\n"
    "N0TXz4Htecekt0wmTxFPDFQF0Oz8Mp1l+Ig+X//d+a/EHRt/AitRQoCz0A1HilDX\n"
    "fZT3WOMaKxWgbR8SohhvunXy5Ke8/QQUe++je2zW9rR/qy6T9PnuQFcQ/bGbwNUn\n"
    "+zATQ4uPcbyjCbmKuKpidj7lbvFlP2yqRBFpmWsP9yRBc8FIwuNjmBgwIiasz/AA\n"
    "bfjeTRdpin1vMlZRc/wl2VDcTIdO6DaU1EJic3SoznV0DlgtnXF7is36RvkAQXQM\n"
    "08kgOwFAaeVBusbeD94FKkVI1+DDBHchoyB9Pf5pGjCmMYLBpxkelQWiGexxMLAY\n"
    "AwIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

// Test message: "19100117 abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab"
// This simulates the BOINC ownership proof message format: "{account_id} {beacon_public_key_hex}"
inline const std::string g_test_message =
    "19100117 abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab";

// RSA-SHA512 signature of g_test_message using the corresponding private key.
inline const std::string g_test_signature_hex =
    "0be92363c95401588c910a3425413cf6dc9fe512fe7c0cc1520336e9014539d947ab46301c2c9c93d6594ea15584445e426e44d11196265e9d23ca6a089d5c08f298e07dfacb825734f607982e74dcb0e5c50c6b27bac3578dd7bb2b7212a72de69527217b7035c618395eeb5b7dafcf42f654131b2a15d996585b3558898a384f450cc88b7563b284f244418a82f6b512543f5730361187b93c579d4cac1a54fb851e8d602753c6ffff99686a6bb32d8f616578bc401050e330f42beeb393e5182e9b3a6920e2b0d6f83a519ad28fa347d056d50ac6119790b438e03e8c8de7e232ab8650b65b1ecbf58eb433e60ac01f54d0b2660502c8119cea4dbfa3405c";

} // namespace rsa_test_vectors

#endif // GRIDCOIN_TEST_GRIDCOIN_RSA_TEST_VECTORS_H
