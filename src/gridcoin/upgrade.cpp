// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/upgrade.h"
#include "util.h"
#include "init.h"

#include <algorithm>
#include <univalue.h>
#include <vector>
#include <boost/thread.hpp>
#include <boost/exception/exception.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <iostream>

#include <zip.h>
#include <openssl/sha.h>

using namespace GRC;

SnapshotExtractStatus GRC::ExtractStatus;

bool GRC::fCancelOperation = false;

Upgrade::Upgrade()
{
    DownloadStatus.Reset();
    ExtractStatus.Reset();
}

void Upgrade::ScheduledUpdateCheck()
{
    std::string VersionResponse = "";

    CheckForLatestUpdate(VersionResponse);
}

bool Upgrade::CheckForLatestUpdate(std::string& client_message_out, bool ui_dialog, bool snapshotrequest)
{
    // If testnet skip this || If the user changes this to disable while wallet running just drop out of here now.
    // (Need a way to remove items from scheduler.)
    if (fTestNet || (gArgs.GetBoolArg("-disableupdatecheck", false) && !snapshotrequest))
        return false;

    Http VersionPull;

    std::string GithubResponse = "";
    std::string VersionResponse = "";

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
        LogPrintf("WARNING %s: No Response from GitHub", __func__);

        return false;
    }

    std::string GithubReleaseData = "";
    std::string GithubReleaseTypeData = "";
    std::string GithubReleaseBody = "";
    std::string GithubReleaseType = "";

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

    boost::to_lower(GithubReleaseTypeData);
    if (GithubReleaseTypeData.find("leisure") != std::string::npos)
        GithubReleaseType = _("leisure");

    else if (GithubReleaseTypeData.find("mandatory") != std::string::npos)
        GithubReleaseType = _("mandatory");

    else
        GithubReleaseType = _("unknown");

    // Parse version data
    std::vector<std::string> GithubVersion;
    std::vector<int> LocalVersion;

    ParseString(GithubReleaseData, '.', GithubVersion);

    LocalVersion.push_back(CLIENT_VERSION_MAJOR);
    LocalVersion.push_back(CLIENT_VERSION_MINOR);
    LocalVersion.push_back(CLIENT_VERSION_REVISION);

    if (GithubVersion.size() != 4)
    {
        error("%s: Got malformed version (%s)", __func__, GithubReleaseData);

        return false;
    }

    bool NewVersion = false;
    bool NewMandatory = false;

    try {
        // Left to right version numbers.
        // 3 numbers to check for production.
        for (unsigned int x = 0; x < 3; x++)
        {
            if (std::stoi(GithubVersion[x]) > LocalVersion[x])
            {
                NewVersion = true;
                if (x < 2)
                {
                    NewMandatory = true;
                }
                break;
            }
        }
    }
    catch (std::exception& ex)
    {
        error("%s: Exception occurred checking client version against GitHub version (%s)",
                  __func__, ToString(ex.what()));

        return false;
    }

    if (!NewVersion) return NewVersion;

    // New version was found
    client_message_out = _("Local version: ") + strprintf("%d.%d.%d.%d", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR,
                                                          CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD) + "\r\n";
    client_message_out.append(_("GitHub version: ") + GithubReleaseData + "\r\n");
    client_message_out.append(_("This update is ") + GithubReleaseType + "\r\n\r\n");

    // For snapshot requests we will handle things differently after this point
    if (snapshotrequest && NewMandatory)
        return NewVersion;

    if (NewMandatory)
        client_message_out.append(_("WARNING: A mandatory release is available. Please upgrade as soon as possible.")
                                  + "\n");

    std::string ChangeLog = GithubReleaseBody;

    if (ui_dialog)
        uiInterface.UpdateMessageBox(client_message_out, ChangeLog);

    return NewVersion;
}

