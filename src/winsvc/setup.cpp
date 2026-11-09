// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "winsvc/setup.h"

#ifdef WIN32
#include "base58.h"
#include "ipc/win_endpoint.h"
#include "random.h"
#include "support/cleanse.h"
#include "tinyformat.h"
#include "util/fsmodes.h"
#include "util/strencodings.h"
#include "util/string.h"
#include "util/syserror.h"
#include "util/time.h"
#include "util/win_acl.h"
#include "util/winservice.h"
#include "winsvc/accounts.h"
#include "winsvc/layout.h"
#include "winsvc/transfer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <windows.h>
#include <knownfolders.h>
#include <lm.h>
#include <shlobj.h>

namespace winsvc {

namespace {

namespace winacl = util::winacl;
using winacl::AclSpec;
using winacl::ObservedAcl;
using winacl::UniqueHandle;

//! What install, uninstall and status refuse with: the reason, printed, and EXIT_REFUSED. Nothing
//! after the refusal is changed.
class Refusal : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// Output.

//! \p text, in UTF-8, written to the console as wide characters when \p which is one, so that a
//! path with non-ASCII characters shows as it is, and as bytes otherwise.
void WriteText(DWORD which, const std::string& text)
{
    const HANDLE handle = ::GetStdHandle(which);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return;

    DWORD mode = 0;
    if (::GetConsoleMode(handle, &mode)) {
        try {
            const std::wstring wide = Utf8ToWide(text);
            DWORD written = 0;
            ::WriteConsoleW(handle, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
            return;
        } catch (const std::range_error&) {
            // Not UTF-8: written as bytes, below.
        }
    }
    DWORD written = 0;
    ::WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
}

void Say(const std::string& line)
{
    WriteText(STD_OUTPUT_HANDLE, line + "\n");
}

void SayError(const std::string& line)
{
    // Every line here is written to its handle at once (WriteText), and the C library's streams
    // are unbuffered (main), so this flush only keeps anything written through them ahead of the
    // error, as it was written first.
    std::fflush(stdout);
    std::cout.flush();
    WriteText(STD_ERROR_HANDLE, line + "\n");
}

std::string Shown(const std::wstring& text)
{
    try {
        return WideToUtf8(text);
    } catch (const std::range_error&) {
        return "(a name that is not valid UTF-16)";
    }
}

std::string Shown(const fs::path& path)
{
    return Shown(path.wstring());
}

//! A status of the network management functions (NetLocalGroupAdd) as text. Their own codes,
//! NERR_BASE to MAX_NERR, are not in the system's message table: their messages are in
//! netmsg.dll, loaded from the system directory as data. Any other code is a Win32 error.
std::string NetApiText(NET_API_STATUS status)
{
    if (status < NERR_BASE || status > MAX_NERR) return Win32ErrorText(status);

    std::string message;
    std::wstring system(MAX_PATH, L'\0');
    const UINT length = ::GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
    if (length > 0 && length < system.size()) {
        system.resize(length);
        const HMODULE module = ::LoadLibraryExW((system + L"\\netmsg.dll").c_str(), nullptr,
                                                LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (module) {
            wchar_t* buffer = nullptr;
            const DWORD size = ::FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                                    FORMAT_MESSAGE_IGNORE_INSERTS,
                                                module, status, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
            if (size > 0 && buffer) {
                try {
                    message = PrintableAscii(WideToUtf8(std::wstring_view(buffer, size)));
                } catch (const std::range_error&) {
                    // Not valid UTF-16: the number alone, below.
                }
            }
            if (buffer) ::LocalFree(buffer);
            ::FreeLibrary(module);
        }
    }

    if (message.empty()) return strprintf("NET_API_STATUS %u", static_cast<unsigned>(status));
    return strprintf("%s (NET_API_STATUS %u)", message, static_cast<unsigned>(status));
}

[[noreturn]] void ThrowWin32(DWORD error, const std::string& what)
{
    throw std::runtime_error(what + ": " + Win32ErrorText(error));
}

[[noreturn]] void ThrowLastError(const std::string& what)
{
    ThrowWin32(::GetLastError(), what);
}

//! The verb's body, its refusal or failure printed, as the tool's exit code.
int RunVerb(const std::function<void()>& body)
{
    try {
        body();
        return EXIT_OK;
    } catch (const Refusal& e) {
        SayError(std::string("Refused: ") + AsciiWhat(e));
    } catch (const std::exception& e) {
        SayError(std::string("Error: ") + AsciiWhat(e));
    }
    return EXIT_REFUSED;
}

// Handles.

struct ServiceHandleCloser
{
    void operator()(SC_HANDLE handle) const noexcept { ::CloseServiceHandle(handle); }
};
using UniqueServiceHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ServiceHandleCloser>;

struct TaskMemFreer
{
    void operator()(wchar_t* p) const noexcept { ::CoTaskMemFree(p); }
};

UniqueServiceHandle OpenScm(DWORD access)
{
    UniqueServiceHandle scm(::OpenSCManagerW(nullptr, nullptr, access));
    if (!scm) ThrowLastError("OpenSCManagerW");
    return scm;
}

//! The service \p name opened with \p access, or null when it is not installed.
UniqueServiceHandle OpenServiceIfInstalled(SC_HANDLE scm, const std::wstring& name, DWORD access)
{
    UniqueServiceHandle service(::OpenServiceW(scm, name.c_str(), access));
    if (!service && ::GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST) ThrowLastError("OpenServiceW " + Shown(name));
    return service;
}

// This computer.

fs::path ProgramDataPath()
{
    wchar_t* raw = nullptr;
    const HRESULT result = ::SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &raw);
    const std::unique_ptr<wchar_t, TaskMemFreer> path(raw);
    if (FAILED(result) || !path) {
        throw std::runtime_error(strprintf("the ProgramData folder could not be found (SHGetKnownFolderPath: 0x%08x)",
                                           static_cast<unsigned>(result)));
    }
    return fs::path(std::wstring(path.get()));
}

fs::path ModulePath()
{
    std::wstring module(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = ::GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (length == 0) ThrowLastError("GetModuleFileNameW");
        if (length < module.size()) {
            module.resize(length);
            return fs::path(module);
        }
        // Cut short: try again with room for the whole path.
        module.resize(module.size() * 2);
    }
}

//! The path of the file \p handle refers to as Windows resolves it (GetFinalPathNameByHandleW), a
//! drive path without the \\?\ prefix. \p shown names it in an error. A file on a network share
//! is refused.
fs::path FinalPathOf(HANDLE handle, const fs::path& shown)
{
    const DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    std::wstring name(MAX_PATH, L'\0');
    DWORD length = ::GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), flags);
    if (length >= name.size()) {
        // The size needed, with the terminating null.
        name.resize(length);
        length = ::GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), flags);
    }
    if (length == 0 || length >= name.size()) ThrowLastError("GetFinalPathNameByHandleW " + Shown(shown));
    name.resize(length);

    if (name.compare(0, 8, L"\\\\?\\UNC\\") == 0) {
        throw Refusal(strprintf("%s is on a network share. The service runs the daemon from a local disk: install "
                                "Gridcoin on one, then run install again.",
                                Shown(shown)));
    }
    if (name.compare(0, 4, L"\\\\?\\") == 0) name.erase(0, 4);

    return fs::path(name);
}

//! The daemon the service runs: gridcoinresearchd.exe in this program's own folder, as Windows
//! resolves its path. Refused when it is not there.
fs::path DaemonPath()
{
    const fs::path candidate = ModulePath().parent_path() / L"gridcoinresearchd.exe";
    const UniqueHandle handle(::CreateFileW(candidate.wstring().c_str(), FILE_READ_ATTRIBUTES,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!handle) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            throw Refusal(strprintf("the daemon %s is not there. The service runs the gridcoinresearchd.exe installed "
                                    "in the same folder as gridcoin-service.exe; run the gridcoin-service.exe that "
                                    "was installed with it.",
                                    Shown(candidate)));
        }
        ThrowWin32(error, "open " + Shown(candidate));
    }

    BY_HANDLE_FILE_INFORMATION info{};
    if (!::GetFileInformationByHandle(handle.get(), &info)) {
        ThrowLastError("read the attributes of " + Shown(candidate));
    }
    if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        throw Refusal(strprintf("%s is a directory, not the daemon", Shown(candidate)));
    }

    return FinalPathOf(handle.get(), candidate);
}

void RequireElevated(const char* verb)
{
    if (!winacl::CurrentTokenIsElevated()) {
        throw Refusal(strprintf("%s needs an elevated prompt. Open the Command Prompt or PowerShell with \"Run as "
                                "administrator\" and run it again.",
                                verb));
    }
}

void EnablePrivilege(const wchar_t* name)
{
    HANDLE raw = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &raw)) {
        ThrowLastError("OpenProcessToken");
    }
    const UniqueHandle token(raw);

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!::LookupPrivilegeValueW(nullptr, name, &privileges.Privileges[0].Luid)) {
        ThrowLastError("LookupPrivilegeValueW " + Shown(std::wstring(name)));
    }
    if (!::AdjustTokenPrivileges(token.get(), FALSE, &privileges, 0, nullptr, nullptr)) {
        ThrowLastError("AdjustTokenPrivileges " + Shown(std::wstring(name)));
    }
    // AdjustTokenPrivileges succeeds without enabling a privilege the token does not hold.
    if (::GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        throw Refusal(strprintf("this account's token does not hold %s, which this command needs. Run it as an "
                                "administrator, elevated.",
                                Shown(std::wstring(name))));
    }
}

// Accounts.

//! An account by name and SID, or by SID alone when it has no name.
std::string Describe(const Sid& sid)
{
    const std::string name = winacl::AccountNameOf(sid);
    const std::string text = winacl::FormatSid(sid);
    return name.empty() ? text : name + " (" + text + ")";
}

std::string ComputerName()
{
    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
    if (!::GetComputerNameW(computer, &length)) ThrowLastError("GetComputerNameW");
    return WideToUtf8(std::wstring_view(computer, length));
}

//! The SID of the local group \p name, looked up as the core looks up -ipcattachgroup: on this
//! computer only (<computer>\<name>), and only a local group of the computer's own. nullopt when
//! there is no account of that name. Refused when the name is another kind of account.
std::optional<Sid> LocalGroupSid(const std::wstring& name)
{
    const std::string computer = ComputerName();
    const std::string group = WideToUtf8(name);
    const std::optional<std::string> lookup = ipc::win::LocalGroupLookupName(computer, group);
    if (!lookup) throw std::logic_error("'" + group + "' is not a local group name");

    const std::optional<winacl::AccountLookup> account = winacl::LookupAccount(Utf8ToWide(*lookup));
    if (!account) return std::nullopt;
    if (std::optional<std::string> reason = ipc::win::AttachGroupAccountRefusal(*account, computer)) {
        throw Refusal(strprintf("\"%s\" cannot be the attach group: %s. Remove or rename that account, then run "
                                "install again.",
                                group, *reason));
    }
    return account->sid;
}

//! The attach group \p name, created with \p comment when there is none, and its SID.
Sid EnsureLocalGroup(const std::wstring& name, const std::wstring& comment)
{
    std::wstring group = name;
    std::wstring remark = comment;
    LOCALGROUP_INFO_1 info{group.data(), remark.data()};
    DWORD bad_parameter = 0;
    const NET_API_STATUS status = ::NetLocalGroupAdd(nullptr, 1, reinterpret_cast<LPBYTE>(&info), &bad_parameter);
    const bool created = status == NERR_Success;
    if (!created && status != ERROR_ALIAS_EXISTS && status != NERR_GroupExists) {
        throw std::runtime_error("NetLocalGroupAdd \"" + Shown(name) + "\": " + NetApiText(status));
    }

    const std::optional<Sid> sid = LocalGroupSid(name);
    if (!sid) throw std::runtime_error("the group \"" + Shown(name) + "\" cannot be found after it was added");

    Say(strprintf("Group \"%s\": %s, %s", Shown(name), created ? "created" : "already there", winacl::FormatSid(*sid)));
    return *sid;
}

//! Adds \p user to the local group \p group (NetLocalGroupAddMembers). False when it was a member
//! already.
bool AddGroupMember(const std::wstring& group, const Sid& user)
{
    Sid member = user;
    LOCALGROUP_MEMBERS_INFO_0 info{member.data()};
    const NET_API_STATUS status = ::NetLocalGroupAddMembers(nullptr, group.c_str(), 0, reinterpret_cast<LPBYTE>(&info), 1);
    if (status == NERR_Success) return true;
    if (status == ERROR_MEMBER_IN_ALIAS) return false;
    throw std::runtime_error("NetLocalGroupAddMembers \"" + Shown(group) + "\": " + NetApiText(status));
}

//! Removes \p user from the local group \p group (NetLocalGroupDelMembers). False when it was not a
//! member.
bool RemoveGroupMember(const std::wstring& group, const Sid& user)
{
    Sid member = user;
    LOCALGROUP_MEMBERS_INFO_0 info{member.data()};
    const NET_API_STATUS status = ::NetLocalGroupDelMembers(nullptr, group.c_str(), 0, reinterpret_cast<LPBYTE>(&info), 1);
    if (status == NERR_Success) return true;
    if (status == ERROR_MEMBER_NOT_IN_ALIAS) return false;
    throw std::runtime_error("NetLocalGroupDelMembers \"" + Shown(group) + "\": " + NetApiText(status));
}

//! The layout's accounts. The attach groups are left empty; the caller fills them in once they
//! are known.
LayoutSids ServiceSids()
{
    LayoutSids sids;
    sids.admins = winacl::wellknown::Administrators();
    sids.system = winacl::wellknown::System();
    const std::optional<Sid> svc_main = VirtualServiceSid(NamesFor(Network::Main).service);
    const std::optional<Sid> svc_test = VirtualServiceSid(NamesFor(Network::Test).service);
    if (!svc_main || !svc_test) throw std::logic_error("the service names have no service SID");
    sids.svc_main = *svc_main;
    sids.svc_test = *svc_test;
    return sids;
}

