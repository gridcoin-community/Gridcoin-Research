// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/win_acl.h"

#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/string.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
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

// The layout of an ACE, as ParseAce reads it. On Windows each value is checked against
// the SDK below.
constexpr uint8_t TYPE_ALLOWED = 0x00, TYPE_DENIED = 0x01, TYPE_AUDIT = 0x02, TYPE_ALARM = 0x03,
    TYPE_ALLOWED_COMPOUND = 0x04, TYPE_ALLOWED_OBJECT = 0x05, TYPE_DENIED_OBJECT = 0x06, TYPE_AUDIT_OBJECT = 0x07,
    TYPE_ALARM_OBJECT = 0x08, TYPE_ALLOWED_CALLBACK = 0x09, TYPE_DENIED_CALLBACK = 0x0A,
    TYPE_ALLOWED_CALLBACK_OBJECT = 0x0B, TYPE_DENIED_CALLBACK_OBJECT = 0x0C, TYPE_AUDIT_CALLBACK = 0x0D,
    TYPE_ALARM_CALLBACK = 0x0E, TYPE_AUDIT_CALLBACK_OBJECT = 0x0F, TYPE_ALARM_CALLBACK_OBJECT = 0x10,
    TYPE_MANDATORY_LABEL = 0x11, TYPE_RESOURCE_ATTRIBUTE = 0x12, TYPE_SCOPED_POLICY_ID = 0x13,
    TYPE_PROCESS_TRUST_LABEL = 0x14, TYPE_ACCESS_FILTER = 0x15;

constexpr uint8_t FLAG_OBJECT_INHERIT = 0x01, FLAG_CONTAINER_INHERIT = 0x02, FLAG_NO_PROPAGATE_INHERIT = 0x04,
    FLAG_INHERIT_ONLY = 0x08, FLAG_INHERITED = 0x10;

//! The object flags of an object entry, saying which of its two GUIDs are present.
constexpr uint32_t FLAG_OBJECT_TYPE_PRESENT = 0x1, FLAG_INHERITED_OBJECT_TYPE_PRESENT = 0x2;

//! The header is the type, the flags and the 16-bit size of the whole entry. The mask
//! follows it; in a plain entry the SID follows the mask, in an object entry the
//! object flags and GUIDs come first, and in a compound entry a 16-bit compound type
//! and 16 reserved bits.
constexpr size_t HEADER_SIZE = 4, MASK_OFFSET = 4, SID_OFFSET = 8, OBJECT_FLAGS_OFFSET = 8,
    OBJECT_GUIDS_OFFSET = 12, GUID_SIZE = 16, COMPOUND_SID_OFFSET = 12;

//! A SID's revision, sub-authority count and 48-bit authority, before its sub-authorities.
constexpr size_t SID_HEADER_SIZE = 8;

uint32_t ReadLittleEndian32(const unsigned char* p)
{
    return uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 | uint32_t{p[3]} << 24;
}

