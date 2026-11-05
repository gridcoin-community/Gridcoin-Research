// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/win_acl.h"

#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/string.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <initializer_list>
#include <system_error>

#ifdef WIN32
// Each single right as declared in the header, taken before <windows.h> defines a
// macro of the same name, so that it can be checked against the SDK below.
#define WINACL_SINGLE_RIGHTS(X) \
    X(FILE_READ_DATA) X(FILE_LIST_DIRECTORY) X(FILE_WRITE_DATA) X(FILE_ADD_FILE) X(FILE_APPEND_DATA) \
    X(FILE_ADD_SUBDIRECTORY) X(FILE_READ_EA) X(FILE_WRITE_EA) X(FILE_EXECUTE) X(FILE_TRAVERSE) \
    X(FILE_DELETE_CHILD) X(FILE_READ_ATTRIBUTES) X(FILE_WRITE_ATTRIBUTES) X(DELETE) X(READ_CONTROL) \
    X(WRITE_DAC) X(WRITE_OWNER) X(SYNCHRONIZE) X(FILE_ALL_ACCESS) X(GENERIC_ALL) X(GENERIC_EXECUTE) \
    X(GENERIC_WRITE) X(GENERIC_READ)

namespace {
#define WINACL_DECLARED(name) constexpr uint32_t declared_##name = util::winacl::mask::name;
WINACL_SINGLE_RIGHTS(WINACL_DECLARED)
#undef WINACL_DECLARED
} // namespace

#include <windows.h>
#include <aclapi.h>

#define WINACL_CHECK(name) \
    static_assert(declared_##name == static_cast<uint32_t>(name), #name " differs from the Windows SDK");
WINACL_SINGLE_RIGHTS(WINACL_CHECK)
#undef WINACL_CHECK
#undef WINACL_SINGLE_RIGHTS
#endif // WIN32

namespace util::winacl {
namespace {

constexpr size_t MAX_SUB_AUTHORITIES = 15;
constexpr uint64_t MAX_AUTHORITY = (uint64_t{1} << 48) - 1;

const char* const NO_DACL = "it has no DACL (all users would have access)";
const char* const NOT_PROTECTED = "its DACL is not protected (it still inherits ACEs from the parent)";
const char* const NO_OWNER = "its owner could not be read";

//! Whether \p sid is revision 1 with one to fifteen sub-authorities and exactly the
//! length its count gives.
bool WellFormed(const Sid& sid)
{
    return sid.size() >= 8 && sid[0] == 1 && sid[1] >= 1 && sid[1] <= MAX_SUB_AUTHORITIES
        && sid.size() == 8 + 4 * size_t{sid[1]};
}

//! A SID's authority, decimal or hex after "0x", below 2^48.
std::optional<uint64_t> ParseAuthority(const std::string& text)
{
    std::optional<uint64_t> value;

    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        uint64_t hex = 0;
        const char* const first = text.data() + 2;
        const char* const last = text.data() + text.size();
        const auto [end, error] = std::from_chars(first, last, hex, 16);
        if (end == last && error == std::errc{}) value = hex;
    } else {
        value = ToIntegral<uint64_t>(text);
    }

    if (!value || *value > MAX_AUTHORITY) return std::nullopt;

    return value;
}

void AppendLittleEndian(Sid& sid, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        sid.push_back(static_cast<unsigned char>(value >> shift));
    }
}

//! A SID under the NT authority (5).
Sid NtAuthority(std::initializer_list<uint32_t> sub_authorities)
{
    Sid sid{1, static_cast<unsigned char>(sub_authorities.size()), 0, 0, 0, 0, 0, 5};
    for (const uint32_t sub_authority : sub_authorities) {
        AppendLittleEndian(sid, sub_authority);
    }

    return sid;
}

//! \p sid's string form, or a stand-in when it has none.
std::string Describe(const Sid& sid)
{
    const std::string text = FormatSid(sid);

    return text.empty() ? "an unreadable SID" : text;
}

bool IsAllowed(const std::vector<Sid>& allowed, const Sid& sid)
{
    return std::find(allowed.begin(), allowed.end(), sid) != allowed.end();
}

//! Whether an entry of \p kind grants access.
bool IsGrant(AceKind kind)
{
    return kind == AceKind::Allow || kind == AceKind::OtherAllow;
}

//! Reason if there is no DACL, or the owner is unknown or not in \p allowed.
std::optional<std::string> FindForeignOwner(const ObservedAcl& acl, const std::vector<Sid>& allowed)
{
    if (!acl.dacl_present) return NO_DACL;
    if (!acl.owner) return NO_OWNER;
    if (!IsAllowed(allowed, *acl.owner)) {
        return strprintf("it is owned by %s, which is not an allowed owner", Describe(*acl.owner));
    }

    return std::nullopt;
}

//! FindForeignWriter and FindForeignReader, with \p verb for the reason.
std::optional<std::string> FindForeignGrant(const ObservedAcl& acl, const std::vector<Sid>& allowed, uint32_t rights,
                                            const char* verb)
{
    if (std::optional<std::string> reason = FindForeignOwner(acl, allowed)) return reason;

    for (const ObservedAce& ace : acl.aces) {
        if (!IsGrant(ace.kind) || ace.inherit_only || (ace.mask & rights) == 0) continue;
        if (IsAllowed(allowed, ace.trustee)) continue;

        return strprintf("its DACL lets %s %s it (access mask 0x%08x)", Describe(ace.trustee), verb, ace.mask);
    }

    return std::nullopt;
}

//! Whether \p ace is the entry \p want applies.
bool Matches(const ObservedAce& ace, const AceSpec& want)
{
    return ace.kind == AceKind::Allow && !ace.inherit_only && !ace.inherited && ace.inherit == want.inherit
        && ace.mask == want.mask && ace.trustee == want.trustee;
}

} // namespace