// The layout on disk.

bool IsNotFound(DWORD error)
{
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

//! \p path opened as itself (a reparse point at it is opened, not followed) with backup
//! semantics, which also opens a directory and, with SeBackupPrivilege enabled, gives read access
//! whatever the access list. A null handle, with the Win32 error in \p error, when it cannot be
//! opened.
UniqueHandle OpenAsItself(const fs::path& path, DWORD access, DWORD share, DWORD& error)
{
    UniqueHandle handle(::CreateFileW(path.wstring().c_str(), access, share, nullptr, OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    error = handle ? ERROR_SUCCESS : ::GetLastError();
    return handle;
}

//! What is at one path of the layout, read through a handle on it.
struct Level
{
    bool exists{false};
    bool directory{false};
    //! A reparse point of any kind.
    bool reparse{false};
    //! A reparse point that names another path: a junction, a symbolic link or a mount point.
    bool link{false};
    ObservedAcl acl;
};

Level LevelOf(HANDLE handle, const fs::path& path)
{
    FILE_ATTRIBUTE_TAG_INFO info{};
    if (!::GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info))) {
        ThrowLastError("read the attributes of " + Shown(path));
    }
    const fsmodes::WinEntry entry = fsmodes::ClassifyWinEntry(info.FileAttributes, info.ReparseTag);

    Level level;
    level.exists = true;
    level.directory = entry.directory;
    level.reparse = entry.reparse;
    level.link = entry.link;
    level.acl = winacl::ReadAcl(handle, /*kernel_object=*/false);
    return level;
}

//! \p path opened as itself with \p access and \p share, and what is there; a null handle and an
//! absent level when nothing is there. Throws when something is there that cannot be opened or
//! read.
std::pair<UniqueHandle, Level> OpenLevel(const fs::path& path, DWORD access, DWORD share)
{
    DWORD error = 0;
    UniqueHandle handle = OpenAsItself(path, access, share, error);
    if (!handle) {
        if (IsNotFound(error)) return {};
        if (error == winacl::WIN32_UNTRUSTED_MOUNT_POINT) {
            throw Refusal(strprintf("%s: %s", Shown(path), winacl::UNTRUSTED_MOUNT_POINT_REASON));
        }
        // Only a hold meets this: FILE_LIST_DIRECTORY is a data right, refused while another open
        // of the directory does not share reading, as one made with no sharing at all does not.
        if (error == ERROR_SHARING_VIOLATION) {
            throw Refusal(strprintf("%s is open in another process that does not share it, so install cannot hold "
                                    "it while it checks and changes it. Close that program, then run install again.",
                                    Shown(path)));
        }
        ThrowWin32(error, "open " + Shown(path));
    }
    Level level = LevelOf(handle.get(), path);
    return {std::move(handle), std::move(level)};
}

//! What is at \p path, without following a reparse point at it. Absent when nothing is there.
//! Throws when something is there that cannot be opened or read.
Level ReadLevel(const fs::path& path)
{
    return OpenLevel(path, READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
        .second;
}

//! The layout directory \p path held open as itself, without sharing delete, so that while it is
//! held its path keeps naming it: it cannot be renamed or removed. Opened to read its attributes
//! and access list and to list its entries, and with \p to_change also to set its owner and list.
std::pair<UniqueHandle, Level> HoldLevel(const fs::path& path, bool to_change)
{
    const DWORD access =
        READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | (to_change ? WRITE_DAC | WRITE_OWNER : 0);
    return OpenLevel(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE);
}

//! Each entry directly inside the directory \p dir, which \p handle holds with
//! FILE_LIST_DIRECTORY, opened as itself, so a link is read and never followed. An entry gone by
//! the time it is opened is left out. Throws when the directory cannot be listed.
std::vector<LevelEntryFacts> ReadLevelEntries(HANDLE handle, const fs::path& dir)
{
    std::vector<LevelEntryFacts> entries;
    // 8-byte aligned, as FILE_ID_BOTH_DIR_INFO is.
    std::vector<uint64_t> buffer(8192);
    FILE_INFO_BY_HANDLE_CLASS info_class = FileIdBothDirectoryRestartInfo;
    for (;;) {
        if (!::GetFileInformationByHandleEx(handle, info_class, buffer.data(),
                                            static_cast<DWORD>(buffer.size() * sizeof(uint64_t)))) {
            if (::GetLastError() == ERROR_NO_MORE_FILES) break;
            ThrowLastError("list " + Shown(dir));
        }
        info_class = FileIdBothDirectoryInfo;

        const auto* bytes = reinterpret_cast<const unsigned char*>(buffer.data());
        for (size_t offset = 0;;) {
            const auto* info = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(bytes + offset);
            const std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
            if (name != L"." && name != L"..") {
                const fs::path path = dir / name;
                DWORD error = 0;
                const UniqueHandle entry_handle = OpenAsItself(
                    path, READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    error);
                if (entry_handle) {
                    LevelEntryFacts facts;
                    facts.path = path;
                    try {
                        const Level level = LevelOf(entry_handle.get(), path);
                        facts.link = level.link;
                        facts.owner = level.acl.owner;
                        if (facts.owner) facts.owner_name = winacl::AccountNameOf(*facts.owner);
                    } catch (const std::exception&) {
                        // Its owner stays unread, which is refused.
                    }
                    entries.push_back(std::move(facts));
                } else if (!IsNotFound(error)) {
                    ThrowWin32(error, "open " + Shown(path));
                }
            }
            if (info->NextEntryOffset == 0) break;
            offset += info->NextEntryOffset;
        }
    }
    return entries;
}

//! Why install may not take over \p level, found at \p entry's path, or nullopt: it is a reparse
//! point, it is a file where the layout has a directory (or the reverse), or an account other
//! than Administrators, SYSTEM and TrustedInstaller owns it, which an account that could create
//! it before the service was installed would.
std::optional<std::string> LevelRefusal(const LayoutEntry& entry, const Level& level)
{
    const std::string path = Shown(entry.path);
    if (level.reparse) {
        return strprintf("%s is a reparse point (a junction or a link), which install does not follow. Remove it, "
                         "then run install again.",
                         path);
    }
    if (level.directory != entry.directory) {
        return strprintf("%s is a %s where install keeps a %s. Move it away, then run install again.", path,
                         level.directory ? "directory" : "file", entry.directory ? "directory" : "file");
    }

    const std::vector<Sid> owners{winacl::wellknown::Administrators(), winacl::wellknown::System(),
                                  winacl::wellknown::TrustedInstaller()};
    if (!level.acl.owner || std::find(owners.begin(), owners.end(), *level.acl.owner) == owners.end()) {
        return strprintf("%s is owned by %s, not by Administrators, SYSTEM or TrustedInstaller, so another account "
                         "may have made it before the service was installed. Check what it holds, remove it, then "
                         "run install again.",
                         path, level.acl.owner ? Describe(*level.acl.owner) : "an owner that cannot be read");
    }
    return std::nullopt;
}

//! The most install reads of a configuration file.
constexpr uint64_t CONF_SIZE_LIMIT = 1 << 20;

//! The service's configuration file as install and status check it.
struct ConfState
{
    bool exists{false};
    //! Why install does not accept it, when it does not.
    std::optional<std::string> problem;
    std::string text;

    ConfState() = default;
    ConfState(const ConfState&) = delete;
    ConfState& operator=(const ConfState&) = delete;
    ConfState(ConfState&&) = default;
    ConfState& operator=(ConfState&&) = default;
    ~ConfState() { memory_cleanse(text.data(), text.size()); }
};

void ReadAll(HANDLE handle, const fs::path& path, std::string& text)
{
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(handle, &size)) ThrowLastError("read the size of " + Shown(path));
    if (size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > CONF_SIZE_LIMIT) {
        throw Refusal(strprintf("%s is larger than %u bytes, too large for a configuration file", Shown(path),
                                static_cast<unsigned>(CONF_SIZE_LIMIT)));
    }

    text.assign(static_cast<size_t>(size.QuadPart), '\0');
    size_t done = 0;
    while (done < text.size()) {
        DWORD read = 0;
        if (!::ReadFile(handle, text.data() + done, static_cast<DWORD>(text.size() - done), &read, nullptr)) {
            ThrowLastError("read " + Shown(path));
        }
        if (read == 0) break;
        done += read;
    }
    text.resize(done);
}

void WriteAll(HANDLE handle, const fs::path& path, const std::string& text)
{
    size_t done = 0;
    while (done < text.size()) {
        DWORD written = 0;
        if (!::WriteFile(handle, text.data() + done, static_cast<DWORD>(text.size() - done), &written, nullptr)) {
            ThrowLastError("write " + Shown(path));
        }
        // A write that succeeds and writes nothing would never finish.
        if (written == 0) throw std::runtime_error("write " + Shown(path) + ": nothing was written");
        done += written;
    }
    if (!::FlushFileBuffers(handle)) ThrowLastError("flush " + Shown(path));
}

//! The configuration file at \p entry's path, opened as itself with GENERIC_READ and
//! \p extra_access: absent, or there, with its text and why install does not accept it, if it
//! does not: a reparse point, a directory, another owner or access list than \p entry's, or an
//! includeconf line. The handle is kept in \p keep, when given, for a file there.
ConfState CheckConf(const LayoutEntry& entry, DWORD extra_access, UniqueHandle* keep)
{
    DWORD error = 0;
    UniqueHandle handle = OpenAsItself(entry.path, GENERIC_READ | extra_access, FILE_SHARE_READ, error);
    if (!handle) {
        if (IsNotFound(error)) return {};
        ThrowWin32(error, "open " + Shown(entry.path));
    }

    ConfState conf;
    conf.exists = true;
    const Level level = LevelOf(handle.get(), entry.path);
    if (level.reparse) {
        conf.problem = "it is a reparse point (a link)";
    } else if (level.directory) {
        conf.problem = "it is a directory";
    } else if (std::optional<std::string> differs = winacl::CheckMatchesSpec(level.acl, entry.acl)) {
        conf.problem = "its owner or access list is not the one install gives it (" + *differs + ")";
    } else {
        ReadAll(handle.get(), entry.path, conf.text);
        if (!ServiceConfAcceptable(conf.text)) conf.problem = "it has an includeconf line";
    }

    if (keep) *keep = std::move(handle);
    return conf;
}

std::string ConfRefusal(const fs::path& path, const std::string& problem)
{
    return strprintf("the configuration file %s cannot be used: %s. The CLI honours rpcserviceuser only from an "
                     "Administrators-owned file with install's access list and no includeconf, so it would refuse "
                     "the service's RPC listener. Correct the file, or move it away for install to write a new one, "
                     "then run install again.",
                     Shown(path), problem);
}

//! The base58 text of 32 random bytes.
std::string NewRpcPassword()
{
    std::array<unsigned char, 32> bytes{};
    GetStrongRandBytes(bytes);
    std::string password = EncodeBase58(bytes);
    memory_cleanse(bytes.data(), bytes.size());
    return password;
}

//! The names \p lines set, for a message that does not show their values.
std::string NamesSet(const std::string& lines)
{
    std::string names;
    size_t start = 0;
    while (start < lines.size()) {
        size_t end = lines.find('\n', start);
        if (end == std::string::npos) end = lines.size();
        const std::string line = lines.substr(start, end - start);
        if (!line.empty()) names += (names.empty() ? "" : ", ") + line.substr(0, line.find('='));
        start = end + 1;
    }
    return names;
}

//! Creates the configuration file \p entry.path with \p text, owned and listed as \p entry says
//! from its creation, a link at the name not followed. The list is read back through the new
//! handle before anything is written. On a failure after the file was made it is removed again.
void CreateConf(const LayoutEntry& entry, const std::string& text)
{
    const winacl::UniqueSecurityDescriptor sd(winacl::MakeSecurityDescriptor(entry.acl));
    SECURITY_ATTRIBUTES sa{sizeof(sa), sd.get(), FALSE};
    UniqueHandle handle(::CreateFileW(entry.path.wstring().c_str(), GENERIC_WRITE | READ_CONTROL, 0, &sa, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!handle) ThrowLastError("create " + Shown(entry.path));

    try {
        if (std::optional<std::string> differs =
                winacl::CheckMatchesSpec(winacl::ReadAcl(handle.get(), /*kernel_object=*/false), entry.acl)) {
            throw std::runtime_error(strprintf("%s did not keep the access list it was created with (%s)",
                                               Shown(entry.path), *differs));
        }
        WriteAll(handle.get(), entry.path, text);
    } catch (...) {
        handle.reset();
        ::DeleteFileW(entry.path.wstring().c_str());
        throw;
    }
}

//! The network's configuration file: written when there is none, and otherwise given the lines
//! MissingServiceConfLines finds missing. A file install does not accept was refused before
//! anything was changed (Preflight); it is checked again here, through the handle that writes.
void BuildConf(const LayoutEntry& entry, Network net)
{
    UniqueHandle handle;
    const ConfState conf = CheckConf(entry, FILE_APPEND_DATA, &handle);

    std::string password = NewRpcPassword();
    if (!conf.exists) {
        std::string text = BuildServiceConf(net, password);
        CreateConf(entry, text);
        memory_cleanse(text.data(), text.size());
        memory_cleanse(password.data(), password.size());
        Say(strprintf("Wrote %s: rpcuser, a new rpcpassword, server=1 and rpcserviceuser=%s%s", Shown(entry.path),
                      Shown(NamesFor(net).account), NamesFor(net).testnet ? ", testnet=1" : ""));
        return;
    }
    if (conf.problem) {
        memory_cleanse(password.data(), password.size());
        throw Refusal(ConfRefusal(entry.path, *conf.problem));
    }

    std::string missing = MissingServiceConfLines(net, conf.text, password);
    memory_cleanse(password.data(), password.size());
    if (missing.empty()) {
        Say(strprintf("Kept %s: it already sets rpcuser, rpcpassword and rpcserviceuser", Shown(entry.path)));
        return;
    }

    // Opened without FILE_WRITE_DATA, so every write goes to the end of the file.
    try {
        WriteAll(handle.get(), entry.path, missing);
    } catch (...) {
        memory_cleanse(missing.data(), missing.size());
        throw;
    }
    Say(strprintf("Added %s to %s", NamesSet(missing), Shown(entry.path)));
    memory_cleanse(missing.data(), missing.size());
}

//! Why install refuses the entries directly inside the layout directory \p entry, held as
//! \p handle, or nullopt: each one LevelEntryRefusal refuses, named with its owner or as a link.
std::optional<std::string> PlantedEntries(const std::vector<LayoutEntry>& layout, const fs::path& programdata,
                                          const LayoutSids& sids, const LayoutEntry& entry, HANDLE handle)
{
    std::vector<std::string> found;
    for (const LevelEntryFacts& facts : ReadLevelEntries(handle, entry.path)) {
        if (std::optional<std::string> reason = LevelEntryRefusal(layout, programdata, entry.path, facts, sids)) {
            found.push_back(std::move(*reason));
        }
    }
    if (found.empty()) return std::nullopt;

    // Moved out, not removed: what is named can be a wallet.
    return strprintf("%s holds entries install did not make, which another account may have put there: %s. Check "
                     "what each one is, move it out of %s, then run install again.",
                     Shown(entry.path), Join(found, "; "), Shown(entry.path));
}

//! Everything install would refuse in place, looked at before anything is changed: each
//! directory of the layout that exists (LevelRefusal) and the entries directly inside it
//! (PlantedEntries, for every level: ChecksLevelEntries), and the network's configuration file
//! when it exists (CheckConf). \p sids names the groups that exist already; a group install has yet
//! to create is empty, which only makes the lists that name it differ.
void Preflight(const std::vector<LayoutEntry>& layout, Network net, const fs::path& programdata,
               const LayoutSids& sids)
{
    const fs::path own_conf = ConfPath(programdata, net);
    for (const LayoutEntry& entry : layout) {
        if (entry.path == own_conf) {
            const ConfState conf = CheckConf(entry, 0, nullptr);
            if (conf.exists && conf.problem) throw Refusal(ConfRefusal(entry.path, *conf.problem));
            continue;
        }
        if (!entry.directory) continue;

        const auto [handle, level] = HoldLevel(entry.path, /*to_change=*/false);
        if (!level.exists) continue;
        if (std::optional<std::string> refusal = LevelRefusal(entry, level)) throw Refusal(*refusal);

        // A list that is the layout's now says who can add an entry now, not who added one before.
        const bool list_matches = !winacl::CheckMatchesSpec(level.acl, entry.acl);
        if (ChecksLevelEntries(entry, list_matches)) {
            if (std::optional<std::string> planted = PlantedEntries(layout, programdata, sids, entry, handle.get())) {
                throw Refusal(*planted);
            }
        }
    }
}

//! Gives the level \p entry, held as \p handle with WRITE_DAC and WRITE_OWNER, its owner and
//! protected access list through that handle (SetKernelObjectSecurity), which changes the level
//! alone and passes nothing down to what it holds, and confirms them through the handle.
void SetLevelListThroughHandle(HANDLE handle, const LayoutEntry& entry)
{
    const winacl::UniqueSecurityDescriptor sd(winacl::MakeSecurityDescriptor(entry.acl));
    if (!::SetKernelObjectSecurity(handle,
                                   OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
                                       PROTECTED_DACL_SECURITY_INFORMATION,
                                   sd.get())) {
        ThrowLastError("set the owner and access list of " + Shown(entry.path));
    }
    if (std::optional<std::string> not_kept =
            winacl::CheckMatchesSpec(winacl::ReadAcl(handle, /*kernel_object=*/false), entry.acl)) {
        throw std::runtime_error(strprintf("%s did not keep the layout's owner and access list (%s)",
                                           Shown(entry.path), *not_kept));
    }
}

//! Each directory of \p layout, parents first: created with its owner and access list from the
//! start (CreateDirectoryWithAcl), or, when it is there, held open without sharing delete and
//! checked again (LevelRefusal). A level whose list differs is first given the layout's owner and
//! list through the handle held, without passing anything down (SetLevelListThroughHandle), so no
//! other account can add an entry from then on. Then the entries directly inside every level are
//! checked (PlantedEntries). Then a level that differed is given the list by its path (ApplyAcl),
//! which passes the inherited entries down to what it holds, and the list is confirmed through
//! the handle. Then the network's configuration file (BuildConf). The other network's
//! configuration file is its own install's.
void BuildLayout(const std::vector<LayoutEntry>& layout, Network net, const fs::path& programdata,
                 const LayoutSids& sids)
{
    const fs::path own_conf = ConfPath(programdata, net);
    for (const LayoutEntry& entry : layout) {
        if (!entry.directory) {
            if (entry.path == own_conf) BuildConf(entry, net);
            continue;
        }

        if (winacl::CreateDirectoryWithAcl(entry.path, entry.acl)) {
            Say("Created " + Shown(entry.path));
            continue;
        }

        const auto [handle, level] = HoldLevel(entry.path, /*to_change=*/true);
        if (!level.exists) throw std::runtime_error(Shown(entry.path) + " was removed while install ran");
        if (std::optional<std::string> refusal = LevelRefusal(entry, level)) throw Refusal(*refusal);

        const std::optional<std::string> differed = winacl::CheckMatchesSpec(level.acl, entry.acl);
        if (differed) SetLevelListThroughHandle(handle.get(), entry);

        // A refusal from here on leaves the level with its layout list, closed to other accounts,
        // and the entry named in it, which a rerun refuses again.
        if (ChecksLevelEntries(entry, !differed)) {
            if (std::optional<std::string> planted = PlantedEntries(layout, programdata, sids, entry, handle.get())) {
                throw Refusal(*planted);
            }
        }

        if (!differed) {
            Say(strprintf("Kept %s: it has the layout's owner and access list", Shown(entry.path)));
            continue;
        }

        // By the path, which the handle held keeps naming the directory checked.
        winacl::ApplyAcl(entry.path, entry.acl);
        if (std::optional<std::string> not_kept =
                winacl::CheckMatchesSpec(winacl::ReadAcl(handle.get(), /*kernel_object=*/false), entry.acl)) {
            throw std::runtime_error(strprintf("%s did not keep the layout's owner and access list (%s)",
                                               Shown(entry.path), *not_kept));
        }
        Say(strprintf("Set the owner and access list of %s (before: %s)", Shown(entry.path), *differed));
    }
}

// The firewall.

//! The null device, opened so that a child process can inherit it.
UniqueHandle OpenNullDevice()
{
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    UniqueHandle null_device(::CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!null_device) ThrowLastError("open NUL");
    return null_device;
}

//! An inheritable copy of this process's standard handle \p which, or nullopt when it has none.
std::optional<UniqueHandle> InheritableStdHandle(DWORD which)
{
    const HANDLE handle = ::GetStdHandle(which);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return std::nullopt;

    HANDLE copy = nullptr;
    if (!::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(), &copy, 0, TRUE,
                           DUPLICATE_SAME_ACCESS)) {
        return std::nullopt;
    }
    return UniqueHandle(copy);
}

struct AttributeListDeleter
{
    void operator()(LPPROC_THREAD_ATTRIBUTE_LIST list) const noexcept { ::DeleteProcThreadAttributeList(list); }
};

//! Runs netsh.exe from the system directory with \p args and returns its exit code. Its standard
//! handles are this program's, so its output goes where this program's goes, a file it is
//! redirected to included; with \p quiet its output goes to the null device instead. Those three
//! are the only handles it inherits.
DWORD RunNetsh(const std::wstring& args, bool quiet)
{
    std::wstring system(MAX_PATH, L'\0');
    const UINT length = ::GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
    if (length == 0 || length >= system.size()) ThrowLastError("GetSystemDirectoryW");
    system.resize(length);

    const std::wstring netsh = system + L"\\netsh.exe";
    std::wstring command = QuoteArg(netsh) + L" " + args;
    if (!quiet) Say("Running: netsh.exe " + Shown(args));

    // A missing standard handle, or output that is not wanted, is the null device.
    const UniqueHandle null_device = OpenNullDevice();
    std::optional<UniqueHandle> input = InheritableStdHandle(STD_INPUT_HANDLE);
    std::optional<UniqueHandle> output = quiet ? std::nullopt : InheritableStdHandle(STD_OUTPUT_HANDLE);
    std::optional<UniqueHandle> error = quiet ? std::nullopt : InheritableStdHandle(STD_ERROR_HANDLE);
    const auto handle_of = [&null_device](const std::optional<UniqueHandle>& handle) {
        return handle ? static_cast<HANDLE>(handle->get()) : static_cast<HANDLE>(null_device.get());
    };

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = handle_of(input);
    startup.StartupInfo.hStdOutput = handle_of(output);
    startup.StartupInfo.hStdError = handle_of(error);

    // Each handle once: the list may not repeat one.
    std::vector<HANDLE> inherited{startup.StartupInfo.hStdInput, startup.StartupInfo.hStdOutput,
                                  startup.StartupInfo.hStdError};
    std::sort(inherited.begin(), inherited.end(), std::less<HANDLE>());
    inherited.erase(std::unique(inherited.begin(), inherited.end()), inherited.end());

    SIZE_T list_size = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &list_size);
    std::vector<unsigned char> list_buffer(list_size);
    const auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(list_buffer.data());
    if (!::InitializeProcThreadAttributeList(list, 1, 0, &list_size)) {
        ThrowLastError("InitializeProcThreadAttributeList");
    }
    const std::unique_ptr<std::remove_pointer_t<LPPROC_THREAD_ATTRIBUTE_LIST>, AttributeListDeleter> list_guard(list);
    if (!::UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(),
                                     inherited.size() * sizeof(HANDLE), nullptr, nullptr)) {
        ThrowLastError("UpdateProcThreadAttribute");
    }
    startup.lpAttributeList = list;

    PROCESS_INFORMATION process{};
    if (!::CreateProcessW(netsh.c_str(), command.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT,
                          nullptr, nullptr, &startup.StartupInfo, &process)) {
        ThrowLastError("run " + Shown(netsh));
    }
    const UniqueHandle process_handle(process.hProcess);
    const UniqueHandle thread_handle(process.hThread);

    if (::WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0) ThrowLastError("wait for netsh.exe");
    DWORD code = 0;
    if (!::GetExitCodeProcess(process.hProcess, &code)) ThrowLastError("read the exit code of netsh.exe");
    return code;
}

