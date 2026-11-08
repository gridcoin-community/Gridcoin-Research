// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/dir_permissions.h"

#include "tinyformat.h" // strprintf, for the errors when a directory cannot be restricted
#include "util/win_acl.h"
#include "util/syserror.h"

#include <atomic>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef WIN32
#include <windows.h>
#include <vector>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace util {

#ifdef WIN32
//! The owner-only access control list: this process's token user owns the object,
//! and only it and SYSTEM have access. SYSTEM is included because excluding it
//! breaks backup, indexing and service scenarios while adding nothing -- SYSTEM can
//! take ownership of any object regardless of its DACL, so denying it is theatre.
//! Administrators are deliberately *not* included: an administrator can likewise
//! take ownership at will, but leaving the group off keeps the ACL an accurate
//! statement of intent (owner-only) rather than an invitation.
//!
//! The owner is stated rather than left to the creating token, whose default owner
//! in an elevated process is the Administrators group.
winacl::AclSpec OwnerOnlyAclSpec(bool inheritable)
{
    const winacl::Sid user = winacl::CurrentTokenUser();
    const winacl::Sid system = winacl::wellknown::System();

    // On a directory the entries must be inheritable, so anything created inside
    // is owner-only from creation rather than only after an after-the-fact
    // SetNamedSecurityInfo -- that closes the create/protect window rather than
    // merely narrowing it.
    const winacl::Inherit inherit = inheritable ? winacl::Inherit::ContainerAndObject : winacl::Inherit::None;

    // FILE_ALL_ACCESS here is the SDK's macro, equal to winacl::mask::FILE_ALL_ACCESS.
    // It is used rather than GENERIC_ALL because specific rights are stored as
    // written, so the read-back can compare the entries exactly.
    winacl::AclSpec spec{user, {{user, FILE_ALL_ACCESS, inherit}}};

    // A process running as SYSTEM names it once.
    if (system != user) spec.aces.push_back({system, FILE_ALL_ACCESS, inherit});

    return spec;
}

std::string OwnerOnlyRefusalText(const fs::path& path, const std::string& failure)
{
    return strprintf("Refusing to serve IPC: '%s' could not be restricted to the current user -- %s. "
                     "The data directory must live on an NTFS volume that supports access control.",
                     path.string(), failure);
}

void ApplyOwnerOnlyDacl(const fs::path& path, bool inheritable)
{
    // ApplyAcl reads the ACL back before returning. Some filesystems (FAT/exFAT
    // volumes, some network redirectors) accept a SetNamedSecurityInfo call and store
    // nothing, which would otherwise leave the cookie world-readable while every call
    // reported success.
    try {
        winacl::ApplyAcl(path, OwnerOnlyAclSpec(inheritable));
    } catch (const winacl::AclNotKeptError& e) {
        throw std::runtime_error(OwnerOnlyRefusalText(path, e.reason()));
    }
}

void VerifyOwnerOnlyDacl(const fs::path& path)
{
    const winacl::ObservedAcl acl = winacl::ReadAcl(path);
    const std::vector<winacl::Sid> allowed{winacl::CurrentTokenUser(), winacl::wellknown::System()};

    // Present and protected, owned by this account or SYSTEM, and naming nobody else
    // in an allow entry of any kind, inherit-only entries included.
    std::optional<std::string> failure;
    if (acl.dacl_present && !acl.is_protected) {
        failure = "its DACL is not protected (it still inherits ACEs from the parent)";
    } else {
        failure = winacl::FindForeignTrustee(acl, allowed);
    }

    if (failure) throw std::runtime_error(OwnerOnlyRefusalText(path, *failure));
}
#endif // WIN32

namespace {
//! The umask SetOwnerOnlyUmask() replaced, or -1.
std::atomic<int> g_inherited_umask{-1};
} // namespace

void SetOwnerOnlyUmask()
{
#ifndef WIN32
    const mode_t replaced = ::umask(077);
    // A second call replaces the owner-only mask itself; the first answer stands.
    if (replaced != 077) g_inherited_umask.store(static_cast<int>(replaced));
#endif
}

