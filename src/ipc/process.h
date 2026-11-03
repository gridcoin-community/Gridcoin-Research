// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PROCESS_H
#define GRIDCOIN_IPC_PROCESS_H

#include "fs.h"

#include <memory>
#include <optional>
#include <string>
#ifndef WIN32
#include <sys/types.h>
#endif

namespace ipc {

//! Platform IPC process helper: acquires an AF_UNIX socket descriptor by
//! connecting to, or binding+listening on, a named socket. (Gridcoin does not
//! spawn child processes, so unlike Bitcoin Core's Process there is no
//! spawn/checkSpawned here.)
class Process
{
public:
    virtual ~Process() = default;

    //! Canonicalize and connect to \p address under \p ipc_dir, returning the
    //! connected socket descriptor.
    virtual int connect(const fs::path& ipc_dir, std::string& address) = 0;

    //! Canonicalize \p address under \p ipc_dir, create + bind a listening
    //! socket, and return its descriptor. Removes a stale socket file left by a
    //! crash; throws if the path is already bound by a live listener. On POSIX
    //! the socket is left 0600, or 0660 in the attach group when one is
    //! configured.
    virtual int bind(const fs::path& ipc_dir, std::string& address) = 0;
};

//! Constructor for the Process interface.
std::unique_ptr<Process> MakeProcess();

//! -ipcdir if set (AbsPathForConfigVal against the network datadir), else
//! GetDataDir(). Used verbatim: no network suffix is appended to an explicit
//! value. The IPC socket and cookie live here.
fs::path GetIpcDir();

//! Make sure \p dir exists. When this call creates it, it is restricted to this
//! account (POSIX mode 0700 through an fd, Windows owner-only protected DACL)
//! and failing to do so throws; an existing directory is left as configured,
//! with a warning in the log if it is accessible to other users. Throws
//! std::system_error, naming the directory, if it cannot be created.
void PrepareIpcDirectory(const fs::path& dir);

#ifndef WIN32
//! What ApplyPathAccessNoFollow expects to find at the path.
enum class PathKind { Socket, Directory, RegularFile };

//! Set the mode and (optionally) the group of an existing path without following
//! a symlink at it, through an fd of the path.
//! Linux: open(O_PATH|O_NOFOLLOW|O_CLOEXEC), fstat, fchownat(fd,"",-1,gid,AT_EMPTY_PATH),
//! fchmodat2(fd,"",mode,AT_EMPTY_PATH) when SYS_fchmodat2 is defined and succeeds,
//! else chmod("/proc/self/fd/<fd>", mode).
//! Other POSIX: fchownat/fchmodat(AT_FDCWD, path, ..., AT_SYMLINK_NOFOLLOW).
//! Afterwards the path (fstat on the fd on Linux, lstat elsewhere) must show the expected type, st_uid == geteuid(),
//! mode & 07777 == mode, and the group when given; otherwise throws
//! std::system_error.
void ApplyPathAccessNoFollow(const fs::path& path, PathKind kind, mode_t mode, std::optional<gid_t> group);

//! With -ipcattachgroup configured (ipc::AttachGroupGid()), give the IPC
//! directory \p ipcdir mode 0750 and that group through an fd, so members of the
//! group can reach the socket and cookie inside it. Throws std::logic_error when
//! no attach group is configured, and std::system_error when the access cannot be
//! applied; when the group cannot be set because this account is not a member,
//! the message says the core must run with the attach group as a supplementary
//! group.
void ApplyAttachGroupAccess(const fs::path& ipcdir);
#endif // !WIN32
} // namespace ipc

#endif // GRIDCOIN_IPC_PROCESS_H
