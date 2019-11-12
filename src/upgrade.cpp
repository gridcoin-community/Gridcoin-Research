#include "upgrade.h"
#include "util.h"
#include "init.h"

#include <algorithm>
#include <univalue.h>
#include <vector>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/exception/exception.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <ostream>
#include <iostream>

#include <zip.h>
#include <fstream>
#include <openssl/sha.h>

struct_SnapshotExtractStatus ExtractStatus;

bool fCancelOperation = false;

Upgrade::Upgrade()
{
    // Clear the structs
    DownloadStatus.SnapshotDownloadSize = 0;
    DownloadStatus.SnapshotDownloadSpeed = 0;
    DownloadStatus.SnapshotDownloadAmount = 0;
    DownloadStatus.SnapshotDownloadFailed = false;
    DownloadStatus.SnapshotDownloadComplete = false;
    DownloadStatus.SnapshotDownloadProgress = 0;
    ExtractStatus.SnapshotExtractFailed = false;
    ExtractStatus.SnapshotExtractComplete = false;
    ExtractStatus.SnapshotExtractProgress = 0;
}

void Upgrade::CheckForLatestUpdate()
{
    // If testnet skip this || If the user changes this to disable while wallet running just drop out of here now. (need a way to remove items from scheduler)
    if (fTestNet || GetBoolArg("-disableupdatecheck", false))
        return;

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
        LogPrintf("Update Checker: Exception occured while checking for latest update. (%s)", e.what());

        return;
    }

    if (VersionResponse.empty())
    {
        LogPrintf("Update Checker: No Response from github");

        return;
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
        // 'name' for checking if its a mandatory or leisure
        GithubReleaseData = find_value(Response, "tag_name").get_str();
        GithubReleaseTypeData = find_value(Response, "name").get_str();
        GithubReleaseBody = find_value(Response, "body").get_str();
    }

    catch (std::exception& ex)
    {
        LogPrintf("Update Checker: Exception occured while parsing json response (%s)", ex.what());

        return;
    }

    if (GithubReleaseTypeData.find("leisure"))
        GithubReleaseType = _("leisure");

    else if (GithubReleaseTypeData.find("mandatory"))
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
        LogPrintf("Update Check: Got malformed version (%s)", GithubReleaseData);

        return;
    }

    bool NewVersion = false;

    try {
        // Left to right version numbers.
        // 3 numbers to check for production.
        for (unsigned int x = 0; x < 3; x++)
        {
            if (NewVersion)
                break;

            if (std::stoi(GithubVersion[x]) > LocalVersion[x])
                NewVersion = true;
        }
    }
    catch (std::exception& ex)
    {
        LogPrintf("Update Check: Exception occured checking client version against github version (%s)", ToString(ex.what()));

        return;
    }

    if (!NewVersion)
        return;

    // New version was found
    std::string ClientMessage = _("Local version: ") + strprintf("%d.%d.%d.%d", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD) + "\r\n";
    ClientMessage.append(_("Github version: ") + GithubReleaseData + "\r\n");
    ClientMessage.append(_("This update is ") + GithubReleaseType + "\r\n\r\n");

    std::string ChangeLog = GithubReleaseBody;

    uiInterface.UpdateMessageBox(ClientMessage, ChangeLog);

    return;
}

