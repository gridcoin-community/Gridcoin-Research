// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_FSMODES_H
#define GRIDCOIN_UTIL_FSMODES_H

#include "fs.h"
#include "util/win_acl.h"

#include <cstdint>
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
        //! Windows only: the path was opened, but its access list could not be read, so it was
        //! not checked. When this process's own account could not be read, nothing was checked,
        //! and the one finding names the data directory.
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
//! cannot be opened to read its access list is skipped, as one that does not exist is.
std::vector<ModeFinding> CheckOwnerOnlyModes(const fs::path& datadir, const fs::path& conf,
                                             const fs::path& settings, const fs::path& wallet_file,
                                             const fs::path& backupdir, const fs::path& debuglog);

//! True when the path is not a symbolic link and its owner is this process's effective user.
//! On Windows: the path is not a reparse point of any kind, and the owner read through a handle
//! on it is this process's token user.
bool OwnedByUs(const fs::path& path);

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
//! (\p remote, the parent directory as its handle shows it) also a refusal of access, which a
//! share may give for the same reason. Pure.
std::optional<std::string> AclRefusedBy(uint32_t error, bool remote);

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
//! 0600 (POSIX), so the mode is right before any data is written. An existing target keeps its
//! mode. Returns why a target this call created is not owner-only (a filesystem without POSIX
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

} // namespace fsmodes

#endif // GRIDCOIN_UTIL_FSMODES_H
