// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "main.h"
#include "gridcoin/claim.h"
#include "gridcoin/magnitude.h"
#include "gridcoin/quorum.h"
#include "gridcoin/scraper/scraper_net.h"
#include "gridcoin/superblock.h"
#include "util/reverse_iterator.h"

#include <openssl/md5.h>
#include <unordered_map>

using namespace GRC;

// TODO: use a header
ScraperStatsAndVerifiedBeacons  GetScraperStatsByConvergedManifest(const ConvergedManifest& StructConvergedManifest);
ScraperStatsAndVerifiedBeacons  GetScraperStatsFromSingleManifest(CScraperManifest_shared_ptr& manifest);
unsigned int NumScrapersForSupermajority(unsigned int nScraperCount);
mmCSManifestsBinnedByScraper ScraperCullAndBinCScraperManifests();
Superblock ScraperGetSuperblockContract(
    bool bStoreConvergedStats = false,
    bool bContractDirectFromStatsUpdate = false,
    bool bFromHousekeeping = false);

extern CCriticalSection cs_ConvergedScraperStatsCache;
extern ConvergedScraperStats ConvergedScraperStatsCache;

namespace {
//!
//! \brief Organizes superblocks for lookup to calculate research rewards.
//!
class SuperblockIndex
{
    //!
    //! \brief Number of superblocks to store in the active cache.
    //!
    static constexpr size_t CACHE_SIZE = 3;

public:
    //!
    //! \brief Get a reference to the current active superblock.
    //!
    //! \return The most recent superblock applied by the tally.
    //!
    SuperblockPtr Current() const
    {
        if (m_cache.empty()) {
            return SuperblockPtr::Empty();
        }

        return m_cache.front();
    }

    //!
    //! \brief Get a reference to the upcoming superblock.
    //!
    //! After a node receives a new superblock, the tally must commit it before
    //! it becomes active.
    //!
    //! \return A superblock pending activation if one exists. Returns an empty
    //! superblock when the tally already activated the latest superblock.
    //!
    SuperblockPtr Pending() const
    {
        if (m_pending.empty()) {
            return SuperblockPtr::Empty();
        }

        return m_pending.rbegin()->second;
    }

    //!
    //! \brief Determine whether any superblocks are pending activation.
    //!
    //! \return \c true if the index contains a superblock loaded at a height
    //! above the last tally window.
    //!
    bool HasPending() const
    {
        return !m_pending.empty();
    }

    //!
    //! \brief Activate the superblock received at the specified height.
    //!
    //! \param height The height of the block that contains the superblock.
    //!
    //! \return \c true if a superblock at the specified height was activated.
    //!
    bool Commit(const uint32_t height)
    {
        static const auto log = [](const uint32_t height, const char* msg) {
            LogPrintf("SuperblockIndex::Commit(%" PRId64 "): %s", height, msg);
        };

        if (m_pending.empty()) {
            if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE)) {
                log(height, "none pending.");
            }

            return false;
        }

        auto pending_iter = m_pending.upper_bound(height);

        if (pending_iter == m_pending.begin()) {
            if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE)) {
                log(height, "not pending.");
            }

            return false;
        }

        for (auto iter = m_pending.begin() ; iter != pending_iter; ++iter) {
            if (m_cache.size() > CACHE_SIZE + 1) {
                m_cache.pop_back();
            }

            m_cache.emplace_front(std::move(iter->second));
        }

        m_pending.erase(m_pending.begin(), pending_iter);

        log(Current().m_height, "commit.");

        return true;
    }

    //!
    //! \brief Add a new superblock to the index in a pending state.
    //!
    //! \param superblock Contains the superblock data to add.
    //!
    void PushSuperblock(SuperblockPtr superblock)
    {
        // Version 11+ blocks no longer rely on the tally trigger heights. We
        // commit version 2+ superblocks immediately instead of holding these
        // in a pending state:
        //
        if (superblock->m_version >= 2) {
            if (m_cache.size() > CACHE_SIZE + 1) {
                m_cache.pop_back();
            }

            m_cache.emplace_front(std::move(superblock));
        } else {
            m_pending.emplace(superblock.m_height, std::move(superblock));
        }
    }

    //!
    //! \brief Remove the most recently added superblock from the index.
    //!
    void PopSuperblock()
    {
        if (!m_pending.empty()) {
            auto iter = m_pending.end();
            m_pending.erase(--iter);
        } else if (!m_cache.empty()) {
            m_cache.pop_front();
        }
    }

    //!
    //! \brief Refill the superblock index cache.
    //!
    //! \param pindexLast The block to begin loading superblocks backward from.
    //!
    void Reload(const CBlockIndex* pindexLast)
    {
        // Version 11+ blocks no longer rely on the tally trigger heights. We
        // just find and load the most recent superblock:
        //
        if (pindexLast->nVersion >= 11) {
            m_pending.clear();
            m_cache.clear();

            const CBlockIndex* pindex = pindexLast;
            for (; pindex && !pindex->IsSuperblock(); pindex = pindex->pprev);

            m_cache.emplace_front(SuperblockPtr::ReadFromDisk(pindex));

            return;
        }

        // Move committed superblocks back to pending. The next tally recount
        // will commit the superblocks at the appropriate height.
        //
        if (!m_cache.empty()) {
            for (auto&& superblock : m_cache) {
                m_pending.emplace(superblock.m_height, std::move(superblock));
            }

            m_cache.clear();
        }

        if (!m_pending.empty() && m_pending.size() < CACHE_SIZE) {
            const int64_t lowest_height = m_pending.begin()->first;

            while (pindexLast->nHeight >= lowest_height) {
                if (!pindexLast->pprev) {
                    return;
                }

                pindexLast = pindexLast->pprev;
            }
        }

        // TODO: for now, just load the last three superblocks. We'll build a
        // better index when we implement superblock windows:
        //
        while (m_pending.size() < CACHE_SIZE) {
            while (!pindexLast->IsSuperblock()) {
                if (!pindexLast->pprev) {
                    return;
                }

                pindexLast = pindexLast->pprev;
            }

            PushSuperblock(SuperblockPtr::ReadFromDisk(pindexLast));

            pindexLast = pindexLast->pprev;
        }
    }
private:
    //!
    //! \brief A set of recently-added superblocks not yet activated by the
    //! tally recount.
    //!
    std::map<uint32_t, SuperblockPtr> m_pending;

    //!
    //! \brief Contains a cache of recent activated superblocks.
    //!
    //! TODO: refactor this for superblock windows.
    //!
    std::deque<SuperblockPtr> m_cache;
}; // SuperblockIndex

//!
//! \brief A vote for a superblock for legacy quorum consensus.
//!
class QuorumVote
{
public:
    const QuorumHash m_hash;           //!< Hash of the superblock to vote for.
    const CBitcoinAddress m_address;   //!< Address of the voting wallet.
    const CBlockIndex* const m_pindex; //!< Block that contains the vote.