//! The network's inbound rule for \p exe, by the steps FirewallSteps gives for the rules that are
//! there: a rule already there is replaced by adding the new one under FirewallStagingRuleName,
//! deleting the old one and renaming the new one, and a staged rule a stopped install left is
//! deleted only while the rule is there, and otherwise made the rule. No failure leaves the
//! machine without the rule it had.
void AddFirewallRule(Network net, const fs::path& exe)
{
    const std::wstring rule = FirewallRuleName(net);
    const std::wstring staging = FirewallStagingRuleName(net);
    const std::wstring rename = FirewallRenameRuleArgs(staging, rule);
    const bool rule_exists = RunNetsh(FirewallShowRuleArgs(rule), /*quiet=*/true) == 0;
    const bool staging_exists = RunNetsh(FirewallShowRuleArgs(staging), /*quiet=*/true) == 0;

    for (const FirewallStep step : FirewallSteps(rule_exists, staging_exists)) {
        DWORD code = 0;
        std::string failed;
        switch (step) {
        case FirewallStep::AddRule:
            code = RunNetsh(FirewallAddRuleArgs(net, exe, rule), /*quiet=*/false);
            failed = strprintf("could not add the firewall rule \"%s\". Add it from an elevated prompt with: "
                               "netsh.exe %s",
                               Shown(rule), Shown(FirewallAddRuleArgs(net, exe, rule)));
            break;
        case FirewallStep::AddStaging:
            code = RunNetsh(FirewallAddRuleArgs(net, exe, staging), /*quiet=*/false);
            failed = strprintf("could not add the new firewall rule; the rule \"%s\" that was there is kept as it was",
                               Shown(rule));
            break;
        case FirewallStep::DeleteRule:
            code = RunNetsh(FirewallDeleteRuleArgs(rule), /*quiet=*/false);
            failed = strprintf("could not delete the old firewall rule \"%s\", so both it and \"%s\" are there. Delete "
                               "the old one, then rename the new one from an elevated prompt with: netsh.exe %s",
                               Shown(rule), Shown(staging), Shown(rename));
            break;
        case FirewallStep::DeleteStaging:
            code = RunNetsh(FirewallDeleteRuleArgs(staging), /*quiet=*/false);
            failed = strprintf("could not delete the firewall rule \"%s\" an earlier install left; the rule \"%s\" is "
                               "kept as it was",
                               Shown(staging), Shown(rule));
            break;
        case FirewallStep::RenameStagingToRule:
            code = RunNetsh(rename, /*quiet=*/false);
            failed = strprintf("could not rename the firewall rule \"%s\" to \"%s\"; it allows the same. Rename it "
                               "from an elevated prompt with: netsh.exe %s",
                               Shown(staging), Shown(rule), Shown(rename));
            break;
        }
        if (code != 0) {
            throw std::runtime_error(strprintf("netsh.exe %s (exit code %u)", failed, static_cast<unsigned>(code)));
        }
    }

    Say(strprintf("Firewall rule \"%s\": %s; inbound TCP to the daemon, on domain and private networks",
                  Shown(rule), rule_exists || staging_exists ? "replaced" : "added"));
}

// The service's settings.

std::vector<unsigned char> QueryConfig(SC_HANDLE service)
{
    DWORD needed = 0;
    if (!::QueryServiceConfigW(service, nullptr, 0, &needed) && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        ThrowLastError("QueryServiceConfigW");
    }
    std::vector<unsigned char> buffer(std::max<DWORD>(needed, sizeof(QUERY_SERVICE_CONFIGW)));
    if (!::QueryServiceConfigW(service, reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data()),
                               static_cast<DWORD>(buffer.size()), &needed)) {
        ThrowLastError("QueryServiceConfigW");
    }
    return buffer;
}