std::optional<Sid> ParseSidString(std::string_view s)
{
    if (s.substr(0, 2) != "S-") return std::nullopt;

    // The revision, the authority, then the sub-authorities.
    const std::vector<std::string> parts = SplitString(s.substr(2), '-');
    if (parts.size() < 3 || parts.size() > 2 + MAX_SUB_AUTHORITIES || parts[0] != "1") return std::nullopt;

    const std::optional<uint64_t> authority = ParseAuthority(parts[1]);
    if (!authority) return std::nullopt;

    Sid sid{1, static_cast<unsigned char>(parts.size() - 2)};
    for (int shift = 40; shift >= 0; shift -= 8) {
        sid.push_back(static_cast<unsigned char>(*authority >> shift));
    }
    for (size_t i = 2; i < parts.size(); ++i) {
        const std::optional<uint32_t> sub_authority = ToIntegral<uint32_t>(parts[i]);
        if (!sub_authority) return std::nullopt;
        AppendLittleEndian(sid, *sub_authority);
    }

    return sid;
}

std::string FormatSid(const Sid& sid)
{
    if (!WellFormed(sid)) return {};

    uint64_t authority = 0;
    for (size_t i = 2; i < 8; ++i) {
        authority = (authority << 8) | sid[i];
    }

    std::string text = "S-1-";
    if (authority >> 32) {
        text += strprintf("0x%012X", authority);
    } else {
        text += strprintf("%u", authority);
    }

    for (size_t offset = 8; offset < sid.size(); offset += 4) {
        const uint32_t sub_authority = uint32_t{sid[offset]} | uint32_t{sid[offset + 1]} << 8
            | uint32_t{sid[offset + 2]} << 16 | uint32_t{sid[offset + 3]} << 24;
        text += strprintf("-%u", sub_authority);
    }

    return text;
}