    //!
    //! \brief Initialize a legacy quorum vote.
    //!
    //! \param hash    Hash of the superblock to vote for.
    //! \param address Default address of the voting wallet.
    //! \param pindex  Block that contains the vote.
    //!
    QuorumVote(QuorumHash hash, CBitcoinAddress address, const CBlockIndex* pindex)
        : m_hash(hash)
        , m_address(std::move(address))
        , m_pindex(pindex)
    {
    }
};

//!
//! \brief Orchestrates superblock voting for legacy quorum consensus.
//!
//! For the legacy network-wide superblock selection mechanism before block
//! version 11, a node will vote for a superblock that it considers correct
//! by embedding the hash of that superblock in the claim section of blocks
//! that it generates. Another node stakes the final superblock if the hash
//! of the local pending superblock matches the most popular hash by weight
//! of the preceding votes.
//!
//! For better security, superblock staking and validation using data from
//! scraper convergence replaces the problematic quorum mechanism in block
//! version 11 and above.
//!
class LegacyConsensus
{
    //!
    //! \brief Number of blocks to search backward for pending superblock votes.
    //!
    static constexpr int STANDARD_LOOKBACK = 100;

public:
    //!
    //! \brief Determine whether the provided address participates in the
    //! quorum consensus at the specified time.
    //!
    //! \param address Default wallet address of a node in the network.
    //! \param time    Timestamp to check participation at.
    //!
    //! \return \c true if the address matches the subset of addrsses that
    //! particpate in the quorum on the day of the year of \p time.
    //!
    static bool Participating(const std::string& grc_address, const int64_t time)
    {
        // This hash is approximately 25% of the MD5 range (90% for testnet):
        static const arith_uint256 reference_hash = fTestNet
            ? arith_uint256("0x00000000000000000000000000000000ed182f81388f317df738fd9994e7020b")
            : arith_uint256("0x000000000000000000000000000000004d182f81388f317df738fd9994e7020b");

        std::string input = grc_address + "_" + std::to_string(GetDayOfYear(time));
        std::vector<unsigned char> address_day_hash(16);

        MD5(reinterpret_cast<const unsigned char*>(input.data()),
            input.size(),
            address_day_hash.data());

        return arith_uint256("0x" + HexStr(address_day_hash)) < reference_hash;
    }

    //!
    //! \brief Get the hash of the pending superblock with the greatest vote
    //! weight.
    //!
    //! \param pindex Provides context about the chain tip used to calculate
    //! quorum vote weights.
    //!
    //! \return Quorum hash of the most popular superblock or an invalid hash
    //! when no nodes voted in the current superblock cycle.
    //!
    QuorumHash FindPopularHash(const CBlockIndex* const pindex)
    {
        if (!pindex) {
            return QuorumHash();
        }

        RefillVoteCache(pindex);

        const PopularityMap popularity_map = BuildPopularityMap(pindex->nHeight);
        auto popular = popularity_map.cbegin();

        if (popular == popularity_map.cend()) {
            return QuorumHash();
        }

        if (popularity_map.size() == 1) {
            return popular->first;
        }

        for (auto iter = std::next(popular); iter != popularity_map.end(); ++iter) {
            if (iter->second > popular->second) {
                popular = iter;
            }
        }

        return popular->first;
    }

    //!
    //! \brief Store a node's vote for a pending superblock in the cache.
    //!
    //! \param quorum_hash Hash of the superblock the node voted for.
    //! \param grc_address Default wallet address of the node.
    //! \param pindex      The block that contains the vote to store.
    //!
    void RecordVote(
        const QuorumHash& quorum_hash,
        const std::string& grc_address,
        const CBlockIndex* const pindex)
    {
        if (!pindex || !quorum_hash.Valid()) {
            return;
        }

        CBitcoinAddress address(grc_address);

        if (!address.IsValid()) {
            LogPrint(BCLog::LogFlags::VERBOSE,
                     "Quorum::RecordVote: invalid address: %s HASH: %s",
                     grc_address,
                     quorum_hash.ToString());

            return;
        }

        if (!Participating(grc_address, pindex->nTime)) {
            LogPrint(BCLog::LogFlags::VERBOSE, "Quorum::RecordVote: not participating: %s HASH: %s",
                     grc_address,
                     quorum_hash.ToString());

            return;
        }

        m_votes.emplace(pindex->nHeight, QuorumVote(
            quorum_hash,
            std::move(address),
            pindex));
    }

    //!
    //! \brief Remove a node's vote for a pending superblock from the cache.
    //!
    //! \param pindex The block that contains the vote to remove.
    //!
    void ForgetVote(const CBlockIndex* const pindex)
    {
        if (!pindex) {
            return;
        }

        m_votes.erase(pindex->nHeight);
    }

private:
    //!
    //! \brief Associates superblock hashes with the sum of their vote weights.
    //!
    typedef std::unordered_map<QuorumHash, double> PopularityMap;

    //!
    //! \brief Cache of recent superblock votes.
    //!
    //! This class caches superblock votes in received blocks to avoid loading
    //! blocks from disk to calculate pending vote weights. This significantly
    //! increases the initial block synchronization performance when comparing
    //! it to the previous implementation but uses slightly more memory.
    //!
    std::map<int64_t, QuorumVote> m_votes;

    //!
    //! \brief Load superblock votes from disk to prepare the cache for a
    //! recount of pending superblock popularity.
    //!
    //! Typically, this method will only read from disk when the application
    //! first starts or when a reorganization disconnects a sizable batch of
    //! blocks from the chain.
    //!
    //! For simplicity, this caching strategy maintains a constant-sized set
    //! of the most recent blocks needed for a full lookback. We can improve
    //! the memory consumption of the cache by choosing a more precise limit
    //! based on memoization of the heights that voting begins. The cache is
    //! used for historical superblock validation only, so the memory gained
    //! is not really worth the additional complexity.
    //!
    //! \param pindex Block to load votes backward from.
    //!
    void RefillVoteCache(const CBlockIndex* pindex)
    {
        const int64_t min_height = pindex->nHeight - STANDARD_LOOKBACK;

        if (!m_votes.empty()) {
            pindex = m_votes.begin()->second.m_pindex;

            if (pindex->nHeight <= min_height) {
                PurgeVoteCache(min_height);
                return;
            }
        }

        while (pindex && pindex->nHeight > min_height) {
            CBlock block;
            block.ReadFromDisk(pindex);

            if (block.GetClaim().m_quorum_hash.Valid()) {
                Claim claim = block.PullClaim();

                RecordVote(
                    claim.m_quorum_hash,
                    std::move(claim.m_quorum_address),
                    pindex);
            }

            pindex = pindex->pprev;
        }
    }

