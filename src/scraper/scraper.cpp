#include "scraper.h"
#include "scraper_net.h"
#include "http.h"
#include "ui_interface.h"

#include <zlib.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/serialization/binary_object.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/exception/exception.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>

fs::path pathScraper = fs::current_path() / "Scraper";

extern bool fShutdown;
extern bool fDebug;
extern bool fDebug3;
extern std::string msMasterMessagePrivateKey;
extern CWallet* pwalletMain;
bool fScraperActive = false;
std::vector<std::pair<std::string, std::string>> vwhitelist;
std::vector<std::pair<std::string, std::string>> vuserpass;
std::vector<std::pair<std::string, int64_t>> vprojectteamids;
std::vector<std::string> vauthenicationetags;
int64_t ndownloadsize = 0;
int64_t nuploadsize = 0;

enum class logattribute
{
    // Can't use ERROR here because it is defined already in windows.h.
    ERR,
    INFO,
    WARNING,
    CRITICAL
};

struct ScraperFileManifestEntry
{
    std::string filename; // Filename
    std::string project;
    uint256 hash; // hash of file
    int64_t timestamp;
    bool current;
};

typedef std::map<std::string, ScraperFileManifestEntry> ScraperFileManifestMap;

struct ScraperFileManifest
{
    ScraperFileManifestMap mScraperFileManifest;
    uint256 nFileManifestMapHash;
    uint256 nConsensusBlockHash;
    int64_t timestamp;
};

// --------------- project -------------team name -- teamID
typedef std::map<std::string, std::map<std::string, int64_t>> mTeamIDs;
mTeamIDs TeamIDMap;

std::string urlsanity(const std::string& s, const std::string& type);
std::string lowercase(std::string s);
std::string ExtractXML(const std::string XMLdata, const std::string key, const std::string key_end);
ScraperFileManifest StructScraperFileManifest = {};

// Global cache for converged scraper stats. Access must be through a lock.
ConvergedScraperStats ConvergedScraperStatsCache = {};

CCriticalSection cs_Scraper;
CCriticalSection cs_vwhitelist;
CCriticalSection cs_StructScraperFileManifest;
CCriticalSection cs_ConvergedScraperStatsCache;
CCriticalSection cs_TeamIDMap;

void _log(logattribute eType, const std::string& sCall, const std::string& sMessage);

template<typename T>
void ApplyCache(const std::string& key, T& result);

void ScraperApplyAppCacheEntries();
void Scraper(bool bSingleShot = false);
void ScraperSingleShot();
bool ScraperHousekeeping();
bool WhitelistPopulated();
bool UserpassPopulated();
bool ScraperDirectoryAndConfigSanity();
bool StoreBeaconList(const fs::path& file);
bool StoreTeamIDList(const fs::path& file);
bool LoadBeaconList(const fs::path& file, BeaconMap& mBeaconMap);
bool LoadBeaconListFromConvergedManifest(ConvergedManifest& StructConvergedManifest, BeaconMap& mBeaconMap);
bool LoadTeamIDList(const fs::path& file);
std::vector<std::string> split(const std::string& s, const std::string& delim);
uint256 GetmScraperFileManifestHash();
bool StoreScraperFileManifest(const fs::path& file);
bool LoadScraperFileManifest(const fs::path& file);
bool InsertScraperFileManifestEntry(ScraperFileManifestEntry& entry);
unsigned int DeleteScraperFileManifestEntry(ScraperFileManifestEntry& entry);
bool MarkScraperFileManifestEntryNonCurrent(ScraperFileManifestEntry& entry);
ScraperStats GetScraperStatsByConsensusBeaconList();
bool LoadProjectFileToStatsByCPID(const std::string& project, const fs::path& file, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool LoadProjectObjectToStatsByCPID(const std::string& project, const CSerializeData& ProjectData, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool ProcessProjectStatsFromStreamByCPID(const std::string& project, boostio::filtering_istream& sUncompressedIn,
                                         const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool ProcessNetworkWideFromProjectStats(BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool StoreStats(const fs::path& file, const ScraperStats& mScraperStats);
bool ScraperSaveCScraperManifestToFiles(uint256 nManifestHash);
bool ScraperSendFileManifestContents(CBitcoinAddress& Address, CKey& Key);
mmCSManifestsBinnedByScraper BinCScraperManifestsByScraper();
mmCSManifestsBinnedByScraper ScraperDeleteCScraperManifests();
unsigned int ScraperDeleteUnauthorizedCScraperManifests();
bool ScraperDeleteCScraperManifest(uint256 nManifestHash);
bool ScraperConstructConvergedManifest(ConvergedManifest& StructConvergedManifest);
bool ScraperConstructConvergedManifestByProject(std::vector<std::pair<std::string, std::string>> &vwhitelist_local,
                                                mmCSManifestsBinnedByScraper& mMapCSManifestsBinnedByScraper, ConvergedManifest& StructConvergedManifest);
std::string GenerateSBCoreDataFromScraperStats(ScraperStats& mScraperStats);
// Overloaded. See alternative in scraper.h.
std::string ScraperGetNeuralHash(std::string sNeuralContract);

bool DownloadProjectTeamFiles();
bool ProcessProjectTeamFile(const fs::path& file, const std::string& etag, std::map<std::string, int64_t>& mTeamIdsForProject_out);
bool DownloadProjectRacFilesByCPID();
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag,  BeaconConsensus& Consensus);
bool AuthenticationETagUpdate(const std::string& project, const std::string& etag);
void AuthenticationETagClear();

extern void MilliSleep(int64_t n);
extern BeaconConsensus GetConsensusBeaconList();

// For compatibility this will be used for the contract hashing to allow easy
// roll-in of the new NN, because the Quorum functions look for the empty string
// hash value and that will not be invariant if a hash function switch is done now.
extern std::string GetQuorumHash(const std::string& data);
extern std::string UnpackBinarySuperblock(std::string sBlock);
extern std::string PackBinarySuperblock(std::string sBlock);

/**********************
* Scraper Logger      *
**********************/


class logger
{
private:

    //std::ofstream logfile;
    fs::ofstream logfile;

public:

    logger()
    {
        fs::path plogfile = GetDataDir() / "scraper.log";

        logfile.open(plogfile.c_str(), std::ios_base::out | std::ios_base::app);

        if (!logfile.is_open())
            printf("Logging : Failed to open logging file\n");
    }

    ~logger()
    {
        if (logfile.is_open())
        {
            logfile.flush();
            logfile.close();
        }
    }

    void output(const std::string& tofile)
    {
        if (logfile.is_open())
            logfile << tofile << std::endl;

        return;
    }
};


void _log(logattribute eType, const std::string& sCall, const std::string& sMessage)
{
    std::string sType;
    std::string sOut;

    try
    {
        switch (eType)
        {
        case logattribute::INFO:        sType = "INFO";        break;
        case logattribute::WARNING:     sType = "WARNING";     break;
        case logattribute::ERR:         sType = "ERROR";       break;
        case logattribute::CRITICAL:    sType = "CRITICAL";    break;
        }
    }

    catch (std::exception& ex)
    {
        printf("Logger : exception occured in _log function (%s)\n", ex.what());

        return;
    }

    sOut = tfm::format("%s [%s] <%s> : %s", DateTimeStrFormat("%x %H:%M:%S", GetAdjustedTime()), sType, sCall, sMessage);
    logger log;

    log.output(sOut);

    // Send to UI for log window.
    uiInterface.NotifyScraperEvent(scrapereventtypes::Log, CT_NEW, sOut);

    LogPrintf(std::string(sType + ": Scraper: <" + sCall + ">: %s").c_str(), sMessage);

    return;
}




/**********************
* String Builder EXP  *
**********************/

class stringbuilder
{
protected:

    std::stringstream builtstring;

public:

    void append(const std::string &value)
    {
        builtstring << value;
    }

    void append(double value)
    {
        builtstring << value;
    }

    void append(int64_t value)
    {
        builtstring << value;
    }

    void fixeddoubleappend(double value, unsigned int precision)
    {
        builtstring << std::fixed << std::setprecision(precision) << value;
    }

    void nlappend(const std::string& value)
    {
        builtstring << value << "\n";
    }

    std::string value()
    {
        // Prevent a memory leak
        const std::string& out = builtstring.str();

        return out;
    }

    size_t size()
    {
        const std::string& out = builtstring.str();
        return out.size();
    }

    void clear()
    {
        builtstring.clear();
        builtstring.str(std::string());
    }
};


/*********************
* Whitelist Data     *
*********************/

class gridcoinrpc
{
private:

    std::string whitelistrpcdata = "";
    std::string superblockage = "";

public:

    gridcoinrpc()
    {}

    ~gridcoinrpc()
    {}

    bool wlimport()
    {
        LOCK(cs_vwhitelist);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_vwhitelist");

        vwhitelist.clear();

        for (const auto& item : ReadCacheSection(Section::PROJECT))
            vwhitelist.push_back(std::make_pair(item.first, item.second.value));

        // End LOCK(cs_vwhitelist).
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_vwhitelist");

        return true;
    }

    int64_t sbage()
    {
        int64_t superblock_time = ReadCache(Section::SUPERBLOCK, "magnitudes").timestamp;
        int64_t nSBage = GetAdjustedTime() - superblock_time;

        return nSBage;
    }
};

/*********************
* Userpass Data      *
*********************/

class userpass
{
private:

    fs::ifstream userpassfile;

public:

    userpass()
    {
        vuserpass.clear();

        fs::path plistfile = GetDataDir() / "userpass.dat";

        userpassfile.open(plistfile.c_str(), std::ios_base::in);

        if (!userpassfile.is_open())
            _log(logattribute::CRITICAL, "userpass_data", "Failed to open userpass file");
    }

    ~userpass()
    {
        if (userpassfile.is_open())
            userpassfile.close();
    }

    bool import()
    {
        vuserpass.clear();
        std::string inputdata;

        try
        {
            while (std::getline(userpassfile, inputdata))
            {
                std::vector<std::string>vlist = split(inputdata, ";");

                vuserpass.push_back(std::make_pair(vlist[0], vlist[1]));
            }

            _log(logattribute::INFO, "userpass_data_import", "Userpass contains " + std::to_string(vuserpass.size()) + " projects");

            return true;
        }

        catch (std::exception& ex)
        {
            _log(logattribute::CRITICAL, "userpass_data_import", "Failed to userpass import due to exception (" + std::string(ex.what()) + ")");

            return false;
        }
    }
};

/*********************
* Auth Data          *
*********************/

class authdata
{
private:

    fs::ofstream oauthdata;
    stringbuilder outdata;

public:

    authdata(const std::string& project)
    {
        std::string outfile = project + "_auth.dat";
        fs::path poutfile = GetDataDir() / outfile.c_str();

        oauthdata.open(poutfile.c_str(), std::ios_base::out | std::ios_base::app);

        if (!oauthdata.is_open())
            _log(logattribute::CRITICAL, "auth_data", "Failed to open auth data file");
    }

    ~authdata()
    {
        if (oauthdata.is_open())
            oauthdata.close();
    }

    void setoutputdata(const std::string& type, const std::string& name, const std::string& etag)
    {
        outdata.clear();
        outdata.append("<auth><etag>");
        outdata.append(etag);
        outdata.append("</etag>");
        outdata.append("<type>");
        outdata.append(type);
        outdata.nlappend("</type></auth>");
    }

    bool xport()
    {
        try
        {
            if (outdata.size() == 0)
            {
                _log(logattribute::CRITICAL, "user_data_export", "No authentication etags to be exported!");

                return false;
            }

            oauthdata.write(outdata.value().c_str(), outdata.size());

            _log(logattribute::INFO, "auth_data_export", "Exported");

            return true;
        }

        catch (std::exception& ex)
        {
            _log(logattribute::CRITICAL, "auth_data_export", "Failed to export auth data due to exception (" + std::string(ex.what()) + ")");

            return false;
        }
    }
};


template<typename T>
void ApplyCache(const std::string& key, T& result)
{
    // Local reference to avoid double lookup.
    const auto& entry = ReadCache(Section::PROTOCOL, key);

    // If the entry has an empty string (no value) then leave the original undisturbed.
    if (entry.value.empty())
        return;

    try
    {
        if (std::is_same<T, double>::value)
            result = stod(entry.value);
        else if (std::is_same<T, int64_t>::value)
            result = atoi64(entry.value);
        else if (std::is_same<T, unsigned int>::value)
            // Throw away (zero out) negative integer
            // This approach limits the range to the non-negative signed int, but that is good enough.
            result = (unsigned int)std::max(0, stoi(entry.value));
        else if (std::is_same<T, bool>::value)
        {
            if (entry.value == "false" || entry.value == "0")
                result = boost::lexical_cast<T>(false);
            else if (entry.value == "true" || entry.value == "1")
                result = boost::lexical_cast<T>(true);
            else
                throw std::invalid_argument("Argument not true or false");
        }
        else
            result = boost::lexical_cast<T>(entry.value);
    }
    catch (const std::exception&)
    {
        _log(logattribute::ERR, "ScraperApplyAppCacheEntries", tfm::format("Ignoring bad AppCache entry for %s.", key));
    }
}


void ScraperApplyAppCacheEntries()
{
    // If there are AppCache entries for the defaults in scraper.h override them. For the first two, this will also
    // override any GetArgs supplied from the command line, which is appropriate as network policy should take precedence.

    ApplyCache("scrapersleep", nScraperSleep);
    ApplyCache("activebeforesb", nActiveBeforeSB);

    ApplyCache("SCRAPER_RETAIN_NONCURRENT_FILES", SCRAPER_RETAIN_NONCURRENT_FILES);
    ApplyCache("SCRAPER_FILE_RETENTION_TIME", SCRAPER_FILE_RETENTION_TIME);
    ApplyCache("SCRAPER_CMANIFEST_RETAIN_NONCURRENT", SCRAPER_CMANIFEST_RETAIN_NONCURRENT);
    ApplyCache("SCRAPER_CMANIFEST_RETENTION_TIME", SCRAPER_CMANIFEST_RETENTION_TIME);
    ApplyCache("SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES", SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES);
    ApplyCache("MAG_ROUND", MAG_ROUND);
    ApplyCache("NEURALNETWORKMULTIPLIER", NEURALNETWORKMULTIPLIER);
    ApplyCache("CPID_MAG_LIMIT", CPID_MAG_LIMIT);
    ApplyCache("SCRAPER_CONVERGENCE_MINIMUM", SCRAPER_CONVERGENCE_MINIMUM);
    ApplyCache("SCRAPER_CONVERGENCE_RATIO", SCRAPER_CONVERGENCE_RATIO);
    ApplyCache("CONVERGENCE_BY_PROJECT_RATIO", CONVERGENCE_BY_PROJECT_RATIO);
    ApplyCache("ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD", ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD);
    ApplyCache("SCRAPER_MISBEHAVING_NODE_BANSCORE", SCRAPER_MISBEHAVING_NODE_BANSCORE);
    ApplyCache("REQUIRE_TEAM_WHITELIST_MEMBERSHIP", REQUIRE_TEAM_WHITELIST_MEMBERSHIP);
    ApplyCache("TEAM_WHITELIST", TEAM_WHITELIST);
    ApplyCache("SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD", SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD);

    if (fDebug)
    {
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "scrapersleep = " + std::to_string(nScraperSleep));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "activebeforesb = " + std::to_string(nActiveBeforeSB));

        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_RETAIN_NONCURRENT_FILES = " + std::to_string(SCRAPER_RETAIN_NONCURRENT_FILES));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_FILE_RETENTION_TIME = " + std::to_string(SCRAPER_FILE_RETENTION_TIME));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CMANIFEST_RETAIN_NONCURRENT = " + std::to_string(SCRAPER_CMANIFEST_RETAIN_NONCURRENT));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CMANIFEST_RETENTION_TIME = " + std::to_string(SCRAPER_CMANIFEST_RETENTION_TIME));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES = " + std::to_string(SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "MAG_ROUND = " + std::to_string(MAG_ROUND));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "NEURALNETWORKMULTIPLIER = " + std::to_string(NEURALNETWORKMULTIPLIER));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "CPID_MAG_LIMIT = " + std::to_string(CPID_MAG_LIMIT));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CONVERGENCE_MINIMUM = " + std::to_string(SCRAPER_CONVERGENCE_MINIMUM));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CONVERGENCE_RATIO = " + std::to_string(SCRAPER_CONVERGENCE_RATIO));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "CONVERGENCE_BY_PROJECT_RATIO = " + std::to_string(CONVERGENCE_BY_PROJECT_RATIO));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD = " + std::to_string(ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_MISBEHAVING_NODE_BANSCORE = " + std::to_string(SCRAPER_MISBEHAVING_NODE_BANSCORE));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "REQUIRE_TEAM_WHITELIST_MEMBERSHIP = " + std::to_string(REQUIRE_TEAM_WHITELIST_MEMBERSHIP));
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "TEAM_WHITELIST = " + TEAM_WHITELIST);
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD = " + std::to_string(SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD));

        AppCacheSection mScrapers = ReadCacheSection(Section::SCRAPER);

        _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "For information - authorized scraper address list");
        for (auto const& entry : mScrapers)
        {
            _log(logattribute::INFO, "ScraperApplyAppCacheEntries",
                 "Scraper entry: " + entry.first + ", " + entry.second.value + ", " + DateTimeStrFormat("%x %H:%M:%S", entry.second.timestamp));
        }
    }
}



