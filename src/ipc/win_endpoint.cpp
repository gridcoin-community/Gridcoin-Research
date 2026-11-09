// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/win_endpoint.h"

#include "tinyformat.h"
#include "util/strencodings.h"

#include <algorithm>
#include <string_view>
#include <vector>

#ifdef WIN32
#include "ipc/peercred.h"
#include "ipc/process.h"
#include "logging.h"
#include "util/system.h"

#include <stdexcept>
#include <system_error>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif // WIN32

namespace ipc::win {

// The single rights by their bare names: <windows.h>'s macros where it is included,
// util::winacl::mask's constants elsewhere, with the same values.
using namespace util::winacl::mask;

using util::winacl::AceSpec;
using util::winacl::AclSpec;
using util::winacl::Inherit;

namespace {

//! The bit IsReparseTagNameSurrogate() tests: a reparse point with it set stands for
//! another named entity, so a path through it leads elsewhere.
constexpr uint32_t REPARSE_TAG_NAME_SURROGATE = 0x20000000;

//! The attach group's rights on \p obj.
uint32_t GroupMask(IpcObject obj)
{
    switch (obj) {
    case IpcObject::Socket:
        return SOCKET_CONNECT_MASK;
    case IpcObject::Cookie:
        return COOKIE_READ_MASK;
    } // no default case, so the compiler can warn about an object left out

    return 0;
}

//! \p name as the log and an error show it: each part sanitized, the backslash
//! between a domain and an account kept.
std::string ShownAccount(const std::string& name)
{
    std::string shown;
    size_t start = 0;
    while (true) {
        const size_t slash = name.find('\\', start);
        shown += SanitizeString(name.substr(start, slash == std::string::npos ? std::string::npos : slash - start));
        if (slash == std::string::npos) break;
        shown += '\\';
        start = slash + 1;
    }

    return shown;
}

//! How a refusal names an account of \p type.
const char* AccountTypeName(util::winacl::AccountType type)
{
    using util::winacl::AccountType;
    switch (type) {
    case AccountType::User: return "a user account";
    case AccountType::Group: return "a domain group";
    case AccountType::Domain: return "a domain";
    case AccountType::Alias: return "a local group";
    case AccountType::WellKnownGroup: return "a well-known group";
    case AccountType::DeletedAccount: return "a deleted account";
    case AccountType::Invalid: return "an invalid account";
    case AccountType::Unknown: return "an unknown account";
    case AccountType::Computer: return "a computer account";
    case AccountType::Label: return "an integrity label";
    case AccountType::LogonSession: return "a logon session";
    case AccountType::Other: return "an account of another kind";
    } // no default case, so the compiler can warn about a type left out

    return "an account of another kind";
}

//! \p self, and SYSTEM unless it is \p self, with FILE_ALL_ACCESS and \p inherit.
AclSpec OwnerAndSystem(const Sid& self, Inherit inherit)
{
    const Sid system = util::winacl::wellknown::System();
    AclSpec spec{self, {AceSpec{self, FILE_ALL_ACCESS, inherit}}};
    if (system != self) spec.aces.push_back(AceSpec{system, FILE_ALL_ACCESS, inherit});

    return spec;
}

} // namespace

#ifdef WIN32
static_assert((IO_REPARSE_TAG_SYMLINK & REPARSE_TAG_NAME_SURROGATE) != 0 &&
                  (IO_REPARSE_TAG_MOUNT_POINT & REPARSE_TAG_NAME_SURROGATE) != 0 &&
                  (IO_REPARSE_TAG_AF_UNIX & REPARSE_TAG_NAME_SURROGATE) == 0,
              "the name-surrogate bit differs from the Windows SDK's");
static_assert(IsReparseTagNameSurrogate(REPARSE_TAG_NAME_SURROGATE), "IsReparseTagNameSurrogate tests another bit");
static_assert(REPARSE_TAG_AF_UNIX == IO_REPARSE_TAG_AF_UNIX, "the AF_UNIX reparse tag differs from the Windows SDK's");
// The GUI reads the cookie with _wfopen "rb", which asks for GENERIC_READ, that is FILE_GENERIC_READ.
static_assert(COOKIE_READ_MASK == static_cast<uint32_t>(FILE_GENERIC_READ),
              "the attach group's cookie mask is not what an open for reading asks for");
#endif

AclSpec EndpointAclSpec(IpcObject obj, const Sid& self, const std::optional<Sid>& attach_group)
{
    AclSpec spec = OwnerAndSystem(self, Inherit::None);
    if (attach_group) spec.aces.push_back(AceSpec{*attach_group, GroupMask(obj), Inherit::None});

    return spec;
}

AclSpec IpcDirAclSpec(const Sid& self, const Sid& attach_group)
{
    // What the core creates inside is owner-only from creation; the group's view
    // reaches the directory alone.
    AclSpec spec = OwnerAndSystem(self, Inherit::ContainerAndObject);
    spec.aces.push_back(AceSpec{attach_group, DIR_VIEW_MASK, Inherit::None});

    return spec;
}

EntryKind ClassifyEntry(bool directory, std::optional<uint32_t> reparse_tag)
{
    if (reparse_tag && (*reparse_tag & REPARSE_TAG_NAME_SURROGATE) != 0) return EntryKind::Redirect;
    if (directory) return EntryKind::Directory;
    if (reparse_tag == REPARSE_TAG_AF_UNIX) return EntryKind::UnixSocket;

    return EntryKind::File;
}

std::optional<std::string> UntrustedPathRefusal(uint32_t error, const fs::path& path)
{
    if (error != util::winacl::WIN32_UNTRUSTED_MOUNT_POINT) return std::nullopt;

    return strprintf("the IPC endpoint '%s' cannot be checked: %s.", path.string(),
                     util::winacl::UNTRUSTED_MOUNT_POINT_REASON);
}

std::optional<std::string> DirectoryRefusal(const EndpointEntry& dir, const Sid& server, bool require_protected)
{
    const std::string shown = dir.path.string();

    switch (dir.kind) {
    case EntryKind::Directory:
        break;
    case EntryKind::Redirect:
        return strprintf("the directory '%s' is a junction or link, which is not followed", shown);
    case EntryKind::UnixSocket:
    case EntryKind::File:
        return strprintf("'%s' is not a directory", shown);
    } // no default case, so the compiler can warn about a kind left out

    const std::vector<Sid> allowed{server, util::winacl::wellknown::System(),
                                   util::winacl::wellknown::Administrators()};
    if (std::optional<std::string> reason = util::winacl::FindForeignWriter(dir.acl, allowed, IPC_DIR_WRITEISH)) {
        return strprintf("only the server's account (%s), SYSTEM and Administrators may own the directory '%s' or "
                         "add, remove or replace an entry in it, but %s",
                         util::winacl::FormatSid(server), shown, *reason);
    }

    if (require_protected && !dir.acl.is_protected) {
        return strprintf("the directory '%s' must have a protected DACL at a service -ipcdir, and it still inherits "
                         "entries from the directory above it",
                         shown);
    }

    return std::nullopt;
}

std::optional<std::string> SocketRefusal(const EndpointEntry& socket, const Sid& expected)
{
    const std::string shown = socket.path.string();

    switch (socket.kind) {
    case EntryKind::UnixSocket:
        break;
    case EntryKind::Redirect:
        return strprintf("the IPC socket '%s' is a link to another place, which is not followed", shown);
    case EntryKind::Directory:
        return strprintf("the IPC socket '%s' is a directory", shown);
    case EntryKind::File:
        return socket.reparse_tag
                   ? strprintf("the IPC socket '%s' is not an AF_UNIX socket (its reparse tag is 0x%08x)", shown,
                               *socket.reparse_tag)
                   : strprintf("the IPC socket '%s' is not an AF_UNIX socket (it is a plain file)", shown);
    } // no default case, so the compiler can warn about a kind left out

    if (!socket.acl.dacl_present) {
        return strprintf("the IPC socket '%s' has no DACL (all users would have access)", shown);
    }
    if (!socket.acl.owner) {
        return strprintf("the owner of the IPC socket '%s' could not be read", shown);
    }
    if (*socket.acl.owner != expected) {
        return strprintf("the IPC socket '%s' must be owned by the expected server account %s, and its owner is %s",
                         shown, util::winacl::FormatSid(expected), util::winacl::FormatSid(*socket.acl.owner));
    }

    return std::nullopt;
}

bool DirectoryRuleApplies(const util::winacl::ObservedAcl& socket_acl, const Sid& expected, bool service_user_set)
{
    if (service_user_set) return true;

    // FILE_WRITE_DATA is the connect right. A list FindForeignWriter cannot accept
    // for another reason (no DACL, another owner) also applies the rule, though
    // SocketRefusal has refused such a socket already.
    const std::vector<Sid> writers{expected, util::winacl::wellknown::System()};
    return util::winacl::FindForeignWriter(socket_acl, writers, FILE_WRITE_DATA | GENERIC_WRITE | GENERIC_ALL)
        .has_value();
}

std::optional<std::string> EndpointRefusal(const EndpointEntry& socket, const Sid& expected, bool service_user_set,
                                           const std::function<EndpointDirs()>& read_dirs)
{
    if (std::optional<std::string> reason = SocketRefusal(socket, expected)) return reason;

    // Only this account and SYSTEM can write the socket: nobody else can connect to
    // it, and a socket swapped in between is the attestation step's to find, so the
    // directories are not read.
    if (!DirectoryRuleApplies(socket.acl, expected, service_user_set)) return std::nullopt;

    const EndpointDirs dirs = read_dirs();
    if (std::optional<std::string> reason = DirectoryRefusal(dirs.dir, expected, service_user_set)) return reason;

    return DirectoryRefusal(dirs.parent, expected, service_user_set);
}

std::optional<std::string> LocalGroupLookupName(const std::string& computer, const std::string& name)
{
    if (computer.empty() || name.empty()) return std::nullopt;

    // The characters Windows refuses in a user or group name; '\' and '@' would also
    // bring a domain of their own.
    static constexpr std::string_view refused{"\"/\\[]:;|=,+*?<>@"};
    for (const char c : name) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x20 || byte == 0x7f || refused.find(c) != std::string_view::npos) return std::nullopt;
    }

    return computer + "\\" + name;
}

