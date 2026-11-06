// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "winsvc/transfer.h"

#include "crypto/sha256.h"
#include "tinyformat.h"
#include "univalue.h"
#include "util/strencodings.h"
#include "util/string.h"
#include "util/system.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef WIN32
#include <system_error>

#include <windows.h>
#else
#include "util/syserror.h"

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace winsvc {

namespace {

namespace winacl = util::winacl;
using winacl::AceSpec;
using winacl::AclSpec;
using winacl::Inherit;
using winacl::ObservedAcl;

//! The runtime files the core makes again at each start, and the autounlock directory, which is
//! set up again for the account that holds the wallet next.
const std::vector<std::string> RUNTIME_NAMES{"node.sock", "ipc.cookie", ".lock", "gridcoinresearchd.pid", "autounlock"};

//! Whether the path component \p part is the ASCII name \p name, without regard to ASCII case, as
//! Windows opens a name the core asks for.
bool IsName(const fs::path& part, const std::string& name)
{
    using Unit = std::make_unsigned_t<fs::path::value_type>;
    const fs::path::string_type& native = part.native();
    if (native.size() != name.size()) return false;
    for (size_t i = 0; i < name.size(); ++i) {
        const Unit unit = static_cast<Unit>(native[i]);
        if (unit > 0x7f || ToLower(static_cast<char>(unit)) != ToLower(name[i])) return false;
    }
    return true;
}

bool IsAnyName(const fs::path& part, const std::vector<std::string>& names)
{
    return std::any_of(names.begin(), names.end(), [&part](const std::string& name) { return IsName(part, name); });
}

//! \p path for a message, in UTF-8.
std::string Shown(const fs::path& path)
{
#ifdef WIN32
    try {
        return WideToUtf8(path.wstring());
    } catch (const std::range_error&) {
        return "(a path that is not valid UTF-16)";
    }
#else
    return path.string();
#endif
}

//! Main leaves out the test network's directory, which a same-account data directory holds; Test
//! takes only it.
TransferRules WithScope(Network net, TransferRules rules)
{
    if (net == Network::Main) {
        rules.exclude_top_dirs = {"testnet"};
        rules.hold = fs::path(".lock");
    } else {
        rules.only_top_dir = "testnet";
        rules.hold = fs::path("testnet") / ".lock";
    }
    return rules;
}

//! Whether a transfer creates an entry at the destination for \p decision.
bool Creates(EntryDecision decision)
{
    return decision == EntryDecision::Copy || decision == EntryDecision::FilterSettings;
}

//! Whether \p relative is a plain relative path: not empty, not absolute, with no "." or ".."
//! component.
bool PlainRelative(const fs::path& relative)
{
    if (relative.empty() || relative.has_root_path()) return false;
    return std::none_of(relative.begin(), relative.end(),
                        [](const fs::path& part) { return part == "." || part == ".." || part.empty(); });
}

} // namespace

TransferRules RulesForEnable(Network net)
{
    TransferRules rules;
    rules.skip_names = RUNTIME_NAMES;
    // The service reads its own configuration file under conf\, and DB_CONFIG would point the
    // wallet's database at other directories.
    rules.quarantine_names = {"gridcoinresearch.conf", "DB_CONFIG"};
    rules.filter_settings_names = {"gridcoinsettings.json"};
    return WithScope(net, std::move(rules));
}

TransferRules RulesForDisable(Network net)
{
    TransferRules rules;
    rules.skip_names = RUNTIME_NAMES;
    return WithScope(net, std::move(rules));
}

bool InScope(const fs::path& relative, const TransferRules& rules)
{
    if (relative.empty()) return false;
    const fs::path top = *relative.begin();
    if (IsAnyName(top, rules.exclude_top_dirs)) return false;
    return !rules.only_top_dir || IsName(top, *rules.only_top_dir);
}

EntryDecision Decide(const EntryFacts& e, const TransferRules& rules, std::string& reason)
{
    reason.clear();
    if (!InScope(e.relative, rules)) {
        reason = "it is outside the network's data directory";
        return EntryDecision::Skip;
    }

    // By name first, whatever the entry is: a socket file the core left is a reparse point on
    // Windows and neither a file nor a directory elsewhere.
    const fs::path name = e.relative.filename();
    if (IsAnyName(name, rules.skip_names)) {
        reason = "the core makes it again at each start, or it is the autounlock directory, which is set up again";
        return EntryDecision::Skip;
    }

    switch (e.type) {
    case EntryFacts::Type::Symlink:
        reason = "it is a symbolic link, which is not followed";
        return EntryDecision::Refuse;
    case EntryFacts::Type::Reparse:
        reason = "it is a reparse point (a junction, a mount point or another kind), which is not followed";
        return EntryDecision::Refuse;
    case EntryFacts::Type::Other:
        reason = "it is neither a file nor a directory";
        return EntryDecision::Refuse;
    case EntryFacts::Type::Regular:
        if (e.link_count > 1) {
            reason = strprintf("it is a file with %u hard links, and a copy would leave the other names reaching "
                               "the file it came from",
                               e.link_count);
            return EntryDecision::Refuse;
        }
        break;
    case EntryFacts::Type::Directory:
        break;
    }

    const bool quarantine = IsAnyName(name, rules.quarantine_names);
    const bool filter = IsAnyName(name, rules.filter_settings_names);
    if ((quarantine || filter) && e.type == EntryFacts::Type::Directory) {
        reason = "it is a directory where the core keeps a file of that name";
        return EntryDecision::Refuse;
    }
    if (quarantine) return EntryDecision::Quarantine;
    if (filter) {
        if (e.size > SETTINGS_SIZE_LIMIT) {
            reason = strprintf("it is larger than %u bytes, too large for a settings file", SETTINGS_SIZE_LIMIT);
            return EntryDecision::Refuse;
        }
        return EntryDecision::FilterSettings;
    }
    return EntryDecision::Copy;
}

std::pair<std::string, std::vector<std::string>> FilterSettingsJson(const std::string& json)
{
    UniValue in;
    if (!in.read(json) || !in.isObject()) throw std::runtime_error("it is not a JSON object, which the core refuses");

    const std::vector<std::string>& keys = in.getKeys();
    const std::vector<UniValue>& values = in.getValues();
    std::set<std::string> seen;
    UniValue out(UniValue::VOBJ);
    std::vector<std::string> dropped;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (!seen.insert(keys[i]).second) {
            throw std::runtime_error("it sets " + keys[i] + " more than once, which the core refuses");
        }
        // The rule the core applies to the file when it reads it (ArgsManager::ReadSettingsFile).
        if (IsRefusedSettingKey(keys[i])) {
            dropped.push_back(keys[i]);
            continue;
        }
        out.__pushKV(keys[i], values[i]);
    }
    if (dropped.empty()) return {json, {}};

    // As util::WriteSettings writes the file.
    return {out.write(/*prettyIndent=*/1, /*indentLevel=*/4) + "\n", std::move(dropped)};
}

