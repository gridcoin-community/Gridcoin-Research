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
//! Windows counterpart of the POSIX chmod 0700/0600 protection the IPC layer
//! applies to the socket directory, the socket and the cookie file: replace the
//! object's DACL with a *protected* one (inheritance broken:
//! PROTECTED_DACL_SECURITY_INFORMATION) that grants full access to this
//! process's token user and to SYSTEM, and to nobody else -- no Everyone, Users,
//! Authenticated Users or Administrators. The result is then read back and
//! verified, so a filesystem that ignores ACLs cannot leave the secret exposed
//! while reporting success.
//!
//! Declared here (rather than kept local to process.cpp) because handshake.cpp
//! needs the identical treatment for ipc.cookie.
//!
//! \p inheritable makes the two ACEs inheritable by the container's children --
//! use it for the socket *directory* so files created inside it (node.sock,
//! ipc.cookie) are owner-only from the instant they exist, rather than only once
//! this is applied to them after the fact.
//!
//! Fails closed: throws std::system_error (or std::runtime_error when the
//! read-back does not match) instead of logging and continuing, exactly as the
//! POSIX chmod paths do.
void ApplyOwnerOnlyDacl(const fs::path& path, bool inheritable);

//! The DACL ApplyOwnerOnlyDacl installs, for callers that need it at *create*
//! time -- a SECURITY_ATTRIBUTES handed to CreateFileW, which is the Windows
//! analogue of POSIX's mode 0600 on open() and leaves no window in which the
//! object exists unprotected. Returns a PACL, typed as void* only so this header
//! does not have to drag <windows.h> into every consumer of ipc/process.h (see
//! the include-order warning in src/ipc/CMakeLists.txt). The caller owns the
//! result and must ::LocalFree() it. Throws on failure; never returns nullptr.
//!
//! NOTE for callers: a security descriptor passed to CreateFileW is applied only
//! when the file is actually created, and inheritable ACEs from the parent
//! directory are merged in unless the descriptor's SE_DACL_PROTECTED control bit
//! is set. Open with CREATE_NEW (not CREATE_ALWAYS, which keeps an existing
//! file's ACL) and set SE_DACL_PROTECTED.
void* MakeOwnerOnlyDacl(bool inheritable);
#endif // WIN32
} // namespace ipc

#endif // GRIDCOIN_IPC_PROCESS_H
