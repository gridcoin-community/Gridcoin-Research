// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/backup.h"
#include "init.h"
#include "wallet/walletdb.h"
#include "wallet/wallet.h"
#include "util.h"
#include "util/time.h"

#include <string>

using namespace GRC;

fs::path GRC::GetBackupPath()
{
    fs::path defaultDir = GetDataDir() / "walletbackups";
    return GetArg("-backupdir", defaultDir.string());
}

std::string GRC::GetBackupFilename(const std::string& basename, const std::string& suffix)
{
    time_t biTime;
    struct tm * blTime;
    time (&biTime);
    blTime = localtime(&biTime);
    char boTime[200];
    strftime(boTime, sizeof(boTime), "%Y-%m-%dT%H-%M-%S", blTime);
    std::string sBackupFilename;
    fs::path rpath;
    sBackupFilename = basename + "-" + std::string(boTime);
    if (!suffix.empty())
        sBackupFilename = sBackupFilename + "-" + suffix;
    rpath = GetBackupPath() / sBackupFilename;
    return rpath.string();
}

bool GRC::BackupsEnabled()
{
    // If either of these configuration options is explicitly set to zero,
    // disable backups completely:
    return GetArg("-walletbackupinterval", 1) > 0
        && GetArg("-walletbackupintervalsecs", 1) > 0;
}

int64_t GRC::GetBackupInterval()
{
    int64_t backup_interval_secs = GetArg("-walletbackupintervalsecs", 86400);

    // The deprecated -walletbackupinterval option specifies the backup interval
    // as the number of blocks that pass. If someone still uses this in a config
    // file, we'll honor it for now:
    //
    if (mapArgs.count("-walletbackupinterval")) {
        backup_interval_secs = GetArg("-walletbackupinterval", 900) * 90;
    }

    return backup_interval_secs;
}

void GRC::RunBackupJob()
{
    TRY_LOCK(cs_main, locked_main);

    if (!locked_main) {
        return;
    }

    TRY_LOCK(pwalletMain->cs_wallet, locked_wallet);

    if (!locked_wallet) {
        return;
    }

    const int64_t now = GetSystemTimeInSeconds();

    static const int64_t interval = GetBackupInterval();
    static int64_t last_backup_time = pwalletMain->GetLastBackupTime();

    std::vector<std::string> backup_file_type;

    backup_file_type.push_back("wallet.dat");
    backup_file_type.push_back("gridcoinresearch.conf");

    std::vector<std::string> files_removed;

    // The scheduler runs this job at a faster rate than the configured backup
    // interval in case this function skips a cycle because of lock contention
    // in a busy wallet, so we double check the time:
    //
    if (now < last_backup_time + interval) {
        return;
    }

    // Store the last backup time to the wallet file so that we can resume the
    // configured backup schedule between node restarts:
    //
    pwalletMain->StoreLastBackupTime(now);

    const bool wallet_result = BackupWallet(*pwalletMain, GetBackupFilename(backup_file_type[0]));
    const bool config_result = BackupConfigFile(GetBackupFilename(backup_file_type[1]));
    const bool maintain_backups_result = MaintainBackups(GetBackupPath(), backup_file_type, 0, 0, files_removed);

    LogPrintf("%s: Scheduled backup results: Wallet: %s; Config: %s; History Maintenance: %s, %d files removed",
        __func__,
        (wallet_result ? "succeeded" : "failed"),
        (config_result ? "succeeded" : "failed"),
        (maintain_backups_result ? "succeeded" : "failed"),
        files_removed.size());

    last_backup_time = now;
}

bool GRC::BackupConfigFile(const std::string& strDest)
{
    // Check to see if there is a parent_path in strDest to support custom locations by ui - bug fix

    fs::path ConfigSource = GetConfigFile();
    fs::path ConfigTarget = strDest;
    fs::create_directories(ConfigTarget.parent_path());
    try
    {
        #if BOOST_VERSION >= 107400
            fs::copy_file(ConfigSource, ConfigTarget, fs::copy_options::overwrite_existing);
        #elif BOOST_VERSION >= 104000
            fs::copy_file(ConfigSource, ConfigTarget, fs::copy_option::overwrite_if_exists);
        #else
            fs::copy_file(ConfigSource, ConfigTarget);
        #endif
        LogPrintf("BackupConfigFile: Copied gridcoinresearch.conf to %s", ConfigTarget.string());
        return true;
    }
    catch(const fs::filesystem_error &e)
    {
        LogPrintf("BackupConfigFile: Error copying gridcoinresearch.conf to %s - %s", ConfigTarget.string(), e.what());
        return false;
    }
    return false;
}