void Upgrade::SnapshotMain()
{
    std::cout << std::endl;
    std::cout << _("Snapshot Process Has Begun.") << std::endl;
    std::cout << _("Warning: Ending this process after Stage 2 will result in syncing from 0 or an incomplete/corrupted blockchain.") << std::endl << std::endl;

    // Create a thread for snapshot to be downloaded
    boost::thread SnapshotDownloadThread(std::bind(&Upgrade::DownloadSnapshot, this));

    Progress Prog;

    Prog.SetType(0);

    while (!DownloadStatus.SnapshotDownloadComplete)
    {
        if (DownloadStatus.SnapshotDownloadFailed)
            throw std::runtime_error("Failed to download snapshot.zip; See debug.log");

        if (Prog.Update(DownloadStatus.SnapshotDownloadProgress, DownloadStatus.SnapshotDownloadSpeed, DownloadStatus.SnapshotDownloadAmount, DownloadStatus.SnapshotDownloadSize))
            std::cout << Prog.Status() << std::flush;

        MilliSleep(1000);
    }

    // This is needed in some spots as the download can complete before the next progress update occurs so just 100% here as it was successful
    if (Prog.Update(100, -1, DownloadStatus.SnapshotDownloadSize, DownloadStatus.SnapshotDownloadSize))
        std::cout << Prog.Status() << std::flush;

    std::cout << std::endl;

    Prog.SetType(1);

    if (VerifySHA256SUM())
    {
        Prog.Update(100);

        std::cout << Prog.Status() << std::flush;
    }

    else
        throw std::runtime_error("Failed to verify SHA256SUM of snapshot.zip; See debug.log");

    std::cout << std::endl;

    Prog.SetType(2);

    if (CleanupBlockchainData())
    {
        Prog.Update(100);

        std::cout << Prog.Status() << std::flush;
    }

    else
        throw std::runtime_error("Failed to Cleanup previous blockchain data; See debug.log");

    std::cout << std::endl;

    Prog.SetType(3);

    // Create a thread for snapshot to be extracted
    boost::thread SnapshotExtractThread(std::bind(&Upgrade::ExtractSnapshot, this));

    while (!ExtractStatus.SnapshotExtractComplete)
    {
        if (ExtractStatus.SnapshotExtractFailed)
        {
            // Do this without checking on sucess, If it passed in stage 3 it will pass here.
            CleanupBlockchainData();

            throw std::runtime_error("Failed to extract snapshot.zip; See debug.log");
        }

        if (Prog.Update(ExtractStatus.SnapshotExtractProgress))
            std::cout << Prog.Status() << std::flush;

        MilliSleep(1000);
    }

    if (Prog.Update(100))
        std::cout << Prog.Status() << std::flush;

    std::cout << std::endl;
    std::cout << _("Snapshot Process Complete!") << std::endl;

    return;
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
        LogPrintf("Snapshot Downloader: Exception occured while attempting to download snapshot (%s)", e.what());

        DownloadStatus.SnapshotDownloadFailed = true;
    }

    return;
}