// This is the "main" scraper function.
// It will be instantiated as a separate thread if -scraper is specified as a startup argument,
// and operate in a continuous while loop.
// It can also be called in "single shot" mode.
void Scraper(bool bSingleShot)
{
    if (!bSingleShot)
        _log(logattribute::INFO, "Scraper", "Starting Scraper thread.");
    else
        _log(logattribute::INFO, "Scraper", "Running in single shot mode.");

    // This is necessary to maintain compatibility with Windows.
    pathScraper.imbue(std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>()));

    // Hash check
    std::string sHashCheck = "Hello world";
    uint256 nHashCheck = Hash(sHashCheck.begin(), sHashCheck.end());

    if (nHashCheck.GetHex() == "fe65ee46e01d18cebec555978abe82e021e5399171ce470e464996114d72dcf6")
        _log(logattribute::INFO, "Scraper", "Hash for \"Hello world\" is " + Hash(sHashCheck.begin(), sHashCheck.end()).GetHex() + " and is correct.");
    else
        _log(logattribute::ERR, "Scraper", "Hash for \"Hello world\" is " + Hash(sHashCheck.begin(), sHashCheck.end()).GetHex() + " and is NOT correct.");

    uint256 nmScraperFileManifestHash = 0;

    // The scraper thread loop...
    while (!fShutdown)
    {
        // Only proceed if wallet is in sync. Check every 5 seconds since no callback is available.
        // We do NOT want to filter statistics with an out-of-date beacon list or project whitelist.
        // If called in singleshot mode, wallet will most likely be in sync, because the calling functions check
        // beforehand.
        while (OutOfSyncByAge())
        {
            _log(logattribute::INFO, "Scraper", "Wallet not in sync. Sleeping for 8 seconds.");
            MilliSleep(8000);
        }

        {
            LOCK(cs_nSyncTime);
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_nSyncTime");

            nSyncTime = GetAdjustedTime();

            _log(logattribute::INFO, "Scraper", "Wallet is in sync. Continuing.");

            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_nSyncTime");
        }

        // Now that we are in sync, refresh from the AppCache and check for proper directory/file structure.
        // Also delete any unauthorized CScraperManifests received before the wallet was in sync.
        {
            LOCK(cs_Scraper);
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_Scraper");

            ScraperDirectoryAndConfigSanity();
            // UnauthorizedCScraperManifests should only be seen on the first invocation after getting in sync
            // See the comment on the function.

            LOCK(CScraperManifest::cs_mapManifest);
            if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

            ScraperDeleteUnauthorizedCScraperManifests();

            // End LOCK(CScraperManifest::cs_mapManifest)
            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

            // End LOCK(cs_Scraper)
            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
        }

        gridcoinrpc data;
        
        int64_t sbage = data.sbage();
        int64_t nScraperThreadStartTime = GetAdjustedTime();

        // Give nActiveBeforeSB seconds before superblock needed before we sync
        // Note there is a small while loop here to cull incoming manifests from
        // other scrapers while this one is quiescent. An unlikely but possible
        // situation because the nActiveBeforeSB may be set differently on other
        // scrapers.
        // If Scraper is called in singleshot mode, then skip the wait.
        if (!bSingleShot && sbage <= (86400 - nActiveBeforeSB) && sbage >= 0)
        {
            // Don't let nBeforeSBSleep go less than zero, which could happen without max if wallet
            // started with sbage already older than 86400 - nActiveBeforeSB.
            int64_t nBeforeSBSleep = std::max(86400 - nActiveBeforeSB - sbage, (int64_t) 0);

            while (GetAdjustedTime() - nScraperThreadStartTime < nBeforeSBSleep)
            {
                // Take a lock on the whole scraper for this...
                {
                    LOCK(cs_Scraper);
                    if (fDebug) _log(logattribute::INFO, "LOCK", "cs_Scraper");

                    // The only things we do here while quiescent
                    ScraperDirectoryAndConfigSanity();
                    ScraperDeleteCScraperManifests();

                    // End LOCK(cs_Scraper)
                    if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
                }

                sbage = data.sbage();
                _log(logattribute::INFO, "Scraper", "Superblock not needed. age=" + std::to_string(sbage));
                _log(logattribute::INFO, "Scraper", "Sleeping for " + std::to_string(nScraperSleep / 1000) +" seconds");
                
                MilliSleep(nScraperSleep);
            }
        }

        else if (sbage <= -1)
            _log(logattribute::ERR, "Scraper", "RPC error occured, check logs");

        else
        {
            // Take a lock on cs_Scraper for the main activity portion of the loop.
            LOCK(cs_Scraper);
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_Scraper");

            // Refresh the whitelist if its available
            if (!data.wlimport())
                _log(logattribute::WARNING, "Scraper", "Refreshing of whitelist failed.. using old data");

            else
                _log(logattribute::INFO, "Scraper", "Refreshing of whitelist completed");

            // Delete manifest entries not on whitelist. Take a lock on cs_StructScraperFileManifest for this.
            {
                LOCK(cs_StructScraperFileManifest);
                if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");

                ScraperFileManifestMap::iterator entry;

                for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
                {
                    ScraperFileManifestMap::iterator entry_copy = entry++;

                    // Set flag to false. If entry project matches a whitelist project then mark true and break.
                    bool bOnWhitelist = false;
                    std::vector<std::pair<std::string, std::string>> vwhitelist_local {};
                    {
                        LOCK(cs_vwhitelist);
                        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_vwhitelist");

                        vwhitelist_local = vwhitelist;
                        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_vwhitelist");
                    }

                    for (const auto& wlproject : vwhitelist_local)
                    {
                        if (entry_copy->second.project == wlproject.first)
                        {
                            bOnWhitelist = true;
                            break;
                        }
                    }

                    if (!bOnWhitelist)
                    {
                        _log(logattribute::INFO, "Scraper", "Removing manifest entry for non-whitelisted project: " + entry_copy->first);
                        DeleteScraperFileManifestEntry(entry_copy->second);
                    }
                }

                // End LOCK(cs_StructScraperFileManifest)
                if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
            }

            AuthenticationETagClear();

            // Note a lock on cs_StructScraperFileManifest is taken in StoreBeaconList,
            // and the block hash for the consensus block height is updated in the struct.
            if (!StoreBeaconList(pathScraper / "BeaconList.csv.gz"))
                _log(logattribute::ERR, "Scraper", "StoreBeaconList error occurred");
            else
                _log(logattribute::INFO, "Scraper", "Stored Beacon List");

            // If team filtering is set by policy then pull down and retrieve team ID's as needed. This loads the TeamIDMap global.
            // Note that the call(s) to ScraperDirectoryAndConfigSanity() above will preload the team ID map from the persisted file
            // if it exists, so this will minimize the work that DownloadProjectTeamFiles() has to do.
            if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP)
                DownloadProjectTeamFiles();

            DownloadProjectRacFilesByCPID();

            // End LOCK(cs_Scraper)
            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
        }

        _log(logattribute::INFO, "Scraper", "download size so far: " + std::to_string(ndownloadsize) + " upload size so far: " + std::to_string(nuploadsize));

        ScraperStats mScraperStats = GetScraperStatsByConsensusBeaconList();

        _log(logattribute::INFO, "Scraper", "mScraperStats has the following number of elements: " + std::to_string(mScraperStats.size()));

        if (!StoreStats(pathScraper / "Stats.csv.gz", mScraperStats))
            _log(logattribute::ERR, "Scraper", "StoreStats error occurred");
        else
            _log(logattribute::INFO, "Scraper", "Stored stats.");

        // Signal stats event to UI.
        uiInterface.NotifyScraperEvent(scrapereventtypes::Stats, CT_NEW, {});

        // This is the section to send out manifests. Only do if authorized.
        CBitcoinAddress AddressOut;
        CKey KeyOut;
        if (IsScraperAuthorizedToBroadcastManifests(AddressOut, KeyOut))
        {
            // This will push out a new CScraperManifest if the hash has changed of mScraperFileManifestHash.
            // The idea here is to run the scraper loop a number of times over a period of time approaching
            // the due time for the superblock. A number of stats files only update once per day, but a few update
            // as often as once per hour. If one of these file changes during the run up to the superblock, a
            // new manifest should be published, but the older ones retained up to the SCRAPER_CMANIFEST_RETENTION_TIME limit
            // to provide additional matching options.

            // Publish and/or local delete CScraperManifests.
            {
                LOCK2(cs_StructScraperFileManifest, CScraperManifest::cs_mapManifest);
                if (fDebug) _log(logattribute::INFO, "LOCK2", "cs_StructScraperFileManifest, CScraperManifest::cs_mapManifest");


                // If the hash doesn't match (a new one is available), or there are none, then publish a new one.
                if (nmScraperFileManifestHash != StructScraperFileManifest.nFileManifestMapHash
                        || !CScraperManifest::mapManifest.size())
                {
                    _log(logattribute::INFO, "Scraper", "Publishing new CScraperManifest.");

                    // scraper key used for signing is the key returned by IsScraperAuthorizedToBroadcastManifests(KeyOut).
                    if (ScraperSendFileManifestContents(AddressOut, KeyOut))
                        uiInterface.NotifyScraperEvent(scrapereventtypes::Manifest, CT_NEW, {});
                }

                nmScraperFileManifestHash = StructScraperFileManifest.nFileManifestMapHash;

                // End LOCK(cs_StructScraperFileManifest)
                if (fDebug) _log(logattribute::INFO, "ENDLOCK2", "cs_StructScraperFileManifest, CScraperManifest::cs_mapManifest");
            }
        }

        // Don't do this if called with singleshot, because ScraperGetNeuralContract will be done afterwards by
        // the function that called the singleshot.
        if (!bSingleShot)
        {
            ScraperHousekeeping();

            _log(logattribute::INFO, "Scraper", "Sleeping for " + std::to_string(nScraperSleep / 1000) +" seconds");
            MilliSleep(nScraperSleep);
        }
        else
            // This will break from the outer while loop if in singleshot mode and end execution after one pass.
            // otherwise in a continuous while loop with nScraperSleep between iterations.
            break;
    }
}


void ScraperSingleShot()
{
    // Going through Scraper function in single shot mode.

    _log(logattribute::INFO, "ScraperSingleShot", "Calling Scraper function in single shot mode.");

    Scraper(true);
}


// This is the non-scraper "neural-network" node thread...
void NeuralNetwork()
{
    _log(logattribute::INFO, "NeuralNetwork", "Starting Neural Network housekeeping thread (new C++ implementation). \n"
                                              "Note that this does NOT mean the NN is active. This simply does housekeeping"
                                              "functions.");

    while(!fShutdown)
    {
        // Only proceed if wallet is in sync. Check every 5 seconds since no callback is available.
        // We do NOT want to filter statistics with an out-of-date beacon list or project whitelist.
        while (OutOfSyncByAge())
        {
            _log(logattribute::INFO, "NeuralNetwork", "Wallet not in sync. Sleeping for 8 seconds.");
            MilliSleep(8000);
        }

        {
            LOCK(cs_nSyncTime);
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_nSyncTime");

            nSyncTime = GetAdjustedTime();

            _log(logattribute::INFO, "NeuralNetwork", "Wallet is in sync. Continuing.");

            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_nSyncTime");
        }

        // ScraperHousekeeping items are only run in this thread if not handled by the Scraper() thread.
        if (!fScraperActive)
        {
            LOCK(cs_Scraper);
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_Scraper");

            ScraperDirectoryAndConfigSanity();
            // UnauthorizedCScraperManifests should only be seen on the first invocation after getting in sync
            // See the comment on the function.

            LOCK(CScraperManifest::cs_mapManifest);
            if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

            ScraperDeleteUnauthorizedCScraperManifests();

            // END LOCK(CScraperManifest::cs_mapManifest)
            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

            ScraperHousekeeping();

            // END LOCK(cs_Scraper)
            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
        }

        // Use the same sleep interval configured for the scraper.
        _log(logattribute::INFO, "NeuralNetwork", "Sleeping for " + std::to_string(nScraperSleep / 1000) +" seconds");

        MilliSleep(nScraperSleep);
    }
}


