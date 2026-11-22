// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_FILE_TRANSFER_TYPES_H
#define GRIDCOIN_IPC_CAPNP_FILE_TRANSFER_TYPES_H

#include <interfaces/file_transfer.h>

// uint256 <-> Data marshalling (FileTrailer.sha256, FileSink.finish).
#include <ipc/capnp/common-types.h>

#include <ipc/capnp/file_transfer.capnp.proxy.h>

// proxy-types.h (ReadDestUpdate/ReadDestEmplace) precedes the container type
// headers that use it.
#include <mp/proxy-types.h>

#include <mp/type-context.h>
#include <mp/type-data.h>
#include <mp/type-decay.h>
#include <mp/type-number.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>
#include <mp/type-vector.h>

#endif // GRIDCOIN_IPC_CAPNP_FILE_TRANSFER_TYPES_H