    //!
    //! \brief Erase stale entries from the vote cache.
    //!
    //! \param min_height Purge votes in blocks below this height.
    //!
    void PurgeVoteCache(const int64_t min_height)
    {
        auto iter = m_votes.cbegin();

        while (m_votes.size() > STANDARD_LOOKBACK && iter->first < min_height) {
            iter = m_votes.erase(iter);
        }
    }

    //!
    //! \brief Recalculate the vote weight for recent pending superblocks.
    //!
    //! \param last_height Height of the chain tip that defines the high end of
    //! the voting window.
    //!
    //! \return Sums of vote weights of the pending superblocks found in blocks
    //! within the voting window.
    //!
    PopularityMap BuildPopularityMap(const int64_t last_height) const
    {
        // Note: we begin aggregating vote weights from the block before
        // last_height. However, last_height is still the basis for vote
        // weight distance calculations.
        //
        const int64_t max_height = last_height - 1;
        const int64_t min_height = last_height - STANDARD_LOOKBACK;

        PopularityMap popularity_map;
        std::map<CBitcoinAddress, QuorumHash> addresses_seen;

        for (const auto& vote_pair : reverse_iterate(m_votes)) {
            const int64_t vote_height = vote_pair.first;
            const QuorumVote& vote = vote_pair.second;

            if (vote_height > max_height) {
                continue;
            }

            if (vote_height < min_height) {
                break;
            }

            const auto address_iter = addresses_seen.find(vote.m_address);

            // Ignore addresses that voted multiple times for the same hash:
            if (address_iter != addresses_seen.end()
                && address_iter->second == vote.m_hash)
            {
                continue;
            }

            // Sum vote weights to determine overall hash popularity:
            const double distance = last_height - vote_height + 10;
            const double multiplier = distance < 40 ? 400 : 200;

            popularity_map[vote.m_hash] += (1 / distance) * multiplier;
            addresses_seen[vote.m_address] = vote.m_hash;
        }

        return popularity_map;
    }
}; // LegacyConsensus

//!
//! \brief Validates received superblocks against the local scraper manifest
//! data to prevent superblock statistics spoofing attacks.
//!
//! When a node publishes a superblock in a generated block, every node in the
//! network validates the superblock by comparing it to manifest data received
//! from the scrapers. For validation to pass, a node must generate a matching
//! superblock from the local manifest data.
//!
class SuperblockValidator
{
public:
    //!
    //! \brief Describes a superblock validation outcome.
    //!
    enum class Result
    {
        UNKNOWN,           //!< Not enough manifest data to try validation.
        INVALID,           //!< It does not match a valid set of manifest data.
        HISTORICAL,        //!< It is older than the manifest retention period.
        VALID_CURRENT,     //!< It matches the current cached convergence.
        VALID_PAST,        //!< It matches a cached past convergence.
        VALID_BY_MANIFEST, //!< It matches a single manifest supermajority.
        VALID_BY_PROJECT,  //!< It matches by-project fallback supermajority.
    };

    //!
    //! \brief Create a new validator for the provided superblock.
    //!
    //! \param superblock The superblock data to validate.
    //!
    SuperblockValidator(const SuperblockPtr& superblock, size_t hint_bits = 32)
        : m_superblock(superblock)
        , m_quorum_hash(superblock->GetHash())
        , m_hint_shift(32 + clamp<size_t>(32 - hint_bits, 0, 32))
    {
    }

    //!
    //! \brief Perform the validation of the superblock.
    //!
    //! \param use_cache If \c false, skip attempts to validate the superblock
    //! using the cached scraper convergences.
    //!
    //! \return A value that describes the outcome of the validation.
    //!
    Result Validate(const bool use_cache = true) const
    {
        const Superblock local_contract = ScraperGetSuperblockContract(true);

        // If we cannot produce a superblock for comparison from the local set
        // of manifest data, we don't have enough context to try to validate a
        // superblock. Skip the validation and rely on peers to validate it.
        //
        // We also skip the validation if a superblock is encountered while
        // we are not in sync. This results in effectively no loss in network
        // security, because the vast majority of nodes that encounter a
        // staked SB will be in sync, and therefore will reject the SB if it
        // is bad. Any node syncing during that time may accept the bad SB, but
        // then will either reorganize the accepted but bad SB out based on
        // chain trust, or fork, which is what is desired.
        //
        if (!local_contract.WellFormed()) {
            return Result::UNKNOWN;
        }

        if (m_superblock.Age(GetAdjustedTime()) > SCRAPER_CMANIFEST_RETENTION_TIME) {
            return Result::HISTORICAL;
        }

        if (use_cache) {
            if (m_quorum_hash == local_contract.GetHash()) {
                return Result::VALID_CURRENT;
            }

            LogPrintf("ValidateSuperblock(): No match to current convergence.");

            if (TryRecentPastConvergence()) {
                return Result::VALID_PAST;
            }
        }

        LogPrintf("ValidateSuperblock(): No match using cached convergence.");

        if (!m_superblock->ConvergedByProject() && TryByManifest()) {
            return Result::VALID_BY_MANIFEST;
        }

        LogPrintf("ValidateSuperblock(): No match by manifest.");

        if (m_superblock->ConvergedByProject() && TryProjectFallback()) {
            return Result::VALID_BY_PROJECT;
        }

        LogPrintf("ValidateSuperblock(): No match by project.");

        {
            LOCK(cs_ConvergedScraperStatsCache);

            if (OutOfSyncByAge() || !ConvergedScraperStatsCache.bMinHousekeepingComplete) {
                LogPrintf("ValidateSuperblock(): No validation achieved, but node is"
                          "not in sync or minimum housekeeping is not complete"
                          " - skipping validation.");

                return Result::UNKNOWN;
            }
        }

        return Result::INVALID;
    }

private: // SuperblockValidator classes

    //!
    //! \brief Maps candidate project part hashes to a set of scrapers.
    //!
    //! Each set must hold at least the minimum number of scraper IDs for a
    //! supermajority for a project part to be considered for convergence.
    //!
    typedef std::map<uint256, std::set<ScraperID>> CandidatePartHashMap;

    //!
    //! \brief Represents a locally-available manifest project part resolved
    //! from a hint in a superblock.
    //!
    struct ResolvedPart
    {
        uint256 m_part_hash;            //!< Hash of the resolved part.
        uint256 m_source_manifest_hash; //!< Manifest that contains this part.
        int64_t m_source_manifest_time; //!< For selecting the beacon list.

        ResolvedPart(
            uint256 part_hash,
            uint256 source_manifest_hash,
            int64_t source_manifest_time)
            : m_part_hash(part_hash)
            , m_source_manifest_hash(source_manifest_hash)
            , m_source_manifest_time(source_manifest_time)
        {
        }
    };