bool ScraperHousekeeping()
{
    gridcoinrpc data;

    // Refresh the whitelist if its available
    if (!data.wlimport())
        _log(logattribute::WARNING, "ScraperHousekeeping", "Refreshing of whitelist failed.. using old data");

    else
        _log(logattribute::INFO, "ScraperHousekeeping", "Refreshing of whitelist completed");

    // Periodically generate converged manifests and generate SB core and "contract"
    // This will probably be reduced to the commented out call as we near final testing,
    // because ScraperGetNeuralContract(false) is called from the neuralnet native interface
    // with the boolean false, meaning don't store the stats.
    // Lock both cs_Scraper and cs_StructScraperFileManifest.

    //ConvergedManifest StructConvergedManifest;
    std::string sSBCoreData;

    {
        LOCK2(cs_Scraper, cs_StructScraperFileManifest);

        //ScraperConstructConvergedManifest(StructConvergedManifest)
        sSBCoreData = ScraperGetNeuralContract(true, false);
    }

    if (!sSBCoreData.empty())
    {
        // Temporarily here for compatibility checking...
        _log(logattribute::INFO, "ScraperHousekeeping", "Checking compatibility with binary SB pack/unpack by packing then unpacking, then comparing to the original");

        std::string sSBCoreData_out = UnpackBinarySuperblock(PackBinarySuperblock(sSBCoreData));

        if (sSBCoreData == sSBCoreData_out)
            _log(logattribute::INFO, "ScraperHousekeeping", "Generated contract passed binary pack/unpack");
        else
        {
            _log(logattribute::ERR, "ScraperHousekeeping", "Generated contract FAILED binary pack/unpack");
            _log(logattribute::INFO, "ScraperHousekeeping", "sSBCoreData_out = \n" + sSBCoreData_out);
        }
    }

    // Temporarily here to show this node's contract hash.
    _log(logattribute::INFO, "ScraperHousekeeping", "neural contract (sSBCoreData) hash = " + ScraperGetNeuralHash(sSBCoreData));

    // Temporarily here for visibility into the Quorum map...
    if (fDebug)
    {
        _log(logattribute::INFO, "ScraperHousekeeping", "mvNeuralNetworkHash dump");
        for (const auto& network_hash : mvNeuralNetworkHash)
            _log(logattribute::INFO, "ScraperHousekeeping", "NN Contract Hash: " + network_hash.first
                 + ", Popularity: " + std::to_string(network_hash.second));
        _log(logattribute::INFO, "ScraperHousekeeping", "mvCurrentNeuralNetworkHash dump");
        for (const auto& network_hash : mvCurrentNeuralNetworkHash)
            _log(logattribute::INFO, "ScraperHousekeeping", "NN Contract Hash: " + network_hash.first
                 + ", Popularity: " + std::to_string(network_hash.second));
    }

    return true;
}


/**********************
* Sanity              *
**********************/
std::string urlsanity(const std::string& s, const std::string& type)
{
    stringbuilder url;

    if (Contains("einstein", s))
    {
        url.append(s.substr(0,4));
        url.append("s");
        url.append(s.substr(4, s.size()));
    }

    else
        url.append(s);

    url.append("stats/");
    url.append(type);

    if (Contains("gorlaeus", s) || Contains("worldcommunitygrid", s))
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

// A lock on cs_Scraper should be taken before calling this function.
bool ScraperDirectoryAndConfigSanity()
{
    ScraperApplyAppCacheEntries();

    // Check to see if the Scraper directory exists and is a directory. If not create it.
    if (fs::exists(pathScraper))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the Scraper directory.
        if (fs::is_regular_file(pathScraper))
        {
            fs::remove(pathScraper);
            fs::create_directory(pathScraper);
        }
        // Only do the file manifest to directory alignments if the scraper is active.
        else if (fScraperActive)
        {
            // Load the manifest file from the Scraper directory into mScraperFileManifest, if mScraperFileManifest is empty.
            // Lock the manifest while it is being manipulated.
            {
                LOCK(cs_StructScraperFileManifest);
                if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");

                if (StructScraperFileManifest.mScraperFileManifest.empty())
                {
                    _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loading Manifest");
                    if (!LoadScraperFileManifest(pathScraper / "Manifest.csv.gz"))
                        _log(logattribute::ERR, "ScraperDirectoryAndConfigSanity", "Error occurred loading manifest");
                    else
                        _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loaded Manifest file into map.");
                }

                // Align the Scraper directory with the Manifest file.
                // First remove orphan files with no Manifest entry.
                // Check to see if the file exists in the manifest and if the hash matches. If it doesn't
                // remove it.
                ScraperFileManifestMap::iterator entry;

                for (fs::directory_entry& dir : fs::directory_iterator(pathScraper))
                {
                    std::string filename = dir.path().filename().string();

                    if (dir.path().filename() != "Manifest.csv.gz"
                            && dir.path().filename() != "BeaconList.csv.gz"
                            && dir.path().filename() != "Stats.csv.gz"
                            && dir.path().filename() != "ConvergedStats.csv.gz"
                            && dir.path().filename() != "TeamIDs.csv.gz"
                            && fs::is_regular_file(dir))
                    {
                        entry = StructScraperFileManifest.mScraperFileManifest.find(dir.path().filename().string());
                        if (entry == StructScraperFileManifest.mScraperFileManifest.end())
                        {
                            fs::remove(dir.path());
                            _log(logattribute::WARNING, "ScraperDirectoryAndConfigSanity", "Removing orphan file not in Manifest: " + filename);
                            continue;
                        }

                        if (entry->second.hash != GetFileHash(dir))
                        {
                            _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "File failed hash check. Removing file.");
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
                            || (!SCRAPER_RETAIN_NONCURRENT_FILES && entry_copy->second.current == false))
                    {
                        _log(logattribute::WARNING, "ScraperDirectoryAndConfigSanity", "Removing stale or orphan manifest entry: " + entry_copy->first);
                        DeleteScraperFileManifestEntry(entry_copy->second);
                    }
                }

                // End LOCK(cs_StructScraperFileManifest)
                if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
            }

            // If network policy is set to filter on whitelisted teams, then load team ID map from file. This will prevent the heavyweight
            // team file downloads for projects whose team ID's have already been found and stored.
            if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP)
            {
                LOCK(cs_TeamIDMap);
                if (fDebug) _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

                if (TeamIDMap.empty())
                {
                    _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loading team IDs");
                    if (!LoadTeamIDList(pathScraper / "TeamIDs.csv.gz"))
                        _log(logattribute::ERR, "ScraperDirectoryAndConfigSanity", "Error occurred loading team IDs");
                    else
                    {
                        _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loaded team IDs file into map.");
                        if (fDebug3)
                        {
                            _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "TeamIDMap contents:");
                            for (const auto& iter : TeamIDMap)
                            {
                                _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Project = " + iter.first);
                                for (const auto& iter2 : iter.second)
                                {
                                    _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity",
                                         "Team = " + iter2.first + ", TeamID = " + std::to_string(iter2.second));
                                }
                            }
                        }
                    }
                }

                if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
            }
        }
    }
    else
        fs::create_directory(pathScraper);

    return true;
}

/**********************
* Populate Whitelist  *
**********************/

bool WhitelistPopulated()
{
    LOCK(cs_vwhitelist);
    if (fDebug) _log(logattribute::INFO, "LOCK", "cs_vwhitelist");

    if (vwhitelist.empty())
    {
        _log(logattribute::INFO, "WhitelistPopulated", "Whitelist vector currently empty; populating");

        gridcoinrpc data;

        if (data.wlimport())
            _log(logattribute::INFO, "WhitelistPopulated", "Successfully populated whitelist vector");

        else
        {
            _log(logattribute::CRITICAL, "WhitelistPopulated", "Failed to populate whitelist vector; keeping old whitelist data for time being");

            return false;
        }
    }

    _log(logattribute::INFO, "WhitelistPopulated", "Whitelist is populated; Contains " + std::to_string(vwhitelist.size()) + " projects");

    // End LOCK(cs_vwhitelist.
    if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_vwhitelist");

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
        _log(logattribute::INFO, "UserpassPopulated", "Userpass vector currently empty; populating");

        userpass up;

        if (up.import())
            _log(logattribute::INFO, "UserPassPopulated", "Successfully populated userpass vector");

        else
        {
            _log(logattribute::CRITICAL, "UserPassPopulated", "Failed to populate userpass vector");

            return false;
        }
    }

    _log(logattribute::INFO, "UserPassPopulated", "Userpass is populated; Contains " + std::to_string(vuserpass.size()) + " projects");

    return true;
}




/**********************
* Project Team Files  *
**********************/

bool DownloadProjectTeamFiles()
{
    std::vector<std::pair<std::string, std::string>> vwhitelist_local {};

        if (!WhitelistPopulated())
        {
            _log(logattribute::CRITICAL, "DownloadProjectTeamFiles", "Whitelist is not populated");

            return false;
        }

        {
            LOCK(cs_vwhitelist);
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_vwhitelist");

            vwhitelist_local = vwhitelist;
            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_vwhitelist");
        }

        if (!UserpassPopulated())
        {
            _log(logattribute::CRITICAL, "DownloadProjectTeamFiles", "Userpass is not populated");

            return false;
        }

        for (const auto& prjs : vwhitelist_local)
        {
            LOCK(cs_TeamIDMap);
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

            const auto iter = TeamIDMap.find(prjs.first);

            // If there is an entry in the TeamIDMap for the project, and in the submap (team name and team id) there
            // are the correct number of team entries, then skip processing.
            if (iter != TeamIDMap.end() && iter->second.size() == split(TEAM_WHITELIST, ",").size())
            {
                _log(logattribute::INFO, "DownloadProjectTeamFiles", "Correct team whitelist entries already in the team ID map for "
                     + prjs.first + " project. Skipping team file download and processing.");
                continue;
            }

            _log(logattribute::INFO, "DownloadProjectTeamFiles", "Downloading project file for " + prjs.first);

            std::vector<std::string> vPrjUrl = split(prjs.second, "@");

            std::string sUrl = urlsanity(vPrjUrl[0], "team");

            std::string team_file_name = prjs.first + "-team.gz";

            fs::path team_file = pathScraper / team_file_name.c_str();

            // Grab ETag of team file
            Http http;
            std::string sTeamETag;

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

            try
            {
                sTeamETag = http.GetEtag(sUrl, userpass);
            }
            catch (const std::runtime_error& e)
            {
                _log(logattribute::ERR, "DownloadProjectTeamFiles", "Failed to pull team header file for " + prjs.first);
                continue;
            }

            if (sTeamETag.empty())
            {
                _log(logattribute::ERR, "DownloadProjectTeamFiles", "ETag for project is empty" + prjs.first);

                continue;
            }
            else
                _log(logattribute::INFO, "DownloadProjectTeamFiles", "Successfully pulled team header file for " + prjs.first);

            if (buserpass)
            {
                authdata ad(lowercase(prjs.first));

                ad.setoutputdata("team", prjs.first, sTeamETag);

                if (!ad.xport())
                    _log(logattribute::CRITICAL, "DownloadProjectTeamFiles", "Failed to export etag for " + prjs.first + " to authentication file");
            }

            std::string chketagfile = prjs.first + "-" + sTeamETag + ".csv" + ".gz";
            fs::path chkfile = pathScraper / chketagfile.c_str();

            if (fs::exists(chkfile))
            {
                _log(logattribute::INFO, "DownloadProjectTeamFiles", "Etag file for " + prjs.first + " already exists");
                continue;
            }
            else
                fs::remove(team_file);

            try
            {
                http.Download(sUrl, team_file.string(), userpass);
            }
            catch(const std::runtime_error& e)
            {
                _log(logattribute::ERR, "DownloadProjectTeamFiles", "Failed to download project team file for " + prjs.first);
                continue;
            }


            std::map<std::string, int64_t> mTeamIDsForProject = {};

            if (ProcessProjectTeamFile(team_file.string(), sTeamETag, mTeamIDsForProject))
            {
                // Insert or update team IDs for the project into the team ID map.
                TeamIDMap[prjs.first] = mTeamIDsForProject;
            }

            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
        }

    return true;
}



bool ProcessProjectTeamFile(const fs::path& file, const std::string& etag, std::map<std::string, int64_t>& mTeamIdsForProject_out)
{
    // If passed an empty file, immediately return false.
    if (file.string().empty())
        return false;

    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "ProcessProjectTeamFile", "Failed to open team gzip file (" + file.string() + ")");

        return 0;
    }

    _log(logattribute::INFO, "ProcessProjectTeamFile", "Opening team file (" + file.string() + ")");

    boostio::filtering_istream in;

    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string gzetagfile = "";

    // If einstein we store different
    //if (file.string().find("einstein") != std::string::npos)
    //    gzetagfile = "einstein_team.gz";
    //else
    gzetagfile = etag + ".csv" + ".gz";

    //std::string gzetagfile_no_path = gzetagfile;
    // Put path in.
    gzetagfile = ((fs::path)(pathScraper / gzetagfile)).string();

    _log(logattribute::INFO, "ProcessProjectTeamFile", "Started processing " + file.string());

    std::vector<std::string> vTeamWhiteList = split(TEAM_WHITELIST, ",");

    std::string line;
    stringbuilder builder;

    while (std::getline(in, line))
    {
        if (line == "<team>")
            builder.clear();
        else if (line == "</team>")
        {
            const std::string& data = builder.value();
            builder.clear();

            const std::string& sTeamID = ExtractXML(data, "<id>", "</id>");
            const std::string& sTeamName = ExtractXML(data, "<name>", "</name>");

            // See if the team name is in the team whitelist.
            auto iter = find(vTeamWhiteList.begin(), vTeamWhiteList.end(), sTeamName);
            // If it is not continue on to next team.
            if (iter == vTeamWhiteList.end())
                continue;

            int64_t nTeamID = 0;

            try
            {
                nTeamID = atoi64(sTeamID);
            }
            catch (const std::exception&)
            {
                _log(logattribute::ERR, "ProccessProjectTeamFile", tfm::format("Ignoring bad team id for team %s.", sTeamName));
                continue;
            }

            mTeamIdsForProject_out[sTeamName] = nTeamID;
        }
        else
            builder.append(line);
    }

    if (mTeamIdsForProject_out.empty())
    {
        _log(logattribute::CRITICAL, "ProcessProjectTeamFile", "Error in data processing of " + file.string());

        std::string efile = etag + ".gz";
        fs::path fsepfile = pathScraper/ efile;
        ingzfile.close();

        if (fs::exists(fsepfile))
            fs::remove(fsepfile);

        if (fs::exists(file))
            fs::remove(file);

        return false;
    }

    std::string efile = etag + ".gz";
    fs::path fsepfile = pathScraper/ efile;
    ingzfile.close();

    if (fs::exists(fsepfile))
        fs::remove(fsepfile);

    if (fs::exists(file))
        fs::remove(file);

    if (mTeamIdsForProject_out.size() < vTeamWhiteList.size())
        _log(logattribute::ERR, "ProcessProjectTeamFile", "Unable to determine team IDs for one or more whitelisted teams.");

    // The below is not an ideal implementation, because the entire map is going to be written out to disk each time.
    // The TeamIDs file is actually very small though, and this primitive implementation will suffice.
    _log(logattribute::INFO, "ProcessProjectTeamFile", "Persisting Team ID entries to disk.");
    if (!StoreTeamIDList(pathScraper / "TeamIDs.csv.gz"))
        _log(logattribute::ERR, "ProcessProjectTeamFile", "StoreTeamIDList error occurred.");
    else
        _log(logattribute::INFO, "ProcessProjectTeamFile", "Stored Team ID entries.");

    _log(logattribute::INFO, "ProcessProjectTeamFile", "Finished processing " + file.string());

    return true;
}