std::optional<std::string> ServiceUserNameRefusal(const std::string& name)
{
    const size_t slash = name.find('\\');
    if (slash == std::string::npos || slash == 0 || slash + 1 == name.size() ||
        name.find('\\', slash + 1) != std::string::npos) {
        return strprintf("-ipcserviceuser must name the account with its domain, as DOMAIN\\account (the service's "
                         "is NT SERVICE\\gridcoinresearchd), and '%s' does not",
                         ShownAccount(name));
    }

    return std::nullopt;
}

std::optional<std::string> ServiceUserAccountRefusal(const util::winacl::AccountLookup& account)
{
    using util::winacl::AccountType;
    if (account.type == AccountType::User) return std::nullopt;

    // A service's SID is S-1-5-80 and the five words of the hash of its name
    // (SHA-1). Other SIDs under S-1-5-80, such as NT SERVICE\ALL SERVICES
    // (S-1-5-80-0), name no one service.
    const std::string sid = util::winacl::FormatSid(account.sid);
    const bool service_sid = sid.rfind("S-1-5-80-", 0) == 0 && std::count(sid.begin(), sid.end(), '-') == 3 + 5;
    if (account.type == AccountType::WellKnownGroup && service_sid) return std::nullopt;

    return strprintf("the -ipcserviceuser account (%s) is %s, and the core runs as a user account or as a service "
                     "(NT SERVICE)",
                     sid, AccountTypeName(account.type));
}

