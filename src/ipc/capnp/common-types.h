// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H
#define GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H

#include <pubkey.h>
#include <support/allocators/secure.h>
#include <support/cleanse.h>
#include <uint256.h>

#include <mp/proxy-types.h>
#include <mp/util.h>

#include <cstring>
#include <stdexcept>
#include <vector>

//! Custom marshalling for Gridcoin value types that cross the IPC boundary but
//! are not plain capnp structs. Included (via each schema's
//! $Proxy.includeTypes -> *-types.h) so the generated proxy code can build/read
//! them. Lives in namespace mp so ADL finds these next to the library's own
//! CustomBuildField/CustomReadField overloads.
namespace mp {

//! uint256 is marshalled as its 32 raw bytes in a capnp Data field. (A blob
//! rather than a $Proxy.wrap struct: it is an opaque fixed-width hash, not a
//! record of named fields.)
template <typename Value, typename Output>
void CustomBuildField(TypeList<uint256>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    auto result = output.init(value.size());
    std::memcpy(result.begin(), value.begin(), value.size());
}

template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<uint256>, Priority<1>, InvokeContext& invoke_context, Input&& input, ReadDest&& read_dest)
{
    auto data = input.get();
    // The Data field comes off the wire (untrusted in the separated build). The
    // uint256(vector) constructor asserts the length and then memcpy's a fixed
    // 32 bytes, so a short buffer would read out of bounds once asserts are
    // compiled out (NDEBUG). Validate the length here and fail fast on mismatch.
    if (data.size() != uint256::size()) {
        throw std::runtime_error("uint256 IPC field has wrong length");
    }
    return read_dest.construct(std::vector<unsigned char>(data.begin(), data.end()));
}

//! CKeyID is a uint160 (20 raw bytes), marshalled as its bytes in a Data field
//! like uint256. Only key *identifiers* and public keys cross the boundary --
//! private key material never does (see interfaces::Wallet).
template <typename Value, typename Output>
void CustomBuildField(TypeList<CKeyID>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    auto result = output.init(value.size());
    std::memcpy(result.begin(), value.begin(), value.size());
}

template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<CKeyID>, Priority<1>, InvokeContext& invoke_context, Input&& input, ReadDest&& read_dest)
{
    auto data = input.get();
    if (data.size() != CKeyID::size()) {
        throw std::runtime_error("CKeyID IPC field has wrong length");
    }
    return read_dest.construct(uint160(std::vector<unsigned char>(data.begin(), data.end())));
}

//! CPubKey is a variable-length (33- or 65-byte) public key, marshalled as its
//! serialized bytes in a Data field. The read side reconstructs via the
//! two-iterator constructor, which self-validates the length and marks the key
//! invalid on a malformed (untrusted) buffer.
template <typename Value, typename Output>
void CustomBuildField(TypeList<CPubKey>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    auto result = output.init(value.size());
    std::memcpy(result.begin(), value.begin(), value.size());
}

template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<CPubKey>, Priority<1>, InvokeContext& invoke_context, Input&& input, ReadDest&& read_dest)
{
    auto data = input.get();
    return read_dest.construct(data.begin(), data.end());
}

//! SecureString (the wallet passphrase) is marshalled as raw bytes in a Data
//! field. Bespoke rather than reusing the std::string hook so the plaintext
//! copies are explicitly scrubbed: the read side constructs the SecureString
//! (whose secure_allocator zeroes on destruction) and then memory_cleanse()s the
//! received wire buffer, so no plaintext lingers in the node's message arena.
//! (The send-side capnp segment is not scrubbed here -- libmultiprocess exposes
//! no post-send per-message hook -- but it lives only in the short-lived request
//! arena of the calling process, which already holds the passphrase the user
//! just typed.)
template <typename Value, typename Output>
void CustomBuildField(TypeList<SecureString>, Priority<1>, InvokeContext& invoke_context, Value&& value, Output&& output)
{
    auto result = output.init(value.size());
    std::memcpy(result.begin(), value.data(), value.size());
}

template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<SecureString>, Priority<1>, InvokeContext& invoke_context, Input&& input, ReadDest&& read_dest)
{
    auto data = input.get();
    decltype(auto) result = read_dest.construct(CharCast(data.begin()), data.size());
    memory_cleanse(const_cast<char*>(CharCast(data.begin())), data.size());
    return result;
}

} // namespace mp

#endif // GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H
