#ifndef FWD_H
#define FWD_H

#include <string>
#include <vector>
#include <unordered_map>

#include "support/allocators/zeroafterfree.h"
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
    OutOfSync,
    Log,
    Stats,
    Manifest,
    Convergence,
    SBContract,
    Sleep
};

enum class scraperSBvalidationtype
{
    Invalid,
    Unknown,
    CurrentCachedConvergence,
    CachedPastConvergence,
    ManifestLevelConvergence,
    ProjectLevelConvergence
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
    // It is the hash of the data in the ConvergedManifestPartsMap in the order of the key. It represents
    // the composite convergence by taking parts piecewise in the case of the fallback to bByParts (project) level.
    uint256 nContentHash;
    uint256 ConsensusBlock;
    int64_t timestamp;
    bool bByParts;

    mConvergedManifestParts ConvergedManifestPartsMap;

    // Used when convergence is at the manifest level (normal)
    std::map<ScraperID, uint256> mIncludedScraperManifests;
    // The below is the manifest content hash for the underlying manifests that comprise the convergence. This
    // will only be populated if the convergence is at the manifest level (bByParts == false). In that case, each
    // manifest's content in the convergence must be the same. If the convergence is by project, this does not
    // make sense to populate. See the above comment.
    uint256 nUnderlyingManifestContentHash;

    // Used when convergence is at the manifest level (normal) and also at the part (project) level for
    // scrapers that are not part of any part (project) level convergence.
    std::vector<ScraperID> vIncludedScrapers;
    std::vector<ScraperID> vExcludedScrapers;
    std::vector<ScraperID> vScrapersNotPublishing;

    // Used when convergence is at the project (bByParts) level (fallback)
    // ----- Project --------- ScraperID
    std::multimap<std::string, ScraperID> mIncludedScrapersbyProject;
    // ----- ScraperID ------- Project
    std::multimap<ScraperID, std::string> mIncludedProjectsbyScraper;
    // When bByParts (project) level convergence occurs, this records the the count of scrapers in the
    // convergences by project.
    std::map<std::string, unsigned int> mScraperConvergenceCountbyProject;

    // --------- project
    std::vector<std::string> vExcludedProjects;
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

// Extended AppCache structures similar to those in AppCache.h, except a deleted flag is provided
struct AppCacheEntryExt
{
    std::string value; // Value of entry.
    int64_t timestamp; // Timestamp of entry/deletion
    bool deleted; // Deleted flag.
};

typedef std::unordered_map<std::string, AppCacheEntryExt> AppCacheSectionExt;

#endif // FWD_H