std::optional<std::string> LocalTreePathProblem(const std::wstring& path)
{
    std::wstring text = path;
    std::replace(text.begin(), text.end(), L'/', L'\\');
    const auto letter = [](wchar_t c) { return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z'); };
    if (text.size() < 4 || !letter(text[0]) || text[1] != L':' || text[2] != L'\\') {
        return std::string("it is not an absolute path on a local drive, such as C:\\Users\\<account>\\AppData\\Roaming\\"
                           "GridcoinResearch");
    }
    if (text.back() == L'\\') return std::string("it ends in a backslash; give it without one");

    size_t start = 3;
    while (start <= text.size()) {
        size_t end = text.find(L'\\', start);
        if (end == std::wstring::npos) end = text.size();
        const std::wstring part = text.substr(start, end - start);
        if (part.empty()) return std::string("it has an empty component (two backslashes in a row)");
        if (part == L"." || part == L"..") return std::string("it has a '.' or '..' component");
        start = end + 1;
    }
    return std::nullopt;
}

MovedTreeSecurity EnableSecurity(const fs::path& programdata, Network net, const LayoutSids& sids)
{
    MovedTreeSecurity security;
    security.owner = net == Network::Main ? sids.svc_main : sids.svc_test;

    const fs::path datadir = DataDir(programdata, net);
    for (const LayoutEntry& entry : ServiceLayout(programdata, sids)) {
        if (entry.path == datadir) security.root_spec = entry.acl;
    }
    return security;
}

AclSpec OwnerOnlySpec(const Sid& user)
{
    return AclSpec{user,
                   {{user, detail::FULL, Inherit::ContainerAndObject},
                    {winacl::wellknown::System(), detail::FULL, Inherit::ContainerAndObject}}};
}

AclSpec SpecForFile(const AclSpec& spec)
{
    AclSpec file = spec;
    for (AceSpec& ace : file.aces) ace.inherit = Inherit::None;
    return file;
}

std::optional<std::string> InheritedOnlyRefusal(const ObservedAcl& acl, const Sid& owner, const AclSpec& root_spec)
{
    if (!acl.owner) return std::string("its owner cannot be read");
    if (*acl.owner != owner) {
        return strprintf("it is owned by %s, not %s", winacl::FormatSid(*acl.owner), winacl::FormatSid(owner));
    }
    if (!acl.dacl_present) return std::string("it has no DACL (all users would have access)");
    if (acl.is_protected) return std::string("its DACL is protected, so it does not inherit");

    std::vector<AceSpec> expected;
    for (const AceSpec& ace : root_spec.aces) {
        if (ace.inherit == Inherit::ContainerAndObject) expected.push_back(ace);
    }
    const auto matches = [](const winacl::ObservedAce& ace, const AceSpec& want) {
        return ace.kind == winacl::AceKind::Allow && ace.inherited && !ace.inherit_only && ace.trustee == want.trustee &&
               ace.mask == want.mask;
    };

    for (const winacl::ObservedAce& ace : acl.aces) {
        if (ace.kind != winacl::AceKind::Allow && ace.kind != winacl::AceKind::OtherAllow) continue;
        if (!ace.inherited) {
            return strprintf("its DACL holds an entry of its own (%s, access mask 0x%08x)",
                             winacl::FormatSid(ace.trustee), ace.mask);
        }
        if (std::none_of(expected.begin(), expected.end(), [&](const AceSpec& want) { return matches(ace, want); })) {
            return strprintf("its DACL holds an inherited entry the root does not pass down (%s, access mask 0x%08x)",
                             winacl::FormatSid(ace.trustee), ace.mask);
        }
    }
    for (const AceSpec& want : expected) {
        if (std::none_of(acl.aces.begin(), acl.aces.end(),
                         [&](const winacl::ObservedAce& ace) { return matches(ace, want); })) {
            return strprintf("its DACL lacks the inherited entry for %s (access mask 0x%08x)",
                             winacl::FormatSid(want.trustee), want.mask);
        }
    }
    return std::nullopt;
}

// The file system. Each platform gives the same two classes: a Dir, a directory opened as itself
// and held while what it holds is read or changed, and a File, a file opened as itself. Every
// entry is reached from the Dir that holds it, never by following anything.

namespace {

//! The size of the buffer a copy goes through.
constexpr size_t COPY_CHUNK = 1 << 20;

#ifdef WIN32

std::string Win32Text(DWORD error)
{
    std::string message = std::system_category().message(static_cast<int>(error));
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == '.')) {
        message.pop_back();
    }
    return strprintf("%s (error %u)", message, static_cast<unsigned>(error));
}

bool IsNotFound(DWORD error)
{
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

//! \p path in the form that names it exactly ("\\?\C:\..."), so that Windows takes every
//! component as it is written. A path in another form is kept as it is.
std::wstring ExactPath(const fs::path& path)
{
    std::wstring text = path.wstring();
    std::replace(text.begin(), text.end(), L'/', L'\\');
    if (text.compare(0, 4, L"\\\\?\\") == 0) return text;
    if (text.size() >= 3 && text[1] == L':' && text[2] == L'\\') return L"\\\\?\\" + text;
    return text;
}

//! The path of what \p handle refers to as Windows resolves it, in the "\\?\C:\..." form.
std::optional<std::wstring> FinalPath(HANDLE handle, DWORD& error)
{
    const DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    std::wstring name(MAX_PATH, L'\0');
    DWORD length = ::GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), flags);
    if (length >= name.size()) {
        // The size needed, with the terminating null.
        name.resize(length);
        length = ::GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), flags);
    }
    if (length == 0 || length >= name.size()) {
        error = ::GetLastError();
        return std::nullopt;
    }
    name.resize(length);
    return name;
}

bool SamePath(const std::wstring& a, const std::wstring& b)
{
    return ::CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()),
                                  TRUE) == CSTR_EQUAL;
}

//! What a handle opened on an entry as itself says the entry is.
struct HandleFacts
{
    EntryFacts::Type type{EntryFacts::Type::Other};
    uint64_t links{1};
    uint64_t size{0};
    DWORD attributes{0};
};

bool ReadHandleFacts(HANDLE handle, HandleFacts& facts, DWORD& error)
{
    BY_HANDLE_FILE_INFORMATION info{};
    FILE_ATTRIBUTE_TAG_INFO tag{};
    if (!::GetFileInformationByHandle(handle, &info) ||
        !::GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag))) {
        error = ::GetLastError();
        return false;
    }
    facts.attributes = info.dwFileAttributes;
    facts.links = info.nNumberOfLinks;
    facts.size = (uint64_t{info.nFileSizeHigh} << 32) | info.nFileSizeLow;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        facts.type = tag.ReparseTag == IO_REPARSE_TAG_SYMLINK ? EntryFacts::Type::Symlink : EntryFacts::Type::Reparse;
    } else if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        facts.type = EntryFacts::Type::Directory;
    } else if (info.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) {
        facts.type = EntryFacts::Type::Other;
    } else {
        facts.type = EntryFacts::Type::Regular;
    }
    return true;
}

//! Marks what \p handle (opened with DELETE) refers to for removal when the handle closes. A
//! read-only entry is made writable first, as Windows does not remove one.
bool DisposeOf(HANDLE handle, DWORD attributes, DWORD& error)
{
    if (attributes & FILE_ATTRIBUTE_READONLY) {
        FILE_BASIC_INFO basic{};
        // Zero times are left as they are; zero attributes would be too.
        basic.FileAttributes = (attributes & ~DWORD{FILE_ATTRIBUTE_READONLY}) ? (attributes & ~DWORD{FILE_ATTRIBUTE_READONLY})
                                                                            : DWORD{FILE_ATTRIBUTE_NORMAL};
        if (!::SetFileInformationByHandle(handle, FileBasicInfo, &basic, sizeof(basic))) {
            error = ::GetLastError();
            return false;
        }
    }
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!::SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition))) {
        error = ::GetLastError();
        return false;
    }
    return true;
}

class File
{
public:
    File(winacl::UniqueHandle handle, fs::path path) : m_handle(std::move(handle)), m_path(std::move(path)) {}

    bool Read(unsigned char* buffer, size_t size, size_t& got, std::string& error)
    {
        DWORD read = 0;
        if (!::ReadFile(m_handle.get(), buffer, static_cast<DWORD>(std::min<size_t>(size, COPY_CHUNK)), &read,
                        nullptr)) {
            error = "read " + Shown(m_path) + ": " + Win32Text(::GetLastError());
            return false;
        }
        got = read;
        return true;
    }

    bool Write(const unsigned char* data, size_t size, std::string& error)
    {
        size_t done = 0;
        while (done < size) {
            DWORD written = 0;
            if (!::WriteFile(m_handle.get(), data + done, static_cast<DWORD>(std::min<size_t>(size - done, COPY_CHUNK)),
                             &written, nullptr)) {
                error = "write " + Shown(m_path) + ": " + Win32Text(::GetLastError());
                return false;
            }
            // A write that succeeds and writes nothing would never finish.
            if (written == 0) {
                error = "write " + Shown(m_path) + ": nothing was written";
                return false;
            }
            done += written;
        }
        return true;
    }

    bool Sync(std::string& error)
    {
        if (::FlushFileBuffers(m_handle.get())) return true;
        error = "flush " + Shown(m_path) + ": " + Win32Text(::GetLastError());
        return false;
    }

    bool Rewind(std::string& error)
    {
        LARGE_INTEGER zero{};
        if (::SetFilePointerEx(m_handle.get(), zero, nullptr, FILE_BEGIN)) return true;
        error = "seek " + Shown(m_path) + ": " + Win32Text(::GetLastError());
        return false;
    }

private:
    winacl::UniqueHandle m_handle;
    fs::path m_path;
};

