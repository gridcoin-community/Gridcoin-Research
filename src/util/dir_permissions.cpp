// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/dir_permissions.h"

#include <cerrno>
#include <string>
#include <system_error>

#ifdef WIN32
#include <windows.h>
#include <aclapi.h>
#include <vector>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace util {
namespace {

#ifdef WIN32

//! The two trustees an IPC object's DACL may name: this process's token user
//! ("me") and SYSTEM. SYSTEM is included because excluding it breaks backup,
//! indexing and service scenarios while adding nothing -- SYSTEM can take
//! ownership of any object regardless of its DACL, so denying it is theatre.
//! Administrators are deliberately *not* included: an administrator can likewise
//! take ownership at will, but leaving the group off keeps the ACL an accurate
//! statement of intent (owner-only) rather than an invitation.
struct OwnerOnlyTrustees
{
    std::vector<unsigned char> token_user; //!< TOKEN_USER header + the SID it points into
    unsigned char system_sid[SECURITY_MAX_SID_SIZE];

    PSID user() const
    {
        return reinterpret_cast<const TOKEN_USER*>(token_user.data())->User.Sid;
    }
    PSID system() const { return const_cast<unsigned char*>(system_sid); }
};

//! Resolve the process token user SID and the SYSTEM SID. Throws on failure --
//! without both SIDs no DACL can be built, and silently skipping the DACL is the
//! very hole this exists to close.
OwnerOnlyTrustees GetOwnerOnlyTrustees()
{
    OwnerOnlyTrustees t{};

    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "OpenProcessToken");
    }
    // First call sizes the buffer (it fails with ERROR_INSUFFICIENT_BUFFER by design).
    DWORD len = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &len);
    t.token_user.resize(len ? len : 1);
    if (!::GetTokenInformation(token, TokenUser, t.token_user.data(), len, &len)) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(token);
        throw std::system_error(static_cast<int>(err), std::system_category(),
                                "GetTokenInformation(TokenUser)");
    }
    ::CloseHandle(token);

    DWORD system_sid_size = sizeof(t.system_sid);
    if (!::CreateWellKnownSid(WinLocalSystemSid, nullptr, t.system_sid, &system_sid_size)) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "CreateWellKnownSid(WinLocalSystemSid)");
    }
    return t;
}

//! Build the two-ACE allow-only DACL for \p trustees. The caller owns the
//! returned ACL and must LocalFree() it.
PACL BuildOwnerOnlyDacl(const OwnerOnlyTrustees& trustees, bool inheritable)
{
    EXPLICIT_ACCESS_W ea[2] = {};
    const PSID sids[2] = {trustees.user(), trustees.system()};
    // On a directory the ACEs must be inheritable, so anything created inside
    // (node.sock, ipc.cookie) is owner-only from creation rather than only after
    // an after-the-fact SetNamedSecurityInfo -- that closes the create/protect
    // window rather than merely narrowing it.
    const DWORD inheritance = inheritable ? (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE)
                                          : NO_INHERITANCE;
    for (int i = 0; i < 2; ++i) {
        ea[i].grfAccessPermissions = GENERIC_ALL;
        ea[i].grfAccessMode = SET_ACCESS;
        ea[i].grfInheritance = inheritance;
        ea[i].Trustee.pMultipleTrustee = nullptr;
        ea[i].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
        ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[i].Trustee.TrusteeType = i == 0 ? TRUSTEE_IS_USER : TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[i].Trustee.ptstrName = reinterpret_cast<LPWSTR>(sids[i]);
    }

    PACL dacl = nullptr;
    const DWORD rc = ::SetEntriesInAclW(2, ea, nullptr, &dacl);
    if (rc != ERROR_SUCCESS || dacl == nullptr) {
        throw std::system_error(static_cast<int>(rc), std::system_category(), "SetEntriesInAcl");
    }
    return dacl;
}

