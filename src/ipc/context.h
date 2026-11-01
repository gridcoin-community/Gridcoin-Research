// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CONTEXT_H
#define GRIDCOIN_IPC_CONTEXT_H

namespace ipc {
//! Context struct giving IPC protocol implementations access to application
//! state, in case a hook needs to run extra code that is not needed within a
//! single process. Empty for now; a place to hang shared state later.
struct Context
{
};
} // namespace ipc

#endif // GRIDCOIN_IPC_CONTEXT_H
