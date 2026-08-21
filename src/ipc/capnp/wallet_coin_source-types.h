// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_WALLET_COIN_SOURCE_TYPES_H
#define GRIDCOIN_IPC_CAPNP_WALLET_COIN_SOURCE_TYPES_H

// Pulls in interfaces::WalletCoinSource plus the GRC:: coin channel value
// types (wallet_coin_channel.h: CoinRecord / CoinGroupInfo / the results /
// the event payloads).
#include <interfaces/wallet_coin_source.h>

// uint256 <-> Data marshalling, reached through COutPoint::hash.
#include <ipc/capnp/common-types.h>

// COutPoint is wrapped by wallet.capnp's OutPoint, which this schema imports
// and uses rather than declaring a second wrapper for the same C++ type.
#include <ipc/capnp/wallet.capnp.proxy-types.h>

#include <ipc/capnp/wallet_coin_source.capnp.proxy.h>

// proxy-types.h (ReadDestUpdate/ReadDestEmplace/ProxyStruct) precedes the
// container/variant type headers that use it.
#include <mp/proxy-types.h>

// WalletCoinEventPayload (std::variant) <-> WalletCoinEventPayloadWire
// discriminated struct. Uses the mp field machinery, so it follows
// proxy-types.h.
#include <ipc/capnp/type-variant.h>

#include <mp/type-context.h>
#include <mp/type-data.h>
#include <mp/type-decay.h>
#include <mp/type-interface.h>
#include <mp/type-number.h>
#include <mp/type-pair.h>
// reconcileSelection takes and returns std::set<COutPoint>.
#include <mp/type-set.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>
#include <mp/type-vector.h>

#endif // GRIDCOIN_IPC_CAPNP_WALLET_COIN_SOURCE_TYPES_H