//! What QueryServiceConfig2W gives for \p level, in a buffer at least \p minimum bytes long.
std::vector<unsigned char> QueryConfig2(SC_HANDLE service, DWORD level, size_t minimum, const char* what)
{
    DWORD needed = 0;
    if (!::QueryServiceConfig2W(service, level, nullptr, 0, &needed) && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        ThrowLastError(std::string("QueryServiceConfig2W ") + what);
    }
    std::vector<unsigned char> buffer(std::max<size_t>(needed, minimum));
    if (!::QueryServiceConfig2W(service, level, buffer.data(), static_cast<DWORD>(buffer.size()), &needed)) {
        ThrowLastError(std::string("QueryServiceConfig2W ") + what);
    }
    return buffer;
}

SERVICE_STATUS_PROCESS QueryStatus(SC_HANDLE service)
{
    SERVICE_STATUS_PROCESS status{};
    DWORD needed = 0;
    if (!::QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status),
                                &needed)) {
        ThrowLastError("QueryServiceStatusEx");
    }
    return status;
}

std::string StateName(DWORD state)
{
    switch (state) {
    case SERVICE_STOPPED: return "stopped";
    case SERVICE_START_PENDING: return "starting";
    case SERVICE_STOP_PENDING: return "stopping";
    case SERVICE_RUNNING: return "running";
    case SERVICE_CONTINUE_PENDING: return "continuing";
    case SERVICE_PAUSE_PENDING: return "pausing";
    case SERVICE_PAUSED: return "paused";
    }
    return strprintf("state %u", static_cast<unsigned>(state));
}

std::string StartTypeName(DWORD start_type, bool delayed)
{
    switch (start_type) {
    case SERVICE_AUTO_START: return delayed ? "automatic (delayed start)" : "automatic";
    case SERVICE_DEMAND_START: return "manual";
    case SERVICE_DISABLED: return "disabled";
    case SERVICE_BOOT_START: return "boot";
    case SERVICE_SYSTEM_START: return "system";
    }
    return strprintf("start type %u", static_cast<unsigned>(start_type));
}

std::string SidTypeName(DWORD sid_type)
{
    if (sid_type == SID_TYPE_RESTRICTED) return "restricted";
    if (sid_type == SID_TYPE_UNRESTRICTED) return "unrestricted";
    if (sid_type == SERVICE_SID_TYPE_NONE) return "none";
    return strprintf("type %u", static_cast<unsigned>(sid_type));
}

bool SamePath(const std::wstring& a, const std::wstring& b)
{
    // As NTFS compares names: without regard to case, by Windows' own upper-case table.
    return ::CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()),
                                  TRUE) == CSTR_EQUAL;
}

//! Prints the service's settings, each with what install sets when it differs. False when one
//! differs.
bool ShowServiceSettings(SC_HANDLE service, Network net, const std::optional<fs::path>& exe,
                         const fs::path& programdata)
{
    const NetworkNames names = NamesFor(net);
    bool matches = true;
    const auto note = [&matches](bool ok, const std::string& expected) {
        if (ok) return std::string();
        matches = false;
        return " (install sets " + expected + ")";
    };

    const SERVICE_STATUS_PROCESS status = QueryStatus(service);
    std::string state = StateName(status.dwCurrentState);
    if (status.dwCurrentState == SERVICE_RUNNING) {
        state += strprintf(", process %u", static_cast<unsigned>(status.dwProcessId));
    }
    if (status.dwCurrentState == SERVICE_STOPPED && status.dwWin32ExitCode != NO_ERROR) {
        state += strprintf(", last exit %u/%u", static_cast<unsigned>(status.dwWin32ExitCode),
                           static_cast<unsigned>(status.dwServiceSpecificExitCode));
    }
    Say("  State: " + state);

    const std::vector<unsigned char> config_buffer = QueryConfig(service);
    const auto* config = reinterpret_cast<const QUERY_SERVICE_CONFIGW*>(config_buffer.data());
    const std::vector<unsigned char> delayed_buffer =
        QueryConfig2(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, sizeof(SERVICE_DELAYED_AUTO_START_INFO),
                     "delayed automatic start");
    const bool delayed_start =
        reinterpret_cast<const SERVICE_DELAYED_AUTO_START_INFO*>(delayed_buffer.data())->fDelayedAutostart != FALSE;
    Say("  Start type: " + StartTypeName(config->dwStartType, delayed_start) +
        note(config->dwStartType == SERVICE_DISABLED || (config->dwStartType == SERVICE_AUTO_START && delayed_start),
             "disabled, and automatic with a delayed start once a wallet is moved in"));

    const std::wstring account = config->lpServiceStartName ? config->lpServiceStartName : L"";
    Say("  Account: " + Shown(account) + note(SamePath(account, names.account), Shown(names.account)));

    const std::vector<unsigned char> sid_buffer =
        QueryConfig2(service, SERVICE_CONFIG_SERVICE_SID_INFO, sizeof(SERVICE_SID_INFO), "SID type");
    const DWORD sid_type = reinterpret_cast<const SERVICE_SID_INFO*>(sid_buffer.data())->dwServiceSidType;
    Say("  SID type: " + SidTypeName(sid_type) +
        note(sid_type == SID_TYPE_RESTRICTED || sid_type == SID_TYPE_UNRESTRICTED,
             "restricted, or unrestricted with --sid-type=unrestricted"));

    const std::vector<unsigned char> privileges_buffer =
        QueryConfig2(service, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, sizeof(SERVICE_REQUIRED_PRIVILEGES_INFOW),
                     "required privileges");
    std::wstring privileges;
    for (const wchar_t* p = reinterpret_cast<const SERVICE_REQUIRED_PRIVILEGES_INFOW*>(privileges_buffer.data())
                                ->pmszRequiredPrivileges;
         p && *p; p += wcslen(p) + 1) {
        privileges += (privileges.empty() ? L"" : L", ") + std::wstring(p);
    }
    Say("  Required privileges: " + (privileges.empty() ? std::string("(all the account holds)") : Shown(privileges)) +
        note(privileges == REQUIRED_PRIVILEGE, Shown(std::wstring(REQUIRED_PRIVILEGE)) + " alone"));

    const std::vector<unsigned char> preshutdown_buffer =
        QueryConfig2(service, SERVICE_CONFIG_PRESHUTDOWN_INFO, sizeof(SERVICE_PRESHUTDOWN_INFO), "preshutdown timeout");
    const DWORD preshutdown =
        reinterpret_cast<const SERVICE_PRESHUTDOWN_INFO*>(preshutdown_buffer.data())->dwPreshutdownTimeout;
    Say(strprintf("  Preshutdown timeout: %u ms", static_cast<unsigned>(preshutdown)) +
        note(preshutdown == PRESHUTDOWN_TIMEOUT_MS, strprintf("%u ms", PRESHUTDOWN_TIMEOUT_MS)));

    const std::vector<unsigned char> failure_buffer =
        QueryConfig2(service, SERVICE_CONFIG_FAILURE_ACTIONS, sizeof(SERVICE_FAILURE_ACTIONSW), "failure actions");
    const auto* failure = reinterpret_cast<const SERVICE_FAILURE_ACTIONSW*>(failure_buffer.data());
    std::string actions;
    bool actions_match = failure->cActions == FAILURE_RESTART_DELAYS_MS.size() &&
                         failure->dwResetPeriod == FAILURE_RESET_SECONDS;
    for (DWORD i = 0; i < failure->cActions; ++i) {
        const SC_ACTION& action = failure->lpsaActions[i];
        const std::string described = action.Type == SC_ACTION_RESTART
                                          ? strprintf("restart after %u ms", static_cast<unsigned>(action.Delay))
                                          : strprintf("action %u", static_cast<unsigned>(action.Type));
        actions += (actions.empty() ? "" : ", ") + described;
        if (i < FAILURE_RESTART_DELAYS_MS.size() &&
            (action.Type != SC_ACTION_RESTART || action.Delay != FAILURE_RESTART_DELAYS_MS[i])) {
            actions_match = false;
        }
    }
    const std::vector<unsigned char> flag_buffer =
        QueryConfig2(service, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, sizeof(SERVICE_FAILURE_ACTIONS_FLAG),
                     "failure actions flag");
    const auto* flag = reinterpret_cast<const SERVICE_FAILURE_ACTIONS_FLAG*>(flag_buffer.data());
    const bool on_error = flag->fFailureActionsOnNonCrashFailures != FALSE;
    Say(strprintf("  Failure actions: %s; reset after %u s; also after a stop with an error: %s",
                  actions.empty() ? std::string("none") : actions, static_cast<unsigned>(failure->dwResetPeriod),
                  on_error ? "yes" : "no") +
        note(actions_match && on_error,
             strprintf("a restart after %u, %u and %u ms, reset after %u s, also after a stop with an error",
                       FAILURE_RESTART_DELAYS_MS[0], FAILURE_RESTART_DELAYS_MS[1], FAILURE_RESTART_DELAYS_MS[2],
                       FAILURE_RESET_SECONDS)));

    const std::wstring image_path = config->lpBinaryPathName ? config->lpBinaryPathName : L"";
    Say("  ImagePath: " + Shown(image_path));
    const std::optional<std::wstring> image_exe = ImagePathExecutable(image_path);
    if (!exe) {
        matches = false;
        Say("  ImagePath check: not made, as the daemon next to this program was not found");
    } else if (!image_exe || !SamePath(*image_exe, exe->wstring())) {
        matches = false;
        Say(strprintf("  ImagePath check: it runs %s, not the daemon next to this program, %s. Run install "
                      "--reconfigure from the Gridcoin installation the service should run.",
                      image_exe ? Shown(*image_exe) : std::string("no executable"), Shown(*exe)));
    } else if (image_path != BuildImagePath(*exe, net, programdata)) {
        matches = false;
        Say("  ImagePath check: its arguments differ from the ones install writes, " +
            Shown(BuildImagePath(*exe, net, programdata)) + ". Run install --reconfigure to apply them.");
    } else {
        Say("  ImagePath check: the command line install writes, for the daemon next to this program");
    }

    return matches;
}

//! One layout entry as status reports it, and whether it matches.
std::pair<std::string, bool> EntryStatus(const LayoutEntry& entry)
{
    Level level;
    try {
        level = ReadLevel(entry.path);
    } catch (const std::exception& e) {
        return {std::string("could not be read: ") + AsciiWhat(e), false};
    }
    if (!level.exists) return {"absent", false};
    if (level.reparse) return {"a reparse point (a junction or a link)", false};
    if (level.directory != entry.directory) {
        return {level.directory ? "a directory, not a file" : "a file, not a directory", false};
    }
    if (std::optional<std::string> differs = winacl::CheckMatchesSpec(level.acl, entry.acl)) {
        return {"differs from the layout: " + *differs, false};
    }
    return {"ok", true};
}

//! The network's state file as status and disable read it: opened as itself, and read only when
//! it is a plain file that is the administrators' alone (StateFileRefusal).
struct StateFile
{
    bool exists{false};
    //! A link, a directory or another kind of reparse point, which is not read.
    bool plain{true};
    //! Why it was not read when it is plain: its owner or access list, with the owner named.
    std::optional<std::string> refusal;
    std::string text;
};

//! \p path read as StateFile says. Throws when it is there and cannot be opened or read.
StateFile ReadStateFile(const fs::path& path)
{
    StateFile state;
    DWORD error = 0;
    const UniqueHandle handle =
        OpenAsItself(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, error);
    if (!handle) {
        if (IsNotFound(error)) return state;
        ThrowWin32(error, "open " + Shown(path));
    }
    state.exists = true;

    const Level level = LevelOf(handle.get(), path);
    if (level.reparse || level.directory) {
        state.plain = false;
        return state;
    }
    if (const std::optional<std::string> refusal = StateFileRefusal(level.acl)) {
        state.refusal = strprintf("%s; it is owned by %s", *refusal,
                                  level.acl.owner ? Describe(*level.acl.owner) : std::string("an owner that cannot be read"));
        return state;
    }
    ReadAll(handle.get(), path, state.text);
    return state;
}

//! Prints whose wallet the service holds, from the network's state file, when the file is the
//! administrators' alone (StateFileRefusal); otherwise that it was not read, and why.
void ShowStateFile(const fs::path& path)
{
    try {
        const StateFile state = ReadStateFile(path);
        if (!state.exists) {
            Say("Wallet: none moved in (no " + Shown(path) + ")");
            return;
        }
        if (!state.plain) {
            Say("Wallet: " + Shown(path) + " is not a plain file, so it was not read");
            return;
        }
        if (state.refusal) {
            Say("Wallet: " + Shown(path) + " was not read, as another account may have written it: " + *state.refusal);
            return;
        }
        const std::string& text = state.text;

        const std::optional<std::string> user_sid = StateFileValue(text, "user_sid");
        const std::optional<Sid> sid = user_sid ? winacl::ParseSidString(*user_sid) : std::nullopt;
        const std::string user = sid ? Describe(*sid) : user_sid ? "'" + *user_sid + "' (not a SID)" : "nobody named";
        Say(strprintf("Wallet: moved in from %s by enable for %s, at %s", StateFileValue(text, "from").value_or("?"),
                      user, StateFileValue(text, "enabled_at").value_or("?")));
    } catch (const std::exception& e) {
        Say("Wallet: " + Shown(path) + " could not be read: " + AsciiWhat(e));
    }
}

//! Whether something is at \p path, opened as itself with backup semantics. Throws when that
//! cannot be told.
bool Exists(const fs::path& path)
{
    DWORD error = 0;
    const UniqueHandle handle =
        OpenAsItself(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, error);
    if (handle) return true;
    if (IsNotFound(error)) return false;
    ThrowWin32(error, "check for " + Shown(path));
}

