#include "scraper.h"

extern bool fShutdown;

bool find(const std::string& s, const std::string& find);
std::string urlsanity(const std::string& s, const std::string& type);
std::string lowercase(std::string s);
std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);
std::vector<std::string> vXMLData(const std::string& xmldata, int64_t teamid);
int64_t teamid(const std::string& xmldata);
Manifest mManifest;

CCriticalSection cs_mManifest;

bool WhitelistPopulated();
bool UserpassPopulated();
bool DownloadProjectRacFilesByCPID();
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag);
bool AuthenticationETagUpdate(const std::string& project, const std::string& etag);
void AuthenticationETagClear();

// No header storage by day number gz
// bool xDownloadProjectTeamFiles();
// int64_t xProcessProjectTeamFile(const fs::path& file);
// bool xDownloadProjectRacFiles();
// bool xProcessProjectRacFile(const fs::path& file, int64_t teamid);
int GetDayOfYear(int64_t timestamp);

void testdata(const std::string& etag);

// Note these three are initialized here for standalone mode compile, but will be overwritten by
// the values from GetArg in init.cpp when compiled as part of the wallet.
unsigned int nScraperSleep = 60000;
unsigned int nActiveBeforeSB = 300;
boost::filesystem::path pathScraper = fs::current_path() / "Scraper";

extern void MilliSleep(int64_t n);
extern BeaconMap GetConsensusBeaconList();

// This is the scraper thread...
void Scraper(bool fScraperStandalone)
{
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
            // Load the manifest file from the Scraper directory info mManifest.

            _log(INFO, "Scraper", "Loading Manifest");
            if (!LoadManifest(pathScraper / "Manifest.csv.gz"))
                _log(ERROR, "Scraper", "Error occurred loading manifest");
            else
                _log(INFO, "Scraper", "Loaded Manifest file into map.");

            // Align the Scraper directory with the Manifest file.
            // First remove orphan files with no Manifest entry.
            // Lock the manifest while it is being manipulated.
            {
                LOCK(cs_mManifest);
                
                for (fs::directory_entry& dir : fs::directory_iterator(pathScraper))
                {
                    // Check to see if the file exists in the manifest. If it doesn't
                    // remove it.
                    Manifest::iterator entry;

                    std::string filename = dir.path().filename().c_str();

                    if(dir.path().filename() != "Manifest.csv.gz" && dir.path().filename() != "BeaconList.csv.gz" && fs::is_regular_file(dir))
                    {
                        _log(INFO, "Scraper", "Checking files in Scraper directory to see if in Manifest: " + filename);
                        
                        entry = mManifest.find(dir.path().filename().c_str());
                        if (entry == mManifest.end())
                        {
                            fs::remove(dir.path());
                            _log(WARNING, "Scraper", "Removing orphan file not in Manifest: " + filename);
                        }
                    }
                }
            }

            // Now iterate through the Manifest map and remove Manifest entries with no file.
            for (auto const& entry : mManifest)
            {
                _log(INFO, "Scraper", "Checking mManifest entries to see if corresponding file is present in Scraper directory: " + entry.first);
                
                if(!fs::exists(pathScraper / entry.first))
                {
                    _log(WARNING, "Scraper", "Removing orphan mManifest entry: " + entry.first);
                    
                    mManifest.erase(entry.first);
                }
            }
        }
     }
    else
        fs::create_directory(pathScraper);


    // The scraper thread loop...
    while (fScraperStandalone || !fShutdown)
    {
        gridcoinrpc data;
        
        int64_t sbage = data.sbage();

        // Give nActiveBeforeSB seconds before superblock needed before we sync
        if (sbage <= (86400 - nActiveBeforeSB) && sbage >= 0)
        {
            _log(INFO, "Scraper", "Superblock not needed. age=" + std::to_string(sbage));

            // Don't let nBeforeSBSleep go less than zero, which could happen without max if wallet
            // started with sbage already older than 86400 - nActiveBeforeSB.
            int64_t nBeforeSBSleep = std::max(86400 - nActiveBeforeSB - sbage, (int64_t) 0);
            _log(INFO, "Scraper", "Sleeping for " + std::to_string(nBeforeSBSleep) + " seconds.");

            MilliSleep(nBeforeSBSleep * 1000);
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

            AuthenticationETagClear();

            if (!StoreBeaconList(pathScraper / "BeaconList.csv.gz"))
                _log(ERROR, "Scraper", "StoreBeaconList error occurred");
            else
                _log(INFO, "Scraper", "Stored Beacon List");

            DownloadProjectRacFilesByCPID();
        }

        _nntester(INFO, "Scraper", "download size so far: " + std::to_string(ndownloadsize) + " upload size so far: " + std::to_string(nuploadsize));

        _log(INFO, "Scraper", "Sleeping for " + std::to_string(nScraperSleep) +" milliseconds");

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


std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end)
{

    std::string extraction = "";
    std::string::size_type loc = XMLdata.find( key, 0 );

    if(loc != std::string::npos)
    {
        std::string::size_type loc_end = XMLdata.find(key_end, loc+3);

        if (loc_end != std::string::npos)
            extraction = XMLdata.substr(loc+(key.length()),loc_end-loc-(key.length()));
    }

    return extraction;
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

    string.append(std::to_string(time(NULL)));
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

        std::string chketagfile = prjs.first + "-" + sRacETag + "-ByCPID" + ".gz";
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

    return true;
}




