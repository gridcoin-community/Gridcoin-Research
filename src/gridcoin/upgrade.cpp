// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/upgrade.h"
#include "util.h"
#include "init.h"

#include <algorithm>
#include <stdexcept>
#include <univalue.h>
#include <vector>
#include <boost/thread.hpp>
#include <boost/exception/exception.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <iostream>

using namespace GRC;

Upgrade::Upgrade()
{
    DownloadStatus.Reset();
}

void Upgrade::ScheduledUpdateCheck()
{
    std::string VersionResponse;
    std::string change_log;

    Upgrade::UpgradeType upgrade_type {Upgrade::UpgradeType::Unknown};

    CheckForLatestUpdate(VersionResponse, change_log, upgrade_type);
}

bool Upgrade::CheckForLatestUpdate(std::string& client_message_out, std::string& change_log, Upgrade::UpgradeType& upgrade_type,
                                   bool ui_dialog)
{
    // If testnet skip this || If the user changes this to disable while wallet running just drop out of here now.
    // (Need a way to remove items from scheduler.)
    if (fTestNet || gArgs.GetBoolArg("-disableupdatecheck", false))
        return false;

    Http VersionPull;

    std::string GithubResponse;
    std::string VersionResponse;

    // We receive the response and it's in a json reply
    UniValue Response(UniValue::VOBJ);

    try
    {
        VersionResponse = VersionPull.GetLatestVersionResponse();
    }

    catch (const std::runtime_error& e)
    {
        return error("%s: Exception occurred while checking for latest update. (%s)", __func__, e.what());
    }

    if (VersionResponse.empty())
    {
        LogPrintf("WARNING: %s: No Response from GitHub", __func__);

        return false;
    }

    std::string GithubReleaseData;
    std::string GithubReleaseTypeData;
    std::string GithubReleaseBody;
    std::string GithubReleaseType;

    try
    {
        Response.read(VersionResponse);

        // Get the information we need:
        // 'body' for information about changes
        // 'tag_name' for version
        // 'name' for checking if it is a mandatory or leisure
        GithubReleaseData = find_value(Response, "tag_name").get_str();
        GithubReleaseTypeData = find_value(Response, "name").get_str();
        GithubReleaseBody = find_value(Response, "body").get_str();
    }

    catch (std::exception& ex)
    {
        error("%s: Exception occurred while parsing json response (%s)", __func__, ex.what());

        return false;
    }

    GithubReleaseTypeData = ToLower(GithubReleaseTypeData);

    if (GithubReleaseTypeData.find("leisure") != std::string::npos) {
        GithubReleaseType = _("leisure");
        upgrade_type = Upgrade::UpgradeType::Leisure;
    } else if (GithubReleaseTypeData.find("mandatory") != std::string::npos) {
        GithubReleaseType = _("mandatory");
        // This will be confirmed below by also checking the second position version. If not incremented, then it will
        // be set to unknown.
        upgrade_type = Upgrade::UpgradeType::Mandatory;
    } else {
        GithubReleaseType = _("unknown");
        upgrade_type = Upgrade::UpgradeType::Unknown;
    }

    // Parse version data
    std::vector<std::string> GithubVersion;
    std::vector<int> LocalVersion;

    ParseString(GithubReleaseData, '.', GithubVersion);

    LocalVersion.push_back(CLIENT_VERSION_MAJOR);
    LocalVersion.push_back(CLIENT_VERSION_MINOR);
    LocalVersion.push_back(CLIENT_VERSION_REVISION);
    LocalVersion.push_back(CLIENT_VERSION_BUILD);

    if (GithubVersion.size() != 4)
    {
        error("%s: Got malformed version (%s)", __func__, GithubReleaseData);

        return false;
    }

    bool NewVersion = false;
    bool NewMandatory = false;
    bool same_version = true;

    try {
        // Left to right version numbers.
        // 4 numbers to check.
        for (unsigned int x = 0; x <= 3; x++) {
            int github_version = 0;

            if (!ParseInt32(GithubVersion[x], &github_version)) {
                throw std::invalid_argument("Failed to parse GitHub version from official GitHub project repo.");
            }

            if (github_version > LocalVersion[x]) {
                NewVersion = true;
                same_version = false;

                if (x < 2 && upgrade_type == Upgrade::UpgradeType::Mandatory) {
                    NewMandatory = true;
                } else {
                    upgrade_type = Upgrade::UpgradeType::Unknown;
                }
            } else {
                same_version &= (github_version == LocalVersion[x]);
            }
        }
    } catch (std::exception& ex) {
        error("%s: Exception occurred checking client version against GitHub version (%s)",
                  __func__, ToString(ex.what()));

        upgrade_type = Upgrade::UpgradeType::Unknown;
        return false;
    }

    // Populate client_message_out regardless of whether new version is found, because we are using this method for
    // the version information button in the "About Gridcoin" dialog.
    client_message_out = _("Local version: ") + strprintf("%d.%d.%d.%d", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR,
                                                          CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD) + "\r\n";
    client_message_out.append(_("GitHub version: ") + GithubReleaseData + "\r\n");

    if (NewVersion) {
        client_message_out.append(_("This update is ") + GithubReleaseType + "\r\n\r\n");
    } else if (same_version) {
        client_message_out.append(_("The latest release is ") + GithubReleaseType + "\r\n\r\n");
        client_message_out.append(_("You are running the latest release.") + "\n");
    } else {
        client_message_out.append(_("The latest release is ") + GithubReleaseType + "\r\n\r\n");

        // If not a new version available and the version is not the same, the only thing left is that we are running
        // a version greater than the latest release version, so set the upgrade_type to Unsupported, which is used for a
        // warning.
        upgrade_type = Upgrade::UpgradeType::Unsupported;
        client_message_out.append(_("WARNING: You are running a version that is higher than the latest release.") + "\n");
    }

    change_log = GithubReleaseBody;

    if (!NewVersion) return false;

    if (NewMandatory) {
        client_message_out.append(_("WARNING: A mandatory release is available. Please upgrade as soon as possible.")
                                  + "\n");
    }

    if (ui_dialog) {
        uiInterface.UpdateMessageBox(client_message_out, static_cast<int>(upgrade_type), change_log);
    }

    return true;
}