//! Removes each directory of \p dirs, in order, when it is empty, and says what it kept and why.
void RemoveEmptyDirectories(const std::vector<fs::path>& dirs)
{
    for (const fs::path& dir : dirs) {
        Level level;
        try {
            level = ReadLevel(dir);
        } catch (const std::exception& e) {
            Say("Kept " + Shown(dir) + ": " + AsciiWhat(e));
            continue;
        }
        if (!level.exists) continue;
        if (level.reparse || !level.directory) {
            Say("Kept " + Shown(dir) + ": it is not a plain directory");
            continue;
        }

        if (::RemoveDirectoryW(dir.wstring().c_str())) {
            Say("Removed " + Shown(dir));
            continue;
        }
        const DWORD error = ::GetLastError();
        Say("Kept " + Shown(dir) + ": " + (error == ERROR_DIR_NOT_EMPTY ? "it is not empty" : Win32ErrorText(error)));
    }
}

void InstallService(const ToolOptions& options)
{
    const NetworkNames names = NamesFor(options.net);
    Say(strprintf("Installing the %s service %s.", Shown(names.datadir_leaf), Shown(names.service)));

    RequireElevated("install");
    // SeRestorePrivilege gives the layout an owner other than this account and writes the access
    // list of an entry whose list does not let this account; SeBackupPrivilege reads any entry.
    EnablePrivilege(SE_RESTORE_NAME);
    EnablePrivilege(SE_BACKUP_NAME);

    const fs::path exe = DaemonPath();
    Say("Daemon: " + Shown(exe));
    // The daemon refuses -service from a folder another account can change; so does install,
    // before anything is changed.
    if (const std::optional<std::string> writer =
            winacl::FindForeignWriterOnChain(exe, util::winsvc::ImageDirectoryAllowed(/*service=*/true, Sid{}))) {
        throw Refusal(strprintf("%s, or a directory above it, can be changed by an account other than SYSTEM, "
                                "Administrators and TrustedInstaller: %s. The service refuses to start from there. "
                                "Install Gridcoin where only administrators can change it, such as the default folder "
                                "under Program Files, then run install again.",
                                Shown(exe), *writer));
    }

    const fs::path programdata = ProgramDataPath();
    Say("Data: " + Shown(GridcoinRoot(programdata)));
    const std::wstring image_path = BuildImagePath(exe, options.net, programdata);
    // The service manager expands %NAME% in a command line from the system's environment.
    if (image_path.find(L'%') != std::wstring::npos) {
        throw Refusal(strprintf("the service's command line, %s, holds a '%%', which the service manager would read "
                                "as part of an environment variable. Install Gridcoin in a folder whose path has none.",
                                Shown(image_path)));
    }

    const UniqueServiceHandle scm = OpenScm(SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
    const DWORD service_access = SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS;
    UniqueServiceHandle service = OpenServiceIfInstalled(scm.get(), names.service, service_access);
    if (service && !options.reconfigure) {
        throw Refusal(strprintf("the service %s is already installed. To give it this release's settings, run "
                                "gridcoin-service.exe install --reconfigure%s.",
                                Shown(names.service), options.net == Network::Test ? " --network=test" : ""));
    }

    // The groups that exist already, so that a level whose list is the layout's is seen to be so.
    LayoutSids sids = ServiceSids();
    sids.grp_main = LocalGroupSid(NamesFor(Network::Main).group).value_or(Sid{});
    sids.grp_test = LocalGroupSid(NamesFor(Network::Test).group).value_or(Sid{});
    Preflight(ServiceLayout(programdata, sids), options.net, programdata, sids);

    // Both networks' groups, as the shared directories name both.
    sids.grp_main = EnsureLocalGroup(NamesFor(Network::Main).group,
                                     L"Members may attach a Gridcoin GUI to the mainnet service");
    sids.grp_test = EnsureLocalGroup(NamesFor(Network::Test).group,
                                     L"Members may attach a Gridcoin GUI to the testnet service");

    if (service) {
        if (!::ChangeServiceConfigW(service.get(), SERVICE_WIN32_OWN_PROCESS, SERVICE_NO_CHANGE, SERVICE_ERROR_NORMAL,
                                    image_path.c_str(), nullptr, nullptr, nullptr, names.account.c_str(), nullptr,
                                    names.display.c_str())) {
            ThrowLastError("ChangeServiceConfigW " + Shown(names.service));
        }
        Say(strprintf("Service %s: reconfigured; its start type is kept", Shown(names.service)));
    } else {
        service.reset(::CreateServiceW(scm.get(), names.service.c_str(), names.display.c_str(), service_access,
                                       SERVICE_WIN32_OWN_PROCESS, SERVICE_DISABLED, SERVICE_ERROR_NORMAL,
                                       image_path.c_str(), nullptr, nullptr, nullptr, names.account.c_str(), nullptr));
        if (!service) {
            if (::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE) {
                throw Refusal(strprintf("the service %s was uninstalled and Windows has not finished removing it. "
                                        "Close the Services console and any other program that has it open, or "
                                        "restart Windows, then run install again.",
                                        Shown(names.service)));
            }
            ThrowLastError("CreateServiceW " + Shown(names.service));
        }
        Say(strprintf("Service %s: created, disabled", Shown(names.service)));
    }
    Say("  ImagePath: " + Shown(image_path));

    ConfigureService(service.get(), options.net, options.restricted_sid);
    Say(strprintf("  SID type %s; required privileges %s alone; preshutdown timeout %u ms; a restart after %u, %u "
                  "and %u ms, reset after %u s; a delayed automatic start once it is enabled",
                  options.restricted_sid ? "restricted" : "unrestricted", Shown(std::wstring(REQUIRED_PRIVILEGE)),
                  PRESHUTDOWN_TIMEOUT_MS, FAILURE_RESTART_DELAYS_MS[0], FAILURE_RESTART_DELAYS_MS[1],
                  FAILURE_RESTART_DELAYS_MS[2], FAILURE_RESET_SECONDS));

    // The account the service manager gives the service is the one the layout's lists name.
    const Sid& expected = options.net == Network::Main ? sids.svc_main : sids.svc_test;
    const std::optional<Sid> account = winacl::LookupAccountSidByName(names.account, /*require_alias=*/false);
    if (!account || *account != expected) {
        throw std::runtime_error(strprintf("the service's account %s has the SID %s, not %s", Shown(names.account),
                                           account ? winacl::FormatSid(*account) : std::string("(none)"),
                                           winacl::FormatSid(expected)));
    }
    Say(strprintf("  Account: %s, %s", Shown(names.account), winacl::FormatSid(*account)));

    BuildLayout(ServiceLayout(programdata, sids), options.net, programdata, sids);
    AddFirewallRule(options.net, exe);

    Say(strprintf("Installed. The service stays disabled until a wallet is moved in: gridcoin-service.exe enable "
                  "--user=<account>%s moves that account's wallet into it and starts it.",
                  options.net == Network::Test ? " --network=test" : ""));
}

void UninstallService(const ToolOptions& options)
{
    const NetworkNames names = NamesFor(options.net);
    const Network other = options.net == Network::Main ? Network::Test : Network::Main;
    Say(strprintf("Uninstalling the %s service %s.", Shown(names.datadir_leaf), Shown(names.service)));

    RequireElevated("uninstall");
    EnablePrivilege(SE_BACKUP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);

    const fs::path programdata = ProgramDataPath();
    const UniqueServiceHandle scm = OpenScm(SC_MANAGER_CONNECT);
    UniqueServiceHandle service =
        OpenServiceIfInstalled(scm.get(), names.service, DELETE | SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS);

    const std::string disable_first = strprintf("Run gridcoin-service.exe disable%s first: it stops the service, "
                                                "moves the wallet back to its user and disables the service.",
                                                options.net == Network::Test ? " --network=test" : "");
    if (service) {
        const std::vector<unsigned char> config_buffer = QueryConfig(service.get());
        const DWORD start_type = reinterpret_cast<const QUERY_SERVICE_CONFIGW*>(config_buffer.data())->dwStartType;
        if (start_type != SERVICE_DISABLED) {
            throw Refusal(strprintf("the service %s is enabled (start type %s). %s", Shown(names.service),
                                    StartTypeName(start_type, false), disable_first));
        }
        const DWORD state = QueryStatus(service.get()).dwCurrentState;
        if (state != SERVICE_STOPPED) {
            throw Refusal(strprintf("the service %s is %s. %s", Shown(names.service), StateName(state), disable_first));
        }
    } else {
        Say(strprintf("The service %s is not installed; removing what is left of it.", Shown(names.service)));
    }

    const fs::path wallet = NetDataDir(programdata, options.net) / L"wallet.dat";
    if (Exists(wallet)) {
        throw Refusal(strprintf("%s holds a wallet. %s", Shown(wallet),
                                service ? disable_first
                                        : std::string("Move it out of the service's data directory first.")));
    }

    if (service) {
        if (!::DeleteService(service.get())) ThrowLastError("DeleteService " + Shown(names.service));
        service.reset();
        Say(strprintf("Service %s: removed", Shown(names.service)));
    }

    const DWORD code = RunNetsh(FirewallDeleteRuleArgs(FirewallRuleName(options.net)), /*quiet=*/false);
    Say(strprintf("Firewall rule \"%s\": %s", Shown(FirewallRuleName(options.net)),
                  code == 0 ? "removed" : strprintf("not removed; netsh.exe exited with %u, as when there is none",
                                                    static_cast<unsigned>(code))));
    // And one a stopped install left under its staging name.
    RunNetsh(FirewallDeleteRuleArgs(FirewallStagingRuleName(options.net)), /*quiet=*/true);

    // Bottom-up: this network's directories, then, when the other network's service is not
    // installed either, its directories and the shared ones. Only empty ones go.
    std::vector<fs::path> dirs{IpcDir(programdata, options.net)};
    if (options.net == Network::Test) dirs.push_back(NetDataDir(programdata, options.net));
    dirs.push_back(DataDir(programdata, options.net));
    if (!OpenServiceIfInstalled(scm.get(), NamesFor(other).service, SERVICE_QUERY_STATUS)) {
        dirs.push_back(IpcDir(programdata, other));
        if (other == Network::Test) dirs.push_back(NetDataDir(programdata, other));
        dirs.push_back(DataDir(programdata, other));
        dirs.push_back(GridcoinRoot(programdata) / L"ipc");
        dirs.push_back(QuarantineDir(programdata));
        dirs.push_back(GridcoinRoot(programdata) / L"conf");
        dirs.push_back(GridcoinRoot(programdata));
    }
    RemoveEmptyDirectories(dirs);

    Say(strprintf("Uninstalled. Kept: the configuration file %s, which a later install uses again, and the groups "
                  "\"%s\" and \"%s\".",
                  Shown(ConfPath(programdata, options.net)), Shown(NamesFor(Network::Main).group),
                  Shown(NamesFor(Network::Test).group)));
}

//! Prints the service's state and settings (ShowServiceSettings), or that it is not installed, or
//! that this account cannot read them (ClassifyServiceOpen), as the default access list of a
//! service does not let an account read them unless it is signed in interactively. False unless
//! the service is installed and every setting is what install sets. Nothing here stops the rest of
//! status.
bool ShowService(Network net, const std::optional<fs::path>& exe, const fs::path& programdata)
{
    const NetworkNames names = NamesFor(net);
    const UniqueServiceHandle scm(::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) {
        Say(strprintf("Service %s: the service control manager could not be opened: %s", Shown(names.service),
                      Win32ErrorText(::GetLastError())));
        return false;
    }

    const UniqueServiceHandle service(
        ::OpenServiceW(scm.get(), names.service.c_str(), SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS));
    const DWORD error = service ? ERROR_SUCCESS : ::GetLastError();
    switch (ClassifyServiceOpen(error)) {
    case ServiceOpen::Opened:
        break;
    case ServiceOpen::NotInstalled:
        Say(strprintf("Service %s: not installed", Shown(names.service)));
        return false;
    case ServiceOpen::Unreadable:
        Say(ServiceUnreadableText(net));
        return false;
    case ServiceOpen::Failed:
        Say(strprintf("Service %s: could not be opened: %s", Shown(names.service), Win32ErrorText(error)));
        return false;
    }

    Say(strprintf("Service %s: installed", Shown(names.service)));
    try {
        return ShowServiceSettings(service.get(), net, exe, programdata);
    } catch (const std::exception& e) {
        Say(std::string("  The service's settings could not be read: ") + AsciiWhat(e));
        return false;
    }
}

bool ShowStatus(const ToolOptions& options)
{
    const NetworkNames names = NamesFor(options.net);
    Say(strprintf("Status of the %s service %s (%s).", Shown(names.datadir_leaf), Shown(names.service),
                  Shown(names.display)));

    bool matches = true;
    if (winacl::CurrentTokenIsElevated()) {
        try {
            EnablePrivilege(SE_BACKUP_NAME);
        } catch (const std::exception& e) {
            Say(std::string("Note: ") + AsciiWhat(e));
        }
    } else {
        Say("Not elevated: the entries this account cannot read are reported as such. Run it from an elevated "
            "prompt to check every one.");
    }

    std::optional<fs::path> exe;
    try {
        exe = DaemonPath();
        Say("Daemon next to this program: " + Shown(*exe));
    } catch (const std::exception& e) {
        matches = false;
        Say(std::string("Daemon next to this program: ") + AsciiWhat(e));
    }

    const fs::path programdata = ProgramDataPath();
    if (!ShowService(options.net, exe, programdata)) matches = false;

    LayoutSids sids = ServiceSids();
    for (const Network net : {Network::Main, Network::Test}) {
        const std::wstring& group = NamesFor(net).group;
        std::optional<Sid> sid;
        try {
            sid = LocalGroupSid(group);
        } catch (const std::exception& e) {
            Say(strprintf("Group \"%s\": %s", Shown(group), AsciiWhat(e)));
        }
        if (sid) {
            Say(strprintf("Group \"%s\": %s", Shown(group), winacl::FormatSid(*sid)));
            (net == Network::Main ? sids.grp_main : sids.grp_test) = *sid;
        } else {
            matches = false;
            Say(strprintf("Group \"%s\": missing, so the entries that name it are reported as differing",
                          Shown(group)));
        }
    }

    Say("Layout:");
    const fs::path own_conf = ConfPath(programdata, options.net);
    const fs::path other_conf = ConfPath(programdata, options.net == Network::Main ? Network::Test : Network::Main);
    for (const LayoutEntry& entry : ServiceLayout(programdata, sids)) {
        const auto [text, ok] = EntryStatus(entry);
        // The other network's configuration file is its own install's.
        if (!ok && entry.path != other_conf) matches = false;
        Say("  " + Shown(entry.path) + ": " + text);
    }

    for (const LayoutEntry& entry : ServiceLayout(programdata, sids)) {
        if (entry.path != own_conf) continue;
        try {
            const ConfState conf = CheckConf(entry, 0, nullptr);
            if (!conf.exists) {
                matches = false;
                Say("Configuration file: absent; install writes it");
            } else if (conf.problem) {
                matches = false;
                Say("Configuration file: " + ConfRefusal(entry.path, *conf.problem));
            } else {
                Say("Configuration file: accepted (the layout's owner and access list, no includeconf line)");
            }
        } catch (const std::exception& e) {
            matches = false;
            Say(std::string("Configuration file: could not be read: ") + AsciiWhat(e));
        }
    }

    ShowStateFile(StatePath(programdata, options.net));
    return matches;
}

// Moving a wallet in and out.

//! How long enable waits for the service to run, and disable for it to stop.
constexpr DWORD START_WAIT_SECONDS = 120;
constexpr DWORD STOP_WAIT_SECONDS = 600;

DWORD StartTypeOf(SC_HANDLE service)
{
    const std::vector<unsigned char> buffer = QueryConfig(service);
    return reinterpret_cast<const QUERY_SERVICE_CONFIGW*>(buffer.data())->dwStartType;
}

//! Gives the service the start type \p start_type, and an automatic start the delayed start
//! install configures.
void SetStartType(SC_HANDLE service, const NetworkNames& names, DWORD start_type)
{
    if (!::ChangeServiceConfigW(service, SERVICE_NO_CHANGE, start_type, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr)) {
        ThrowLastError("ChangeServiceConfigW " + Shown(names.service));
    }
    if (start_type == SERVICE_AUTO_START) {
        SERVICE_DELAYED_AUTO_START_INFO delayed{TRUE};
        if (!::ChangeServiceConfig2W(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed)) {
            ThrowLastError("ChangeServiceConfig2W delayed automatic start " + Shown(names.service));
        }
    }
}

//! The service's status once it has left the pending state \p pending, or after \p seconds,
//! looked at once a second.
SERVICE_STATUS_PROCESS WaitWhilePending(SC_HANDLE service, DWORD pending, DWORD seconds)
{
    SERVICE_STATUS_PROCESS status = QueryStatus(service);
    for (DWORD waited = 0; status.dwCurrentState == pending && waited < seconds; ++waited) {
        ::Sleep(1000);
        status = QueryStatus(service);
    }
    return status;
}

//! Starts the service and waits for it to run. Throws, saying how it ended, when it does not run
//! within START_WAIT_SECONDS.
DWORD StartAndWait(SC_HANDLE service, const NetworkNames& names)
{
    if (!::StartServiceW(service, 0, nullptr) && ::GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
        ThrowLastError("StartServiceW " + Shown(names.service));
    }
    const SERVICE_STATUS_PROCESS status = WaitWhilePending(service, SERVICE_START_PENDING, START_WAIT_SECONDS);
    if (status.dwCurrentState != SERVICE_RUNNING) {
        throw std::runtime_error(strprintf("the service %s did not reach running within %u s: it is %s (exit %u/%u)",
                                           Shown(names.service), static_cast<unsigned>(START_WAIT_SECONDS),
                                           StateName(status.dwCurrentState),
                                           static_cast<unsigned>(status.dwWin32ExitCode),
                                           static_cast<unsigned>(status.dwServiceSpecificExitCode)));
    }
    return status.dwProcessId;
}

//! Marks what \p handle, opened with DELETE, refers to for removal when the handle closes.
bool MarkForRemoval(HANDLE handle)
{
    FILE_DISPOSITION_INFO disposition{TRUE};
    return ::SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition)) != FALSE;
}