bool GRC::BackupWallet(const CWallet& wallet, const std::string& strDest)
{
    if (!wallet.fFileBacked)
        return false;

    LOCK(bitdb.cs_db);
    if (!bitdb.mapFileUseCount.count(wallet.strWalletFile) || bitdb.mapFileUseCount[wallet.strWalletFile] == 0)
    {
        // Flush log data to the dat file
        bitdb.CloseDb(wallet.strWalletFile);
        bitdb.CheckpointLSN(wallet.strWalletFile);
        LogPrintf("Issuing lsn_reset for backup file portability.");
        bitdb.lsn_reset(wallet.strWalletFile);
        bitdb.mapFileUseCount.erase(wallet.strWalletFile);

        // Copy wallet.dat
        fs::path WalletSource = GetDataDir() / wallet.strWalletFile;
        fs::path WalletTarget = strDest;
        fs::create_directories(WalletTarget.parent_path());
        if (fs::is_directory(WalletTarget))
            WalletTarget /= wallet.strWalletFile;
        try
        {
#if BOOST_VERSION >= 107400
            fs::copy_file(WalletSource, WalletTarget, fs::copy_options::overwrite_existing);
#elif BOOST_VERSION >= 104000
            fs::copy_file(WalletSource, WalletTarget, fs::copy_option::overwrite_if_exists);
#else
            fs::copy_file(WalletSource, WalletTarget);
#endif
            LogPrintf("BackupWallet: Copied wallet.dat to %s", WalletTarget.string());
        }
        catch(const fs::filesystem_error &e) {
            LogPrintf("BackupWallet: Error copying wallet.dat to %s - %s", WalletTarget.string(), e.what());
            return false;
        }

        return true;
    }

    return false;
}

