// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SCRAPER_FWD_H
#define GRIDCOIN_SCRAPER_FWD_H

#include <string>
#include <vector>
#include <unordered_map>

#include "gridcoin/scraper/scraper_net.h"
#include "util.h"
#include "streams.h"


/*********************
* Scraper ENUMS      *
*********************/

/** Defines the object type of the stats entry */
enum class statsobjecttype
{
    NetworkWide,
    byCPID,
    byProject,
    byCPIDbyProject
};

/** Defines the event type in the scraper system */
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

/** Defines the validation type of the convergence achieved by the subscriber */
enum class scraperSBvalidationtype
{
    Invalid,
    Unknown,
    CurrentCachedConvergence,
    CachedPastConvergence,
    ManifestLevelConvergence,
    ProjectLevelConvergence
};

/** Currently the scraperID is a string. */
typedef std::string ScraperID;
/** The inner map is sorted in descending order of time. The pair is manifest hash, content hash. */
typedef std::multimap<int64_t, std::pair<uint256, uint256>, std::greater <int64_t>> mCSManifest;
/** This is sCManifestName, which is the string version of the originating scraper pubkey. */
typedef std::map<ScraperID, mCSManifest> mmCSManifestsBinnedByScraper;

/** Make the smart shared pointer a little less awkward for CScraperManifest */
typedef std::shared_ptr<CScraperManifest> CScraperManifest_shared_ptr;

/** Note the CParts pointed to by this map are safe to access, because the pointers are guaranteed valid
 * as long as the holding CScraperManifests (both in the CScaperManifest global map, and this cache)
 * still exist. So the safety of these pointers is coincident with the lifespan of CScraperManifests
 * that have reference to them. If you have questions about this, you should review the CSplitBlob abstract
 * class, which is the base class of the CScraperManifest class, and provides the mechanisms for part
 * control. Note that two LOCKS are used to protect the integrity of the underlying global maps,
 * CScraperManifest::cs_mapManifest and CSplitBlob::cs_mapParts.
 * ---------- Project -- Converged Part Pointer
 * std::map<std::string, CSplitBlob::CPart*> mConvergedManifestPart_ptrs
 */
typedef std::map<std::string, CSplitBlob::CPart*> mConvergedManifestPart_ptrs;

/** Used for a "convergence", which is the result of the subscriber comparing published manifests from the scrapers in
 *  accordance with the rules of convergence, where the rules have been met. The convergence is used as the basis of
 *  constructing a superblock. */
struct ConvergedManifest
{
    /** Empty converged manifest constructor */
    ConvergedManifest();

    /** For constructing a dummy converged manifest from a single manifest */
    ConvergedManifest(CScraperManifest_shared_ptr& in);

    /** Call operator to update an already initialized ConvergedManifest with a passed in CScraperManifest */
    bool operator()(const CScraperManifest_shared_ptr& in);

    /** IMPORTANT... nContentHash is NOT the hash of part hashes in the order of vParts unlike CScraper::manifest.
     * It is the hash of the data in the ConvergedManifestPartsMap in the order of the key. It represents
     * the composite convergence by taking parts piecewise in the case of the fallback to bByParts (project) level.
     */
    uint256 nContentHash;
    /** The block on which the convergence was formed. */
    uint256 ConsensusBlock;
    /** The time of the convergence. */
    int64_t timestamp = 0;
    /** Flag indicating whether the convergence was formed at the project level. If the rules of convergence cannot
     * be met at the whole manifest level (which could happen if a project were not available in some manifests, for
     * example), then in the fallback to put together a convergence from matching at the project (part) level, this flag
     * will be set if a convergence is formed that way.
     */
    bool bByParts = false;

    /** The shared pointer to the CScraperManifest that underlies the convergence. If the convergence was at the manifest
     * level, this will be the manifest selected as the representation of the equivalent manifests used to determine the
     * convergence. If the convergence is at the parts (project) level, then this will point to a synthesized (non-published)
     * manifest which only has the vParts vector filled out, and which content is the parts selected to form the byParts
     * (project) level convergence. Note that this synthesized manifest is LOCAL ONLY and will not be added to mapManifests
     * or published to other nodes. Convergences in general are the responsibility of the subscriber, not the publisher.
     */
    CScraperManifest_shared_ptr CScraperConvergedManifest_ptr = nullptr;

    /** A map to the pointers of the parts used to form the convergence. This will be essentially the same as the vParts
     * vector in the CScraperManifest pointed to by the CScraperConvergedManifest_ptr.
     */
    mConvergedManifestPart_ptrs ConvergedManifestPartPtrsMap;

    /** A map of the manifests by hash (key) that formed this convergence. This is used when convergence is at the manifest
     * level (normal).
     */
    std::map<ScraperID, uint256> mIncludedScraperManifests;
    /** The below is the manifest content hash for the underlying manifests that comprise the convergence. This
     * will only be populated if the convergence is at the manifest level (bByParts == false). In that case, each
     * manifest's content in the convergence must be the same. If the convergence is by project, this does not
     * make sense to populate.
     */
    uint256 nUnderlyingManifestContentHash;

    /** The publishing scrapers included in the convergence. If the convergence is at the project level, a scraper in this
     * vector would have to be included in at least one project level match for the synthesized convergence.
     */
    std::vector<ScraperID> vIncludedScrapers;
    /** The publishing scrapers excluded from the convergence. If the convergence is at the project level, a scraper in this
     *   vector would not have been included in ANY project level match for the synthesized convergence.
     */
    std::vector<ScraperID> vExcludedScrapers;
    /** The scrapers not publishing (i.e. no manifests present with the retention period) when the convergence was formed. */
    std::vector<ScraperID> vScrapersNotPublishing;