void Upgrade::SnapshotMain()
{
    std::cout << std::endl;
    std::cout << _("Snapshot Process Has Begun.") << std::endl;
    std::cout << _("Warning: Ending this process after Stage 2 will result in syncing from 0 or an "
                   "incomplete/corrupted blockchain.") << std::endl << std::endl;

    // Verify a mandatory release is not available before we continue to snapshot download.
    std::string VersionResponse = "";

    if (CheckForLatestUpdate(VersionResponse, false, true))
    {
        std::cout << this->ResetBlockchainMessages(UpdateAvailable) << std::endl;
        std::cout << this->ResetBlockchainMessages(GithubResponse) << std::endl;
        std::cout << VersionResponse << std::endl;

        throw std::runtime_error(_("Failed to download snapshot as mandatory client is available for download."));
    }

    Progress progress;

    progress.SetType(Progress::Type::SnapshotDownload);

    // Create a worker thread to do all of the heavy lifting. We are going to use a ping-pong workflow state here,
    // with progress Type as the trigger.
    boost::thread WorkerMainThread(std::bind(&Upgrade::WorkerMain, boost::ref(progress)));

    while (!DownloadStatus.GetSnapshotDownloadComplete())
    {
        if (DownloadStatus.GetSnapshotDownloadFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            throw std::runtime_error("Failed to download snapshot.zip; See debug.log");
        }

        if (progress.Update(DownloadStatus.GetSnapshotDownloadProgress(), DownloadStatus.GetSnapshotDownloadSpeed(),
                        DownloadStatus.GetSnapshotDownloadAmount(), DownloadStatus.GetSnapshotDownloadSize()))
        {
            std::cout << progress.Status() << std::flush;
        }

        MilliSleep(1000);
    }

    // This is needed in some spots as the download can complete before the next progress update occurs so just 100% here
    // as it was successful
    if (progress.Update(100, -1, DownloadStatus.GetSnapshotDownloadSize(), DownloadStatus.GetSnapshotDownloadSize()))
    {
        std::cout << progress.Status() << std::flush;
    }

    std::cout << std::endl;

    progress.SetType(Progress::Type::SHA256SumVerification);

    while (!DownloadStatus.GetSHA256SUMComplete())
    {
        if (DownloadStatus.GetSHA256SUMFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            throw std::runtime_error("Failed to verify SHA256SUM of snapshot.zip; See debug.log");
        }

        if (progress.Update(DownloadStatus.GetSHA256SUMProgress()))
        {
            std::cout << progress.Status() << std::flush;
        }

        MilliSleep(1000);
    }

    if (progress.Update(100)) std::cout << progress.Status() << std::flush;

    std::cout << std::endl;

    progress.SetType(Progress::Type::CleanupBlockchainData);

    while (!DownloadStatus.GetCleanupBlockchainDataComplete())
    {
        if (DownloadStatus.GetCleanupBlockchainDataFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            throw std::runtime_error("Failed to cleanup previous blockchain data prior to extraction of snapshot.zip; "
                                     "See debug.log");
        }

        if (progress.Update(DownloadStatus.GetCleanupBlockchainDataProgress()))
        {
            std::cout << progress.Status() << std::flush;
        }

        MilliSleep(1000);
    }

    if (progress.Update(100)) std::cout << progress.Status() << std::flush;

    std::cout << std::endl;

    progress.SetType(Progress::Type::SnapshotExtraction);

    while (!ExtractStatus.GetSnapshotExtractComplete())
    {
        if (ExtractStatus.GetSnapshotExtractFailed())
        {
            WorkerMainThread.interrupt();
            WorkerMainThread.join();
            // Do this without checking on success, If it passed in stage 3 it will pass here.
            CleanupBlockchainData();

            throw std::runtime_error("Failed to extract snapshot.zip; See debug.log");
        }

        if (progress.Update(ExtractStatus.GetSnapshotExtractProgress()))
            std::cout << progress.Status() << std::flush;

        MilliSleep(1000);
    }

    if (progress.Update(100)) std::cout << progress.Status() << std::flush;

    std::cout << std::endl;

    // This interrupt-join needs to be here to ensure the WorkerMain interrupts the while loop and collapses before the
    // Progress object that was passed to it is destroyed.
    WorkerMainThread.interrupt();
    WorkerMainThread.join();

    std::cout << _("Snapshot Process Complete!") << std::endl;

    return;
}

