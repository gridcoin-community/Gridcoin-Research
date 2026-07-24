// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_WALLET_TX_SOURCE_TYPES_H
#define GRIDCOIN_IPC_CAPNP_WALLET_TX_SOURCE_TYPES_H

// Pulls in interfaces::WalletTxSource plus the GRC:: channel/filter/record
// value types (wallet_tx_channel.h -> wallet_tx_filter.h + wallet_tx_record.h).
#include <interfaces/wallet_tx_source.h>

// uint256 <-> Data marshalling (TransactionRecord.hash, the rowForKey/
// getRowDetail hash params).
#include <ipc/capnp/common-types.h>

#include <ipc/capnp/wallet_tx_source.capnp.proxy.h>

// proxy-types.h (ReadDestUpdate/ReadDestEmplace/ProxyStruct) precedes the
// container/variant type headers that use it.
#include <mp/proxy-types.h>

// WalletEventPayload (std::variant) <-> WalletEventPayloadWire discriminated
// struct. Uses the mp field machinery, so it follows proxy-types.h.
#include <ipc/capnp/type-variant.h>

#include <mp/type-context.h>
#include <mp/type-data.h>
#include <mp/type-decay.h>
#include <mp/type-interface.h>
#include <mp/type-number.h>
#include <mp/type-pair.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>
#include <mp/type-vector.h>

#endif // GRIDCOIN_IPC_CAPNP_WALLET_TX_SOURCE_TYPES_H
