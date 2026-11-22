// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_TLS_LIBRARIES_H
#define GRIDCOIN_UTIL_TLS_LIBRARIES_H

namespace util {

//! Initialises OpenSSL and libcurl for the process, in that order, and cleans
//! libcurl up when the process ends.
//!
//! OpenSSL is initialised without its configuration file. The first
//! OPENSSL_init_crypto() call in a process decides whether openssl.cnf is read,
//! so the program makes that first call itself, in one place. libcurl's own
//! start-up reaches OpenSSL too, which is why it follows here and not in a
//! static object of the HTTP code.
//!
//! The first instance in a process does the work and owns the clean-up. Later
//! instances do nothing. On failure it writes one line to stderr and exits,
//! because logging is not up yet where this runs.
class TlsLibrariesScope
{
public:
    TlsLibrariesScope();
    ~TlsLibrariesScope();

    TlsLibrariesScope(const TlsLibrariesScope&) = delete;
    TlsLibrariesScope& operator=(const TlsLibrariesScope&) = delete;

    //! Whether an instance has initialised the libraries in this process.
    static bool Initialised();

private:
    bool m_owner{false};
};

} // namespace util

#endif // GRIDCOIN_UTIL_TLS_LIBRARIES_H