void Upgrade::WorkerMain(Progress& progress)
{
    // The "steps" are triggered in SnapshotMain but processed here in this switch statement.
    bool finished = false;

    while (!finished && !fCancelOperation)
    {
        boost::this_thread::interruption_point();

        switch (progress.GetType())
        {
        case Progress::Type::SnapshotDownload:
            if (DownloadStatus.GetSnapshotDownloadFailed())
            {
                finished = true;
                return;
            }
            else if (!DownloadStatus.GetSnapshotDownloadComplete())
            {
                DownloadSnapshot();
            }
            break;
        case Progress::Type::SHA256SumVerification:
            if (DownloadStatus.GetSHA256SUMFailed())
            {
                finished = true;
                return;
            }
            else if (!DownloadStatus.GetSHA256SUMComplete())
            {
                 VerifySHA256SUM();
            }
            break;
        case Progress::Type::CleanupBlockchainData:
            if (DownloadStatus.GetCleanupBlockchainDataFailed())
            {
                finished = true;
                return;
            }
            else if (!DownloadStatus.GetCleanupBlockchainDataComplete())
            {
                CleanupBlockchainData();
            }
            break;
        case Progress::Type::SnapshotExtraction:
            if (ExtractStatus.GetSnapshotExtractFailed())
            {
                finished = true;
                return;
            }
            else if (!ExtractStatus.GetSnapshotExtractComplete())
            {
                ExtractSnapshot();
            }
        }

        MilliSleep(1000);
    }
}