class Dir
{
public:
    //! \p path opened as a directory, itself and not what a link at it names. nullopt with
    //! \p absent when nothing is there, and with \p error otherwise.
    static std::optional<Dir> Open(const fs::path& path, bool& absent, std::string& error)
    {
        absent = false;
        winacl::UniqueHandle handle(::CreateFileW(ExactPath(path).c_str(), READ_ACCESS, SHARE, nullptr, OPEN_EXISTING,
                                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!handle) {
            const DWORD code = ::GetLastError();
            absent = IsNotFound(code);
            error = absent ? Shown(path) + " is not there" : "open " + Shown(path) + ": " + Win32Text(code);
            return std::nullopt;
        }
        DWORD code = 0;
        const std::optional<std::wstring> final_path = FinalPath(handle.get(), code);
        if (!final_path) {
            error = "read the path of " + Shown(path) + ": " + Win32Text(code);
            return std::nullopt;
        }
        if (final_path->compare(0, 8, L"\\\\?\\UNC\\") == 0) {
            error = Shown(path) + " is on a network share; the wallet is moved between local directories only";
            return std::nullopt;
        }
        return FromHandle(std::move(handle), *final_path, path, error);
    }

    //! The directory \p name inside this one, opened as itself.
    std::optional<Dir> Sub(const fs::path& name, std::string& error) const
    {
        winacl::UniqueHandle handle = OpenChild(name, READ_ACCESS, SHARE, 0, error);
        if (!handle) return std::nullopt;
        return FromHandle(std::move(handle), ChildPath(name), Child(name), error);
    }

    //! The names of the entries in this directory, sorted.
    bool Names(std::vector<fs::path>& names, std::string& error) const
    {
        names.clear();
        // 8-byte aligned, as FILE_ID_BOTH_DIR_INFO is.
        std::vector<uint64_t> buffer(8192);
        FILE_INFO_BY_HANDLE_CLASS info_class = FileIdBothDirectoryRestartInfo;
        for (;;) {
            if (!::GetFileInformationByHandleEx(m_handle.get(), info_class, buffer.data(),
                                                static_cast<DWORD>(buffer.size() * sizeof(uint64_t)))) {
                const DWORD code = ::GetLastError();
                if (code == ERROR_NO_MORE_FILES) break;
                error = "list " + Shown(m_path) + ": " + Win32Text(code);
                return false;
            }
            info_class = FileIdBothDirectoryInfo;

            const auto* bytes = reinterpret_cast<const unsigned char*>(buffer.data());
            for (size_t offset = 0;;) {
                const auto* info = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(bytes + offset);
                std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
                if (name != L"." && name != L"..") names.emplace_back(std::move(name));
                if (info->NextEntryOffset == 0) break;
                offset += info->NextEntryOffset;
            }
        }
        std::sort(names.begin(), names.end());
        return true;
    }

    //! What the entry \p name is, read through a handle on it as itself. False with \p absent when
    //! it is not there.
    bool Facts(const fs::path& name, EntryFacts& facts, bool& absent, std::string& error) const
    {
        absent = false;
        DWORD code = 0;
        winacl::UniqueHandle handle = OpenChild(name, FILE_READ_ATTRIBUTES | SYNCHRONIZE, SHARE_ALL, 0, error, &code);
        if (!handle) {
            absent = IsNotFound(code);
            return false;
        }
        HandleFacts read;
        if (!ReadHandleFacts(handle.get(), read, code)) {
            error = "read the attributes of " + Shown(Child(name)) + ": " + Win32Text(code);
            return false;
        }
        facts.type = read.type;
        facts.link_count = read.links;
        facts.size = read.size;
        return true;
    }

    //! The file \p name opened as itself to be read, refused unless it is a file with one link.
    //! It is shared in every way, as a file another process writes can still be read.
    std::optional<File> OpenRead(const fs::path& name, std::string& error) const
    {
        winacl::UniqueHandle handle = OpenChild(name, GENERIC_READ, SHARE_ALL, FILE_FLAG_SEQUENTIAL_SCAN, error);
        if (!handle) return std::nullopt;
        HandleFacts facts;
        DWORD code = 0;
        if (!ReadHandleFacts(handle.get(), facts, code)) {
            error = "read the attributes of " + Shown(Child(name)) + ": " + Win32Text(code);
            return std::nullopt;
        }
        if (facts.type != EntryFacts::Type::Regular || facts.links != 1) {
            error = Shown(Child(name)) + " changed while it was copied: it is no longer a file with one link";
            return std::nullopt;
        }
        return File(std::move(handle), Child(name));
    }

    //! A new file \p name, which must not be there, opened to be written and read back, with the
    //! access list this directory passes down.
    std::optional<File> Create(const fs::path& name, std::string& error) const
    {
        const std::wstring path = ChildPath(name);
        winacl::UniqueHandle handle(::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr,
                                                  CREATE_NEW,
                                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS |
                                                      FILE_FLAG_OPEN_REPARSE_POINT,
                                                  nullptr));
        if (!handle) {
            const DWORD code = ::GetLastError();
            error = code == ERROR_FILE_EXISTS ? Shown(Child(name)) + " is already there"
                                              : "create " + Shown(Child(name)) + ": " + Win32Text(code);
            return std::nullopt;
        }
        if (!CheckCreatedWhereAsked(handle.get(), name, error)) return std::nullopt;
        return File(std::move(handle), Child(name));
    }