    /** Used when convergence is at the project (bByParts) level (fallback)
     * -------------- Project --- ScraperID
     * std::multimap<std::string, ScraperID> mIncludedScrapersbyProject
     */
    std::multimap<std::string, ScraperID> mIncludedScrapersbyProject;
    /** Used when convergence is at the project (bByParts) level (fallback)
     * ------------- ScraperID --- Project
     * std::multimap<ScraperID, std::string> mIncludedProjectsbyScraper
     */
    std::multimap<ScraperID, std::string> mIncludedProjectsbyScraper;
    /** When bByParts (project) level convergence occurs, this records the count of scrapers in the
     * convergences by project.
     */
    std::map<std::string, unsigned int> mScraperConvergenceCountbyProject;

    /** The projects excluded from the convergence. Since the convergence rules REQUIRE a fallback to project level
     * convergence if the trial convergence formed at the manifest level excludes a project, this vector should only have
     * an entry if bByParts is also true.
     */
    std::vector<std::string> vExcludedProjects;

    //!
    //! \brief The list of projects that have been greylisted.
    //!
    std::vector<std::string> vGreylistedProjects;

    /** Populates the part pointers map in the convergence */
    bool PopulateConvergedManifestPartPtrsMap();

    /** Computes the converged content hash */
    void ComputeConvergedContentHash();
};


/** Used for the key of the statistics map(s) in the scraper */
struct ScraperObjectStatsKey
{
    statsobjecttype objecttype;
    std::string objectID;
};

/** Used for the value of the stats entries in the statistics map(s) in the scraper */
struct ScraperObjectStatsValue
{
    double dTC;
    double dRAT;
    double dRAC;
    double dAvgRAC;
    double dMag;
};

/** Used for the stats entries in the statistics map(s) in the scraper */
struct ScraperObjectStats
{
    ScraperObjectStatsKey statskey;
    ScraperObjectStatsValue statsvalue;
};

/** Comparison for the statistics map entry ordering */
struct ScraperObjectStatsKeyComp
{
    bool operator() ( ScraperObjectStatsKey a, ScraperObjectStatsKey b ) const
    {
        return std::make_pair(a.objecttype, a.objectID) < std::make_pair(b.objecttype, b.objectID);
    }
};

/** Definition of the scraper statistics map(s) */
typedef std::map<ScraperObjectStatsKey, ScraperObjectStats, ScraperObjectStatsKeyComp> ScraperStats;

/** modeled after AppCacheEntry/Section but named separately. */
struct ScraperBeaconEntry
{
    std::string value; //!< Value of entry.
    int64_t timestamp; //!< Timestamp of entry.
};

/** Definition of the scraper beacon map */
typedef std::map<std::string, ScraperBeaconEntry> ScraperBeaconMap;

/** Small structure to define the fields and (un)serialization for pending beacon entries */
struct ScraperPendingBeaconEntry
{
    std::string cpid;
    uint160 key_id;
    int64_t timestamp;

    template<typename Stream>
    void Serialize(Stream& stream) const
    {
        stream << cpid;
        stream << timestamp;
        stream << key_id;
    }

    template<typename Stream>
    void Unserialize(Stream& stream)
    {
        stream >> cpid;
        stream >> timestamp;
        stream >> key_id;
    }
};

/** - Base58 encoded public key - {cpid, timestamp, keyid}
 *  std::map<std::string, ScraperPendingBeaconEntry> ScraperPendingBeaconMap
 */
typedef std::map<std::string, ScraperPendingBeaconEntry> ScraperPendingBeaconMap;

/** Used to hold the block hash, scraper beacon map and pending beacon map at the ladder consensus point. This will be used
 * as appropriate in the convergence formed.
 */
struct BeaconConsensus
{
    uint256 nBlockHash;
    ScraperBeaconMap mBeaconMap;
    ScraperPendingBeaconMap mPendingMap;
};

/** Small structure to define the fields for verified beacons and (un)serialization */
struct ScraperVerifiedBeacons
{
    // Initialize the timestamp to the current adjusted time.
    int64_t timestamp = GetAdjustedTime();
    ScraperPendingBeaconMap mVerifiedMap;

    bool LoadedFromDisk = false;

    template<typename Stream>
    void Serialize(Stream& stream) const
    {
        stream << mVerifiedMap;
        stream << timestamp;
    }

    template<typename Stream>
    void Unserialize(Stream& stream)
    {
        stream >> mVerifiedMap;
        stream >> timestamp;
    }
};

/**
 * @brief Used to hold total credits across ALL cpids for each project. This is to support auto greylisting.
 */
struct ScraperProjectsAllCpidTotalCredit
{
    int64_t m_timestamp = GetAdjustedTime();
    std::map<std::string, double> m_all_cpid_total_credit_map;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_timestamp);
        READWRITE(m_all_cpid_total_credit_map);
    }
};

/** A combination of scraper stats and verified beacons. For convenience in the interface between the scraper and the
 * quorum/superblock code.
 */
struct ScraperStatsVerifiedBeaconsTotalCredits
{
    ScraperStats mScraperStats;
    ScraperPendingBeaconMap mVerifiedMap;
    std::map<std::string, double> m_total_credit_map;
};

#endif // GRIDCOIN_SCRAPER_FWD_H
