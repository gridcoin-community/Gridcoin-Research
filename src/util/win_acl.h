// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_WIN_ACL_H
#define GRIDCOIN_UTIL_WIN_ACL_H

#include "fs.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

//! Windows access control stated as data, and strict checks against it.
//!
//! An AclSpec is what an object's owner and DACL must be; an ObservedAcl is what
//! ReadAcl found on one. The checks between them are pure, so they build and are
//! tested on every platform. Only reading, applying and building an ACL needs
//! Windows.
//!
//! The header stays free of <windows.h>: a SID is a byte vector in the binary form
//! Windows uses, and a handle or security descriptor is a void*.
namespace util::winacl {

//! A SID in its binary form: the revision, the sub-authority count, the 48-bit
//! authority big-endian, then each 32-bit sub-authority little-endian.
using Sid = std::vector<unsigned char>;

//! Parse a SID string such as "S-1-5-18". nullopt unless it is revision 1, with one to
//! fifteen sub-authorities of 32 bits, and nothing else, in the one form FormatSid
//! writes: decimal numbers with no leading zero, and an authority below 2^32 in
//! decimal or one of 2^32 or more as "0x" and twelve upper-case hex digits.
std::optional<Sid> ParseSidString(std::string_view s);

//! The string form of \p sid as ConvertSidToStringSid writes it, an authority of
//! 2^32 or more in hex. Empty if \p sid is not a well-formed SID with one to
//! fifteen sub-authorities.
std::string FormatSid(const Sid& sid);

namespace wellknown {
Sid System();           //!< S-1-5-18, NT AUTHORITY\SYSTEM
Sid Administrators();   //!< S-1-5-32-544, BUILTIN\Administrators
Sid TrustedInstaller(); //!< S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464
Sid Users();            //!< S-1-5-32-545, BUILTIN\Users
Sid AnonymousLogon();   //!< S-1-5-7, NT AUTHORITY\ANONYMOUS LOGON
} // namespace wellknown

//! SECURITY_MANDATORY_MEDIUM_RID, the integrity level of an ordinary desktop process
//! (checked against the SDK in win_acl.cpp).
constexpr uint32_t INTEGRITY_MEDIUM_RID = 0x2000;

//! Whether a process whose token is \p token_elevated (TokenElevation) and runs at
//! \p integrity_rid counts as elevated: its token is elevated, or its integrity level is
//! above Medium, which a token need not report as elevated. A level below Medium does not
//! count. Pure.
bool AboveMediumOrElevated(bool token_elevated, uint32_t integrity_rid);

//! The attributes of a token's group that say how it counts, with the Win32 values of
//! SE_GROUP_ENABLED and SE_GROUP_USE_FOR_DENY_ONLY (checked against the SDK in
//! win_acl.cpp). A deny-only group only ever denies access.
constexpr uint32_t GROUP_ENABLED = 0x4, GROUP_USE_FOR_DENY_ONLY = 0x10;

//! One group of an access token: its SID and its SE_GROUP_* attributes.
struct TokenGroup
{
    Sid sid;
    uint32_t attributes{0};
};

//! Access rights, with their Win32 values; win_acl.cpp checks each against the SDK.
//!
//! <windows.h> defines every single right below as a macro of the same name and
//! value. In a file that includes it before this header the macros stand in for
//! them, and the combinations are built from the macros. In any file that includes
//! <windows.h>, write the bare SDK name for a single right; the combinations can be
//! named as mask::FILE_WRITEISH and so on everywhere.
namespace mask {
#ifndef FILE_READ_DATA
constexpr uint32_t FILE_READ_DATA = 0x1, FILE_LIST_DIRECTORY = 0x1, FILE_WRITE_DATA = 0x2, FILE_ADD_FILE = 0x2,
    FILE_APPEND_DATA = 0x4, FILE_ADD_SUBDIRECTORY = 0x4, FILE_READ_EA = 0x8, FILE_WRITE_EA = 0x10,
    FILE_EXECUTE = 0x20, FILE_TRAVERSE = 0x20, FILE_DELETE_CHILD = 0x40, FILE_READ_ATTRIBUTES = 0x80,
    FILE_WRITE_ATTRIBUTES = 0x100, DELETE = 0x10000, READ_CONTROL = 0x20000, WRITE_DAC = 0x40000,
    WRITE_OWNER = 0x80000, SYNCHRONIZE = 0x100000, FILE_ALL_ACCESS = 0x1F01FF, GENERIC_ALL = 0x10000000,
    GENERIC_EXECUTE = 0x20000000, GENERIC_WRITE = 0x40000000, GENERIC_READ = 0x80000000;
#endif

//! A file: anything that changes what it holds or who controls it.
constexpr uint32_t FILE_WRITEISH = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES
    | DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_ALL | GENERIC_WRITE;

//! An ancestor directory on an executed path. This leaves out FILE_ADD_SUBDIRECTORY,
//! which every volume root grants to Users and which cannot replace an existing
//! entry.
constexpr uint32_t CHAIN_DIR_WRITEISH = FILE_ADD_FILE | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_DELETE_CHILD
    | DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_ALL | GENERIC_WRITE;

//! An IPC directory, where nobody else may add, remove or replace an entry.
constexpr uint32_t IPC_DIR_WRITEISH = CHAIN_DIR_WRITEISH | FILE_ADD_SUBDIRECTORY;

//! A directory above a data file's own directory, where only the entries that already
//! exist on the path matter: the rights that replace or re-point one (FILE_DELETE_CHILD, and
//! DELETE, which renames the directory itself away) or take the directory over (WRITE_DAC,
//! WRITE_OWNER, GENERIC_ALL). Creating an entry, or changing the directory's attributes or
//! extended attributes, does neither, so FILE_ADD_FILE, FILE_ADD_SUBDIRECTORY, FILE_WRITE_EA,
//! FILE_WRITE_ATTRIBUTES and GENERIC_WRITE (which maps to those for a directory) are left
//! out. FILE_ADD_FILE matters only where a name can be planted: in the data file's own
//! directory, and in an executable's directory, which the loader searches for libraries.
//! Both of those are checked with CHAIN_DIR_WRITEISH.
constexpr uint32_t ANCESTOR_DIR_WRITEISH = FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_ALL;

//! Anything that reads what a file holds. For a secret, a check with these rights is
//! paired with FindForeignWriter(FILE_WRITEISH): a trustee holding WRITE_DAC,
//! WRITE_OWNER or GENERIC_WRITE can grant itself read, so READISH alone is not enough.
constexpr uint32_t READISH = FILE_READ_DATA | GENERIC_READ | GENERIC_ALL;
} // namespace mask

//! How an entry is inherited. ContainerAndObject is OBJECT_INHERIT_ACE together
//! with CONTAINER_INHERIT_ACE.
enum class Inherit : uint8_t
{
    None,
    ContainerAndObject,
};

//! One allow entry to apply.
struct AceSpec
{
    Sid trustee;
    uint32_t mask{0};
    Inherit inherit{Inherit::None};
};

//! An owner and DACL to apply. The DACL is always applied protected, and holds
//! allow entries only.
struct AclSpec
{
    Sid owner;
    std::vector<AceSpec> aces;
};

//! The kind of an entry as read. OtherAllow is every other allow type (callback,
//! object, compound, conditional); Other is anything that is neither an allow nor a
//! deny.
enum class AceKind : uint8_t
{
    Allow,
    Deny,
    OtherAllow,
    Other,
};

//! One entry as read.
struct ObservedAce
{
    AceKind kind{AceKind::Other};
    //! Empty when the entry carries no SID or one that is not well formed.
    Sid trustee;
    uint32_t mask{0};
    bool inherit_only{false};
    bool inherited{false};
    //! The entry's inheritance as an AceSpec states it, or nullopt for one no
    //! AceSpec states (object-only, container-only, or no-propagate).
    std::optional<Inherit> inherit{Inherit::None};
};

//! An owner and DACL as read.
struct ObservedAcl
{
    std::optional<Sid> owner;
    //! False for an absent or NULL DACL, which gives everyone full access.
    bool dacl_present{false};
    bool is_protected{false};
    std::vector<ObservedAce> aces;
};

//! Reason if: no DACL; owner not in \p allowed; or an Allow/OtherAllow ACE (not
//! inherit-only) gives any bit of \p writeish to a trustee not in \p allowed.
//! nullopt when clean. An owner that could not be read is not an allowed one.
std::optional<std::string> FindForeignWriter(const ObservedAcl& acl, const std::vector<Sid>& allowed,
                                             uint32_t writeish);

//! FindForeignWriter with mask::READISH. It does not see a trustee that can change the
//! ACL to grant itself read; for a secret, call FindForeignWriter with FILE_WRITEISH too.
std::optional<std::string> FindForeignReader(const ObservedAcl& acl, const std::vector<Sid>& allowed);

//! One path, and its owner and DACL as read.
struct PathAcl
{
    fs::path path;
    ObservedAcl acl;
};

//! Reason if a data file, or a directory above it, can be changed by a trustee not in
//! \p allowed. \p chain is the file first, then its directory, then each directory above
//! that up to the root of its volume. The file is checked with mask::FILE_WRITEISH, its
//! directory with mask::CHAIN_DIR_WRITEISH, as a name can be planted there, and every
//! directory above with mask::ANCESTOR_DIR_WRITEISH. A missing DACL, or an owner not in
//! \p allowed, is a reason at every level, as an owner can always change the DACL. The
//! reason names the path. nullopt when clean, and for an empty \p chain. Pure.
std::optional<std::string> FindForeignWriterOnDataChain(const std::vector<PathAcl>& chain,
                                                        const std::vector<Sid>& allowed);

//! Why \p leaf cannot be checked as a chain: it is not absolute (on this platform's terms), or
//! it has a "." or ".." component. nullopt when it can. Pure.
std::optional<std::string> ChainPathProblem(const fs::path& leaf);

//! Reason if: no DACL; owner not in \p allowed; or any Allow/OtherAllow ACE, an
//! inherit-only one included, names a trustee not in \p allowed, whatever it grants.
//! nullopt when clean. An inherit-only entry counts because on a directory it is
//! what every new entry receives.
std::optional<std::string> FindForeignTrustee(const ObservedAcl& acl, const std::vector<Sid>& allowed);

//! Reason unless: the DACL is present and protected, the owner is spec.owner, every
//! Allow/OtherAllow ACE equals one spec ACE (an explicit Allow entry for the same
//! trustee, mask and inheritance, not inherit-only), and none of spec's is missing.
//! Deny entries and entries of other kinds are ignored.
std::optional<std::string> CheckMatchesSpec(const ObservedAcl& acl, const AclSpec& spec);

//! The entry at \p bytes as Windows lays it out in a DACL, where \p size bytes may be
//! read: the header (type, flags, 16-bit size), then for the access, audit, alarm and
//! label types the mask, any object flags and GUIDs, and the SID. nullopt, with nothing
//! read past \p size, when the entry is shorter than its header, its size field is
//! below the header's size or above \p size, it ends before the object flags, GUIDs
//! or SID its type calls for, or its SID is not well formed. A type with no known
//! layout is Other, with no trustee and no mask.
std::optional<ObservedAce> ParseAce(const unsigned char* bytes, size_t size);

//! The kind of account a name maps to, as LookupAccountNameW's SID_NAME_USE gives it.
enum class AccountType : uint8_t
{
    User,
    Group,
    Domain,
    Alias,
    WellKnownGroup,
    DeletedAccount,
    Invalid,
    Unknown,
    Computer,
    Label,
    LogonSession,
    Other,
};

//! What a name maps to: the SID, the domain the account was found in (the
//! ReferencedDomainName, such as the computer's name for a local account, "BUILTIN"
//! or "NT SERVICE"), and the kind of account.
struct AccountLookup
{
    Sid sid;
    std::string domain;
    AccountType type{AccountType::Other};
};

//! ERROR_UNTRUSTED_MOUNT_POINT, which an open gets where Windows will not follow a junction
//! on the path: current Windows 11 builds do not follow one that an account that is not an
//! administrator made. win_acl.cpp checks the value against the SDK where it defines it.
constexpr uint32_t WIN32_UNTRUSTED_MOUNT_POINT = 448;

//! Why a path refused with WIN32_UNTRUSTED_MOUNT_POINT cannot be used, and what to do, for a
//! message that names the path. ASCII.
inline constexpr const char* UNTRUSTED_MOUNT_POINT_REASON =
    "Windows does not follow a junction on this path, because an account that is not an administrator made it "
    "(an untrusted mount point). Use the path of the directory the junction leads to, or have an administrator "
    "make the junction";

//! A Win32 handle, closed by \p Close when its guard goes out of scope or is reset.
//! Null and INVALID_HANDLE_VALUE (all bits set) both mean no handle, so what a failed
//! CreateFileW or CreateEventW returns can be taken as it is, and get() then gives
//! null. Move-only. The closer is a parameter so that these rules are tested on
//! every platform; Windows code uses UniqueHandle.
template <typename Close>
class BasicUniqueHandle
{
public:
    BasicUniqueHandle() = default;
    explicit BasicUniqueHandle(void* handle) noexcept : m_handle(Holds(handle) ? handle : nullptr) {}
    ~BasicUniqueHandle() { reset(); }