    //!
    //! \brief Maintains the context of a whitelisted project for validating
    //! fallback-to-project convergence scenarios.
    //!
    struct ResolvedProject
    {
        //!
        //! \brief The manifest part hashes procured from the convergence hints
        //! in the superblock that may correspond to the parts of this project.
        //!
        //! The \c ProjectResolver will attempt to match these hashes to a part
        //! contained in a manifest for each scraper to find a supermajority.
        //!
        //! Each set must hold at least the minimum number of scraper IDs for a
        //! supermajority for each project or the superblock validation fails.
        //!
        CandidatePartHashMap m_candidate_hashes;

        //!
        //! \brief The manifest part hashes found in a manifest published by a
        //! scraper used to retrieve the part for the project to construct the
        //! convergence for comparison to the superblock.
        //!
        //! After successfully matching each of the convergence hints in the
        //! superblock to a manifest project, the \c ProjectCombiner selects
        //! a combination of the resolved parts from each \c ResolvedProject
        //! to construct convergences until one matches the superblock.
        //!
        std::vector<ResolvedPart> m_resolved_parts;

        //!
        //! \brief Divisor set by the \c ProjectCombiner for iteraton over each
        //! convergence combination.
        //!
        size_t m_combiner_mask;

        //!
        //! \brief Initialize a new project context object.
        //!
        ResolvedProject() : m_combiner_mask(0)
        {
        }

        //!
        //! \brief Initialize a new project context object with the provided
        //! manifest part hashes.
        //!
        //! \param candidate hashes The manifest part hashes procured from the
        //! convergence hints in the superblock.
        //!
        ResolvedProject(CandidatePartHashMap candidate_hashes)
            : m_candidate_hashes(std::move(candidate_hashes))
            , m_combiner_mask(0)
        {
        }

        //!
        //! \brief Determine whether the supplied manifest part matches a part
        //! for the convergence of this project.
        //!
        //! \param part_hash The hash of a project part from a manifest.
        //!
        //! \return \c true if the part hash matches a part annotated for this
        //! project by a hint in the validated superblock.
        //!
        bool Expects(const uint256& part_hash) const
        {
            return m_candidate_hashes.count(part_hash) > 0;
        }

        //!
        //! \brief Associate a scraper for the specified part.
        //!
        //! \param part_hash     Hash of the project part to tally.
        //! \param scraper_id    The scraper that published the specified part.
        //! \param supermajority Number of scrapers that must agree on a part.
        //!
        //! \return \c true if the part is associated with a supermajority of
        //! scrapers.
        //!
        bool Tally(const uint256& part_hash, ScraperID scraper_id, size_t supermajority)
        {
            auto& scrapers = m_candidate_hashes.at(part_hash);

            scrapers.emplace(std::move(scraper_id));

            return scrapers.size() >= supermajority;
        }

        //!
        //! \brief Commit the part hash to this project and record the timestamp
        //! of the manifest.
        //!
        //! \param part Hashes of the candidate part and source manifest.
        //!
        void LinkPart(ResolvedPart part)
        {
            m_candidate_hashes.erase(part.m_part_hash);

            m_resolved_parts.emplace_back(part);
        }
    };

    //!
    //! \brief Generates a superblock hash from the contained convergence of
    //! manifest parts for comparison to the validated superblock.
    //!
    class ConvergenceCandidate
    {
    public:
        //!
        //! \brief Add the provided manifest part to the convergence.
        //!
        //! \param project_name      Identifies the project to add.
        //! \param project_part_data Serialized project stats of the part.
        //!
        void AddPart(std::string project_name, CSplitBlob::CPart* project_part_ptr)
        {
            m_convergence.ConvergedManifestPartPtrsMap.emplace(
                std::move(project_name),
                std::move(project_part_ptr));
        }

        //!
        //! \brief Calculate a superblock hash from the supplied manifest data
        //! that matches the set of resolved project parts.
        //!
        //! \return A superblock hash generated from the convergence to compare
        //! to the superblock under validation.
        //!
        QuorumHash ComputeQuorumHash() const
        {
            const ScraperStatsAndVerifiedBeacons stats_and_verified_beacons = GetScraperStatsByConvergedManifest(m_convergence);

            return QuorumHash::Hash(stats_and_verified_beacons);
        }

    private:
        ConvergedManifest m_convergence; //!< Used to compute a superblock hash
    };

    //!
    //! \brief Constructs by-project convergences from the supplied set of the
    //! project parts resolved from the superblock.
    //!
    //! Nodes validate superblocks in by-project fallback convergence cases by
    //! matching manifest project parts to the convengence hints embedded in a
    //! superblock project section. These hints are truncated SHA256 hashes so
    //! a hint can qualify more than one manifest part for consideration while
    //! reconstructing a convergence for validation.
    //!
    //! This class behaves like a cursor through each combination of the parts
    //! it is initialized with. For each call to \c GetNextConvergence(), this
    //! class generates a convergence using a new combination of project parts
    //! until it exhausts every combination or until the client code matches a
    //! superblock with one of the results.
    //!
    //! We have built here a catapult that fires a huge boulder to kill a tiny
    //! bird--the chance of a project part hash collision resolved by the hint
    //! in the superblock is incredibly small, but this provision prevents the
    //! validation of a superblock from failing if a collision ever occurs.
    //!
    class ProjectCombiner
    {
    public:
        //!
        //! \brief Initialize a project combiner with the provided collection
        //! of resolved projects.
        //!
        //! \param projects Contains matching manifest parts hashes for each
        //! project hinted in the superblock.
        //!
        ProjectCombiner(std::map<std::string, ResolvedProject> projects)
            : m_projects(std::move(projects))
            , m_total_combinations(1)
            , m_current_combination(0)
        {
            for (auto& project_pair : reverse_iterate(m_projects)) {
                project_pair.second.m_combiner_mask = m_total_combinations;
                m_total_combinations *= project_pair.second.m_resolved_parts.size();
            }
        }

        //!
        //! \brief Initialize a project combiner that produces no results.
        //!
        ProjectCombiner()
            : m_total_combinations(0)
            , m_current_combination(0)
        {
        }

        //!
        //! \brief Get the total number of possible project part combinations
        //! that the object can create convergences from.
        //!
        //! \return
        //!
        size_t TotalCombinations() const
        {
            return m_total_combinations;
        }

