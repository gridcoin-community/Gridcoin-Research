// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "gridcoin/scraper/scraper_net.h"
#include "util.h"
#include "streams.h"


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

typedef std::shared_ptr<CScraperManifest> CScraperManifest_shared_ptr;

// Note the CParts pointed to by this map are safe to access, because the pointers are guaranteed valid
// as long as the holding CScraperManifests (both in the CScaperManifest global map, and this cache)
// still exist. So the safety of these pointers is coincident with the lifespan of CScraperManifests
// that have reference to them. If you have questions about this, you should review the CSplitBlob abstract
// class, which is the base class of the CScraperManifest class, and provides the mechanisms for part
// control. Note that two LOCKS are used to protect the integrity of the underlying global maps,
// CScraperManifest::cs_mapManifest and CSplitBlob::cs_mapParts.
// -------------- Project -- Converged Part Pointer
typedef std::map<std::string, CSplitBlob::CPart*> mConvergedManifestPart_ptrs;

struct ConvergedManifest
{
    // Empty converged manifest constructor
    ConvergedManifest()
    {
        nContentHash = {};
        ConsensusBlock = {};
        timestamp = 0;
        bByParts = false;

        CScraperConvergedManifest_ptr = nullptr;

        ConvergedManifestPartPtrsMap = {};

        mIncludedScraperManifests = {};

        nUnderlyingManifestContentHash = {};

        vIncludedScrapers = {};
        vExcludedScrapers = {};
        vScrapersNotPublishing = {};

        mIncludedScrapersbyProject = {};
        mIncludedProjectsbyScraper = {};

        mScraperConvergenceCountbyProject = {};

        vExcludedProjects = {};
    }

    // For constructing a dummy converged manifest from a single manifest
    ConvergedManifest(CScraperManifest_shared_ptr& in)
    {
        ConsensusBlock = in->ConsensusBlock;
        timestamp = GetAdjustedTime();
        bByParts = false;

        CScraperConvergedManifest_ptr = in;

        PopulateConvergedManifestPartPtrsMap();

        ComputeConvergedContentHash();

        nUnderlyingManifestContentHash = in->nContentHash;
    }

    // Call operator to update an already initialized ConvergedManifest with a passed in CScraperManifest
    bool operator()(const CScraperManifest_shared_ptr& in)
    {
        ConsensusBlock = in->ConsensusBlock;
        timestamp = GetAdjustedTime();
        bByParts = false;

        CScraperConvergedManifest_ptr = in;

        bool bConvergedContentHashMatches = PopulateConvergedManifestPartPtrsMap();

        ComputeConvergedContentHash();

        nUnderlyingManifestContentHash = in->nContentHash;

        return bConvergedContentHashMatches;
    }

    // IMPORTANT... nContentHash is NOT the hash of part hashes in the order of vParts unlike CScraper::manifest.
    // It is the hash of the data in the ConvergedManifestPartsMap in the order of the key. It represents
    // the composite convergence by taking parts piecewise in the case of the fallback to bByParts (project) level.
    uint256 nContentHash;
    uint256 ConsensusBlock;
    int64_t timestamp;
    bool bByParts;

    CScraperManifest_shared_ptr CScraperConvergedManifest_ptr;

    mConvergedManifestPart_ptrs ConvergedManifestPartPtrsMap;

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

    bool PopulateConvergedManifestPartPtrsMap()
    {
        if (CScraperConvergedManifest_ptr == nullptr) return false;

        int iPartNum = 0;
        CDataStream ss(SER_NETWORK,1);
        WriteCompactSize(ss, CScraperConvergedManifest_ptr->vParts.size());
        uint256 nContentHashCheck;

        for (const auto& iter : CScraperConvergedManifest_ptr->vParts)
        {
            std::string sProject;

            if (iPartNum == 0)
                sProject = "BeaconList";
            else
                sProject = CScraperConvergedManifest_ptr->projects[iPartNum-1].project;

            // Copy the pointer to the CPart into the map. This is ok, because the parts will be held
            // until the CScraperManifest in this object is destroyed and all of the manifest refs to the part
            // are gone.
            ConvergedManifestPartPtrsMap.insert(std::make_pair(sProject, iter));

            // Serialize the hash to doublecheck the content hash.
            ss << iter->hash;

            iPartNum++;
        }

        ss << CScraperConvergedManifest_ptr->ConsensusBlock;

        nContentHashCheck = Hash(ss.begin(), ss.end());

        if (nContentHashCheck != CScraperConvergedManifest_ptr->nContentHash)
        {
            LogPrintf("ERROR: PopulateConvergedManifestPartPtrsMap(): Selected Manifest content hash check failed! "
                      "nContentHashCheck = %s and nContentHash = %s.",
                      nContentHashCheck.GetHex(), CScraperConvergedManifest_ptr->nContentHash.GetHex());
            return false;
        }

        return true;
    }

    void ComputeConvergedContentHash()
    {
        CDataStream ss(SER_NETWORK,1);

        for (const auto& iter : ConvergedManifestPartPtrsMap)
        {
            ss << iter.second->data;
        }

        nContentHash = Hash(ss.begin(), ss.end());
    }
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

// This is modeled after AppCacheEntry/Section but named separately.
struct ScraperBeaconEntry
{
    std::string value; //!< Value of entry.
    int64_t timestamp; //!< Timestamp of entry.
};

typedef std::map<std::string, ScraperBeaconEntry> ScraperBeaconMap;

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

// --- Base58 encoded public key ---- cpid, timestamp
typedef std::map<std::string, ScraperPendingBeaconEntry> ScraperPendingBeaconMap;

struct BeaconConsensus
{
    uint256 nBlockHash;
    ScraperBeaconMap mBeaconMap;
    ScraperPendingBeaconMap mPendingMap;
};

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

struct ScraperStatsAndVerifiedBeacons
{
    ScraperStats mScraperStats;
    ScraperPendingBeaconMap mVerifiedMap;
};

// Extended AppCache structures similar to those in AppCache.h, except a deleted flag is provided
struct AppCacheEntryExt
{
    std::string value; // Value of entry.
    int64_t timestamp; // Timestamp of entry/deletion
    bool deleted; // Deleted flag.
};

typedef std::unordered_map<std::string, AppCacheEntryExt> AppCacheSectionExt;
