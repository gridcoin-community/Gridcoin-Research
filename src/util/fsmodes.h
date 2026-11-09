// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_FSMODES_H
#define GRIDCOIN_UTIL_FSMODES_H

#include "fs.h"
#include "util/win_acl.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>

//! Access modes of the files a node keeps in its data directory.
//!
//! The core never changes the mode of a path that already exists, because the
//! operator may have widened it on purpose (a backup agent, a shared group). It
//! creates new backup copies and directories owner-only, and it reports existing
//! paths whose mode gives other accounts more access than NotOwnedModeAcceptable
//! allows, naming the chmod that would fix each one.
//!
//! On Windows the same rules apply to access lists, each read through a handle on
//! the path: an existing list is never changed; one that lets an account other than
//! this one, SYSTEM and Administrators read or change the path is reported
//! (ClassifyWinAcl), naming the icacls command that would fix it; and what the core
//! creates is owner-only from creation.
namespace fsmodes {

struct ModeFinding
{
    enum class Kind
    {
        //! An existing path owned by this account has a mode wider than the rule allows. On
        //! Windows: its access list lets another account read or change it, or it is on a
        //! volume that keeps no access lists.
        WideMode,
        //! The same for a path owned by another account. Reported only, as this account cannot fix it.
        WideModeNotOwned,
        //! The path is a symbolic link (on Windows, a reparse point that names another path, such
        //! as a symbolic link or a junction). It is reported and never followed.
        Symlink,
        //! Windows only: the path exists but could not be opened (UnopenedPathFinding), or was
        //! opened but its access list could not be read, so it was not checked; or it is a
        //! directory whose entries could not be listed (UnlistedDirectoryFinding). When this
        //! process's own account could not be read, nothing was checked, and the one finding
        //! names the data directory.
        Unreadable,
    };

    fs::path path;
    Kind kind;
    //! The permission bits (st_mode & 07777) of the path itself, 0 for a symbolic link. Always 0
    //! on Windows, where the message names the accounts and their rights instead.
    unsigned mode;
    //! One line ready for the log. The fixed text is ASCII; the path is shown as given.
    std::string message;
};

//! What the Windows check reports for \p path, which it could not open with the Win32 error
//! \p error: nothing for ERROR_FILE_NOT_FOUND and ERROR_PATH_NOT_FOUND, as a path that does
//! not exist is skipped; otherwise an Unreadable finding that names the path and why, which for
//! a junction Windows does not follow (util::winacl::WIN32_UNTRUSTED_MOUNT_POINT) also says what
//! to do, and for any other error gives \p error_text. Pure.
std::optional<ModeFinding> UnopenedPathFinding(const fs::path& path, uint32_t error, const std::string& error_text);

//! What the Windows check reports for the directory \p dir, which it opened but could not list,
//! or stopped listing, with the Win32 error \p error: nothing for ERROR_FILE_NOT_FOUND and
//! ERROR_PATH_NOT_FOUND, as a directory gone since it was opened is skipped; otherwise an
//! Unreadable finding that names the directory and gives \p error_text, as the files in it were
//! not checked. Pure.
std::optional<ModeFinding> UnlistedDirectoryFinding(const fs::path& dir, uint32_t error, const std::string& error_text);

//! Checks the data directory, the wallet file, database/ and database/log.*, the
//! configuration file, the settings file, the backup directory and the regular files
//! directly in it, and the debug log. An empty path, and a path that does not exist,
//! is skipped. Nothing is modified. The data directory, the backup directory, the configuration
//! file and debug.log may each be a symbolic link (moved to another disk): the link is followed
//! and the rule applies to its target. Every other path that is a symbolic link, including the
//! files inside the backup directory, is reported and not followed.
//!
//! On Windows a junction, a symbolic link and every other reparse point that names another
//! path count as a symbolic link here. A reparse point that does not (the placeholder of a
//! cloud file, a deduplicated file) is the file itself and is checked as one. A path that
//! exists but cannot be opened to read its access list, such as one through a junction Windows
//! does not follow, is reported (UnopenedPathFinding), and so is a directory whose entries cannot
//! be listed (UnlistedDirectoryFinding).
std::vector<ModeFinding> CheckOwnerOnlyModes(const fs::path& datadir, const fs::path& conf,
                                             const fs::path& settings, const fs::path& wallet_file,
                                             const fs::path& backupdir, const fs::path& debuglog);

//! True when the path is not a symbolic link and its owner is this process's effective user.
//! On Windows: the path is not a reparse point of any kind, and the owner read through a handle
//! on it is this process's token user.
bool OwnedByUs(const fs::path& path);

//! What makes a Windows path this account's or not, as OwnedByUs reads it.
struct WinOwnership
{
    enum class Kind
    {
        //! Its owner is this process's token user: OwnedByUs is true.
        Owned,
        //! It could not be opened (\p error says why), as when it does not exist.
        Unopened,
        //! It is a reparse point of any kind, which OwnedByUs refuses.
        Link,
        //! It is on a volume that keeps no owners or access lists (FAT, exFAT).
        NoAclVolume,
        //! Another account owns it.
        Foreign,
        //! Its owner could not be read (\p error says why).
        Unreadable,
    };

