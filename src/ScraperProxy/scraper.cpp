#include "scraper.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

extern bool fShutdown;

bool find(const std::string& s, const std::string& find);
std::string urlsanity(const std::string& s, const std::string& type);
std::string lowercase(std::string s);
std::string ExtractXML(const std::string XMLdata, const std::string key, const std::string key_end);
std::vector<std::string> vXMLData(const std::string& xmldata, int64_t teamid);
int64_t teamid(const std::string& xmldata);
ScraperFileManifest StructScraperFileManifest;

CCriticalSection cs_StructScraperFileManifest;

bool WhitelistPopulated();
bool UserpassPopulated();
bool DownloadProjectRacFilesByCPID();
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag);
bool AuthenticationETagUpdate(const std::string& project, const std::string& etag);
void AuthenticationETagClear();

int GetDayOfYear(int64_t timestamp);

void testdata(const std::string& etag);

// Note these four are initialized here for standalone mode compile, but will be overwritten by
// the values from GetArg in init.cpp when compiled as part of the wallet.
unsigned int nScraperSleep = 60000;
unsigned int nActiveBeforeSB = 300;
bool fScraperRetainNonCurrentFiles = false;
fs::path pathScraper = fs::current_path() / "Scraper";

extern void MilliSleep(int64_t n);
extern BeaconConsensus GetConsensusBeaconList();

// This is the scraper thread...
void Scraper(bool fScraperStandalone)
{
    _log(INFO, "Scraper", "Starting Scraper thread.");

    // This is necessary to maintain compatibility with Windows.
    pathScraper.imbue(std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>()));

    // Hash check
    std::string sHashCheck = "Hello world";
    uint256 nHashCheck = Hash(sHashCheck.begin(), sHashCheck.end());

    if (nHashCheck.GetHex() == "fe65ee46e01d18cebec555978abe82e021e5399171ce470e464996114d72dcf6")
        _log(INFO, "Scraper", "Hash for \"Hello world\" is " + Hash(sHashCheck.begin(), sHashCheck.end()).GetHex() + " and is correct.");
    else
        _log(ERROR, "Scraper", "Hash for \"Hello world\" is " + Hash(sHashCheck.begin(), sHashCheck.end()).GetHex() + " and is NOT correct.");

    // Check to see if the Scraper directory exists and is a directory. If not create it.
    if(fs::exists(pathScraper))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the Scraper directory.
        if(fs::is_regular_file(pathScraper))
        {
            fs::remove(pathScraper);
            fs::create_directory(pathScraper);
        }
        else
        {
            // Load the manifest file from the Scraper directory into mScraperFileManifest.

            _log(INFO, "Scraper", "Loading Manifest");
            if (!LoadScraperFileManifest(pathScraper / "Manifest.csv.gz"))
                _log(ERROR, "Scraper", "Error occurred loading manifest");
            else
                _log(INFO, "Scraper", "Loaded Manifest file into map.");

            // Align the Scraper directory with the Manifest file.
            // First remove orphan files with no Manifest entry.
            // Lock the manifest while it is being manipulated.
            {
                LOCK(cs_StructScraperFileManifest);

                // Check to see if the file exists in the manifest and if the hash matches. If it doesn't
                // remove it.
                ScraperFileManifestMap::iterator entry;

                for (fs::directory_entry& dir : fs::directory_iterator(pathScraper))
                {
                    std::string filename = dir.path().filename().string();

                    if(dir.path().filename() != "Manifest.csv.gz"
                            && dir.path().filename() != "BeaconList.csv.gz"
                            && dir.path().filename() != "Stats.csv.gz"
                            && dir.path().filename() != "ConvergedStats.csv.gz"
                            && fs::is_regular_file(dir))
                    {
                        entry = StructScraperFileManifest.mScraperFileManifest.find(dir.path().filename().string());
                        if (entry == StructScraperFileManifest.mScraperFileManifest.end())
                        {
                            fs::remove(dir.path());
                            _log(WARNING, "Scraper", "Removing orphan file not in Manifest: " + filename);
                            continue;
                        }

                        if (entry->second.hash != GetFileHash(dir))
                        {
                            _log(INFO, "Scraper", "File failed hash check. Removing file.");
                            fs::remove(dir.path());
                        }
                    }
                }

                // Now iterate through the Manifest map and remove entries with no file, or entries and files older than
                // SCRAPER_FILE_RETENTION_TIME, whether they are current or not, and remove non-current files regardless of time
                //if fScraperRetainNonCurrentFiles is false.
                for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
                {
                    ScraperFileManifestMap::iterator entry_copy = entry++;

                    if (!fs::exists(pathScraper / entry_copy->first)
                            || ((GetAdjustedTime() - entry_copy->second.timestamp) > SCRAPER_FILE_RETENTION_TIME)
                            || (!fScraperRetainNonCurrentFiles && entry_copy->second.current == false))
                    {
                        _log(WARNING, "Scraper", "Removing stale or orphan manifest entry: " + entry_copy->first);
                        DeleteScraperFileManifestEntry(entry_copy->second);
                    }
                }
            }
        }
    }
    else
        fs::create_directory(pathScraper);


    uint256 nmScraperFileManifestHash = 0;

    // The scraper thread loop...
    while (fScraperStandalone || !fShutdown)
    {
        gridcoinrpc data;
        
        int64_t sbage = data.sbage();
        int64_t nScraperThreadStartTime = GetAdjustedTime();

        // Give nActiveBeforeSB seconds before superblock needed before we sync
        // Note there is a small while loop here to cull incoming manifests from
        // other scrapers while this one is quiescent. An unlikely but possible
        // situation because the nActiveBeforeSB may be set differently on other
        // scrapers.
        if (sbage <= (86400 - nActiveBeforeSB) && sbage >= 0)
        {
            // Don't let nBeforeSBSleep go less than zero, which could happen without max if wallet
            // started with sbage already older than 86400 - nActiveBeforeSB.
            int64_t nBeforeSBSleep = std::max(86400 - nActiveBeforeSB - sbage, (int64_t) 0);

            _log(INFO, "Scraper", "Superblock not needed. age=" + std::to_string(sbage));

            while (GetAdjustedTime() - nScraperThreadStartTime < nBeforeSBSleep)
            {
                // The only thing we do here while quiescent is cull manifests received
                // from other scrapers.
                ScraperDeleteCScraperManifests();

                _log(INFO, "Scraper", "Sleeping for " + std::to_string(nScraperSleep) +" milliseconds");
                MilliSleep(nScraperSleep);
            }
        }

        else if (sbage <= -1)
            _log(ERROR, "Scraper", "RPC error occured, check logs");

        else
        {
            // Refresh the whitelist if its available
            if (!data.wlimport())
                _log(WARNING, "Scraper", "Refreshing of whitelist failed.. using old data");

            else
                _log(INFO, "Scraper", "Refreshing of whitelist completed");

            // Delete manifest entries not on whitelist. Take a lock on cs_StructScraperFileManifest for this.
            {
                LOCK(cs_StructScraperFileManifest);

                ScraperFileManifestMap::iterator entry;

                for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
                {
                    ScraperFileManifestMap::iterator entry_copy = entry++;

                    // Set flag to false. If entry project matches a whitelist project then mark true and break.
                    bool bOnWhitelist = false;
                    for (const auto& wlproject : vwhitelist)
                    {
                        if(entry_copy->second.project == wlproject.first)
                        {
                            bOnWhitelist = true;
                            break;
                        }
                    }

                    if (!bOnWhitelist)
                    {
                        _log(INFO, "Scraper", "Removing manifest entry for non-whitelisted project: " + entry_copy->first);
                        DeleteScraperFileManifestEntry(entry_copy->second);
                    }
                }
            }

            AuthenticationETagClear();

            // Note a lock on cs_StructScraperFileManifest is taken in StoreBeaconList,
            // and the block hash for the consensus block height is updated in the struct.
            if (!StoreBeaconList(pathScraper / "BeaconList.csv.gz"))
                _log(ERROR, "Scraper", "StoreBeaconList error occurred");
            else
                _log(INFO, "Scraper", "Stored Beacon List");

            DownloadProjectRacFilesByCPID();
        }

        _nntester(INFO, "Scraper", "download size so far: " + std::to_string(ndownloadsize) + " upload size so far: " + std::to_string(nuploadsize));

        ScraperStats mScraperStats = GetScraperStatsByConsensusBeaconList();

        _log(INFO, "Scraper", "mScraperStats has the following number of elements: " + std::to_string(mScraperStats.size()));

        if (!StoreStats(pathScraper / "Stats.csv.gz", mScraperStats))
            _log(ERROR, "Scraper", "StoreStats error occurred");
        else
            _log(INFO, "Scraper", "Stored stats.");


        // This is the section to send out manifests. Only do if authorized.
        if (IsScraperAuthorizedToBroadcastManifests())
        {
            // This will push out a new CScraperManifest if the hash has changed of mScraperFileManifestHash.
            // The idea here is to run the scraper loop a number of times over a period of time approaching
            // the due time for the superblock. A number of stats files only update once per day, but a few update
            // as often as once per hour. If one of these file changes during the run up to the superblock, a
            // new manifest should be published, but the older ones retained up to the SCRAPER_CMANIFEST_RETENTION_TIME limit
            // to provide additional matching options.

            // Get default wallet public key
            std::string sDefaultKey;
            {
                LOCK(pwalletMain->cs_wallet);

                sDefaultKey = pwalletMain->vchDefaultKey.GetID().ToString();

            }

            // Publish and/or local delete CScraperManifests.
            {
                LOCK(cs_StructScraperFileManifest);

                // If the hash doesn't match (a new one is available), or there are none, then publish a new one.
                if (nmScraperFileManifestHash != StructScraperFileManifest.nFileManifestMapHash
                        || !CScraperManifest::mapManifest.size())
                {
                    _log(INFO, "Scraper", "Publishing new CScraperManifest.");
                    ScraperSendFileManifestContents(sDefaultKey);
                }

                nmScraperFileManifestHash = StructScraperFileManifest.nFileManifestMapHash;

                ConvergedManifest StructConvergedManifest;
                BeaconMap mBeaconMap;

                //ScraperConstructConvergedManifest also culls old CScraperManifests.
                if (ScraperConstructConvergedManifest(StructConvergedManifest))
                {
                LoadBeaconListFromConvergedManifest(StructConvergedManifest, mBeaconMap);
                ScraperStats mScraperConvergedStats = GetScraperStatsByConvergedManifest(StructConvergedManifest);

                _log(INFO, "Scraper", "mScraperStats has the following number of elements: " + std::to_string(mScraperConvergedStats.size()));

                if (!StoreStats(pathScraper / "ConvergedStats.csv.gz", mScraperConvergedStats))
                    _log(ERROR, "Scraper", "StoreStats error occurred");
                else
                    _log(INFO, "Scraper", "Stored converged stats.");
                }
            }
        }

        _log(INFO, "Scraper", "Sleeping for " + std::to_string(nScraperSleep) +" milliseconds");
        MilliSleep(nScraperSleep);
    }
}