/**********************
* Project RAC Files   *
**********************/

bool DownloadProjectRacFilesByCPID()
{
    std::vector<std::pair<std::string, std::string>> vwhitelist_local {};

    if (!WhitelistPopulated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectRacFiles", "Whitelist is not populated");

        return false;
    }

    {
        LOCK(cs_vwhitelist);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_vwhitelist");

        vwhitelist_local = vwhitelist;
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_vwhitelist");
    }

    if (!UserpassPopulated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectRacFiles", "Userpass is not populated");

        return false;
    }

    // Get a consensus map of Beacons.
    BeaconConsensus Consensus = GetConsensusBeaconList();
    _log(logattribute::INFO, "DownloadProjectRacFiles", "Getting consensus map of Beacons.");

    for (const auto& prjs : vwhitelist_local)
    {
        _log(logattribute::INFO, "DownloadProjectRacFiles", "Downloading project file for " + prjs.first);

        std::vector<std::string> vPrjUrl = split(prjs.second, "@");

        std::string sUrl = urlsanity(vPrjUrl[0], "user");

        std::string rac_file_name = prjs.first + +"-user.gz";

        fs::path rac_file = pathScraper / rac_file_name.c_str();

        //            if (fs::exists(rac_file))
        //                fs::remove(rac_file);

        // Grab ETag of rac file
        Http http;
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

        try
        {
            sRacETag = http.GetEtag(sUrl, userpass);
        } catch (const std::runtime_error& e)
        {
            _log(logattribute::ERR, "DownloadProjectRacFiles", "Failed to pull rac header file for " + prjs.first);
            continue;
        }

        if (sRacETag.empty())
        {
            _log(logattribute::ERR, "DownloadProjectRacFiles", "ETag for project is empty" + prjs.first);

            continue;
        }

        else
            _log(logattribute::INFO, "DownloadProjectRacFiles", "Successfully pulled rac header file for " + prjs.first);

        if (buserpass)
        {
            authdata ad(lowercase(prjs.first));

            ad.setoutputdata("user", prjs.first, sRacETag);

            if (!ad.xport())
                _log(logattribute::CRITICAL, "DownloadProjectRacFiles", "Failed to export etag for " + prjs.first + " to authentication file");
        }

        std::string chketagfile = prjs.first + "-" + sRacETag + ".csv" + ".gz";
        fs::path chkfile = pathScraper / chketagfile.c_str();

        if (fs::exists(chkfile))
        {
            _log(logattribute::INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.first + " already exists");
            //_log(logattribute::INFO, "DownloadProjectRacFiles", "Etag file size " + std::to_string(fs::file_size(chkfile)));
            continue;
        }
        else
            fs::remove(chkfile);

        try
        {
            http.Download(sUrl, rac_file.string(), userpass);
        }
        catch(const std::runtime_error& e)
        {
            _log(logattribute::ERR, "DownloadProjectRacFiles", "Failed to download project rac file for " + prjs.first);
            continue;
        }

        ProcessProjectRacFileByCPID(prjs.first, rac_file.string(), sRacETag, Consensus);
    }

    // After processing, update global structure with the timestamp of the latest file in the manifest.
    {
        LOCK(cs_StructScraperFileManifest);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");

        int64_t nMaxTime = 0;
        for (const auto& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            nMaxTime = std::max(nMaxTime, entry.second.timestamp);
        }

        StructScraperFileManifest.timestamp = nMaxTime;

        // End LOCK(cs_StructScraperFileManifest)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
    }
    return true;
}




// This version uses a consensus beacon map (and teamid, if team filtering is specificed by policy) to filter statistics.
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag, BeaconConsensus& Consensus)
{
    // Set fileerror flag to true until made false by the completion of one successful injection of user stats into stream.
    bool bfileerror = true;

    // If passed an empty file, immediately return false.
    if (file.string().empty())
        return false;

    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "ProcessProjectRacFileByCPID", "Failed to open rac gzip file (" + file.string() + ")");

        return false;
    }

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Opening rac file (" + file.string() + ")");

    boostio::filtering_istream in;

    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string gzetagfile = "";

    gzetagfile = project + "-" + etag + ".csv" + ".gz";

    std::string gzetagfile_no_path = gzetagfile;
    // Put path in.
    gzetagfile = ((fs::path)(pathScraper / gzetagfile)).string();

    std::ofstream outgzfile(gzetagfile, std::ios_base::out | std::ios_base::binary);
    boostio::filtering_ostream out;
    out.push(boostio::gzip_compressor());
    out.push(outgzfile);

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Started processing " + file.string());

    std::map<std::string, int64_t> mTeamIDsForProject = {};
    // Take a lock on cs_TeamIDMap to populate local whitelist TeamID vector for this project.
    if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP)
    {
        LOCK(cs_TeamIDMap);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

        mTeamIDsForProject = TeamIDMap.find(project)->second;

        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
    }

    std::string line;
    stringbuilder builder;
    out << "# total_credit,expavg_time,expavgcredit,cpid" << std::endl;
    while (std::getline(in, line))
    {
        if (line == "<user>")
            builder.clear();
        else if (line == "</user>")
        {
            const std::string& data = builder.value();
            builder.clear();

            const std::string& cpid = ExtractXML(data, "<cpid>", "</cpid>");
            if (Consensus.mBeaconMap.count(cpid) < 1)
                continue;

            // Only do this if team membership filtering is specified by network policy.
            if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP)
            {
                const std::string& sTeamID = ExtractXML(data, "<teamid>", "</teamid>");
                int64_t nTeamID = 0;

                try
                {
                    nTeamID = atoi64(sTeamID);
                }
                catch (const std::exception&)
                {
                    _log(logattribute::ERR, "ProcessProjectRacFileByCPID", "Bad team id in user stats file data.");
                    continue;
                }

                // Check to see if the user's team ID is in the whitelist team ID map for the project. If not continue.
                for (auto const& iTeam : mTeamIDsForProject)
                {
                    if (iTeam.second == nTeamID)
                        continue;
                }
            }

            // User beacon verified. Append its statistics to the CSV output.
            out << ExtractXML(data, "<total_credit>", "</total_credit>") << ","
                << ExtractXML(data, "<expavg_time>", "</expavg_time>") << ","
                << ExtractXML(data, "<expavg_credit>", "</expavg_credit>") << ","
                << cpid
                << std::endl;

            // If we get here at least once then there is at least one CPID being put in the file.
            // So set the bfileerror flag to false.
            bfileerror = false;
        }
        else
            builder.append(line);
    }

    // TODO: More error checking on stream errors.
    if (bfileerror)
    {
        _log(logattribute::CRITICAL, "ProcessProjectRacFileByCPID", "Error in data processing of " + file.string() + "; Aborted processing");

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

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Finished processing " + file.string());

    ingzfile.close();
    out.flush();
    out.reset();
    outgzfile.close();

    // Hash the file.
    
    uint256 nFileHash = GetFileHash(gzetagfile);
    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "FileHash by GetFileHash " + nFileHash.ToString());


    try
    {
        size_t filea = fs::file_size(file);
        fs::path temp = gzetagfile.c_str();
        size_t fileb = fs::file_size(temp);

        _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Processing new rac file " + file.string() + "(" + std::to_string(filea) + " -> " + std::to_string(fileb) + ")");

        ndownloadsize += (int64_t)filea;
        nuploadsize += (int64_t)fileb;
    }
    catch (fs::filesystem_error& e)
    {
        _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "FS Error -> " + std::string(e.what()));
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
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");
        
        // Iterate mScraperFileManifest to find any prior records for the same project and change current flag to false,
        // or delete if older than SCRAPER_FILE_RETENTION_TIME or non-current and fScraperRetainNonCurrentFiles
        // is false.

        ScraperFileManifestMap::iterator entry;
        for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
        {
            ScraperFileManifestMap::iterator entry_copy = entry++;

            if (entry_copy->second.project == project && entry_copy->second.current == true)
            {
                _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Marking old project manifest entry as current = false.");
                MarkScraperFileManifestEntryNonCurrent(entry_copy->second);
            }

            // If records are older than SCRAPER_FILE_RETENTION_TIME delete record, or if fScraperRetainNonCurrentFiles is false,
            // delete all non-current records, including the one just marked non-current.
            if (((GetAdjustedTime() - entry_copy->second.timestamp) > SCRAPER_FILE_RETENTION_TIME)
                    || (entry_copy->second.project == project && entry_copy->second.current == false && !SCRAPER_RETAIN_NONCURRENT_FILES))
            {
                DeleteScraperFileManifestEntry(entry_copy->second);
            }
        }

        if (!InsertScraperFileManifestEntry(NewRecord))
            _log(logattribute::WARNING, "ProcessProjectRacFileByCPID", "Manifest entry already exists for " + nFileHash.ToString() + " " + gzetagfile);
        else
            _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Created manifest entry for " + nFileHash.ToString() + " " + gzetagfile);

        // The below is not an ideal implementation, because the entire map is going to be written out to disk each time.
        // The manifest file is actually very small though, and this primitive implementation will suffice. I could
        // put it up in the while loop above, but then there is a much higher risk that the manifest file could be out of
        // sync if the wallet is ended during the middle of pulling the files.
        _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Persisting manifest entry to disk.");
        if (!StoreScraperFileManifest(pathScraper / "Manifest.csv.gz"))
            _log(logattribute::ERR, "ProcessProjectRacFileByCPID", "StoreScraperFileManifest error occurred");
        else
            _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Stored Manifest");

        // End LOCK(cs_StructScraperFileManifest)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
    }

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Complete Process");

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

/***********************
* Persistance          *
************************/

bool LoadBeaconList(const fs::path& file, BeaconMap& mBeaconMap)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "LoadBeaconList", "Failed to open beacon gzip file (" + file.string() + ")");

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



bool LoadTeamIDList(const fs::path& file)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "LoadTeamIDList", "Failed to open Team ID gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;

    // Header. This is used to construct the team names vector, since the team IDs were stored in the same order.
    std::getline(in, line);

    // This is in the form Project, Gridcoin, ...."
    std::vector<std::string> vTeamNames = split(line, ",");
    if (fDebug3) _log(logattribute::INFO, "LoadTeamIDList", "Size of vTeamNames = " + std::to_string(vTeamNames.size()));

    while (std::getline(in, line))
    {
        std::string sProject = {};
        std::map<std::string, int64_t> mTeamIDsForProject = {};

        std::vector<std::string> vline = split(line, ",");

        unsigned int iTeamName = 0;
        // Populate team IDs into map.
        for (const auto& iter : vline)
        {
            int64_t nTeamID;

            // Skip (probably stale or bad entry with more or less team IDs than the header.
            if (vline.size() != vTeamNames.size())
                continue;

            // The first element is the project
            if (!iTeamName)
                sProject = iter;
            else
            {
                try
                {
                    nTeamID = atoi64(iter);
                }
                catch (std::exception&)
                {
                    _log(logattribute::ERR, "LoadTeamIDList", "Ignoring invalid team id found in team id file.");
                    continue;
                }

                std::string sTeamName = vTeamNames.at(iTeamName);

                mTeamIDsForProject[sTeamName] = nTeamID;
            }

            iTeamName++;
        }

        LOCK(cs_TeamIDMap);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

        // Insert into whitelist team ID map.
        if (!sProject.empty())
            TeamIDMap[sProject] = mTeamIDsForProject;

        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
    }

    return true;
}



