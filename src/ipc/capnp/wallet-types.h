// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_WALLET_TYPES_H
#define GRIDCOIN_IPC_CAPNP_WALLET_TYPES_H

#include <interfaces/wallet.h>

// uint256/CKeyID/CPubKey/SecureString <-> Data marshalling.
#include <ipc/capnp/common-types.h>

// The Wallet schema imports handler.capnp (handleX returns Handler) and
// node.capnp (handleStatusChanged reuses Node.VoidCallback); their proxy types
// must be in scope to marshal those.
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

#endif // GRIDCOIN_IPC_CAPNP_WALLET_TYPES_H
