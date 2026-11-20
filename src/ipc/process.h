// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PROCESS_H
#define GRIDCOIN_IPC_PROCESS_H

#include "fs.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
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
    //! connected socket descriptor. On Windows the endpoint is checked first
    //! (ipc::win::CheckServerEndpointBeforeConnect): node.sock must be an AF_UNIX
    //! socket owned by the expected server, and when another account can write it
    //! its directory and the parent must keep other accounts out too, and are held
    //! open until the connect is made. A refusal throws
    //! interfaces::PeerRefusedError, and a missing endpoint a std::system_error
    //! with std::errc::no_such_file_or_directory, before anything is connected. In
    //! the same-account mode a socket swapped between the check and the connect is
    //! for the attestation step after connecting to find. On POSIX the peer is
    //! checked after connecting, by the caller.
    virtual int connect(const fs::path& ipc_dir, std::string& address) = 0;

    //! Canonicalize \p address under \p ipc_dir, create + bind a listening
    //! socket, and return its descriptor. Removes a stale socket file left by a
    //! crash; throws if the path is already bound by a live listener. On POSIX
    //! the socket is left 0600, or 0660 in the attach group when one is
    //! configured; on Windows it gets ipc::win::EndpointAclSpec(Socket, ...) for
    //! this account and the configured attach group.
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
//! both resolved; one that does not exist yet is not the data directory. The same
//! on every platform.
std::optional<std::string> AttachGroupIpcDirRefusal();

//! What identifies a file system object while it exists: on POSIX the device and
//! inode, on Windows the volume serial number and the file index
//! (GetFileInformationByHandle).
struct FileId
{
    uint64_t volume{0};
    uint64_t index{0};

    bool operator==(const FileId& other) const { return volume == other.volume && index == other.index; }
};

//! The refusal, naming -ipcattachgroup, -ipcdir and the data directory, when the
//! IPC directory \p ipcdir, as opened, has the id \p ipcdir_id of one of
//! \p datadirs (the path and id of the network data directory, and of the base one
//! above it); nullopt otherwise. Both platforms refuse the attach group's directory
//! with it before any access is changed.
std::optional<std::string> DataDirectoryRefusal(const fs::path& ipcdir, const FileId& ipcdir_id,
                                                const std::vector<std::pair<fs::path, FileId>>& datadirs);

//! \p dir as the IPC directory is created and checked on Windows: lexically normal,
//! without a trailing separator. nullopt when a component is "." or "..", which
//! Windows refuses in an IPC directory: such a spelling can make a directory other
//! than the one it names.
std::optional<fs::path> PlainIpcDirectory(const fs::path& dir);

//! What the daemon prepares its IPC directory with when an attach group is
//! configured: make sure \p dir exists, then give it the attach group's access.
//!
//! POSIX: a directory this call creates is created owner-only as
//! PrepareIpcDirectory() creates it (refusing the data directory as
//! ApplyAttachGroupAccess() does), then ApplyAttachGroupAccess() gives it mode
//! 0750 in the group. An existing directory is not warned about, since its access
//! is set here; when that changes its mode or group, a line logs the mode and group
//! it had.
//!
//! Windows: ipc::win::ApplyAttachGroupDirectoryAccess() with the configured group,
//! which refuses the data directory by its file ID before anything is changed.
//!
//! Throws as those do, and std::logic_error when no attach group is configured.
void PrepareAttachGroupDirectory(const fs::path& dir);

//! Make sure \p dir exists. When this call creates it, it is restricted to this
//! account (POSIX mode 0700 through an fd; on Windows created with the owner-only
//! protected DACL, refusing a spelling PlainIpcDirectory() refuses) and failing to
//! do so throws; an existing directory is left as configured,
//! with a warning in the log if it is accessible to other users. On Windows an
//! existing directory is checked instead when -ipcdir is set: one that
//! ipc::win::ServerIpcDirRefusal() refuses (a junction or link, no DACL, an owner
//! other than this account, SYSTEM or Administrators, or another trustee that can
//! add, remove or replace an entry) throws std::runtime_error, so the core does not
//! serve in it. Throws std::system_error, naming the directory, if it cannot be
//! created.
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
#endif // !WIN32
} // namespace ipc

#endif // GRIDCOIN_IPC_PROCESS_H