// This is the non-scraper "neural-network" node thread...
void NeuralNetwork()
{
    _log(INFO, "NeuralNetwork", "Starting Neural Network thread (new C++ implementation).");

    while(!fShutdown)
    {
        // These items are only run in this thread if not handled by the Scraper() thread.
        if (!fScraperActive)
        {
            ConvergedManifest StructConvergedManifest;
            BeaconMap mBeaconMap;

            //ScraperConstructConvergedManifest also culls old CScraperManifests.
            if(ScraperConstructConvergedManifest(StructConvergedManifest))
            {
            LoadBeaconListFromConvergedManifest(StructConvergedManifest, mBeaconMap);
            ScraperStats mScraperConvergedStats = GetScraperStatsByConvergedManifest(StructConvergedManifest);

            _log(INFO, "Scraper", "mScraperStats has the following number of elements: " + std::to_string(mScraperConvergedStats.size()));

            if (!StoreStats(pathScraper / "ConvergedStats.csv.gz", mScraperConvergedStats))
                _log(ERROR, "Scraper", "StoreStats error occurred");
            else
                _log(INFO, "Scraper", "Stored converged stats.");
            }
        }

        // Use the same sleep interval as the scraper. This defaults to 60 seconds.
        _log(INFO, "NeuralNetwork", "Sleeping for " + std::to_string(nScraperSleep) +" milliseconds");

        MilliSleep(nScraperSleep);
    }
}



/**********************
* Sanity              *
**********************/

bool find(const std::string& s, const std::string& find)
{
    size_t pos = s.find(find);

    if (pos == std::string::npos)
        return false;

    else
        return true;
}

std::string urlsanity(const std::string& s, const std::string& type)
{
    stringbuilder url;

    if (find(s, "einstein"))
    {
        url.append(s.substr(0,4));
        url.append("s");
        url.append(s.substr(4, s.size()));
    }

    else
        url.append(s);

    url.append("stats/");
    url.append(type);

    if (find(s, "gorlaeus") || find(s, "worldcommunitygrid"))
        url.append(".xml.gz");

    else
        url.append(".gz");

    return url.value();
}

std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    return s;
}


bool ScraperDirectorySanity()
{
    // Check for the existence and proper form of the Scraper subdirectory (pathScraper), and make if necessary.
    if(fs::exists(pathScraper))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the Scraper directory.
        if(fs::is_regular_file(pathScraper))
        {
            fs::remove(pathScraper);
            fs::create_directory(pathScraper);
        }
    }
    else
        fs::create_directory(pathScraper);

    // Need to implement error handling. For now, returns true.

    return true;
}



/**********************
* Scraper Logger      *
**********************/

void _log(logattribute eType, const std::string& sCall, const std::string& sMessage)
{
    std::string sType;
    std::string sOut;

    try
    {
        switch (eType)
        {
            case INFO:        sType = "INFO";        break;
            case WARNING:     sType = "WARNING";     break;
            case ERROR:       sType = "ERROR";       break;
            case CRITICAL:    sType = "CRITICAL";    break;
        }
    }

    catch (std::exception& ex)
    {
        printf("Logger : exception occured in _log function (%s)\n", ex.what());

        return;
    }

    stringbuilder string;

    string.append(std::to_string(GetAdjustedTime()));
    string.append(" [");
    string.append(sType);
    string.append("] <");
    string.append(sCall);
    string.append("> : ");
    string.append(sMessage);
    sOut = string.value();

    logger log;

    log.output(sOut);

    return;
}

void _nntester(logattribute eType, const std::string& sCall, const std::string& sMessage)
{
    std::string sType;
    std::string sOut;

    try
    {
        switch (eType)
        {
            case INFO:        sType = "INFO";        break;
            case WARNING:     sType = "WARNING";     break;
            case ERROR:       sType = "ERROR";       break;
            case CRITICAL:    sType = "CRITICAL";    break;
        }
    }

    catch (std::exception& ex)
    {
        printf("Logger : exception occured in _log function (%s)\n", ex.what());

        return;
    }

    stringbuilder string;

    string.append(std::to_string(time(NULL)));
    string.append(" [");
    string.append(sType);
    string.append("] <");
    string.append(sCall);
    string.append("> : ");
    string.append(sMessage);
    sOut = string.value();

    nntester log;

    log.output(sOut);

    return;
}


/**********************
* Populate Whitelist  *
**********************/

bool WhitelistPopulated()
{
    if (vwhitelist.empty())
    {
        _log(INFO, "WhitelistPopulated", "Whitelist vector currently empty; populating");

        gridcoinrpc data;

        if (data.wlimport())
            _log(INFO, "WhitelistPopulated", "Successfully populated whitelist vector");

        else
        {
            _log(CRITICAL, "WhitelistPopulated", "Failed to populate whitelist vector; keeping old whitelist data for time being");

            return false;
        }
    }

    _log(INFO, "WhitelistPopulated", "Whitelist is populated; Contains " + std::to_string(vwhitelist.size()) + " projects");

    return true;
}

void AuthenticationETagClear()
{
    fs::path file = fs::current_path() / "auth.dat";

    if (fs::exists(file))
        fs::remove(file);
}

/**********************
* Populate UserPass   *
**********************/

bool UserpassPopulated()
{
    if (vuserpass.empty())
    {
        _log(INFO, "UserpassPopulated", "Userpass vector currently empty; populating");

        userpass up;

        if (up.import())
            _log(INFO, "UserPassPopulated", "Successfully populated userpass vector");

        else
        {
            _log(CRITICAL, "UserPassPopulated", "Failed to populate userpass vector");

            return false;
        }
    }

    _log(INFO, "UserPassPopulated", "Userpass is populated; Contains " + std::to_string(vuserpass.size()) + " projects");

    return true;
}



/**********************
* Project RAC Files   *
**********************/

