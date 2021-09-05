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

// Thread safety. See scraper.cpp for documentation.
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

/**
 * @brief Returns the hash of the provided input file path. If the file path cannot be resolved or an exception occurs
 * in processing the file, a null hash is returned.
 * @param inputfile
 * @return uint256 hash
 */
uint256 GetFileHash(const fs::path& inputfile);
/**
 * @brief Provides the computed scraper stats and verified beacons from the input converged manifest
 * @param StructConvergedManifest
 * @return ScraperStatsAndVerifiedBeacons
 */
ScraperStatsAndVerifiedBeacons GetScraperStatsByConvergedManifest(const ConvergedManifest& StructConvergedManifest);
/**
 * @brief Gets a copy of the extended scrapers cache global. This global is an extension of the appcache in that it
 * retains deleted entries with a deleted flag.
 * @return AppCacheSectionExt
 */
AppCacheSectionExt GetExtendedScrapersCache();
/**
 * @brief Returns whether this node is authorized to download statistics.
 * @return bool
 */
bool IsScraperAuthorized();
/**
 * @brief Returns whether this node is authorized to broadcast statistics manifests to the network as a scraper.
 * @param AddressOut
 * @param KeyOut
 * @return bool
 *
 * The idea here is that there are two levels of authorization. The first level is whether any
 * node can operate as a "scraper", in other words, download the stats files themselves.
 * The second level, which is the IsScraperAuthorizedToBroadcastManifests() function,
 * is to authorize a particular node to actually be able to publish manifests.
 * The second function is intended to override the first, with the first being a network wide
 * policy. So to be clear, if the network wide policy has IsScraperAuthorized() set to false
 * then ONLY nodes that have IsScraperAuthorizedToBroadcastManifests() can download stats at all.
 * If IsScraperAuthorized() is set to true, then you have two levels of operation allowed.
 * Nodes can run -scraper and download stats for themselves. They will only be able to publish
 * manifests if for that node IsScraperAuthorizedToBroadcastManifests() evaluates to true.
 * This allows flexibility in network policy, and will allow us to convert from a scraper based
 * approach to convergence back to individual node stats download and convergence without a lot of
 * headaches.
 *
 * This function checks to see if the local node is authorized to publish manifests. Note that this code could be
 * modified to bypass this check, so messages sent will also be validated on receipt by the complement
 * to this function, IsManifestAuthorized(CKey& Key) in the CScraperManifest class.
 */
bool IsScraperAuthorizedToBroadcastManifests(CBitcoinAddress& AddressOut, CKey& KeyOut);
/**
 * @brief Returns whether the scraper with the input public key at the input time has exceeded the maximum allowable
 * manifest publishing rate. This is a DoS function and is used to issue misbehavior points, which could result in banning
 * the node that has exceeded the max publishing rate.
 * @param nTime
 * @param PubKey
 * @return bool
 *
 * This function computes the average time between manifests as a function of the last 10 received manifests
 * plus the nTime provided as the argument. This gives ten intervals for sampling between manifests. If the
 * average time between manifests is less than 50% of the nScraperSleep interval, or the most recent manifest
 * for a scraper is more than five minutes in the future (accounts for clock skew) then the publishing rate
 * of the scraper is deemed too high. This is actually used in CScraperManifest::IsManifestAuthorized to ban
 * a scraper that is abusing the network by sending too many manifests over a very short period of time.
 */
bool IsScraperMaximumManifestPublishingRateExceeded(int64_t& nTime, CPubKey& PubKey);
/**
 * @brief Generates a superblock (contract) from the current convergence. It will construct/update the convergence if needed.
 * @param bStoreConvergedStats
 * @param bContractDirectFromStatsUpdate
 * @param bFromHousekeeping
 * @return GRC::Superblock
 */
GRC::Superblock ScraperGetSuperblockContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false,
                                             bool bFromHousekeeping = false);
/**
 * @brief Gets the verified beacon ID's from the input converged manifest (overloaded)
 * @param StructConvergedManifest
 * @return std::vector<uint160> of beacon ID's
 */
std::vector<uint160> GetVerifiedBeaconIDs(const ConvergedManifest& StructConvergedManifest);
/**
 * @brief Gets the verified beacon ID's from the input VerifiedBeaconMap (overloaded)
 * @param VerifiedBeaconMap
 * @return std::vector<uint160> of beacon ID's
 */
std::vector<uint160> GetVerifiedBeaconIDs(const ScraperPendingBeaconMap& VerifiedBeaconMap);
/**
 * @brief Returns the scraper stats and verified beacons in one structure from the input ConvergedScraperStats
 * @param stats
 * @return ScraperStatsAndVerifiedBeacons
 */
ScraperStatsAndVerifiedBeacons GetScraperStatsAndVerifiedBeacons(const ConvergedScraperStats &stats);
/**
 * @brief Returns a map of pending beacons
 * @return ScraperPendingBeaconMap of pending beacons
 */
ScraperPendingBeaconMap GetPendingBeaconsForReport();
/**
 * @brief Returns a map of verified beacons
 * @param from_global
 * @return ScraperPendingBeaconMap of verified beacons
 */
ScraperPendingBeaconMap GetVerifiedBeaconsForReport(bool from_global = false);

/** Vector of strings that correspond with the statsobjecttype ENUM class */
static std::vector<std::string> vstatsobjecttypestrings = { "NetWorkWide", "byCPID", "byProject", "byCPIDbyProject" };

/** Vector of strings that correspond with the scraperSBvalidationtype ENUM class */
static std::vector<std::string> scraperSBvalidationtypestrings = {
    "Invalid",
    "Unknown",
    "CurrentCachedConvergence",
    "CachedPastConvergence",
    "ManifestLevelConvergence",
    "ProjectLevelConvergence"
};

/**
 * @brief Returns text that corresponds to the input statsobjecttype
 * @param StatsObjType
 * @return std::string
 */
const std::string GetTextForstatsobjecttype(statsobjecttype StatsObjType)
{
    return vstatsobjecttypestrings[static_cast<int>(StatsObjType)];
}

/**
 * @brief Returns text that corresponds to the input scraperSBvalidationtype
 * @param ScraperSBValidationType
 * @return std::string
 */
const std::string GetTextForscraperSBvalidationtype(scraperSBvalidationtype ScraperSBValidationType)
{
    return scraperSBvalidationtypestrings[static_cast<int>(ScraperSBValidationType)];
}

/**
 * @brief Rounds the double floating point magnitude according to the global parameter MAG_ROUND.
 * @param dMag
 * @return double
 */
double MagRound(double dMag)
{
    return round(dMag / MAG_ROUND) * MAG_ROUND;
}

/**
 * @brief Returns the number of scrapers required for a supermajority when determining a convergence. This is a CONSENSUS
 * critical function
 * @param nScraperCount
 * @return unsigned int
 */
unsigned int NumScrapersForSupermajority(unsigned int nScraperCount)
{
    LOCK(cs_ScraperGlobals);

    unsigned int nRequired = std::max(SCRAPER_CONVERGENCE_MINIMUM,
                                      (unsigned int)std::ceil(SCRAPER_CONVERGENCE_RATIO * nScraperCount));

    return nRequired;
}

#endif // GRIDCOIN_SCRAPER_SCRAPER_H