namespace wellknown {
Sid System() { return NtAuthority({18}); }
Sid Administrators() { return NtAuthority({32, 544}); }
Sid TrustedInstaller() { return NtAuthority({80, 956008885, 3418522649, 1831038044, 1853292631, 2271478464}); }
Sid Users() { return NtAuthority({32, 545}); }
Sid AnonymousLogon() { return NtAuthority({7}); }
} // namespace wellknown

std::optional<std::string> FindForeignWriter(const ObservedAcl& acl, const std::vector<Sid>& allowed,
                                             uint32_t writeish)
{
    return FindForeignGrant(acl, allowed, writeish, "change");
}

std::optional<std::string> FindForeignReader(const ObservedAcl& acl, const std::vector<Sid>& allowed)
{
    return FindForeignGrant(acl, allowed, mask::READISH, "read");
}

std::optional<std::string> FindForeignTrustee(const ObservedAcl& acl, const std::vector<Sid>& allowed)
{
    if (std::optional<std::string> reason = FindForeignOwner(acl, allowed)) return reason;

    for (const ObservedAce& ace : acl.aces) {
        if (!IsGrant(ace.kind) || IsAllowed(allowed, ace.trustee)) continue;

        return strprintf("its DACL grants %s access%s (access mask 0x%08x)", Describe(ace.trustee),
                         ace.inherit_only ? " to what is created inside it" : "", ace.mask);
    }

    return std::nullopt;
}

std::optional<std::string> CheckMatchesSpec(const ObservedAcl& acl, const AclSpec& spec)
{
    if (!acl.dacl_present) return NO_DACL;
    if (!acl.is_protected) return NOT_PROTECTED;
    if (!acl.owner) return NO_OWNER;
    if (*acl.owner != spec.owner) {
        return strprintf("it is owned by %s, not %s", Describe(*acl.owner), Describe(spec.owner));
    }

    for (const ObservedAce& ace : acl.aces) {
        if (!IsGrant(ace.kind)) continue;

        const bool applied = std::any_of(spec.aces.begin(), spec.aces.end(),
                                         [&ace](const AceSpec& want) { return Matches(ace, want); });
        if (!applied) {
            return strprintf("its DACL holds an entry that was not applied (%s, access mask 0x%08x)",
                             Describe(ace.trustee), ace.mask);
        }
    }

    for (const AceSpec& want : spec.aces) {
        const bool present = std::any_of(acl.aces.begin(), acl.aces.end(),
                                         [&want](const ObservedAce& ace) { return Matches(ace, want); });
        if (!present) {
            return strprintf("its DACL lacks the entry for %s (access mask 0x%08x)", Describe(want.trustee), want.mask);
        }
    }

    return std::nullopt;
}

