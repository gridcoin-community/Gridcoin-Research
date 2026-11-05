// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_BACKUP_H
#define GRIDCOIN_BACKUP_H

#include "fs.h"

#include <cstdint>
#include <string>
#include <vector>

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

//! The automatic wallet backups in \p backup_dir whose names give a time before
//! \p encrypted_at (seconds since the epoch), in name order. Those made before a wallet was
//! encrypted hold its unencrypted keys.
//!
//! An automatic backup is a regular file directly in \p backup_dir with a name that
//! GetBackupFilename("wallet.dat") gives, or gave in an older release:
//! wallet.dat-YYYY-MM-DDTHH-MM-SS, then a Z, then a suffix that begins with '-', the last two
//! each optional. The time in the name is UTC. A name with a control character (a byte below
//! 0x20, or 0x7f) is not listed, since the names reach the RPC result and the GUI. A symbolic
//! link is never followed, so one with such a name is not listed, and neither is a FIFO or
//! anything else that is not a regular file. A directory that does not exist or cannot be read
//! lists nothing (the second is logged). Nothing is deleted: a plaintext backup can be the
//! user's last way back into a wallet whose new passphrase is lost, so the user deletes them by
//! hand.
std::vector<fs::path> ListAutomaticWalletBackupsBefore(const fs::path& backup_dir, int64_t encrypted_at);

//! What encryptwallet adds to its result: the backups in \p found, from \p backup_dir, by
//! name, and any backup made by hand before encryption, hold the unencrypted keys; the order
//! in which to get rid of them; and where the docs say more.
std::string PreEncryptionBackupWarning(const fs::path& backup_dir, const std::vector<fs::path>& found);
}

#endif // GRIDCOIN_BACKUP_H