//! The lock file \p lock of a data directory, opened as itself with backup semantics and locked
//! over its whole range, as a core locks it (fsbridge::FileLock), until the handle closes: no
//! core starts on the directory meanwhile. Made when it is not there, which \p created says.
//! Refused when a core holds it.
UniqueHandle HoldDataDirectoryLock(const fs::path& lock, bool& created)
{
    const std::string busy = strprintf("a Gridcoin core is using %s: it holds %s. Stop that core, then run this again.",
                                       Shown(lock.parent_path()), Shown(lock));
    const HANDLE raw = ::CreateFileW(lock.wstring().c_str(), GENERIC_READ | DELETE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                     nullptr);
    const DWORD error = ::GetLastError();
    UniqueHandle handle(raw);
    if (!handle) {
        if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) throw Refusal(busy);
        ThrowWin32(error, "open " + Shown(lock));
    }
    created = error != ERROR_ALREADY_EXISTS;

    const Level level = LevelOf(handle.get(), lock);
    if (level.reparse || level.directory) {
        throw Refusal(strprintf("%s is not a plain file, so it cannot be held as the core's lock. Check what it is and "
                                "move it away, then run this again.",
                                Shown(lock)));
    }
    OVERLAPPED overlapped{};
    if (!::LockFileEx(handle.get(), LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD,
                      &overlapped)) {
        const DWORD code = ::GetLastError();
        if (created) MarkForRemoval(handle.get());
        if (code == ERROR_LOCK_VIOLATION) throw Refusal(busy);
        ThrowWin32(code, "lock " + Shown(lock));
    }
    return handle;
}

//! \p text, a directory given as \p option, as a path a wallet tree is moved at. Refused unless it
//! is an absolute path on a local drive (LocalTreePathProblem).
fs::path TreePathOption(const std::string& option, const std::string& text)
{
    std::wstring wide;
    try {
        wide = Utf8ToWide(text);
    } catch (const std::range_error&) {
        throw Refusal(option + " is not valid UTF-8");
    }
    std::replace(wide.begin(), wide.end(), L'/', L'\\');
    if (const std::optional<std::string> problem = LocalTreePathProblem(wide)) {
        throw Refusal(strprintf("%s %s cannot be used: %s.", option, text, *problem));
    }
    return fs::path(wide);
}

//! \p path as Windows finds it now (ResolvedTreePath): the path the later steps of enable or
//! disable that copy, remove or set access lists open, so that a junction put on the way to it
//! after this is refused, not followed. A path that is not there, nor the directory that would
//! hold it, is kept as it is given, for the checks that follow to refuse.
fs::path PinnedTreePath(const fs::path& path)
{
    bool absent = false;
    std::string error;
    if (const std::optional<fs::path> resolved = ResolvedTreePath(path, absent, error)) return *resolved;
    if (absent) return path;
    throw Refusal(error);
}

//! The names of \p entries, at most five of them, for a message.
std::string FirstNames(const std::vector<LevelEntryFacts>& entries)
{
    std::vector<std::string> names;
    for (size_t i = 0; i < entries.size() && i < 5; ++i) names.push_back(Shown(entries[i].path.filename()));
    std::string text = Join(names, ", ");
    if (entries.size() > names.size()) text += strprintf(" and %u more", static_cast<unsigned>(entries.size() - names.size()));
    return text;
}

//! Refused unless the service's data directory \p datadir is there as install made it and the
//! network data directory holds nothing: a wallet is moved only into an empty one, so nothing of
//! another wallet or chain is mixed with it. True when the network data directory is an empty
//! directory below the data directory (the test network's, which a run of the service made), which
//! the caller removes, as the transfer creates it.
bool RequireEmptyNetDataDir(const LayoutEntry& datadir, Network net, const fs::path& programdata)
{
    const std::string net_flag = net == Network::Test ? " --network=test" : "";
    const fs::path net_dir = NetDataDir(programdata, net);
    std::vector<LevelEntryFacts> entries;
    {
        const auto [handle, level] = HoldLevel(datadir.path, /*to_change=*/false);
        if (!level.exists) {
            throw Refusal(strprintf("%s is not there. Run gridcoin-service.exe install%s first.", Shown(datadir.path),
                                    net_flag));
        }
        if (std::optional<std::string> refusal = LevelRefusal(datadir, level)) throw Refusal(*refusal);
        if (std::optional<std::string> differs = winacl::CheckMatchesSpec(level.acl, datadir.acl)) {
            throw Refusal(strprintf("%s does not have the layout's owner and access list (%s), which the moved wallet "
                                    "takes. Run gridcoin-service.exe install --reconfigure%s, then enable again.",
                                    Shown(datadir.path), *differs, net_flag));
        }
        if (net == Network::Main) entries = ReadLevelEntries(handle.get(), datadir.path);
    }
    bool empty_below = false;
    if (net == Network::Test) {
        const auto [handle, level] = HoldLevel(net_dir, /*to_change=*/false);
        if (level.exists) {
            if (level.reparse || !level.directory) {
                throw Refusal(strprintf("%s is not a plain directory. Check what it is and move it away, then run "
                                        "enable again.",
                                        Shown(net_dir)));
            }
            entries = ReadLevelEntries(handle.get(), net_dir);
            empty_below = entries.empty();
        }
    }
    if (!entries.empty()) {
        throw Refusal(strprintf("%s is not empty: it holds %s. A wallet is moved only into an empty data directory, so "
                                "that nothing of another wallet or chain is mixed with it. Check what is there (it may "
                                "hold a wallet), move it out, then run enable again.",
                                Shown(net_dir), FirstNames(entries)));
    }
    return empty_below;
}

//! What removing a moved copy again came to, for a message: empty when all of it went.
std::string RemoveCopy(const fs::path& root, const std::vector<fs::path>& top)
{
    std::string error;
    if (top.empty() || RemoveTreeNoFollow(root, top, error)) return {};
    return error;
}

//! The network of the core that \p task starts, in \p core_net, or false with \p why when it cannot
//! be told. The task runs the launcher named in its action; the launcher's core line says -testnet
//! or, otherwise, the data directory whose configuration file selects the network (\p default_root,
//! the account's, when the line gives none). Both files are read as data, as the transfer reads
//! the account's files, and nothing in them is run.
bool CoreStartNetwork(const CoreStartTask& task, const fs::path& default_root, Network& core_net, std::string& why)
{
    if (!task.arguments) {
        why = "its action is not a single program";
        return false;
    }
    const std::optional<std::string> launcher_path = LauncherPathOfArguments(*task.arguments);
    if (!launcher_path) {
        why = "its action does not run a launcher";
        return false;
    }
    // The task's paths are the account's to write, so each is checked to be on a local drive
    // before anything opens it: a network path would have this elevated process connect where the
    // account chose.
    const auto local_path = [](const std::string& text, const std::string& what) {
        const std::wstring wide = Utf8ToWide(text);
        if (const std::optional<std::string> problem = LocalTreePathProblem(wide)) {
            throw Refusal(strprintf("%s %s is not read: %s", what, text, *problem));
        }
        return fs::path(wide);
    };
    try {
        std::string text;
        bool absent = false;
        std::string error;
        const fs::path launcher = PinnedTreePath(local_path(*launcher_path, "its launcher"));
        if (!ReadFileNoFollow(launcher, SETTINGS_SIZE_LIMIT, text, absent, error)) {
            why = absent ? "its launcher " + Shown(launcher) + " is not there" : error;
            return false;
        }
        const std::optional<LauncherCore> core = ParseLauncherCoreLine(text);
        if (!core) {
            why = "its launcher " + Shown(launcher) + " has no generated core line";
            return false;
        }
        if (core->testnet) {
            core_net = Network::Test;
            return true;
        }
        const fs::path conf_dir = core->datadir ? local_path(*core->datadir, "its data directory") : default_root;
        const fs::path conf = PinnedTreePath(conf_dir / L"gridcoinresearch.conf");
        if (!ReadFileNoFollow(conf, SETTINGS_SIZE_LIMIT, text, absent, error)) {
            why = absent ? Shown(conf) + " is not there" : error;
            return false;
        }
        core_net = NetworkOfConf(text);
        return true;
    } catch (const std::exception& e) {
        why = AsciiWhat(e);
        return false;
    }
}

