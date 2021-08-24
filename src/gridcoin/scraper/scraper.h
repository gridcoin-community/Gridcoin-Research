// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SCRAPER_SCRAPER_H
#define GRIDCOIN_SCRAPER_SCRAPER_H

#include <atomic>
#include <inttypes.h>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <vector>
#include <map>

#include "sync.h"
#include "wallet/wallet.h"

#include <memory>
#include "net.h"
#include "rpc/protocol.h"

// See fwd.h for certain forward declarations that need to be included in other areas.
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/superblock.h"

// Thread safety
extern CCriticalSection cs_Scraper;
extern CCriticalSection cs_ScraperGlobals;
extern CCriticalSection cs_mScrapersExt;
extern CCriticalSection cs_StructScraperFileManifest;
extern CCriticalSection cs_ConvergedScraperStatsCache;
extern CCriticalSection cs_TeamIDMap;
extern CCriticalSection cs_VerifiedBeacons;

/********************************************
* Global Defaults (externs for header file) *
*********************************************/

extern unsigned int nScraperSleep;
extern unsigned int nActiveBeforeSB;

extern bool fExplorer;

extern bool SCRAPER_RETAIN_NONCURRENT_FILES;
extern int64_t SCRAPER_FILE_RETENTION_TIME;
extern int64_t EXPLORER_EXTENDED_FILE_RETENTION_TIME;
extern bool SCRAPER_CMANIFEST_RETAIN_NONCURRENT;
extern int64_t SCRAPER_CMANIFEST_RETENTION_TIME;
extern bool SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES;
extern std::atomic<double> MAG_ROUND;
extern std::atomic<double>  NETWORK_MAGNITUDE;
extern std::atomic<double>  CPID_MAG_LIMIT;
extern unsigned int SCRAPER_CONVERGENCE_MINIMUM;
extern double SCRAPER_CONVERGENCE_RATIO;
extern double CONVERGENCE_BY_PROJECT_RATIO;
extern bool ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD;
extern unsigned int SCRAPER_MISBEHAVING_NODE_BANSCORE;
extern bool REQUIRE_TEAM_WHITELIST_MEMBERSHIP;
extern std::string TEAM_WHITELIST;
extern int64_t SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD;

extern CCriticalSection cs_mScrapersExt;

extern AppCacheSectionExt mScrapersExt;

/*********************
* Functions          *
*********************/

uint256 GetFileHash(const fs::path& inputfile);
ScraperStatsAndVerifiedBeacons GetScraperStatsByConvergedManifest(const ConvergedManifest& StructConvergedManifest);
AppCacheSectionExt GetExtendedScrapersCache();
bool IsScraperAuthorized();
bool IsScraperAuthorizedToBroadcastManifests(CBitcoinAddress& AddressOut, CKey& KeyOut);
bool IsScraperMaximumManifestPublishingRateExceeded(int64_t& nTime, CPubKey& PubKey);
GRC::Superblock ScraperGetSuperblockContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false, bool bFromHousekeeping = false);
scraperSBvalidationtype ValidateSuperblock(const GRC::Superblock& NewFormatSuperblock, bool bUseCache = true, unsigned int nReducedCacheBits = 32);
std::vector<uint160> GetVerifiedBeaconIDs(const ConvergedManifest& StructConvergedManifest);
std::vector<uint160> GetVerifiedBeaconIDs(const ScraperPendingBeaconMap& VerifiedBeaconMap);
ScraperStatsAndVerifiedBeacons GetScraperStatsAndVerifiedBeacons(const ConvergedScraperStats &stats);
ScraperPendingBeaconMap GetPendingBeaconsForReport();
ScraperPendingBeaconMap GetVerifiedBeaconsForReport(bool from_global = false);

static std::vector<std::string> vstatsobjecttypestrings = { "NetWorkWide", "byCPID", "byProject", "byCPIDbyProject" };

static std::vector<std::string> scraperSBvalidationtypestrings = {
    "Invalid",
    "Unknown",
    "CurrentCachedConvergence",
    "CachedPastConvergence",
    "ManifestLevelConvergence",
    "ProjectLevelConvergence"
};


const std::string GetTextForstatsobjecttype(statsobjecttype StatsObjType)
{
    return vstatsobjecttypestrings[static_cast<int>(StatsObjType)];
}

const std::string GetTextForscraperSBvalidationtype(scraperSBvalidationtype ScraperSBValidationType)
{
    return scraperSBvalidationtypestrings[static_cast<int>(ScraperSBValidationType)];
}


double MagRound(double dMag)
{
    return round(dMag / MAG_ROUND) * MAG_ROUND;
}

unsigned int NumScrapersForSupermajority(unsigned int nScraperCount)
{
    LOCK(cs_ScraperGlobals);

    unsigned int nRequired = std::max(SCRAPER_CONVERGENCE_MINIMUM, (unsigned int)std::ceil(SCRAPER_CONVERGENCE_RATIO * nScraperCount));

    return nRequired;
}

/*********************
* Scraper            *
*********************/

// For version 2 of the scraper we will restructure into a class. For now this is a placeholder.
/*
 * class scraper
{
public:
    scraper();
};
*/

#endif // GRIDCOIN_SCRAPER_SCRAPER_H
