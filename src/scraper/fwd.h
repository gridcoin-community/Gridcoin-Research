#ifndef FWD_H
#define FWD_H

#include <string>
#include <vector>

#include "util.h"


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

enum class scrapereventtypes
{
    Stats,
    Manifest,
    Convergence,
    SBContract,
};



/*********************
* Global Vars        *
*********************/


typedef std::string ScraperID;
// The inner map is sorted in descending order of time. The pair is manifest hash, content hash.
typedef std::multimap<int64_t, std::pair<uint256, uint256>, std::greater <int64_t>> mCSManifest;
// This is sCManifestName, which is the string version of the originating scraper pubkey.
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
    std::vector<std::pair<std::string, std::string>> vExcludedProjects;
};

#endif // FWD_H
