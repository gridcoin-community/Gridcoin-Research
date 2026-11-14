// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/tls_libraries.h>

#include <curl/curl.h>
#include <openssl/crypto.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace {
std::atomic<bool> g_initialised{false};
} // anonymous namespace

namespace util {

TlsLibrariesScope::TlsLibrariesScope()
{
    if (g_initialised.exchange(true)) {
        return;
    }

    m_owner = true;

    // This has to be the process's first OpenSSL initialisation call. Once
    // OpenSSL has been told not to load its configuration, a later request to
    // load it, from libcurl or anything else, does nothing.
    if (OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, nullptr) != 1) {
        std::fputs("Error: OpenSSL failed to initialise.\n", stderr);
        std::exit(EXIT_FAILURE);
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        std::fputs("Error: libcurl failed to initialise.\n", stderr);
        std::exit(EXIT_FAILURE);
    }
}

TlsLibrariesScope::~TlsLibrariesScope()
{
    if (m_owner) {
        curl_global_cleanup();
    }
}

bool TlsLibrariesScope::Initialised()
{
    return g_initialised.load();
}

} // namespace util
