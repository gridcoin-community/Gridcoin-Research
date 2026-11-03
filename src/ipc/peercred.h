// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PEERCRED_H
#define GRIDCOIN_IPC_PEERCRED_H

#include "fs.h"

#include <optional>
#include <string>
#ifndef WIN32
#include <sys/types.h> // uid_t, gid_t, mode_t
#include <vector>
#endif

//! The OS peer check on the IPC socket.
//!
//! Each end of the AF_UNIX connection reads the identity of the process at the
//! other end (ReadPeerIdentity) and decides with a policy function before it
//! does anything else: the core on each accepted connection before serving it
//! (ServerPeerPolicy), the GUI on its connected socket before it presents the
//! cookie (ClientPeerPolicy). Every refusal is logged with the uid(s) and the
//! rule that failed.
//!
//! The rules are pure functions over plain data (ServerPeerDecision,
//! ClientPeerDecision), so every branch is unit-testable without a second OS
//! account. The platform backends plug in at the hooks ReadPeerIdentity,
//! ConfigureAttachGroup and AttachGroupGid, which live in peercred_posix.cpp and
//! peercred_win.cpp.
//!
//! On POSIX the identity comes from SO_PEERCRED on Linux and getpeereid()
//! elsewhere, and a peer that cannot be read is refused. The core admits its own
//! uid, and with -ipcattachgroup also a non-root peer in that group; root is
//! never admitted through the group. The GUI admits a core running as its own
//! uid, and with -ipcserviceuser also a core running as that account, provided
//! the IPC directory is a real directory (not a symlink), owned by that account
//! or root, and not writable by group or others.
//!
//! On Windows AF_UNIX exposes no peer credentials, and the server never receives
//! a socket fd from kj. Until the Windows backend exists, the owner-only DACL on
//! node.sock and ipc.cookie is the guard, and either side refuses as soon as a
//! different account is configured (-ipcattachgroup or -ipcserviceuser).
namespace ipc {

#ifndef WIN32
//! The OS identity of the process at the other end of an AF_UNIX connection.
//! groups holds supplementary groups where the platform backend can read them,
//! and is empty otherwise.
struct PeerIdentity
{
    uid_t uid{static_cast<uid_t>(-1)};
    gid_t gid{static_cast<gid_t>(-1)};
    std::vector<gid_t> groups;
};
#else
//! The Windows fields arrive with the Windows backend; none is read yet.
struct PeerIdentity
{
};
#endif

enum class PeerVerdict { Allow, Refuse };

//! Hook. On POSIX this is the identity of the peer of a connected or accepted
//! AF_UNIX \p fd (SO_PEERCRED on Linux, getpeereid() elsewhere; groups left
//! empty), or nullopt, with a log line, when it cannot be read. Callers refuse on
//! nullopt. The Windows placeholder ignores \p fd (kj gives the server -1) and
//! returns an empty PeerIdentity.
std::optional<PeerIdentity> ReadPeerIdentity(int fd);

//! Hook. Resolve the group \p name and remember it as the attach group; "" clears
//! it and returns true. Returns false if the group does not exist. The daemon
//! calls it once, with -ipcattachgroup, before it serves. The Windows
//! placeholder returns true only for "".
bool ConfigureAttachGroup(const std::string& name);

#ifndef WIN32
//! Hook. The gid ConfigureAttachGroup resolved, or nullopt when none is
//! configured. (Windows has no gid_t; its attach group arrives with the Windows
//! backend.)
std::optional<gid_t> AttachGroupGid();

//! Whether \p gid is \p peer's primary group or one of its supplementary groups.
bool PeerInGroup(const PeerIdentity& peer, gid_t gid);

struct ServerPeerContext
{
    uid_t self_uid{static_cast<uid_t>(-1)};
    std::optional<gid_t> attach_gid;
};

//! The core's rule for an accepted peer: its own uid is allowed; root is
//! refused; a peer in the attach group is allowed; anything else is refused.
PeerVerdict ServerPeerDecision(const PeerIdentity& peer, const ServerPeerContext& ctx);

//! What lstat() says about the IPC directory the GUI connects into.
struct IpcDirFacts
{
    bool exists{false};
    bool is_directory{false};
    bool is_symlink{false};
    uid_t owner{static_cast<uid_t>(-1)};
    mode_t mode{0};
};

struct ClientPeerContext
{
    uid_t self_uid{static_cast<uid_t>(-1)};
    std::optional<uid_t> service_uid;
    IpcDirFacts dir;
};

//! The GUI's rule for the core it connected to: a core running as the GUI's own
//! uid is allowed. A core at another uid is allowed only when it is the
//! configured service account and the IPC directory exists, is a directory and
//! not a symlink, is owned by the service account or root, and is not writable
//! by group or others.
PeerVerdict ClientPeerDecision(const PeerIdentity& server, const ClientPeerContext& ctx);
#endif // !WIN32

//! The core's check of an accepted peer: ServerPeerDecision with this process's
//! effective uid and AttachGroupGid(). Windows placeholder: allow unless
//! -ipcattachgroup is set.
PeerVerdict ServerPeerPolicy(const PeerIdentity& peer);

//! The GUI's check of the core it connected to, before it presents the cookie:
//! ClientPeerDecision with this process's effective uid, the uid of the
//! -ipcserviceuser account (looked up on each call) and lstat() of \p ipcdir,
//! the directory that holds the socket (read on each call). Windows placeholder:
//! allow unless -ipcserviceuser is set.
PeerVerdict ClientPeerPolicy(const PeerIdentity& server, const fs::path& ipcdir);

//! The value of a name-valued IPC option such as "-ipcserviceuser", or "" when
//! it is unset or negated (-noipcserviceuser). The daemon and the peer policies
//! all read these options through it, so they share one rule for the -no form.
std::string ConfiguredOption(const std::string& arg);

//! The directory that holds the socket named by a canonical "unix:<path>"
//! address (as Process::connect and Process::bind rewrite it), or an empty path
//! when \p canonical_address is not of that form.
fs::path SocketDirOf(const std::string& canonical_address);

//! One-line description of the peer check compiled in, for the daemon's startup
//! log, so an operator never has to guess what is enforced on this platform.
const char* PeerCredentialEnforcement();

} // namespace ipc

#endif // GRIDCOIN_IPC_PEERCRED_H
