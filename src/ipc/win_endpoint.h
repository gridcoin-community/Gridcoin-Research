// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_WIN_ENDPOINT_H
#define GRIDCOIN_IPC_WIN_ENDPOINT_H

#include "fs.h"
#include "util/win_acl.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

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
//! The GUI's check before it connects has two parts. In every mode node.sock
//! must be an AF_UNIX socket with a DACL, owned by the expected server account
//! (SocketRefusal). The directory rule (DirectoryRefusal on the socket's directory
//! and on its parent) applies only when someone other than the expected account
//! and SYSTEM can write the socket, or -ipcserviceuser is set
//! (DirectoryRuleApplies): the attach-group and service modes, where the
//! directories are what keep another account from replacing the socket between
//! the check and the connect. It asks for a protected DACL only when
//! -ipcserviceuser is set, the service -ipcdir that someone else could have
//! created before the service was installed.
//!
//! In the same-account mode the directories are not checked. Data directories
//! made by earlier releases inherit %APPDATA%'s ACL, and others sit under a volume
//! root or behind a junction; refusing them would strand existing users. There the
//! protection against a socket swapped between the check and the connect rests on
//! the attestation step after connecting, which checks the owner of the
//! attestation pipe the server opens.
//!
//! Reparse points. Each object checked is opened with FILE_FLAG_OPEN_REPARSE_POINT
//! and backup semantics, so a reparse point at it is opened itself rather than
//! followed, and its owner and DACL are read through that handle. node.sock must
//! carry the AF_UNIX reparse tag; a link there (a junction, a symbolic link, a
//! mount point) or any other object is refused in every mode. When the directory
//! rule applies, a link at the directory or its parent is refused as well, because
//! through it the object checked would not be the one connect reaches, and both
//! are held open without sharing delete until the connect is made. In the
//! same-account mode a junction above the socket is followed as the system
//! resolves it. Directories above the parent are always resolved as usual.
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

//! IO_REPARSE_TAG_AF_UNIX: the reparse tag an AF_UNIX socket file carries
//! (checked against the Windows SDK in win_endpoint.cpp).
constexpr uint32_t REPARSE_TAG_AF_UNIX = 0x80000023;

//! What an open without following found at one path of the endpoint.
enum class EntryKind : uint8_t
{
    //! A directory that does not send its path elsewhere; a reparse point that is
    //! not a name surrogate, such as a cloud-files directory, is one too.
    Directory,
    //! A reparse point with the AF_UNIX tag that is not a directory: a socket.
    UnixSocket,
    //! Anything else that does not send its path elsewhere, such as a regular file
    //! or a file with another reparse tag.
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
    //! Its reparse tag, when it is a reparse point.
    std::optional<uint32_t> reparse_tag;
    util::winacl::ObservedAcl acl;
};

//! The socket's directory and that directory's parent.
struct EndpointDirs
{
    EndpointEntry dir;
    EndpointEntry parent;
};

//! Why \p socket is not the expected server's socket, or nullopt. In every mode
//! it must be a UnixSocket (a link at the socket's name is refused), have a DACL,
//! and be owned by \p expected. The reason names the socket and the rule.
std::optional<std::string> SocketRefusal(const EndpointEntry& socket, const Sid& expected);

//! Whether the directory rule applies to an endpoint whose socket has
//! \p socket_acl: when \p service_user_set (-ipcserviceuser), or when the socket's
//! list is not one only \p expected and SYSTEM can write, that is when
//! FindForeignWriter(socket_acl, {expected, SYSTEM}, FILE_WRITE_DATA |
//! GENERIC_WRITE | GENERIC_ALL) finds a reason. FILE_WRITE_DATA is the right an
//! AF_UNIX connect needs, so this is true when another account holds the right to
//! connect, as the attach group does.
bool DirectoryRuleApplies(const util::winacl::ObservedAcl& socket_acl, const Sid& expected, bool service_user_set);

//! Why \p dir may not hold the endpoint of a server running as \p server, or
//! nullopt. It must be a Directory, and FindForeignWriter(acl, {server, SYSTEM,
//! Administrators}, mask::IPC_DIR_WRITEISH) must find nothing: no missing DACL, no
//! other owner, and no other trustee that can add, remove or replace an entry.
//! With \p require_protected its DACL must also be protected. The reason names the
//! directory.
std::optional<std::string> DirectoryRefusal(const EndpointEntry& dir, const Sid& server, bool require_protected);

//! The GUI's rule before it connects, or nullopt when the endpoint may be
//! connected to: SocketRefusal(socket, \p expected); then, only when
//! DirectoryRuleApplies(socket.acl, \p expected, \p service_user_set),
//! \p read_dirs is called once and the directory and the parent it returns must
//! each pass DirectoryRefusal(entry, \p expected, \p service_user_set). Otherwise
//! \p read_dirs is not called, so in the same-account mode nothing above the
//! socket is looked at. The reason names the object and the rule.
std::optional<std::string> EndpointRefusal(const EndpointEntry& socket, const Sid& expected, bool service_user_set,
                                           const std::function<EndpointDirs()>& read_dirs);