bool Upgrade::GetActualCleanupPath(fs::path& actual_cleanup_path)
{
    actual_cleanup_path = GetDataDir();

    // This is required because of problems with junction point handling in the boost filesystem library. Please see
    // https://github.com/boostorg/filesystem/issues/125. We are not quite ready to switch over to std::filesystem yet.
    // 1. I don't know whether the issue is fixed there, and
    // 2. Not all C++17 compilers have the filesystem headers, since this was merged from boost in 2017.
    //
    // I don't believe it is very common for Windows users to redirect the Gridcoin data directory with a junction point,
    // but it is certainly possible. We should handle it as gracefully as possible.
    if (fs::is_symlink(actual_cleanup_path))
    {
        LogPrintf("INFO: %s: Data directory is a symlink.",
                  __func__);

        try
        {
            LogPrintf("INFO: %s: True path for the symlink is %s.", __func__, fs::read_symlink(actual_cleanup_path).string());

            actual_cleanup_path = fs::read_symlink(actual_cleanup_path);
        }
        catch (fs::filesystem_error &ex)
        {
            error("%s: The data directory symlink or junction point cannot be resolved to the true canonical path. "
                  "This can happen on Windows. Please change the data directory specified to the actual true path "
                  "using the  -datadir=<path> option and try again.", __func__);

            DownloadStatus.SetCleanupBlockchainDataFailed(true);

            return false;
        }
    }

    return true;
}

