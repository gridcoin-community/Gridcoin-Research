// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PROCESS_H
#define GRIDCOIN_IPC_PROCESS_H

#include "fs.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#ifndef WIN32
#include <sys/stat.h>
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

//! The reason the core refuses to start when -ipcattachgroup is set and -ipcdir
//! is not, or names the data directory, or nullopt. The attach group is given
//! access to the IPC directory, and without -ipcdir that is the data directory.
//! Both options are read as ConfiguredOption() reads them, so a negated or empty
//! value counts as unset. An -ipcdir is compared with the data directory, and on
//! a test network also with the base data directory above it, with the links in
//! both resolved; one that does not exist yet is not the data directory. nullopt
//! on Windows, where ConfigureAttachGroup() refuses any attach group.
std::optional<std::string> AttachGroupIpcDirRefusal();

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
//! else chmod("/proc/self/fd/<fd>", mode). When a group is given, the mode is first
//! narrowed to (current mode & mode), so the new group never holds a bit the final
//! mode does not grant, then the group is changed, then the final mode is set.
//! Other POSIX: fchownat/fchmodat(AT_FDCWD, path, ..., AT_SYMLINK_NOFOLLOW).
//! Afterwards the path (fstat on the fd on Linux, lstat elsewhere) must show the expected type, st_uid == geteuid(),
//! mode & 07777 == mode, and the group when given; otherwise throws
//! std::system_error.
//!
//! When given, \p check is called with what that fstat (lstat) reports for the
//! path as opened, once the type and owner have been checked and before
//! anything is changed. It throws to refuse the path, and the exception
//! propagates.
void ApplyPathAccessNoFollow(const fs::path& path, PathKind kind, mode_t mode, std::optional<gid_t> group,
                             const std::function<void(const struct stat&)>& check = {});

//! The warning PrepareIpcDirectory logs for an existing IPC directory with the
//! given mode, owner and group, or nullopt when there is nothing to say: the
//! directory gives no access to group or others, or it is exactly the state
//! ApplyAttachGroupAccess leaves it in (mode 0750, \p attach_gid, owned by
//! \p self_uid). The text says whether the socket and cookie inside it are
//! owner-only or accessible to the attach group.
std::optional<std::string> IpcDirWarning(const std::string& dir, mode_t mode, uid_t owner, gid_t group,
                                         uid_t self_uid, std::optional<gid_t> attach_gid);

//! With -ipcattachgroup configured (ipc::AttachGroupGid()), give the IPC
//! directory \p ipcdir mode 0750 and that group through an fd, so members of the
//! group can reach the socket and cookie inside it. Throws std::logic_error when
//! no attach group is configured, and std::system_error when the access cannot be
//! applied; when the group cannot be set because this account is not a member,
//! the message says the core must run with the attach group as a supplementary
//! group. A directory that opens as the network data directory, or the base one
//! above it (the same device and inode), is refused before anything is changed,
//! with a std::runtime_error naming -ipcattachgroup and -ipcdir: a spelling
//! AttachGroupIpcDirRefusal() cannot resolve, such as "<datadir>/x/.." with no x,
//! or a bind mount, reaches it there.
void ApplyAttachGroupAccess(const fs::path& ipcdir);

//! What the daemon prepares its IPC directory with when an attach group is
//! configured: make sure \p dir exists, created owner-only as
//! PrepareIpcDirectory() creates it (refusing the data directory as
//! ApplyAttachGroupAccess() does), then give it the attach group's access
//! (ApplyAttachGroupAccess()). An existing directory is not warned about, since
//! its access is set here; when that changes its mode or group, a line logs the
//! mode and group it had. Throws as those two do.
void PrepareAttachGroupDirectory(const fs::path& dir);
#endif // !WIN32
} // namespace ipc

#endif // GRIDCOIN_IPC_PROCESS_H