bool StoreBeaconList(const fs::path& file)
{
    BeaconConsensus Consensus = GetConsensusBeaconList();
    
    _log(logattribute::INFO, "StoreBeaconList", "ReadCacheSection element count: " + std::to_string(ReadCacheSection(Section::BEACON).size()));
    _log(logattribute::INFO, "StoreBeaconList", "mBeaconMap element count: " + std::to_string(Consensus.mBeaconMap.size()));

    // Update block hash for block at consensus height to StructScraperFileManifest.
    // Requires a lock.
    {
        LOCK(cs_StructScraperFileManifest);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");

        StructScraperFileManifest.nConsensusBlockHash = Consensus.nBlockHash;

        // End LOCK(cs_StructScraperFileManifest)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
    }

    if (fs::exists(file))
        fs::remove(file);

    std::ofstream outgzfile(file.string().c_str(), std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(logattribute::ERR, "StoreBeaconList", "Failed to open beacon list gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreBeaconList", "Started processing " + file.string());
    
    // Header
    stream << "CPID," << "Time," << "Beacon\n";

    for (auto const& entry : Consensus.mBeaconMap)
    {
        std::string sBeaconEntry = entry.first + "," + std::to_string(entry.second.timestamp) + "," + entry.second.value + "\n";
        stream << sBeaconEntry;
    }

    _log(logattribute::INFO, "StoreBeaconList", "Finished processing beacon data from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreBeaconList", "Process Complete.");

    return true;
}



bool StoreTeamIDList(const fs::path& file)
{
    LOCK(cs_TeamIDMap);

    if (fs::exists(file))
        fs::remove(file);

    std::ofstream outgzfile(file.string().c_str(), std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(logattribute::ERR, "StoreTeamIDList", "Failed to open team ID list gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreTeamIDList", "Started processing " + file.string());

    // Header
    stream << "Project";

    std::vector<std::string> vTeamWhiteList = split(TEAM_WHITELIST, ",");
    std::set<std::string> setTeamWhiteList;

    // Ensure that the team names are in the correct order.
    for (auto const& iTeam: vTeamWhiteList)
        setTeamWhiteList.insert(iTeam);

    for (auto const& iTeam: setTeamWhiteList)
        stream << "," << iTeam;

    stream << std::endl;

    // Data
    for (auto const& iProject : TeamIDMap)
    {
        std::string sProjectEntry = {};

        stream << iProject.first;

        for (auto const& iTeam : iProject.second)
            sProjectEntry += "," + std::to_string(iTeam.second);

        stream << sProjectEntry << std::endl;
    }

    _log(logattribute::INFO, "StoreTeamIDList", "Finished processing Team ID data from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreTeamIDList", "Process Complete.");

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
    if (fs::exists(pathScraper / entry.filename))
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
        _log(logattribute::ERR, "LoadScraperFileManifest", "Failed to open manifest gzip file (" + file.string() + ")");

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
            if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");

            InsertScraperFileManifestEntry(LoadEntry);

            // End LOCK(cs_StructScraperFileManifest
            if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
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
        _log(logattribute::ERR, "StoreScraperFileManifest", "Failed to open manifest gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreScraperFileManifest", "Started processing " + file.string());

    //Lock StructScraperFileManifest during serialize to string.
    {
        LOCK(cs_StructScraperFileManifest);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");
        
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

        // end LOCK(cs_StructScraperFileManifest)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
    }

    _log(logattribute::INFO, "StoreScraperFileManifest", "Finished processing manifest from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreScraperFileManifest", "Process Complete.");

    return true;
}



bool StoreStats(const fs::path& file, const ScraperStats& mScraperStats)
{
    if (fs::exists(file))
        fs::remove(file);

    std::ofstream outgzfile(file.string().c_str(), std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(logattribute::ERR, "StoreStats", "Failed to open stats gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreStats", "Started processing " + file.string());

    // Header.
    stream << "StatsType," << "Project," << "CPID," << "TC," << "RAT," << "RAC," << "AvgRAC," << "Mag\n";

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
                + std::to_string(entry.second.statsvalue.dAvgRAC) + ","
                + std::to_string(entry.second.statsvalue.dMag) + ","
                + "\n";
        stream << sScraperStatsEntry;
    }

    _log(logattribute::INFO, "StoreStats", "Finished processing stats from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreStats", "Process Complete.");

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
        _log(logattribute::ERR, "LoadProjectFileToStatsByCPID", "Failed to open project user stats gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    // TODO implement file error handling
    // bool bcomplete = false;
    // bool bfileerror = false;

    bool bResult = ProcessProjectStatsFromStreamByCPID(project, in, projectmag, mBeaconMap, mScraperStats);

    return bResult;
}



bool LoadProjectObjectToStatsByCPID(const std::string& project, const CSerializeData& ProjectData, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    boostio::basic_array_source<char> input_source(&ProjectData[0], ProjectData.size());
    boostio::stream<boostio::basic_array_source<char>> ingzss(input_source);

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzss);

    bool bResult = ProcessProjectStatsFromStreamByCPID(project, in, projectmag, mBeaconMap, mScraperStats);

    return bResult;
}



bool ProcessProjectStatsFromStreamByCPID(const std::string& project, boostio::filtering_istream& sUncompressedIn,
                                         const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    std::vector<std::string> vXML;

    // Lets vector the user blocks
    std::string line;
    double dProjectRAC = 0.0;
    while (std::getline(sUncompressedIn, line))
    {
        if (line[0] == '#')
            continue;

        std::vector<std::string> fields;
        boost::split(fields, line, boost::is_any_of(","), boost::token_compress_on);

        if (fields.size() < 4)
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
        // At the individual (byCPIDbyProject) level the AvgRAC is the same as the RAC.
        statsentry.statsvalue.dAvgRAC = statsentry.statsvalue.dRAC;
        // Mag is dealt with on the second pass... so is left at 0.0 on the first pass.

        statsentry.statskey.objecttype = statsobjecttype::byCPIDbyProject;
        statsentry.statskey.objectID = project + "," + cpid;

        // Insert stats entry into map by the key.
        mScraperStats[statsentry.statskey] = statsentry;

        // Increment project
        dProjectRAC += statsentry.statsvalue.dRAC;
    }

    _log(logattribute::INFO, "LoadProjectObjectToStatsByCPID", "There are " + std::to_string(mScraperStats.size()) + " CPID entries for " + project);

    // The mScraperStats here is scoped to only this project so we do not need project filtering here.
    ScraperStats::iterator entry;

    for (auto const& entry : mScraperStats)
    {
        ScraperObjectStats statsentry;

        statsentry.statskey = entry.first;
        statsentry.statsvalue.dTC = entry.second.statsvalue.dTC;
        statsentry.statsvalue.dRAT = entry.second.statsvalue.dRAT;
        statsentry.statsvalue.dRAC = entry.second.statsvalue.dRAC;
        // As per the above the individual (byCPIDbyProject) level the AvgRAC is the same as the RAC.
        statsentry.statsvalue.dAvgRAC = entry.second.statsvalue.dAvgRAC;
        statsentry.statsvalue.dMag = MagRound(entry.second.statsvalue.dRAC / dProjectRAC * projectmag);

        // Update map entry with the magnitude.
        mScraperStats[statsentry.statskey] = statsentry;
    }

    // Due to rounding to MAG_ROUND, the actual total project magnitude will not be exactly projectmag,
    // but it should be very close. Roll up project statistics.
    ScraperObjectStats ProjectStatsEntry = {};

    ProjectStatsEntry.statskey.objecttype = statsobjecttype::byProject;
    ProjectStatsEntry.statskey.objectID = project;

    unsigned int nCPIDCount = 0;
    for (auto const& entry : mScraperStats)
    {
        ProjectStatsEntry.statsvalue.dTC += entry.second.statsvalue.dTC;
        ProjectStatsEntry.statsvalue.dRAT += entry.second.statsvalue.dRAT;
        ProjectStatsEntry.statsvalue.dRAC += entry.second.statsvalue.dRAC;
        ProjectStatsEntry.statsvalue.dMag += entry.second.statsvalue.dMag;

        nCPIDCount++;
    }

    //Compute AvgRAC for project across CPIDs and set.
    (nCPIDCount > 0) ? ProjectStatsEntry.statsvalue.dAvgRAC = ProjectStatsEntry.statsvalue.dRAC / nCPIDCount : ProjectStatsEntry.statsvalue.dAvgRAC = 0.0;

    // Insert project level map entry.
    mScraperStats[ProjectStatsEntry.statskey] = ProjectStatsEntry;

    return true;

}



// ------------------------------------------------ In ------------------- both In/Out
bool ProcessNetworkWideFromProjectStats(BeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    // We are going to cut across projects and group by CPID.

    //Also track the network wide rollup.
    ScraperObjectStats NetworkWideStatsEntry = {};

    NetworkWideStatsEntry.statskey.objecttype = statsobjecttype::NetworkWide;
    // ObjectID is blank string for network-wide.
    NetworkWideStatsEntry.statskey.objectID = "";

    unsigned int nCPIDProjectCount = 0;
    for (auto const& beaconentry : mBeaconMap)
    {
        ScraperObjectStats CPIDStatsEntry = {};

        CPIDStatsEntry.statskey.objecttype = statsobjecttype::byCPID;
        CPIDStatsEntry.statskey.objectID = beaconentry.first;

        unsigned int nProjectCount = 0;
        for (auto const& innerentry : mScraperStats)
        {
            // Only select the individual byCPIDbyProject stats for the selected CPID. Leave out the project rollup (byProj) ones,
            // otherwise dimension mixing will result.

            std::string objectID = innerentry.first.objectID;

            std::size_t found = objectID.find(CPIDStatsEntry.statskey.objectID);

            if (innerentry.first.objecttype == statsobjecttype::byCPIDbyProject && found!=std::string::npos)
            {
                CPIDStatsEntry.statsvalue.dTC += innerentry.second.statsvalue.dTC;
                CPIDStatsEntry.statsvalue.dRAT += innerentry.second.statsvalue.dRAT;
                CPIDStatsEntry.statsvalue.dRAC += innerentry.second.statsvalue.dRAC;
                CPIDStatsEntry.statsvalue.dMag += innerentry.second.statsvalue.dMag;
                // Note the following is VERY inelegant. It CAPS the CPID magnitude to CPID_MAG_LIMIT.
                // No attempt to renormalize the magnitudes due to this cap is done at this time. This means
                // The total magnitude across projects will NOT match the total across all CPIDs and the network.
                CPIDStatsEntry.statsvalue.dMag = std::min(CPID_MAG_LIMIT, CPIDStatsEntry.statsvalue.dMag);

                nProjectCount++;
                nCPIDProjectCount++;
            }
        }

        // Compute CPID AvgRAC across the projects for that CPID and set.
        (nProjectCount > 0) ? CPIDStatsEntry.statsvalue.dAvgRAC = CPIDStatsEntry.statsvalue.dRAC / nProjectCount : CPIDStatsEntry.statsvalue.dAvgRAC = 0.0;

        // Insert the byCPID entry into the overall map.
        mScraperStats[CPIDStatsEntry.statskey] = CPIDStatsEntry;

        // Increement the network wide stats.
        NetworkWideStatsEntry.statsvalue.dTC += CPIDStatsEntry.statsvalue.dTC;
        NetworkWideStatsEntry.statsvalue.dRAT += CPIDStatsEntry.statsvalue.dRAT;
        NetworkWideStatsEntry.statsvalue.dRAC += CPIDStatsEntry.statsvalue.dRAC;
        NetworkWideStatsEntry.statsvalue.dMag += CPIDStatsEntry.statsvalue.dMag;
    }

    // Compute Network AvgRAC across all ByCPIDByProject elements and set.
    (nCPIDProjectCount > 0) ? NetworkWideStatsEntry.statsvalue.dAvgRAC = NetworkWideStatsEntry.statsvalue.dRAC / nCPIDProjectCount : NetworkWideStatsEntry.statsvalue.dAvgRAC = 0.0;

    // Insert the (single) network-wide entry into the overall map.
    mScraperStats[NetworkWideStatsEntry.statskey] = NetworkWideStatsEntry;

    return true;
}


ScraperStats GetScraperStatsByConsensusBeaconList()
{
    _log(logattribute::INFO, "GetScraperStatsByConsensusBeaconList", "Beginning stats processing.");

    // Enumerate the count of active projects from the file manifest. Since the manifest is
    // constructed starting with the whitelist, and then using only the current files, this
    // will always be less than or equal to the whitelist count from vwhitelist.
    unsigned int nActiveProjects = 0;
    {
        LOCK(cs_StructScraperFileManifest);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");

        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            if (entry.second.current)
                nActiveProjects++;
        }

        // End LOCK(cs_StructScraperFileManifest)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
    }
    double dMagnitudePerProject = NEURALNETWORKMULTIPLIER / nActiveProjects;

    //Get the Consensus Beacon map and initialize mScraperStats.
    BeaconConsensus Consensus = GetConsensusBeaconList();

    ScraperStats mScraperStats;

    {
        LOCK(cs_StructScraperFileManifest);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_StructScraperFileManifest");

        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {

            if (entry.second.current)
            {
                std::string project = entry.first;
                fs::path file = pathScraper / entry.second.filename;
                ScraperStats mProjectScraperStats;

                _log(logattribute::INFO, "GetScraperStatsByConsensusBeaconList", "Processing stats for project: " + project);

                LoadProjectFileToStatsByCPID(project, file, dMagnitudePerProject, Consensus.mBeaconMap, mProjectScraperStats);

                // Insert into overall map.
                for (auto const& entry2 : mProjectScraperStats)
                {
                    mScraperStats[entry2.first] = entry2.second;
                }
            }
        }

        // End LOCK(cs_StructScraperFileManifest)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_StructScraperFileManifest");
    }

    ProcessNetworkWideFromProjectStats(Consensus.mBeaconMap, mScraperStats);

    _log(logattribute::INFO, "GetScraperStatsByConsensusBeaconList", "Completed stats processing");

    return mScraperStats;
}


ScraperStats GetScraperStatsByConvergedManifest(ConvergedManifest& StructConvergedManifest)
{
    _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Beginning stats processing.");

    // Enumerate the count of active projects from the converged manifest. One of the parts
    // is the beacon list, is not a project, which is why there is a -1.
    unsigned int nActiveProjects = StructConvergedManifest.ConvergedManifestPartsMap.size() - 1;
    _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Number of active projects in converged manifest = " + std::to_string(nActiveProjects));

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
        if (project != "BeaconList")
        {
            _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Processing stats for project: " + project);

            LoadProjectObjectToStatsByCPID(project, entry->second, dMagnitudePerProject, mBeaconMap, mProjectScraperStats);

            // Insert into overall map.
            for (auto const& entry2 : mProjectScraperStats)
            {
                mScraperStats[entry2.first] = entry2.second;
            }
        }
    }

    ProcessNetworkWideFromProjectStats(mBeaconMap, mScraperStats);

    _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Completed stats processing");

    return mScraperStats;
}


std::string ExplainMagnitude(std::string sCPID)
{
    // See if converged stats/contract update needed...
    bool bConvergenceUpdateNeeded = true;
    {
        LOCK(cs_ConvergedScraperStatsCache);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");


        if (GetAdjustedTime() - ConvergedScraperStatsCache.nTime < nScraperSleep)
            bConvergenceUpdateNeeded = false;

        // End LOCK(cs_ConvergedScraperStatsCache)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
    }

    if (bConvergenceUpdateNeeded)
        // Don't need the output but will use the global cache, which will be updated.
        ScraperGetNeuralContract(false, false);

    // A purposeful copy here to avoid a long-term lock. May want to change to direct reference
    // and allow locking during the output.
    ScraperStats mScraperConvergedStats;
    {
        LOCK(cs_ConvergedScraperStatsCache);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

        mScraperConvergedStats = ConvergedScraperStatsCache.mScraperConvergedStats;

        // End LOCK(cs_ConvergedScraperStatsCache)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
    }

    stringbuilder out;

    out.append("CPID,Project,RAC,Project_Total_RAC,Project_Avg_RAC,Project Mag,Cumulative RAC,Cumulative Mag,Errors<ROW>");

    double dCPIDCumulativeRAC = 0.0;
    double dCPIDCumulativeMag = 0.0;

    for (auto const& entry : mScraperConvergedStats)
    {
        // Only select the individual byCPIDbyProject stats for the selected CPID.

        std::size_t found = entry.first.objectID.find(sCPID);

        if (entry.first.objecttype == statsobjecttype::byCPIDbyProject && found!=std::string::npos)
        {
            dCPIDCumulativeRAC += entry.second.statsvalue.dRAC;
            dCPIDCumulativeMag += entry.second.statsvalue.dMag;

            std::string sInput = entry.first.objectID;

            // Remove ,CPID from key objectID to obtain referenced project.
            std::string sProject = sInput.erase(sInput.find("," + sCPID), sCPID.length() + 1);

            ScraperObjectStatsKey ProjectKey;

            ProjectKey.objecttype = statsobjecttype::byProject;
            ProjectKey.objectID = sProject;

            auto const& iProject = mScraperConvergedStats.find(ProjectKey);

            out.append(sCPID + ",");
            out.append(sProject + ",");
            out.fixeddoubleappend(iProject->second.statsvalue.dRAC, 2);
            out.append(",");
            out.fixeddoubleappend(iProject->second.statsvalue.dAvgRAC, 2);
            out.append(",");
            out.fixeddoubleappend(iProject->second.statsvalue.dMag, 2);
            out.append(",");
            out.fixeddoubleappend(dCPIDCumulativeRAC, 2);
            out.append(",");
            out.fixeddoubleappend(dCPIDCumulativeMag, 2);
            out.append(",");
            //The last field is for errors, but there are not any, so the <ROW> is next.
            out.append("<ROW>");
        }
    }

    // "Signature"
    // TODO: Why the magic version number of 430 from .NET? Should this be a constant?
    // Should we take a lock on cs_main to read GlobalCPUMiningCPID?
    out.append("NN Host Version: 430, ");
    out.append("NeuralHash: " + ConvergedScraperStatsCache.sContractHash + ", ");
    out.append("SignatureCPID: " + GlobalCPUMiningCPID.cpid + ", ");
    out.append("Time: " + DateTimeStrFormat("%x %H:%M:%S",  GetAdjustedTime()) + "<ROW>");

    //Totals
    out.append("Total RAC: ");
    out.fixeddoubleappend(dCPIDCumulativeRAC, 2);
    out.append("<ROW>");
    out.append("Total Mag: ");
    out.fixeddoubleappend(dCPIDCumulativeMag, 2);

    return out.value();
}