bool DownloadProjectRacFilesByCPID()
{
    if (!WhitelistPopulated())
    {
        _log(CRITICAL, "DownloadProjectRacFiles", "Whitelist is not populated");

        return false;
    }

    if (!UserpassPopulated())
    {
        _log(CRITICAL, "DownloadProjectRacFiles", "Userpass is not populated");

        return false;
    }

    for (const auto& prjs : vwhitelist)
    {
        _log(INFO, "DownloadProjectRacFiles", "Downloading project file for " + prjs.first);

        std::vector<std::string> vPrjUrl = split(prjs.second, "@");

        std::string sUrl = urlsanity(vPrjUrl[0], "user");

        std::string rac_file_name = prjs.first + +"-user.gz";

        fs::path rac_file = pathScraper / rac_file_name.c_str();

//            if (fs::exists(rac_file))
//                fs::remove(rac_file);

        // Grab ETag of rac file
        statscurl racetagcurl;
        std::string sRacETag;

        bool buserpass = false;
        std::string userpass;

        for (const auto& up : vuserpass)
        {
            if (up.first == prjs.first)
            {
                buserpass = true;

                userpass = up.second;

                break;
            }
        }

        if (buserpass)
        {
            if (!racetagcurl.http_header(sUrl, sRacETag, userpass))
            {
                _log(ERROR, "DownloadProjectRacFiles", "Failed to pull rac header file for " + prjs.first);

                continue;
            }
        }

        else
            if (!racetagcurl.http_header(sUrl, sRacETag))
            {
                _log(ERROR, "DownloadProjectRacFiles", "Failed to pull rac header file for " + prjs.first);

                continue;
            }

        if (sRacETag.empty())
        {
            _log(ERROR, "DownloadProjectRacFiles", "ETag for project is empty" + prjs.first);

            continue;
        }

        else
        _log(INFO, "DownloadProjectRacFiles", "Successfully pulled rac header file for " + prjs.first);

        if (buserpass)
        {
            authdata ad(lowercase(prjs.first));

            ad.setoutputdata("user", prjs.first, sRacETag);

            if (!ad.xport())
                _log(CRITICAL, "DownloadProjectRacFiles", "Failed to export etag for " + prjs.first + " to authentication file");
        }

        std::string chketagfile = prjs.first + "-" + sRacETag + ".csv" + ".gz";
        fs::path chkfile = pathScraper / chketagfile.c_str();

        if (fs::exists(chkfile))
        {
            _log(INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.first + " already exists");

            _nntester(INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.first + " already exists");

            continue;
        }

        else
            fs::remove(chkfile);

        _nntester(INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.first + " already exists");

        statscurl raccurl;

        if (buserpass)
        {
            if (!raccurl.http_download(sUrl, rac_file.string(), userpass))
            {
                _log(ERROR, "DownloadProjectRacFiles", "Failed to download project rac file for " + prjs.first);

                continue;
            }
        }

        else
            if (!raccurl.http_download(sUrl, rac_file.string()))
            {
                _log(ERROR, "DownloadProjectRacFiles", "Failed to download project rac file for " + prjs.first);

                continue;
            }

        ProcessProjectRacFileByCPID(prjs.first, rac_file.string(), sRacETag);
    }

    // After processing, update global structure with the timestamp of the latest file in the manifest.
    {
        LOCK(cs_StructScraperFileManifest);

        int64_t nMaxTime = 0;
        for (const auto& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            nMaxTime = std::max(nMaxTime, entry.second.timestamp);
        }

        StructScraperFileManifest.timestamp = nMaxTime;
    }
    return true;
}




// This version uses a consensus beacon map rather than the teamid to filter statistics.
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag)
{
    // Get a consensus map of Beacons.
    BeaconConsensus Consensus = GetConsensusBeaconList();
    
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(ERROR, "ProcessProjectRacFileByCPID", "Failed to open rac gzip file (" + file.string() + ")");

        return false;
    }

    _log(INFO, "ProcessProjectRacFileByCPID", "Opening rac file (" + file.string() + ")");

    boostio::filtering_istream in;

    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string gzetagfile = "";

    // If einstein we store different
//    if (file.string().find("einstein") != std::string::npos)
//        gzetagfile = "einstein_user.csv.gz";

//    else
        gzetagfile = project + "-" + etag + ".csv" + ".gz";

    std::string gzetagfile_no_path = gzetagfile;
    // Put path in.
    gzetagfile = ((fs::path)(pathScraper / gzetagfile)).string();

    std::ofstream outgzfile(gzetagfile, std::ios_base::out | std::ios_base::binary);
    boostio::filtering_ostream out;
    out.push(boostio::gzip_compressor());
    out.push(outgzfile);

    _log(INFO, "ProcessProjectRacFileByCPID", "Started processing " + file.string());

    std::string line;
    stringbuilder builder;
    out << "# total_credit,expavg_time,expavgcredit,cpid" << std::endl;
    while (std::getline(in, line))
    {
        if(line == "<user>")
            builder.clear();
        else if(line == "</user>")
        {
            const std::string& data = builder.value();
            builder.clear();

            const std::string& cpid = ExtractXML(data, "<cpid>", "</cpid>");
            if (Consensus.mBeaconMap.count(cpid) < 1)
                continue;

            // User beacon verified. Append its statistics to the CSV output.
            out << ExtractXML(data, "<total_credit>", "</total_credit>") << ","
                << ExtractXML(data, "<expavg_time>", "</expavg_time>") << ","
                << ExtractXML(data, "<expavg_credit>", "</expavg_credit>") << ","
                << cpid
                << std::endl;
        }
        else
            builder.append(line);
    }

    // TODO: Error out on stream errors.
    /*
    if (bfileerror)
    {
        _log(CRITICAL, "ProcessProjectRacFileByCPID", "Error in data processing of " + file.string() + "; Aborted processing");

        std::string efile = etag + ".gz";
        fs::path fsepfile = pathScraper/ efile;
        ingzfile.close();
        outgzfile.flush();
        outgzfile.close();

        if (fs::exists(fsepfile))
            fs::remove(fsepfile);

        if (fs::exists(file))
            fs::remove(file);

        return false;
    }*/

    _log(INFO, "ProcessProjectRacFileByCPID", "Finished processing " + file.string());

    ingzfile.close();
    out.flush();
    out.reset();
    outgzfile.close();

    // Hash the file.
    
    uint256 nFileHash = GetFileHash(gzetagfile);
    _log(INFO, "ProcessProjectRacFileByCPID", "FileHash by GetFileHash " + nFileHash.ToString());


    try
    {
    size_t filea = fs::file_size(file);
    fs::path temp = gzetagfile.c_str();
    size_t fileb = fs::file_size(temp);

    _nntester(INFO, "ProcessProjectRacFileByCPID", "Processing new rac file " + file.string() + "(" + std::to_string(filea) + " -> " + std::to_string(fileb) + ")");

    ndownloadsize += (int64_t)filea;
    nuploadsize += (int64_t)fileb;

    }

    catch (fs::filesystem_error& e)
    {
        _log(INFO, "ProcessProjectRacFileByCPID", "FS Error -> " + std::string(e.what()));
    }

    fs::remove(file);

    ScraperFileManifestEntry NewRecord;

    // Don't include path in Manifest, because this is local node dependent.
    NewRecord.filename = gzetagfile_no_path;
    NewRecord.project = project;
    NewRecord.hash = nFileHash;
    NewRecord.timestamp = GetAdjustedTime();
    // By definition the record we are about to insert is current. If a new file is downloaded for
    // a given project, it has to be more up to date than any others.
    NewRecord.current = true;
    
    // Code block to lock StructScraperFileManifest during record insertion and delete because we want this atomic.
    {
        LOCK(cs_StructScraperFileManifest);
        
        // Iterate mScraperFileManifest to find any prior records for the same project and change current flag to false,
        // or delete if older than SCRAPER_FILE_RETENTION_TIME or non-current and fScraperRetainNonCurrentFiles
        // is false.

        ScraperFileManifestMap::iterator entry;
        for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
        {
            ScraperFileManifestMap::iterator entry_copy = entry++;

            if (entry_copy->second.project == project && entry_copy->second.current == true)
            {
                _log(INFO, "ProcessProjectRacFileByCPID", "Marking old project manifest entry as current = false.");
                MarkScraperFileManifestEntryNonCurrent(entry_copy->second);
            }

            // If records are older than SCRAPER_FILE_RETENTION_TIME delete record, or if fScraperRetainNonCurrentFiles is false,
            // delete all non-current records, including the one just marked non-current.
            if (((GetAdjustedTime() - entry_copy->second.timestamp) > SCRAPER_FILE_RETENTION_TIME)
                    || (entry_copy->second.project == project && entry_copy->second.current == false && !fScraperRetainNonCurrentFiles))
            {
                DeleteScraperFileManifestEntry(entry_copy->second);
            }
        }

        if(!InsertScraperFileManifestEntry(NewRecord))
            _log(WARNING, "ProcessProjectRacFileByCPID", "Manifest entry already exists for " + nFileHash.ToString() + " " + gzetagfile);
        else
            _log(INFO, "ProcessProjectRacFileByCPID", "Created manifest entry for " + nFileHash.ToString() + " " + gzetagfile);

        // The below is not an ideal implementation, because the entire map is going to be written out to disk each time.
        // The manifest file is actually very small though, and this primitive implementation will suffice. I could
        // put it up in the while loop above, but then there is a much higher risk that the manifest file could be out of
        // sync if the wallet is ended during the middle of pulling the files.
        _log(INFO, "ProcessProjectRacFileByCPID", "Persisting manifest entry to disk.");
        if(!StoreScraperFileManifest(pathScraper / "Manifest.csv.gz"))
            _log(ERROR, "ProcessProjectRacFileByCPID", "StoreScraperFileManifest error occurred");
        else
            _log(INFO, "ProcessProjectRacFileByCPID", "Stored Manifest");
    }

    _log(INFO, "ProcessProjectRacFileByCPID", "Complete Process");

    return true;
}




