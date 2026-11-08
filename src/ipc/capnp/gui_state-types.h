// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_GUI_STATE_TYPES_H
#define GRIDCOIN_IPC_CAPNP_GUI_STATE_TYPES_H

#include <interfaces/gui_state.h>

// The GuiState schema imports handler.capnp (the handleX methods return
// Handler) and the schemas of the structs its snapshot nests (mrc, researcher,
// psgt, wallet); their proxy types must be in scope to marshal those.
#include <ipc/capnp/handler.capnp.proxy-types.h>
#include <ipc/capnp/mrc.capnp.proxy-types.h>
#include <ipc/capnp/psgt.capnp.proxy-types.h>
#include <ipc/capnp/researcher.capnp.proxy-types.h>
#include <ipc/capnp/wallet.capnp.proxy-types.h>

#include <ipc/capnp/gui_state.capnp.proxy.h>

// proxy-types.h (ReadDestUpdate/ReadDestEmplace) precedes the container type
// headers that use it.
#include <mp/proxy-types.h>

// The callbacks (type-function), the returned Handler (type-interface), the
// snapshot and its nested structs (type-struct), their scalars and Text, and the
// researcher part's std::optional member (type-optional).
#include <mp/type-context.h>
#include <mp/type-decay.h>
#include <mp/type-function.h>
#include <mp/type-interface.h>
#include <mp/type-number.h>
#include <mp/type-optional.h>
#include <mp/type-string.h>
#include <mp/type-struct.h>

#endif // GRIDCOIN_IPC_CAPNP_GUI_STATE_TYPES_H
