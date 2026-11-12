// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/autounlock_blob.h"

#include "tinyformat.h"
#include "util/system.h"

#include <algorithm>

namespace {
constexpr char MAGIC[] = "GRCAUB01";
constexpr size_t MAGIC_BYTES = sizeof(MAGIC) - 1;
constexpr size_t SCOPE_OFFSET = MAGIC_BYTES;
constexpr size_t NETWORK_LENGTH_OFFSET = SCOPE_OFFSET + 1;
constexpr size_t NETWORK_OFFSET = NETWORK_LENGTH_OFFSET + 1;
constexpr size_t MAX_NETWORK_BYTES = 16;

constexpr char ENTROPY_PREFIX[] = "gridcoin-autounlock-v1:";

bool IsScope(unsigned char scope)
{
    return scope == static_cast<unsigned char>(BlobScope::LocalMachine)
        || scope == static_cast<unsigned char>(BlobScope::ServiceAccount);
}

//! One to MAX_NETWORK_BYTES letters a-z, compared as bytes so that no locale is involved.
bool IsNetworkName(const std::string& network)
{
    if (network.empty() || network.size() > MAX_NETWORK_BYTES) return false;

    return std::all_of(network.begin(), network.end(), [](char c) { return c >= 'a' && c <= 'z'; });
}
} // namespace

std::vector<unsigned char> SerializeAutoUnlockBlob(const AutoUnlockBlob& blob)
{
    const unsigned char scope = static_cast<unsigned char>(blob.scope);
    const size_t size = NETWORK_OFFSET + blob.network.size() + blob.dpapi.size();

    if (!IsScope(scope) || !IsNetworkName(blob.network) || blob.dpapi.empty()) return {};
    if (size > AUTOUNLOCK_BLOB_MAX_BYTES) return {};

    std::vector<unsigned char> bytes;
    bytes.reserve(size);
    bytes.insert(bytes.end(), MAGIC, MAGIC + MAGIC_BYTES);
    bytes.push_back(scope);
    bytes.push_back(static_cast<unsigned char>(blob.network.size()));
    bytes.insert(bytes.end(), blob.network.begin(), blob.network.end());
    bytes.insert(bytes.end(), blob.dpapi.begin(), blob.dpapi.end());

    return bytes;
}

std::optional<AutoUnlockBlob> ParseAutoUnlockBlob(Span<const unsigned char> bytes)
{
    if (bytes.size() > AUTOUNLOCK_BLOB_MAX_BYTES || bytes.size() < NETWORK_OFFSET) return std::nullopt;
    if (!std::equal(MAGIC, MAGIC + MAGIC_BYTES, bytes.begin())) return std::nullopt;

    const unsigned char scope = bytes[SCOPE_OFFSET];
    const size_t network_bytes = bytes[NETWORK_LENGTH_OFFSET];

    if (!IsScope(scope)) return std::nullopt;

    // The name must fit, with a DPAPI blob of at least one byte after it.
    if (network_bytes > MAX_NETWORK_BYTES || bytes.size() <= NETWORK_OFFSET + network_bytes) return std::nullopt;

    const auto network_begin = bytes.begin() + NETWORK_OFFSET;
    const auto dpapi_begin = network_begin + network_bytes;

    AutoUnlockBlob blob;
    blob.scope = static_cast<BlobScope>(scope);
    blob.network.assign(network_begin, dpapi_begin);
    blob.dpapi.assign(dpapi_begin, bytes.end());

    if (!IsNetworkName(blob.network)) return std::nullopt;

    return blob;
}

std::vector<unsigned char> AutoUnlockEntropy(const std::string& network)
{
    std::vector<unsigned char> entropy(ENTROPY_PREFIX, ENTROPY_PREFIX + sizeof(ENTROPY_PREFIX) - 1);
    entropy.insert(entropy.end(), network.begin(), network.end());

    return entropy;
}

std::optional<fs::path> AutoUnlockBlobPath(std::string& error)
{
    error.clear();

    // -noautounlockblob reads as "0", and means the same as leaving the option out.
    const std::string value = gArgs.IsArgNegated("-autounlockblob") ? std::string()
                                                                     : gArgs.GetArg("-autounlockblob", "");

    if (value.empty()) return GetDataDir() / "autounlock" / "passphrase.dpapi";

    const fs::path path(value);

    if (!path.is_absolute()) {
        error = strprintf("-autounlockblob must be an absolute path, not \"%s\"", value);
        return std::nullopt;
    }

    return path;
}