    Kind kind{Kind::Unreadable};
    //! For Foreign: the owner, and the name Windows gives it, empty when it gives none or when it
    //! was not looked up.
    util::winacl::Sid owner;
    std::string owner_name;
    std::string error;
};

//! Why the Windows path \p path is not this account's (\p ownership), as one clause for a log
//! line or a note: for a path another account owns, its owner by name and SID; that a link is
//! not followed; or that a volume without owners or access lists cannot show whose a file is.
//! \p self, named \p self_name (empty when it has no name), is this account. Empty for Owned.
//!
//! Without \p service it also gives the icacls command that makes \p self the owner, to run as
//! an administrator (elevated) or as this account where it may change the owner. With
//! \p service (the configuration file of the Windows service) it says instead that the file
//! belongs to the administrators by design, and never gives the service the file: the caller
//! names the fixes that keep it theirs. ASCII apart from the path and the names. Pure.
std::string WinNotOwnedText(const fs::path& path, const WinOwnership& ownership, const util::winacl::Sid& self,
                            const std::string& self_name, bool service);

#ifdef WIN32
//! What OwnedByUs reads for \p path, without following a reparse point: OwnedByUs(path) is
//! ReadWinOwnership(path).kind == Owned. It looks up no account name, which the message alone
//! needs: \p owner_name is left empty.
WinOwnership ReadWinOwnership(const fs::path& path);

//! WinNotOwnedText for \p path as it is now, with this process's token user as \p self and the
//! owner's name looked up (LookupAccountSidW).
std::string WinNotOwnedText(const fs::path& path, bool service);
#endif

//! What AppendToOwnedFile did.
enum class AppendStatus
{
    //! The text was written at the end of the file.
    Appended,
    //! Nothing was written: the path is a symbolic link, or not a regular file this account owns.
    NotOwned,
    //! The file could not be opened; nothing was written.
    OpenFailed,
    //! The file was opened but the write failed. What was written before the failure stays.
    WriteFailed,
};

struct AppendResult
{
    AppendStatus status;
    //! Why it failed, for OpenFailed and WriteFailed.
    std::string error;
};

//! Appends \p text to the existing file at \p path, which must be a regular file this account
//! owns, as OwnedByUs() says. The file is opened without following a symbolic link and checked on
//! the open descriptor before anything is written, so a file swapped for a link or for another
//! account's file after an earlier OwnedByUs() is not written. On Windows the file is opened
//! without following a reparse point, and one that is a reparse point of any kind is not written.
AppendResult AppendToOwnedFile(const fs::path& path, const std::string& text);

#ifndef WIN32
//! The rule for every checked path: no access for "other" and no write for the group.
bool NotOwnedModeAcceptable(unsigned mode);
#endif

//! What ClassifyWinAcl makes of a Windows access list.
enum class WinAclVerdict : uint8_t
{
    //! Only this account, SYSTEM and Administrators can read or change the path.
    Clean,
    //! This account owns the path, and another account can read or change it.
    Wide,
    //! The same for a path this account does not own. Reported only.
    WideNotOwned,
};

//! The Windows rule for every checked path. The allowed accounts are \p self, SYSTEM and
//! Administrators. Anything that lets another account read (mask::READISH) or change
//! (mask::FILE_WRITEISH, or FILE_DELETE_CHILD, which deletes a directory's entries) the path is
//! a finding: an allow entry for it, a missing DACL, and an owner that is not an allowed
//! account, since an owner can always change the access list. Deny and inherit-only entries
//! are not. A finding on a path \p self owns is Wide, on any other WideNotOwned. Pure.
WinAclVerdict ClassifyWinAcl(const util::winacl::ObservedAcl& acl, const util::winacl::Sid& self);

//! The startup warning for \p path, whose access list ClassifyWinAcl did not find Clean:
//! which accounts can read or change it, and the icacls commands that remove their access. For
//! a path \p self owns, one command makes it owner-only, as the core creates it. For any other
//! the commands give this account no entry; they make it the owner only when the owner is
//! outside the allowed accounts or could not be read. The text says who can run them: an
//! administrator (elevated), this account where it may take ownership or change the
//! permissions, and an owner that stays.
//! \p directory says whether the path is a directory, whose entries then inherit the fix. Pure.
std::string WinAclWarning(const fs::path& path, const util::winacl::ObservedAcl& acl,
                          const util::winacl::Sid& self, bool directory);

//! The startup warning for \p path on a volume that keeps no access lists (FAT, exFAT): every
//! account can read and change it, and the remedy is an NTFS volume, not icacls. Pure.
std::string WinNoAclVolumeWarning(const fs::path& path);

//! What the startup check makes of a Windows path from its attributes and, for a reparse point,
//! its reparse tag (FILE_ATTRIBUTE_TAG_INFO).
struct WinEntry
{
    bool directory{false};
    //! A reparse point of any kind. OwnedByUs and AppendToOwnedFile refuse every one.
    bool reparse{false};
    //! A reparse point that names another path (IsReparseTagNameSurrogate): a symbolic link, a
    //! junction or a mount point. The check reports it as a link. A reparse point that does not,
    //! such as the placeholder of a cloud file, is checked as the file itself.
    bool link{false};
};

//! \p attributes and \p reparse_tag as a WinEntry. The tag is read only when the attributes
//! mark a reparse point. Pure.
WinEntry ClassifyWinEntry(uint32_t attributes, uint32_t reparse_tag);

//! Who refused the owner-only access list of a new file whose creation with it failed with the
//! Win32 error \p error: "the volume" or "the network share", and the file is then created
//! without the list (CopyFileOwnerOnly, CheckBackupDir). nullopt when the failure is not such a
//! refusal, and nothing is created. A refusal is an owner, group, ACL or descriptor the volume
//! does not take, an operation it does not support, or a privilege not held; on a network share
//! (\p remote, the parent directory as OnNetworkShare reads it) also a refusal of access, which
//! a share may give for the same reason. Pure.
std::optional<std::string> AclRefusedBy(uint32_t error, bool remote);

//! Whether the directory a new file is created in is on a network share, AclRefusedBy's
//! \p remote. \p final_path is the directory's final path as a handle on it gives it
//! (GetFinalPathNameByHandleW, VOLUME_NAME_DOS), which for anything a network redirector
//! serves has the UNC form \\?\UNC\server\share\..., or nullopt when the directory could not be
//! opened or its final path could not be read. Then \p by_name decides, the check on the
//! directory's name (its volume's drive type); it is called only then. Reads nothing else.
bool OnNetworkShare(const std::optional<std::wstring>& final_path, const std::function<bool()>& by_name);

//! What the startup check of the backup directory found.
struct BackupDirCheck
{
    //! Why no file can be written there, or nullopt when one can.
    std::optional<std::string> not_writable;
    //! Why the files written there are not owner-only, or nullopt: the directory, created
    //! by the check, could not be restricted, or a file created there with mode 0600 has
    //! another mode (a filesystem without POSIX modes, such as FAT). Backups are still
    //! written there. On Windows the probe is created with the owner-only access list, and
    //! a volume that does not keep it (FAT, exFAT) or a network share that does not accept it
    //! gives the reason.
    std::optional<std::string> not_owner_only;
};

//! Creates the backup directory if absent, owner-only where the filesystem allows it, then
//! creates and removes a file in it.
BackupDirCheck CheckBackupDir(const fs::path& backupdir);

//! CheckBackupDir with the probe file named by \p probe_name, for tests.
BackupDirCheck CheckBackupDir(const fs::path& backupdir, const std::function<std::string()>& probe_name);

//! The startup warning for what CheckBackupDir found in \p backupdir, or nullopt when there
//! is nothing to say.
std::optional<std::string> BackupDirWarning(const fs::path& backupdir, const BackupDirCheck& check);

//! Copies a file, overwriting the target. A target that does not exist is created with mode
//! 0600 (POSIX), set through its descriptor whatever the umask, so the mode is right before any
//! data is written. An existing target keeps its mode. Returns why a target this call created is not owner-only (a filesystem without POSIX
//! modes gives it another mode), or nullopt. Throws fs::filesystem_error on failure.
//!
//! On Windows a target that does not exist is created with the owner-only access list (this
//! account and SYSTEM, protected) and read back through its handle before any data is written;
//! an existing target keeps its access list. A link at the new target's name is not followed:
//! the name counts as taken, and the existing-target path, which follows it, applies. The
//! reason returned is why the new target does not hold that list: a volume that keeps no access
//! lists, such as FAT, or a network share that refused the list, where the target is created
//! with the list it inherits. Error texts are ASCII.
std::optional<std::string> CopyFileOwnerOnly(const fs::path& source, const fs::path& target);

//! Creates the new file \p path for writing, exclusively and owner-only from creation, and
//! returns it open as a FILE* in mode "w", or nullptr with \p error saying why. A name that is
//! taken, by a link too, whether or not the link leads anywhere, is refused with \p taken set:
//! nothing there is opened, followed or truncated, and the refusal and the creation are one
//! step. \p not_owner_only says why a file created is not owner-only, or is nullopt.
//!
//! POSIX: open(O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC) with mode 0600. A filesystem
//! without POSIX modes, which gives the file a mode of its own, is \p not_owner_only.
//!
//! Windows: CREATE_NEW with the owner-only access list, a link at the name not followed, as
//! CopyFileOwnerOnly creates a new target, and in text mode. A volume that keeps no access
//! lists, or a network share that refuses the list, where the file is created with the list it
//! inherits, is \p not_owner_only. Error texts are ASCII.
//!
//! \p binary opens the file in binary mode on Windows, which a file of bytes needs (the mode
//! decides whether "\n" is written as "\r\n"); POSIX has no such difference.
std::FILE* CreateNewOwnerOnlyFile(const fs::path& path, bool& taken, std::optional<std::string>& not_owner_only,
                                  std::string& error, bool binary = false);

//! What KeepPermissions did.
struct KeepResult
{
    enum class Kind
    {
        //! Nothing exists at the old name, so there is nothing to carry. The new file stays as it was
        //! created, owner-only.
        NoOldFile,
        //! The new file now has the permissions of the old one.
        Kept,
        //! The old file exists but its permissions could not be read or put on the new file (\p error
        //! says why). The new file stays as it was created, owner-only.
        NotKept,
        //! Asked for this account's files only: the old file is another account's, whose permissions
        //! say nothing of what this account wants. The new file stays as it was created, owner-only.
        NotOurs,
    };

