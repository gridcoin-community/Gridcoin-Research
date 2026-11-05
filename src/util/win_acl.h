// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_WIN_ACL_H
#define GRIDCOIN_UTIL_WIN_ACL_H

#include "fs.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

#ifdef WIN32
//! The SID of this process's token user. Throws std::system_error.
Sid CurrentTokenUser();

//! Whether this process's token is elevated (TokenElevation). Throws std::system_error.
bool CurrentTokenIsElevated();

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

//! \p aces as a DACL of allow entries, in order. The caller owns the result and must
//! LocalFree() it. Throws std::system_error, or std::runtime_error for an entry whose
//! trustee is not a well-formed SID.
void* BuildDacl(const std::vector<AceSpec>& aces);

//! \p spec as a self-relative security descriptor with a protected DACL, for
//! SECURITY_ATTRIBUTES. Free it with FreeSecurityDescriptor. Throws as BuildDacl does.
void* MakeSecurityDescriptor(const AclSpec& spec);

//! Free a descriptor from MakeSecurityDescriptor. A null \p sd is ignored.
void FreeSecurityDescriptor(void* sd);

//! The SID \p name maps to (LookupAccountNameW). nullopt when it maps to none, or
//! when \p require_alias and it is not an alias (a local group such as BUILTIN\Users).
//! Throws std::system_error on any other failure.
std::optional<Sid> LookupAccountSidByName(const std::wstring& name, bool require_alias);

//! Reason if \p leaf, or a directory above it, can be changed by a trustee not in
//! \p allowed: the leaf is checked with FILE_WRITEISH and every ancestor up to the
//! root with CHAIN_DIR_WRITEISH. A reason too when \p leaf is not absolute or has a
//! "." or ".." component. Throws std::system_error when an ACL cannot be read.
std::optional<std::string> FindForeignWriterOnChain(const fs::path& leaf, const std::vector<Sid>& allowed);
#endif // WIN32

} // namespace util::winacl

#endif // GRIDCOIN_UTIL_WIN_ACL_H
