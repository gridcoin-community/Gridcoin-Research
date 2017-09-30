#ifndef BACKUP_H
#define BACKUP_H

// Backup related functions
#include <string>

class CWallet;

std::string GetBackupFilename(const std::string& basename, const std::string& suffix = "");
bool BackupConfigFile(const std::string& strDest);
bool BackupWallet(const CWallet& wallet, const std::string& strDest);
extern bool BackupPrivateKeys(const CWallet& wallet, std::string& sTarget, std::string& sErrors);
extern bool BackupWallet(const CWallet& wallet, const std::string& strDest);

#endif // BACKUP_H