        //!
        //! \brief Generate the next convergence in sequence from the resolved
        //! project parts.
        //!
        //! Jim Owens enlightens us with a stimulating description of his algorithm:
        //!
        //! Compute number of permutations and cumulative placevalue. For instance, if there were 4 projects, and
        //! the number of candidate parts for each of the projects were 2, 3, 2, and 2, then the total number of
        //! permutations would be 2 * 3 * 2 * 2 = 24, and the cumulative placevalue would be
        //! -------------------- 12,  4,  2,  1. Notice we will use a reverse iterator to fill in the placevalue
        //! as we compute the total number of permutations. This is ridiculous of course because 99.99999% of the
        //! time it is going to be 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 (one permutation) or at most something like
        //! 1 1 1 1 1 1 1 1 2 1 1 1 1 1 1 1 1 1 1, which will resolve to two permutations, but I am doing the full algorithm
        //! to be thorough.
        //!
        //! A further explanation...
        //!
        //! The easiest way to understand this algorithm is to compare it to a binary number....
        //! Each place has two possibilities, so the total possibilities are 2^n where you have n places.
        //! I.e if you had 4 places (think four projects) each with 2 candidate parts, you would have 0000 to 1111.
        //! This is permutation index 0 to 7 for a total of eight permutations. If the outer loop is the
        //! permutation index, you derive the place values by multiplying the combinations for the places
        //! up to but not including the place for the four digit binary number ... the place values are 8, 4, 2, 1.
        //!
        //! So, if you are at index 5 this is, starting at the most significant digit,
        //! 5/8 = 0 remainder 5.
        //! 5/4 = 1 remainder 1.
        //! 1/2 = 0 remainder 1.
        //! 1/1 = 1 remainder 0.
        //! (This of course is 0101 binary for 5.)
        //!
        //! If the number of candidate parts varies for each project (which it will in this problem), the place
        //! is a different base for each place, and then you track the number of possibilities in each place with
        //! the helper as I described in the comments. This allows the outer loop to simply run consecutively through
        //! the permutation index from 0 to n-1 permutations, and then for each permutation index value the repeated
        //! division in the inner loop  “decodes” the index into the selected part for each placeholder (project).
        //! Since you calculated the permutation index max n by muliplyjng all of the possibilities together for each
        //! place you know you have covered them all. I think it is a pretty elegant approach to the problem with iteration.
        //!
        //! \return A convergence to generate a superblock hash from if a new
        //! combination is possible.
        //!
        boost::optional<ConvergenceCandidate> GetNextConvergence()
        {
            if (m_current_combination == m_total_combinations) {
                return boost::none;
            }

            ConvergenceCandidate convergence;
            size_t remainder = m_current_combination;
            uint256 latest_manifest;
            int64_t latest_manifest_time = 0;

            for (const auto& project_pair : m_projects) {
                const ResolvedProject& project = project_pair.second;
                const size_t part_index = remainder / project.m_combiner_mask;

                    LogPrint(BCLog::LogFlags::VERBOSE,
                        "ValidateSuperblock(): uncached by project: "
                        "%" PRIszu "/%" PRIszu " = part_index = %" PRIszu " index max = %" PRIszu,
                        remainder,
                        project.m_combiner_mask,
                        part_index,
                        project.m_resolved_parts.size() - 1);

                const auto& resolved_part = project.m_resolved_parts[part_index];

                convergence.AddPart(
                    project_pair.first, // project name
                    GetResolvedPartPtr(resolved_part.m_part_hash));

                remainder -= part_index * project.m_combiner_mask;

                // Find the most recent manifest that provided one of the parts
                // to fetch the beacon list from:
                //
                if (resolved_part.m_source_manifest_time > latest_manifest_time) {
                    latest_manifest = resolved_part.m_source_manifest_hash;
                    latest_manifest_time = resolved_part.m_source_manifest_time;
                }
            }

            AddBeaconPartsData(convergence, latest_manifest);

            ++m_current_combination;

            return boost::make_optional(std::move(convergence));
        }

    private:
        //!
        //! \brief The collection of manifest project parts grouped by project
        //! to create convergence combinations from.
        //!
        std::map<std::string, ResolvedProject> m_projects;

        size_t m_total_combinations;  //!< Number of project part combinations.
        size_t m_current_combination; //!< Number of the combination to try.

        //!
        //! \brief Fetch the project part data for the specified part hash.
        //!
        //! \param part_hash Identifies the project part to fetch.
        //!
        //! \return Serialized binary data of the part to add to a convergence.
        //!
        static CSplitBlob::CPart* GetResolvedPartPtr(const uint256& part_hash)
        {
            LOCK(CSplitBlob::cs_mapParts);

            const auto iter = CSplitBlob::mapParts.find(part_hash);

            // If the resolved part disappeared, we cannot proceed, but
            // the most recent project part should always exist:
            if (iter == CSplitBlob::mapParts.end()) {
                LogPrintf("ValidateSuperblock(): project part disappeared.");
                return nullptr;
            }

            return &(iter->second);
        }

        //!
        //! \brief Insert the beacon list and verified beacons part data from
        //! the specified manifest into the provided convergence candidate.
        //!
        //! \param convergence   Convergence to add the beacon parts to.
        //! \param manifest_hash Identifies the manifest to fetch the parts from.
        //!
        static void AddBeaconPartsData(
            ConvergenceCandidate& convergence,
            const uint256& manifest_hash)
        {
            LOCK(CScraperManifest::cs_mapManifest);

            const auto iter = CScraperManifest::mapManifest.find(manifest_hash);

            // If the manifest for the beacon list disappeared, we cannot
            // proceed, but the most recent manifest should always exist:
            if (iter == CScraperManifest::mapManifest.end()) {
                LogPrintf("ValidateSuperblock(): beacon manifest disappeared.");
                return;
            }

            const CScraperManifest_shared_ptr manifest = iter->second;

            // If the manifest for the beacon list is now empty, we cannot
            // proceed, but ProjectResolver should always select manifests
            // with a beacon list part:
            if (manifest->vParts.empty()) {
                LogPrintf("ValidateSuperblock(): beacon list part missing.");
                return;
            }

            convergence.AddPart("BeaconList", manifest->vParts[0]);

            // Find the offset of the verified beacons project part. Typically
            // this exists at vParts offset 1 when a scraper verified at least
            // one pending beacon. If it doesn't exist, omit the part from the
            // reconstructed convergence:
            const auto verified_beacons_entry_iter = std::find_if(
                manifest->projects.begin(),
                manifest->projects.end(),
                [](const CScraperManifest::dentry& entry) {
                    return entry.project == "VerifiedBeacons";
                });

            if (verified_beacons_entry_iter == manifest->projects.end()) {
                LogPrintf("ValidateSuperblock(): verified beacon project missing.");
                return;
            }

            const size_t part_offset = verified_beacons_entry_iter->part1;

            if (part_offset == 0 || part_offset >= manifest->vParts.size()) {
                LogPrintf("ValidateSuperblock(): out-of-range verified beacon part.");
                return;
            }

            convergence.AddPart("VerifiedBeacons", manifest->vParts[part_offset]);
        }
    }; // ProjectCombiner