//! The SID at \p offset in the \p size bytes at \p bytes, if a well-formed one starts
//! there and ends within them.
std::optional<Sid> SidIn(const unsigned char* bytes, size_t size, size_t offset)
{
    if (offset > size || size - offset < SID_HEADER_SIZE) return std::nullopt;

    const size_t length = SID_HEADER_SIZE + 4 * size_t{bytes[offset + 1]};
    if (size - offset < length) return std::nullopt;

    Sid sid(bytes + offset, bytes + offset + length);
    if (!WellFormed(sid)) return std::nullopt;

    return sid;
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

#ifdef WIN32
static_assert(TYPE_ALLOWED == ACCESS_ALLOWED_ACE_TYPE && TYPE_DENIED == ACCESS_DENIED_ACE_TYPE);
static_assert(TYPE_AUDIT == SYSTEM_AUDIT_ACE_TYPE && TYPE_ALARM == SYSTEM_ALARM_ACE_TYPE);
static_assert(TYPE_ALLOWED_COMPOUND == ACCESS_ALLOWED_COMPOUND_ACE_TYPE);
static_assert(TYPE_ALLOWED_OBJECT == ACCESS_ALLOWED_OBJECT_ACE_TYPE && TYPE_DENIED_OBJECT == ACCESS_DENIED_OBJECT_ACE_TYPE);
static_assert(TYPE_AUDIT_OBJECT == SYSTEM_AUDIT_OBJECT_ACE_TYPE && TYPE_ALARM_OBJECT == SYSTEM_ALARM_OBJECT_ACE_TYPE);
static_assert(TYPE_ALLOWED_CALLBACK == ACCESS_ALLOWED_CALLBACK_ACE_TYPE);
static_assert(TYPE_DENIED_CALLBACK == ACCESS_DENIED_CALLBACK_ACE_TYPE);
static_assert(TYPE_ALLOWED_CALLBACK_OBJECT == ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE);
static_assert(TYPE_DENIED_CALLBACK_OBJECT == ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE);
static_assert(TYPE_AUDIT_CALLBACK == SYSTEM_AUDIT_CALLBACK_ACE_TYPE && TYPE_ALARM_CALLBACK == SYSTEM_ALARM_CALLBACK_ACE_TYPE);
static_assert(TYPE_AUDIT_CALLBACK_OBJECT == SYSTEM_AUDIT_CALLBACK_OBJECT_ACE_TYPE);
static_assert(TYPE_ALARM_CALLBACK_OBJECT == SYSTEM_ALARM_CALLBACK_OBJECT_ACE_TYPE);
static_assert(TYPE_MANDATORY_LABEL == SYSTEM_MANDATORY_LABEL_ACE_TYPE);
static_assert(TYPE_RESOURCE_ATTRIBUTE == SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE);
static_assert(TYPE_SCOPED_POLICY_ID == SYSTEM_SCOPED_POLICY_ID_ACE_TYPE);
static_assert(TYPE_PROCESS_TRUST_LABEL == SYSTEM_PROCESS_TRUST_LABEL_ACE_TYPE);
static_assert(TYPE_ACCESS_FILTER == SYSTEM_ACCESS_FILTER_ACE_TYPE);

static_assert(FLAG_OBJECT_INHERIT == OBJECT_INHERIT_ACE && FLAG_CONTAINER_INHERIT == CONTAINER_INHERIT_ACE);
static_assert(FLAG_NO_PROPAGATE_INHERIT == NO_PROPAGATE_INHERIT_ACE && FLAG_INHERIT_ONLY == INHERIT_ONLY_ACE);
static_assert(FLAG_INHERITED == INHERITED_ACE);
static_assert(FLAG_OBJECT_TYPE_PRESENT == ACE_OBJECT_TYPE_PRESENT);
static_assert(FLAG_INHERITED_OBJECT_TYPE_PRESENT == ACE_INHERITED_OBJECT_TYPE_PRESENT);

static_assert(HEADER_SIZE == sizeof(ACE_HEADER) && MASK_OFFSET == offsetof(ACCESS_ALLOWED_ACE, Mask));
static_assert(SID_OFFSET == offsetof(ACCESS_ALLOWED_ACE, SidStart));
static_assert(SID_OFFSET == offsetof(ACCESS_DENIED_CALLBACK_ACE, SidStart));
static_assert(SID_OFFSET == offsetof(SYSTEM_AUDIT_ACE, SidStart));
static_assert(SID_OFFSET == offsetof(SYSTEM_MANDATORY_LABEL_ACE, SidStart));
static_assert(OBJECT_FLAGS_OFFSET == offsetof(ACCESS_ALLOWED_OBJECT_ACE, Flags));
static_assert(OBJECT_GUIDS_OFFSET == offsetof(ACCESS_ALLOWED_OBJECT_ACE, ObjectType) && GUID_SIZE == sizeof(GUID));
static_assert(OBJECT_GUIDS_OFFSET + 2 * GUID_SIZE == offsetof(ACCESS_ALLOWED_OBJECT_ACE, SidStart));
static_assert(OBJECT_GUIDS_OFFSET + 2 * GUID_SIZE == offsetof(ACCESS_DENIED_CALLBACK_OBJECT_ACE, SidStart));
static_assert(SID_HEADER_SIZE == offsetof(SID, SubAuthority) && MAX_SUB_AUTHORITIES == SID_MAX_SUB_AUTHORITIES);
#endif // WIN32

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

std::optional<ObservedAce> ParseAce(const unsigned char* bytes, size_t size)
{
    if (bytes == nullptr || size < HEADER_SIZE) return std::nullopt;

    // The size field covers the whole entry, which may not reach past what may be read.
    const size_t ace_size = size_t{bytes[2]} | size_t{bytes[3]} << 8;
    if (ace_size < HEADER_SIZE || ace_size > size) return std::nullopt;

    ObservedAce ace;
    const uint8_t flags = bytes[1];
    ace.inherit_only = (flags & FLAG_INHERIT_ONLY) != 0;
    ace.inherited = (flags & FLAG_INHERITED) != 0;

    const uint8_t propagation =
        static_cast<uint8_t>(flags & (FLAG_OBJECT_INHERIT | FLAG_CONTAINER_INHERIT | FLAG_NO_PROPAGATE_INHERIT));
    if (propagation == 0) {
        ace.inherit = Inherit::None;
    } else if (propagation == (FLAG_OBJECT_INHERIT | FLAG_CONTAINER_INHERIT)) {
        ace.inherit = Inherit::ContainerAndObject;
    } else {
        ace.inherit = std::nullopt;
    }

    size_t sid_offset = SID_OFFSET;
    bool object = false;

    switch (bytes[0]) {
    case TYPE_ALLOWED:
        ace.kind = AceKind::Allow;
        break;
    case TYPE_ALLOWED_CALLBACK:
        ace.kind = AceKind::OtherAllow;
        break;
    case TYPE_ALLOWED_COMPOUND:
        ace.kind = AceKind::OtherAllow;
        sid_offset = COMPOUND_SID_OFFSET;
        break;
    case TYPE_ALLOWED_OBJECT:
    case TYPE_ALLOWED_CALLBACK_OBJECT:
        ace.kind = AceKind::OtherAllow;
        object = true;
        break;
    case TYPE_DENIED:
    case TYPE_DENIED_CALLBACK:
        ace.kind = AceKind::Deny;
        break;
    case TYPE_DENIED_OBJECT:
    case TYPE_DENIED_CALLBACK_OBJECT:
        ace.kind = AceKind::Deny;
        object = true;
        break;
    case TYPE_AUDIT:
    case TYPE_ALARM:
    case TYPE_AUDIT_CALLBACK:
    case TYPE_ALARM_CALLBACK:
    case TYPE_MANDATORY_LABEL:
    case TYPE_RESOURCE_ATTRIBUTE:
    case TYPE_SCOPED_POLICY_ID:
    case TYPE_PROCESS_TRUST_LABEL:
    case TYPE_ACCESS_FILTER:
        ace.kind = AceKind::Other;
        break;
    case TYPE_AUDIT_OBJECT:
    case TYPE_ALARM_OBJECT:
    case TYPE_AUDIT_CALLBACK_OBJECT:
    case TYPE_ALARM_CALLBACK_OBJECT:
        ace.kind = AceKind::Other;
        object = true;
        break;
    default:
        // A type with no known layout is read as its header alone.
        ace.kind = AceKind::Other;
        return ace;
    }

    if (object) {
        if (ace_size < OBJECT_GUIDS_OFFSET) return std::nullopt;

        const uint32_t object_flags = ReadLittleEndian32(bytes + OBJECT_FLAGS_OFFSET);
        sid_offset = OBJECT_GUIDS_OFFSET;
        if (object_flags & FLAG_OBJECT_TYPE_PRESENT) sid_offset += GUID_SIZE;
        if (object_flags & FLAG_INHERITED_OBJECT_TYPE_PRESENT) sid_offset += GUID_SIZE;
    }

    std::optional<Sid> trustee = SidIn(bytes, ace_size, sid_offset);
    if (!trustee) return std::nullopt;

    // Every SID offset lies past the mask, so the SID's bounds check covers the mask too.
    ace.mask = ReadLittleEndian32(bytes + MASK_OFFSET);
    ace.trustee = std::move(*trustee);

    return ace;
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

//! A copy of the SID \p psid points to.
std::optional<Sid> SidOf(PSID psid)
{
    if (psid == nullptr || !::IsValidSid(psid)) return std::nullopt;

    return SidIn(static_cast<const unsigned char*>(psid), ::GetLengthSid(psid), 0);
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
        const auto* const acl_begin = reinterpret_cast<const unsigned char*>(dacl);
        const auto* const acl_end = acl_begin + dacl->AclSize;

        for (DWORD i = 0; i < dacl->AceCount; ++i) {
            void* entry = nullptr;
            if (!::GetAce(dacl, i, &entry)) {
                const DWORD error = ::GetLastError();
                ::LocalFree(sd);
                ThrowWin32(error, "GetAce");
            }

            // An entry lies inside its ACL, and what is left of the ACL is what may be read.
            const auto* const bytes = static_cast<const unsigned char*>(entry);
            std::optional<ObservedAce> ace;
            if (bytes >= acl_begin + sizeof(ACL) && bytes < acl_end) {
                ace = ParseAce(bytes, static_cast<size_t>(acl_end - bytes));
            }
            if (!ace) {
                ::LocalFree(sd);
                ThrowWin32(ERROR_INVALID_ACL, "ParseAce");
            }
            acl.aces.push_back(std::move(*ace));
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
