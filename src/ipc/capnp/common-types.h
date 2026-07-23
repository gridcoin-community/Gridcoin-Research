// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H
#define GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H

#include <uint256.h>

#include <mp/proxy-types.h>
#include <mp/util.h>

#include <cstring>
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
    return read_dest.construct(std::vector<unsigned char>(data.begin(), data.end()));
}

} // namespace mp

#endif // GRIDCOIN_IPC_CAPNP_COMMON_TYPES_H
