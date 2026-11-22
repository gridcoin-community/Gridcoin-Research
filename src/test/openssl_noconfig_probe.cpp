// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! Checks that util::TlsLibrariesScope keeps OpenSSL from reading a
//! configuration file.
//!
//!     openssl_noconfig_probe <conf> [--control]
//!
//! The file named activates the built-in "null" provider. The probe points
//! OPENSSL_CONF at it, constructs the scope, then asks OpenSSL to load its
//! configuration the way a library used later in the process would, and passes
//! if the null provider is absent. With --control it leaves the scope out and
//! passes if the provider is present, which shows that the file and the check
//! work. It needs OpenSSL 3 for providers and returns 77 (skipped) without it.

#include <util/tls_libraries.h>

#include <openssl/crypto.h>
#include <openssl/opensslv.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#endif

int main(int argc, char* argv[])
{
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    (void)argc;
    (void)argv;
    return 77;
#else
    if (argc < 2 || argc > 3 || (argc == 3 && std::strcmp(argv[2], "--control") != 0)) {
        std::fputs("usage: openssl_noconfig_probe <conf> [--control]\n", stderr);
        return 2;
    }

    const bool control = argc == 3;

    if (setenv("OPENSSL_CONF", argv[1], 1) != 0) {
        std::perror("setenv");
        return 2;
    }

    if (control) {
        if (util::TlsLibrariesScope::Initialised()) {
            std::fputs("control run found the libraries initialised\n", stderr);
            return 1;
        }

        OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, nullptr);
        const int loaded = OSSL_PROVIDER_available(nullptr, "null");
        std::puts(loaded == 1 ? "control: the null provider is available, as expected"
                              : "control: the null provider is NOT available; the configuration file was not read");
        return loaded == 1 ? 0 : 1;
    }

    const util::TlsLibrariesScope tls_libraries;

    if (!util::TlsLibrariesScope::Initialised()) {
        std::fputs("the scope did not report the libraries initialised\n", stderr);
        return 1;
    }

    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, nullptr);
    const int loaded = OSSL_PROVIDER_available(nullptr, "null");
    std::puts(loaded == 0 ? "the null provider is not available, as expected"
                          : "the null provider IS available; the configuration file was read");
    return loaded == 0 ? 0 : 1;
#endif
}