// This version uses a consensus beacon map rather than the teamid to filter statistics.
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag)
{
    // Get a consensus map of Beacons.
    BeaconMap mBeaconMap = GetConsensusBeaconList();
    
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
//        gzetagfile = "einstein_user-ByCPID.gz";

//    else
        gzetagfile = project + "-" + etag + "-ByCPID" + ".gz";

    std::string gzetagfile_no_path = gzetagfile;
    // Put path in.
    gzetagfile = ((fs::path)(pathScraper / gzetagfile)).c_str();

    std::ofstream outgzfile(gzetagfile, std::ios_base::out | std::ios_base::binary);

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(INFO, "ProcessProjectRacFileByCPID", "Started processing " + file.string());

    stringbuilder outdata;
    outdata.nlappend("<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>");
    stream << outdata.value();

    bool bcomplete = false;
    bool bfileerror = false;
    std::vector<std::string> vXML;

    while (!bcomplete && !bfileerror)
    {
        // Find users block
        std::string line;

        while (std::getline(in, line))
        {
            if (line != "<users>")
                continue;

            else
                break;
        }
        // <users> block found
        // Lets vector the  <user> blocks

        while (std::getline(in, line))
        {
            if (bcomplete)
                break;

            if (line == "</users>")
            {
                bcomplete = true;

                break;
            }

            if (line == "<user>")
            {
                stringbuilder userdata;

                userdata.nlappend(line);

                while (std::getline(in, line))
                {
                    if (line == "</user>")
                    {
                        userdata.nlappend(line);

                        // This uses the beacon map rather than the teamid to select the statistics.
                        if (mBeaconMap.count(ExtractXML(userdata.value(), "<cpid>", "</cpid>")) >= 1)
                            vXML.push_back(userdata.value());

                        break;
                    }

                    userdata.nlcleanappend(line);
                }
            }
        }

        // If the file is complete/incomplete we will reach here. however bcomplete would be true to break this outter most loop.
        // In case where we reach here but the bool for bcomplete is not true then the file was incomplete!
        if (!bcomplete)
            bfileerror = true;
    }

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
    }

    _log(INFO, "ProcessProjectRacFileByCPID", "Finished processing " + file.string());
    _log(INFO, "ProcessProjectRacFileByCPID", "Started processing CPID rac data and stripping");

    // Strip the rac xml data to contain only why we need to be compressed from vector
    // We Need:
    // total_credit, expavg_time, expavg_credit, teamid, cpid
    // We Don't need:
    // id, country, name, url
    // In reality we DO NOT need total_credit till TCD
    // In reality we DO NOT need expavg_time but this will make nn compatible
    // I've also opted to save 1 byte a line by not doing newlines since this a scraper

    stringbuilder xmlout;

    xmlout.nlappend("<users>");

    for (auto const& vv : vXML)
    {
        std::string stotal_credit = ExtractXML(vv, "<total_credit>", "</total_credit>");
        std::string sexpavg_time = ExtractXML(vv, "<expavg_time>", "</expavg_time>");
        std::string sexpavg_credit = ExtractXML(vv, "<expavg_credit>", "</expavg_credit>");
        std::string scpid = ExtractXML(vv, "<cpid>", "</cpid>");

        xmlout.nlappend("<user>");
        xmlout.xmlappend("total_credit", stotal_credit);
        xmlout.xmlappend("expavg_time", sexpavg_time);
        xmlout.xmlappend("expavg_credit", sexpavg_credit);
        xmlout.xmlappend("cpid", scpid);
        xmlout.nlappend("</user>");
    }

    xmlout.nlappend("</users>");

    stream << xmlout.value();

    _log(INFO, "ProcessProjectRacFileByCPID", "Finished processing team rac data; stripping complete");

    out.push(stream);

    // Copy out to file.
    boost::iostreams::copy(out, outgzfile);

    ingzfile.close();
    outgzfile.flush();
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

    ManifestEntry ManifestEntry;

    // Don't include path in Manifest, because this is local node dependent.
    ManifestEntry.filename = gzetagfile_no_path;
    ManifestEntry.hash = nFileHash;
    ManifestEntry.timestamp = GetAdjustedTime();

    if(!InsertManifestEntry(ManifestEntry))
        _log(WARNING, "ProcessProjectRacFileByCPID", "Manifest entry already exists for " + nFileHash.ToString() + " " + gzetagfile);
    else
        _log(INFO, "ProcessProjectRacFileByCPID", "Created manifest entry for " + nFileHash.ToString() + " " + gzetagfile);

    // The below is not an ideal implementation, because the entire map is going to be written out to disk each time.
    // The manifest file is actually very small though, and this primitive implementation will suffice. I could
    // put it up in the while loop above, but then there is a much higher risk that the manifest file could be out of
    // sync if the wallet is ended during the middle of pulling the files.
    _log(INFO, "Scraper", "Persisting manifest entry to disk.");
    if(!StoreManifest(pathScraper / "Manifest.csv.gz"))
        _log(ERROR, "Scraper", "StoreManifest error occurred");
    else
        _log(INFO, "Scraper", "Stored Manifest");

    _log(INFO, "ProcessProjectRacFileByCPID", "Complete Process");

    return true;
}





