// Backup related functions are placed here to keep vital sections of
// code contained while maintaining clean code.

#include "walletdb.h"
#include "wallet.h"
#include "util.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fstream>
#include <string>

boost::filesystem::path GetBackupPath();

using namespace boost;

boost::filesystem::path GetBackupPath()
{
    filesystem::path defaultDir = GetDataDir() / "walletbackups";
    return GetArg("-backupdir", defaultDir.string());
}

std::string GetBackupFilename(const std::string& basename, const std::string& suffix = "")
{
    time_t biTime;
    struct tm * blTime;
    time (&biTime);
    blTime = localtime(&biTime);
    char boTime[200];
    strftime(boTime, sizeof(boTime), "%Y-%m-%dT%H-%M-%S", blTime);
    std::string sBackupFilename;
    filesystem::path rpath;
    sBackupFilename = basename + "-" + std::string(boTime);
    if (!suffix.empty())
        sBackupFilename = sBackupFilename + "-" + suffix;
    rpath = GetBackupPath() / sBackupFilename;
    return rpath.string();
}

bool BackupConfigFile(const std::string& strDest)
{
    // Check to see if there is a parent_path in strDest to support custom locations by ui - bug fix

    filesystem::path ConfigSource = GetDataDir() / "gridcoinresearch.conf";
    filesystem::path ConfigTarget = strDest;
    filesystem::create_directories(ConfigTarget.parent_path());
    try
    {
        #if BOOST_VERSION >= 104000
            filesystem::copy_file(ConfigSource, ConfigTarget, filesystem::copy_option::overwrite_if_exists);
        #else
            filesystem::copy_file(ConfigSource, ConfigTarget);
        #endif
        printf("BackupConfigFile: Copied gridcoinresearch.conf to %s\n", ConfigTarget.string().c_str());
        return true;
    }
    catch(const filesystem::filesystem_error &e)
    {
        printf("BackupConfigFile: Error copying gridcoinresearch.conf to %s - %s\n", ConfigTarget.string().c_str(), e.what());
        return false;
    }
    return false;
}

bool BackupWallet(const CWallet& wallet, const std::string& strDest)
{
    if (!wallet.fFileBacked)
        return false;
    while (!fShutdown)
    {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(wallet.strWalletFile) || bitdb.mapFileUseCount[wallet.strWalletFile] == 0)
            {
                // Flush log data to the dat file
                bitdb.CloseDb(wallet.strWalletFile);
                bitdb.CheckpointLSN(wallet.strWalletFile);
                bitdb.mapFileUseCount.erase(wallet.strWalletFile);

                // Copy wallet.dat
                filesystem::path WalletSource = GetDataDir() / wallet.strWalletFile;
                filesystem::path WalletTarget = strDest;
                filesystem::create_directories(WalletTarget.parent_path());
                if (filesystem::is_directory(WalletTarget))
                    WalletTarget /= wallet.strWalletFile;
                try
                {
                    #if BOOST_VERSION >= 104000
                        filesystem::copy_file(WalletSource, WalletTarget, filesystem::copy_option::overwrite_if_exists);
                    #else
                        filesystem::copy_file(WalletSource, WalletTarget);
                    #endif
                    printf("BackupWallet: Copied wallet.dat to %s\r\n", WalletTarget.string().c_str());
                    return true;
                }
                catch(const filesystem::filesystem_error &e) {
                    printf("BackupWallet: Error copying wallet.dat to %s - %s\r\n", WalletTarget.string().c_str(), e.what());
                    return false;
                }
            }
        }
        MilliSleep(100);
    }
    return false;
}

bool BackupPrivateKeys(const CWallet& wallet, std::string& sTarget, std::string& sErrors)
{
    if (wallet.IsLocked() || fWalletUnlockStakingOnly)
    {
        sErrors = "Wallet needs to be fully unlocked to backup private keys. ";
        return false;
    }
    filesystem::path PrivateKeysTarget = GetBackupFilename("keys.dat");
    filesystem::create_directories(PrivateKeysTarget.parent_path());
    sTarget = PrivateKeysTarget.string();
    std::ofstream myBackup;
    myBackup.open (PrivateKeysTarget.string().c_str());
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
    printf("BackupPrivateKeys: Backup made to %s\r\n", PrivateKeysTarget.string().c_str());
    myBackup.close();
    return true;
}