    //! A new directory \p name, which must not be there, with the access list this directory
    //! passes down.
    bool MakeDir(const fs::path& name, std::string& error) const
    {
        const std::wstring path = ChildPath(name);
        if (!::CreateDirectoryW(path.c_str(), nullptr)) {
            const DWORD code = ::GetLastError();
            error = code == ERROR_ALREADY_EXISTS ? Shown(Child(name)) + " is already there"
                                                 : "create " + Shown(Child(name)) + ": " + Win32Text(code);
            return false;
        }
        winacl::UniqueHandle handle(::CreateFileW(path.c_str(), DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, SHARE,
                                                  nullptr, OPEN_EXISTING,
                                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!handle) {
            error = "open " + Shown(Child(name)) + " after creating it: " + Win32Text(::GetLastError());
            ::RemoveDirectoryW(path.c_str());
            return false;
        }
        return CheckCreatedWhereAsked(handle.get(), name, error);
    }

    //! Whether anything is at \p name, read as itself.
    bool Has(const fs::path& name, bool& has, std::string& error) const
    {
        DWORD code = 0;
        const winacl::UniqueHandle handle = OpenChild(name, FILE_READ_ATTRIBUTES | SYNCHRONIZE, SHARE_ALL, 0, error,
                                                      &code, /*check_path=*/false);
        has = static_cast<bool>(handle);
        return has || IsNotFound(code);
    }

    //! Removes \p name, and everything below it when it is a directory, without following
    //! anything: a link or other reparse point is removed as an entry. Nothing there is not an
    //! error.
    bool Remove(const fs::path& name, std::string& error) const
    {
        DWORD code = 0;
        winacl::UniqueHandle handle =
            OpenChild(name, DELETE | FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE,
                      SHARE, 0, error, &code);
        if (!handle) return IsNotFound(code);

        HandleFacts facts;
        if (!ReadHandleFacts(handle.get(), facts, code)) {
            error = "read the attributes of " + Shown(Child(name)) + ": " + Win32Text(code);
            return false;
        }
        if (facts.type == EntryFacts::Type::Directory) {
            const Dir dir(std::move(handle), ChildPath(name), Child(name));
            std::vector<fs::path> names;
            if (!dir.Names(names, error)) return false;
            for (const fs::path& entry : names) {
                if (!dir.Remove(entry, error)) return false;
            }
            if (!DisposeOf(dir.m_handle.get(), facts.attributes, code)) {
                error = "remove " + Shown(Child(name)) + ": " + Win32Text(code);
                return false;
            }
            return true;
        }
        if (!DisposeOf(handle.get(), facts.attributes, code)) {
            error = "remove " + Shown(Child(name)) + ": " + Win32Text(code);
            return false;
        }
        return true;
    }

    //! The entry \p name opened as itself with \p access, through this directory's path, and its
    //! facts. For ResetTreeSecurity.
    winacl::UniqueHandle OpenEntry(const fs::path& name, DWORD access, HandleFacts& facts, std::string& error) const
    {
        winacl::UniqueHandle handle = OpenChild(name, access, SHARE, 0, error);
        if (!handle) return handle;
        DWORD code = 0;
        if (!ReadHandleFacts(handle.get(), facts, code)) {
            error = "read the attributes of " + Shown(Child(name)) + ": " + Win32Text(code);
            return winacl::UniqueHandle();
        }
        return handle;
    }

    //! A Dir for the directory \p handle holds, found at \p final_path, opened as \p shown.
    static std::optional<Dir> FromHandle(winacl::UniqueHandle handle, const std::wstring& final_path,
                                         const fs::path& shown, std::string& error)
    {
        HandleFacts facts;
        DWORD code = 0;
        if (!ReadHandleFacts(handle.get(), facts, code)) {
            error = "read the attributes of " + Shown(shown) + ": " + Win32Text(code);
            return std::nullopt;
        }
        if (facts.type == EntryFacts::Type::Symlink || facts.type == EntryFacts::Type::Reparse) {
            error = Shown(shown) + " is a link or another reparse point, which is not followed";
            return std::nullopt;
        }
        if (facts.type != EntryFacts::Type::Directory) {
            error = Shown(shown) + " is not a directory";
            return std::nullopt;
        }
        return Dir(std::move(handle), final_path, shown);
    }

    fs::path Child(const fs::path& name) const { return m_path / name; }

private:
    //! A directory's entries may be read and changed by others while it is held, but it cannot be
    //! renamed or removed, so the path its entries are opened by keeps naming it.
    static constexpr DWORD SHARE = FILE_SHARE_READ | FILE_SHARE_WRITE;
    static constexpr DWORD SHARE_ALL = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

    //! What a held directory is opened for: to list and read what it holds.
    static constexpr DWORD READ_ACCESS = FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE;

    Dir(winacl::UniqueHandle handle, std::wstring final_path, fs::path shown)
        : m_handle(std::move(handle)), m_final(std::move(final_path)), m_path(std::move(shown))
    {
    }

    std::wstring ChildPath(const fs::path& name) const { return m_final + L"\\" + name.wstring(); }

    //! \p name opened as itself, and refused when Windows resolved it anywhere but in this
    //! directory, as when a directory above was moved and a link put in its place.
    winacl::UniqueHandle OpenChild(const fs::path& name, DWORD access, DWORD share, DWORD flags, std::string& error,
                                   DWORD* code_out = nullptr, bool check_path = true) const
    {
        const std::wstring path = ChildPath(name);
        winacl::UniqueHandle handle(::CreateFileW(path.c_str(), access, share, nullptr, OPEN_EXISTING,
                                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT | flags,
                                                  nullptr));
        if (!handle) {
            const DWORD code = ::GetLastError();
            if (code_out) *code_out = code;
            error = IsNotFound(code) ? Shown(Child(name)) + " is not there"
                                     : "open " + Shown(Child(name)) + ": " + Win32Text(code);
            return handle;
        }
        if (code_out) *code_out = ERROR_SUCCESS;
        if (check_path) {
            DWORD code = 0;
            const std::optional<std::wstring> final_path = FinalPath(handle.get(), code);
            if (!final_path || !SamePath(*final_path, path)) {
                error = Shown(Child(name)) + " was not found where it was looked for (a directory on its path was "
                        "moved or replaced while it was open)";
                if (code_out) *code_out = final_path ? ERROR_ACCESS_DENIED : code;
                return winacl::UniqueHandle();
            }
        }
        return handle;
    }

    //! Whether what \p handle refers to, just created as \p name, is in this directory. When it
    //! is not, it is removed again.
    bool CheckCreatedWhereAsked(HANDLE handle, const fs::path& name, std::string& error) const
    {
        DWORD code = 0;
        const std::optional<std::wstring> final_path = FinalPath(handle, code);
        if (final_path && SamePath(*final_path, ChildPath(name))) return true;

        DisposeOf(handle, 0, code);
        error = Shown(Child(name)) + " was created somewhere else than asked (a directory on its path was moved or "
                "replaced while it was open), and was removed again";
        return false;
    }

    winacl::UniqueHandle m_handle;
    std::wstring m_final;
    fs::path m_path;
};

#else // WIN32

class Fd
{
public:
    Fd() = default;
    explicit Fd(int fd) : m_fd(fd) {}
    Fd(Fd&& other) noexcept : m_fd(std::exchange(other.m_fd, -1)) {}
    Fd& operator=(Fd&& other) noexcept
    {
        if (this != &other) {
            Reset();
            m_fd = std::exchange(other.m_fd, -1);
        }
        return *this;
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    ~Fd() { Reset(); }

    int Get() const { return m_fd; }
    void Reset()
    {
        if (m_fd >= 0) ::close(m_fd);
        m_fd = -1;
    }

private:
    int m_fd{-1};
};

EntryFacts::Type TypeOf(mode_t mode)
{
    if (S_ISDIR(mode)) return EntryFacts::Type::Directory;
    if (S_ISREG(mode)) return EntryFacts::Type::Regular;
    if (S_ISLNK(mode)) return EntryFacts::Type::Symlink;
    return EntryFacts::Type::Other;
}

class File
{
public:
    File(Fd fd, fs::path path) : m_fd(std::move(fd)), m_path(std::move(path)) {}

    bool Read(unsigned char* buffer, size_t size, size_t& got, std::string& error)
    {
        ssize_t read = 0;
        do {
            read = ::read(m_fd.Get(), buffer, size);
        } while (read < 0 && errno == EINTR);
        if (read < 0) {
            error = "read " + Shown(m_path) + ": " + SysErrorString(errno);
            return false;
        }
        got = static_cast<size_t>(read);
        return true;
    }

    bool Write(const unsigned char* data, size_t size, std::string& error)
    {
        size_t done = 0;
        while (done < size) {
            const ssize_t written = ::write(m_fd.Get(), data + done, size - done);
            if (written < 0 && errno == EINTR) continue;
            if (written < 0) {
                error = "write " + Shown(m_path) + ": " + SysErrorString(errno);
                return false;
            }
            // A write that succeeds and writes nothing would never finish.
            if (written == 0) {
                error = "write " + Shown(m_path) + ": nothing was written";
                return false;
            }
            done += static_cast<size_t>(written);
        }
        return true;
    }

    bool Sync(std::string& error)
    {
        if (::fsync(m_fd.Get()) == 0) return true;
        error = "flush " + Shown(m_path) + ": " + SysErrorString(errno);
        return false;
    }

    bool Rewind(std::string& error)
    {
        if (::lseek(m_fd.Get(), 0, SEEK_SET) == 0) return true;
        error = "seek " + Shown(m_path) + ": " + SysErrorString(errno);
        return false;
    }

private:
    Fd m_fd;
    fs::path m_path;
};

class Dir
{
public:
    static std::optional<Dir> Open(const fs::path& path, bool& absent, std::string& error)
    {
        absent = false;
        const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            const int code = errno;
            absent = code == ENOENT;
            error = OpenError(path, code);
            return std::nullopt;
        }
        return Dir(Fd(fd), path);
    }

    std::optional<Dir> Sub(const fs::path& name, std::string& error) const
    {
        const int fd = ::openat(m_fd.Get(), name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            error = OpenError(Child(name), errno);
            return std::nullopt;
        }
        return Dir(Fd(fd), Child(name));
    }