/***********************
* Scraper networking   *
************************/

bool ScraperSaveCScraperManifestToFiles(uint256 nManifestHash)
{
    // Make sure the Scraper directory itself exists, because this function could be called from outside
    // the scraper thread loop, and therefore the directory may not have been set up yet.
    {
        LOCK(cs_Scraper);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_Scraper");

        ScraperDirectoryAndConfigSanity();

        // End LOCK(cs_Scraper).
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
    }

    fs::path savepath = pathScraper / "incoming";

    // Check to see if the Scraper incoming directory exists and is a directory. If not create it.
    if (fs::exists(savepath))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the Scraper directory.
        if (fs::is_regular_file(savepath))
        {
            fs::remove(savepath);
            fs::create_directory(savepath);
        }
    }
    else
        fs::create_directory(savepath);

    LOCK(CScraperManifest::cs_mapManifest);
    if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

    // Select manifest based on provided hash.
    auto pair = CScraperManifest::mapManifest.find(nManifestHash);
    const CScraperManifest& manifest = *pair->second;

    // Write out to files the parts. Note this assumes one-to-one part to file. Needs to
    // be fixed for more than one part per file.
    int iPartNum = 0;
    for (const auto& iter : manifest.vParts)
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
            _log(logattribute::ERR, "ScraperSaveCScraperManifestToFiles", "Failed to open file (" + outputfile + ")");

            return false;
        }

        outfile.write((const char*)iter->data.data(), iter->data.size());

        outfile.flush();
        outfile.close();

        iPartNum++;
    }

    if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

    return true;
}

// The idea here is that there are two levels of authorization. The first level is whether any
// node can operate as a "scraper", in other words, download the stats files themselves.
// The second level, which is the IsScraperAuthorizedToBroadcastManifests() function,
// is to authorize a particular node to actually be able to publish manifests.
// The second function is intended to override the first, with the first being a network wide
// policy. So to be clear, if the network wide policy has IsScraperAuthorized() set to false
// then ONLY nodes that have IsScraperAuthorizedToBroadcastManifests() can download stats at all.
// If IsScraperAuthorized() is set to true, then you have two levels of operation allowed.
// Nodes can run -scraper and download stats for themselves. They will only be able to publish
// manifests if for that node IsScraperAuthorizedToBroadcastManifests() evaluates to true.
// This allows flexibility in network policy, and will allow us to convert from a scraper based
// approach to convergence back to individual node stats download and convergence without a lot of
// headaches.
bool IsScraperAuthorized()
{
    return ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD;
}

// This checks to see if the local node is authorized to publish manifests. Note that this code could be
// modified to bypass this check, so messages sent will also be validated on receipt by the complement
// to this function, IsManifestAuthorized(CKey& Key) in the CScraperManifest class.
bool IsScraperAuthorizedToBroadcastManifests(CBitcoinAddress& AddressOut, CKey& KeyOut)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    AppCacheSection mScrapers = ReadCacheSection(Section::SCRAPER);

    std::string sScraperAddressFromConfig = GetArg("-scraperkey", "false");

    // Check against the -scraperkey config entry first and return quickly to avoid extra work.
    // If the config entry exists and is in the map (i.e. in the appcache)...
    auto entry = mScrapers.find(sScraperAddressFromConfig);

    // If the address (entry) exists in the config and appcache...
    if (sScraperAddressFromConfig != "false" && entry != mScrapers.end())
    {
        if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Entry from config/command line found in AppCache.");

        // ... and is enabled...
        if (entry->second.value == "true" || entry->second.value == "1")
        {
            if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Entry in appcache is enabled.");

            CBitcoinAddress address(sScraperAddressFromConfig);
            CPubKey ScraperPubKey(ParseHex(sScraperAddressFromConfig));

            CKeyID KeyID;
            address.GetKeyID(KeyID);

            // ... and the address is valid...
            if (address.IsValid())
            {
                if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The address is valid.");
                if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "(Doublecheck) The address is " + address.ToString());

                // ... and it exists in the wallet...
                if (pwalletMain->GetKey(KeyID, KeyOut))
                {
                    // ... and the key returned from the wallet is valid and matches the provided public key...
                    assert(KeyOut.IsValid());

                    if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The wallet key for the address is valid.");

                    AddressOut = address;

                    // Note that KeyOut here will have the correct key to use by THIS node.

                    _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests",
                         "Found address " + sScraperAddressFromConfig + " in both the wallet and appcache. \n"
                         "This scraper is authorized to publish manifests.");

                    return true;
                }
            }
        }
    }
    // If a -scraperkey config entry has not been specified, we will walk through all of the addresses in the wallet
    // until we hit the first one that is in the list.
    else
    {
        for (auto const& item : pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;

            std::string sScraperAddress = address.ToString();
            if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Checking address " + sScraperAddress);

            entry = mScrapers.find(sScraperAddress);

            // The address is found in the appcache...
            if (entry != mScrapers.end())
            {
                if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Entry found in AppCache");

                // ... and is enabled...
                if (entry->second.value == "true" || entry->second.value == "1")
                {
                    if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "AppCache entry enabled");

                    CKeyID KeyID;
                    address.GetKeyID(KeyID);

                    // ... and the address is valid...
                    if (address.IsValid())
                    {
                        if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The address is valid.");
                        if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "(Doublecheck) The address is " + address.ToString());

                        // ... and it exists in the wallet... (It SHOULD here... it came from the map...)
                        if (pwalletMain->GetKey(KeyID, KeyOut))
                        {
                            // ... and the key returned from the wallet is valid ...
                            assert(KeyOut.IsValid());

                            if (fDebug) _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The wallet key for the address is valid.");

                            AddressOut = address;

                            // Note that KeyOut here will have the correct key to use by THIS node.

                            _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests",
                                 "Found address " + sScraperAddress + " in both the wallet and appcache. \n"
                                 "This scraper is authorized to publish manifests.");

                            return true;
                        }
                    }
                }
            }
        }
    }

    // If we made it here, there is no match or valid key in the wallet

    _log(logattribute::WARNING, "IsScraperAuthorizedToBroadcastManifests", "No key found in wallet that matches authorized scrapers in appcache.");

    return false;
}


// This function is necessary because some CScraperManifest messages are likely to be received before the wallet is in sync. Therefore, they
// cannot be checked at that time by the deserialize check. Instead, while the wallet is not in sync, the local CScraperManifest flag
// bCheckedAuthorized will be set to false on any manifests received during that time. Once the wallet is in sync, this function will be
// called and will walk the mapManifest and check all Manifests to ensure the PubKey in the manifest is in the
// authorized scraper list in the AppCache. If it passes the flag will be set to true. If it fails, the manifest will be deleted. All manifests
// must be checked, because we have to deal with another condition where a scraper is deauthorized by network policy. This means manifests may
// not be authorized even if the bCheckedAuthorized is true from a prior check.

// A lock needs to be taken on CScraperManifest::cs_mapManifest before calling this function.
unsigned int ScraperDeleteUnauthorizedCScraperManifests()
{
    unsigned int nDeleted = 0;

    for (auto iter = CScraperManifest::mapManifest.begin(); iter != CScraperManifest::mapManifest.end(); )
    {
        auto iter_copy = iter;

        CScraperManifest& manifest = *iter->second;

        // We are not going to do anything with the banscore here, but it is an out parameter of IsManifestAuthorized.
        unsigned int banscore_out = 0;

        if (CScraperManifest::IsManifestAuthorized(manifest.pubkey, banscore_out))
            manifest.bCheckedAuthorized = true;
        else
        {
            _log(logattribute::WARNING, "ScraperDeleteUnauthorizedCScraperManifests", "Deleting unauthorized manifest with hash " + iter->first.GetHex());
            // Delete from CScraperManifest map
            ScraperDeleteCScraperManifest(iter_copy->first);
            nDeleted++;
        }

        ++iter;
    }

    return nDeleted;
}


// A lock needs to be taken on cs_StructScraperFileManifest for this function.
// The sCManifestName is the public key of the scraper in address form.
bool ScraperSendFileManifestContents(CBitcoinAddress& Address, CKey& Key)
{
    // This "broadcasts" the current ScraperFileManifest contents to the network.

    auto manifest = std::unique_ptr<CScraperManifest>(new CScraperManifest());

    // The manifest name is the authorized address of the scraper.
    manifest->sCManifestName = Address.ToString();

    // Also store local sCManifestName, because the manifest will be std::moved by addManifest.
    std::string sCManifestName = Address.ToString();

    manifest->nTime = StructScraperFileManifest.timestamp;

    // Also store local nTime, because the manifest will be std::moved by addManifest.
    int64_t nTime = StructScraperFileManifest.timestamp;

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
        _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to open file (" + inputfile.string() + ")");
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
        _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to read file (" + inputfile.string() + ")");
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

        //_log(logattribute::INFO, "ScraperSendFileManifestContents", "Input file for CScraperManifest is " + inputfile.string());

        fs::path inputfilewpath = pathScraper / inputfile;

        // open input file, and associate with CAutoFile
        FILE *file = fopen(inputfilewpath.string().c_str(), "rb");
        CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);

        if (!filein)
        {
            _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to open file (" + inputfile.string() + ")");
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
            _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to read file (" + inputfile.string() + ")");
            return false;
        }

        filein.fclose();


        CScraperManifest::dentry ProjectEntry;

        ProjectEntry.project = entry.second.project;
        std::string sProject = entry.second.project + "-";

        std::string sinputfile = inputfile.string();
        std::string suffix = ".csv.gz";

        // Remove project-
        sinputfile.erase(sinputfile.find(sProject), sProject.length());
        // Remove suffix. What is left is the ETag.
        ProjectEntry.ETag = sinputfile.erase(sinputfile.find(suffix), suffix.length());

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

    // "Sign" and "send".

    LOCK(CScraperManifest::cs_mapManifest);

    bool bAddManifestSuccessful = CScraperManifest::addManifest(std::move(manifest), Key);

    if (fDebug)
    {
        if (bAddManifestSuccessful)
            _log(logattribute::INFO, "ScraperSendFileManifestContents", "addManifest (send) from this scraper (address "
                 + sCManifestName + ") successful, timestamp "
                 + DateTimeStrFormat("%x %H:%M:%S", nTime));
        else
            _log(logattribute::ERR, "ScraperSendFileManifestContents", "addManifest (send) from this scraper (address "
                 + sCManifestName + ") FAILED, timestamp "
                 + DateTimeStrFormat("%x %H:%M:%S", nTime));
    }

    return bAddManifestSuccessful;
}