    //!
    //! \brief Prepares a set of manifest parts for each project hinted in the
    //! superblock to find a supermajority for every project.
    //!
    class ProjectResolver
    {
    public:
        //!
        //! \brief Initialize a new project resolver.
        //!
        //! \param candidate_parts      Map of project names to manifest part
        //! hashes produced from superblock convergence hints.
        //! \param manifests_by_scraper Manifest hashes grouped by scraper.
        //!
        ProjectResolver(
            std::map<std::string, CandidatePartHashMap> candidate_parts,
            mmCSManifestsBinnedByScraper manifests_by_scraper)
            : m_manifests_by_scraper(std::move(manifests_by_scraper))
            , m_supermajority(NumScrapersForSupermajority(m_manifests_by_scraper.size()))
        {
            for (auto&& project_part_pair : candidate_parts) {
                m_resolved_projects.emplace(
                    std::move(project_part_pair.first),
                    ResolvedProject(std::move(project_part_pair.second)));
            }
        }

        //!
        //! \brief Scan the local manifest data to reconstruct a supermajority
        //! of manifest parts that match the candidate parts in the superblock.
        //!
        //! \return \c false after failing to resolve a matching part for each
        //! of the projects in the superblock, or when the superblock contains
        //! only some of the converged projects.
        //!
        ProjectCombiner ResolveProjectParts()
        {
            // Collect the manifest parts that match the hints in the superblock
            // and record missing projects, if any:
            //
            for (const auto& by_scraper : m_manifests_by_scraper) {
                const ScraperID& scraper_id = by_scraper.first;
                const mCSManifest& hashes_by_time = by_scraper.second;

                for (const auto& hashes : hashes_by_time) {
                    const uint256& manifest_hash = hashes.second.first;

                    ResolvePartsFor(manifest_hash, scraper_id);
                }
            }

            // Check that superblock is not missing a project that the scrapers
            // successfully converged on:
            //
            for (const auto& project_pair : m_other_projects) {
                if (project_pair.second.size() >= m_supermajority) {
                    LogPrintf(
                        "ValidateSuperblock(): fallback by-project resolution "
                        "failed. Converged project %s missing from superblock.",
                        project_pair.first);

                    return ProjectCombiner();
                }
            }

            // Check that each of the project parts hinted in the superblock
            // matched a manifest part that the scrapers converged on:
            //
            for (const auto& project_pair : m_resolved_projects) {
                if (project_pair.second.m_resolved_parts.empty()) {
                    LogPrintf(
                        "ValidateSuperblock(): fallback by-project resolution "
                        "failed. No manifest parts matched project %s.",
                        project_pair.first);

                    return ProjectCombiner();
                }
            }

            return ProjectCombiner(std::move(m_resolved_projects));
        }

    private:
        //!
        //! \brief Manifest hashes grouped by scraper to resolve a by-project
        //! fallback convergence from.
        //!
        const mmCSManifestsBinnedByScraper m_manifests_by_scraper;

        //!
        //! \brief The number of scrapers that must agree about a project to
        //! consider it valid in a superblock.
        //!
        const size_t m_supermajority;

        //!
        //! \brief Contains the project resolution context for each of the
        //! projects hinted in the superblock.
        //!
        //! Keyed by project name.
        //!
        std::map<std::string, ResolvedProject> m_resolved_projects;

        //!
        //! \brief Contains the scrapers that published manifest data for any
        //! projects not hinted in the superblock.
        //!
        //! Keyed by project name. A scraper supermajority for any project
        //! disqualifies the superblock (it failed to include the project).
        //!
        std::map<std::string, std::set<ScraperID>> m_other_projects;

        //!
        //! \brief Record the supplied scraper ID for the specified project to
        //! track supermajority status.
        //!
        //! \return A pointer to the project resolution state if the project
        //! exists in the superblock.
        //!
        ResolvedProject*
        TallyProject(const std::string& project, const ScraperID& scraper_id)
        {
            if (m_resolved_projects.count(project)) {
                return &m_resolved_projects.at(project);
            }

            // The special beacon list and verified beacons parts are stored as
            // projects parts. These are not considered for convergence at this
            // stage:
            //
            if (project == "BeaconList" || project == "VerifiedBeacons") {
                return nullptr;
            }

            m_other_projects[project].emplace(scraper_id);

            return nullptr;
        }

        //!
        //! \brief Match each part hinted by the superblock to a local manifest
        //! published by each scraper.
        //!
        //! \param manifest_hash Hash of the manifest to match parts for.
        //! \param scraper_id    Used to establish supermajority for a manifest.
        //!
        void ResolvePartsFor(const uint256& manifest_hash, const ScraperID& scraper_id)
        {
            LOCK(CScraperManifest::cs_mapManifest);

            const auto iter = CScraperManifest::mapManifest.find(manifest_hash);

            if (iter == CScraperManifest::mapManifest.end()) {
                return;
            }

            const CScraperManifest_shared_ptr manifest = iter->second;

            for (const auto& entry : manifest->projects) {
                auto project_option = TallyProject(entry.project, scraper_id);

                // If this project does not exist in the superblock, skip the
                // attempt to associate its parts:
                //
                if (!project_option || entry.part1 >= (int)manifest->vParts.size()) {
                    continue;
                }

                const uint256& part_hash = manifest->vParts[entry.part1]->hash;

                if (project_option->Expects(part_hash)
                    && project_option->Tally(part_hash, scraper_id, m_supermajority))
                {
                    project_option->LinkPart(ResolvedPart(
                        manifest->vParts[entry.part1]->hash,
                        manifest_hash,
                        entry.LastModified));
                }
            }
        }
    }; // ProjectResolver

private: // SuperblockValidator fields

    const SuperblockPtr& m_superblock; //!< Points to the superblock to validate.
    const QuorumHash m_quorum_hash;    //!< Hash of the superblock to validate.
    const size_t m_hint_shift;         //!< For testing by-project combinations.

private: // SuperblockValidator methods

    //!
    //! \brief Validate the superblock by comparing it to recent past converged
    //! manifests in the cache.
    //!
    //! The scraper convergence cache stores a set of previous convergences for
    //! the current superblock cycle with matching superblock hashes. This step
    //! accepts superblocks if the embedded converged manifest hash hints match
    //! a hash of a past convergence in this cache.
    //!
    //! \return \c true if the hinted convergence's superblock hash matches the
    //! hash of the validated superblock.
    //!
    bool TryRecentPastConvergence() const
    {
        LOCK(cs_ConvergedScraperStatsCache);

        const uint32_t hint = m_superblock->m_convergence_hint;
        const auto iter = ConvergedScraperStatsCache.PastConvergences.find(hint);

        return iter != ConvergedScraperStatsCache.PastConvergences.end()
            && iter->second.first == m_quorum_hash;
    }