std::optional<std::string> AttachGroupAccountRefusal(const util::winacl::AccountLookup& account,
                                                     const std::string& computer)
{
    if (account.type != util::winacl::AccountType::Alias) {
        return strprintf("it is %s, not a local group", AccountTypeName(account.type));
    }

    // Computer names compare without regard to ASCII case.
    if (account.domain.empty() || ToLower(account.domain) != ToLower(computer)) {
        return strprintf("it was found in '%s', not among this computer's (%s) own accounts",
                         SanitizeString(account.domain), SanitizeString(computer));
    }

    return std::nullopt;
}

#ifdef WIN32
namespace {

using util::winacl::UniqueHandle;

[[noreturn]] void ThrowWin32(DWORD error, const std::string& what)
{
    throw std::system_error(static_cast<int>(error), std::system_category(), what);
}

//! \p path opened to read its attributes and its owner and DACL, with backup
//! semantics so a directory opens too, and FILE_FLAG_OPEN_REPARSE_POINT so a
//! reparse point at \p path is opened itself rather than followed. Without
//! \p share_delete nobody can rename or delete it while the handle is open. A
//! missing path throws std::system_error with std::errc::no_such_file_or_directory.
//! A path through a junction Windows does not follow throws std::runtime_error, a
//! refusal naming the path and what to do (UntrustedPathRefusal): that is not a
//! passing failure but how the path is laid out. Anything else throws the Win32
//! error.
HANDLE OpenNoFollow(const fs::path& path, bool share_delete)
{
    const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | (share_delete ? FILE_SHARE_DELETE : 0);
    // A hold that does not share delete also asks for FILE_READ_DATA (FILE_LIST_DIRECTORY on a
    // directory): Windows checks an open's sharing only against opens that ask for data, execute
    // or delete rights, so one that asks only for the attributes and the list keeps nothing in place.
    const DWORD access = READ_CONTROL | FILE_READ_ATTRIBUTES | (share_delete ? 0 : FILE_READ_DATA);
    const HANDLE handle = ::CreateFileW(path.wstring().c_str(), access, share, nullptr,
                                        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory), path.string());
        }
        if (std::optional<std::string> refusal = UntrustedPathRefusal(error, path)) throw std::runtime_error(*refusal);
        ThrowWin32(error, "open " + path.string());
    }

    return handle;
}