#ifdef WIN32
namespace {

[[noreturn]] void ThrowWin32(DWORD error, const std::string& what)
{
    throw std::system_error(static_cast<int>(error), std::system_category(), what);
}

//! Throw unless \p sid is well formed. Windows reads a SID by the length its count
//! gives, so a malformed one would be read past its end.
void RequireWellFormed(const Sid& sid, const char* what)
{
    if (!WellFormed(sid)) throw std::runtime_error(strprintf("%s is not a well-formed SID", what));
}

//! The SID at \p p, if a well-formed one starts there and ends no later than \p end.
std::optional<Sid> SidAt(const unsigned char* p, const unsigned char* end)
{
    if (p >= end || end - p < 8) return std::nullopt;

    const size_t length = 8 + 4 * size_t{p[1]};
    if (static_cast<size_t>(end - p) < length) return std::nullopt;

    Sid sid(p, p + length);
    if (!WellFormed(sid)) return std::nullopt;

    return sid;
}

//! A copy of the SID \p psid points to.
std::optional<Sid> SidOf(PSID psid)
{
    if (psid == nullptr || !::IsValidSid(psid)) return std::nullopt;

    const auto* const p = static_cast<const unsigned char*>(psid);

    return SidAt(p, p + ::GetLengthSid(psid));
}

//! The offset of the SID in an object entry: after its flags and whichever GUIDs they
//! say are present. 0 if the entry is too short to hold its flags.
size_t ObjectAceSidOffset(const unsigned char* bytes, size_t size)
{
    const size_t flags_offset = sizeof(ACE_HEADER) + sizeof(ACCESS_MASK);
    if (size < flags_offset + sizeof(DWORD)) return 0;

    DWORD flags = 0;
    std::memcpy(&flags, bytes + flags_offset, sizeof(flags));

    size_t offset = flags_offset + sizeof(DWORD);
    if (flags & ACE_OBJECT_TYPE_PRESENT) offset += sizeof(GUID);
    if (flags & ACE_INHERITED_OBJECT_TYPE_PRESENT) offset += sizeof(GUID);

    return offset;
}

ObservedAce ReadAce(const void* entry)
{
    const auto* const header = static_cast<const ACE_HEADER*>(entry);
    const auto* const bytes = static_cast<const unsigned char*>(entry);
    const size_t size = header->AceSize;

    ObservedAce ace;
    ace.inherit_only = (header->AceFlags & INHERIT_ONLY_ACE) != 0;
    ace.inherited = (header->AceFlags & INHERITED_ACE) != 0;

    const BYTE propagation =
        static_cast<BYTE>(header->AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | NO_PROPAGATE_INHERIT_ACE));
    if (propagation == 0) {
        ace.inherit = Inherit::None;
    } else if (propagation == (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)) {
        ace.inherit = Inherit::ContainerAndObject;
    } else {
        ace.inherit = std::nullopt;
    }

    // Every access entry starts with its header and a 32-bit mask.
    if (size >= sizeof(ACE_HEADER) + sizeof(ACCESS_MASK)) {
        std::memcpy(&ace.mask, bytes + sizeof(ACE_HEADER), sizeof(ace.mask));
    }

    const size_t plain_sid_offset = sizeof(ACE_HEADER) + sizeof(ACCESS_MASK);
    size_t sid_offset = 0;

    switch (header->AceType) {
    case ACCESS_ALLOWED_ACE_TYPE:
        ace.kind = AceKind::Allow;
        sid_offset = plain_sid_offset;
        break;
    case ACCESS_ALLOWED_CALLBACK_ACE_TYPE:
        ace.kind = AceKind::OtherAllow;
        sid_offset = plain_sid_offset;
        break;
    case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
    case ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE:
        ace.kind = AceKind::OtherAllow;
        sid_offset = ObjectAceSidOffset(bytes, size);
        break;
    case ACCESS_ALLOWED_COMPOUND_ACE_TYPE:
        // The mask, a 16-bit compound type and 16 reserved bits, then the server SID.
        ace.kind = AceKind::OtherAllow;
        sid_offset = plain_sid_offset + sizeof(DWORD);
        break;
    case ACCESS_DENIED_ACE_TYPE:
    case ACCESS_DENIED_CALLBACK_ACE_TYPE:
        ace.kind = AceKind::Deny;
        sid_offset = plain_sid_offset;
        break;
    case ACCESS_DENIED_OBJECT_ACE_TYPE:
    case ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE:
        ace.kind = AceKind::Deny;
        sid_offset = ObjectAceSidOffset(bytes, size);
        break;
    default:
        ace.kind = AceKind::Other;
        break;
    }

    if (sid_offset != 0 && sid_offset < size) {
        if (std::optional<Sid> trustee = SidAt(bytes + sid_offset, bytes + size)) ace.trustee = std::move(*trustee);
    }

    return ace;
}