    bool Names(std::vector<fs::path>& names, std::string& error) const
    {
        names.clear();
        // A descriptor of its own, as reading a directory moves its position.
        const int fd = ::openat(m_fd.Get(), ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        DIR* const dir = fd < 0 ? nullptr : ::fdopendir(fd);
        if (dir == nullptr) {
            error = "list " + Shown(m_path) + ": " + SysErrorString(errno);
            if (fd >= 0) ::close(fd);
            return false;
        }
        for (;;) {
            errno = 0;
            const dirent* const entry = ::readdir(dir);
            if (entry == nullptr) break;
            const std::string name = entry->d_name;
            if (name != "." && name != "..") names.emplace_back(name);
        }
        const int code = errno;
        ::closedir(dir);
        if (code != 0) {
            error = "list " + Shown(m_path) + ": " + SysErrorString(code);
            return false;
        }
        std::sort(names.begin(), names.end());
        return true;
    }

    bool Facts(const fs::path& name, EntryFacts& facts, bool& absent, std::string& error) const
    {
        absent = false;
        struct stat st{};
        if (::fstatat(m_fd.Get(), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
            absent = errno == ENOENT;
            error = "read the attributes of " + Shown(Child(name)) + ": " + SysErrorString(errno);
            return false;
        }
        facts.type = TypeOf(st.st_mode);
        facts.link_count = static_cast<uint64_t>(st.st_nlink);
        facts.size = static_cast<uint64_t>(st.st_size);
        return true;
    }

    std::optional<File> OpenRead(const fs::path& name, std::string& error) const
    {
        // Without blocking, as a FIFO put in a file's place would until something writes to it.
        const int fd = ::openat(m_fd.Get(), name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            error = OpenError(Child(name), errno);
            return std::nullopt;
        }
        Fd guard(fd);
        struct stat st{};
        if (::fstat(fd, &st) != 0) {
            error = "read the attributes of " + Shown(Child(name)) + ": " + SysErrorString(errno);
            return std::nullopt;
        }
        if (!S_ISREG(st.st_mode) || st.st_nlink != 1) {
            error = Shown(Child(name)) + " changed while it was copied: it is no longer a file with one link";
            return std::nullopt;
        }
        return File(std::move(guard), Child(name));
    }

    std::optional<File> Create(const fs::path& name, std::string& error) const
    {
        const int fd = ::openat(m_fd.Get(), name.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd < 0) {
            error = errno == EEXIST ? Shown(Child(name)) + " is already there"
                                    : "create " + Shown(Child(name)) + ": " + SysErrorString(errno);
            return std::nullopt;
        }
        Fd guard(fd);
        // Owner-only whatever the umask.
        if (::fchmod(fd, 0600) != 0) {
            error = "set the mode of " + Shown(Child(name)) + ": " + SysErrorString(errno);
            ::unlinkat(m_fd.Get(), name.c_str(), 0);
            return std::nullopt;
        }
        return File(std::move(guard), Child(name));
    }

    bool MakeDir(const fs::path& name, std::string& error) const
    {
        if (::mkdirat(m_fd.Get(), name.c_str(), 0700) != 0) {
            error = errno == EEXIST ? Shown(Child(name)) + " is already there"
                                    : "create " + Shown(Child(name)) + ": " + SysErrorString(errno);
            return false;
        }
        // Owner-only whatever the umask. Removed again when that fails, as nothing else knows it
        // was made.
        const std::optional<Dir> made = Sub(name, error);
        if (!made || ::fchmod(made->m_fd.Get(), 0700) != 0) {
            if (made) error = "set the mode of " + Shown(Child(name)) + ": " + SysErrorString(errno);
            ::unlinkat(m_fd.Get(), name.c_str(), AT_REMOVEDIR);
            return false;
        }
        return true;
    }

    bool Has(const fs::path& name, bool& has, std::string& error) const
    {
        struct stat st{};
        has = ::fstatat(m_fd.Get(), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0;
        if (has || errno == ENOENT) return true;
        error = "read the attributes of " + Shown(Child(name)) + ": " + SysErrorString(errno);
        return false;
    }

    bool Remove(const fs::path& name, std::string& error) const
    {
        EntryFacts facts;
        bool absent = false;
        if (!Facts(name, facts, absent, error)) return absent;

        if (facts.type == EntryFacts::Type::Directory) {
            const std::optional<Dir> dir = Sub(name, error);
            if (!dir) return false;
            std::vector<fs::path> names;
            if (!dir->Names(names, error)) return false;
            for (const fs::path& entry : names) {
                if (!dir->Remove(entry, error)) return false;
            }
        }
        const int flags = facts.type == EntryFacts::Type::Directory ? AT_REMOVEDIR : 0;
        if (::unlinkat(m_fd.Get(), name.c_str(), flags) != 0 && errno != ENOENT) {
            error = "remove " + Shown(Child(name)) + ": " + SysErrorString(errno);
            return false;
        }
        return true;
    }

    fs::path Child(const fs::path& name) const { return m_path / name; }

private:
    Dir(Fd fd, fs::path path) : m_fd(std::move(fd)), m_path(std::move(path)) {}

    static std::string OpenError(const fs::path& path, int code)
    {
        if (code == ENOENT) return Shown(path) + " is not there";
        if (code == ELOOP) return Shown(path) + " is a symbolic link, which is not followed";
        if (code == ENOTDIR) return Shown(path) + " is not a directory";
        return "open " + Shown(path) + ": " + SysErrorString(code);
    }

    Fd m_fd;
    fs::path m_path;
};

#endif // WIN32

//! Reads \p in to its end, writing each chunk to \p out and hashing it, then reads \p out back and
//! compares. \p data, when given, is written instead of reading \p in.
bool WriteAndVerify(File* in, const std::string* data, File& out, const fs::path& out_path, uint64_t& bytes,
                    std::string& error)
{
    CSHA256 written_hash;
    uint64_t written = 0;
    std::vector<unsigned char> buffer(COPY_CHUNK);
    if (data != nullptr) {
        const auto* bytes_in = reinterpret_cast<const unsigned char*>(data->data());
        if (!out.Write(bytes_in, data->size(), error)) return false;
        written_hash.Write(bytes_in, data->size());
        written = data->size();
    } else {
        for (;;) {
            size_t got = 0;
            if (!in->Read(buffer.data(), buffer.size(), got, error)) return false;
            if (got == 0) break;
            if (!out.Write(buffer.data(), got, error)) return false;
            written_hash.Write(buffer.data(), got);
            written += got;
        }
    }
    if (!out.Sync(error) || !out.Rewind(error)) return false;

    // Read back, and compared with what was read from the source. This guards against a write that did
    // not land as written. It has no test, as one would need a fault injected into the write path.
    CSHA256 read_hash;
    uint64_t read = 0;
    for (;;) {
        size_t got = 0;
        if (!out.Read(buffer.data(), buffer.size(), got, error)) return false;
        if (got == 0) break;
        read_hash.Write(buffer.data(), got);
        read += got;
    }
    std::array<unsigned char, CSHA256::OUTPUT_SIZE> expected{};
    std::array<unsigned char, CSHA256::OUTPUT_SIZE> actual{};
    written_hash.Finalize(expected.data());
    read_hash.Finalize(actual.data());
    if (read != written || expected != actual) {
        error = strprintf("%s does not read back as it was written (%u bytes written, %u read back)", Shown(out_path),
                          written, read);
        return false;
    }
    bytes = written;
    return true;
}

//! Reads all of \p in, up to \p limit bytes.
bool ReadAll(File& in, const fs::path& path, uint64_t limit, std::string& text, std::string& error)
{
    text.clear();
    std::vector<unsigned char> buffer(COPY_CHUNK);
    for (;;) {
        size_t got = 0;
        if (!in.Read(buffer.data(), buffer.size(), got, error)) return false;
        if (got == 0) return true;
        text.append(reinterpret_cast<const char*>(buffer.data()), got);
        if (text.size() > limit) {
            error = strprintf("%s is larger than %u bytes, too large to read", Shown(path), limit);
            return false;
        }
    }
}

//! Removes \p base / \p relative, a path of one or more plain components, through the directories on
//! the way, held as themselves.
bool RemoveBelow(const Dir& base, const fs::path& relative, std::string& error)
{
    std::vector<Dir> held;
    held.reserve(std::distance(relative.begin(), relative.end()));
    const Dir* at = &base;
    const fs::path dirs = relative.parent_path();
    for (const fs::path& part : dirs) {
        std::optional<Dir> next = at->Sub(part, error);
        if (!next) return false;
        held.push_back(std::move(*next));
        at = &held.back();
    }
    return at->Remove(relative.filename(), error);
}

//! One transfer: the copy, and what it created, so that a failure removes all of it.
class Transfer
{
public:
    Transfer(const TransferRules& rules, const fs::path& quarantine_root, TransferResult& result)
        : m_rules(rules), m_quarantine_root(quarantine_root), m_result(result)
    {
    }

    void Run(const fs::path& src, const fs::path& dst)
    {
        std::string error;
        bool absent = false;
        std::optional<Dir> source = Dir::Open(src, absent, error);
        if (!source) return Fail(error);

        if (m_rules.only_top_dir) {
            const fs::path only = source->Child(*m_rules.only_top_dir);
            EntryFacts facts;
            if (!source->Facts(fs::path(*m_rules.only_top_dir), facts, absent, error)) {
                return Fail(absent ? Shown(only) + " is not there, so there is no wallet to move" : error);
            }
            if (facts.type != EntryFacts::Type::Directory) return Fail(Shown(only) + " is not a directory");
        }

        // Every entry decided before anything is written: a refusal writes nothing.
        if (!Plan(*source, fs::path())) return;
        if (const std::optional<std::string> refusal = DestinationRefusal(src, dst, m_rules)) return Fail(*refusal);

        m_dst_name = dst.filename();
        m_dst = Dir::Open(dst, absent, error);
        if (!m_dst) {
            if (!absent) return Fail(error);
            m_dst_parent = Dir::Open(dst.parent_path(), absent, error);
            if (!m_dst_parent) return Fail(error);
            if (!m_dst_parent->MakeDir(dst.filename(), error)) return Fail(error);
            m_result.created_dst = true;
            m_dst = m_dst_parent->Sub(dst.filename(), error);
            if (!m_dst) return FailAndUndo(error);
        }

        bool kept = false;
        if (!CopyDir(*source, *m_dst, fs::path(), kept, true, fs::path())) return Undo();
        m_result.ok = true;
    }

private:
    void Fail(const std::string& error)
    {
        m_result.ok = false;
        m_result.error = error;
    }

    void FailAndUndo(const std::string& error)
    {
        Fail(error);
        Undo();
    }

    bool Plan(const Dir& dir, const fs::path& relative)
    {
        std::vector<fs::path> names;
        std::string error;
        if (!dir.Names(names, error)) {
            Fail(error);
            return false;
        }
        for (const fs::path& name : names) {
            const fs::path entry_relative = relative.empty() ? name : relative / name;
            if (relative.empty() && !InScope(entry_relative, m_rules)) continue;

            EntryFacts facts;
            bool absent = false;
            if (!dir.Facts(name, facts, absent, error)) {
                if (absent) continue;
                Fail(error);
                return false;
            }
            facts.relative = entry_relative;
            std::string reason;
            const EntryDecision decision = Decide(facts, m_rules, reason);
            if (decision == EntryDecision::Refuse) {
                Fail(Shown(dir.Child(name)) + ": " + reason + ". Nothing was copied.");
                return false;
            }
            if (decision == EntryDecision::FilterSettings) {
                // Read and parsed now, so that a settings file the core would refuse stops the
                // transfer before anything is written.
                std::optional<File> in = dir.OpenRead(name, error);
                std::string text;
                if (!in || !ReadAll(*in, dir.Child(name), SETTINGS_SIZE_LIMIT, text, error)) {
                    Fail(error);
                    return false;
                }
                try {
                    FilterSettingsJson(text);
                } catch (const std::runtime_error& e) {
                    Fail(Shown(dir.Child(name)) + ": " + e.what() + ". Correct or remove it, then try again. "
                         "Nothing was copied.");
                    return false;
                }
            }
            if (decision == EntryDecision::Copy && facts.type == EntryFacts::Type::Directory) {
                const std::optional<Dir> sub = dir.Sub(name, error);
                if (!sub) {
                    Fail(error);
                    return false;
                }
                if (!Plan(*sub, entry_relative)) return false;
            }
        }
        return true;
    }

    //! Copies what \p from holds into \p to. \p kept is set when something in \p from or below it
    //! stays in the source (a quarantined entry, or the held file). The entries to remove are added
    //! to the result: those of this directory when something stays in it or it is the root, and
    //! otherwise the caller names the directory itself.
    //! The entries made directly in \p to are recorded in the result as created, below \p prefix, when
    //! \p record is set: the root's, and those of a directory that was there already.
    bool CopyDir(const Dir& from, const Dir& to, const fs::path& relative, bool& kept, bool record,
                 const fs::path& prefix)
    {
        const bool top = relative.empty();
        std::vector<fs::path> mine;
        std::vector<fs::path> names;
        std::string error;
        if (!from.Names(names, error)) {
            Fail(error);
            return false;
        }
        for (const fs::path& name : names) {
            const fs::path entry_relative = top ? name : relative / name;
            if (top && !InScope(entry_relative, m_rules)) continue;

            EntryFacts facts;
            bool absent = false;
            if (!from.Facts(name, facts, absent, error)) {
                if (absent) continue;
                Fail(error);
                return false;
            }
            facts.relative = entry_relative;
            std::string reason;
            const EntryDecision decision = Decide(facts, m_rules, reason);

            switch (decision) {
            case EntryDecision::Skip:
                if (m_rules.hold && entry_relative == *m_rules.hold) {
                    kept = true;
                } else {
                    mine.push_back(entry_relative);
                }
                continue;
            case EntryDecision::Refuse:
                Fail(Shown(from.Child(name)) + ": " + reason + " (it changed while the tree was copied)");
                return false;
            case EntryDecision::Quarantine:
                if (!QuarantineFile(from, name, entry_relative, nullptr)) return false;
                kept = true;
                m_result.source_kept.push_back(entry_relative);
                continue;
            case EntryDecision::FilterSettings:
                if (!FilterFile(from, name, to, entry_relative, record ? prefix / name : fs::path())) return false;
                mine.push_back(entry_relative);
                continue;
            case EntryDecision::Copy:
                break;
            }

            if (facts.type == EntryFacts::Type::Directory) {
                // A directory that is there already is gone into, and only what is made in it is ours.
                bool merge = false;
                if (top) {
                    EntryFacts there;
                    bool gone = false;
                    if (to.Facts(name, there, gone, error)) {
                        merge = there.type == EntryFacts::Type::Directory;
                    } else if (!gone) {
                        Fail(error);
                        return false;
                    }
                }
                if (!merge) {
                    if (!to.MakeDir(name, error)) {
                        Fail(error);
                        return false;
                    }
                    if (record) m_result.created_top.push_back(prefix / name);
                }
                const std::optional<Dir> sub_from = from.Sub(name, error);
                const std::optional<Dir> sub_to = sub_from ? to.Sub(name, error) : std::nullopt;
                if (!sub_from || !sub_to) {
                    Fail(error);
                    return false;
                }
                bool sub_kept = false;
                if (!CopyDir(*sub_from, *sub_to, entry_relative, sub_kept, merge, merge ? prefix / name : fs::path())) {
                    return false;
                }
                if (sub_kept) {
                    kept = true;
                } else {
                    mine.push_back(entry_relative);
                }
                continue;
            }

            std::optional<File> in = from.OpenRead(name, error);
            if (!in) {
                Fail(error);
                return false;
            }
            std::optional<File> out = to.Create(name, error);
            if (!out) {
                Fail(error);
                return false;
            }
            if (record) m_result.created_top.push_back(prefix / name);
            uint64_t bytes = 0;
            if (!WriteAndVerify(&*in, nullptr, *out, to.Child(name), bytes, error)) {
                Fail(error);
                return false;
            }
            ++m_result.files;
            m_result.bytes += bytes;
            mine.push_back(entry_relative);
        }
        if (kept || top) {
            m_result.source_remove.insert(m_result.source_remove.end(), mine.begin(), mine.end());
        }
        return true;
    }

    //! The settings file \p name, with the keys the core refuses from it dropped. The file as it
    //! was goes to the quarantine directory when a key was dropped, so nothing of it is lost.
    bool FilterFile(const Dir& from, const fs::path& name, const Dir& to, const fs::path& relative,
                    const fs::path& created_as)
    {
        std::string error;
        std::optional<File> in = from.OpenRead(name, error);
        if (!in) {
            Fail(error);
            return false;
        }
        std::string original;
        if (!ReadAll(*in, from.Child(name), SETTINGS_SIZE_LIMIT, original, error)) {
            Fail(error);
            return false;
        }

        std::pair<std::string, std::vector<std::string>> filtered;
        try {
            filtered = FilterSettingsJson(original);
        } catch (const std::runtime_error& e) {
            Fail(Shown(from.Child(name)) + ": " + e.what() + ". Correct or remove it, then try again.");
            return false;
        }
        if (!filtered.second.empty() && !QuarantineFile(from, name, relative, &original)) return false;

        std::optional<File> out = to.Create(name, error);
        if (!out) {
            Fail(error);
            return false;
        }
        if (!created_as.empty()) m_result.created_top.push_back(created_as);
        uint64_t bytes = 0;
        if (!WriteAndVerify(nullptr, &filtered.first, *out, to.Child(name), bytes, error)) {
            Fail(error);
            return false;
        }
        ++m_result.files;
        m_result.bytes += bytes;
        m_result.dropped_settings.insert(m_result.dropped_settings.end(), filtered.second.begin(),
                                         filtered.second.end());
        return true;
    }

    //! Copies \p name, or \p content when given, to the quarantine directory below \p relative,
    //! creating the directories on the way.
    bool QuarantineFile(const Dir& from, const fs::path& name, const fs::path& relative, const std::string* content)
    {
        std::string error;
        if (m_quarantine_root.empty()) {
            Fail(Shown(from.Child(name)) + " is to be quarantined, and no quarantine directory was given");
            return false;
        }
        if (!m_quarantine) {
            bool absent = false;
            m_quarantine = Dir::Open(m_quarantine_root, absent, error);
            if (!m_quarantine) {
                if (!absent) {
                    Fail(error);
                    return false;
                }
                m_quarantine_parent = Dir::Open(m_quarantine_root.parent_path(), absent, error);
                if (!m_quarantine_parent || !m_quarantine_parent->MakeDir(m_quarantine_root.filename(), error)) {
                    Fail(error);
                    return false;
                }
                m_quarantine_created = true;
                m_quarantine = m_quarantine_parent->Sub(m_quarantine_root.filename(), error);
                if (!m_quarantine) {
                    Fail(error);
                    return false;
                }
            }
        }

        // The directories below the quarantine directory, made by this transfer when not there, and
        // held while the file is written.
        const fs::path dirs = relative.parent_path();
        std::vector<Dir> held;
        held.reserve(std::distance(dirs.begin(), dirs.end()));
        const Dir* at = &*m_quarantine;
        for (const fs::path& part : dirs) {
            bool has = false;
            if (!at->Has(part, has, error)) {
                Fail(error);
                return false;
            }
            if (!has) {
                if (!at->MakeDir(part, error)) {
                    Fail(error);
                    return false;
                }
                if (at == &*m_quarantine) m_quarantine_top.push_back(part);
            }
            std::optional<Dir> next = at->Sub(part, error);
            if (!next) {
                Fail(error);
                return false;
            }
            held.push_back(std::move(*next));
            at = &held.back();
        }

        std::optional<File> in;
        if (content == nullptr) {
            in = from.OpenRead(name, error);
            if (!in) {
                Fail(error);
                return false;
            }
        }
        std::optional<File> out = at->Create(name, error);
        if (!out) {
            Fail(error);
            return false;
        }
        if (at == &*m_quarantine) m_quarantine_top.push_back(name);
        uint64_t bytes = 0;
        if (!WriteAndVerify(in ? &*in : nullptr, content, *out, at->Child(name), bytes, error)) {
            Fail(error);
            return false;
        }
        m_result.quarantined.push_back(m_quarantine_root / relative);
        return true;
    }

    //! Removes everything this transfer created, and leaves the result saying nothing was.
    void Undo()
    {
        std::vector<std::string> left;
        std::string error;
        if (m_dst) {
            for (auto it = m_result.created_top.rbegin(); it != m_result.created_top.rend(); ++it) {
                if (!RemoveBelow(*m_dst, *it, error)) left.push_back(error);
            }
        }
        if (m_result.created_dst && m_dst_parent) {
            m_dst.reset();
            if (!m_dst_parent->Remove(m_dst_name, error)) left.push_back(error);
        }
        if (m_quarantine) {
            if (m_quarantine_created && m_quarantine_parent) {
                m_quarantine.reset();
                if (!m_quarantine_parent->Remove(m_quarantine_root.filename(), error)) left.push_back(error);
            } else {
                for (auto it = m_quarantine_top.rbegin(); it != m_quarantine_top.rend(); ++it) {
                    if (!m_quarantine->Remove(*it, error)) left.push_back(error);
                }
            }
        }

        if (!left.empty()) {
            m_result.error += " Removing what was copied failed too, so remove it by hand: " + Join(left, "; ");
        }
        m_result.ok = false;
        m_result.files = 0;
        m_result.bytes = 0;
        m_result.created_top.clear();
        m_result.quarantined.clear();
        m_result.dropped_settings.clear();
        m_result.source_remove.clear();
        m_result.source_kept.clear();
        m_result.created_dst = false;
    }

    const TransferRules& m_rules;
    const fs::path m_quarantine_root;
    TransferResult& m_result;
    std::optional<Dir> m_dst_parent;
    std::optional<Dir> m_dst;
    fs::path m_dst_name;
    std::optional<Dir> m_quarantine_parent;
    std::optional<Dir> m_quarantine;
    bool m_quarantine_created{false};
    //! The entries this transfer made directly in a quarantine directory that was there.
    std::vector<fs::path> m_quarantine_top;
};

} // namespace

namespace {

//! Looks at the top-level names the transfer would create in \p dst. \p there gets the entries that
//! are in the way; \p merged the names of directories that are there and are gone into, with no
//! entry of the transfer's in them. Returns an error text when something cannot be read.
std::optional<std::string> ScanDestination(const fs::path& src, const fs::path& dst, const TransferRules& rules,
                                           std::vector<std::string>& there, std::vector<fs::path>& merged)
{
    std::string error;
    bool absent = false;
    const std::optional<Dir> source = Dir::Open(src, absent, error);
    if (!source) return error;
    const std::optional<Dir> target = Dir::Open(dst, absent, error);
    if (!target) {
        if (absent) return std::nullopt;
        return error;
    }

    std::vector<fs::path> names;
    if (!source->Names(names, error)) return error;
    for (const fs::path& name : names) {
        EntryFacts facts;
        if (!source->Facts(name, facts, absent, error)) {
            if (absent) continue;
            return error;
        }
        facts.relative = name;
        std::string reason;
        if (!Creates(Decide(facts, rules, reason))) continue;

        bool has = false;
        if (!target->Has(name, has, error)) return error;
        if (!has) continue;

        // A real directory at the name is gone into when none of the entries the transfer would
        // create in it is there. Anything else at the name is in the way.
        EntryFacts present;
        if (facts.type != EntryFacts::Type::Directory || !target->Facts(name, present, absent, error) ||
            present.type != EntryFacts::Type::Directory) {
            there.push_back(Shown(target->Child(name)));
            continue;
        }
        const std::optional<Dir> source_sub = source->Sub(name, error);
        const std::optional<Dir> target_sub = source_sub ? target->Sub(name, error) : std::nullopt;
        if (!source_sub || !target_sub) return error;
        std::vector<fs::path> children;
        if (!source_sub->Names(children, error)) return error;
        const size_t before = there.size();
        for (const fs::path& child : children) {
            EntryFacts child_facts;
            if (!source_sub->Facts(child, child_facts, absent, error)) {
                if (absent) continue;
                return error;
            }
            child_facts.relative = name / child;
            if (!Creates(Decide(child_facts, rules, reason))) continue;
            bool child_has = false;
            if (!target_sub->Has(child, child_has, error)) return error;
            if (child_has) there.push_back(Shown(target_sub->Child(child)));
        }
        if (there.size() == before) merged.push_back(name);
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> DestinationRefusal(const fs::path& src, const fs::path& dst, const TransferRules& rules)
{
    std::vector<std::string> there;
    std::vector<fs::path> merged;
    if (const std::optional<std::string> error = ScanDestination(src, dst, rules, there, merged)) return error;
    if (there.empty()) return std::nullopt;

    return strprintf("%s already holds %s, which the transfer would create. Nothing is ever copied over an entry "
                     "that is there: check what each one is (a partial copy an interrupted run left can be removed), "
                     "move it away, then try again.",
                     Shown(dst), Join(there, ", "));
}

std::vector<fs::path> MergedDirectories(const fs::path& src, const fs::path& dst, const TransferRules& rules)
{
    std::vector<std::string> there;
    std::vector<fs::path> merged;
    if (ScanDestination(src, dst, rules, there, merged) || !there.empty()) return {};
    return merged;
}

TransferResult TransferTree(const fs::path& src, const fs::path& dst, const TransferRules& rules,
                            const fs::path& quarantine_dir)
{
    TransferResult result;
    Transfer(rules, quarantine_dir, result).Run(src, dst);
    return result;
}

bool RemoveTreeNoFollow(const fs::path& root, const std::vector<fs::path>& only_top, std::string& error)
{
    for (const fs::path& relative : only_top) {
        if (!PlainRelative(relative)) {
            error = Shown(relative) + " is not a path below " + Shown(root);
            return false;
        }
    }

    bool absent = false;
    const std::optional<Dir> top = Dir::Open(root, absent, error);
    if (!top) return absent;

    for (const fs::path& relative : only_top) {
        // The directories on the way, held as themselves.
        const fs::path dirs = relative.parent_path();
        std::vector<Dir> held;
        held.reserve(std::distance(dirs.begin(), dirs.end()));
        const Dir* at = &*top;
        bool there = true;
        for (const fs::path& part : dirs) {
            EntryFacts facts;
            if (!at->Facts(part, facts, absent, error)) {
                if (!absent) return false;
                there = false;
                break;
            }
            if (facts.type != EntryFacts::Type::Directory) {
                there = false;
                break;
            }
            std::optional<Dir> next = at->Sub(part, error);
            if (!next) return false;
            held.push_back(std::move(*next));
            at = &held.back();
        }
        if (there && !at->Remove(relative.filename(), error)) return false;
    }
    return true;
}

bool RemoveDirIfEmpty(const fs::path& root, const fs::path& relative, bool& removed, std::string& error)
{
    removed = false;
    if (!PlainRelative(relative)) {
        error = Shown(relative) + " is not a path below " + Shown(root);
        return false;
    }
    bool absent = false;
    const std::optional<Dir> top = Dir::Open(root, absent, error);
    if (!top) return absent;

    std::vector<Dir> held;
    held.reserve(std::distance(relative.begin(), relative.end()));
    const Dir* at = &*top;
    const fs::path dirs = relative.parent_path();
    for (const fs::path& part : dirs) {
        std::optional<Dir> next = at->Sub(part, error);
        if (!next) return false;
        held.push_back(std::move(*next));
        at = &held.back();
    }

    EntryFacts facts;
    if (!at->Facts(relative.filename(), facts, absent, error)) return absent;
    if (facts.type != EntryFacts::Type::Directory) return true;
    std::optional<Dir> dir = at->Sub(relative.filename(), error);
    if (!dir) return false;
    std::vector<fs::path> names;
    if (!dir->Names(names, error)) return false;
    dir.reset();
    if (!names.empty()) return true;
    if (!at->Remove(relative.filename(), error)) return false;
    removed = true;
    return true;
}

bool ReadFileNoFollow(const fs::path& path, uint64_t limit, std::string& text, bool& absent, std::string& error)
{
    text.clear();
    const std::optional<Dir> parent = Dir::Open(path.parent_path(), absent, error);
    if (!parent) return false;
    EntryFacts facts;
    if (!parent->Facts(path.filename(), facts, absent, error)) return false;
    std::optional<File> in = parent->OpenRead(path.filename(), error);
    if (!in) return false;
    return ReadAll(*in, path, limit, text, error);
}

std::vector<std::string> RunFollowUps(const std::vector<FollowUp>& steps)
{
    std::vector<std::string> warnings;
    std::vector<bool> ok(steps.size(), false);
    for (size_t i = 0; i < steps.size(); ++i) {
        const FollowUp& step = steps[i];
        if (step.needs >= 0 && static_cast<size_t>(step.needs) < i && !ok[step.needs]) {
            warnings.push_back(strprintf("%s was not attempted, as %s failed. To finish it: %s", step.name,
                                         steps[step.needs].name, step.finish));
            continue;
        }
        try {
            step.run();
            ok[i] = true;
        } catch (const std::exception& e) {
            warnings.push_back(strprintf("%s failed: %s. To finish it: %s", step.name, e.what(), step.finish));
        }
    }
    return warnings;
}

#ifdef WIN32
namespace {

//! An absolute security descriptor that holds \p owner and the inherited copies of the entries
//! of \p root_spec that are passed down, as Windows writes them on an entry below the root: each
//! INHERITED_ACE, and on a directory also passed on (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE).
//! The DACL is marked auto-inherited and is not protected.
class InheritedDescriptor
{
public:
    InheritedDescriptor(const Sid& owner, const AclSpec& root_spec, bool directory) : m_owner(owner)
    {
        size_t size = sizeof(ACL);
        for (const AceSpec& ace : root_spec.aces) {
            if (ace.inherit != Inherit::ContainerAndObject) continue;
            if (winacl::FormatSid(ace.trustee).empty()) throw std::runtime_error("an entry's trustee is not a SID");
            size += sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + ace.trustee.size();
        }
        if (winacl::FormatSid(m_owner).empty()) throw std::runtime_error("the owner is not a SID");
        m_acl.assign((size + sizeof(DWORD) - 1) / sizeof(DWORD), 0);
        const PACL acl = reinterpret_cast<PACL>(m_acl.data());
        if (!::InitializeAcl(acl, static_cast<DWORD>(m_acl.size() * sizeof(DWORD)), ACL_REVISION)) {
            throw std::runtime_error("InitializeAcl: " + Win32Text(::GetLastError()));
        }
        const DWORD flags = INHERITED_ACE | (directory ? (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) : 0);
        for (const AceSpec& ace : root_spec.aces) {
            if (ace.inherit != Inherit::ContainerAndObject) continue;
            // AddAccessAllowedAceEx takes a non-const PSID and only reads it.
            if (!::AddAccessAllowedAceEx(acl, ACL_REVISION, flags, ace.mask,
                                         const_cast<unsigned char*>(ace.trustee.data()))) {
                throw std::runtime_error("AddAccessAllowedAceEx: " + Win32Text(::GetLastError()));
            }
        }

        if (!::InitializeSecurityDescriptor(&m_sd, SECURITY_DESCRIPTOR_REVISION) ||
            !::SetSecurityDescriptorOwner(&m_sd, m_owner.data(), FALSE) ||
            !::SetSecurityDescriptorDacl(&m_sd, TRUE, acl, FALSE) ||
            !::SetSecurityDescriptorControl(&m_sd,
                                            static_cast<SECURITY_DESCRIPTOR_CONTROL>(SE_DACL_AUTO_INHERITED |
                                                                                     SE_DACL_PROTECTED),
                                            SE_DACL_AUTO_INHERITED)) {
            throw std::runtime_error("build a security descriptor: " + Win32Text(::GetLastError()));
        }
    }

    InheritedDescriptor(const InheritedDescriptor&) = delete;
    InheritedDescriptor& operator=(const InheritedDescriptor&) = delete;

    PSECURITY_DESCRIPTOR Get() { return &m_sd; }

private:
    Sid m_owner;
    //! DWORD-aligned, as an ACL is.
    std::vector<DWORD> m_acl;
    SECURITY_DESCRIPTOR m_sd{};
};

//! The rights ResetTreeSecurity opens each entry with.
constexpr DWORD SECURITY_ACCESS =
    READ_CONTROL | WRITE_DAC | WRITE_OWNER | FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE;

void RefuseOddEntry(const HandleFacts& facts, const fs::path& path)
{
    if (facts.type == EntryFacts::Type::Symlink || facts.type == EntryFacts::Type::Reparse) {
        throw std::runtime_error(Shown(path) + " is a link or another reparse point, which is not followed");
    }
    if (facts.type == EntryFacts::Type::Other) throw std::runtime_error(Shown(path) + " is neither a file nor a directory");
    if (facts.type == EntryFacts::Type::Regular && facts.links != 1) {
        throw std::runtime_error(strprintf("%s is a file with %u hard links", Shown(path), facts.links));
    }
}

void ResetBelow(const Dir& dir, const Sid& owner, const AclSpec& root_spec)
{
    std::vector<fs::path> names;
    std::string error;
    if (!dir.Names(names, error)) throw std::runtime_error(error);

    for (const fs::path& name : names) {
        HandleFacts facts;
        winacl::UniqueHandle handle = dir.OpenEntry(name, SECURITY_ACCESS, facts, error);
        if (!handle) throw std::runtime_error(error);
        const fs::path path = dir.Child(name);
        RefuseOddEntry(facts, path);

        const bool directory = facts.type == EntryFacts::Type::Directory;
        InheritedDescriptor sd(owner, root_spec, directory);
        if (!::SetKernelObjectSecurity(handle.get(),
                                       OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
                                           UNPROTECTED_DACL_SECURITY_INFORMATION,
                                       sd.Get())) {
            throw std::runtime_error("set the owner and access list of " + Shown(path) + ": " +
                                     Win32Text(::GetLastError()));
        }
        if (const std::optional<std::string> refusal =
                InheritedOnlyRefusal(winacl::ReadAcl(handle.get(), /*kernel_object=*/false), owner, root_spec)) {
            throw std::runtime_error(Shown(path) + " did not keep the owner and access list it was given: " + *refusal);
        }

        if (directory) {
            DWORD code = 0;
            const std::optional<std::wstring> final_path = FinalPath(handle.get(), code);
            if (!final_path) throw std::runtime_error("read the path of " + Shown(path) + ": " + Win32Text(code));
            std::optional<Dir> sub = Dir::FromHandle(std::move(handle), *final_path, path, error);
            if (!sub) throw std::runtime_error(error);
            ResetBelow(*sub, owner, root_spec);
        }
    }
}

} // namespace

void ResetTreeSecurity(const fs::path& root, const Sid& owner, const AclSpec& root_spec)
{
    // The root through the directory that holds it, held meanwhile, so it is the entry of that
    // name there and nothing a link at the name leads to.
    std::string error;
    bool absent = false;
    const std::optional<Dir> parent = Dir::Open(root.parent_path(), absent, error);
    if (!parent) throw std::runtime_error(error);
    HandleFacts facts;
    winacl::UniqueHandle handle = parent->OpenEntry(root.filename(), SECURITY_ACCESS, facts, error);
    if (!handle) throw std::runtime_error(error);
    RefuseOddEntry(facts, root);

    // The root alone, through its handle: nothing is passed down from here.
    const bool directory = facts.type == EntryFacts::Type::Directory;
    const AclSpec spec = directory ? root_spec : SpecForFile(root_spec);
    const winacl::UniqueSecurityDescriptor sd(winacl::MakeSecurityDescriptor(spec));
    if (!::SetKernelObjectSecurity(handle.get(),
                                   OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
                                       PROTECTED_DACL_SECURITY_INFORMATION,
                                   sd.get())) {
        throw std::runtime_error("set the owner and access list of " + Shown(root) + ": " + Win32Text(::GetLastError()));
    }
    if (const std::optional<std::string> differs =
            winacl::CheckMatchesSpec(winacl::ReadAcl(handle.get(), /*kernel_object=*/false), spec)) {
        throw std::runtime_error(Shown(root) + " did not keep the owner and access list it was given: " + *differs);
    }

    if (!directory) return;
    DWORD code = 0;
    const std::optional<std::wstring> final_path = FinalPath(handle.get(), code);
    if (!final_path) throw std::runtime_error("read the path of " + Shown(root) + ": " + Win32Text(code));
    const std::optional<Dir> dir = Dir::FromHandle(std::move(handle), *final_path, root, error);
    if (!dir) throw std::runtime_error(error);
    ResetBelow(*dir, owner, root_spec);
}
#endif // WIN32

} // namespace winsvc
