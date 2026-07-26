// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_INIT_TYPES_H
#define GRIDCOIN_IPC_CAPNP_INIT_TYPES_H

#include <interfaces/init.h>

// Init hands out other interface capabilities, so it needs their proxy-types in
// scope to build/read the returned capabilities.
#include <ipc/capnp/node.capnp.proxy-types.h>
#include <ipc/capnp/staking.capnp.proxy-types.h>
#include <ipc/capnp/wallet.capnp.proxy-types.h>
#include <ipc/capnp/wallet_tx_source.capnp.proxy-types.h>
#include <ipc/capnp/mrc.capnp.proxy-types.h>
#include <ipc/capnp/voting.capnp.proxy-types.h>
#include <ipc/capnp/researcher.capnp.proxy-types.h>
#include <ipc/capnp/psgt.capnp.proxy-types.h>
#include <ipc/capnp/sidestake.capnp.proxy-types.h>

#include <ipc/capnp/init.capnp.proxy.h>

#include <mp/proxy-types.h>

// Type-marshalling support: the construct() ThreadMap exchange (type-threadmap),
// returned interface capabilities (type-interface), Bool/UInt32 scalars
// (type-number), Text (type-string), the BuildInfo/NodeIdentity value structs
// (type-struct), and the framework Context.
#include <mp/type-context.h>
#include <mp/type-decay.h>
#include <mp/type-interface.h>
#include <mp/type-number.h>
// makeWalletTxSource returns a std::shared_ptr<WalletTxSource> capability.
#include <mp/type-pointer.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>
#include <mp/type-threadmap.h>

#endif // GRIDCOIN_IPC_CAPNP_INIT_TYPES_H
