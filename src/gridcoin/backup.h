// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

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
