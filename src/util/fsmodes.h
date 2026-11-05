// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_FSMODES_H
#define GRIDCOIN_UTIL_FSMODES_H

#include "fs.h"

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
//! POSIX only. Windows protects the data directory with an inherited DACL (util/dir_permissions.h).
#ifndef WIN32
namespace fsmodes {

struct ModeFinding
{
    enum class Kind
    {
        //! An existing path owned by this account has a mode wider than the rule allows.
        WideMode,
        //! The same for a path owned by another account. Reported only, as this account cannot fix it.
        WideModeNotOwned,
        //! The path is a symbolic link. It is reported and never followed.
        Symlink,
    };

    fs::path path;
    Kind kind;
    //! The permission bits (st_mode & 07777) of the path itself, 0 for a symbolic link.
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
std::vector<ModeFinding> CheckOwnerOnlyModes(const fs::path& datadir, const fs::path& conf,
                                             const fs::path& settings, const fs::path& wallet_file,
                                             const fs::path& backupdir, const fs::path& debuglog);

//! True when the path is not a symbolic link and its owner is this process's effective user.
bool OwnedByUs(const fs::path& path);

//! The rule for every checked path: no access for "other" and no write for the group.
bool NotOwnedModeAcceptable(unsigned mode);

//! What the startup check of the backup directory found.
struct BackupDirCheck
{
    //! Why no file can be written there, or nullopt when one can.
    std::optional<std::string> not_writable;
    //! Why the files written there are not owner-only, or nullopt: the directory, created
    //! by the check, could not be restricted, or a file created there with mode 0600 has
    //! another mode (a filesystem without POSIX modes, such as FAT). Backups are still
    //! written there.
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
std::optional<std::string> CopyFileOwnerOnly(const fs::path& source, const fs::path& target);

} // namespace fsmodes
#endif // !WIN32

#endif // GRIDCOIN_UTIL_FSMODES_H