void EnableService(const ToolOptions& options)
{
    const NetworkNames names = NamesFor(options.net);
    const Network net = options.net;
    const std::string net_flag = net == Network::Test ? " --network=test" : "";
    const std::string user_arg = options.user.value_or("");
    Say(strprintf("Enabling the %s service %s with the wallet of %s.", Shown(names.datadir_leaf), Shown(names.service),
                  user_arg));

    RequireElevated("enable");
    // SeBackupPrivilege reads the account's owner-only data directory, and SeRestorePrivilege
    // removes entries there and gives the moved tree the service's owner.
    EnablePrivilege(SE_BACKUP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);
    const fs::path programdata = PinnedTreePath(ProgramDataPath());

    // The service must be installed, disabled and stopped.
    const UniqueServiceHandle scm = OpenScm(SC_MANAGER_CONNECT);
    const UniqueServiceHandle service =
        OpenServiceIfInstalled(scm.get(), names.service,
                               SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | SERVICE_CHANGE_CONFIG | SERVICE_START);
    if (!service) {
        throw Refusal(strprintf("the service %s is not installed. Run gridcoin-service.exe install%s first.",
                                Shown(names.service), net_flag));
    }
    const DWORD start_type = StartTypeOf(service.get());
    if (start_type != SERVICE_DISABLED) {
        throw Refusal(strprintf("the service %s is not disabled (start type %s), so a wallet may be moved in already. "
                                "Run gridcoin-service.exe status%s to see whose.",
                                Shown(names.service), StartTypeName(start_type, false), net_flag));
    }
    const DWORD state = QueryStatus(service.get()).dwCurrentState;
    if (state != SERVICE_STOPPED) {
        throw Refusal(strprintf("the service %s is %s. Stop it first (Stop-Service %s), then run enable again.",
                                Shown(names.service), StateName(state), Shown(names.service)));
    }

    // No wallet moved in already, and the network data directory empty.
    const fs::path state_path = StatePath(programdata, net);
    if (Exists(state_path)) {
        throw Refusal(strprintf("%s is there, so a wallet was moved in already. Run gridcoin-service.exe status%s to "
                                "see whose, and disable%s to move it back.",
                                Shown(state_path), net_flag, net_flag));
    }
    LayoutSids sids = ServiceSids();
    sids.grp_main = LocalGroupSid(NamesFor(Network::Main).group).value_or(Sid{});
    sids.grp_test = LocalGroupSid(NamesFor(Network::Test).group).value_or(Sid{});
    if ((net == Network::Main ? sids.grp_main : sids.grp_test).empty()) {
        throw Refusal(strprintf("the group \"%s\" is not there. Run gridcoin-service.exe install --reconfigure%s first.",
                                Shown(names.group), net_flag));
    }
    const fs::path datadir = DataDir(programdata, net);
    const fs::path net_dir = NetDataDir(programdata, net);
    std::optional<LayoutEntry> datadir_entry;
    for (const LayoutEntry& entry : ServiceLayout(programdata, sids)) {
        if (entry.path == datadir) datadir_entry = entry;
    }
    if (!datadir_entry) throw std::logic_error("the layout has no data directory");
    const bool remove_empty_net_dir = RequireEmptyNetDataDir(*datadir_entry, net, programdata);

    // The account, and where its wallet is.
    std::string why;
    const std::optional<UserAccount> user = FindUserAccount(user_arg, why);
    if (!user) throw Refusal(strprintf("--user=%s cannot be used: %s.", user_arg, why));
    Say(strprintf("Account: %s, %s", user->name, winacl::FormatSid(user->sid)));

    fs::path root;
    if (options.from) {
        root = TreePathOption("--from", *options.from);
    } else {
        const std::optional<fs::path> profile = ProfileDirectory(user->sid);
        if (!profile) {
            throw Refusal(strprintf("%s has no profile on this computer, as an account that never signed in here has "
                                    "none. Give its data directory with --from=<directory>.",
                                    user->name));
        }
        root = *profile / L"AppData" / L"Roaming" / L"GridcoinResearch";
        if (const std::optional<std::string> problem = LocalTreePathProblem(root.wstring())) {
            throw Refusal(strprintf("its data directory %s cannot be used: %s. Give it with --from=<directory>.",
                                    Shown(root), *problem));
        }
    }
    // Resolved once: the steps below that copy, remove or set access lists open it at this path
    // only, and refuse it if Windows later finds it elsewhere.
    root = PinnedTreePath(root);
    const fs::path net_root = net == Network::Test ? root / L"testnet" : root;
    Say("From: " + Shown(net_root));
    const Level wallet = ReadLevel(net_root / L"wallet.dat");
    if (!wallet.exists) {
        throw Refusal(strprintf("%s holds no wallet.dat, so there is no wallet to move. Give the data directory with "
                                "--from=<directory>.",
                                Shown(net_root)));
    }
    if (wallet.reparse || wallet.directory) {
        throw Refusal(strprintf("%s is not a plain file, so it is not moved.", Shown(net_root / L"wallet.dat")));
    }

    // No core of the account's running, in any session: a GUI, a daemon, or the two processes of
    // one started elevated.
    const std::vector<CoreProcess> cores = CoresRunningAs(user->sid);
    if (!cores.empty()) {
        std::vector<std::string> running;
        for (const CoreProcess& core : cores) {
            running.push_back(strprintf("%s (process %u, session %u%s)", core.image, core.pid, core.session,
                                        core.account_unknown ? ", whose account cannot be read" : ""));
        }
        throw Refusal(strprintf("a Gridcoin core of %s is running: %s. Close every Gridcoin GUI and core of %s, then "
                                "run enable again.",
                                user->name, Join(running, ", "), user->name));
    }

    // No core starts on the data directory while it is copied.
    bool lock_created = false;
    UniqueHandle lock = HoldDataDirectoryLock(net_root / L".lock", lock_created);
    Say("Holding " + Shown(net_root / L".lock"));

    // Everything from here up to the removal of the account's wallet.dat is undone on a failure.
    bool task_disabled = false;
    bool copied = false;
    bool state_written = false;
    TransferResult result;
    const fs::path quarantine =
        QuarantineDir(programdata) /
        Utf8ToWide(strprintf("%s-%s", Shown(names.datadir_leaf), FormatISO8601DateTimeDashSep(GetTime())));
    try {
        const CoreStartTask task = ReadCoreStartTask();
        if (!task.exists) {
            Say("Core-Start: there is no \\Gridcoin\\Core-Start task");
        } else if (!task.principal_sid || *task.principal_sid != user->sid) {
            Say(strprintf("Core-Start: left as it is; it runs as %s, not %s",
                          task.principal.empty() ? std::string("nobody named") : task.principal, user->name));
        } else if (!task.enabled) {
            Say("Core-Start: disabled already, and left so");
        } else {
            // Only a task that runs a core of this network is turned off. When its network cannot
            // be told, it is, as a core started without its wallet is the worse outcome.
            Network task_net = net;
            std::string unknown;
            const bool known = CoreStartNetwork(task, root, task_net, unknown);
            if (known && task_net != net) {
                Say(strprintf("Core-Start: left as it is; it runs the %s network, not this one",
                              task_net == Network::Main ? "main" : "test"));
            } else {
                SetCoreStartTaskEnabled(false);
                task_disabled = true;
                if (known) {
                    Say(strprintf("Core-Start: disabled, so that it does not start a core of %s without its wallet",
                                  user->name));
                } else {
                    Say(strprintf("Core-Start: disabled; its network could not be told (%s)", unknown));
                }
            }
        }

        if (remove_empty_net_dir && !::RemoveDirectoryW(net_dir.wstring().c_str())) {
            ThrowLastError("remove the empty " + Shown(net_dir));
        }

        result = TransferTree(root, datadir, RulesForEnable(net), quarantine);
        if (!result.ok) throw std::runtime_error("the copy failed: " + result.error);
        copied = true;
        Say(strprintf("Copied %u files (%u bytes) from %s to %s; each was read back and matched", result.files,
                      result.bytes, Shown(net_root), Shown(net_dir)));
        for (const fs::path& path : result.quarantined) {
            Say("Quarantined, as the service does not read it: " + Shown(path));
        }
        if (!result.dropped_settings.empty()) {
            Say(strprintf("Dropped from gridcoinsettings.json, as the core takes them only from its configuration file: "
                          "%s",
                          Join(result.dropped_settings, ", ")));
        }

        const MovedTreeSecurity security = EnableSecurity(programdata, net, sids);
        ResetTreeSecurity(net_dir, security.owner, security.root_spec);
        Say(strprintf("Gave %s the service's access list; %s owns everything in it", Shown(net_dir),
                      Shown(names.account)));

        CreateConf(LayoutEntry{state_path, false, StateFileSpec(sids)},
                   StateFileText(winacl::FormatSid(user->sid), Shown(root), FormatISO8601DateTime(GetTime()),
                                 task_disabled));
        state_written = true;
        Say("Wrote " + Shown(state_path));

        // The account's copy of the wallet goes first. Once it has, the service holds the only one.
        const fs::path wallet_relative =
            net == Network::Test ? fs::path(L"testnet") / L"wallet.dat" : fs::path(L"wallet.dat");
        std::string error;
        if (!RemoveTreeNoFollow(root, {wallet_relative}, error)) {
            throw std::runtime_error("the wallet could not be removed from where it came from: " + error);
        }
    } catch (const std::exception& e) {
        std::vector<std::string> left;
        if (state_written && !::DeleteFileW(state_path.wstring().c_str())) {
            left.push_back("delete " + Shown(state_path) + ": " + Win32ErrorText(::GetLastError()));
        }
        if (copied) {
            if (std::string error = RemoveCopy(datadir, result.created_top); !error.empty()) left.push_back(error);
            if (!result.quarantined.empty()) {
                if (std::string error = RemoveCopy(QuarantineDir(programdata), {quarantine.filename()});
                    !error.empty()) {
                    left.push_back(error);
                }
            }
        }
        if (task_disabled) {
            try {
                SetCoreStartTaskEnabled(true);
            } catch (const std::exception& task_error) {
                left.push_back(std::string("turn Core-Start back on: ") + AsciiWhat(task_error));
            }
        }
        if (lock_created) MarkForRemoval(lock.get());

        const std::string undo =
            left.empty() ? std::string("Nothing was changed: what was copied was removed again, and the data "
                                       "directory the wallet came from is as it was.")
                         : "Undoing it was not complete, so finish it by hand: " + Join(left, "; ") + ".";
        if (dynamic_cast<const Refusal*>(&e) != nullptr) throw Refusal(std::string(AsciiWhat(e)) + " " + undo);
        throw std::runtime_error(std::string(AsciiWhat(e)) + ". " + undo);
    }

    // From here the service holds the only wallet, so nothing is undone: a step that fails is
    // named, with what to do by hand, and the steps after it still run.
    std::vector<std::string> warnings;

    // The rest of the account's copy, with the lock held, so that no core of the account starts in
    // a half-emptied tree; then the lock file goes when the handle closes, and the directory it
    // was in if nothing else is left there.
    std::string left_behind;
    try {
        std::string error;
        if (RemoveTreeNoFollow(root, result.source_remove, error)) {
            Say(strprintf("Removed the wallet's tree from %s", Shown(net_root)));
        } else {
            left_behind = error;
        }
    } catch (const std::exception& e) {
        left_behind = AsciiWhat(e);
    }
    if (!left_behind.empty()) {
        warnings.push_back(strprintf("Warning: the wallet is in the service, but part of its old tree was not "
                                     "removed: %s. Remove what is left of %s by hand; its wallet.dat is gone "
                                     "already.",
                                     left_behind, Shown(net_root)));
    }
    if (!MarkForRemoval(lock.get())) {
        warnings.push_back(strprintf("Warning: %s could not be marked for removal. Delete it by hand.",
                                     Shown(net_root / L".lock")));
    }
    lock.reset();
    if (net == Network::Test) {
        try {
            std::string error;
            bool removed = false;
            if (RemoveDirIfEmpty(root, L"testnet", removed, error)) {
                if (!removed) Say(strprintf("Left in place, as it still holds files: %s",
                                            Shown(net_root)));
            } else {
                warnings.push_back("Warning: " + error);
            }
        } catch (const std::exception& e) {
            warnings.push_back(std::string("Warning: ") + AsciiWhat(e));
        }
    }
    for (const fs::path& kept : result.source_kept) {
        Say("Left where it was, the account's own: " + Shown(root / kept));
    }

    const std::string group_command = strprintf("net localgroup \"%s\" \"%s\" /add", Shown(names.group), user->name);
    const std::string start_command = strprintf("sc config %s start= delayed-auto", Shown(names.service));
    const std::vector<FollowUp> steps = {
        {strprintf("adding %s to the group \"%s\"", user->name, Shown(names.group)), group_command,
         [&] {
             const bool added = AddGroupMember(names.group, user->sid);
             Say(strprintf("Group \"%s\": %s %s", Shown(names.group), user->name,
                           added ? "added" : "is a member already"));
         },
         -1},
        {strprintf("setting the service %s to start automatically", Shown(names.service)), start_command,
         [&] {
             SetStartType(service.get(), names, SERVICE_AUTO_START);
             Say(strprintf("Service %s: set to start automatically (delayed start)", Shown(names.service)));
         },
         -1},
        {strprintf("starting the service %s", Shown(names.service)), "sc start " + Shown(names.service),
         [&] {
             const DWORD pid = StartAndWait(service.get(), names);
             Say(strprintf("Service %s: running, process %u", Shown(names.service), static_cast<unsigned>(pid)));
         },
         1},
    };
    for (const std::string& warning : RunFollowUps(steps)) {
        warnings.push_back("Warning: the wallet is now in the service's data directory, but " + warning);
    }

    if (!warnings.empty()) {
        for (const std::string& warning : warnings) SayError(warning);
        throw std::runtime_error(strprintf("the wallet was moved into the service %s, but enable is not finished: see "
                                           "the warnings above. Nothing was undone",
                                           Shown(names.service)));
    }
    Say(strprintf("Enabled. %s must sign out and sign in again before a GUI can attach (group membership is read at "
                  "sign-in).",
                  user->name));
    Say(std::string("GUI: Start-GridcoinGui.ps1 -Service") + (net == Network::Test ? " -Network test" : ""));
    Say("Autounlock: set it up again in the GUI or with setautounlock from an administrator console.");
}