uint256 GetFileHash(const fs::path& inputfile)
{
    // open input file, and associate with CAutoFile
    FILE *file = fopen(inputfile.string().c_str(), "rb");
    CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    uint256 nHash = 0;
    
    if (!filein)
        return nHash;

    // use file size to size memory buffer
    int dataSize = boost::filesystem::file_size(inputfile);
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);

    // read data and checksum from file
    try
    {
        filein.read((char *)&vchData[0], dataSize);
    }
    catch (std::exception &e)
    {
        return nHash;
    }

    filein.fclose();

    CDataStream ssFile(vchData, SER_DISK, CLIENT_VERSION);

    nHash = Hash(ssFile.begin(), ssFile.end());

    return nHash;
}


// Note that cs_StructScraperFileManifest needs to be taken before calling.
uint256 GetmScraperFileManifestHash()
{
    uint256 nHash;
    CDataStream ss(SER_NETWORK, 1);

    for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
    {
        ss << entry.second.filename
           << entry.second.project
           << entry.second.hash
           << entry.second.timestamp
           << entry.second.current;
    }

    nHash = Hash(ss.begin(), ss.end());

    return nHash;
}


void testdata(const std::string& etag)
{
    std::ifstream ingzfile( (pathScraper / (fs::path)(etag + ".gz")).string(), std::ios_base::in | std::ios_base::binary);


    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;
    printf("size: %lu\n", in.size());
    while (std::getline(in, line))
    {
        printf("data: %s\n", line.c_str());
    }
}




/***********************
* Persistance          *
************************/

bool LoadBeaconList(const fs::path& file, BeaconMap& mBeaconMap)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(ERROR, "LoadBeaconList", "Failed to open beacon gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;

    int64_t ntimestamp;
    
    // Header -- throw away.
    std::getline(in, line);

    while (std::getline(in, line))
    {
        BeaconEntry LoadEntry;
        std::string key;

        std::vector<std::string> vline = split(line, ",");

        key = vline[0];

        std::istringstream sstimestamp(vline[1]);
        sstimestamp >> ntimestamp;
        LoadEntry.timestamp = ntimestamp;

        LoadEntry.value = vline[2];

        mBeaconMap[key] = LoadEntry;
    }

    return true;
}


