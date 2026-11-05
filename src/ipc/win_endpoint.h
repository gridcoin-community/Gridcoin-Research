// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_WIN_ENDPOINT_H
#define GRIDCOIN_IPC_WIN_ENDPOINT_H

#include "fs.h"
#include "util/win_acl.h"

#include <cstdint>
#include <optional>
#include <string>

//! The access lists of the multiprocess endpoint on Windows, node.sock and
//! ipc.cookie, in each mode, and the GUI's check of the server's endpoint before
//! it connects.
//!
//! On Windows each object's own DACL decides who may open it. Everyone holds
//! "Bypass traverse checking", so the DACL of the directory around it does not;
//! that decides who may add, remove or replace an entry. In the same-account mode
//! node.sock and ipc.cookie are owned by the core's account, and only that account
//! and SYSTEM have access. With -ipcattachgroup (the service mode) the attach group
//! is added, with the right to connect to the socket or to read the cookie and
//! nothing more. Neither object has an inheritable entry.
//!
//! The access lists and the rules over what was read are pure, so they build and
//! are tested on every platform. Reading and applying them needs Windows.
//!
//! The GUI's directory rule asks for a protected DACL only when -ipcserviceuser is
//! set, that is at a service -ipcdir. Same-account data directories created by
//! earlier releases inherit %APPDATA%'s ACL and are not protected; refusing them
//! would strand existing users, and the server is still pinned by the owner of the
//! socket and, after connecting, of the attestation pipe. The directory rule is
//! there for a %ProgramData% directory someone else created before the service was
//! installed.
//!
//! Reparse points. node.sock, its directory and the directory's parent are each
//! opened with FILE_FLAG_OPEN_REPARSE_POINT and backup semantics, so a reparse
//! point at any of them is opened itself rather than followed, and each owner and
//! DACL is read through that handle. A reparse point that is a name surrogate (a
//! junction, a symbolic link, a mount point) at any of the three is then refused,
//! because through it the object checked would not be the one connect reaches; a
//! data directory that is itself a junction is refused too. The socket's own
//! AF_UNIX reparse point is not a name surrogate. The directories above the parent
//! are resolved by the system as usual.
namespace ipc::win {

using util::winacl::Sid;

//! The two endpoint objects in the IPC directory.
enum class IpcObject : uint8_t
{
    Socket, //!< node.sock
    Cookie, //!< ipc.cookie
};

namespace detail {
// The single rights by their bare names, which are <windows.h>'s macros of the
// same values in a file that included it first (see util/win_acl.h).
using namespace util::winacl::mask;
constexpr uint32_t SOCKET_CONNECT = FILE_WRITE_DATA | FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE;
constexpr uint32_t COOKIE_READ = FILE_READ_DATA | FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE;
constexpr uint32_t DIR_VIEW = FILE_TRAVERSE | FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE;
} // namespace detail

//! The attach group's rights on node.sock: FILE_WRITE_DATA, which an AF_UNIX
//! connect is expected to need (still to be confirmed on a Windows host), and the
//! attributes and DACL that the GUI reads before it connects.
constexpr uint32_t SOCKET_CONNECT_MASK = detail::SOCKET_CONNECT;

//! The attach group's rights on ipc.cookie: read it, its attributes and its DACL.
constexpr uint32_t COOKIE_READ_MASK = detail::COOKIE_READ;

//! The attach group's rights on an IPC directory: traverse and list it, and read
//! its attributes and DACL, which the GUI checks before it connects.
constexpr uint32_t DIR_VIEW_MASK = detail::DIR_VIEW;

//! Owner \p self; \p self and SYSTEM with FILE_ALL_ACCESS (SYSTEM named once when
//! it is \p self); plus \p attach_group, when set, with the object's mask
//! (SOCKET_CONNECT_MASK or COOKIE_READ_MASK). No entry is inheritable.
util::winacl::AclSpec EndpointAclSpec(IpcObject obj, const Sid& self, const std::optional<Sid>& attach_group);

//! The access list the core gives an IPC directory it owns in the attach-group
//! mode: owner \p self; \p self and SYSTEM with FILE_ALL_ACCESS, inherited by what
//! is created inside; and \p attach_group with DIR_VIEW_MASK on the directory
//! alone.
util::winacl::AclSpec IpcDirAclSpec(const Sid& self, const Sid& attach_group);

//! What an open without following found at one path of the endpoint.
enum class EntryKind : uint8_t
{
    //! A directory that does not send its path elsewhere.
    Directory,
    //! Anything else that does not, such as a regular file or the socket's own
    //! AF_UNIX reparse point.
    File,
    //! A reparse point that is a name surrogate (a junction, a symbolic link, a
    //! mount point): it sends its path elsewhere.
    Redirect,
};

//! The kind of an object from whether it is a directory and, when it is a reparse
//! point, its tag. A tag with the name-surrogate bit (0x20000000) is a Redirect,
//! whether or not the object is a directory.
EntryKind ClassifyEntry(bool directory, std::optional<uint32_t> reparse_tag);

//! One object on the endpoint's path, as opened without following it.
struct EndpointEntry
{
    fs::path path;
    EntryKind kind{EntryKind::File};
    util::winacl::ObservedAcl acl;
};

//! node.sock, its directory and the directory's parent.
struct EndpointFacts
{
    EndpointEntry socket;
    EndpointEntry dir;
    EndpointEntry parent;
};

//! Why \p dir may not hold the endpoint of a server running as \p server, or
//! nullopt. It must be a Directory, and FindForeignWriter(acl, {server, SYSTEM,
//! Administrators}, mask::IPC_DIR_WRITEISH) must find nothing: no missing DACL, no
//! other owner, and no other trustee that can add, remove or replace an entry.
//! With \p require_protected its DACL must also be protected. The reason names the
//! directory.
std::optional<std::string> DirectoryRefusal(const EndpointEntry& dir, const Sid& server, bool require_protected);

//! The GUI's rule before it connects, over what was read, or nullopt when the
//! endpoint may be connected to. node.sock must be neither a Directory nor a
//! Redirect and must be owned by \p expected, and its directory and the
//! directory's parent must each pass DirectoryRefusal(entry, \p expected,
//! \p require_protected). The reason names the object and the rule.
std::optional<std::string> EndpointRefusal(const EndpointFacts& facts, const Sid& expected, bool require_protected);

//! The name the local group \p name is looked up by: "<computer>\<name>", so that
//! only this computer's own accounts are searched and no domain group of the same
//! name is taken for it. nullopt when \p computer or \p name is empty, or \p name
//! holds a control character or one a Windows account name cannot hold
//! (" / \ [ ] : ; | = , + * ? < > @), so a name that names a domain is refused.
std::optional<std::string> LocalGroupLookupName(const std::string& computer, const std::string& name);

#ifdef WIN32
//! The group ConfigureAttachGroup(name) resolved (<COMPUTERNAME>\<name>, a local
//! group, SidTypeAlias, only), or nullopt when none is configured. The WIN32
//! counterpart of AttachGroupGid(): the SID string, such as "S-1-5-21-...-1005".
//! It lives with ConfigureAttachGroup, in peercred_win.cpp.
std::optional<std::string> AttachGroupSid();

//! EndpointAclSpec(obj, CurrentTokenUser(), the group \p attach_group_sid names).
//! Throws std::logic_error for a SID string that does not parse, and
//! std::system_error when the token cannot be read.
util::winacl::AclSpec ThisProcessEndpointAclSpec(IpcObject obj, const std::optional<std::string>& attach_group_sid);

//! Make \p spec the owner and protected DACL of the endpoint object \p path and
//! read it back (util::winacl::ApplyAcl). Throws std::system_error when a call
//! fails, and std::runtime_error, worded as the refusal to serve IPC, when the
//! volume does not keep the list.
void ApplyEndpointAcl(const fs::path& path, const util::winacl::AclSpec& spec);

//! ApplyEndpointAcl(path, ThisProcessEndpointAclSpec(obj, AttachGroupSid())).
void ApplyEndpointAcl(const fs::path& path, IpcObject obj);

//! Expected server: SID of -ipcserviceuser when set, else CurrentTokenUser().
//! Throws std::runtime_error naming the account when it does not exist, and
//! std::system_error when it cannot be looked up.
Sid ExpectedServerSid();

//! GUI, before connect: node.sock owner == expected; its directory and the
//! directory's parent owned by expected, SYSTEM or Administrators and free of
//! foreign IPC_DIR_WRITEISH grants; when -ipcserviceuser is set both must also be
//! protected (EndpointRefusal). Each is opened and read without following a
//! reparse point at it. Throws std::runtime_error naming the object and the rule;
//! std::system_error with std::errc::no_such_file_or_directory when one of the
//! three does not exist, which is a server not listening; and std::system_error
//! with the Win32 error when one cannot be opened or read.
void CheckServerEndpointBeforeConnect(const fs::path& socket_path, const Sid& expected);

//! Server, for an IPC directory the operator named with -ipcdir: why this account
//! must not serve in \p dir, or nullopt. DirectoryRefusal(dir as opened without
//! following it, CurrentTokenUser(), false). Throws std::system_error when \p dir
//! cannot be opened or its ACL read.
std::optional<std::string> ServerIpcDirRefusal(const fs::path& dir);

//! Server, with -ipcattachgroup: make the existing IPC directory \p dir ready for
//! \p attach_group. It is opened without following a reparse point and without
//! sharing delete, so its path keeps naming it, and refused, with a
//! std::runtime_error naming -ipcattachgroup and -ipcdir, before anything is
//! changed when it is a Redirect or has the file ID (volume serial number and file
//! index) of the network data directory or the base one above it. One this account
//! owns gets IpcDirAclSpec(self, \p attach_group), confirmed through the handle,
//! and a change to one that existed before (\p created false) is logged. One owned
//! by another account, such as gridcoin-service.exe's Administrators-owned layout,
//! is never changed and must pass DirectoryRefusal(entry, self, false). Throws
//! std::runtime_error on a refusal and std::system_error when a call fails.
void ApplyAttachGroupDirectoryAccess(const fs::path& dir, const Sid& attach_group, bool created);
#endif // WIN32

} // namespace ipc::win

#endif // GRIDCOIN_IPC_WIN_ENDPOINT_H