bool GRC::MaintainBackups(fs::path wallet_backup_path, std::vector<std::string> backup_file_type,
                   unsigned int retention_by_num, unsigned int retention_by_days, std::vector<std::string>& files_removed)
{
    // Backup file retention maintainer. Adapted from the scraper/main log archiver core.
    // This is count three for this type code:
    // TODO: Probably a good idea to encapsulate it into its own function that can be
    //used by backups and both loggers.

    bool maintain_backup_retention = GetBoolArg("-maintainbackupretention", false);

    // Nothing to do if maintain_backup_retention is not set, which is the default to be
    // safe (i.e. retain backups indefinitely is the default behavior).
    if (!maintain_backup_retention) return true;

    // Zeroes for both incoming retention arguments mean that the config file settings should be used.
      if (!retention_by_num && !retention_by_days)
    {
        // If either argument is set, then assign the one that is set.
        if (IsArgSet("-walletbackupretainnumfiles") || IsArgSet("-walletbackupretainnumdays"))
        {
            // Default to zero for the unset argument, which means unset here. Also, clamp
            // to zero for nonsensical negative values. That kind of stupidity will be
            // caught and dealt with below.
            retention_by_num = (unsigned int) std::max((int64_t) 0, GetArg("-walletbackupretainnumfiles", 0));
            retention_by_days = (unsigned int) std::max((int64_t) 0, GetArg("-walletbackupretainnumdays", 0));
        }
        else
        {
            // Default to 365 for each. (A very conservative setting.)
            retention_by_num = (unsigned int) GetArg("-walletbackupretainnumfiles", 365);
            retention_by_days = (unsigned int) GetArg("-walletbackupretainnumdays", 365);
        }
     }

     // This is a conditional clamp that checks for nonsensical values to protect people.
     // If -maintainbackupretention is set, but the other two are not, they both will default to
     // 365, which will pass this check. What this does is detect a scenario where both are
     // set to something less than 7, which is probably not a good idea. One of them can be set
     // to less than seven, or even not set (which is the same as zero), as long as the other
     // set to 7 or more. If both are set to less than seven, then they both will be clamped
     // to 7. Remember the retention will follow whichever results in more files being
     // retained, so the "and" condition is sufficient here.
     if (retention_by_num < 7 && retention_by_days < 7)
     {
         LogPrintf("ERROR: MaintainBackups: Nonsensical values specified for backup retention. "
                   "Clamping both number of files and number of days to 7. Retention will follow "
                   "whichever results in the most retention to be safe.");

         retention_by_num = 7;
         retention_by_days = 7;
     }

    LogPrintf ("INFO: MaintainBackups: number of files to retain %i, number of days to retain %i. "
               "The retention will follow whichever results in the greater number of files "
               "retained.", retention_by_num, retention_by_days);

    int64_t retention_cutoff_time = GetSystemTimeInSeconds() - retention_by_days * 86400;

    // Iterate through the log archive directory and delete the oldest files beyond the retention rules.
    // The names are in format <file type>-YYYY-MM-DDTHH-MM-SS for the backup entries, so iterate
    // through each file type and then filter by the sort order. The greater than sort in the set
    // will then return descending order by datetime.

    try {
        for (const auto& file_type : backup_file_type)
        {
            std::set<fs::directory_entry, std::greater <fs::directory_entry>> sorted_dir_entries;

            for (fs::directory_entry& dir_entry : fs::directory_iterator(wallet_backup_path))
            {
                std::string filename = dir_entry.path().filename().string();
                size_t found_pos = filename.find(file_type);

                if (found_pos != std::string::npos) sorted_dir_entries.insert(dir_entry);
            }

            // Now iterate through set of filtered filenames. Delete all files that do not meet
            // retention rules.
            unsigned int i = 0;
            for (auto const& iter : sorted_dir_entries)
            {
                std::string filename = iter.path().filename().string();

                int64_t imbedded_file_time = 0;

                size_t found_pos = filename.find("-");

                // If there is a dash in the filename to indicate the presence of a datetime,
                // and the substring after the dash has positive length, then attempt to parse
                // into a posix datetime;
                if (found_pos != std::string::npos && filename.size() - (found_pos + 1) > 0)
                {
                    std::string datetime = filename.substr(found_pos + 1, filename.size() - (found_pos + 1));

                    imbedded_file_time = ParseISO8601DateTime(datetime);

                    // If ParseISO8601DateTime can't parse the imbedded datetime string, it will return 0.
                    if (!imbedded_file_time)
                    {
                        LogPrintf("WARN: MaintainBackups: Unable to parse date-time in backup filename."
                                  "Ignoring time retention for this file.");
                    }
                }

                // This is a little tricky. If retention_by_days is set (i.e. greater than zero), then if the datetime is parseable
                // from the filename, then delete the file if both the datetime is earlier than the retention cutoff time AND
                // the number of the file is greater than the maximum number to retain. There might be a more compressed way to
                // write this, but it would probably be inscrutable, so it is expressed in a way that promotes understanding.
                bool remove_file = false;

                if (retention_by_days > 0)
                {
                    if (i >= retention_by_num && (!imbedded_file_time || (imbedded_file_time < retention_cutoff_time)))
                    {
                        remove_file = true;
                    }

                }
                // If retention_by_days is zero, then it is not set, and just remove files based on the number to retain rule.
                else
                {
                    if (i >= retention_by_num)
                    {
                        remove_file = true;
                    }
                }

                // Remove the file and push the filename back to the passed by reference vector parameter for info.
                if (remove_file)
                {
                    fs::remove(iter.path());

                    files_removed.push_back(iter.path().filename().string());

                    LogPrintf("INFO: MaintainBackups: Removed old backup file %s.", iter.path().filename().string());
                }

                ++i;
            } // end of SortedDirEntries for loop.
        } // end of backup_file_type for loop.
    }
    catch (const fs::filesystem_error &e)
    {
        LogPrintf("ERROR: MaintainBackups: Error managing backup file retention: %s", e.what());

        return false;
    }

    return true;
}

bool GRC::BackupPrivateKeys(const CWallet& wallet, std::string& sTarget, std::string& sErrors)
{
    if (wallet.IsLocked() || fWalletUnlockStakingOnly)
    {
        sErrors = "Wallet needs to be fully unlocked to backup private keys.";
        return false;
    }
    fs::path PrivateKeysTarget = GetBackupFilename("keys.dat");
    fs::create_directories(PrivateKeysTarget.parent_path());
    sTarget = PrivateKeysTarget.string();
    fsbridge::ofstream myBackup;
    myBackup.open(PrivateKeysTarget);
    std::string sError;
    for(const auto& keyPair : wallet.GetAllPrivateKeys(sError))
    {
        if (!sError.empty())
        {
            sErrors = sError;
            return false;
        }
        myBackup << "Address: " << keyPair.first.ToString() << ", Secret: " << keyPair.second.ToString() << std::endl;
    }
    LogPrintf("BackupPrivateKeys: Backup made to %s", PrivateKeysTarget.string());
    myBackup.close();
    return true;
}