    //!
    //! \brief Validate the superblock by comparing it to all of the manifests
    //! stored locally on the node with a content hash that matches the hint.
    //!
    //! When no cached manifest matches the superblock, we can try to match it
    //! to a single manifest with a content hash that the supermajority of the
    //! scrapers agree on. This routine only selects the manifests that have a
    //! content hash corresponding to the convergence hint in the superblock.
    //!
    //! \return \c true when the hinted manifest's superblock hash matches the
    //! hash of the validated superblock.
    //!
    bool TryByManifest() const
    {
        const mmCSManifestsBinnedByScraper manifests_by_scraper = ScraperCullAndBinCScraperManifests();
        const size_t supermajority = NumScrapersForSupermajority(manifests_by_scraper.size());

        std::map<uint256, size_t> content_hash_tally;

        for (const auto& by_scraper : manifests_by_scraper) {
            const mCSManifest& hashes_by_time = by_scraper.second;

            for (const auto& hashes : hashes_by_time) {
                const uint256& manifest_hash = hashes.second.first;
                const uint256& content_hash = hashes.second.second;

                if (content_hash.GetUint64() >> 32 == m_superblock->m_manifest_content_hint) {
                    if (!content_hash_tally.emplace(content_hash, 1).second) {
                        content_hash_tally[content_hash]++;
                    }

                    LogPrint(BCLog::LogFlags::VERBOSE,
                             "ValidateSuperblock(): manifest content hash %s "
                             "matched convergence hint. Matches %" PRIszu,
                             content_hash.ToString(),
                             content_hash_tally[content_hash]);

                    if (content_hash_tally[content_hash] >= supermajority) {
                            LogPrint(BCLog::LogFlags::VERBOSE,
                                "ValidateSuperblock(): supermajority found for "
                                "manifest content hash: %s. Trying validation.",
                                content_hash.ToString());

                        if (TryManifest(manifest_hash)) {
                            return true;
                        } else {
                            // Disqualify this content hash:
                            content_hash_tally[content_hash] = 0;
                        }
                    }
                }
            }
        }

        return false;
    }

    //!
    //! \brief Validate the superblock by comparing the specified manifest.
    //!
    //! \return \c true if the specified manifest builds a superblock that
    //! matches the validated superblock.
    //!
    bool TryManifest(const uint256& manifest_hash) const
    {
        CScraperManifest_shared_ptr manifest;

        LOCK(CScraperManifest::cs_mapManifest);

        const auto iter = CScraperManifest::mapManifest.find(manifest_hash);

        if (iter == CScraperManifest::mapManifest.end()) {
            LogPrintf("ValidateSuperblock(): manifest not found");
            return false;
        }

        manifest = iter->second;

        return TryManifest(manifest);
    }

    //!
    //! \brief Validate the superblock by comparing the provided manifest.
    //!
    //! \return \c true if the provided manifest builds a superblock that
    //! matches the validated superblock.
    //!
    bool TryManifest(CScraperManifest_shared_ptr& manifest) const
    {
        const ScraperStatsAndVerifiedBeacons stats_and_verified_beacons = GetScraperStatsFromSingleManifest(manifest);

        return QuorumHash::Hash(stats_and_verified_beacons) == m_quorum_hash;
    }

    //!
    //! \brief Validate the superblock by comparing it to a convergence created
    //! by matching the project convergence hints in that superblock to project
    //! parts agreed upon by a supermajority of the scrapers.
    //!
    //! In the fallback-to-project-level convergence scenario, superblocks will
    //! contain project convergence hints for the manifest parts used to create
    //! the superblock. This routine matches each project convergence hint to a
    //! manifest part for each project. If a node can reconstruct a convergence
    //! from those parts agreed upon by a supermajority of scrapers using local
    //! manifest data, validation passes for the superblock.
    //!
    //! Validation will fail if the reconstructed convergence contains projects
    //! not present in the superblock.
    //!
    //! \return \c true if the reconstructed convergence by project builds a
    //! superblock that matches the validated superblock.
    //!
    bool TryProjectFallback() const
    {
        const std::multimap<uint32_t, std::string> hints = CollectPartHints();
        std::map<std::string, CandidatePartHashMap> candidates = CollectCandidateParts(hints);

        if (candidates.size() != hints.size()) {
            LogPrintf(
                "ValidateSuperblock(): fallback by-project resolution failed. "
                "Could not match every project convergence hint to a part.");

            return false;
        }

        ProjectResolver resolver(std::move(candidates), ScraperCullAndBinCScraperManifests());
        ProjectCombiner combiner = resolver.ResolveProjectParts();

        LogPrint(BCLog::LogFlags::VERBOSE,
                 "ValidateSuperblock(): by-project possible combinations: %" PRIszu,
                 combiner.TotalCombinations());

        while (const auto combination_option = combiner.GetNextConvergence()) {
            if (combination_option->ComputeQuorumHash() == m_quorum_hash) {
                return true;
            }
        }

        return false;
    }

    //!
    //! \brief Build a collection of project-level convergence hints from the
    //! superblock.
    //!
    //! \return A map of manifest project part hints to project names.
    //!
    std::multimap<uint32_t, std::string> CollectPartHints() const
    {
        std::multimap<uint32_t, std::string> hints;
        size_t hint_shift = m_hint_shift - 32;

        for (const auto& project_pair : m_superblock->m_projects) {
            hints.emplace(
                project_pair.second.m_convergence_hint >> hint_shift,
                project_pair.first); // Project name
        }

        return hints;
    }

    //!
    //! \brief Build a collection of manifest project part hashes from the
    //! supplied convergence hints.
    //!
    //! \return A map of project names to manifest project part hashes.
    //!
    std::map<std::string, CandidatePartHashMap>
    CollectCandidateParts(const std::multimap<uint32_t, std::string>& hints) const
    {
        std::map<std::string, CandidatePartHashMap> candidates;

        LOCK(CSplitBlob::cs_mapParts);

        for (const auto& part_pair : CSplitBlob::mapParts) {
            const uint32_t hint = part_pair.first.GetUint64() >> m_hint_shift;
            const auto hint_range = hints.equal_range(hint);

            for (auto i = hint_range.first; i != hint_range.second; ++i) {
                candidates[i->second].emplace(
                    part_pair.second.hash,
                    std::initializer_list<ScraperID> { });
            }
        }

        return candidates;
    }
}; // SuperblockValidator

//!
//! \brief Stores recent superblock data.
//!
SuperblockIndex g_superblock_index;

//!
//! \brief Orchestrates superblock voting for legacy quorum consensus.
//!
LegacyConsensus g_legacy_consensus;

} // anonymous namespace


// -----------------------------------------------------------------------------
// Class: Quorum
// -----------------------------------------------------------------------------

