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

enum class statsobjecttype
{
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
    std::string sContractHash;
    std::string sContract;
};

/*********************
* Global Defaults    *
*********************/

// These can get overridden by the GetArgs in init.cpp or ScraperApplyAppCacheEntries.
// The appcache entries will take precedence.

// The amount of time to wait between scraper loop runs.
unsigned int nScraperSleep = 300000;
// The amount of time before SB is due to start scraping.
unsigned int nActiveBeforeSB = 14400;

// These can be overridden by ScraperApplyAppCacheEntries().

// The flag to control whether non-current statistics files are retained.
bool SCRAPER_RETAIN_NONCURRENT_FILES = true;
// Define 48 hour retention time for stats files, current or not.
int64_t SCRAPER_FILE_RETENTION_TIME = 48 * 3600;
// Define whether prior CScraperManifests are kept.
bool SCRAPER_CMANIFEST_RETAIN_NONCURRENT = true;
// Define CManifest scraper object retention time.
int64_t SCRAPER_CMANIFEST_RETENTION_TIME = 48 * 3600;
bool SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES = false;
double MAG_ROUND = 0.01;
double NEURALNETWORKMULTIPLIER = 115000;
double CPID_MAG_LIMIT = 32767;
// This settings below are important. This sets the minimum number of scrapers
// that must be available to form a convergence. Above this minimum, the ratio
// is followed. For example, if there are 4 scrapers, a ratio of 0.6 would require
// CEILING(0.6 * 4) = 3. See NumScrapersForSupermajority below.
// If there is only 1 scraper available, and the mininum is 2, then a convergence
// will not happen. Setting this below 2 will allow convergence to happen without
// cross checking, and is undesirable, because the scrapers are not supposed to be
// trusted entities.
unsigned int SCRAPER_CONVERGENCE_MINIMUM = 2;
// 0.6 seems like a reasonable standard for agreement. It will require...
// 2 out of 3, 3 out of 4, 3 out of 5, 4 out of 6, 5 out of 7, 5 out of 8, etc.
double SCRAPER_CONVERGENCE_RATIO = 0.6;
// By Project Fallback convergence rule as a ratio of projects converged vs whitelist.
// For 20 whitelisted projects this means up to five can be excluded and a contract formed.
double CONVERGENCE_BY_PROJECT_RATIO = 0.75;
// Allow non-scraper nodes to download stats?
bool ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD = false;

/*********************
* Functions          *
*********************/

uint256 GetFileHash(const fs::path& inputfile);
ScraperStats GetScraperStatsByConvergedManifest(ConvergedManifest& StructConvergedManifest);
std::string ExplainMagnitude(std::string sCPID);
bool IsScraperAuthorized();
bool IsScraperAuthorizedToBroadcastManifests();
std::string ScraperGetNeuralContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false);
std::string ScraperGetNeuralHash();
bool ScraperSynchronizeDPOR();

double MagRound(double dMag)
{
    return round(dMag / MAG_ROUND) * MAG_ROUND;
}

unsigned int NumScrapersForSupermajority(unsigned int nScraperCount)
{
    unsigned int nRequired = std::max(SCRAPER_CONVERGENCE_MINIMUM, (unsigned int)std::ceil(SCRAPER_CONVERGENCE_RATIO * nScraperCount));
    
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