bool Upgrade::VerifySHA256SUM()
{
    Http HTTPHandler;

    std::string ServerSHA256SUM = "";

    try
    {
        ServerSHA256SUM = HTTPHandler.GetSnapshotSHA256();
    }

    catch (std::runtime_error& e)
    {
        LogPrintf("Snapshot (VerifySHA256SUM): Exception occured while attempting to retrieve snapshot SHA256SUM (%s)", e.what());
    }

    if (ServerSHA256SUM.empty())
    {
        LogPrintf("Snapshot (VerifySHA256SUM): Empty sha256sum returned from server");

        return false;
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    boost::filesystem::path fileloc = GetDataDir() / "snapshot.zip";
    unsigned char *buffer[32768];
    int bytesread = 0;

    FILE *file = fsbridge::fopen(fileloc, "rb");

    if (!file)
    {
        LogPrintf("Snapshot (VerifySHA256SUM): Failed to open snapshot.zip");

        return false;
    }

    while ((bytesread = fread(buffer, 1, sizeof(buffer), file)))
        SHA256_Update(&ctx, buffer, bytesread);

    SHA256_Final(digest, &ctx);

    char mdString[SHA256_DIGEST_LENGTH*2+1];

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

    std::string FileSHA256SUM = {mdString};

    if (ServerSHA256SUM == FileSHA256SUM)
        return true;

    else
    {
        LogPrintf("Snapshot (VerifySHA256SUM): Mismatch of sha256sum of snapshot.zip (Server = %s / File = %s)", ServerSHA256SUM, FileSHA256SUM);

        return false;
    }
}

bool Upgrade::CleanupBlockchainData()
{
    boost::filesystem::path CleanupPath = GetDataDir();

    // We must delete previous blockchain data
    // txleveldb
    // blk*.dat
    boost::filesystem::directory_iterator IterEnd;

    try
    {
        // Remove the files. We iterate as we know blk* will exist more and more in future as well
        for (boost::filesystem::directory_iterator Iter(CleanupPath); Iter != IterEnd; ++Iter)
        {
            if (boost::filesystem::is_directory(Iter->path()))
            {
                size_t DirLoc = Iter->path().string().find("txleveldb");

                if (DirLoc != std::string::npos)
                    if (!boost::filesystem::remove_all(*Iter))
                        return false;

                continue;

            }

            else if (boost::filesystem::is_regular_file(*Iter))
            {
                size_t FileLoc = Iter->path().filename().string().find("blk");

                if (FileLoc != std::string::npos)
                {
                    std::string filetocheck = Iter->path().filename().string();
                    // Check it ends with .dat and starts with blk
                    if (filetocheck.substr(0, 3) == "blk" && filetocheck.substr(filetocheck.length() - 4, 4) == ".dat")
                        if (!boost::filesystem::remove(*Iter))
                            return false;
                }
                continue;
            }
        }
    }

    catch (boost::filesystem::filesystem_error &ex)
    {
        LogPrintf("Snapshot (CleanupBlockchainData): Exception occured: %s", ex.what());

        return false;
    }

    return true;
}

bool Upgrade::ExtractSnapshot()
{
    std::string ArchiveFileString = GetDataDir().string() +  "/snapshot.zip";
    const char* ArchiveFile = ArchiveFileString.c_str();
    boost::filesystem::path ExtractPath = GetDataDir();
    struct zip* ZipArchive;
    struct zip_file* ZipFile;
    struct zip_stat ZipStat;
    char Buf[1024*1024];
    int err;
    uint64_t i, j;
    int64_t entries, len, sum;
    long long totaluncompressedsize = 0;
    long long currentuncompressedsize = 0;
    int64_t lastupdated = GetAdjustedTime();

    try
    {
        ZipArchive = zip_open(ArchiveFile, 0, &err);

        if (ZipArchive == nullptr)
        {
            zip_error_to_str(Buf, sizeof(Buf), err, errno);

            ExtractStatus.SnapshotExtractFailed = true;

            LogPrintf("Snapshot (ExtractSnapshot): Error opening snapshot.zip: %s", Buf);

            return false;
        }

        entries = zip_get_num_entries(ZipArchive, 0);

        // Lets scan for total size uncompressed so we can do a detailed progress for the watching user
        for (j = 0; j < (uint64_t)entries; j++)
        {
            if (zip_stat_index(ZipArchive, j, 0, &ZipStat) == 0)
            {
                if (ZipStat.name[strlen(ZipStat.name) - 1] != '/')
                    totaluncompressedsize += ZipStat.size;
            }
        }

        // Now extract
        for (i = 0; i < (uint64_t)entries; i++)
        {
            if (zip_stat_index(ZipArchive, i, 0, &ZipStat) == 0)
            {
                // Does this require a directory
                if (ZipStat.name[strlen(ZipStat.name) - 1] == '/')
                    boost::filesystem::create_directory(ExtractPath / ZipStat.name);

                else
                {
                    ZipFile = zip_fopen_index(ZipArchive, i, 0);

                    if (!ZipFile)
                    {
                        ExtractStatus.SnapshotExtractFailed = true;

                        LogPrintf("Snapshot (ExtractSnapshot): Error opening file %s within snapshot.zip", ZipStat.name);

                        return false;
                    }

                    boost::filesystem::path ExtractFileString = ExtractPath / ZipStat.name;

                    FILE* ExtractFile = fsbridge::fopen(ExtractFileString, "wb");

                    if (!ExtractFile)
                    {
                        ExtractStatus.SnapshotExtractFailed = true;

                        LogPrintf("Snapshot (ExtractSnapshot): Error opening file %s on filesystem", ZipStat.name);

                        return false;
                    }

                    sum = 0;

                    while ((uint64_t)sum != ZipStat.size)
                    {
                        boost::this_thread::interruption_point();

                        len = zip_fread(ZipFile, Buf, 1024*1024);

                        if (len < 0)
                        {
                            ExtractStatus.SnapshotExtractFailed = true;

                            LogPrintf("Snapshot (ExtractSnapshot): Failed to read zip buffer");

                            return false;
                        }

                        fwrite(Buf, 1, (uint64_t)len, ExtractFile);

                        sum += len;
                        currentuncompressedsize += len;

                        // Update Progress every 1 second
                        if (GetAdjustedTime() > lastupdated)
                        {
                            lastupdated = GetAdjustedTime();

                            ExtractStatus.SnapshotExtractProgress = ((currentuncompressedsize / (double)totaluncompressedsize) * 100);
                        }
                    }

                    fclose(ExtractFile);
                    zip_fclose(ZipFile);
                }
            }
        }

        if (zip_close(ZipArchive) == -1)
        {
            ExtractStatus.SnapshotExtractFailed = true;

            LogPrintf("Sanpshot (ExtractSnapshot): Failed to close snapshot.zip");

            return false;
        }

        ExtractStatus.SnapshotExtractComplete = true;
    }

    catch (boost::thread_interrupted&)
    {
        return false;
    }

    return true;
}

void Upgrade::DeleteSnapshot()
{
    // File is out of scope now check if it exists and if so delete it.
    // This covers partial downloaded files or a http response downloaded into file.
    std::string snapshotfile = GetArg("-snapshoturl", "https://download.gridcoin.us/download/downloadstake/signed/snapshot.zip");

    size_t pos = snapshotfile.find_last_of("/");

    snapshotfile = snapshotfile.substr(pos + 1, (snapshotfile.length() - pos - 1));

    try
    {
        boost::filesystem::path snapshotpath = GetDataDir() / snapshotfile;

        if (boost::filesystem::exists(snapshotpath))
            if (boost::filesystem::is_regular_file(snapshotpath))
                boost::filesystem::remove(snapshotpath);
    }

    catch (boost::filesystem::filesystem_error& e)
    {
        LogPrintf("Snapshot Downloader: Exception occured while attempting to delete snapshot (%s)", e.code().message());
    }
}