void Upgrade::CleanupBlockchainData(bool include_blockchain_data_files)
{
    fs::path CleanupPath;

    if (!GetActualCleanupPath(CleanupPath)) return;

    unsigned int total_items = 0;
    unsigned int items = 0;

    // We must delete previous blockchain data
    // txleveldb
    // accrual
    // blk*.dat
    fs::directory_iterator IterEnd;

    // Count for progress bar first
    try
    {
        for (fs::directory_iterator Iter(CleanupPath); Iter != IterEnd; ++Iter)
        {
            if (fs::is_directory(Iter->path()))
            {
                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "txleveldb")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();
                         ++it)
                    {
                        ++total_items;
                    }
                }

                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "accrual")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();
                         ++it)
                    {
                        ++total_items;
                    }
                }

                // If it was a directory no need to check if a regular file below.
                continue;
            }

            else if (fs::is_regular_file(*Iter) && include_blockchain_data_files)
            {
                size_t FileLoc = Iter->path().filename().string().find("blk");

                if (FileLoc != std::string::npos)
                {
                    std::string filetocheck = Iter->path().filename().string();

                    // Check it ends with .dat and starts with blk
                    if (filetocheck.substr(0, 3) == "blk" && filetocheck.substr(filetocheck.length() - 4, 4) == ".dat")
                    {
                        ++total_items;
                    }
                }
            }
        }
    }
    catch (fs::filesystem_error &ex)
    {
        error("%s: Exception occurred: %s", __func__, ex.what());

        DownloadStatus.SetCleanupBlockchainDataFailed(true);

        return;
    }

    if (!total_items)
    {
        // Nothing to clean up!

        DownloadStatus.SetCleanupBlockchainDataComplete(true);

        return;
    }

    // Now try the cleanup.
    try
    {
        // Remove the files. We iterate as we know blk* will exist more and more in future as well
        for (fs::directory_iterator Iter(CleanupPath); Iter != IterEnd; ++Iter)
        {
            if (fs::is_directory(Iter->path()))
            {
                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "txleveldb")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();)
                    {
                        fs::path filepath = *it++;

                        if (fs::remove(filepath))
                        {
                            ++items;
                            DownloadStatus.SetCleanupBlockchainDataProgress(items * 100 / total_items);
                        }
                        else
                        {
                            DownloadStatus.SetCleanupBlockchainDataFailed(true);

                            return;
                        }
                    }
                }

                if (fs::relative(Iter->path(), CleanupPath) == (fs::path) "accrual")
                {
                    for (fs::recursive_directory_iterator it(Iter->path());
                         it != fs::recursive_directory_iterator();)
                    {
                        fs::path filepath = *it++;

                        if (fs::remove(filepath))
                        {
                            ++items;
                            DownloadStatus.SetCleanupBlockchainDataProgress(items * 100 / total_items);
                        }
                        else
                        {
                            DownloadStatus.SetCleanupBlockchainDataFailed(true);

                            return;
                        }
                    }
                }

                // If it was a directory no need to check if a regular file below.
                continue;
            }

            else if (fs::is_regular_file(*Iter) && include_blockchain_data_files)
            {
                size_t FileLoc = Iter->path().filename().string().find("blk");

                if (FileLoc != std::string::npos)
                {
                    std::string filetocheck = Iter->path().filename().string();

                    // Check it ends with .dat and starts with blk
                    if (filetocheck.substr(0, 3) == "blk" && filetocheck.substr(filetocheck.length() - 4, 4) == ".dat")
                    {
                        if (fs::remove(*Iter))
                        {
                            ++items;
                            DownloadStatus.SetCleanupBlockchainDataProgress(items * 100 / total_items);
                        }
                        else
                        {
                            DownloadStatus.SetCleanupBlockchainDataFailed(true);

                            return;
                        }
                    }
                }
            }
        }
    }
    catch (fs::filesystem_error &ex)
    {
        error("%s: Exception occurred: %s", __func__, ex.what());

        DownloadStatus.SetCleanupBlockchainDataFailed(true);

        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Prior blockchain data cleanup successful.", __func__);

    DownloadStatus.SetCleanupBlockchainDataProgress(100);
    DownloadStatus.SetCleanupBlockchainDataComplete(true);

    return;
}

bool Upgrade::ResetBlockchainData(bool include_blockchain_data_files)
{
    CleanupBlockchainData(include_blockchain_data_files);

    return (DownloadStatus.GetCleanupBlockchainDataComplete() && !DownloadStatus.GetCleanupBlockchainDataFailed());
}

bool Upgrade::MoveBlockDataFiles(std::vector<std::pair<fs::path, uintmax_t>>& block_data_files)
{
    fs::path cleanup_path;

    if (!GetActualCleanupPath(cleanup_path)) return false;

    fs::directory_iterator IterEnd;

    try {
        for (fs::directory_iterator Iter(cleanup_path); Iter != IterEnd; ++Iter) {
            if (fs::is_regular_file(*Iter)) {
                size_t FileLoc = Iter->path().filename().string().find("blk");

                if (FileLoc != std::string::npos) {
                    std::string filetocheck = Iter->path().filename().string();

                    // Check it ends with .dat and starts with blk
                    if (filetocheck.substr(0, 3) == "blk" && filetocheck.substr(filetocheck.length() - 4, 4) == ".dat") {
                        fs::path new_name = *Iter;
                        new_name.replace_extension(".dat.orig");

                        uintmax_t file_size = fs::file_size(Iter->path());

                        // Rename with orig as the extension, because ProcessBlock will load blocks into a new block data
                        // file.
                        fs::rename(*Iter, new_name);
                        block_data_files.push_back(std::make_pair(new_name, file_size));
                    }
                }
            }
        }
    } catch (fs::filesystem_error &ex) {
        error("%s: Exception occurred: %s. Failed to rename block data files to blk*.dat.orig in preparation for "
              "reindexing.", __func__, ex.what());

        return false;
    }

    return true;
}

