// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_AUTOUNLOCK_BLOB_H
#define GRIDCOIN_WALLET_AUTOUNLOCK_BLOB_H

#include "fs.h"
#include "span.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

//! The file that holds the wallet passphrase for autounlock on Windows, encrypted with
//! DPAPI. The core reads and writes it, and GridcoinCommon.ps1 writes it too, so its
//! layout is fixed:
//!
//!   bytes 0-7        "GRCAUB01"
//!   byte 8           the scope (BlobScope)
//!   byte 9           the length N of the network name, 1 to 16
//!   bytes 10-(9+N)   the network name (Params().NetworkIDString()), letters a-z
//!   the rest         the DPAPI blob, never empty
//!
//! The whole file is at most AUTOUNLOCK_BLOB_MAX_BYTES long. The DPAPI entropy is
//! AutoUnlockEntropy(network), and the plaintext is the passphrase's bytes as typed
//! (UTF-8).

//! Whose DPAPI key encrypts the passphrase.
enum class BlobScope : uint8_t {
    //! The machine's key (CRYPTPROTECT_LOCAL_MACHINE), for a core that runs as the
    //! wallet's own account. Every account on the machine can decrypt it, so only the
    //! file's access control list keeps it to that account.
    LocalMachine = 1,
    //! The key of the account the Windows service runs as.
    ServiceAccount = 2,
};

//! The autounlock file's content.
struct AutoUnlockBlob
{
    BlobScope scope{BlobScope::LocalMachine};
    std::string network;
    std::vector<unsigned char> dpapi;
};

constexpr size_t AUTOUNLOCK_BLOB_MAX_BYTES = 65536;

//! \p blob in the file layout. Empty when the layout cannot hold it: a scope that is not
//! a BlobScope, a network name that is not 1 to 16 letters a-z, an empty DPAPI blob, or
//! a result longer than AUTOUNLOCK_BLOB_MAX_BYTES.
std::vector<unsigned char> SerializeAutoUnlockBlob(const AutoUnlockBlob& blob);

//! The content of a file in the layout. nullopt for anything else: more than
//! AUTOUNLOCK_BLOB_MAX_BYTES, another magic, a scope that is not a BlobScope, a network
//! name that is not 1 to 16 letters a-z, or no DPAPI blob after it.
std::optional<AutoUnlockBlob> ParseAutoUnlockBlob(Span<const unsigned char> bytes);

//! The DPAPI entropy for \p network: the ASCII bytes of "gridcoin-autounlock-v1:"
//! followed by \p network.
std::vector<unsigned char> AutoUnlockEntropy(const std::string& network);

//! Where the autounlock file is. -autounlockblob=<absolute path> names it. Given without
//! a value, negated, or not given at all (as when autounlock is being set up), it is
//! GetDataDir()/autounlock/passphrase.dpapi. A relative path is refused: nullopt, with
//! \p error naming the value.
std::optional<fs::path> AutoUnlockBlobPath(std::string& error);

#endif // GRIDCOIN_WALLET_AUTOUNLOCK_BLOB_H