bool StoreBeaconList(const fs::path& file)
{
    BeaconConsensus Consensus = GetConsensusBeaconList();
    
    _log(INFO, "StoreBeaconList", "ReadCacheSection element count: " + std::to_string(ReadCacheSection(Section::BEACON).size()));
    _log(INFO, "StoreBeaconList", "mBeaconMap element count: " + std::to_string(Consensus.mBeaconMap.size()));

    // Update block hash for block at consensus height to StructScraperFileManifest.
    // Requires a lock.
    {
        LOCK(cs_StructScraperFileManifest);

        StructScraperFileManifest.nConsensusBlockHash = Consensus.nBlockHash;
    }

    if (fs::exists(file))
        fs::remove(file);

    std::ofstream outgzfile(file.string().c_str(), std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(ERROR, "StoreBeaconList", "Failed to open beacon list gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(INFO, "StoreBeaconList", "Started processing " + file.string());
    
    // Header
    stream << "CPID," << "Time," << "Beacon\n";

    for (auto const& entry : Consensus.mBeaconMap)
    {
        std::string sBeaconEntry = entry.first + "," + std::to_string(entry.second.timestamp) + "," + entry.second.value + "\n";
        stream << sBeaconEntry;
    }

    _log(INFO, "StoreBeaconList", "Finished processing beacon data from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(INFO, "StoreBeaconList", "Process Complete.");

    return true;
}




// Insert entry into Manifest. Note that cs_StructScraperFileManifest needs to be taken before calling.
bool InsertScraperFileManifestEntry(ScraperFileManifestEntry& entry)
{
    // This less readable form is so we know whether the element already existed or not.
    std::pair<ScraperFileManifestMap::iterator, bool> ret;
    {
        ret = StructScraperFileManifest.mScraperFileManifest.insert(std::make_pair(entry.filename, entry));
        // If successful insert, rehash map and record in struct for easy comparison later. If already
        // exists, hash is unchanged.
        if (ret.second)
            StructScraperFileManifest.nFileManifestMapHash = GetmScraperFileManifestHash();
    }

    // True if insert was sucessful, false if entry with key (hash) already exists in map.
    return ret.second;
}

// Delete entry from Manifest and corresponding file if it exists. Note that cs_StructScraperFileManifest needs to be taken before calling.
unsigned int DeleteScraperFileManifestEntry(ScraperFileManifestEntry& entry)
{
    unsigned int ret;

    // Delete corresponding file if it exists.
    if(fs::exists(pathScraper / entry.filename))
        fs::remove(pathScraper /entry.filename);

    ret = StructScraperFileManifest.mScraperFileManifest.erase(entry.filename);

    // If an element was deleted then rehash the map and store hash in struct.
    if (ret)
        StructScraperFileManifest.nFileManifestMapHash = GetmScraperFileManifestHash();

    // Returns number of elements erased, either 0 or 1.
    return ret;
}



// Mark manifest entry non-current. The reason this is encapsulated in a function is
// to  ensure the rehash is done. Note that cs_StructScraperFileManifest needs to be
// taken before calling.
bool MarkScraperFileManifestEntryNonCurrent(ScraperFileManifestEntry& entry)
{
    entry.current = false;

    StructScraperFileManifest.nFileManifestMapHash = GetmScraperFileManifestHash();

    return true;
}


bool LoadScraperFileManifest(const fs::path& file)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(ERROR, "LoadScraperFileManifest", "Failed to open manifest gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;

    ScraperFileManifestEntry LoadEntry;

    int64_t ntimestamp;

    // Header - throw away.
    std::getline(in, line);
    
    while (std::getline(in, line))
    {

        std::vector<std::string> vline = split(line, ",");

        uint256 nhash;
        nhash.SetHex(vline[0].c_str());
        LoadEntry.hash = nhash;

        LoadEntry.current = std::stoi(vline[1]);

        std::istringstream sstimestamp(vline[2]);
        sstimestamp >> ntimestamp;
        LoadEntry.timestamp = ntimestamp;

        LoadEntry.project = vline[3];
        
        LoadEntry.filename = vline[4];

        // Lock cs_StructScraperFileManifest before updating
        // global structure.
        {
            LOCK(cs_StructScraperFileManifest);

            InsertScraperFileManifestEntry(LoadEntry);
        }
    }

    return true;
}


bool StoreScraperFileManifest(const fs::path& file)
{
    if (fs::exists(file))
        fs::remove(file);

    std::ofstream outgzfile(file.string().c_str(), std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(ERROR, "StoreScraperFileManifest", "Failed to open manifest gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(INFO, "StoreScraperFileManifest", "Started processing " + file.string());

    //Lock StructScraperFileManifest during serialize to string.
    {
        LOCK(cs_StructScraperFileManifest);
        
        // Header.
        stream << "Hash," << "Current," << "Time," << "Project," << "Filename\n";
        
        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            uint256 nEntryHash = entry.second.hash;

            std::string sScraperFileManifestEntry = nEntryHash.GetHex() + ","
                   + std::to_string(entry.second.current) + ","
                   + std::to_string(entry.second.timestamp) + ","
                   + entry.second.project + ","
                   + entry.first + "\n";
            stream << sScraperFileManifestEntry;
        }
    }

    _log(INFO, "StoreScraperFileManifest", "Finished processing manifest from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(INFO, "StoreScraperFileManifest", "Process Complete.");

    return true;
}



bool StoreStats(const fs::path& file, const ScraperStats& mScraperStats)
{
    if (fs::exists(file))
        fs::remove(file);

    std::ofstream outgzfile(file.string().c_str(), std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(ERROR, "StoreStats", "Failed to open stats gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(INFO, "StoreStats", "Started processing " + file.string());

    // Header.
    stream << "StatsType," << "Project," << "CPID," << "TC," << "RAT," << "RAC," << "Mag\n";

    for (auto const& entry : mScraperStats)
    {
        // This nonsense is to align the key to the columns of the csv.
        std::string sobjectIDforcsv;

        switch(entry.first.objecttype)
        {
        case statsobjecttype::byCPIDbyProject:
        {
            sobjectIDforcsv = entry.first.objectID;
            break;
        }
        case statsobjecttype::byProject:
        {
            sobjectIDforcsv = entry.first.objectID + ",";
            break;
        }
        case statsobjecttype::byCPID:
        {
            sobjectIDforcsv = "," + entry.first.objectID;
            break;
        }
        case statsobjecttype::NetworkWide:
        {
            sobjectIDforcsv = ",";
            break;
        }
        }

        std::string sScraperStatsEntry = GetTextForstatsobjecttype(entry.first.objecttype) + ","
                + sobjectIDforcsv + ","
                + std::to_string(entry.second.statsvalue.dTC) + ","
                + std::to_string(entry.second.statsvalue.dRAT) + ","
                + std::to_string(entry.second.statsvalue.dRAC) + ","
                + std::to_string(entry.second.statsvalue.dMag) + ","
                + "\n";
        stream << sScraperStatsEntry;
    }

    _log(INFO, "StoreStats", "Finished processing stats from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(INFO, "StoreStats", "Process Complete.");

    return true;
}

/***********************
* Stats Computations   *
************************/



bool LoadProjectFileToStatsByCPID(const std::string& project, const fs::path& file, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(ERROR, "LoadProjectFileToStatsByCPID", "Failed to open project user stats gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    bool bcomplete = false;
    bool bfileerror = false;
    std::vector<std::string> vXML;

    // Lets vector the user blocks
    std::string line;
    double dProjectTC = 0.0;
    double dProjectRAT = 0.0;
    double dProjectRAC = 0.0;
    while (std::getline(in, line))
    {
        if(line[0] == '#')
            continue;

        std::vector<std::string> fields;
        boost::split(fields, line, boost::is_any_of(","), boost::token_compress_on);

        if(fields.size() < 4)
            continue;

        ScraperObjectStats statsentry = {};

        const std::string& sTC = fields[0];
        const std::string& sRAT = fields[1];
        const std::string& sRAC = fields[2];
        const std::string& cpid = fields[3];

        // Replace blank strings with zeros.
        statsentry.statsvalue.dTC = (sTC.empty()) ? 0.0 : std::stod(sTC);
        statsentry.statsvalue.dRAT = (sRAT.empty()) ? 0.0 : std::stod(sRAT);
        statsentry.statsvalue.dRAC = (sRAC.empty()) ? 0.0 : std::stod(sRAC);
        // Mag is dealt with on the second pass... so is left at 0.0 on the first pass.

        statsentry.statskey.objecttype = byCPIDbyProject;
        statsentry.statskey.objectID = project + "," + cpid;

        // Insert stats entry into map by the key.
        mScraperStats[statsentry.statskey] = statsentry;

        // Increment project
        dProjectTC += statsentry.statsvalue.dTC;
        dProjectRAT += statsentry.statsvalue.dRAT;
        dProjectRAC += statsentry.statsvalue.dRAC;
    }

    ingzfile.close();

    _log(INFO, "LoadProjectFileToStatsByCPID", "There are " + std::to_string(mScraperStats.size()) + " CPID entries for " + project);

    // The mScraperStats here is scoped to only this project so we do not need project filtering here.
    ScraperStats::iterator entry;

    for (auto const& entry : mScraperStats)
    {
        ScraperObjectStats statsentry;

        statsentry.statskey = entry.first;
        statsentry.statsvalue.dTC = entry.second.statsvalue.dTC;
        statsentry.statsvalue.dRAT = entry.second.statsvalue.dRAT;
        statsentry.statsvalue.dRAC = entry.second.statsvalue.dRAC;
        statsentry.statsvalue.dMag = MagRound(entry.second.statsvalue.dRAC / dProjectRAC * projectmag);

        // Update map entry with the magnitude.
        mScraperStats[statsentry.statskey] = statsentry;
    }

    // Due to rounding to MAG_ROUND, the actual total project magnitude will not be exactly projectmag,
    // but it should be very close. Roll up project statistics.
    ScraperObjectStats ProjectStatsEntry = {};

    ProjectStatsEntry.statskey.objecttype = byProject;
    ProjectStatsEntry.statskey.objectID = project;

    for (auto const& entry : mScraperStats)
    {
        ProjectStatsEntry.statsvalue.dTC += entry.second.statsvalue.dTC;
        ProjectStatsEntry.statsvalue.dRAT += entry.second.statsvalue.dRAT;
        ProjectStatsEntry.statsvalue.dRAC += entry.second.statsvalue.dRAC;
        ProjectStatsEntry.statsvalue.dMag += entry.second.statsvalue.dMag;
    }

    // Insert project level map entry.
    mScraperStats[ProjectStatsEntry.statskey] = ProjectStatsEntry;

    return true;
}



bool LoadProjectObjectToStatsByCPID(const std::string& project, const CSerializeData& ProjectData, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    boostio::basic_array_source<char> input_source(&ProjectData[0], ProjectData.size());
    boostio::stream<boostio::basic_array_source<char>> ingzss(input_source);

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzss);

    std::vector<std::string> vXML;

    // Lets vector the user blocks
    std::string line;
    double dProjectTC = 0.0;
    double dProjectRAT = 0.0;
    double dProjectRAC = 0.0;
    while (std::getline(in, line))
    {
        if(line[0] == '#')
            continue;

        std::vector<std::string> fields;
        boost::split(fields, line, boost::is_any_of(","), boost::token_compress_on);

        if(fields.size() < 4)
            continue;

        ScraperObjectStats statsentry = {};

        const std::string& sTC = fields[0];
        const std::string& sRAT = fields[1];
        const std::string& sRAC = fields[2];
        const std::string& cpid = fields[3];

        // Replace blank strings with zeros.
        statsentry.statsvalue.dTC = (sTC.empty()) ? 0.0 : std::stod(sTC);
        statsentry.statsvalue.dRAT = (sRAT.empty()) ? 0.0 : std::stod(sRAT);
        statsentry.statsvalue.dRAC = (sRAC.empty()) ? 0.0 : std::stod(sRAC);
        // Mag is dealt with on the second pass... so is left at 0.0 on the first pass.

        statsentry.statskey.objecttype = byCPIDbyProject;
        statsentry.statskey.objectID = project + "," + cpid;

        // Insert stats entry into map by the key.
        mScraperStats[statsentry.statskey] = statsentry;

        // Increment project
        dProjectTC += statsentry.statsvalue.dTC;
        dProjectRAT += statsentry.statsvalue.dRAT;
        dProjectRAC += statsentry.statsvalue.dRAC;
    }

    _log(INFO, "LoadProjectObjectToStatsByCPID", "There are " + std::to_string(mScraperStats.size()) + " CPID entries for " + project);

    // The mScraperStats here is scoped to only this project so we do not need project filtering here.
    ScraperStats::iterator entry;

    for (auto const& entry : mScraperStats)
    {
        ScraperObjectStats statsentry;

        statsentry.statskey = entry.first;
        statsentry.statsvalue.dTC = entry.second.statsvalue.dTC;
        statsentry.statsvalue.dRAT = entry.second.statsvalue.dRAT;
        statsentry.statsvalue.dRAC = entry.second.statsvalue.dRAC;
        statsentry.statsvalue.dMag = MagRound(entry.second.statsvalue.dRAC / dProjectRAC * projectmag);

        // Update map entry with the magnitude.
        mScraperStats[statsentry.statskey] = statsentry;
    }

    // Due to rounding to MAG_ROUND, the actual total project magnitude will not be exactly projectmag,
    // but it should be very close. Roll up project statistics.
    ScraperObjectStats ProjectStatsEntry = {};

    ProjectStatsEntry.statskey.objecttype = byProject;
    ProjectStatsEntry.statskey.objectID = project;

    for (auto const& entry : mScraperStats)
    {
        ProjectStatsEntry.statsvalue.dTC += entry.second.statsvalue.dTC;
        ProjectStatsEntry.statsvalue.dRAT += entry.second.statsvalue.dRAT;
        ProjectStatsEntry.statsvalue.dRAC += entry.second.statsvalue.dRAC;
        ProjectStatsEntry.statsvalue.dMag += entry.second.statsvalue.dMag;
    }

    // Insert project level map entry.
    mScraperStats[ProjectStatsEntry.statskey] = ProjectStatsEntry;

    return true;
}






ScraperStats GetScraperStatsByConsensusBeaconList()
{
    _log(INFO, "GetScraperStatsByConsensusBeaconList", "Beginning stats processing.");

    // Enumerate the count of active projects from the file manifest. Since the manifest is
    // constructed starting with the whitelist, and then using only the current files, this
    // will always be less than or equal to the whitelist count from vwhitelist.
    unsigned int nActiveProjects = 0;
    {
        LOCK(cs_StructScraperFileManifest);

        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            if (entry.second.current)
                nActiveProjects++;
        }
    }
    double dMagnitudePerProject = NEURALNETWORKMULTIPLIER / nActiveProjects;

    //Get the Consensus Beacon map and initialize mScraperStats.
    BeaconConsensus Consensus = GetConsensusBeaconList();

    ScraperStats mScraperStats;

    {
        LOCK(cs_StructScraperFileManifest);

        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {

            if (entry.second.current)
            {
                std::string project = entry.first;
                fs::path file = pathScraper / entry.second.filename;
                ScraperStats mProjectScraperStats;

                _log(INFO, "GetScraperStatsByConsensusBeaconList", "Processing stats for project: " + project);

                LoadProjectFileToStatsByCPID(project, file, dMagnitudePerProject, Consensus.mBeaconMap, mProjectScraperStats);

                // Insert into overall map.
                for (auto const& entry2 : mProjectScraperStats)
                {
                    mScraperStats[entry2.first] = entry2.second;
                }
            }
        }
    }

    // Now are are going to cut across projects and group by CPID.

    //Also track the network wide rollup.
    ScraperObjectStats NetworkWideStatsEntry = {};

    NetworkWideStatsEntry.statskey.objecttype = NetworkWide;
    // ObjectID is blank string for network-wide.
    NetworkWideStatsEntry.statskey.objectID = "";

    for (auto const& beaconentry : Consensus.mBeaconMap)
    {
        ScraperObjectStats CPIDStatsEntry = {};

        CPIDStatsEntry.statskey.objecttype = byCPID;
        CPIDStatsEntry.statskey.objectID = beaconentry.first;

        for (auto const& innerentry : mScraperStats)
        {
            // Only select the individual byCPIDbyProject stats for the selected CPID. Leave out the project rollup (byProj) ones,
            // otherwise dimension mixing will result.

            std::string objectID = innerentry.first.objectID;

            std::size_t found = objectID.find(CPIDStatsEntry.statskey.objectID);

            if (innerentry.first.objecttype == byCPIDbyProject && found!=std::string::npos)
            {
                CPIDStatsEntry.statsvalue.dTC += innerentry.second.statsvalue.dTC;
                CPIDStatsEntry.statsvalue.dRAT += innerentry.second.statsvalue.dRAT;
                CPIDStatsEntry.statsvalue.dRAC += innerentry.second.statsvalue.dRAC;
                CPIDStatsEntry.statsvalue.dMag += innerentry.second.statsvalue.dMag;
            }
        }

        // Insert the byCPID entry into the overall map.
        mScraperStats[CPIDStatsEntry.statskey] = CPIDStatsEntry;

        // Increement the network wide stats.
        NetworkWideStatsEntry.statsvalue.dTC += CPIDStatsEntry.statsvalue.dTC;
        NetworkWideStatsEntry.statsvalue.dRAT += CPIDStatsEntry.statsvalue.dRAT;
        NetworkWideStatsEntry.statsvalue.dRAC += CPIDStatsEntry.statsvalue.dRAC;
        NetworkWideStatsEntry.statsvalue.dMag += CPIDStatsEntry.statsvalue.dMag;
    }

    // Insert the (single) network-wide entry into the overall map.
    mScraperStats[NetworkWideStatsEntry.statskey] = NetworkWideStatsEntry;

    _log(INFO, "GetScraperStatsByConsensusBeaconList", "Completed stats processing");

    return mScraperStats;
}


ScraperStats GetScraperStatsByConvergedManifest(ConvergedManifest& StructConvergedManifest)
{
    _log(INFO, "GetScraperStatsByConvergedManifest", "Beginning stats processing.");

    // Enumerate the count of active projects from the converged manifest. The part 0, which
    // is the beacon list, is not a project, which is why there is a -1.
    unsigned int nActiveProjects = StructConvergedManifest.ConvergedManifestPartsMap.size() - 1;
    _log(INFO, "GetScraperStatsByConvergedManifest", "Number of active projects in converged manifest = " + std::to_string(nActiveProjects));

    double dMagnitudePerProject = NEURALNETWORKMULTIPLIER / nActiveProjects;

    //Get the Consensus Beacon map and initialize mScraperStats.
    BeaconMap mBeaconMap;
    LoadBeaconListFromConvergedManifest(StructConvergedManifest, mBeaconMap);

    ScraperStats mScraperStats;

    for (auto entry = StructConvergedManifest.ConvergedManifestPartsMap.begin(); entry != StructConvergedManifest.ConvergedManifestPartsMap.end(); ++entry)
    {
        std::string project = entry->first;
        ScraperStats mProjectScraperStats;

        // Do not process the BeaconList itself as a project stats file.
        if(project != "BeaconList")
        {
            _log(INFO, "GetScraperStatsByConvergedManifest", "Processing stats for project: " + project);

            LoadProjectObjectToStatsByCPID(project, entry->second, dMagnitudePerProject, mBeaconMap, mProjectScraperStats);

            // Insert into overall map.
            for (auto const& entry2 : mProjectScraperStats)
            {
                mScraperStats[entry2.first] = entry2.second;
            }
        }
    }

    // Now are are going to cut across projects and group by CPID.

    //Also track the network wide rollup.
    ScraperObjectStats NetworkWideStatsEntry = {};

    NetworkWideStatsEntry.statskey.objecttype = NetworkWide;
    // ObjectID is blank string for network-wide.
    NetworkWideStatsEntry.statskey.objectID = "";

    for (auto const& beaconentry : mBeaconMap)
    {
        ScraperObjectStats CPIDStatsEntry = {};

        CPIDStatsEntry.statskey.objecttype = byCPID;
        CPIDStatsEntry.statskey.objectID = beaconentry.first;

        for (auto const& innerentry : mScraperStats)
        {
            // Only select the individual byCPIDbyProject stats for the selected CPID. Leave out the project rollup (byProj) ones,
            // otherwise dimension mixing will result.

            std::string objectID = innerentry.first.objectID;

            std::size_t found = objectID.find(CPIDStatsEntry.statskey.objectID);

            if (innerentry.first.objecttype == byCPIDbyProject && found!=std::string::npos)
            {
                CPIDStatsEntry.statsvalue.dTC += innerentry.second.statsvalue.dTC;
                CPIDStatsEntry.statsvalue.dRAT += innerentry.second.statsvalue.dRAT;
                CPIDStatsEntry.statsvalue.dRAC += innerentry.second.statsvalue.dRAC;
                CPIDStatsEntry.statsvalue.dMag += innerentry.second.statsvalue.dMag;
            }
        }

        // Insert the byCPID entry into the overall map.
        mScraperStats[CPIDStatsEntry.statskey] = CPIDStatsEntry;

        // Increement the network wide stats.
        NetworkWideStatsEntry.statsvalue.dTC += CPIDStatsEntry.statsvalue.dTC;
        NetworkWideStatsEntry.statsvalue.dRAT += CPIDStatsEntry.statsvalue.dRAT;
        NetworkWideStatsEntry.statsvalue.dRAC += CPIDStatsEntry.statsvalue.dRAC;
        NetworkWideStatsEntry.statsvalue.dMag += CPIDStatsEntry.statsvalue.dMag;
    }

    // Insert the (single) network-wide entry into the overall map.
    mScraperStats[NetworkWideStatsEntry.statskey] = NetworkWideStatsEntry;

    _log(INFO, "GetScraperStatsByConsensusBeaconList", "Completed stats processing");

    return mScraperStats;
}


/***********************
* Scraper networking   *
************************/

bool ScraperSaveCScraperManifestToFiles(uint256 nManifestHash)
{
    // Make sure the Scraper directory itself exists, because this function could be called from outside
    // the scraper thread loop, and therefore the directory may not have been set up yet.
    ScraperDirectorySanity();

    fs::path savepath = pathScraper / "incoming";

    // Check to see if the Scraper incoming directory exists and is a directory. If not create it.
    if(fs::exists(savepath))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the Scraper directory.
        if(fs::is_regular_file(savepath))
        {
            fs::remove(savepath);
            fs::create_directory(savepath);
        }
    }
    else
        fs::create_directory(savepath);


    // Select manifest based on provided hash.
    auto pair = CScraperManifest::mapManifest.find(nManifestHash);
    const CScraperManifest& manifest = *pair->second;

    // Write out to files the parts. Note this assumes one-to-one part to file. Needs to
    // be fixed for more than one part per file.
    int iPartNum = 0;
    for(const auto& iter : manifest.vParts)
    {
        std::string outputfile;
        fs::path outputfilewpath;

        if (iPartNum == 0)
            outputfile = "BeaconList.csv.gz";
        else
            outputfile = manifest.projects[iPartNum-1].project + "-" + manifest.projects[iPartNum-1].ETag + ".csv.gz";

        outputfilewpath = savepath / outputfile;

        std::ofstream outfile(outputfilewpath.string().c_str(), std::ios_base::out | std::ios_base::binary);

        if (!outfile)
        {
            _log(ERROR, "ScraperSaveCScraperManifestToFiles", "Failed to open file (" + outputfile + ")");

            return false;
        }

        outfile.write((const char*)iter->data.data(), iter->data.size());

        outfile.flush();
        outfile.close();

        iPartNum++;
    }

    return true;
}


bool IsScraperAuthorizedToBroadcastManifests()
{
    // Stub for check against public key in AppCache to authorize sending manifests.
    return true;
}


// A lock needs to be taken on cs_StructScraperFileManifest for this function.
bool ScraperSendFileManifestContents(std::string sCManifestName)
{
    // This "broadcasts" the current ScraperFileManifest contents to the network.

    auto manifest = std::unique_ptr<CScraperManifest>(new CScraperManifest());

    // This should be replaced with the proper field name...
    manifest->sCManifestName = sCManifestName;

    manifest->nTime = StructScraperFileManifest.timestamp;
    manifest->ConsensusBlock = StructScraperFileManifest.nConsensusBlockHash;

    // This will have to be changed to support files bigger than 32 MB, where more than one
    // part per object will be required.
    int iPartNum = 0;

    // Read in BeaconList
    fs::path inputfile = "BeaconList.csv.gz";
    fs::path inputfilewpath = pathScraper / inputfile;
    
    // open input file, and associate with CAutoFile
    FILE *file = fopen(inputfilewpath.string().c_str(), "rb");
    CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);

    if (!filein)
    {
        _log(ERROR, "ScraperSendFileManifestContents", "Failed to open file (" + inputfile.string() + ")");
        return false;
    }

    // use file size to size memory buffer
    int dataSize = boost::filesystem::file_size(inputfilewpath);
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);

    // read data from file
    try
    {
        filein.read((char *)&vchData[0], dataSize);
    }
    catch (std::exception &e)
    {
        _log(ERROR, "ScraperSendFileManifestContents", "Failed to read file (" + inputfile.string() + ")");
        return false;
    }

    filein.fclose();

    // The first part number will be the BeaconList.
    manifest->BeaconList = iPartNum;
    manifest->BeaconList_c = 0;

    CDataStream part(vchData, SER_NETWORK, 1);

    manifest->addPartData(std::move(part));

    iPartNum++;

    for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
    {
        // If SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES is false, only include current files to send across the network.
        if (!SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES && !entry.second.current)
            continue;

        fs::path inputfile = entry.first;

        //_log(INFO, "ScraperSendFileManifestContents", "Input file for CScraperManifest is " + inputfile.string());

        fs::path inputfilewpath = pathScraper / inputfile;

        // open input file, and associate with CAutoFile
        FILE *file = fopen(inputfilewpath.string().c_str(), "rb");
        CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);

        if (!filein)
        {
            _log(ERROR, "ScraperSendFileManifestContents", "Failed to open file (" + inputfile.string() + ")");
            return false;
        }

        // use file size to size memory buffer
        int dataSize = boost::filesystem::file_size(inputfilewpath);
        std::vector<unsigned char> vchData;
        vchData.resize(dataSize);

        // read data from file
        try
        {
            filein.read((char *)&vchData[0], dataSize);
        }
        catch (std::exception &e)
        {
            _log(ERROR, "ScraperSendFileManifestContents", "Failed to read file (" + inputfile.string() + ")");
            return false;
        }

        filein.fclose();


        CScraperManifest::dentry ProjectEntry;

        ProjectEntry.project = entry.second.project;
        std::string sProject = entry.second.project + "-";

        std::string sinputfile = inputfile.string();
        std::string suffix = ".csv.gz";

        std::string ETag = sinputfile.erase(sinputfile.find(sProject), sProject.length());
        ETag = sinputfile.erase(sinputfile.find(suffix), suffix.length());

        ProjectEntry.ETag = ETag;

        ProjectEntry.LastModified = entry.second.timestamp;

        // For now each object will only have one part.
        ProjectEntry.part1 = iPartNum;
        ProjectEntry.partc = 0;
        ProjectEntry.GridcoinTeamID = -1; //Not used anymore

        ProjectEntry.current = entry.second.current;

        ProjectEntry.last = 1;

        manifest->projects.push_back(ProjectEntry);

        CDataStream part(vchData, SER_NETWORK, 1);

        manifest->addPartData(std::move(part));

        iPartNum++;
    }

    // Sign and "send".
    CKey key;
    std::vector<unsigned char> vchPrivKey = ParseHex(msMasterMessagePrivateKey);
    key.SetPrivKey(CPrivKey(vchPrivKey.begin(),vchPrivKey.end()));

    CScraperManifest::addManifest(std::move(manifest), key);

    return true;
}

// ------------------------------------ This an out parameter.
bool ScraperConstructConvergedManifest(ConvergedManifest& StructConvergedManifest)
{
    bool bConvergenceSuccessful = false;

    // Call ScraperDeleteCScraperManifests() to ensure we have culled old manifests. This will
    // return a map of manifests binned by Scraper after the culling.
    // TODO: Put locking around CScraperManifest::mapManifest, because technically, a new one
    // could appear at any moment, even between the below call and the following code. This should
    // really be atomic. Note that without the lock, the worst that could happen is that a scraper
    // could publish (and the node receive) a new manifest after the below call, which means an extra
    // manifest would be in the map from a scraper.
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper = ScraperDeleteCScraperManifests();
    
    // Do a map for unique manifest times ordered by descending time then content hash.
    std::multimap<int64_t, uint256, greater<int64_t>> mManifestsBinnedByTime;
    // and also by content hash, then scraperID and manifest (not content) hash.
    std::multimap<uint256, std::pair<ScraperID, uint256>> mManifestsBinnedbyContent;
    std::multimap<uint256, std::pair<ScraperID, uint256>>::iterator convergence;
    
    unsigned int nScraperCount = mMapCSManifestsBinnedByScraper.size();

    _log(INFO, "ScraperConstructConvergedManifest", "Number of Scrapers with manifests = " + std::to_string(nScraperCount));

    for (const auto& iter : mMapCSManifestsBinnedByScraper)
    {
        // iter.second is the mCSManifest
        for (const auto& iter_inner : iter.second)
        {
            // Insert into mManifestsBinnedByTime multimap. Iter_inner.first is the manifest time,
            // iter_inner.second.second is the manifest CONTENT hash.
            mManifestsBinnedByTime.insert(std::make_pair(iter_inner.first, iter_inner.second.second));

            // Even though this is a multimap on purpose because we are going to count occurances of the same key,
            // We need to prevent the insertion of a second entry with the same content from the same scraper. This
            // could otherwise happen if a scraper is shutdown and restarted, and it publishes a new manifest
            // before it receives manifests from the other nodes (including its own prior manifests).

            auto range = mManifestsBinnedbyContent.equal_range(iter_inner.second.second);
            bool bAlreadyExists = false;
            for (auto iter3 = range.first; iter3 != range.second; ++iter3)
            {
                // ---- ScraperID ------ Candidate scraperID to insert
                if (iter3->second.first == iter.first)
                    bAlreadyExists = true;
            }

            if (!bAlreadyExists)
            {
                // Insert into mManifestsBinnedbyContent ------------- content hash --------------------- ScraperID ------ manifest hash.
                mManifestsBinnedbyContent.insert(std::make_pair(iter_inner.second.second, std::make_pair(iter.first, iter_inner.second.first)));
                _log(INFO, "ScraperConstructConvergedManifest", "mManifestsBinnedbyContent insert "
                     + iter_inner.second.second.GetHex() + ", " + iter.first + ", " + iter_inner.second.first.GetHex());
            }
        }
    }
    
    // Walk the time map (backwards in time because the sort order is descending), and select the first
    // manifest content hash that meets the convergence rule.
    for (const auto& iter : mManifestsBinnedByTime)
    {
        // Notice the below is NOT using the time. We switch to the content only. The time is only used to make sure
        // we test the convergence of the manifests in time order, but once a content hash is selected based on the time,
        // only the content hash is used to count occurrences in the multimap, because the times for the same
        // content hash manifest will be different across different scrapers.
        unsigned int nIdenticalContentManifestCount = mManifestsBinnedbyContent.count(iter.second);
        if (nIdenticalContentManifestCount >= NumScrapersForSupermajority(nScraperCount))
        {
            // Find the first one of equivalent content manifests.
            convergence = mManifestsBinnedbyContent.find(iter.second);

            _log(INFO, "ScraperConstructConvergedManifest", "Found convergence on manifest " + convergence->second.second.GetHex()
                 + " at " + std::to_string(iter.first)
                 + " with " + std::to_string(nIdenticalContentManifestCount) + " scrapers out of " + std::to_string(nScraperCount)
                 + " agreeing.");

            bConvergenceSuccessful = true;

            break;
        }
    }

    if (bConvergenceSuccessful)
    {
        // Select agreed upon (converged) CScraper manifest based on converged hash.
        auto pair = CScraperManifest::mapManifest.find(convergence->second.second);
        const CScraperManifest& manifest = *pair->second;

        // Fill out the ConvergedManifest structure. Note this assumes one-to-one part to project statistics BLOB. Needs to
        // be fixed for more than one part per BLOB. This is easy in this case, because it is all from/referring to one manifest.

        StructConvergedManifest.nContentHash = convergence->first;
        StructConvergedManifest.ConsensusBlock = manifest.ConsensusBlock;
        StructConvergedManifest.timestamp = GetAdjustedTime();
        StructConvergedManifest.bByParts = false;

        int iPartNum = 0;
        CDataStream ss(SER_NETWORK,1);
        WriteCompactSize(ss, manifest.vParts.size());
        uint256 nContentHashCheck;

        for(const auto& iter : manifest.vParts)
        {
            std::string sProject;

            if (iPartNum == 0)
                sProject = "BeaconList";
            else
                sProject = manifest.projects[iPartNum-1].project;

            // Copy the parts data into the map keyed by project.
            StructConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair(sProject, iter->data));

            // Serialize the hash to doublecheck the content hash.
            ss << iter->hash;

            iPartNum++;
        }
        ss << StructConvergedManifest.ConsensusBlock;

        nContentHashCheck = Hash(ss.begin(), ss.end());

        if (nContentHashCheck != StructConvergedManifest.nContentHash)
        {
            bConvergenceSuccessful = false;
            _log(ERROR, "ScraperConstructConvergedManifest", "Selected Converged Manifest content hash check failed! nContentHashCheck = "
                 + nContentHashCheck.GetHex() + " and nContentHash = " + StructConvergedManifest.nContentHash.GetHex());
        }
    }


    // TODO: Write the part level convergence routine. The above will only work reasonably well if
    // SCRAPER_CMANIFEST_RETAIN_NONCURRENT = true (which keeps a history of whole manifests around, and
    // SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES = false (which only includes current files in the manifests).
    // This may be the simpler way to go rather than flipping the above flags and matching at the part level,
    // And since the parts are not actually duplicated between manifests if they have the same hash (they are referenced),
    // the only additional overhead is the map entry and fields at the CScraperManifest level, which is small compared to the
    // parts themselves.

    if(!bConvergenceSuccessful)
        _log(INFO, "ScraperConstructConvergedManifest", "No convergence on manifests by content at the manifest level.");

    return bConvergenceSuccessful;
    
}



mmCSManifestsBinnedByScraper BinCScraperManifestsByScraper()
{
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper;

    // Make use of the ordered element feature of above map to bin by scraper and then order by manifest time.
    for (auto iter = CScraperManifest::mapManifest.begin(); iter != CScraperManifest::mapManifest.end(); ++iter)
    {
        CScraperManifest& manifest = *iter->second;

        std::string sManifestName = manifest.sCManifestName;
        int64_t nTime = manifest.nTime;
        uint256 nHash = *manifest.phash;
        uint256 nContentHash = manifest.nContentHash;

        mCSManifest mManifestInner;

        auto BinIter = mMapCSManifestsBinnedByScraper.find(sManifestName);

        // No scraper bin yet - so new insert.
        if (BinIter == mMapCSManifestsBinnedByScraper.end())
        {
            mManifestInner.insert(std::make_pair(nTime, std::make_pair(nHash, nContentHash)));
            mMapCSManifestsBinnedByScraper.insert(std::make_pair(sManifestName, mManifestInner));
        }
        else
        {
            mManifestInner = BinIter->second;
            mManifestInner.insert(std::make_pair(nTime, std::make_pair(nHash, nContentHash)));
            std::swap(mManifestInner, BinIter->second);
        }
    }
    
    return mMapCSManifestsBinnedByScraper;
}


mmCSManifestsBinnedByScraper ScraperDeleteCScraperManifests()
{
    // Apply the SCRAPER_CMANIFEST_RETAIN_NONCURRENT bool and if false delete any existing
    // CScraperManifests other than the current one for each scraper.
    // TODO: Locking around CScraperManifest map.

    _log(INFO, "ScraperDeleteCScraperManifests", "Deleting old CScraperManifests.");

    // Bin by scraper and order by manifest time within scraper bin.
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper = BinCScraperManifestsByScraper();

    _log(INFO, "ScraperDeleteCScraperManifests", "mMapCSManifestsBinnedByScraper size = " + std::to_string(mMapCSManifestsBinnedByScraper.size()));

    if (!SCRAPER_CMANIFEST_RETAIN_NONCURRENT)
    {
        // For each scraper, delete every manifest EXCEPT the latest.
        for (auto iter = mMapCSManifestsBinnedByScraper.begin(); iter != mMapCSManifestsBinnedByScraper.end(); ++iter)
        {
            mCSManifest mManifestInner = iter->second;

            _log(INFO, "ScraperDeleteCScraperManifests", "mManifestInner size = " + std::to_string(mManifestInner.size()) +
                 " for " + iter->first + " scraper");

            // This preserves the LATEST CScraperManifest entry for the given scraper, because the inner map is in descending order,
            // and the first element is therefore the LATEST, and is skipped.
            for (auto iter_inner = ++mManifestInner.begin(); iter_inner != mManifestInner.end(); ++iter_inner)
            {
                
                _log(INFO, "ScraperDeleteCScraperManifests", "Deleting non-current manifest " + iter_inner->second.first.GetHex()
                     + " from scraper source " + iter->first);
                
                // Delete from CScraperManifest map
                ScraperDeleteCScraperManifest(iter_inner->second.first);
            }
        }
    }

    // If any CScraperManifest has exceeded SCRAPER_CMANIFEST_RETENTION_TIME, then
    // delete.
    for (auto iter = CScraperManifest::mapManifest.begin(); iter != CScraperManifest::mapManifest.end(); )
    {
        // Copy iterator for deletion operation and increment iterator.
        auto iter_copy = iter;

        CScraperManifest& manifest = *iter->second;
        
        if (GetAdjustedTime() - manifest.nTime > SCRAPER_CMANIFEST_RETENTION_TIME)
        {
            _log(INFO, "Scraper", "Deleting old CScraperManifest with hash " + iter->first.GetHex());
            // Delete from CScraperManifest map
            ScraperDeleteCScraperManifest(iter_copy->first);
        }
        
        ++iter;
    }

    // Reload mMapCSManifestsBinnedByScraper after deletions. This is not particularly efficient, but the map is not
    // that large.
    mMapCSManifestsBinnedByScraper = BinCScraperManifestsByScraper();
    return mMapCSManifestsBinnedByScraper;
}



// ---------------------------------------------- In ---------------------------------------- Out
bool LoadBeaconListFromConvergedManifest(ConvergedManifest& StructConvergedManifest, BeaconMap& mBeaconMap)
{
    // Find the beacon list.
    auto iter = StructConvergedManifest.ConvergedManifestPartsMap.find("BeaconList");

    boostio::basic_array_source<char> input_source(&iter->second[0], iter->second.size());
    boostio::stream<boostio::basic_array_source<char>> ingzss(input_source);

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzss);

    std::string line;

    int64_t ntimestamp;

    // Header -- throw away.
    std::getline(in, line);

    while (std::getline(in, line))
    {
        BeaconEntry LoadEntry;
        std::string key;

        std::vector<std::string> vline = split(line, ",");

        key = vline[0];

        std::istringstream sstimestamp(vline[1]);
        sstimestamp >> ntimestamp;
        LoadEntry.timestamp = ntimestamp;

        LoadEntry.value = vline[2];

        mBeaconMap[key] = LoadEntry;
    }

    _log(INFO, "LoadBeaconListFromConvergedManifest", "mBeaconMap element count: " + std::to_string(mBeaconMap.size()));

    return true;
}




bool ScraperDeleteCScraperManifest(uint256 nManifestHash)
{
    // This deletes a manifest.
    // Note this just deletes the local copy. TODO: Implement manifest delete message.
    
    // Select manifest based on provided hash.
    auto pair = CScraperManifest::mapManifest.find(nManifestHash);
    CScraperManifest& manifest = *pair->second;

    bool ret = manifest.DeleteManifest(nManifestHash);

    return ret;
}



UniValue sendscraperfilemanifest(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 1 )
        throw std::runtime_error(
                "sendscraperfilemanifest <name>\n"
                "Send a CScraperManifest object with the ScraperFileManifest.\n"
                );

    bool ret = ScraperSendFileManifestContents(params[0].get_str());

    return UniValue(ret);
}



UniValue savescraperfilemanifest(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 1 )
        throw std::runtime_error(
                "savescraperfilemanifest <hash>\n"
                "Send a CScraperManifest object with the ScraperFileManifest.\n"
                );

    bool ret = ScraperSaveCScraperManifestToFiles(uint256(params[0].get_str()));

    return UniValue(ret);
}


UniValue deletecscrapermanifest(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 1 )
        throw std::runtime_error(
                "deletecscrapermanifest <hash>\n"
                "delete manifest object.\n"
                );
    bool ret = ScraperDeleteCScraperManifest(uint256(params[0].get_str()));
    return UniValue(ret);
}