    BasicUniqueHandle(BasicUniqueHandle&& other) noexcept : m_handle(other.release()) {}
    BasicUniqueHandle& operator=(BasicUniqueHandle&& other) noexcept
    {
        if (this != &other) reset(other.release());
        return *this;
    }
    BasicUniqueHandle(const BasicUniqueHandle&) = delete;
    BasicUniqueHandle& operator=(const BasicUniqueHandle&) = delete;

    void* get() const noexcept { return m_handle; }
    explicit operator bool() const noexcept { return m_handle != nullptr; }

    //! Give the handle up without closing it.
    void* release() noexcept { return std::exchange(m_handle, nullptr); }

    //! Close the handle held, if any, and hold \p handle instead.
    void reset(void* handle = nullptr) noexcept
    {
        void* const old = std::exchange(m_handle, Holds(handle) ? handle : nullptr);
        if (old != nullptr) Close{}(old);
    }

private:
    //! Whether \p handle is a handle: neither null nor INVALID_HANDLE_VALUE.
    static bool Holds(void* handle) noexcept
    {
        return handle != nullptr && reinterpret_cast<std::intptr_t>(handle) != -1;
    }

    void* m_handle{nullptr};
};

#ifdef WIN32
//! Closes a Win32 handle (CloseHandle).
struct HandleCloser
{
    void operator()(void* handle) const noexcept;
};

//! The Win32 handle guard.
using UniqueHandle = BasicUniqueHandle<HandleCloser>;

//! A copy of the SID \p psid (a PSID) points to, or nullopt when it is null or not a
//! well-formed SID with one to fifteen sub-authorities.
std::optional<Sid> SidAt(void* psid);

//! What GetTokenInformation writes for \p information_class (a
//! TOKEN_INFORMATION_CLASS) of the access token \p token, in a buffer sized to fit
//! and aligned for the structure it holds. Throws std::system_error naming \p name.
std::vector<unsigned char> QueryTokenInformation(void* token, int information_class, const char* name);

//! The SID of this process's token user. Throws std::system_error.
Sid CurrentTokenUser();

//! The owner this process's token gives what it creates (TokenOwner): its user, or for an
//! elevated administrator by default Administrators. Throws std::system_error.
Sid CurrentTokenOwner();

//! Whether this process's token is elevated (TokenElevation). Throws std::system_error.
bool CurrentTokenIsElevated();

//! The integrity level of this process's token, a SECURITY_MANDATORY_*_RID: the last
//! sub-authority of its mandatory label (TokenIntegrityLevel). Throws
//! std::system_error.
uint32_t CurrentTokenIntegrityRid();

//! The groups of this process's token (ReadTokenGroups). Throws std::system_error.
std::vector<TokenGroup> CurrentTokenGroups();

//! The user of the access token \p token, opened for TOKEN_QUERY (TokenUser). Throws
//! std::system_error.
Sid ReadTokenUser(void* token);

//! The default owner of the access token \p token, opened for TOKEN_QUERY (TokenOwner).
//! Throws std::system_error.
Sid ReadTokenOwner(void* token);

//! The groups of the access token \p token, opened for TOKEN_QUERY, in the token's order
//! (TokenGroups). Throws std::system_error.
std::vector<TokenGroup> ReadTokenGroups(void* token);

//! Whether the access token \p token, opened for TOKEN_QUERY, is elevated
//! (TokenElevation). Throws std::system_error.
bool ReadTokenElevated(void* token);

//! The integrity level of the access token \p token, opened for TOKEN_QUERY: the last
//! sub-authority of its mandatory label (TokenIntegrityLevel). Throws std::system_error.
uint32_t ReadTokenIntegrityRid(void* token);

//! The owner and DACL of \p path (GetNamedSecurityInfoW). Throws std::system_error.
ObservedAcl ReadAcl(const fs::path& path);

//! The owner and DACL of the object \p handle refers to (GetSecurityInfo), read as a
//! kernel object when \p kernel_object and as a file otherwise. Throws
//! std::system_error.
ObservedAcl ReadAcl(void* handle, bool kernel_object);

//! ApplyAcl's error when the ACL read back is not the one applied, as on a volume
//! that accepts the call and stores nothing. reason() is CheckMatchesSpec's text.
class AclNotKeptError : public std::runtime_error
{
public:
    AclNotKeptError(const fs::path& path, std::string reason);