bool Quorum::Participating(const std::string& grc_address, const int64_t time)
{
    return LegacyConsensus::Participating(grc_address, time);
}

QuorumHash Quorum::FindPopularHash(const CBlockIndex* const pindex)
{
    return g_legacy_consensus.FindPopularHash(pindex);
}

void Quorum::RecordVote(
    const QuorumHash quorum_hash,
    const std::string& grc_address,
    const CBlockIndex* const pindex)
{
    g_legacy_consensus.RecordVote(quorum_hash, grc_address, pindex);
}

void Quorum::ForgetVote(const CBlockIndex* const pindex)
{
    g_legacy_consensus.ForgetVote(pindex);
}

bool Quorum::ValidateSuperblockClaim(
    const Claim& claim,
    const SuperblockPtr& superblock,
    const CBlockIndex* const pindex)
{
    if (!SuperblockNeeded(pindex->nTime)) {
        return error("ValidateSuperblockClaim(): superblock too early.");
    }

    if (pindex->nVersion >= 11) {
        if (superblock->m_version < 2) {
            return error("ValidateSuperblockClaim(): rejected legacy version.");
        }

        // Superblocks are not included in the input for the claim hash
        // so we need to compare the computed hash to the claim hash to
        // protect the integrity of the superblock data:
        //
        if (superblock->GetHash() != claim.m_quorum_hash) {
            return error("ValidateSuperblockClaim(): quorum hash mismatch.");
        }

        return ValidateSuperblock(superblock);
    }

    const CBitcoinAddress address(claim.m_quorum_address);

    if (!address.IsValid()) {
        return error("ValidateSuperblockClaim(): "
            "invalid quorum address: %s", claim.m_quorum_address);
    }

    if (!Participating(claim.m_quorum_address, pindex->nTime)) {
        return error("ValidateSuperblockClaim(): "
            "ineligible quorum participant: %s", claim.m_quorum_address);
    }

    // Popular hash search begins from the block before:
    const QuorumHash popular_hash = FindPopularHash(pindex->pprev);

    if (superblock->GetHash() != popular_hash) {
        return error("ValidateSuperblockClaim(): "
            "superblock hash mismatch: %s popular: %s",
            superblock->GetHash().ToString(),
            popular_hash.ToString());
    }

    return true;
}

bool Quorum::ValidateSuperblock(
    const SuperblockPtr& superblock,
    const bool use_cache,
    const size_t hint_bits)
{
    using Result = SuperblockValidator::Result;

    const Result result = SuperblockValidator(superblock, hint_bits).Validate(use_cache);
    std::string message;

    switch (result) {
        case Result::UNKNOWN:
            message = "UNKNOWN - Waiting for manifest data";
            break;
        case Result::INVALID:
            message = "INVALID - Validation failed";
            break;
        case Result::HISTORICAL:
            message = "HISTORICAL - Skipped historical superblock.";
            break;
        case Result::VALID_CURRENT:
            message = "VALID_CURRENT - Matched current cached convergence";
            break;
        case Result::VALID_PAST:
            message = "VALID_PAST - Matched past cached convergence";
            break;
        case Result::VALID_BY_MANIFEST:
            message = "VALID_BY_MANIFEST - Matched supermajority by manifest";
            break;
        case Result::VALID_BY_PROJECT:
            message = "VALID_BY_PROJECT - Matched supermajority by project";
            break;
    }

    LogPrintf("ValidateSuperblock(): %s.", message);

    return result != Result::INVALID;
}

Magnitude Quorum::GetMagnitude(const Cpid cpid)
{
    return Quorum::CurrentSuperblock()->m_cpids.MagnitudeOf(cpid);
}

Magnitude Quorum::GetMagnitude(const MiningId mining_id)
{
    if (const auto cpid_option = mining_id.TryCpid()) {
        return GetMagnitude(*cpid_option);
    }

    return Magnitude::Zero();
}

std::vector<ExplainMagnitudeProject> Quorum::ExplainMagnitude(const Cpid cpid)
{
    // Force a scraper convergence update if needed:
    // TODO: unwrap this from ScraperGetSuperblockContract()
    CreateSuperblock();

    const std::string cpid_str = cpid.ToString();
    const Span<const char> cpid_span = MakeSpan(cpid_str);

    // Compare the stats entry CPID and return the project name if it matches:
    const auto try_item = [&](const std::string& object_id) {
        const Span<const char> id_span = MakeSpan(object_id);
        const Span<const char> cpid_subspan = id_span.last(32);

        if (cpid_subspan != cpid_span) {
            return Span<const char>();
        }

        return id_span.first(id_span.size() - 33);
    };

    std::vector<ExplainMagnitudeProject> projects;

    LOCK(cs_ConvergedScraperStatsCache);

    // Although the map is ordered, the keys begin with project names, so we
    // cannot binary search for a block of CPID entries yet:
    //
    for (const auto& entry : ConvergedScraperStatsCache.mScraperConvergedStats) {
        if (entry.first.objecttype == statsobjecttype::byCPIDbyProject) {
            const Span<const char> project_name = try_item(entry.first.objectID);

            if (project_name.size() > 0) {
                projects.emplace_back(
                    std::string(project_name.begin(), project_name.end()),
                    entry.second.statsvalue.dRAC,
                    entry.second.statsvalue.dMag);
            }
        }
    }

    return projects;
}

SuperblockPtr Quorum::CurrentSuperblock()
{
    return g_superblock_index.Current();
}

SuperblockPtr Quorum::PendingSuperblock()
{
    return g_superblock_index.Pending();
}

bool Quorum::HasPendingSuperblock()
{
    return g_superblock_index.HasPending();
}

bool Quorum::SuperblockNeeded(const int64_t now)
{
    if (HasPendingSuperblock()) {
        return false;
    }

    const SuperblockPtr superblock = g_superblock_index.Current();

    return !superblock->WellFormed()
        || superblock.Age(now) > GetSuperblockAgeSpacing(nBestHeight);

}

void Quorum::LoadSuperblockIndex(const CBlockIndex* pindexLast)
{
    if (!pindexLast) {
        return;
    }

    g_superblock_index.Reload(pindexLast);
}

Superblock Quorum::CreateSuperblock()
{
    return ScraperGetSuperblockContract();
}

void Quorum::PushSuperblock(SuperblockPtr superblock)
{
    LogPrintf("Quorum::PushSuperblock(%" PRId64 ")", superblock.m_height);

    g_superblock_index.PushSuperblock(std::move(superblock));
}

void Quorum::PopSuperblock(const CBlockIndex* const pindex)
{
    LogPrintf("Quorum::PopSuperblock(%" PRId64 ")", pindex->nHeight);

    g_superblock_index.PopSuperblock();
}

bool Quorum::CommitSuperblock(const uint32_t height)
{
    return g_superblock_index.Commit(height);
}