void Upgrade::DownloadSnapshot()
{
     // Download the snapshot.zip
    Http HTTPHandler;

    try
    {
        HTTPHandler.DownloadSnapshot();
    }
    catch(std::runtime_error& e)
    {
        error("%s: Exception occurred while attempting to download snapshot (%s)", __func__, e.what());

        DownloadStatus.SetSnapshotDownloadFailed(true);

        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Snapshot download complete.", __func__);

    DownloadStatus.SetSnapshotDownloadComplete(true);

    return;
}

void Upgrade::VerifySHA256SUM()
{
    Http HTTPHandler;

    std::string ServerSHA256SUM = "";

    try
    {
        ServerSHA256SUM = HTTPHandler.GetSnapshotSHA256();
    }
    catch (std::runtime_error& e)
    {
        error("%s: Exception occurred while attempting to retrieve snapshot SHA256SUM (%s)",
              __func__, e.what());

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }

    if (ServerSHA256SUM.empty())
    {
        error("%s: Empty SHA256SUM returned from server.", __func__);

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    fs::path fileloc = GetDataDir() / "snapshot.zip";
    unsigned char *buffer[32768];
    int bytesread = 0;

    CAutoFile file(fsbridge::fopen(fileloc, "rb"), SER_DISK, CLIENT_VERSION);

    if (file.IsNull())
    {
        error("%s: Failed to open snapshot.zip.", __func__);

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }

    unsigned int total_reads = fs::file_size(fileloc) / sizeof(buffer) + 1;

    unsigned int read_count = 0;
    while ((bytesread = fread(buffer, 1, sizeof(buffer), file.Get())))
    {
        SHA256_Update(&ctx, buffer, bytesread);
        ++read_count;

        DownloadStatus.SetSHA256SUMProgress(read_count * 100 / total_reads);
    }

    SHA256_Final(digest, &ctx);

    char mdString[SHA256_DIGEST_LENGTH*2+1];

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

    std::string FileSHA256SUM = {mdString};

    if (ServerSHA256SUM == FileSHA256SUM)
    {
        LogPrint(BCLog::LogFlags::VERBOSE, "INFO %s: SHA256SUM verification successful.", __func__);

        DownloadStatus.SetSHA256SUMProgress(100);
        DownloadStatus.SetSHA256SUMComplete(true);

        return;
    }
    else
    {
        error("%s: Mismatch of SHA256SUM of snapshot.zip (Server = %s / File = %s)",
              __func__, ServerSHA256SUM, FileSHA256SUM);

        DownloadStatus.SetSHA256SUMFailed(true);

        return;
    }
}

void Upgrade::CleanupBlockchainData()
{
    fs::path CleanupPath = GetDataDir();

    // This is required because of problems with junction point handling in the boost filesystem library. Please see
    // https://github.com/boostorg/filesystem/issues/125. We are not quite ready to switch over to std::filesystem yet.
    // 1. I don't know whether the issue is fixed there, and
    // 2. Not all C++17 compilers have the filesystem headers, since this was merged from boost in 2017.
    //
    // I don't believe it is very common for Windows users to redirect the Gridcoin data directory with a junction point,
    // but it is certainly possible. We should handle it as gracefully as possible.
    if (fs::is_symlink(CleanupPath))
    {
        LogPrintf("INFO: %s: Data directory is a symlink.",
                  __func__);

        try
        {
            LogPrintf("INFO: %s: True path for the symlink is %s.", __func__, fs::read_symlink(CleanupPath).string());

            CleanupPath = fs::read_symlink(CleanupPath);
        }
        catch (fs::filesystem_error &ex)
        {
            error("%s: The data directory symlink or junction point cannot be resolved to the true canonical path. "
                  "This can happen on Windows. Please change the data directory specified to the actual true path "
                  "using the  -datadir=<path> option and try again.", __func__);

            DownloadStatus.SetCleanupBlockchainDataFailed(true);

            return;
        }
    }

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

            else if (fs::is_regular_file(*Iter))
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

            else if (fs::is_regular_file(*Iter))
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

void Upgrade::ExtractSnapshot()
{
    try
    {
        zip_error_t* err = new zip_error_t;
        struct zip* ZipArchive;

        std::string archive_file_string = (GetDataDir() / "snapshot.zip").string();
        const char* archive_file = archive_file_string.c_str();

        int ze;

        ZipArchive = zip_open(archive_file, 0, &ze);
        zip_error_init_with_code(err, ze);

        if (ZipArchive == nullptr)
        {
            ExtractStatus.SetSnapshotExtractFailed(true);

            error("%s: Error opening snapshot.zip as zip archive: %s", __func__, zip_error_strerror(err));

            return;
        }

        fs::path ExtractPath = GetDataDir();
        struct zip_stat ZipStat;
        int64_t lastupdated = GetAdjustedTime();
        long long totaluncompressedsize = 0;
        long long currentuncompressedsize = 0;
        uint64_t entries = (uint64_t) zip_get_num_entries(ZipArchive, 0);

        // Let's scan for total size uncompressed so we can do a detailed progress for the watching user
        for (u_int64_t j = 0; j < entries; ++j)
        {
            if (zip_stat_index(ZipArchive, j, 0, &ZipStat) == 0)
            {
                if (ZipStat.name[strlen(ZipStat.name) - 1] != '/')
                    totaluncompressedsize += ZipStat.size;
            }
        }

        // This protects against a divide by error below and properly returns false
        // if the zip file has no entries.
        if (!totaluncompressedsize)
        {
            ExtractStatus.SetSnapshotZipInvalid(true);

            error("%s: Error - snapshot.zip has no entries", __func__);

            return;
        }

        // Now extract
        for (u_int64_t i = 0; i < entries; ++i)
        {
            if (zip_stat_index(ZipArchive, i, 0, &ZipStat) == 0)
            {
                // Does this require a directory
                if (ZipStat.name[strlen(ZipStat.name) - 1] == '/')
                {
                    fs::create_directory(ExtractPath / ZipStat.name);
                }
                else
                {
                    struct zip_file* ZipFile;

                    ZipFile = zip_fopen_index(ZipArchive, i, 0);

                    if (!ZipFile)
                    {
                        ExtractStatus.SetSnapshotExtractFailed(true);

                        error("%s: Error opening file %s within snapshot.zip", __func__, ZipStat.name);

                        return;
                    }


                    fs::path ExtractFileString = ExtractPath / ZipStat.name;

                    CAutoFile ExtractFile(fsbridge::fopen(ExtractFileString, "wb"), SER_DISK, CLIENT_VERSION);

                    if (ExtractFile.IsNull())
                    {
                        ExtractStatus.SetSnapshotExtractFailed(true);

                        error("%s: Error opening file %s on filesystem", __func__, ZipStat.name);

                        return;
                    }

                    int64_t sum = 0;

                    while ((uint64_t) sum < ZipStat.size)
                    {
                        int64_t len = 0;
                        // Note that using a buffer larger than this risks crashes on macOS.
                        char Buf[256*1024];

                        boost::this_thread::interruption_point();

                        len = zip_fread(ZipFile, &Buf, 256*1024);

                        if (len < 0)
                        {
                            ExtractStatus.SetSnapshotExtractFailed(true);

                            error("%s: Failed to read zip buffer", __func__);

                            return;
                        }

                        fwrite(Buf, 1, (uint64_t) len, ExtractFile.Get());

                        sum += len;
                        currentuncompressedsize += len;

                        // Update Progress every 1 second
                        if (GetAdjustedTime() > lastupdated)
                        {
                            lastupdated = GetAdjustedTime();

                            ExtractStatus.SetSnapshotExtractProgress(currentuncompressedsize * 100
                                                                     / totaluncompressedsize);
                        }
                    }

                    zip_fclose(ZipFile);
                }
            }
        }

        if (zip_close(ZipArchive) == -1)
        {
            ExtractStatus.SetSnapshotExtractFailed(true);

            error("%s: Failed to close snapshot.zip", __func__);

            return;
        }
    }

    catch (boost::thread_interrupted&)
    {
        ExtractStatus.SetSnapshotExtractFailed(true);

        return;
    }

    catch (std::exception& e)
    {
        error("%s: Error occurred during snapshot zip file extraction: %s", __func__, e.what());

        ExtractStatus.SetSnapshotExtractFailed(true);

        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Snapshot.zip extraction successful.", __func__);

    ExtractStatus.SetSnapshotExtractProgress(100);
    ExtractStatus.SetSnapshotExtractComplete(true);
    return;
}

void Upgrade::DeleteSnapshot()
{
    // File is out of scope now check if it exists and if so delete it.
    try
    {
        fs::path snapshotpath = GetDataDir() / "snapshot.zip";

        if (fs::exists(snapshotpath))
            if (fs::is_regular_file(snapshotpath))
                fs::remove(snapshotpath);
    }

    catch (fs::filesystem_error& e)
    {
        LogPrintf("Snapshot Downloader: Exception occurred while attempting to delete snapshot (%s)", e.code().message());
    }
}

bool Upgrade::ResetBlockchainData()
{
    CleanupBlockchainData();

    return (DownloadStatus.GetCleanupBlockchainDataComplete() && !DownloadStatus.GetCleanupBlockchainDataFailed());
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
        case UpdateAvailable: stream << _("Unable to download a snapshot, as the wallet has detected that a new mandatory "
                                          "version is available for install. The mandatory upgrade must be installed before "
                                          "the snapshot can be downloaded and applied."); break;
        case GithubResponse: stream << _("Latest Version GitHub data response:"); break;
    }

    const std::string& output = stream.str();

    return output;
}