// ------------------------------------ This an out parameter.
bool ScraperConstructConvergedManifest(ConvergedManifest& StructConvergedManifest)
{
    bool bConvergenceSuccessful = false;

    // get the whitelist to fill out the excluded projects vector later on. This is currently a global
    // But may be changed to a passed parameter later. It is a small vector, so copy the global into a
    // local to avoid holding the lock for extended periods.

    std::vector<std::pair<std::string, std::string>> vwhitelist_local {};

    {
        LOCK(cs_vwhitelist);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_vwhitelist");

        vwhitelist_local = vwhitelist;
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_vwhitelist");
    }

    // Call ScraperDeleteCScraperManifests() to ensure we have culled old manifests. This will
    // return a map of manifests binned by Scraper after the culling.
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper = ScraperDeleteCScraperManifests();
    
    // Do a map for unique manifest times ordered by descending time then content hash.
    std::multimap<int64_t, uint256, greater<int64_t>> mManifestsBinnedByTime;
    // and also by content hash, then scraperID and manifest (not content) hash.
    std::multimap<uint256, std::pair<ScraperID, uint256>> mManifestsBinnedbyContent;
    std::multimap<uint256, std::pair<ScraperID, uint256>>::iterator convergence;
    
    unsigned int nScraperCount = mMapCSManifestsBinnedByScraper.size();

    _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Number of Scrapers with manifests = " + std::to_string(nScraperCount));

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
            // ------------------------------------------------  manifest CONTENT hash
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
                if (fDebug3) _log(logattribute::INFO, "ScraperConstructConvergedManifest", "mManifestsBinnedbyContent insert, timestamp "
                                  + DateTimeStrFormat("%x %H:%M:%S", iter_inner.first)
                                  + ", content hash "+ iter_inner.second.second.GetHex()
                                  + ", scraper ID " + iter.first
                                  + ", manifest hash" + iter_inner.second.first.GetHex());
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

            _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Found convergence on manifest " + convergence->second.second.GetHex()
                 + " at " + DateTimeStrFormat("%x %H:%M:%S",  iter.first)
                 + " with " + std::to_string(nIdenticalContentManifestCount) + " scrapers out of " + std::to_string(nScraperCount)
                 + " agreeing.");

            bConvergenceSuccessful = true;

            // Note this break is VERY important, it prevents considering essentially the same manifest that meets convergence multiple times.
            break;
        }
    }

    if (bConvergenceSuccessful)
    {
        LOCK(CScraperManifest::cs_mapManifest);
        if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

        // Select agreed upon (converged) CScraper manifest based on converged hash.
        auto pair = CScraperManifest::mapManifest.find(convergence->second.second);
        const CScraperManifest& manifest = *pair->second;

        // Fill out the ConvergedManifest structure. Note this assumes one-to-one part to project statistics BLOB. Needs to
        // be fixed for more than one part per BLOB. This is easy in this case, because it is all from/referring to one manifest.

        StructConvergedManifest.ConsensusBlock = manifest.ConsensusBlock;
        StructConvergedManifest.timestamp = GetAdjustedTime();
        StructConvergedManifest.bByParts = false;

        int iPartNum = 0;
        CDataStream ss(SER_NETWORK,1);
        WriteCompactSize(ss, manifest.vParts.size());
        uint256 nContentHashCheck;

        for (const auto& iter : manifest.vParts)
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

        if (nContentHashCheck != convergence->first)
        {
            bConvergenceSuccessful = false;
            _log(logattribute::ERR, "ScraperConstructConvergedManifest", "Selected Converged Manifest content hash check failed! nContentHashCheck = "
                 + nContentHashCheck.GetHex() + " and nContentHash = " + StructConvergedManifest.nContentHash.GetHex());
            // Reinitialize StructConvergedManifest
            StructConvergedManifest = {};
        }
        else // Content matches so we have a confirmed convergence.
        {
            // The ConvergedManifest content hash is NOT the same as the hash above from the CScraper::manifest, because it needs to be in the order of the
            // map key and on the data, not the order of vParts by the part hash. So, unfortunately, we have to walk through the map again to hash it correctly.
            CDataStream ss2(SER_NETWORK,1);
            for (const auto& iter : StructConvergedManifest.ConvergedManifestPartsMap)
                ss2 << iter.second;

            StructConvergedManifest.nContentHash = Hash(ss2.begin(), ss2.end());

            // Fill out the excluded projects vector...
            for (const auto& iProjects : vwhitelist_local)
            {
                if (StructConvergedManifest.ConvergedManifestPartsMap.find(iProjects.first) == StructConvergedManifest.ConvergedManifestPartsMap.end())
                {
                    // Project in whitelist was not in the map, so it goes in the exclusion vector.
                    //StructConvergedManifest.vExcludedProjects.push_back(std::make_pair(iProjects.first, "Converged manifests (agreed by multiple scrapers) excluded project."));
                    //_log(logattribute::WARNING, "ScraperConstructConvergedManifestByProject", "Project "
                    //     + iProjects.first
                    //     + " was excluded because the converged manifests from the scrapers all excluded the project.");

                    // To deal with a corner case of a medium term project fallout at the head of the list (more recent) where it is still available within
                    // the 48 hour retention window, fallback to project level convergence to attempt to recover the project.
                    _log(logattribute::WARNING, "ScraperConstructConvergedManifestByProject", "Project "
                         + iProjects.first
                         + " was excluded because the converged manifests from the scrapers all excluded the project. \n"
                         + "Falling back to attempt convergence by project to try and recover excluded project.");
                    
                    bConvergenceSuccessful = false;
                    
                    // Since we are falling back to project level and discarding this convergence, no need to process any more once one missed project is found.
                    break;
                }
            }
        }

        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");
    }

    if (!bConvergenceSuccessful)
    {
        _log(logattribute::INFO, "ScraperConstructConvergedManifest", "No convergence on manifests by content at the manifest level.");

        // Reinitialize StructConvergedManifest
        StructConvergedManifest = {};

        // Try to form a convergence by project objects (parts)...
        bConvergenceSuccessful = ScraperConstructConvergedManifestByProject(vwhitelist_local, mMapCSManifestsBinnedByScraper, StructConvergedManifest);

        // If we have reached here. All attempts at convergence have failed. Reinitialize StructConvergedManifest to eliminate stale or
        // partially filled-in data.
        if (!bConvergenceSuccessful)
            StructConvergedManifest = {};
    }

    // Signal UI of the status of convergence attempt.
    if (bConvergenceSuccessful)
        uiInterface.NotifyScraperEvent(scrapereventtypes::Convergence, CT_NEW, {});
    else
        uiInterface.NotifyScraperEvent(scrapereventtypes::Convergence, CT_DELETED, {});

    return bConvergenceSuccessful;
    
}

// Subordinate function to ScraperConstructConvergedManifest to try to find a convergence at the Project (part) level
// if there is no convergence at the manifest level.
// ------------------------------------------------------------------------ In ------------------------------------------------- Out
bool ScraperConstructConvergedManifestByProject(std::vector<std::pair<std::string, std::string>> &vwhitelist_local,
                                                mmCSManifestsBinnedByScraper& mMapCSManifestsBinnedByScraper, ConvergedManifest& StructConvergedManifest)
{
    bool bConvergenceSuccessful = false;

    CDataStream ss(SER_NETWORK,1);
    uint256 nConvergedConsensusBlock = 0;
    int64_t nConvergedConsensusTime = 0;
    uint256 nManifestHashForConvergedBeaconList = 0;

    // We are going to do this for each project in the whitelist.
    unsigned int iCountSuccesfulConvergedProjects = 0;
    unsigned int nScraperCount = mMapCSManifestsBinnedByScraper.size();

    _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Number of Scrapers with manifests = " + std::to_string(nScraperCount));

    for (const auto& iWhitelistProject : vwhitelist_local)
    {
        // Do a map for unique ProjectObject times ordered by descending time then content hash. Note that for Project Objects (Parts),
        // the content hash is the object hash. We also need the consensus block here, because we are "composing" the manifest by
        // parts, so we will need to choose the latest consensus block by manifest time. This will occur naturally below if tracked in
        // this manner. We will also want the BeaconList from the associated manifest.
        // ------ manifest time --- object hash - consensus block hash - manifest hash.
        std::multimap<int64_t, std::tuple<uint256, uint256, uint256>, greater<int64_t>> mProjectObjectsBinnedByTime;
        // and also by project object (content) hash, then scraperID and project.
        std::multimap<uint256, std::pair<ScraperID, std::string>> mProjectObjectsBinnedbyContent;
        std::multimap<uint256, std::pair<ScraperID, std::string>>::iterator ProjectConvergence;

        // For the selected project in the whitelist, walk each scraper.
        for (const auto& iter : mMapCSManifestsBinnedByScraper)
        {
            // iter.second is the mCSManifest. Walk each manifest in each scraper.
            for (const auto& iter_inner : iter.second)
            {
                // This is the referenced CScraperManifest hash
                uint256 nCSManifestHash = iter_inner.second.first;

                LOCK(CScraperManifest::cs_mapManifest);
                if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

                // Select manifest based on provided hash.
                auto pair = CScraperManifest::mapManifest.find(nCSManifestHash);
                CScraperManifest& manifest = *pair->second;

                // Find the part number in the manifest that corresponds to the whitelisted project.
                // Once we find a part that corresponds to the selected project in the given manifest, then break,
                // because there can only be one part in a manifest corresponding to a given project.
                int nPart = -1;
                int64_t nProjectObjectTime = 0;
                uint256 nProjectObjectHash = 0;
                for (const auto& vectoriter : manifest.projects)
                {
                    if (vectoriter.project == iWhitelistProject.first)
                    {
                        nPart = vectoriter.part1;
                        nProjectObjectTime = vectoriter.LastModified;
                        break;
                    }
                }

                // Part -1 means not found, Part 0 is the beacon list, so needs to be greater than zero.
                if (nPart > 0)
                {
                    // Get the hash of the part referenced in the manifest.
                    nProjectObjectHash = manifest.vParts[nPart]->hash;

                    // Insert into mManifestsBinnedByTime multimap.
                    mProjectObjectsBinnedByTime.insert(std::make_pair(nProjectObjectTime, std::make_tuple(nProjectObjectHash, manifest.ConsensusBlock, *manifest.phash)));

                    // Even though this is a multimap on purpose because we are going to count occurances of the same key,
                    // We need to prevent the insertion of a second entry with the same content from the same scraper. This is
                    // even more true here at the part level than at the manifest level, because if both SCRAPER_CMANIFEST_RETAIN_NONCURRENT
                    // and SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES are true, then there can be many references
                    // to the same part by different manifests of the same scraper in addition to across scrapers.
                    auto range = mProjectObjectsBinnedbyContent.equal_range(nProjectObjectHash);
                    bool bAlreadyExists = false;
                    for (auto iter3 = range.first; iter3 != range.second; ++iter3)
                    {
                        // ---- ScraperID ------ Candidate scraperID to insert
                        if (iter3->second.first == iter.first)
                            bAlreadyExists = true;
                    }

                    if (!bAlreadyExists)
                    {
                        // Insert into mProjectObjectsBinnedbyContent -------- content hash ------------------- ScraperID -------- Project.
                        mProjectObjectsBinnedbyContent.insert(std::make_pair(nProjectObjectHash, std::make_pair(iter.first, iWhitelistProject.first)));
                        if (fDebug3) _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "mManifestsBinnedbyContent insert "
                                         + nProjectObjectHash.GetHex() + ", " + iter.first + ", " + iWhitelistProject.first);
                    }
                }

                if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");
            }
        }

        // Walk the time map (backwards in time because the sort order is descending), and select the first
        // Project Part (Object) content hash that meets the convergence rule.
        for (const auto& iter : mProjectObjectsBinnedByTime)
        {
            // Notice the below is NOT using the time. We switch to the content only. The time is only used to make sure
            // we test the convergence of the project objects in time order, but once a content hash is selected based on the time,
            // only the content hash is used to count occurrences in the multimap, because the times for the same
            // project object (part hash) will be different across different manifests and different scrapers.
            unsigned int nIdenticalContentManifestCount = mProjectObjectsBinnedbyContent.count(std::get<0>(iter.second));
            if (nIdenticalContentManifestCount >= NumScrapersForSupermajority(nScraperCount))
            {
                // Find the first one of equivalent parts ------------------ by hash.
                ProjectConvergence = mProjectObjectsBinnedbyContent.find(std::get<0>(iter.second));

                _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Found convergence on project object " + ProjectConvergence->first.GetHex()
                     + " for project " + iWhitelistProject.first
                     + " with " + std::to_string(nIdenticalContentManifestCount) + " scrapers out of " + std::to_string(nScraperCount)
                     + " agreeing.");

                // Get the actual part ----------------- by hash.
                auto iPart = CSplitBlob::mapParts.find(std::get<0>(iter.second));

                uint256 nContentHashCheck = Hash(iPart->second.data.begin(), iPart->second.data.end());

                if (nContentHashCheck != iPart->first)
                {
                    _log(logattribute::ERR, "ScraperConstructConvergedManifestByProject", "Selected Converged Project Object content hash check failed! nContentHashCheck = "
                         + nContentHashCheck.GetHex() + " and nContentHash = " + iPart->first.GetHex());
                    break;
                }

                // Put Project Object (Part) in StructConvergedManifest keyed by project.
                StructConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair(iWhitelistProject.first, iPart->second.data));

                // If the indirectly referenced manifest has a consensus time that is greater than already recorded, replace with that time, and also
                // change the consensus block to the referred to consensus block. (Note that this is scoped at even above the individual project level, so
                // the result after iterating through all projects will be the latest manifest time and consensus block that corresponds to any of the
                // parts that meet convergence.) We will also get the manifest hash too, so we can retrieve the associated BeaconList that was used.
                if (iter.first > nConvergedConsensusTime)
                {
                    nConvergedConsensusTime = iter.first;
                    nConvergedConsensusBlock = std::get<1>(iter.second);
                    nManifestHashForConvergedBeaconList = std::get<2>(iter.second);
                }

                iCountSuccesfulConvergedProjects++;

                // Note this break is VERY important, it prevents considering essentially the same project object that meets convergence multiple times.
                break;
            }
        }
    }

    // If we meet the rule of CONVERGENCE_BY_PROJECT_RATIO, then proceed to fill out the rest of the map.
    if ((double)iCountSuccesfulConvergedProjects / (double)vwhitelist_local.size() >= CONVERGENCE_BY_PROJECT_RATIO)
    {
        // Fill out the the rest of the ConvergedManifest structure. Note this assumes one-to-one part to project statistics BLOB. Needs to
        // be fixed for more than one part per BLOB. This is easy in this case, because it is all from/referring to one manifest.

        // Lets use the BeaconList from the manifest referred to by nManifestHashForConvergedBeaconList. Technically there is no exact answer to
        // the BeaconList that should be used in the convergence when putting it together at the individual part level, because each project part
        // could have used a different BeaconList (subject to the consensus ladder. It makes sense to use the "newest" one that is associated
        // with a manifest that has the newest part associated with a successful part (project) level convergence.

        LOCK(CScraperManifest::cs_mapManifest);
        if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

        // Select manifest based on provided hash.
        auto pair = CScraperManifest::mapManifest.find(nManifestHashForConvergedBeaconList);
        CScraperManifest& manifest = *pair->second;

        // The vParts[0] is always the BeaconList.
        StructConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair("BeaconList", manifest.vParts[0]->data));

        StructConvergedManifest.ConsensusBlock = nConvergedConsensusBlock;

        // The ConvergedManifest content hash is in the order of the map key and on the data.
        for (const auto& iter : StructConvergedManifest.ConvergedManifestPartsMap)
            ss << iter.second;

        StructConvergedManifest.nContentHash = Hash(ss.begin(), ss.end());
        StructConvergedManifest.timestamp = GetAdjustedTime();
        StructConvergedManifest.bByParts = true;

        bConvergenceSuccessful = true;

        _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Successful convergence by project: "
             + std::to_string(iCountSuccesfulConvergedProjects) + " out of " + std::to_string(vwhitelist_local.size())
             + " projects at "
             + DateTimeStrFormat("%x %H:%M:%S",  StructConvergedManifest.timestamp));

        // Fill out the excluded projects vector...
        for (const auto& iProjects : vwhitelist_local)
        {
            if (StructConvergedManifest.ConvergedManifestPartsMap.find(iProjects.first) == StructConvergedManifest.ConvergedManifestPartsMap.end())
            {
                // Project in whitelist was not in the map, so it goes in the exclusion vector.
                StructConvergedManifest.vExcludedProjects.push_back(std::make_pair(iProjects.first, "No convergence was found at the fallback (project) level."));
                _log(logattribute::WARNING, "ScraperConstructConvergedManifestByProject", "Project "
                     + iProjects.first
                     + " was excluded because there was no convergence from the scrapers for this project at the project level.");
            }
        }

        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");
    }

    if (!bConvergenceSuccessful)
        _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "No convergence on manifests by projects.");

    return bConvergenceSuccessful;

}


