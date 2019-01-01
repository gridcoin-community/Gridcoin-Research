#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include <inttypes.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <map>
#include <unordered_map>
#include <boost/locale.hpp>
#include <codecvt>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <fstream>
#include <sstream>

#include "sync.h"
#include "appcache.h"
#include "beacon.h"
#include "wallet.h"
#include "global_objects_noui.hpp"

#include <memory>
#include "net.h"
// Do not include rpcserver.h yet, as it includes conflicting ExtractXML functions. To do: remove duplicates.
//#include "rpcserver.h"
#include "rpcprotocol.h"
#include "scraper_net.h"
#include "util.h"

/*********************
* Scraper Namepsace  *
*********************/

namespace fs = boost::filesystem;
namespace boostio = boost::iostreams;

/*********************
* Scraper ENUMS      *
*********************/

enum class logattribute {
    // Can't use ERROR here because it is defined already in windows.h.
    ERR,
    INFO,
    WARNING,
    CRITICAL
};

enum class statsobjecttype {
    NetworkWide,
    byCPID,
    byProject,
    byCPIDbyProject
};

static std::vector<std::string> vstatsobjecttypestrings = { "NetWorkWide", "byCPID", "byProject", "byCPIDbyProject" };

const std::string GetTextForstatsobjecttype(statsobjecttype StatsObjType)
{
  return vstatsobjecttypestrings[static_cast<int>(StatsObjType)];
}

/*********************
* Global Vars        *
*********************/

extern bool fDebug;
extern std::string msMasterMessagePrivateKey;
extern CWallet* pwalletMain;
std::vector<std::pair<std::string, std::string>> vwhitelist;
std::vector<std::pair<std::string, std::string>> vuserpass;
std::vector<std::pair<std::string, int64_t>> vprojectteamids;
std::vector<std::string> vauthenicationetags;
int64_t ndownloadsize = 0;
int64_t nuploadsize = 0;
bool fScraperActive = false;

CCriticalSection cs_vwhitelist;

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

typedef std::string ScraperID;
// The inner map is sorted in descending order of time. The pair is manifest hash, content hash.
typedef std::multimap<int64_t, pair<uint256, uint256>, greater <int64_t>> mCSManifest;
// Right now this is using sCManifestName, but needs to be changed to pubkey once the pubkey part is finished.
// See the ScraperID typedef above.
typedef std::map<ScraperID, mCSManifest> mmCSManifestsBinnedByScraper;

// -------------- Project ---- Converged Part
typedef std::map<std::string, CSerializeData> mConvergedManifestParts;
// Note that this IS a copy not a pointer. Since manifests and parts can be deleted because of aging rules,
// it is dangerous to save memory and point to the actual part objects themselves.

struct ConvergedManifest
{
    // IMPORTANT... nContentHash is NOT the hash of part hashes in the order of vParts unlike CScraper::manifest.
    // It is the hash of the data in the ConvergedManifestPartsMap in the order of the key.
    uint256 nContentHash;
    uint256 ConsensusBlock;
    int64_t timestamp;
    bool bByParts;

    mConvergedManifestParts ConvergedManifestPartsMap;
    // ------------------ project ----- reason for exclusion
    std::vector<std::pair<std::string, std::string>> vExcludedProjects;
};


struct ScraperObjectStatsKey
{
    statsobjecttype objecttype;
    std::string objectID;
};

struct ScraperObjectStatsValue
{
    double dTC;
    double dRAT;
    double dRAC;
    double dAvgRAC;
    double dMag;
};

struct ScraperObjectStats
{
    ScraperObjectStatsKey statskey;
    ScraperObjectStatsValue statsvalue;
};

struct ScraperObjectStatsKeyComp
{
    bool operator() ( ScraperObjectStatsKey a, ScraperObjectStatsKey b ) const
    {
        return std::make_pair(a.objecttype, a.objectID) < std::make_pair(b.objecttype, b.objectID);
    }
};

typedef std::map<ScraperObjectStatsKey, ScraperObjectStats, ScraperObjectStatsKeyComp> ScraperStats;

struct ConvergedScraperStats
{
    int64_t nTime;
    ScraperStats mScraperConvergedStats;
    uint256 nContractHash;
    std::string sContract;
};

/*********************
* Global Constants   *
*********************/

// We may want to turn some of these into administrative message appcache entries...

