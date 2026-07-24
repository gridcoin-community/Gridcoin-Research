// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_MRC_TYPES_H
#define GRIDCOIN_IPC_CAPNP_MRC_TYPES_H

#include <interfaces/mrc.h>

// The MRC schema imports handler.capnp (handleMRCChanged returns Handler) and
// node.capnp (the callback reuses Node.VoidCallback); their proxy types must be
// in scope to marshal those.
#include <ipc/capnp/handler.capnp.proxy-types.h>
#include <ipc/capnp/node.capnp.proxy-types.h>

#include <ipc/capnp/mrc.capnp.proxy.h>

#include <mp/proxy-types.h>

#include <mp/type-context.h>
#include <mp/type-decay.h>
#include <mp/type-function.h>
#include <mp/type-interface.h>
#include <mp/type-number.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>

#endif // GRIDCOIN_IPC_CAPNP_MRC_TYPES_H