// A lock should be taken on CScraperManifest::cs_Manifest before calling this function.
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

    _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Deleting old CScraperManifests.");

    LOCK(CScraperManifest::cs_mapManifest);
    if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

    // First check for unauthorized manifests just in case a scraper has been deauthorized.
    unsigned int nDeleted = ScraperDeleteUnauthorizedCScraperManifests();
    if (nDeleted)
        _log(logattribute::WARNING, "ScraperDeleteCScraperManifests", "Deleted " + std::to_string(nDeleted) + " unauthorized manifests.");

    // Bin by scraper and order by manifest time within scraper bin.
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper = BinCScraperManifestsByScraper();

    _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "mMapCSManifestsBinnedByScraper size = " + std::to_string(mMapCSManifestsBinnedByScraper.size()));

    if (!SCRAPER_CMANIFEST_RETAIN_NONCURRENT)
    {
        // For each scraper, delete every manifest EXCEPT the latest.
        for (auto iter = mMapCSManifestsBinnedByScraper.begin(); iter != mMapCSManifestsBinnedByScraper.end(); ++iter)
        {
            mCSManifest mManifestInner = iter->second;

            _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "mManifestInner size = " + std::to_string(mManifestInner.size()) +
                 " for " + iter->first + " scraper");

            // This preserves the LATEST CScraperManifest entry for the given scraper, because the inner map is in descending order,
            // and the first element is therefore the LATEST, and is skipped.
            for (auto iter_inner = ++mManifestInner.begin(); iter_inner != mManifestInner.end(); ++iter_inner)
            {
                
                _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Deleting non-current manifest " + iter_inner->second.first.GetHex()
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
            _log(logattribute::INFO, "Scraper", "Deleting old CScraperManifest with hash " + iter->first.GetHex());
            // Delete from CScraperManifest map
            ScraperDeleteCScraperManifest(iter_copy->first);
        }
        
        ++iter;
    }

    // Reload mMapCSManifestsBinnedByScraper after deletions. This is not particularly efficient, but the map is not
    // that large. (The lock on CScraperManifest::cs_mapManifest is still held from above.)
    mMapCSManifestsBinnedByScraper = BinCScraperManifestsByScraper();

    if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

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

    _log(logattribute::INFO, "LoadBeaconListFromConvergedManifest", "mBeaconMap element count: " + std::to_string(mBeaconMap.size()));

    return true;
}


// A lock should be taken on CScraperManifest::cs_mapManifest before calling this function.
bool ScraperDeleteCScraperManifest(uint256 nManifestHash)
{
    // This deletes a manifest from the map.
    
    // Select manifest based on provided hash.
    auto pair = CScraperManifest::mapManifest.find(nManifestHash);
    CScraperManifest& manifest = *pair->second;

    bool ret = manifest.DeleteManifest(nManifestHash);

    return ret;
}


/***********************
*    Neural Network    *
************************/


std::string GenerateSBCoreDataFromScraperStats(ScraperStats& mScraperStats)
{
    stringbuilder xmlout;

    xmlout.append("<AVERAGES>");

    // The <AVERAGES> in the SB core data are actually the project level
    for (auto const& entry : mScraperStats)
    {
        if (entry.first.objecttype == statsobjecttype::byProject)
        {
            xmlout.append(entry.first.objectID);
            xmlout.append(",");
            xmlout.fixeddoubleappend(entry.second.statsvalue.dAvgRAC, 0);
            xmlout.append(",");
            xmlout.fixeddoubleappend(entry.second.statsvalue.dRAC, 0);
            xmlout.append(";");
        }
    }

    // Find the single network wide NN entry and put in string.
    ScraperObjectStatsKey StatsKey;
    StatsKey.objecttype = statsobjecttype::NetworkWide;
    StatsKey.objectID = "";

    const auto iter = mScraperStats.find(StatsKey);

    xmlout.append("NeuralNetwork");
    xmlout.append(",");
    xmlout.fixeddoubleappend(iter->second.statsvalue.dAvgRAC, 0);
    xmlout.append(",");
    xmlout.fixeddoubleappend(iter->second.statsvalue.dRAC, 0);
    xmlout.append(";");

    xmlout.append("</AVERAGES>");

    xmlout.append("<QUOTES>btc,0;grc,0;</QUOTES>");

    xmlout.append("<MAGNITUDES>");

    // The <MAGNITUDES> in the SB core data are actually at the CPID level.
    unsigned int nZeros = 0;
    for (auto const& entry : mScraperStats)
    {
        if (entry.first.objecttype == statsobjecttype::byCPID)
        {
            // If the magnitude entry is zero suppress the CPID and increment the zero counter.
            if (std::round(entry.second.statsvalue.dMag) > 0)
            {
                xmlout.append(entry.first.objectID);
                xmlout.append(",");
                xmlout.fixeddoubleappend(entry.second.statsvalue.dMag, 0);
                xmlout.append(";");
            }
            else
                nZeros++;
        }
    }

    // Put all of the zero CPID mags at the end with 15,0; entries.
    // TODO: This should be replaced with a <ZERO>X</ZERO> block as in the packed version
    // at the next mandatory after the new NN rollout. This will require a change to the packer conditioned on the bv.
    for (unsigned int i = 1; i <= nZeros; i++)
        xmlout.append("0,15;");

    xmlout.append("</MAGNITUDES>");

    std::string sSBCoreData = xmlout.value();

    _log(logattribute::INFO, "GenerateSBCoreDataFromScraperStats", "Generated SB Core Data.");

    return sSBCoreData;
}
//*/

std::string ScraperGetNeuralContract(bool bStoreConvergedStats, bool bContractDirectFromStatsUpdate)
{
    // If not in sync then immediately bail with a empty string.
    if (OutOfSyncByAge())
        return std::string();

    // Check the age of the ConvergedScraperStats cache. If less than nScraperSleep / 1000 old (for seconds), then simply report back the cache contents.
    // This prevents the relatively heavyweight stats computations from running too often. The time here may not exactly align with
    // the scraper loop if it is running, but that is ok. The scraper loop updates the time in the cache too.
    bool bConvergenceUpdateNeeded = true;
    {
        LOCK(cs_ConvergedScraperStatsCache);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

        if (GetAdjustedTime() - ConvergedScraperStatsCache.nTime < (nScraperSleep / 1000))
            bConvergenceUpdateNeeded = false;

        // End LOCK(cs_ConvergedScraperStatsCache)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
    }

    ConvergedManifest StructConvergedManifest;
    BeaconMap mBeaconMap;
    std::string sSBCoreData;

    // if bConvergenceUpdate is needed, and...
    // If bContractDirectFromStatsUpdate is set to true, this means that this is being called from
    // ScraperSynchronizeDPOR() in fallback mode to force a single shot update of the stats files and
    // direct generation of the contract from the single shot run. This will return immediately with a blank if
    // IsScraperAuthorized() evaluates to false, because that means that by network policy, no non-scraper
    // stats downloads are allowed by unauthorized scraper nodes.
    // (If bConvergenceUpdate is not needed, then the scraper is operating by convergence already...
    if (bConvergenceUpdateNeeded)
    {
        if (!bContractDirectFromStatsUpdate)
        {
            // ScraperConstructConvergedManifest also culls old CScraperManifests. If no convergence, then
            // you can't make a SB core and you can't make a contract, so return the empty string.
            if (ScraperConstructConvergedManifest(StructConvergedManifest))
            {
                // This is to display the element count in the beacon map.
                LoadBeaconListFromConvergedManifest(StructConvergedManifest, mBeaconMap);

                ScraperStats mScraperConvergedStats = GetScraperStatsByConvergedManifest(StructConvergedManifest);

                _log(logattribute::INFO, "ScraperGetNeuralContract", "mScraperStats has the following number of elements: " + std::to_string(mScraperConvergedStats.size()));

                if (bStoreConvergedStats)
                {
                    if (!StoreStats(pathScraper / "ConvergedStats.csv.gz", mScraperConvergedStats))
                        _log(logattribute::ERR, "ScraperGetNeuralContract", "StoreStats error occurred");
                    else
                        _log(logattribute::INFO, "ScraperGetNeuralContract", "Stored converged stats.");
                }

                // I know this involves a copy operation, but it minimizes the lock time on the cache... we may want to
                // lock before and do a direct assignment, but that will lock the cache for the whole stats computation,
                // which is not really necessary.
                {
                    LOCK(cs_ConvergedScraperStatsCache);
                    if (fDebug) _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

                    std::string sSBCoreDataPrev = ConvergedScraperStatsCache.sContract;

                    sSBCoreData = GenerateSBCoreDataFromScraperStats(mScraperConvergedStats);

                    ConvergedScraperStatsCache.mScraperConvergedStats = mScraperConvergedStats;
                    ConvergedScraperStatsCache.nTime = GetAdjustedTime();
                    ConvergedScraperStatsCache.sContractHash = ScraperGetNeuralHash(sSBCoreData);
                    ConvergedScraperStatsCache.sContract = sSBCoreData;
                    ConvergedScraperStatsCache.vExcludedProjects = StructConvergedManifest.vExcludedProjects;

                    // Signal UI of SBContract status
                    if (!sSBCoreData.empty())
                    {
                        if (!sSBCoreDataPrev.empty())
                        {
                            // If the current is not empty and the previous is not empty and not the same, then there is an updated contract.
                            if (sSBCoreData != sSBCoreDataPrev)
                                uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_UPDATED, {});
                        }
                        else
                            // If the previous was empty and the current is not empty, then there is a new contract.
                            uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_NEW, {});
                    }
                    else
                        if (!sSBCoreDataPrev.empty())
                            // If the current is empty and the previous was not empty, then the contract has been deleted.
                            uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_DELETED, {});

                    // End LOCK(cs_ConvergedScraperStatsCache)
                    if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
                }


                if (fDebug)
                    _log(logattribute::INFO, "ScraperGetNeuralContract", "SB Core Data from convergence \n" + sSBCoreData);
                else
                    _log(logattribute::INFO, "ScraperGetNeuralContract", "SB Core Data from convergence");

                return sSBCoreData;
            }
            else
                return std::string();
        }
        // If bContractDirectFromStatsUpdate is true, then this is the single shot pass.
        else if (IsScraperAuthorized())
        {
            // This part is the "second trip through from ScraperSynchronizeDPOR() as a fallback, if
            // authorized.

            // Do a single shot through the main scraper function to update all of the files.
            ScraperSingleShot();

            // Notice there is NO update to the ConvergedScraperStatsCache here, as that is not
            // appropriate for the single shot.
            ScraperStats mScraperStats = GetScraperStatsByConsensusBeaconList();
            sSBCoreData = GenerateSBCoreDataFromScraperStats(mScraperStats);

            // Signal the UI there is a contract.
            if(!sSBCoreData.empty())
                uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_NEW, {});

            if (fDebug)
                _log(logattribute::INFO, "ScraperGetNeuralContract", "SB Core Data from single shot\n" + sSBCoreData);
            else
                _log(logattribute::INFO, "ScraperGetNeuralContract", "SB Core Data from single shot");

            return sSBCoreData;
        }
    }
    else
    {
        // If we are here, we are using cached information.

        LOCK(cs_ConvergedScraperStatsCache);
        if (fDebug) _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

        sSBCoreData = ConvergedScraperStatsCache.sContract;

        if (fDebug)
            _log(logattribute::INFO, "ScraperGetNeuralContract", "SB Core Data from cached converged stats\n" + sSBCoreData);
        else
            _log(logattribute::INFO, "ScraperGetNeuralContract", "SB Core Data from cached converged stats");

        // End LOCK(cs_ConvergedScraperStatsCache)
        if (fDebug) _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");

    }

    return sSBCoreData;
}


// Note: This is simply a wrapper around GetQuorumHash in main.cpp for compatibility purposes. See the comments below.
std::string ScraperGetNeuralHash()
{
    std::string sNeuralContract = ScraperGetNeuralContract(false, false);

    std::string sHash;

    //sHash = Hash(sNeuralContract.begin(), sNeuralContract.end()).GetHex();

    // This is the hash currently used by the old NN. Continue to use it for compatibility purpose until the next mandatory
    // after the new NN rollout, when the old NN hash function can be retired in favor of the commented out code above.
    // The intent will be to uncomment out the above line, and then reverse the roles of ScraperGetNeuralHash() and
    // GetQuorumHash().
    sHash = GetQuorumHash(sNeuralContract);

    return sHash;
}


// Note: This is simply a wrapper around GetQuorumHash in main.cpp for compatibility purposes. See the comments below.
std::string ScraperGetNeuralHash(std::string sNeuralContract)
{
    std::string sHash;

    //sHash = Hash(sNeuralContract.begin(), sNeuralContract.end()).GetHex();

    // This is the hash currently used by the old NN. Continue to use it for compatibility purpose until the next mandatory
    // after the new NN rollout, when the old NN hash function can be retired in favor of the commented out code above.
    // The intent will be to uncomment out the above line, and then reverse the roles of ScraperGetNeuralHash() and
    // GetQuorumHash().
    sHash = GetQuorumHash(sNeuralContract);

    return sHash;
}


bool ScraperSynchronizeDPOR()
{
    bool bStatus = false;

    // First check to see if there is already a scraper convergence and a contract formable. If so, then no update needed.
    // If the appropriate scrapers are running and the node is in communication, then this is most likely going to be
    // the path, which means very little work.
    std::string sNeuralContract = ScraperGetNeuralContract(false, false);

    if (sNeuralContract != "")
    {
        bStatus = true;
        // Return immediately here if successful, otherwise fallback to direct download if authorized.
        return bStatus;
    }
    else
    {
        // Try again with the bool bContractDirectFromStatsUpdate set to true. This will cause a one-shot sync of the
        // Scraper file manifest to the stats sites and then a construction of the contract directly from resultant
        // mScraperStats, if the network policy allows one-off individual node stats downloads (i.e. IsScraperAuthorized()
        // is true). If IsScraperAuthorized() is false, then this will do nothing but return an empty string.
        std::string sNeuralContract = ScraperGetNeuralContract(false, true);
    }

    if (sNeuralContract != "")
        bStatus = true;

    return bStatus;
}


/***********************
*    RPC Functions     *
************************/

UniValue sendscraperfilemanifest(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0 )
        throw std::runtime_error(
                "sendscraperfilemanifest\n"
                "Send a CScraperManifest object with the ScraperFileManifest.\n"
                );

    CBitcoinAddress AddressOut;
    CKey KeyOut;
    bool ret;
    if (IsScraperAuthorizedToBroadcastManifests(AddressOut, KeyOut))
    {
        ret = ScraperSendFileManifestContents(AddressOut, KeyOut);
        uiInterface.NotifyScraperEvent(scrapereventtypes::Manifest, CT_NEW, {});
    }
    else
        ret = false;

    return UniValue(ret);
}



UniValue savescraperfilemanifest(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1 )
        throw std::runtime_error(
                "savescraperfilemanifest <hash>\n"
                "Send a CScraperManifest object with the ScraperFileManifest.\n"
                );

    bool ret = ScraperSaveCScraperManifestToFiles(uint256(params[0].get_str()));

    return UniValue(ret);
}


UniValue deletecscrapermanifest(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1 )
        throw std::runtime_error(
                "deletecscrapermanifest <hash>\n"
                "delete manifest object.\n"
                );

    LOCK(CScraperManifest::cs_mapManifest);
    if (fDebug) _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

    bool ret = ScraperDeleteCScraperManifest(uint256(params[0].get_str()));

    if (fDebug) _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

    return UniValue(ret);
}


