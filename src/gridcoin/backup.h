// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_BACKUP_H
#define GRIDCOIN_BACKUP_H

#include "fs.h"

#include <string>

class CWallet;

namespace GRC {
std::string GetBackupFilename(const std::string& basename, const std::string& suffix = "");
fs::path GetBackupPath();

bool BackupsEnabled();
int64_t GetBackupInterval();
void RunBackupJob();

bool BackupConfigFile(const std::string& strDest);
bool BackupSettingsFile(const std::string& strDest);
bool MaintainBackups(fs::path wallet_backup_path, std::vector<std::string> backup_file_type,
                   unsigned int retention_by_num, unsigned int retention_by_days, std::vector<std::string>& files_removed);
//! Where a copy BackupWallet makes is going.
enum class BackupTarget
{
    //! A backup in the backup directory (-backupdir): kept even where the filesystem
    //! cannot make it owner-only, with one log line per process (CreateBackupDirectory).
    BackupDirectory,
    //! A snapshot in the core's transfer directory: owner-only or refused.
    TransferSnapshot,
};

bool BackupWallet(const CWallet& wallet, const std::string& strDest,
                  BackupTarget target = BackupTarget::BackupDirectory);

//! Logs one line, the first time in this process, saying that wallet backups in \p dir are
//! written although they cannot be made owner-only, and why (\p reason). Returns whether this
//! call logged.
bool LogUnrestrictedBackupOnce(const fs::path& dir, const std::string& reason);

//! Lets the next LogUnrestrictedBackupOnce log again.
void ResetUnrestrictedBackupLogForTesting();
}

#endif // GRIDCOIN_BACKUP_H
