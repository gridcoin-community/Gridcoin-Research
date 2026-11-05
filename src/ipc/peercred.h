// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PEERCRED_H
#define GRIDCOIN_IPC_PEERCRED_H

#include "fs.h"

#include <cstdint>
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
//! account; the Windows ones build and are tested on every platform. The
//! platform backends plug in at the hooks ReadPeerIdentity, ConfigureAttachGroup
//! and AttachGroupGid, which live in peercred_posix.cpp and peercred_win.cpp.
//!
//! On POSIX the identity comes from SO_PEERCRED and SO_PEERGROUPS on Linux and getpeereid()
//! elsewhere, and a peer that cannot be read is refused. The core admits its own
//! uid, and with -ipcattachgroup also a non-root peer in that group; root is
//! never admitted through the group. The GUI admits a core running as its own
//! uid, and with -ipcserviceuser also a core running as that account, provided
//! the IPC directory is a real directory (not a symlink), owned by that account
//! or root, and not writable by group or others.
//!
//! On Windows AF_UNIX exposes no peer credentials, and the server never receives
//! a socket fd from kj, so ReadPeerIdentity reads nothing there. The DACLs on
//! node.sock and ipc.cookie (ipc/win_endpoint.h) decide who may connect: this
//! account and SYSTEM, and with -ipcattachgroup also the group's right to connect
//! and to read the cookie. In every mode the peer is identified by the named-pipe
//! identity check, which runs on the connection before authentication
//! (ipc/win_attest.h, Init::beginAttest and Init::finishAttest). The core reads
//! the GUI's token user, attach-group membership and integrity level through the
//! pipe and decides with ServerPeerPolicy. The GUI checks that node.sock, before
//! it connects (and its directories when another account can write the socket),
//! and the check's pipe, before it writes anything into it, are owned by the
//! account it expects (ipc::win::ExpectedServerSid).
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
#endif

//! The identity of a Windows IPC peer; on Windows it is PeerIdentity. The core
//! fills it from what the identity check read (ServeInit's finishAttest). The
//! GUI's connect path builds none: it checks the core's account through the
//! owners of node.sock and of the check's pipe instead (see ClientPeerPolicy).
struct WinPeerIdentity
{
    //! The peer's account as a SID string, "S-1-5-21-...", from the token user
    //! the identity check read. Empty when nothing was read.
    std::string sid;
    //! The SECURITY_MANDATORY_*_RID of the peer's token; 0 when nothing was read.
    uint32_t integrity_rid{0};
    //! CheckTokenMembership's verdict for the attach group, taken while the
    //! token was held, so a group the token holds for deny only does not count.
    bool attach_group_member{false};
};

#ifdef WIN32
using PeerIdentity = WinPeerIdentity;
#endif

enum class PeerVerdict { Allow, Refuse };

//! What the core's rule for a Windows peer decides with.
struct WinServerPeerContext
{
    std::string self_sid;                //!< this process's token user, as a SID string
    bool attach_group_configured{false}; //!< whether -ipcattachgroup resolved a group
};

//! The core's rule for a Windows peer. Refused unless its SID was read and its
//! integrity level is Medium (0x2000) exactly. Then allowed when it is this
//! process's account, or when an attach group is configured and it is a member;
//! refused for anyone else.
PeerVerdict ServerPeerDecision(const WinPeerIdentity& peer, const WinServerPeerContext& ctx);

//! The rule for a Windows core: allowed only when its SID is \p expected_sid,
//! and neither is empty. The rule ClientPeerPolicy applies on Windows.
PeerVerdict ClientPeerDecision(const WinPeerIdentity& server, const std::string& expected_sid);

//! Hook. On POSIX this is the identity of the peer of a connected or accepted
//! AF_UNIX \p fd (SO_PEERCRED and SO_PEERGROUPS on Linux, getpeereid() elsewhere, where groups are
//! left empty), or nullopt, with a log line, when it cannot be read. Callers refuse on
//! nullopt. On Windows it is always nullopt: \p fd is ignored (kj gives the server
//! -1), and nothing on Windows serves or connects on it. The identity check that
//! runs on the connection identifies the peer there instead.
std::optional<PeerIdentity> ReadPeerIdentity(int fd);

//! Hook. Resolve the group \p name and remember it as the attach group; "" clears
//! it and returns true. Returns false if the group does not exist or, on Linux, if
//! this process does not hold it (the unit needs SupplementaryGroups=). On
//! Windows the name is looked up as <COMPUTERNAME>\<name> and must be a local group
//! (SidTypeAlias); a name that brings a domain of its own is refused. The daemon
//! calls it once, with -ipcattachgroup, before it serves.
bool ConfigureAttachGroup(const std::string& name);

//! The operator-facing reason ConfigureAttachGroup(\p name) would fail, or nullopt
//! when it would not. It names the group; for a group this process does not
//! hold (Linux) it says to add SupplementaryGroups=<name> to the service unit or
//! to run the core with that group; for an unknown group it says the group does
//! not exist; on Windows, for a name that is not a local group, it says so. The
//! daemon uses it as the text of its startup error.
std::optional<std::string> AttachGroupProblem(const std::string& name);

#ifndef WIN32
//! Hook. The gid ConfigureAttachGroup resolved, or nullopt when none is
//! configured. (Windows has no gid_t; its counterpart is
//! ipc::win::AttachGroupSid() in ipc/win_endpoint.h.)
std::optional<gid_t> AttachGroupGid();

//! What ReadPeerIdentity does when reading the peer's supplementary groups fails
//! with a given errno.
enum class PeerGroupsError {
    //! The kernel has no SO_PEERGROUPS (ENOPROTOOPT, kernels before 4.13). The
    //! identity from SO_PEERCRED is kept with empty groups.
    EmptyGroups,
    //! Any other failure refuses the peer.
    Refuse,
};

PeerGroupsError PeerGroupsErrorAction(int err);

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
//! effective uid and AttachGroupGid(). On Windows, ServerPeerDecision with this
//! process's token user and whether ipc::win::AttachGroupSid() is set, over the
//! identity the identity check read (ServeInit's finishAttest); refused when the
//! token user cannot be read.
PeerVerdict ServerPeerPolicy(const PeerIdentity& peer);

//! The GUI's check of the core it connected to, before it presents the cookie:
//! ClientPeerDecision with this process's effective uid, the uid of the
//! -ipcserviceuser account and lstat() of \p ipcdir, the directory that holds
//! the socket. A core at the caller's own uid is allowed at once, without the
//! account lookup or the lstat; for any other core both are made on each call.
//!
//! On Windows it is ClientPeerDecision with ipc::win::ExpectedServerSid(), refused
//! when that cannot be resolved, and \p ipcdir is not read. The Windows connect
//! path does not call it, as the socket gives no identity to call it with. The
//! same rule, against the same ExpectedServerSid(), is enforced there on the
//! objects themselves: ipc::win::CheckServerEndpointBeforeConnect refuses a
//! node.sock owned by another account before the connect, and
//! ipc::win::AttestClientPipe::Open refuses an identity-check pipe owned by
//! another account before anything is written into it.
PeerVerdict ClientPeerPolicy(const PeerIdentity& server, const fs::path& ipcdir);

//! The value of a name-valued IPC option such as "-ipcserviceuser", or "" when
//! it is unset or negated (-noipcserviceuser). The daemon and the peer policies
//! all read these options through it, so they share one rule for the -no form.
//! The rule is ::GetConfiguredOption (util/system.h), which the wallet backup
//! outside the IPC library reads -ipcattachgroup through as well.
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
