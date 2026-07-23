// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_NODE_TYPES_H
#define GRIDCOIN_IPC_CAPNP_NODE_TYPES_H

#include <interfaces/node.h>

// uint256 <-> Data marshalling, and the Handler capability the handleX methods
// return.
#include <ipc/capnp/common-types.h>
#include <ipc/capnp/handler.capnp.proxy-types.h>

#include <ipc/capnp/node.capnp.proxy.h>

// proxy-types.h (ReadDestUpdate/ReadDestEmplace) must precede the container type
// headers (optional/struct/pair/vector) that use it via CTAD.
#include <mp/proxy-types.h>

// Type-marshalling support for the capnp types this schema uses.
#include <mp/type-context.h>
#include <mp/type-data.h>
#include <mp/type-decay.h>
#include <mp/type-function.h>
#include <mp/type-interface.h>
#include <mp/type-number.h>
#include <mp/type-optional.h>
#include <mp/type-pair.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>
#include <mp/type-vector.h>

#endif // GRIDCOIN_IPC_CAPNP_NODE_TYPES_H