    Kind kind{Kind::NoOldFile};
    //! Why, for NotKept. ASCII apart from a path.
    std::string error;
};

//! Carries the permissions of the file at \p existing onto \p temp, the file that is about to
//! replace it (a temporary file renamed over it, or a file recreated at its name after a
//! rotation). The file the core rewrites keeps the permissions it had, because the operator may
//! have widened them on purpose; the core warns where it already warns about a wide file
//! (CheckOwnerOnlyModes) and never changes the file that is being replaced. Call it before the
//! rename. Neither file is created or removed.
//!
//! POSIX: the permission bits (st_mode & 0777, never set-user-ID, set-group-ID or sticky) of the
//! name itself (lstat; a link has none to carry, so a link at \p existing is NoOldFile) and, where this process may set it, the group,
//! set through the descriptor (fchown, then fchmod). A group that cannot be set (EPERM) stays as it
//! is, and then nothing is carried (NotKept) when the old file gives its group any permission, so
//! that the old group's permissions do not reach another group. The path of the new file is never
//! changed.
//!
//! Windows: the access list (DACL) and whether it is protected, read through a handle on
//! \p existing opened with READ_CONTROL and without following a reparse point, and set through
//! the handle of \p temp (SetSecurityInfo with PROTECTED_ or UNPROTECTED_DACL_SECURITY_
//! INFORMATION), then read back. The owner stays the account that wrote \p temp: no privilege
//! is used to set another one. \p temp must have been opened with WRITE_DAC.
//!
//! \p temp is a stream the caller holds open (the descriptor or handle under a FILE*), or a path
//! that is opened and closed here without following a link. The second form is for a file that
//! a library has created and closed (the Berkeley DB rewrite) or that a stream class holds.
KeepResult KeepPermissions(const fs::path& existing, std::FILE* temp);
KeepResult KeepPermissions(const fs::path& existing, const fs::path& temp);

#ifndef WIN32
//! The same on an open file descriptor, for a caller that holds one and no FILE*. With \p own_only,
//! the permissions of an old file this process's effective user does not own are not carried
//! (NotOurs), as compared on the same lstat that reads them.
KeepResult KeepPermissions(const fs::path& existing, int fd, bool own_only = false);
#else
//! The same on a handle opened with WRITE_DAC. With \p own_only, the access list of an old file whose
//! owner, read through the same handle as its list, is not this process's token user is not
//! carried (NotOurs).
KeepResult KeepPermissions(const fs::path& existing, void* handle, bool own_only = false);
#endif

//! Creates \p temp, a file that will replace \p target, and returns it open for writing, or
//! nullptr with \p error saying why. \p temp is removed first if it is there (a link there is
//! removed, never followed; a directory there is left, and the creation then fails), then created exclusively and owner-only (CreateNewOwnerOnlyFile),
//! then given the permissions \p target has, if it exists (KeepPermissions). \p warning holds
//! what the caller should log when the file is not what it should be: that it is not owner-only
//! on a filesystem that keeps no modes, or that the permissions of \p target could not be kept.
//! The caller writes the file, closes it and renames it over \p target.
//! \p binary is CreateNewOwnerOnlyFile's. With \p keep_now false the permissions are not carried
//! over here: the caller does it with KeepPermissions(target, temp) once the file has been written,
//! for a writer that reopens the file by path and would be refused by a read-only old mode.
std::FILE* CreateReplacementFile(const fs::path& target, const fs::path& temp, std::string& error,
                                 std::optional<std::string>& warning, bool binary = false, bool keep_now = true);

} // namespace fsmodes

#endif // GRIDCOIN_UTIL_FSMODES_H