uint256 GetFileHash(const fs::path& inputfile)
{
    // open input file, and associate with CAutoFile
    FILE *file = fopen(inputfile.string().c_str(), "rb");
    CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!filein)
        return error("FileHash() : open failed");

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(inputfile);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if ( dataSize < 0 ) dataSize = 0;
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);

    filein.fclose();

    CDataStream ssFile(vchData, SER_DISK, CLIENT_VERSION);

    uint256 nHash = Hash(ssFile.begin(), ssFile.end());

    return nHash;
}





void testdata(const std::string& etag)
{
    std::ifstream ingzfile( (pathScraper / (fs::path)(etag + ".gz")).c_str(), std::ios_base::in | std::ios_base::binary);


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


bool StoreBeaconList(const fs::path& file)
{
    BeaconMap mBeaconMap = GetConsensusBeaconList();
    
    _log(INFO, "StoreBeaconList", "ReadCacheSection element count: " + std::to_string(ReadCacheSection("beacon").size()));
    _log(INFO, "StoreBeaconList", "mBeaconMap element count: " + std::to_string(mBeaconMap.size()));

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

    for (auto const& entry : mBeaconMap)
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




/***********************
* Manifest persistance *
************************/


// Insert entry into Manifest
bool InsertManifestEntry(ManifestEntry entry)
{
    // This less readable form is so we know whether the element already existed or not.
    std::pair<Manifest::iterator,bool> ret;
    {
        LOCK(cs_mManifest);
        
        ret = mManifest.insert(std::make_pair(entry.filename, entry));
    }

    // True if insert was sucessful, false if entry with key (hash) already exists in map.
    return ret.second;
}

// Delete entry from Manifest
unsigned int DeleteManifestEntry(ManifestEntry entry)
{
    unsigned int ret;
    {
        LOCK(cs_mManifest);

        ret = mManifest.erase(entry.filename);
    }

    // Returns number of elements erased, either 0 or 1.
    return ret;
}



bool LoadManifest(const fs::path& file)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(ERROR, "LoadManifest", "Failed to open manifest gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;

    ManifestEntry LoadEntry;

    int64_t ntimestamp;

    while (std::getline(in, line))
    {

        std::vector<std::string> vline = split(line, ",");

        //std::istringstream sshash(vline[0]);
        //sshash >> nhash;
        //LoadEntry.hash = nhash;
        
        uint256 nhash;
        nhash.SetHex(vline[0].c_str());
        LoadEntry.hash = nhash;

        std::istringstream sstimestamp(vline[1]);
        sstimestamp >> ntimestamp;
        LoadEntry.timestamp = ntimestamp;
        
        LoadEntry.filename = vline[2];

        InsertManifestEntry(LoadEntry);
    }

    return true;
}


bool StoreManifest(const fs::path& file)
{
    if (fs::exists(file))
        fs::remove(file);

    std::ofstream outgzfile(file.string().c_str(), std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(ERROR, "StoreManifest", "Failed to open manifest gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(INFO, "StoreManifest", "Started processing " + file.string());

    //Lock mManifest during serialize to string.
    {
        LOCK(cs_mManifest);
        
        for (auto const& entry : mManifest)
        {
            uint256 nEntryHash = entry.second.hash;

            std::string sManifestEntry = nEntryHash.GetHex() + "," + std::to_string(entry.second.timestamp) + "," + entry.first + "\n";
            stream << sManifestEntry;
        }
    }

    _log(INFO, "StoreManifest", "Finished processing manifest from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(INFO, "StoreManifest", "Process Complete.");

    return true;
}