//! What GetNamedSecurityInfoW or GetSecurityInfo returned, as an ObservedAcl. Frees \p sd.
ObservedAcl ToObservedAcl(PSECURITY_DESCRIPTOR sd, PSID owner, PACL dacl)
{
    SECURITY_DESCRIPTOR_CONTROL control{};
    DWORD revision = 0;
    if (!::GetSecurityDescriptorControl(sd, &control, &revision)) {
        const DWORD error = ::GetLastError();
        ::LocalFree(sd);
        ThrowWin32(error, "GetSecurityDescriptorControl");
    }

    ObservedAcl acl;
    acl.owner = SidOf(owner);
    // A NULL DACL, like an absent one, gives everyone full access.
    acl.dacl_present = (control & SE_DACL_PRESENT) != 0 && dacl != nullptr;
    acl.is_protected = (control & SE_DACL_PROTECTED) != 0;

    if (acl.dacl_present) {
        for (DWORD i = 0; i < dacl->AceCount; ++i) {
            void* entry = nullptr;
            if (!::GetAce(dacl, i, &entry)) {
                const DWORD error = ::GetLastError();
                ::LocalFree(sd);
                ThrowWin32(error, "GetAce");
            }
            acl.aces.push_back(ReadAce(entry));
        }
    }

    ::LocalFree(sd);

    return acl;
}

//! The owner of \p wpath, or nullopt if it cannot be read.
std::optional<Sid> ReadOwner(std::wstring& wpath)
{
    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (::GetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr,
                                nullptr, &sd) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    std::optional<Sid> sid = SidOf(owner);
    ::LocalFree(sd);

    return sid;
}

//! Query \p what of this process's token into \p buffer.
void QueryProcessToken(TOKEN_INFORMATION_CLASS what, std::vector<unsigned char>& buffer, const char* name)
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        ThrowWin32(::GetLastError(), "OpenProcessToken");
    }

    // The first call sizes the buffer; it fails with ERROR_INSUFFICIENT_BUFFER by design.
    DWORD length = 0;
    ::GetTokenInformation(token, what, nullptr, 0, &length);
    buffer.assign(length ? length : 1, 0);

    if (!::GetTokenInformation(token, what, buffer.data(), length, &length)) {
        const DWORD error = ::GetLastError();
        ::CloseHandle(token);
        ThrowWin32(error, std::string("GetTokenInformation(") + name + ")");
    }

    ::CloseHandle(token);
}

} // namespace

Sid CurrentTokenUser()
{
    std::vector<unsigned char> buffer;
    QueryProcessToken(TokenUser, buffer, "TokenUser");

    std::optional<Sid> sid = SidOf(reinterpret_cast<const TOKEN_USER*>(buffer.data())->User.Sid);
    if (!sid) ThrowWin32(ERROR_INVALID_SID, "GetTokenInformation(TokenUser)");

    return *sid;
}

bool CurrentTokenIsElevated()
{
    std::vector<unsigned char> buffer;
    QueryProcessToken(TokenElevation, buffer, "TokenElevation");
    if (buffer.size() < sizeof(TOKEN_ELEVATION)) {
        ThrowWin32(ERROR_INSUFFICIENT_BUFFER, "GetTokenInformation(TokenElevation)");
    }

    TOKEN_ELEVATION elevation{};
    std::memcpy(&elevation, buffer.data(), sizeof(elevation));

    return elevation.TokenIsElevated != 0;
}

ObservedAcl ReadAcl(const fs::path& path)
{
    std::wstring wpath = path.wstring();
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;

    // PROTECTED_DACL_SECURITY_INFORMATION is a set-side flag and is rejected here.
    // Whether the DACL is protected is read from the descriptor's control bits.
    const DWORD rc = ::GetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT,
                                             OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                             &owner, nullptr, &dacl, nullptr, &sd);
    if (rc != ERROR_SUCCESS) ThrowWin32(rc, "GetNamedSecurityInfo " + path.string());

    return ToObservedAcl(sd, owner, dacl);
}

ObservedAcl ReadAcl(void* handle, bool kernel_object)
{
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;

    const DWORD rc = ::GetSecurityInfo(static_cast<HANDLE>(handle), kernel_object ? SE_KERNEL_OBJECT : SE_FILE_OBJECT,
                                       OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                       &owner, nullptr, &dacl, nullptr, &sd);
    if (rc != ERROR_SUCCESS) ThrowWin32(rc, "GetSecurityInfo");

    return ToObservedAcl(sd, owner, dacl);
}

