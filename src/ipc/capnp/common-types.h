// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H
#define GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H

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

} // namespace mp

#endif // GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H
