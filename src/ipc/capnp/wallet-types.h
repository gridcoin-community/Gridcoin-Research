// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_WALLET_TYPES_H
#define GRIDCOIN_IPC_CAPNP_WALLET_TYPES_H

#include <interfaces/wallet.h>

// uint256/CKeyID/CPubKey/SecureString <-> Data marshalling.
#include <ipc/capnp/common-types.h>

#include <wallet/capability.h>

// The Wallet schema imports file_transfer.capnp (beginBackup returns a
// FileSource), handler.capnp (handleX returns Handler) and node.capnp
// (handleStatusChanged reuses Node.VoidCallback); their proxy types must be in
// scope to marshal those.
#include <ipc/capnp/file_transfer.capnp.proxy-types.h>
#include <ipc/capnp/handler.capnp.proxy-types.h>
#include <ipc/capnp/node.capnp.proxy-types.h>

#include <ipc/capnp/wallet.capnp.proxy.h>

// proxy-types.h (ReadDestUpdate/ReadDestEmplace) precedes the container type
// headers that use it.
#include <mp/proxy-types.h>

#include <mp/type-context.h>
#include <mp/type-data.h>
#include <mp/type-decay.h>
#include <mp/type-function.h>
#include <mp/type-interface.h>
#include <mp/type-map.h>
#include <mp/type-number.h>
#include <mp/type-optional.h>
#include <mp/type-pair.h>
#include <mp/type-set.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>
#include <mp/type-vector.h>

#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace mp {

//! CapabilityKind crosses as a UInt8 (wallet.capnp beginElevation). The
//! library's generic enum reader casts whatever integer arrives, and every value
//! of the underlying byte is a value of the enumeration, so a kind this build
//! does not know would reach the wallet as if it were one. Refused here
//! instead: the call fails on the server, and the connection goes on serving.
template <typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<CapabilityKind>, Priority<2>, InvokeContext& invoke_context, Input&& input,
                               ReadDest&& read_dest)
{
    const auto value = input.get();
    static_assert(std::is_same_v<std::decay_t<decltype(value)>, uint8_t>,
                  "CapabilityKind crosses as UInt8, the width of its underlying type");

    if (!IsKnownCapabilityKind(static_cast<CapabilityKind>(value))) {
        throw std::runtime_error("CapabilityKind IPC field out of range");
    }

    return read_dest.construct(static_cast<CapabilityKind>(value));
}

} // namespace mp

#endif // GRIDCOIN_IPC_CAPNP_WALLET_TYPES_H