// Define 48 hour retention time for stats files, current or not.
static int64_t SCRAPER_FILE_RETENTION_TIME = 48 * 3600;
// Define whether prior CScraperManifests are kept.
static bool SCRAPER_CMANIFEST_RETAIN_NONCURRENT = true;
// Define CManifest scraper object retention time.
static int64_t SCRAPER_CMANIFEST_RETENTION_TIME = 48 * 3600;
static bool SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES = false;
static const double MAG_ROUND = 0.01;
static const double NEURALNETWORKMULTIPLIER = 115000;
// This settings below are important. This sets the minimum number of scrapers
// that must be available to form a convergence. Above this minimum, the ratio
// is followed. For example, if there are 4 scrapers, a ratio of 0.6 would require
// CEILING(0.6 * 4) = 3. See NumScrapersForSupermajority below.
// If there is only 1 scraper available, and the mininum is 2, then a convergence
// will not happen. Setting this below 2 will allow convergence to happen without
// cross checking, and is undesirable, because the scrapers are not supposed to be
// trusted entities.
static const unsigned int SCRAPER_SUPERMAJORITY_MINIMUM = 2;
// 0.6 seems like a reasonable standard for agreement. It will require...
// 2 out of 3, 3 out of 4, 3 out of 5, 4 out of 6, 5 out of 7, 5 out of 8, etc.
static const double SCRAPER_SUPERMAJORITY_RATIO = 0.6;
// By Project Fallback convergence rule as a ratio of projects converged vs whitelist.
// For 20 whitelisted projects this means up to five can be excluded and a contract formed.
static const double CONVERGENCE_BY_PROJECT_RATIO = 0.75;

/*********************
* Functions          *
*********************/

void _log(logattribute eType, const std::string& sCall, const std::string& sMessage);
void Scraper(bool bSingleShot = false);
void ScraperSingleShot();
bool ScraperDirectorySanity();
bool StoreBeaconList(const fs::path& file);
bool LoadBeaconList(const fs::path& file, BeaconMap& mBeaconMap);
bool LoadBeaconListFromConvergedManifest(ConvergedManifest& StructConvergedManifest, BeaconMap& mBeaconMap);
std::vector<std::string> split(const std::string& s, const std::string& delim);
uint256 GetFileHash(const fs::path& inputfile);
uint256 GetmScraperFileManifestHash();
bool StoreScraperFileManifest(const fs::path& file);
bool LoadScraperFileManifest(const fs::path& file);
bool InsertScraperFileManifestEntry(ScraperFileManifestEntry& entry);
unsigned int DeleteScraperFileManifestEntry(ScraperFileManifestEntry& entry);
bool MarkScraperFileManifestEntryNonCurrent(ScraperFileManifestEntry& entry);
ScraperStats GetScraperStatsByConsensusBeaconList();
ScraperStats GetScraperStatsByConvergedManifest(ConvergedManifest& StructConvergedManifest);
std::string ExplainMagnitude(std::string sCPID);
bool LoadProjectFileToStatsByCPID(const std::string& project, const fs::path& file, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool LoadProjectObjectToStatsByCPID(const std::string& project, const CSerializeData& ProjectData, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool ProcessProjectStatsFromStreamByCPID(const std::string& project, boostio::filtering_istream& sUncompressedIn,
                                         const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool StoreStats(const fs::path& file, const ScraperStats& mScraperStats);
bool ScraperSaveCScraperManifestToFiles(uint256 nManifestHash);
bool IsScraperAuthorized();
bool IsScraperAuthorizedToBroadcastManifests();
bool ScraperSendFileManifestContents(std::string CManifestName);
mmCSManifestsBinnedByScraper BinCScraperManifestsByScraper();
mmCSManifestsBinnedByScraper ScraperDeleteCScraperManifests();
bool ScraperDeleteCScraperManifest(uint256 nManifestHash);
bool ScraperConstructConvergedManifest(ConvergedManifest& StructConvergedManifest);
bool ScraperConstructConvergedManifestByProject(std::vector<std::pair<std::string, std::string>> &vwhitelist_local,
                                                mmCSManifestsBinnedByScraper& mMapCSManifestsBinnedByScraper, ConvergedManifest& StructConvergedManifest);
std::string GenerateSBCoreDataFromScraperStats(ScraperStats& mScraperStats);
std::string ScraperGetNeuralContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false);
std::string ScraperGetNeuralHash();
bool ScraperSynchronizeDPOR();

double MagRound(double dMag)
{
    return round(dMag / MAG_ROUND) * MAG_ROUND;
}

unsigned int NumScrapersForSupermajority(unsigned int nScraperCount)
{
    unsigned int nRequired = std::max(SCRAPER_SUPERMAJORITY_MINIMUM, (unsigned int)std::ceil(SCRAPER_SUPERMAJORITY_RATIO * nScraperCount));
    
    return nRequired;
}

/*********************
* Scraper            *
*********************/

class scraper
{
public:
    scraper();
};


/**********************
* Network Node Logger *
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

    void cleanappend(const std::string& value)
    {
        builtstring << value.substr(1, value.size());
    }

    void nlcleanappend(const std::string& value)
    {
        builtstring << value.substr(1, value.size()) << "\n";
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

        vwhitelist.clear();

        for(const auto& item : ReadCacheSection(Section::PROJECT))
            vwhitelist.push_back(std::make_pair(item.first, item.second.value));

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
