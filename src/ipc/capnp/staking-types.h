// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_STAKING_TYPES_H
#define GRIDCOIN_IPC_CAPNP_STAKING_TYPES_H

#include <interfaces/staking.h>
#include <ipc/capnp/staking.capnp.proxy.h>

// proxy-types.h defines ReadDestUpdate/ReadDestEmplace, which the container type
// headers below (type-optional here) use via CTAD; it must precede them.
#include <mp/proxy-types.h>

// Type-marshalling support: Bool/Float64 scalars (type-number), Text<->std::string
// (type-string), std::optional<double> results (type-optional), and the
// framework Context.
#include <mp/type-context.h>
#include <mp/type-decay.h>
#include <mp/type-number.h>
#include <mp/type-optional.h>
#include <mp/type-string.h>

#endif // GRIDCOIN_IPC_CAPNP_STAKING_TYPES_H