//! The name the local group \p name is looked up by: "<computer>\<name>", so that
//! only this computer's own accounts are searched and no domain group of the same
//! name is taken for it. nullopt when \p computer or \p name is empty, or \p name
//! holds a control character or one a Windows account name cannot hold
//! (" / \ [ ] : ; | = , + * ? < > @), so a name that names a domain is refused.
std::optional<std::string> LocalGroupLookupName(const std::string& computer, const std::string& name);

//! Why \p name cannot name the -ipcserviceuser account, or nullopt. It must name
//! its domain and the account, "DOMAIN\account" with one backslash and a name on
//! each side (as "NT SERVICE\gridcoinresearchd"), so it is looked up in that domain
//! alone and never matched to an account of the same name elsewhere.
std::optional<std::string> ServiceUserNameRefusal(const std::string& name);

//! Why \p account, what -ipcserviceuser maps to, cannot be the expected server, or
//! nullopt. It must be a user account, or a service SID: a well-known group whose
//! SID is S-1-5-80 and five more sub-authorities (NT SERVICE\<name>). Any group,
//! alias or other kind is refused, and so is S-1-5-80-0 (NT SERVICE\ALL SERVICES).
std::optional<std::string> ServiceUserAccountRefusal(const util::winacl::AccountLookup& account);

//! Why \p account, what LocalGroupLookupName(\p computer, name) maps to, cannot be
//! the attach group, or nullopt. It must be a local group (an alias) whose domain
//! is \p computer, compared without regard to ASCII case, so a builtin alias found
//! under the computer's name, such as BUILTIN\Users, is refused.
std::optional<std::string> AttachGroupAccountRefusal(const util::winacl::AccountLookup& account,
                                                     const std::string& computer);

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

//! The refusal to serve IPC when the endpoint object \p path did not keep its access
//! list, \p reason saying how (CheckMatchesSpec's text).
std::string EndpointAclNotKeptText(const fs::path& path, const std::string& reason);

//! Make \p spec the owner and protected DACL of the endpoint object \p path and
//! read it back (util::winacl::ApplyAcl). Throws std::system_error when a call
//! fails, and std::runtime_error, worded as the refusal to serve IPC, when the
//! volume does not keep the list.
void ApplyEndpointAcl(const fs::path& path, const util::winacl::AclSpec& spec);

//! ApplyEndpointAcl(path, ThisProcessEndpointAclSpec(obj, AttachGroupSid())).
void ApplyEndpointAcl(const fs::path& path, IpcObject obj);

//! Expected server: SID of -ipcserviceuser when set, else CurrentTokenUser().
//! The option must pass ServiceUserNameRefusal and what it maps to
//! ServiceUserAccountRefusal. Throws std::runtime_error naming the account when it
//! does not, or does not exist, and std::system_error when it cannot be looked up.
Sid ExpectedServerSid();

//! \p path opened without following a reparse point at it, as the checks below
//! read each object: its kind, reparse tag, owner and DACL. Throws
//! std::system_error with std::errc::no_such_file_or_directory when it does not
//! exist, and with the Win32 error when it cannot be opened or read.
EndpointEntry ReadEntryNoFollow(const fs::path& path);

//! What CheckServerEndpointBeforeConnect leaves open until the connect is made:
//! when the directory rule applied, the socket's directory and its parent, opened
//! without sharing delete so neither can be renamed or removed in between;
//! nothing otherwise. Closes what it holds when destroyed.
class EndpointHold
{
public:
    EndpointHold() = default;
    ~EndpointHold();
    EndpointHold(EndpointHold&& other) noexcept = default;
    EndpointHold& operator=(EndpointHold&&) = delete;
    EndpointHold(const EndpointHold&) = delete;
    EndpointHold& operator=(const EndpointHold&) = delete;

    //! Take \p handle, a Win32 HANDLE, to close it when this is destroyed.
    void Add(void* handle);

    //! How many handles it holds: two when the directory rule applied, else none.
    size_t size() const { return m_handles.size(); }

private:
    std::vector<void*> m_handles;
};

//! GUI, before connect: EndpointRefusal over node.sock and, when the directory
//! rule applies, its directory and the directory's parent, each read with
//! ReadEntryNoFollow after \p socket_path is made lexically normal. The protected
//! requirement and the rule's -ipcserviceuser term follow that option. Returns
//! what must stay open until the connect is made. Throws std::runtime_error naming
//! the object and the rule; std::system_error with
//! std::errc::no_such_file_or_directory when an object read does not exist, which
//! is a server not listening; and std::system_error with the Win32 error when one
//! cannot be opened or read.
EndpointHold CheckServerEndpointBeforeConnect(const fs::path& socket_path, const Sid& expected);

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