    const std::string& reason() const { return m_reason; }

private:
    std::string m_reason;
};

//! Make \p spec the owner and protected DACL of \p path (SetNamedSecurityInfoW), then
//! read it back. The owner is written only when it differs, so WRITE_OWNER is needed
//! only on an object whose owner has to change. Throws std::system_error when a call
//! fails and AclNotKeptError when what is read back does not match \p spec.
void ApplyAcl(const fs::path& path, const AclSpec& spec);

//! Make the DACL of the file \p to, a handle opened with WRITE_DAC and READ_CONTROL, the DACL of
//! the file \p from, a handle opened with READ_CONTROL: every entry as it is, a NULL DACL as a
//! NULL DACL, and protected exactly when \p from is (SetSecurityInfo with PROTECTED_ or
//! UNPROTECTED_DACL_SECURITY_INFORMATION). The owner of \p to is not touched, so no privilege is
//! needed. \p to is then read back and its protection and explicit entries compared with \p from's;
//! the entries a parent hands down (inherited ones) are not compared, as Windows may take them
//! from the parent again. Throws std::system_error when a call fails and AclNotKeptError when
//! what is read back is not what was set.
void CopyDacl(void* from, void* to);

//! \p aces as a DACL of allow entries, in order. The caller owns the result and must
//! LocalFree() it. Throws std::system_error, or std::runtime_error for an entry whose
//! trustee is not a well-formed SID.
void* BuildDacl(const std::vector<AceSpec>& aces);

//! The mandatory label a new object is created with. Default gives it none. MediumNoReadUp gives
//! it an explicit Medium label with NO_READ_UP and NO_WRITE_UP. A creator may set a label up to its
//! own integrity level, so this needs no privilege at Medium or above.
enum class MandatoryLabel : uint8_t
{
    Default,
    MediumNoReadUp,
};

//! \p spec as a self-relative security descriptor with a protected DACL, and \p label in its
//! SACL unless it is Default, for SECURITY_ATTRIBUTES. Free it with FreeSecurityDescriptor.
//! Throws as BuildDacl does.
void* MakeSecurityDescriptor(const AclSpec& spec, MandatoryLabel label = MandatoryLabel::Default);

//! Whether the object \p handle, opened with READ_CONTROL, carries the label
//! MandatoryLabel::MediumNoReadUp gives: Medium integrity with NO_READ_UP. A volume that keeps no
//! label, or an object made without one, does not. Throws std::system_error when it cannot be read.
bool HasMediumNoReadUpLabel(void* handle);

//! Create the directory \p dir with \p spec as its owner and protected DACL from the
//! start (CreateDirectoryW with MakeSecurityDescriptor(spec)), so it is never there
//! with another list, then read the list back through a handle on the directory
//! itself (no reparse point followed) and compare it with CheckMatchesSpec. Returns
//! false, changing nothing, when \p dir already exists: when CreateDirectoryW says
//! so, or when it fails otherwise (such as ERROR_ACCESS_DENIED from a parent that
//! does not grant FILE_ADD_SUBDIRECTORY) and \p dir, opened as itself, is a
//! directory. Throws std::system_error when a call fails, and AclNotKeptError when
//! what is read back does not match; on either, after the directory was made, it is
//! removed again.
bool CreateDirectoryWithAcl(const fs::path& dir, const AclSpec& spec);

//! Free a descriptor from MakeSecurityDescriptor. A null \p sd is ignored.
void FreeSecurityDescriptor(void* sd);

//! Frees a descriptor from MakeSecurityDescriptor (FreeSecurityDescriptor).
struct SecurityDescriptorFreer
{
    void operator()(void* sd) const noexcept { FreeSecurityDescriptor(sd); }
};

//! A descriptor from MakeSecurityDescriptor, freed when it goes out of scope.
using UniqueSecurityDescriptor = std::unique_ptr<void, SecurityDescriptorFreer>;

//! The name Windows gives the account \p sid (LookupAccountSidW), as "DOMAIN\name" in UTF-8,
//! or empty when it gives none.
std::string AccountNameOf(const Sid& sid);

//! What \p name maps to (LookupAccountNameW): its SID, the domain it was found in,
//! and its kind. nullopt when it maps to none. Throws std::system_error on any other
//! failure, and std::range_error when the domain is not valid UTF-16.
std::optional<AccountLookup> LookupAccount(const std::wstring& name);

//! The SID \p name maps to (LookupAccount). nullopt when it maps to none, or
//! when \p require_alias and it is not an alias (a local group such as BUILTIN\Users).
//! Throws std::system_error on any other failure.
std::optional<Sid> LookupAccountSidByName(const std::wstring& name, bool require_alias);

//! Reason if \p leaf, or a directory above it, can be changed by a trustee not in
//! \p allowed: the leaf is checked with FILE_WRITEISH and every ancestor up to the
//! root with CHAIN_DIR_WRITEISH. A reason too when \p leaf is not absolute or has a
//! "." or ".." component (ChainPathProblem). Throws std::system_error when an ACL cannot be
//! read.
//!
//! The walk is lexical: the directories checked are the ones \p leaf names. Each access list
//! is read by name (GetNamedSecurityInfoW), which follows a junction or a symbolic link on the
//! path, so a level that is one is checked as what it leads to, and the directories above that
//! target are not checked.
std::optional<std::string> FindForeignWriterOnChain(const fs::path& leaf, const std::vector<Sid>& allowed);

//! The pure FindForeignWriterOnDataChain for the data file \p leaf, with the owner and DACL of
//! every directory above it read by name (ReadAcl), and of the file itself as \p leaf_acl
//! gives it, read through a handle the caller holds on it, or by name when \p leaf_acl is
//! null. A reason too when \p leaf is not absolute or has a "." or ".." component
//! (ChainPathProblem). Throws std::system_error when an ACL cannot be read.
//!
//! The walk is lexical, as FindForeignWriterOnChain's is: a junction or a symbolic link among
//! the directories is checked as what it leads to, and the directories above that target are
//! not checked.
std::optional<std::string> FindForeignWriterOnDataChain(const fs::path& leaf, const std::vector<Sid>& allowed,
                                                        const ObservedAcl* leaf_acl = nullptr);
#endif // WIN32

} // namespace util::winacl

#endif // GRIDCOIN_UTIL_WIN_ACL_H
