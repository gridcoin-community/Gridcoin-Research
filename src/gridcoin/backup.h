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
bool MaintainBackups(fs::path wallet_backup_path, std::vector<std::string> backup_file_type,
                   unsigned int retention_by_num, unsigned int retention_by_days, std::vector<std::string>& files_removed);
bool BackupWallet(const CWallet& wallet, const std::string& strDest);
bool BackupPrivateKeys(const CWallet& wallet, std::string& sTarget, std::string& sErrors);
}

#endif // GRIDCOIN_BACKUP_H
