// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PROCESS_H
#define GRIDCOIN_IPC_PROCESS_H

#include "fs.h"

#include <memory>
#include <string>

namespace ipc {

//! Platform IPC process helper: acquires an AF_UNIX socket descriptor by
//! connecting to, or binding+listening on, a named socket. (Gridcoin does not
//! spawn child processes, so unlike Bitcoin Core's Process there is no
//! spawn/checkSpawned here.)
class Process
{
public:
    virtual ~Process() = default;

    //! Canonicalize and connect to \p address under \p data_dir, returning the
    //! connected socket descriptor.
    virtual int connect(const fs::path& data_dir, std::string& address) = 0;

    //! Canonicalize \p address under \p data_dir, create + bind a listening
    //! socket, and return its descriptor. Removes a stale socket file left by a
    //! crash; throws if the path is already bound by a live listener.
    virtual int bind(const fs::path& data_dir, std::string& address) = 0;
};

//! Constructor for the Process interface.
std::unique_ptr<Process> MakeProcess();

#ifdef WIN32

#endif // WIN32
} // namespace ipc

#endif // GRIDCOIN_IPC_PROCESS_H