AclNotKeptError::AclNotKeptError(const fs::path& path, std::string reason)
    : std::runtime_error(
          strprintf("'%s' did not keep the access control list applied to it -- %s", path.string(), reason))
    , m_reason(std::move(reason))
{
}

void ApplyAcl(const fs::path& path, const AclSpec& spec)
{
    RequireWellFormed(spec.owner, "the owner");
    PACL dacl = static_cast<PACL>(BuildDacl(spec.aces));

    // SetNamedSecurityInfoW takes a mutable LPWSTR and PSID, hence the local copies.
    std::wstring wpath = path.wstring();
    Sid owner = spec.owner;

    // Writing the owner needs WRITE_OWNER, which owning the object does not give. An
    // owner that is already right is left alone, so only an object whose owner has to
    // change needs that right.
    SECURITY_INFORMATION info = DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION;
    if (ReadOwner(wpath) != spec.owner) info |= OWNER_SECURITY_INFORMATION;

    const DWORD rc = ::SetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT, info,
                                             (info & OWNER_SECURITY_INFORMATION) ? owner.data() : nullptr,
                                             nullptr, dacl, nullptr);
    ::LocalFree(dacl);
    if (rc != ERROR_SUCCESS) ThrowWin32(rc, "SetNamedSecurityInfo " + path.string());

    // Setting an ACL can succeed nominally on a volume that does not store one (FAT and
    // exFAT volumes, some network redirectors), so it is read back before anything
    // relies on it.
    if (std::optional<std::string> reason = CheckMatchesSpec(ReadAcl(path), spec)) {
        throw AclNotKeptError(path, std::move(*reason));
    }
}

void* BuildDacl(const std::vector<AceSpec>& aces)
{
    // Each allow entry is its header and mask followed by the SID.
    size_t size = sizeof(ACL);
    for (const AceSpec& ace : aces) {
        RequireWellFormed(ace.trustee, "an entry's trustee");
        size += sizeof(ACE_HEADER) + sizeof(ACCESS_MASK) + ace.trustee.size();
    }
    // An ACL's size is a 16-bit field, and stays DWORD-aligned.
    if (size > 0xFFFC) throw std::runtime_error("BuildDacl: too many entries for one ACL");

    PACL dacl = static_cast<PACL>(::LocalAlloc(LPTR, size));
    if (dacl == nullptr) ThrowWin32(::GetLastError(), "LocalAlloc");

    if (!::InitializeAcl(dacl, static_cast<DWORD>(size), ACL_REVISION)) {
        const DWORD error = ::GetLastError();
        ::LocalFree(dacl);
        ThrowWin32(error, "InitializeAcl");
    }

    for (const AceSpec& ace : aces) {
        const DWORD flags =
            ace.inherit == Inherit::ContainerAndObject ? (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) : 0;
        // AddAccessAllowedAceEx takes a non-const PSID and only reads it.
        if (!::AddAccessAllowedAceEx(dacl, ACL_REVISION, flags, ace.mask,
                                     const_cast<unsigned char*>(ace.trustee.data()))) {
            const DWORD error = ::GetLastError();
            ::LocalFree(dacl);
            ThrowWin32(error, "AddAccessAllowedAceEx");
        }
    }

    return dacl;
}