//! What \p handle, opened by OpenNoFollow on \p path, refers to: its kind from its
//! attributes and reparse tag, and its owner and DACL read through the handle.
EndpointEntry ReadEntry(HANDLE handle, const fs::path& path)
{
    FILE_ATTRIBUTE_TAG_INFO info{};
    if (!::GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info))) {
        ThrowWin32(::GetLastError(), "GetFileInformationByHandleEx " + path.string());
    }

    EndpointEntry entry;
    entry.path = path;
    if ((info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        entry.reparse_tag = static_cast<uint32_t>(info.ReparseTag);
    }
    entry.kind = ClassifyEntry((info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, entry.reparse_tag);
    entry.acl = util::winacl::ReadAcl(handle, /*kernel_object=*/false);

    return entry;
}

//! The volume serial number and file index of the object \p handle refers to, or
//! nullopt when they cannot be read.
//!
//! On ReFS a file ID is 128 bits (FILE_ID_INFO), and the 64-bit index read here
//! may then be shared by two objects on the volume. The ID only refuses the data
//! directory, so such a collision can only refuse a directory that is not the data
//! directory, never let the data directory through.
std::optional<FileId> FileIdOf(HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION info{};
    if (!::GetFileInformationByHandle(handle, &info)) return std::nullopt;

    return FileId{info.dwVolumeSerialNumber, (static_cast<uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow};
}

//! The network data directory and the base one above it, each with its file ID,
//! opened as the core uses them: a junction there stands for the directory it leads
//! to. One that cannot be opened is left out, as on POSIX one stat() fails for.
std::vector<std::pair<fs::path, FileId>> DataDirectoryIds()
{
    std::vector<std::pair<fs::path, FileId>> ids;
    for (const bool net_specific : {true, false}) {
        const fs::path datadir = GetDataDir(net_specific);
        const HANDLE handle = ::CreateFileW(datadir.wstring().c_str(), FILE_READ_ATTRIBUTES,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;

        const UniqueHandle guard(handle);
        if (const std::optional<FileId> id = FileIdOf(handle)) ids.emplace_back(datadir, *id);
    }

    return ids;
}

} // namespace

AclSpec ThisProcessEndpointAclSpec(IpcObject obj, const std::optional<std::string>& attach_group_sid)
{
    std::optional<Sid> group;
    if (attach_group_sid) {
        group = util::winacl::ParseSidString(*attach_group_sid);
        if (!group) throw std::logic_error(strprintf("the attach group's SID '%s' does not parse", *attach_group_sid));
    }

    return EndpointAclSpec(obj, util::winacl::CurrentTokenUser(), group);
}

std::string EndpointAclNotKeptText(const fs::path& path, const std::string& reason)
{
    return strprintf("Refusing to serve IPC: '%s' did not keep its access control list -- %s. The IPC directory must "
                     "live on an NTFS volume that supports access control.",
                     path.string(), reason);
}

void ApplyEndpointAcl(const fs::path& path, const AclSpec& spec)
{
    // ApplyAcl reads the list back: a volume that accepts the call and stores
    // nothing (FAT, exFAT, some redirectors) would leave the object open to all.
    try {
        util::winacl::ApplyAcl(path, spec);
    } catch (const util::winacl::AclNotKeptError& e) {
        throw std::runtime_error(EndpointAclNotKeptText(path, e.reason()));
    }
}

void ApplyEndpointAcl(const fs::path& path, IpcObject obj)
{
    ApplyEndpointAcl(path, ThisProcessEndpointAclSpec(obj, AttachGroupSid()));
}

void ApplyBoundSocketAcl(const fs::path& path)
{
    // Opened without following a reparse point at it, and without sharing delete, so that while
    // the handle is open the path keeps naming the socket checked.
    const UniqueHandle handle(OpenNoFollow(path, /*share_delete=*/false));
    const EndpointEntry entry = ReadEntry(handle.get(), path);
    if (entry.kind != EntryKind::UnixSocket) {
        throw std::runtime_error(strprintf("Refusing to serve IPC: '%s' is not the socket this process bound: it is "
                                           "not an AF_UNIX socket",
                                           path.string()));
    }
    // A socket another account put at the name is that account's: it cannot give it the owner
    // this process's token gives what it creates, or this process's account.
    if (entry.acl.owner != util::winacl::CurrentTokenOwner() && entry.acl.owner != util::winacl::CurrentTokenUser()) {
        throw std::runtime_error(strprintf("Refusing to serve IPC: '%s' is not the socket this process bound: its "
                                           "owner is %s, not this process's account",
                                           path.string(),
                                           entry.acl.owner ? util::winacl::FormatSid(*entry.acl.owner)
                                                           : std::string("an owner that cannot be read")));
    }

    const AclSpec spec = ThisProcessEndpointAclSpec(IpcObject::Socket, AttachGroupSid());
    ApplyEndpointAcl(path, spec);

    // ApplyAcl went by the path; the handle holds the socket that was checked.
    if (std::optional<std::string> reason =
            util::winacl::CheckMatchesSpec(util::winacl::ReadAcl(handle.get(), /*kernel_object=*/false), spec)) {
        throw std::runtime_error(EndpointAclNotKeptText(path, *reason));
    }
}

Sid ExpectedServerSid()
{
    const std::string account = ConfiguredOption("-ipcserviceuser");
    if (account.empty()) return util::winacl::CurrentTokenUser();

    if (std::optional<std::string> reason = ServiceUserNameRefusal(account)) throw std::runtime_error(*reason);

    std::wstring wide;
    try {
        wide = Utf8ToWide(account);
    } catch (const std::range_error&) {
        throw std::runtime_error(
            strprintf("the -ipcserviceuser account '%s' is not valid UTF-8", ShownAccount(account)));
    }

    const std::optional<util::winacl::AccountLookup> found = util::winacl::LookupAccount(wide);
    if (!found) {
        throw std::runtime_error(
            strprintf("the -ipcserviceuser account '%s' does not exist on this computer", ShownAccount(account)));
    }
    if (std::optional<std::string> reason = ServiceUserAccountRefusal(*found)) throw std::runtime_error(*reason);

    return found->sid;
}

EndpointEntry ReadEntryNoFollow(const fs::path& path)
{
    const UniqueHandle handle(OpenNoFollow(path, /*share_delete=*/true));

    return ReadEntry(handle.get(), path);
}

EndpointHold::~EndpointHold()
{
    for (void* handle : m_handles) ::CloseHandle(static_cast<HANDLE>(handle));
}

void EndpointHold::Add(void* handle)
{
    // Reserved first, so that a failure to grow does not lose the handle.
    try {
        m_handles.reserve(m_handles.size() + 1);
    } catch (...) {
        ::CloseHandle(static_cast<HANDLE>(handle));
        throw;
    }
    m_handles.push_back(handle);
}

EndpointHold CheckServerEndpointBeforeConnect(const fs::path& socket_path, const Sid& expected)
{
    // Made lexically normal before the parent is taken: Windows resolves "." and
    // ".." in a path by its spelling too, so the directory taken here is the one
    // the connect goes through.
    const fs::path socket = socket_path.lexically_normal();
    const fs::path dir = socket.parent_path();
    if (!socket.is_absolute() || !dir.has_relative_path()) {
        throw std::runtime_error(strprintf("the IPC socket '%s' is not in a directory below the root of a volume",
                                           socket_path.string()));
    }

    // The socket first: when it is missing no server is listening, which is not a
    // refusal.
    const EndpointEntry socket_entry = ReadEntryNoFollow(socket);

    // When the rule applies, the directory and its parent are held open without
    // sharing delete until the connect is made, so neither can be renamed or
    // removed between this check and the connect.
    EndpointHold hold;
    const auto read_dirs = [&] {
        const HANDLE dir_handle = OpenNoFollow(dir, /*share_delete=*/false);
        hold.Add(dir_handle);
        const HANDLE parent_handle = OpenNoFollow(dir.parent_path(), /*share_delete=*/false);
        hold.Add(parent_handle);

        return EndpointDirs{ReadEntry(dir_handle, dir), ReadEntry(parent_handle, dir.parent_path())};
    };

    // The directory rule's -ipcserviceuser term, and its protected requirement (see
    // the header).
    const bool service = !ConfiguredOption("-ipcserviceuser").empty();
    if (std::optional<std::string> reason = EndpointRefusal(socket_entry, expected, service, read_dirs)) {
        throw std::runtime_error(*reason);
    }

    return hold;
}

std::optional<std::string> ServerIpcDirRefusal(const fs::path& dir)
{
    return DirectoryRefusal(ReadEntryNoFollow(dir), util::winacl::CurrentTokenUser(), /*require_protected=*/false);
}

void ApplyAttachGroupDirectoryAccess(const fs::path& dir, const Sid& attach_group, bool created)
{
    // Opened without following a reparse point at it, and without sharing delete,
    // so that while the handle is open the path keeps naming the directory checked.
    const UniqueHandle handle(OpenNoFollow(dir, /*share_delete=*/false));
    const EndpointEntry entry = ReadEntry(handle.get(), dir);
    if (entry.kind != EntryKind::Directory) {
        throw std::runtime_error(strprintf("-ipcattachgroup is set and -ipcdir (%s) is not a plain directory; a "
                                           "junction or link is not followed. Set -ipcdir to a directory of its own "
                                           "for the IPC socket and cookie.",
                                           dir.string()));
    }

    // By identity, so whatever spelling reached it, and before anything is changed.
    const std::optional<FileId> id = FileIdOf(handle.get());
    if (!id) ThrowWin32(::GetLastError(), "GetFileInformationByHandle " + dir.string());
    if (std::optional<std::string> refusal = DataDirectoryRefusal(dir, *id, DataDirectoryIds())) {
        throw std::runtime_error(*refusal);
    }

    const Sid self = util::winacl::CurrentTokenUser();

    // An existing directory of another account's, such as the Administrators-owned
    // layout gridcoin-service.exe makes, is checked as it is and never changed.
    if (!created && entry.acl.owner != self) {
        if (std::optional<std::string> reason = DirectoryRefusal(entry, self, /*require_protected=*/false)) {
            throw std::runtime_error(strprintf("Refusing to serve IPC: %s", *reason));
        }
        return;
    }

    const AclSpec spec = IpcDirAclSpec(self, attach_group);
    const std::optional<std::string> differed = util::winacl::CheckMatchesSpec(entry.acl, spec);
    if (!differed) return;

    ApplyEndpointAcl(dir, spec);

    // ApplyAcl went by the path; the handle holds the directory that was checked.
    if (std::optional<std::string> reason =
            util::winacl::CheckMatchesSpec(util::winacl::ReadAcl(handle.get(), /*kernel_object=*/false), spec)) {
        throw std::runtime_error(strprintf("Refusing to serve IPC: the IPC directory '%s' did not keep the attach "
                                           "group's access list -- %s",
                                           dir.string(), *reason));
    }

    // A directory made just now was created with this list and read back, so only a
    // change to one that was there already is logged.
    if (!created) {
        LogPrintf("IPC: gave the IPC directory %s the attach group's access list (this account and SYSTEM full "
                  "access, the group may view it); before, %s\n",
                  dir.string(), *differed);
    }
}
#endif // WIN32

} // namespace ipc::win