void DisableService(const ToolOptions& options)
{
    const NetworkNames names = NamesFor(options.net);
    const Network net = options.net;
    const std::string net_flag = net == Network::Test ? " --network=test" : "";
    Say(strprintf("Disabling the %s service %s and moving its wallet back.", Shown(names.datadir_leaf),
                  Shown(names.service)));

    RequireElevated("disable");
    // SeBackupPrivilege reads the service's data directory, and SeRestorePrivilege gives the
    // moved tree the account's owner.
    EnablePrivilege(SE_BACKUP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);
    const fs::path programdata = PinnedTreePath(ProgramDataPath());

    const UniqueServiceHandle scm = OpenScm(SC_MANAGER_CONNECT);
    const UniqueServiceHandle service =
        OpenServiceIfInstalled(scm.get(), names.service,
                               SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | SERVICE_CHANGE_CONFIG | SERVICE_START |
                                   SERVICE_STOP);
    if (!service) {
        throw Refusal(strprintf("the service %s is not installed, so there is no wallet in it to move back.",
                                Shown(names.service)));
    }

    // Whose wallet it is, from the state file, read only when it is the administrators' alone.
    const fs::path state_path = StatePath(programdata, net);
    const StateFile state = ReadStateFile(state_path);
    if (!state.exists) {
        throw Refusal(strprintf("no wallet was moved into the service %s: %s is not there.", Shown(names.service),
                                Shown(state_path)));
    }
    if (!state.plain) {
        throw Refusal(strprintf("%s is not a plain file, so it is not read. Check what it is.", Shown(state_path)));
    }
    if (state.refusal) {
        throw Refusal(strprintf("%s was not read, as another account may have written it: %s. Check it; the file "
                                "enable writes is the administrators' alone.",
                                Shown(state_path), *state.refusal));
    }
    const std::optional<std::string> user_text = StateFileValue(state.text, "user_sid");
    const std::optional<Sid> user = user_text ? winacl::ParseSidString(*user_text) : std::nullopt;
    if (!user) throw Refusal(strprintf("%s names no account (user_sid).", Shown(state_path)));
    const std::string user_name = Describe(*user);
    const bool task_disabled = StateFileValue(state.text, "task_disabled") == std::optional<std::string>("1");
    const std::optional<std::string> from = StateFileValue(state.text, "from");
    if (!options.to && !from) {
        throw Refusal(strprintf("%s does not say where the wallet came from (from). Give it with --to=<directory>.",
                                Shown(state_path)));
    }
    // Resolved once: the steps below that copy, remove or set access lists open it at this path
    // only, and refuse it if Windows later finds it elsewhere.
    const fs::path target = PinnedTreePath(options.to ? TreePathOption("--to", *options.to)
                                                      : TreePathOption("from in " + Shown(state_path), *from));
    const fs::path net_target = net == Network::Test ? target / L"testnet" : target;
    Say("Account: " + user_name);
    Say("To: " + Shown(net_target));

    // The target is the account's own directory, or one to be made.
    const Level target_level = ReadLevel(target);
    if (target_level.exists) {
        if (target_level.reparse || !target_level.directory) {
            throw Refusal(strprintf("%s is not a plain directory. Give the account's data directory with "
                                    "--to=<directory>.",
                                    Shown(target)));
        }
        if (!DisableTargetOwnerAllowed(target_level.acl.owner, *user)) {
            throw Refusal(strprintf("%s is owned by %s, not %s, SYSTEM or Administrators, so the wallet is not moved "
                                    "into it. Give a directory of the account's own with --to=<directory>.",
                                    Shown(target),
                                    target_level.acl.owner ? Describe(*target_level.acl.owner)
                                                           : std::string("an owner that cannot be read"),
                                    user_name));
        }
    } else {
        const Level parent = ReadLevel(target.parent_path());
        if (!parent.exists || parent.reparse || !parent.directory) {
            throw Refusal(strprintf("%s is not there, and neither is the directory it would be made in, %s.",
                                    Shown(target), Shown(target.parent_path())));
        }
    }
    const fs::path datadir = DataDir(programdata, net);
    TransferRules rules = RulesForDisable(net);
    // What the copy makes is closed from the start to all but the account, SYSTEM and this process,
    // not open to what the target passes down until the reset below gives it the account's list.
    rules.created_acl = CreatedSpec(*user, winacl::CurrentTokenUser());
    if (const std::optional<std::string> refusal = DestinationRefusal(datadir, target, rules)) throw Refusal(*refusal);
    // A directory of the account's that is there already, such as the test network's with the
    // configuration file enable left in it, is gone into when it is the account's own. What is made
    // in it is reset below; the directory itself is not.
    for (const fs::path& merged : MergedDirectories(datadir, target, rules)) {
        const Level there = ReadLevel(target / merged);
        if (there.reparse || !there.directory) {
            throw Refusal(strprintf("%s is not a plain directory. Move it away, then run disable again.",
                                    Shown(target / merged)));
        }
        if (!there.acl.owner || *there.acl.owner != *user) {
            throw Refusal(strprintf("%s is owned by %s, not %s, so the wallet is not moved into it. Move it away, "
                                    "then run disable again.",
                                    Shown(target / merged),
                                    there.acl.owner ? Describe(*there.acl.owner)
                                                    : std::string("an owner that cannot be read"),
                                    user_name));
        }
    }

    // Disabled first, so that no failure action or automatic start brings the service back while
    // its wallet moves; then stopped.
    const DWORD previous_start = StartTypeOf(service.get());
    const bool was_running = QueryStatus(service.get()).dwCurrentState != SERVICE_STOPPED;
    SetStartType(service.get(), names, SERVICE_DISABLED);
    Say(strprintf("Service %s: set to disabled", Shown(names.service)));

    bool copied = false;
    TransferResult result;
    bool lock_created = false;
    UniqueHandle lock;
    const fs::path quarantine =
        QuarantineDir(programdata) /
        Utf8ToWide(strprintf("%s-%s", Shown(names.datadir_leaf), FormatISO8601DateTimeDashSep(GetTime())));
    try {
        if (was_running) {
            SERVICE_STATUS control{};
            if (!::ControlService(service.get(), SERVICE_CONTROL_STOP, &control) &&
                ::GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
                ThrowLastError("ControlService stop " + Shown(names.service));
            }
            Say(strprintf("Service %s: stopping (the core flushes its wallet and chain first; up to %u s)",
                          Shown(names.service), static_cast<unsigned>(STOP_WAIT_SECONDS)));
        }
        const SERVICE_STATUS_PROCESS stopped = WaitWhilePending(service.get(), SERVICE_STOP_PENDING, STOP_WAIT_SECONDS);
        if (stopped.dwCurrentState != SERVICE_STOPPED) {
            throw std::runtime_error(strprintf("the service %s did not stop within %u s: it is %s",
                                               Shown(names.service), static_cast<unsigned>(STOP_WAIT_SECONDS),
                                               StateName(stopped.dwCurrentState)));
        }
        Say(strprintf("Service %s: stopped", Shown(names.service)));

        lock = HoldDataDirectoryLock(NetDataDir(programdata, net) / L".lock", lock_created);

        result = TransferTree(datadir, target, rules, quarantine);
        if (!result.ok) throw std::runtime_error("the copy failed: " + result.error);
        copied = true;
        Say(strprintf("Copied %u files (%u bytes) from %s to %s; each was read back and matched", result.files,
                      result.bytes, Shown(NetDataDir(programdata, net)), Shown(net_target)));
        for (const fs::path& path : result.quarantined) {
            Say("Quarantined, as the account's core would read it as its own: " + Shown(path));
        }
        if (!result.dropped_settings.empty()) {
            Say(strprintf("Dropped from gridcoinsettings.json, as the core takes them only from its configuration file: "
                          "%s",
                          Join(result.dropped_settings, ", ")));
        }

        // Owner-only for the account: the target when this made it, and otherwise each entry made.
        const winacl::AclSpec owner_only = OwnerOnlySpec(*user);
        if (result.created_dst) {
            ResetTreeSecurity(target, *user, owner_only);
        } else {
            for (const fs::path& top : result.created_top) ResetTreeSecurity(target / top, *user, owner_only);
        }
        Say(strprintf("Gave what was copied the owner-only access list of %s", user_name));

        // The service's copy of the wallet goes first. The lock stays held until the rest is gone.
        const fs::path wallet_relative =
            net == Network::Test ? fs::path(L"testnet") / L"wallet.dat" : fs::path(L"wallet.dat");
        std::string error;
        if (!RemoveTreeNoFollow(datadir, {wallet_relative}, error)) {
            throw std::runtime_error("the wallet could not be removed from the service: " + error);
        }
    } catch (const std::exception& e) {
        std::vector<std::string> left;
        if (copied) {
            std::vector<fs::path> made = result.created_top;
            fs::path root = target;
            if (result.created_dst) {
                made = {target.filename()};
                root = target.parent_path();
            }
            if (std::string error = RemoveCopy(root, made); !error.empty()) left.push_back(error);
            if (!result.quarantined.empty()) {
                if (std::string error = RemoveCopy(QuarantineDir(programdata), {quarantine.filename()});
                    !error.empty()) {
                    left.push_back(error);
                }
            }
        }
        // As it was before, with the lock let go so that the core can start again.
        if (lock) {
            if (lock_created) MarkForRemoval(lock.get());
            lock.reset();
        }
        try {
            SetStartType(service.get(), names, previous_start);
            if (was_running) StartAndWait(service.get(), names);
        } catch (const std::exception& service_error) {
            left.push_back(std::string("restore the service: ") + AsciiWhat(service_error));
        }

        const std::string undo =
            left.empty() ? strprintf("Nothing was changed: what was copied was removed again, and the service has its "
                                     "wallet and its start type back%s.",
                                     was_running ? " and runs again" : "")
                         : "Undoing it was not complete, so finish it by hand: " + Join(left, "; ") + ".";
        if (dynamic_cast<const Refusal*>(&e) != nullptr) throw Refusal(std::string(AsciiWhat(e)) + " " + undo);
        throw std::runtime_error(std::string(AsciiWhat(e)) + ". " + undo);
    }

    // From here the account holds the only wallet, so nothing is undone: a step that fails is
    // named, with what to do by hand, and the steps after it still run.
    std::vector<std::string> warnings;
    const fs::path net_dir = NetDataDir(programdata, net);

    // The rest of the service's copy, its autounlock file with it, with the lock held, so that no
    // core starts in a half-emptied tree; then the lock file goes when the handle closes, and the
    // directory it was in.
    std::string left_behind;
    try {
        std::string error;
        if (RemoveTreeNoFollow(datadir, result.source_remove, error)) {
            Say(strprintf("Removed the wallet's tree from %s", Shown(net_dir)));
        } else {
            left_behind = error;
        }
    } catch (const std::exception& e) {
        left_behind = AsciiWhat(e);
    }
    if (!left_behind.empty()) {
        warnings.push_back(strprintf("Warning: the wallet is back, but part of the service's copy was not removed: "
                                     "%s. Remove what is left of %s by hand; its wallet.dat is gone already.",
                                     left_behind, Shown(net_dir)));
    }
    if (!MarkForRemoval(lock.get())) {
        warnings.push_back(strprintf("Warning: %s could not be marked for removal. Delete it by hand.",
                                     Shown(net_dir / L".lock")));
    }
    lock.reset();
    if (net == Network::Test) {
        try {
            std::string error;
            bool removed = false;
            if (!RemoveDirIfEmpty(datadir, L"testnet", removed, error)) warnings.push_back("Warning: " + error);
        } catch (const std::exception& e) {
            warnings.push_back(std::string("Warning: ") + AsciiWhat(e));
        }
    }

    const std::vector<FollowUp> steps = {
        {strprintf("removing %s from the group \"%s\"", user_name, Shown(names.group)),
         strprintf("net localgroup \"%s\" <the account> /delete", Shown(names.group)),
         [&] {
             const bool removed = RemoveGroupMember(names.group, *user);
             Say(strprintf("Group \"%s\": %s %s", Shown(names.group), user_name,
                           removed ? "removed" : "was not a member"));
         },
         -1},
        {"turning Core-Start back on", "Enable-ScheduledTask -TaskPath \\Gridcoin\\ -TaskName Core-Start",
         [&] {
             if (!task_disabled) return;
             const CoreStartTask task = ReadCoreStartTask();
             if (!task.exists) {
                 Say("Core-Start: not there any more, so not turned back on");
             } else if (!task.principal_sid || *task.principal_sid != *user) {
                 Say(strprintf("Core-Start: left as it is; it runs as %s now, not %s",
                               task.principal.empty() ? std::string("nobody named") : task.principal, user_name));
             } else {
                 SetCoreStartTaskEnabled(true);
                 Say("Core-Start: turned back on, as enable had turned it off");
             }
         },
         -1},
        // The state's job ended when the wallet left the service; keeping it would stop a later
        // enable or disable.
        {"deleting the state file " + Shown(state_path), "del \"" + Shown(state_path) + "\"",
         [&] {
             if (!::DeleteFileW(state_path.wstring().c_str())) ThrowLastError("delete " + Shown(state_path));
             Say("Deleted " + Shown(state_path));
         },
         -1},
    };
    for (const std::string& warning : RunFollowUps(steps)) {
        warnings.push_back("Warning: the wallet is back in " + Shown(net_target) + ", but " + warning);
    }

    if (!warnings.empty()) {
        for (const std::string& warning : warnings) SayError(warning);
        throw std::runtime_error(strprintf("the wallet was moved back to %s, but disable is not finished: see the "
                                           "warnings above. Nothing was undone",
                                           Shown(net_target)));
    }
    // Task mode is for an administrator's own wallet: a standard account cannot elevate as itself, and
    // the script registers the tasks for whoever runs it.
    Say(strprintf("Disabled. The wallet is back in %s; %s can start the plain GUI. For unattended start, an "
                  "administrator's own wallet uses Install-GridcoinCoreTask.ps1, run signed in as that account; a "
                  "standard account's uses the service mode.",
                  Shown(net_target), user_name));
    // The service's autounlock file went with its copy, and enable had already removed the account's own.
    Say("Autounlock: off. Set it up again in the GUI or with setautounlock once the wallet starts unattended.");
}

} // namespace

int Install(const ToolOptions& options)
{
    return RunVerb([&options] { InstallService(options); });
}

int Uninstall(const ToolOptions& options)
{
    return RunVerb([&options] { UninstallService(options); });
}

int Enable(const ToolOptions& options)
{
    return RunVerb([&options] { EnableService(options); });
}

int Disable(const ToolOptions& options)
{
    return RunVerb([&options] { DisableService(options); });
}

int Status(const ToolOptions& options)
{
    bool matches = false;
    const int code = RunVerb([&options, &matches] { matches = ShowStatus(options); });
    if (code != EXIT_OK) return code;
    return matches ? EXIT_OK : EXIT_REFUSED;
}

} // namespace winsvc

#endif // WIN32
