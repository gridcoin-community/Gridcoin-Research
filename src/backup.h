#ifndef BACKUP_H
#define BACKUP_H

// Backup related functions
#include <string>
#include <boost/filesystem.hpp>

class CWallet;

std::string GetBackupFilename(const std::string& basename, const std::string& suffix = "");
boost::filesystem::path GetBackupPath();

bool BackupsEnabled();
int64_t GetBackupInterval();
void RunBackupJob();

bool BackupConfigFile(const std::string& strDest);
bool MaintainBackups(boost::filesystem::path wallet_backup_path, std::vector<std::string> backup_file_type,
                   unsigned int retention_by_num, unsigned int retention_by_days, std::vector<std::string>& files_removed);
bool BackupWallet(const CWallet& wallet, const std::string& strDest);
extern bool BackupPrivateKeys(const CWallet& wallet, std::string& sTarget, std::string& sErrors);
extern bool BackupWallet(const CWallet& wallet, const std::string& strDest);

#endif // BACKUP_H