std::optional<unsigned int> InheritedUmask()
{
    const int mask = g_inherited_umask.load();
    if (mask < 0) return std::nullopt;
    return static_cast<unsigned int>(mask);
}

#ifndef WIN32
bool RestrictErrorMeansNoModes(int err)
{
    // EOPNOTSUPP and ENOTSUP are the same value on Linux and differ elsewhere.
    return err == EPERM || err == ENOTSUP || err == EOPNOTSUPP || err == EROFS;
}
#endif

namespace {
#ifdef WIN32
//! Whether the volume holding \p path keeps ACLs (FILE_PERSISTENT_ACLS). A volume
//! that cannot be asked is taken to keep them, so a failure there is not tolerated.
bool VolumeKeepsAcls(const fs::path& path)
{
    wchar_t root[MAX_PATH + 1];
    if (!GetVolumePathNameW(path.wstring().c_str(), root, MAX_PATH + 1)) return true;

    DWORD flags = 0;
    if (!GetVolumeInformationW(root, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0)) return true;

    return (flags & FILE_PERSISTENT_ACLS) != 0;
}
#endif

//! What CreateAndRestrict did.
enum class CreateResult
{
    //! The directory was there already, and is left as it is.
    Existing,
    //! This call created it and restricted it to this account.
    Restricted,
    //! This call created it and could not restrict it because its filesystem keeps
    //! no POSIX modes, or its volume no ACLs; error says why.
    NoModes,
    //! This call created it and could not restrict it for another reason; error says why.
    NotRestricted,
    //! The directory could not be created; error says why.
    Failed,
};

#ifdef WIN32
//! \p path without separators at its end, so that its parent is the directory above it.
//! A root such as "C:\" keeps its separator.
fs::path WithoutTrailingSeparators(const fs::path& path)
{
    std::wstring text = path.wstring();
    while (text.size() > 1 && (text.back() == L'\\' || text.back() == L'/') && text[text.size() - 2] != L':') {
        text.pop_back();
    }

    return fs::path(text);
}

//! Create \p path and, if this call created it, restrict it to this account.
CreateResult CreateAndRestrict(const fs::path& path, std::string& error)
{
    error.clear();

    // The parents get the lists they inherit. The directory itself is created with the
    // owner-only list, inheritable so that entries created inside are owner-only from
    // creation, and read back, so it is never there with another list.
    const fs::path dir = WithoutTrailingSeparators(path);
    const fs::path parent = dir.has_parent_path() ? dir.parent_path() : fs::path(".");

    boost::system::error_code ec;
    if (dir.has_parent_path()) fs::create_directories(parent, ec);
    if (ec) {
        error = "could not create the directory " + path.string() + ": " + ec.message();
        return CreateResult::Failed;
    }

    try {
        if (winacl::CreateDirectoryWithAcl(dir, OwnerOnlyAclSpec(/*inheritable=*/true))) return CreateResult::Restricted;
    } catch (const std::exception& e) {
        // CreateDirectoryWithAcl took away anything it made.
        const auto* const not_kept = dynamic_cast<const winacl::AclNotKeptError*>(&e);
        if (VolumeKeepsAcls(parent)) {
            if (!not_kept) {
                error = "could not create the directory " + path.string() + ": " + AsciiWhat(e);
                return CreateResult::Failed;
            }
            error = "created " + path.string() + " but could not restrict it to this account: " +
                    OwnerOnlyRefusalText(path, not_kept->reason());
            return CreateResult::NotRestricted;
        }

        // A volume that keeps no ACLs: the directory is made without one, for the caller
        // that keeps such a directory.
        boost::system::error_code plain_ec;
        if (!fs::create_directory(dir, plain_ec)) {
            error = "could not create the directory " + path.string() + ": " +
                    (plain_ec ? plain_ec.message() : std::string{AsciiWhat(e)});
            return CreateResult::Failed;
        }
        error = "created " + path.string() + " but could not restrict it to this account: " + AsciiWhat(e) +
                " (the volume keeps no ACLs)";
        return CreateResult::NoModes;
    }

    // Already there: leave it exactly as the operator configured it. Re-tightening a
    // directory somebody widened on purpose is not ours to do. Anything else in its
    // place is an error.
    boost::system::error_code stat_ec;
    if (!fs::is_directory(dir, stat_ec)) {
        error = "could not create the directory " + path.string() + ": something that is not a directory is there";
        return CreateResult::Failed;
    }

    return CreateResult::Existing;
}
#else
//! Create \p path and, if this call created it, restrict it to this account.
CreateResult CreateAndRestrict(const fs::path& path, std::string& error)
{
    error.clear();

    boost::system::error_code ec;
    const bool created = fs::create_directories(path, ec);

    // The status query gets its OWN error_code. Reusing ec would overwrite the
    // create_directories() failure that is about to be reported: when the query
    // itself succeeds, ec is cleared and the message reads "could not create ...:
    // Success", which hides the only actionable part of the diagnostic.
    if (ec) {
        boost::system::error_code stat_ec;
        if (!fs::is_directory(path, stat_ec)) {
            error = "could not create the directory " + path.string() + ": " + ec.message();
            return CreateResult::Failed;
        }
    }

    // Already there: leave it exactly as the operator configured it. Re-tightening
    // a directory somebody widened on purpose is not ours to do, and the secrets
    // that live inside (the IPC socket and cookie) carry their own owner-only
    // protection regardless of how permissive the parent is.
    if (!created) return CreateResult::Existing;

    // We made it, so we set the terms.
    if (::chmod(path.string().c_str(), 0700) != 0) {
        const int err = errno;
        error = strprintf("created %s but could not chmod it to 0700: %s (errno %d)", path.string(),
                          std::generic_category().message(err), err);
        return RestrictErrorMeansNoModes(err) ? CreateResult::NoModes : CreateResult::NotRestricted;
    }

    return CreateResult::Restricted;
}
#endif // WIN32

//! Remove \p path, which this call created and could not restrict, and return false.
bool Withdraw(const fs::path& path, std::string& error)
{
    // This call created the directory and could not restrict it, so it is withdrawn
    // again before returning false. Leaving it behind is what made this fail open:
    // on the NEXT start the directory exists, the existing-directory branch returns
    // true without inspecting it, and a directory we already know we failed to
    // protect is accepted silently and forever. One warning, then permanent exposure.
    //
    // fs::remove is deliberately the non-recursive form: it removes an empty
    // directory or nothing. This runs microseconds after create_directories()
    // created it, so the directory is empty -- but if that assumption is ever
    // wrong, remove() fails and reports rather than destroying anything. A data
    // directory is the last place to use remove_all().
    //
    // A pre-existing directory never reaches here, so the create-only rule is
    // intact: nothing the operator set up is inspected, re-tightened, or removed.
    boost::system::error_code rm_ec;
    fs::remove(path, rm_ec);
    if (rm_ec) {
        error += "; the directory could not be removed either (" + rm_ec.message()
               + "), so it is left in place UNRESTRICTED -- do not put a wallet in it";
    }
    return false;
}
} // namespace

bool CreateOwnerOnlyDirectory(const fs::path& path, std::string& error)
{
    switch (CreateAndRestrict(path, error)) {
    case CreateResult::Existing:
    case CreateResult::Restricted:
        return true;
    case CreateResult::Failed:
        return false;
    case CreateResult::NoModes:
    case CreateResult::NotRestricted:
        break;
    }
    return Withdraw(path, error);
}

bool CreateDirectoryRestrictedIfPossible(const fs::path& path, std::string& error,
                                         std::optional<std::string>& unrestricted)
{
    unrestricted.reset();

    switch (CreateAndRestrict(path, error)) {
    case CreateResult::Existing:
    case CreateResult::Restricted:
        return true;
    case CreateResult::Failed:
        return false;
    case CreateResult::NotRestricted:
        return Withdraw(path, error);
    case CreateResult::NoModes:
        break;
    }

    // A filesystem without modes: kept, and the reason handed back for the caller to
    // report.
    unrestricted = error;
    error.clear();
    return true;
}

} // namespace util