bool Upgrade::LoadBlockchainData(std::vector<std::pair<fs::path, uintmax_t>>& block_data_files, bool sort,
                                 bool cleanup_imported_files)
{
    bool successful = true;

    uintmax_t total_size = 0;
    uintmax_t cumulative_size = 0;

    for (const auto& iter : block_data_files) {
        total_size += iter.second;
    }

    if (!total_size) return false;

    // This conditional sort is necessary to allow for two different desired behaviors. -reindex requires the filesystem
    // entries to be sorted, because the order of files in a filesystem iterator in a directory is not guaranteed. On the
    // other hand, for multiple -loadblock arguments, the order of the arguments should be preserved.
    if (sort) std::sort(block_data_files.begin(), block_data_files.end());

    try {
        for (const auto& iter : block_data_files) {

            unsigned int percent_start = cumulative_size * (uintmax_t) 100 / total_size;

            cumulative_size += iter.second;

            unsigned int percent_end = cumulative_size * (uintmax_t) 100 / total_size;

            FILE *block_data_file = fsbridge::fopen(iter.first, "rb");

            LogPrintf("INFO: %s: Loading blocks from %s.", __func__, iter.first.filename().string());

            if (!LoadExternalBlockFile(block_data_file, iter.second, percent_start, percent_end)) {
                successful = false;

                break;
            }
        }
    } catch (fs::filesystem_error &ex) {
        error("%s: Exception occurred: %s. Failure occurred during attempt to load blocks from original "
              "block data file(s).", __func__, ex.what());

        successful = false;
    }

    if (successful) {
        // Only delete the source files that were imported if cleanup_imported_files is set to true
        if (cleanup_imported_files) {
            try {
                for (const auto& iter : block_data_files) {
                    if (!fs::remove(iter.first)) {
                        LogPrintf("WARN: %s: Reindexing of the blockchain was successful; however, one or more of "
                                  "the original block data files (%s) was not able to be deleted. You "
                                  "will have to delete this file manually.", __func__, iter.first.filename().string());
                    }
                }
            }
            catch (fs::filesystem_error &ex) {
                LogPrintf("WARN: %s: Exception occurred: %s. This error occurred while attempting to delete the original "
                          "block data files (blk*.dat.orig). You will have to delete these manually.", __func__, ex.what());
            }
        }
    } else {
        error("%s: A failure occurred during the reindexing of the block data files. The blockchain state is invalid and "
              "you should restart the wallet with the -resetblockchaindata option to clear out the blockchain database "
              "and re-sync the blockchain from the network.", __func__);

        DownloadStatus.SetCleanupBlockchainDataFailed(true);

        return false;
    }

    LogPrintf("INFO: %s: Reindex of the blockchain data was successful.", __func__);

    return true;
}

std::string Upgrade::ResetBlockchainMessages(ResetBlockchainMsg _msg)
{
    std::stringstream stream;

    switch (_msg) {
        case CleanUp:
        {
            stream << _("Datadir: ");
            stream << GetDataDir().string();
            stream << "\r\n\r\n";
            stream << _("Due to the failure to delete the blockchain data you will be required to manually delete the data "
                        "before starting your wallet.");
            stream << "\r\n";
            stream << _("Failure to do so will result in undefined behaviour or failure to start wallet.");
            stream << "\r\n\r\n";
            stream << _("You will need to delete the following.");
            stream << "\r\n\r\n";
            stream << _("Files:");
            stream << "\r\n";
            stream << "blk000*.dat";
            stream << "\r\n\r\n";
            stream << _("Directories:");
            stream << "\r\n";
            stream << "txleveldb";
            stream << "\r\n";
            stream << "accrual";

            break;
        }
    }

    const std::string& output = stream.str();

    return output;
}