//! Read \p path's DACL back and confirm it says what we just asked for: present,
//! non-NULL, protected (no inherited ACEs), and every ACE an allow-ACE naming
//! only one of \p trustees. This is the "verify" half of fail-closed: some
//! filesystems (FAT/exFAT volumes, some network redirectors) accept a
//! SetNamedSecurityInfo call and store nothing, which would otherwise leave the
//! cookie world-readable while every call reported success.
void VerifyOwnerOnlyDaclImpl(const fs::path& path, const OwnerOnlyTrustees& trustees)
{
    std::wstring wpath = path.wstring();
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    // DACL_SECURITY_INFORMATION only: PROTECTED_DACL_SECURITY_INFORMATION is a
    // set-side flag and is rejected here. Whether the DACL is protected is read
    // below from the descriptor's SE_DACL_PROTECTED control bit instead.
    const DWORD rc = ::GetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                             nullptr, nullptr, &dacl, nullptr, &sd);
    if (rc != ERROR_SUCCESS) {
        throw std::system_error(static_cast<int>(rc), std::system_category(),
                                "GetNamedSecurityInfo " + path.string());
    }

    std::string failure;
    SECURITY_DESCRIPTOR_CONTROL control{};
    DWORD revision = 0;
    if (!::GetSecurityDescriptorControl(sd, &control, &revision)) {
        failure = "the security descriptor could not be read back";
    } else if ((control & SE_DACL_PRESENT) == 0 || dacl == nullptr) {
        // A NULL/absent DACL grants everyone full access -- the worst outcome.
        failure = "it has no DACL (all users would have access)";
    } else if ((control & SE_DACL_PROTECTED) == 0) {
        failure = "its DACL is not protected (it still inherits ACEs from the parent)";
    } else {
        for (WORD i = 0; i < dacl->AceCount; ++i) {
            void* ace = nullptr;
            if (!::GetAce(dacl, i, &ace)) {
                failure = "one of its ACEs could not be read back";
                break;
            }
            const ACE_HEADER* header = static_cast<const ACE_HEADER*>(ace);
            if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) continue; // a deny ACE only narrows
            PSID sid = &static_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart;
            if (!::EqualSid(sid, trustees.user()) && !::EqualSid(sid, trustees.system())) {
                failure = "its DACL grants access to a trustee other than the current user and SYSTEM";
                break;
            }
        }
    }
    if (sd != nullptr) ::LocalFree(sd);
    if (!failure.empty()) {
        throw std::runtime_error(strprintf(
            "Refusing to serve IPC: '%s' could not be restricted to the current user -- %s. "
            "The data directory must live on an NTFS volume that supports access control.",
            path.string(), failure));
    }
}

#endif // WIN32

} // namespace

#ifdef WIN32
void ApplyOwnerOnlyDacl(const fs::path& path, bool inheritable)
{
    const OwnerOnlyTrustees trustees = GetOwnerOnlyTrustees();
    PACL dacl = BuildOwnerOnlyDacl(trustees, inheritable);

    // SetNamedSecurityInfoW takes a mutable LPWSTR, hence the local buffer.
    std::wstring wpath = path.wstring();
    const DWORD rc = ::SetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT,
                                             DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                             nullptr, nullptr, dacl, nullptr);
    ::LocalFree(dacl);
    if (rc != ERROR_SUCCESS) {
        throw std::system_error(static_cast<int>(rc), std::system_category(),
                                "SetNamedSecurityInfo " + path.string());
    }
    // Setting an ACL can succeed nominally on a volume that does not store one;
    // read it back before we act as though the secret is protected.
    VerifyOwnerOnlyDaclImpl(path, trustees);
}

void* MakeOwnerOnlyDacl(bool inheritable)
{
    return BuildOwnerOnlyDacl(GetOwnerOnlyTrustees(), inheritable);
}


void VerifyOwnerOnlyDacl(const fs::path& path)
{
    VerifyOwnerOnlyDaclImpl(path, GetOwnerOnlyTrustees());
}
#endif // WIN32

bool CreateOwnerOnlyDirectory(const fs::path& path, std::string& error)
{
    error.clear();

    boost::system::error_code ec;
    const bool created = fs::create_directories(path, ec);

    if (ec && !fs::is_directory(path, ec)) {
        error = "could not create the directory " + path.string() + ": " + ec.message();
        return false;
    }

    // Already there: leave it exactly as the operator configured it. Re-tightening
    // a directory somebody widened on purpose is not ours to do, and the secrets
    // that live inside (the IPC socket and cookie) carry their own owner-only
    // protection regardless of how permissive the parent is.
    if (!created) return true;

    // We made it, so we set the terms.
#ifdef WIN32
    try {
        // Inheritable: entries created inside are owner-only from creation rather
        // than after the fact.
        ApplyOwnerOnlyDacl(path, /*inheritable=*/true);
    } catch (const std::exception& e) {
        error = "created " + path.string() + " but could not restrict it to this account: " + e.what();
        return false;
    }
#else
    if (::chmod(path.string().c_str(), 0700) != 0) {
        error = "created " + path.string() + " but could not chmod it to 0700 (errno "
              + std::to_string(errno) + ")";
        return false;
    }
#endif

    return true;
}

} // namespace util
