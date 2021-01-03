// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

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

/*********************
* Global Defaults    *
*********************/

// These can get overridden by the GetArgs in init.cpp or ScraperApplyAppCacheEntries.
// The appcache entries will take precedence.

// The amount of time to wait between scraper loop runs. This is in
// milliseconds.
unsigned int nScraperSleep = 300000;
// The amount of time before SB is due to start scraping. This is in
// seconds.
unsigned int nActiveBeforeSB = 14400;

// Explorer mode flag. Only effective if scraper is active.
bool fExplorer = false;

// These can be overridden by ScraperApplyAppCacheEntries().

// The flag to control whether non-current statistics files are retained.
bool SCRAPER_RETAIN_NONCURRENT_FILES = true;
// Define 48 hour retention time for stats files, current or not.
int64_t SCRAPER_FILE_RETENTION_TIME = 48 * 3600;
// Define extended file retention time for explorer mode.
int64_t EXPLORER_EXTENDED_FILE_RETENTION_TIME = 168 * 3600;
// Define whether prior CScraperManifests are kept.
bool SCRAPER_CMANIFEST_RETAIN_NONCURRENT = true;
// Define CManifest scraper object retention time.
int64_t SCRAPER_CMANIFEST_RETENTION_TIME = 48 * 3600;
bool SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES = false;
double MAG_ROUND = 0.01;
double NETWORK_MAGNITUDE = 115000;
double CPID_MAG_LIMIT = GRC::Magnitude::MAX;
// This settings below are important. This sets the minimum number of scrapers
// that must be available to form a convergence. Above this minimum, the ratio
// is followed. For example, if there are 4 scrapers, a ratio of 0.6 would require
// CEILING(0.6 * 4) = 3. See NumScrapersForSupermajority below.
// If there is only 1 scraper available, and the minimum is 2, then a convergence
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
// Misbehaving scraper node banscore
unsigned int SCRAPER_MISBEHAVING_NODE_BANSCORE = 0;
// Require team membership in team whitelist.
bool REQUIRE_TEAM_WHITELIST_MEMBERSHIP = false;
// Default team whitelist
std::string TEAM_WHITELIST = "Gridcoin";
// This is the period after the deauthorizing of a scraper before the nodes will start
// to assign banscore to nodes sending unauthorized manifests.
int64_t SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD = 300;


AppCacheSectionExt mScrapersExt = {};

CCriticalSection cs_mScrapersExt;


/*********************
* Functions          *
*********************/

uint256 GetFileHash(const fs::path& inputfile);
ScraperStatsAndVerifiedBeacons GetScraperStatsByConvergedManifest(const ConvergedManifest& StructConvergedManifest);
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