void* MakeSecurityDescriptor(const AclSpec& spec)
{
    RequireWellFormed(spec.owner, "the owner");
    PACL dacl = static_cast<PACL>(BuildDacl(spec.aces));
    Sid owner = spec.owner;

    // The absolute descriptor points at the owner and DACL above. The self-relative copy
    // made from it holds its own, and outlives them.
    SECURITY_DESCRIPTOR absolute{};
    void* relative = nullptr;
    const char* failed = nullptr;

    if (!::InitializeSecurityDescriptor(&absolute, SECURITY_DESCRIPTOR_REVISION)) {
        failed = "InitializeSecurityDescriptor";
    } else if (!::SetSecurityDescriptorOwner(&absolute, owner.data(), FALSE)) {
        failed = "SetSecurityDescriptorOwner";
    } else if (!::SetSecurityDescriptorDacl(&absolute, TRUE, dacl, FALSE)) {
        failed = "SetSecurityDescriptorDacl";
    } else if (!::SetSecurityDescriptorControl(&absolute, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) {
        failed = "SetSecurityDescriptorControl";
    } else {
        // The first call sizes the buffer; it fails with ERROR_INSUFFICIENT_BUFFER by design.
        DWORD length = 0;
        ::MakeSelfRelativeSD(&absolute, nullptr, &length);
        relative = length ? ::LocalAlloc(LPTR, length) : nullptr;
        if (relative == nullptr || !::MakeSelfRelativeSD(&absolute, relative, &length)) {
            failed = "MakeSelfRelativeSD";
        }
    }
    const DWORD error = failed != nullptr ? ::GetLastError() : ERROR_SUCCESS;

    ::LocalFree(dacl);
    if (failed != nullptr) {
        if (relative != nullptr) ::LocalFree(relative);
        ThrowWin32(error, failed);
    }

    return relative;
}

void FreeSecurityDescriptor(void* sd)
{
    if (sd != nullptr) ::LocalFree(sd);
}

std::optional<Sid> LookupAccountSidByName(const std::wstring& name, bool require_alias)
{
    DWORD sid_size = 0;
    DWORD domain_size = 0;
    SID_NAME_USE use{};

    // The first call sizes both buffers; it fails with ERROR_INSUFFICIENT_BUFFER by design.
    ::LookupAccountNameW(nullptr, name.c_str(), nullptr, &sid_size, nullptr, &domain_size, &use);
    DWORD error = ::GetLastError();
    if (error == ERROR_NONE_MAPPED) return std::nullopt;
    if (error != ERROR_INSUFFICIENT_BUFFER || sid_size == 0) ThrowWin32(error, "LookupAccountNameW");

    Sid sid(sid_size);
    std::wstring domain(domain_size, L'\0');
    if (!::LookupAccountNameW(nullptr, name.c_str(), sid.data(), &sid_size, domain.data(), &domain_size, &use)) {
        error = ::GetLastError();
        if (error == ERROR_NONE_MAPPED) return std::nullopt;
        ThrowWin32(error, "LookupAccountNameW");
    }

    if (require_alias && use != SidTypeAlias) return std::nullopt;

    std::optional<Sid> found = SidOf(sid.data());
    if (!found) ThrowWin32(ERROR_INVALID_SID, "LookupAccountNameW");

    return found;
}

std::optional<std::string> FindForeignWriterOnChain(const fs::path& leaf, const std::vector<Sid>& allowed)
{
    if (!leaf.is_absolute()) return strprintf("'%s' is not an absolute path", leaf.string());
    for (const fs::path& part : leaf) {
        if (part == "." || part == "..") return strprintf("'%s' has a '.' or '..' component", leaf.string());
    }

    if (std::optional<std::string> reason = FindForeignWriter(ReadAcl(leaf), allowed, mask::FILE_WRITEISH)) {
        return strprintf("'%s': %s", leaf.string(), *reason);
    }

    // Every directory above it, up to and including the root of its volume.
    fs::path dir = leaf;
    while (dir.has_relative_path()) {
        dir = dir.parent_path();
        if (std::optional<std::string> reason = FindForeignWriter(ReadAcl(dir), allowed, mask::CHAIN_DIR_WRITEISH)) {
            return strprintf("'%s': %s", dir.string(), *reason);
        }
    }

    return std::nullopt;
}
#endif // WIN32

} // namespace util::winacl
