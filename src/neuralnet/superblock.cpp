#include "compat/endian.h"
#include "hash.h"
#include "neuralnet/superblock.h"
#include "scraper_net.h"
#include "sync.h"
#include "util.h"
#include "util/reverse_iterator.h"

#include <boost/variant/apply_visitor.hpp>
#include <openssl/md5.h>

using namespace NN;

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);

// TODO: use a header
ScraperStats GetScraperStatsByConvergedManifest(const ConvergedManifest& StructConvergedManifest);
ScraperStats GetScraperStatsFromSingleManifest(CScraperManifest &manifest);
unsigned int NumScrapersForSupermajority(unsigned int nScraperCount);
mmCSManifestsBinnedByScraper ScraperCullAndBinCScraperManifests();
Superblock ScraperGetSuperblockContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false);

extern CCriticalSection cs_ConvergedScraperStatsCache;
extern ConvergedScraperStats ConvergedScraperStatsCache;

namespace {
//!
//! \brief Loads a provided set of scraper statistics into a superblock.
//!
//! \tparam T A superblock-like object. For example, a ScraperStatsQuorumHasher
//! object that hashes the input instead of storing the data.
//!
template<typename T>
class ScraperStatsSuperblockBuilder
{
    // The loop below depends on the relative value of these enum types:
    static_assert(statsobjecttype::NetworkWide < statsobjecttype::byCPID,
        "Unexpected enumeration order of scraper stats map object types.");
    static_assert(statsobjecttype::byCPID < statsobjecttype::byCPIDbyProject,
        "Unexpected enumeration order of scraper stats map object types.");
    static_assert(statsobjecttype::byProject < statsobjecttype::byCPIDbyProject,
        "Unexpected enumeration order of scraper stats map object types.");

public:
    //!
    //! \brief Initialize a instance that wraps the provided superblock.
    //!
    //! \param superblock A superblock-like object to load with scraper stats.
    //!
    ScraperStatsSuperblockBuilder(T& superblock) : m_superblock(superblock)
    {
    }

    //!
    //! \brief Load the provided scraper statistics into the wrapped superblock.
    //!
    //! \param stats A complete set of scraper statistics to build a superblock
    //! from.
    //!
    void BuildFromStats(const ScraperStats& stats)
    {
        for (const auto& entry : stats) {
            // A CPID, project name, or project name/CPID pair that identifies
            // the current statistics record:
            const std::string& object_id = entry.first.objectID;

            switch (entry.first.objecttype) {
                // This map starts with a single, network-wide statistics entry.
                // Skip it because superblock objects will recalculate the stats
                // as needed after deserialization.
                //
                case statsobjecttype::NetworkWide:
                    continue;

                case statsobjecttype::byCPID:
                    m_superblock.m_cpids.RoundAndAdd(
                        Cpid::Parse(object_id),
                        entry.second.statsvalue.dMag);

                    break;

                case statsobjecttype::byProject:
                    m_superblock.m_projects.Add(
                        object_id,
                        Superblock::ProjectStats(
                            std::nearbyint(entry.second.statsvalue.dTC),
                            std::nearbyint(entry.second.statsvalue.dAvgRAC),
                            std::nearbyint(entry.second.statsvalue.dRAC))
                    );

                    break;

                // The scraper statistics map orders the entries by "objecttype"
                // starting with "byCPID" and "byProject". After importing these
                // sections into the superblock, we can exit this loop.
                //
                default:
                    return;
            }
        }
    }
private:
    T& m_superblock; //!< Superblock-like object to fill with supplied stats.
};

//!
//! \brief Hashes scraper statistics to produce a quorum hash in the same manner
//! as the hash would be calculated for a superblock.
//!
//! Because of the size of superblock objects, the overhead of allocating and
//! filling superblocks from scraper convergences just to generate the quorum
//! hash for validation can be significant as superblocks grow larger. We can
//! compute a matching superblock hash directly from scraper statistics. This
//! class generates quorum hashes from scraper statistics that will match the
//! hashes of corresponding superblock objects.
//!
//! CONSENSUS: This class will only produce a SHA256 quorum hash for versions
//! 2+ superblocks. Do not use it to produce hashes of scraper statistics for
//! legacy superblocks.
//!
class ScraperStatsQuorumHasher
{
public:
    //!
    //! \brief Initialize a hasher with the provided scraper statistics.
    //!
    //! \param stats The scraper statistics to generate a hash from.
    //!
    ScraperStatsQuorumHasher(const ScraperStats& stats) : m_stats(stats)
    {
    }

    //!
    //! \brief Generate a quorum hash of the provided scraper statistics.
    //!
    //! \param stats The scraper statistics to generate a hash from.
    //!
    //! \return A hash that matches the hash of a corresponding superblock.
    //!
    static QuorumHash Hash(const ScraperStats& stats)
    {
        return ScraperStatsQuorumHasher(stats).GetHash();
    }

    //!
    //! \brief Generate a quorum hash of the wrapped scraper statistics.
    //!
    //! \return A hash that matches the hash of a corresponding superblock.
    //!
    QuorumHash GetHash() const
    {
        SuperblockMock mock;
        ScraperStatsSuperblockBuilder<SuperblockMock> builder(mock);

        builder.BuildFromStats(m_stats);

        return mock.m_proxy.GetHash();
    }

private:
    //!
    //! \brief Provides a compatible interface for calls to NN::Superblock that
    //! directly hashes the data passed.
    //!
    struct SuperblockMock
    {
        //!
        //! \brief Provides a compatible interface for calls to NN::Superblock
        //! containers that directly hashes the data passed.
        //!
        struct HasherProxy
        {
            CHashWriter m_hasher;            //!< Hashes the supplied data.
            uint32_t m_zero_magnitude_count; //!< Tracks zero-magnitude CPIDs.
            bool m_zero_magnitude_hashed;    //!< Tracks when to hash the zeros.

            //!
            //! \brief Initialize a proxy object that hashes supplied superblock
            //! data to produce a quorum hash.
            //!
            HasherProxy()
                : m_hasher(CHashWriter(SER_GETHASH, PROTOCOL_VERSION))
                , m_zero_magnitude_count(0)
                , m_zero_magnitude_hashed(false)
            {
            }

            //!
            //! \brief Hash a CPID/magnitude pair as it would exist in the
            //! Superblock::CpidIndex container.
            //!
            //! \param cpid      The CPID value to hash.
            //! \param magnitude The magnitude value to hash.
            //!
            void Add(Cpid cpid, uint16_t magnitude)
            {
                if (magnitude > 0) {
                    m_hasher << cpid;
                    WriteCompactSize(m_hasher, magnitude);
                } else {
                    m_zero_magnitude_count++;
                }
            }

            //!
            //! \brief Round and hash a CPID/magnitude pair as it would exist
            //! in the Superblock::CpidIndex container.
            //!
            //! \param cpid      The CPID value to hash.
            //! \param magnitude The magnitude value to hash.
            //!
            void RoundAndAdd(Cpid cpid, double magnitude)
            {
                Add(cpid, std::nearbyint(magnitude));
            }

            //!
            //! \brief Hash a project statistics entry as it would exist in the
            //! Superblock::ProjectIndex container.
            //!
            //! \param name  Name of the project to hash.
            //! \param stats Project statistics object to hash.
            //!
            void Add(const std::string& name, Superblock::ProjectStats stats)
            {
                // After ScraperStatsSuperblockBuilder adds every CPID/magnitude
                // pair to a superblock, it then starts to add projects. We need
                // to serialize the zero-magnitude CPID count before we hash the
                // first project:
                //
                if (!m_zero_magnitude_hashed) {
                    m_hasher << VARINT(m_zero_magnitude_count);
                    m_zero_magnitude_hashed = true;
                }

                m_hasher << name << stats;
            }

            //!
            //! \brief Get the final hash of the provided superblock data.
            //!
            //! \return Quorum hash of the data supplied to the proxy.
            //!
            QuorumHash GetHash()
            {
                return QuorumHash(m_hasher.GetHash());
            }
        };

        HasherProxy m_proxy;     //!< Hashes data passed passed to its methods.
        HasherProxy& m_cpids;    //!< Proxies calls for Superblock::CpidIndex.
        HasherProxy& m_projects; //!< Proxies calls for Superblock::ProjectIndex.

        //!
        //! \brief Initialize a mock superblock object.
        //!
        SuperblockMock() : m_cpids(m_proxy), m_projects(m_proxy) { }
    };

    const ScraperStats& m_stats; //!< The stats to hash like a Superblock.
};

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
    SuperblockValidator(const Superblock& superblock, size_t hint_bits = 32)
        : m_superblock(superblock)
        , m_quorum_hash(superblock.GetHash())
        , m_hint_shift(32 + std::max<size_t>(0, std::min<size_t>(32, 32 - hint_bits)))
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
        if (!local_contract.WellFormed()) {
            return Result::UNKNOWN;
        }

        if (m_superblock.Age() < SCRAPER_CMANIFEST_RETENTION_TIME) {
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

        if (!m_superblock.ConvergedByProject() && TryByManifest()) {
            return Result::VALID_BY_MANIFEST;
        }

        LogPrintf("ValidateSuperblock(): No match by manifest.");

        if (m_superblock.ConvergedByProject() && TryProjectFallback()) {
            return Result::VALID_BY_PROJECT;
        }

        LogPrintf("ValidateSuperblock(): No match by project.");

        return Result::INVALID;
    }

private: // SuperblockValidator classes

    //!
    //! \brief Maps candidate project part hashes to a set of scrapers.
    //!
    //! Each set must hold at least the mininum number of scraper IDs for a
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
        //! Each set must hold at least the mininum number of scraper IDs for a
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
        void AddPart(std::string project_name, CSerializeData project_part_data)
        {
            m_convergence.ConvergedManifestPartsMap.emplace(
                std::move(project_name),
                std::move(project_part_data));
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
            const ScraperStats stats = GetScraperStatsByConvergedManifest(m_convergence);

            return QuorumHash::Hash(stats);
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
        //! \brief Initialze a project combiner that produces no results.
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

                if (fDebug) {
                    LogPrintf(
                        "ValidateSuperblock(): uncached by project: "
                        "%" PRIszu "/%" PRIszu " = part_index = %" PRIszu " index max = %" PRIszu,
                        remainder,
                        project.m_combiner_mask,
                        part_index,
                        project.m_resolved_parts.size() - 1);
                }

                const auto& resolved_part = project.m_resolved_parts[part_index];

                convergence.AddPart(
                    project_pair.first, // project name
                    GetResolvedPartData(resolved_part.m_part_hash));

                remainder -= part_index * project.m_combiner_mask;

                // Find the most recent manifest that provided one of the parts
                // to fetch the beacon list from:
                //
                if (resolved_part.m_source_manifest_time > latest_manifest_time) {
                    latest_manifest = resolved_part.m_source_manifest_hash;
                    latest_manifest_time = resolved_part.m_source_manifest_time;
                }
            }

            convergence.AddPart("BeaconList", GetBeaconPartData(latest_manifest));

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
        static CSerializeData GetResolvedPartData(const uint256& part_hash)
        {
            LOCK(CSplitBlob::cs_mapParts);

            const auto iter = CSplitBlob::mapParts.find(part_hash);

            // If the resolved part disappeared, we cannot proceed, but
            // the most recent project part should always exist:
            if (iter == CSplitBlob::mapParts.end()) {
                LogPrintf("ValidateSuperblock(): project part disappeared.");
                return CSerializeData();
            }

            return iter->second.data;
        }

        //!
        //! \brief Fetch the beacon list part data from the specified manifest.
        //!
        //! \param manifest_hash Identifies the manifest to fetch the part from.
        //!
        //! \return Serialized binary data of the beacon list part to add to a
        //! convergence.
        //!
        static CSerializeData GetBeaconPartData(const uint256& manifest_hash)
        {
            LOCK(CScraperManifest::cs_mapManifest);

            const auto iter = CScraperManifest::mapManifest.find(manifest_hash);

            // If the manifest for the beacon list disappeared, we cannot
            // proceed, but the most recent manifest should always exist:
            if (iter == CScraperManifest::mapManifest.end()) {
                LogPrintf("ValidateSuperblock(): beacon list manifest disappeared.");
                return CSerializeData();
            }

            // If the manifest for the beacon list is now empty, we cannot
            // proceed, but ProjectResolver should always select manifests
            // with a beacon list part:
            if (iter->second->vParts.empty()) {
                LogPrintf("ValidateSuperblock(): beacon list part missing.");
                return CSerializeData();
            }

            return iter->second->vParts[0]->data;
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
        //! \return A reference to the project resolution state if the project
        //! exists in the superblock.
        //!
        boost::optional<ResolvedProject&>
        TallyProject(const std::string& project, const ScraperID& scraper_id)
        {
            if (m_resolved_projects.count(project)) {
                return m_resolved_projects.at(project);
            }

            m_other_projects[project].emplace(scraper_id);

            return boost::none;
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

            const CScraperManifest& manifest = *iter->second;

            for (const auto& entry : manifest.projects) {
                auto project_option = TallyProject(entry.project, scraper_id);

                // If this project does not exist in the superblock, skip the
                // attempt to associate its parts:
                //
                if (!project_option
                    || entry.part1 < 1
                    || entry.part1 >= (int)manifest.vParts.size())
                {
                    continue;
                }

                const uint256& part_hash = manifest.vParts[entry.part1]->hash;

                if (project_option->Expects(part_hash)
                    && project_option->Tally(part_hash, scraper_id, m_supermajority))
                {
                    project_option->LinkPart(ResolvedPart(
                        manifest.vParts[entry.part1]->hash,
                        manifest_hash,
                        entry.LastModified));
                }
            }
        }
    }; // ProjectResolver

private: // SuperblockValidator fields

    const Superblock& m_superblock; //!< Points to the superblock to validate.
    const QuorumHash m_quorum_hash; //!< Hash of the superblock to validate.
    const size_t m_hint_shift;      //!< For testing by-project combinations.

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

        const uint32_t hint = m_superblock.m_convergence_hint;
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

                if (content_hash.GetUint64() >> 32 == m_superblock.m_manifest_content_hint) {
                    if (!content_hash_tally.emplace(content_hash, 1).second) {
                        content_hash_tally[content_hash]++;
                    }

                    if (fDebug) {
                        LogPrintf(
                            "ValidateSuperblock(): manifest content hash %s "
                            "matched convergence hint. Matches %" PRIszu,
                            content_hash.ToString(),
                            content_hash_tally[content_hash]);
                    }

                    if (content_hash_tally[content_hash] >= supermajority) {
                        if (fDebug) {
                            LogPrintf(
                                "ValidateSuperblock(): supermajority found for "
                                "manifest content hash: %s. Trying validation.",
                                content_hash.ToString());
                        }

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
        CScraperManifest manifest;

        {
            LOCK(CScraperManifest::cs_mapManifest);

            const auto iter = CScraperManifest::mapManifest.find(manifest_hash);

            if (iter == CScraperManifest::mapManifest.end()) {
                LogPrintf("ValidateSuperblock(): manifest not found");
                return false;
            }

            // This is a copy on purpose to minimize lock time.
            manifest = *iter->second;
        }

        return TryManifest(manifest);
    }

    //!
    //! \brief Validate the superblock by comparing the provided manifest.
    //!
    //! \return \c true if the provided manifest builds a superblock that
    //! matches the validated superblock.
    //!
    bool TryManifest(CScraperManifest& manifest) const
    {
        const ScraperStats stats = GetScraperStatsFromSingleManifest(manifest);

        return QuorumHash::Hash(stats) == m_quorum_hash;
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

        if (fDebug) {
            LogPrintf(
                "ValidateSuperblock(): by-project possible combinations: %" PRIszu,
                combiner.TotalCombinations());
        }

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

        for (const auto& project_pair : m_superblock.m_projects) {
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
//! \brief Parses and unpacks superblock data from legacy superblock contracts.
//!
//! Legacy superblock string contracts contain the following XML-like elements
//! with records delimited by semicolons and fields delimited by commas:
//!
//!   <MAGNITUDES>EXTERNAL_CPID,MAGNITUDE;...</MAGNITUDES>
//!   <AVERAGES>PROJECT_NAME,AVERAGE_RAC,RAC;...</AVERAGES>
//!   <QUOTES>CURRENCY_CODE,PRICE;...</QUOTES>
//!
//! The application no longer supports the vestigial price quotes so these are
//! not parsed by this class.
//!
//! Newer, binary-packed legacy contracts replace <MAGNITUDES>...</MAGNITUDES>
//! elements with these sections to conserve space:
//!
//!   <ZERO>COUNT</ZERO>
//!   <BINARY>PACKED_DATA...</BINARY>
//!
//! ...where PACKED_DATA are the binary representations of alternating 16-byte
//! external CPID values and 2-byte (16-bit) magnitudes. The COUNT placeholder
//! tallies the number of zero-magnitude CPIDs omitted from the contract.
//!
//! Binary-packed superblocks were introduced on mainnet with block 725000 and
//! on testnet at block 10000.
//!
class LegacySuperblockParser
{
public:
    std::string m_magnitudes;        //!< Mapping of CPIDs to magnitudes.
    std::string m_binary_magnitudes; //!< Packed mapping of CPIDs to magnitudes.
    std::string m_averages;          //!< Project RACs and average RACs.
    int32_t m_zero_mags;             //!< Count of CPIDs with zero magnitude.

    //!
    //! \brief Initialize a parser from a legacy superblock contract.
    //!
    //! \param packed Legacy superblock contract to extract data from. The CPID
    //! magnitude records may exist as text or in a packed binary format.
    //!
    LegacySuperblockParser(const std::string& packed)
        : m_magnitudes(ExtractXML(packed, "<MAGNITUDES>", "</MAGNITUDES>"))
        , m_binary_magnitudes(ExtractXML(packed, "<BINARY>", "</BINARY>"))
        , m_averages(ExtractXML(packed, "<AVERAGES>", "</AVERAGES>"))
        , m_zero_mags(0)
    {
        try {
            m_zero_mags = std::stoi(ExtractXML(packed, "<ZERO>", "</ZERO>"));
        } catch (...) {
            LogPrintf("LegacySuperblock: Failed to parse zero mag CPIDs");
        }
    }

    //!
    //! \brief Parse project recent average credit and average RAC from the
    //! legacy superblock data.
    //!
    //! \return Superblock project statistics to set on a superblock instance.
    //!
    Superblock::ProjectIndex ExtractProjects() const
    {
        Superblock::ProjectIndex projects;
        size_t start = 0;
        size_t end = m_averages.find(";");

        while (end != std::string::npos) {
            std::string project_record = m_averages.substr(start, end - start);
            std::vector<std::string> parts = split(project_record, ",");

            start = end + 1;
            end = m_averages.find(";", start);

            if (parts.size() < 2
                || parts[0].empty()
                || parts[0] == "NeuralNetwork") // Ignore network stats
            {
                continue;
            }

            try {
                projects.Add(std::move(parts[0]), Superblock::ProjectStats(
                    std::stoi(parts[1]),                          // average RAC
                    parts.size() > 2 ? std::stoi(parts[2]) : 0)); // RAC

            } catch (...) {
                LogPrintf("ExtractProjects(): Failed to parse project RAC.");
            }
        }

        return projects;
    }

    //!
    //! \brief Parse CPID magnitudes from the legacy superblock data.
    //!
    //! \return Superblock CPID map to set on a superblock instance.
    //!
    Superblock::CpidIndex ExtractMagnitudes() const
    {
        if (!m_binary_magnitudes.empty()) {
            return ExtractBinaryMagnitudes();
        }

        return ExtractTextMagnitudes();
    }

private:
    //!
    //! \brief Unpack CPID magnitudes stored in binary format.
    //!
    //! \return Superblock CPID map to set on a superblock instance.
    //!
    Superblock::CpidIndex ExtractBinaryMagnitudes() const
    {
        Superblock::CpidIndex magnitudes(m_zero_mags);

        const char* const byte_ptr = m_binary_magnitudes.data();
        const size_t binary_size = m_binary_magnitudes.size();

        for (size_t x = 0; x < binary_size && binary_size - x >= 18; x += 18) {
            magnitudes.Add(
                *reinterpret_cast<const Cpid*>(byte_ptr + x),
                be16toh(*reinterpret_cast<const int16_t*>(byte_ptr + x + 16)));
        }

        return magnitudes;
    }

    //!
    //! \brief Parse CPID magnitudes stored as text.
    //!
    //! \return Superblock CPID map to set on a superblock instance.
    //!
    Superblock::CpidIndex ExtractTextMagnitudes() const
    {
        Superblock::CpidIndex magnitudes(m_zero_mags);

        size_t start = 0;
        size_t end = m_magnitudes.find(";");

        while (end != std::string::npos) {
            std::string cpid_record = m_magnitudes.substr(start, end - start);
            std::vector<std::string> parts = split(cpid_record, ",");

            start = end + 1;
            end = m_magnitudes.find(";", start);

            if (parts.size() != 2 || parts[0].empty()) {
                continue;
            }

            try {
                magnitudes.Add(MiningId::Parse(parts[0]), std::stoi(parts[1]));
            } catch(...) {
                LogPrintf("ExtractTextMagnitude(): Failed to parse magnitude.");
            }
        }

        return magnitudes;
    }
}; // LegacySuperblockParser

//!
//! \brief Gets the string representation of a quorum hash object.
//!
struct QuorumHashToStringVisitor : boost::static_visitor<std::string>
{
    //!
    //! \brief Get the string representation of an invalid or empty quorum hash.
    //!
    //! \param invalid The object to create a string for.
    //!
    //! \return An empty string.
    //!
    std::string operator()(const QuorumHash::Invalid invalid) const
    {
        return std::string();
    }

    //!
    //! \brief Get the string representation of a SHA256 quorum hash.
    //!
    //! \param hash The object to create a string for.
    //!
    //! \return 64-character hex-encoded representation of bytes in the hash.
    //!
    std::string operator()(const uint256& hash) const
    {
        return hash.ToString();
    }

    //!
    //! \brief Get the string representation of a legacy MD5 quorum hash.
    //!
    //! \param legacy_hash The object to create a string for.
    //!
    //! \return 32-character hex-encoded representation of bytes in the hash.
    //!
    std::string operator()(const QuorumHash::Md5Sum& legacy_hash) const
    {
        return HexStr(legacy_hash.begin(), legacy_hash.end());
    }
};
} // anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

bool NN::ValidateSuperblock(
    const Superblock& superblock,
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

// -----------------------------------------------------------------------------
// Class: Superblock
// -----------------------------------------------------------------------------

Superblock::Superblock()
    : m_version(Superblock::CURRENT_VERSION)
    , m_convergence_hint(0)
    , m_manifest_content_hint(0)
    , m_height(0)
    , m_timestamp(0)
{
}

Superblock::Superblock(uint32_t version)
    : m_version(version)
    , m_convergence_hint(0)
    , m_manifest_content_hint(0)
    , m_height(0)
    , m_timestamp(0)
{
}

Superblock Superblock::FromConvergence(
    const ConvergedScraperStats& stats,
    const uint32_t version)
{
    Superblock superblock = Superblock::FromStats(stats.mScraperConvergedStats, version);

    superblock.m_convergence_hint = stats.Convergence.nContentHash.GetUint64() >> 32;

    if (!stats.Convergence.bByParts) {
        superblock.m_manifest_content_hint
            = stats.Convergence.nUnderlyingManifestContentHash.GetUint64() >> 32;

        return superblock;
    }

    ProjectIndex& projects = superblock.m_projects;

    // Add hints created from the hashes of converged manifest parts to each
    // superblock project section to assist receiving nodes with validation:
    //
    for (const auto& part_pair : stats.Convergence.ConvergedManifestPartsMap) {
        const std::string& project_name = part_pair.first;
        const CSerializeData& part_data = part_pair.second;

        projects.SetHint(project_name, part_data);
    }

    return superblock;
}

Superblock Superblock::FromStats(const ScraperStats& stats, const uint32_t version)
{
    Superblock superblock(version);
    ScraperStatsSuperblockBuilder<Superblock> builder(superblock);

    if (version == 1) {
        // Force the CPID index into legacy mode to capture zero-magnitude
        // CPIDs that result from rounding.
        //
        // TODO: encapsulate this
        //
        superblock.m_cpids = Superblock::CpidIndex(0);
    }

    builder.BuildFromStats(stats);

    return superblock;
}

Superblock Superblock::UnpackLegacy(const std::string& packed)
{
    if (packed.empty()) {
        return Superblock(1);
    }

    // Legacy-packed superblocks always initialize to version 1:
    Superblock superblock(1);
    LegacySuperblockParser legacy(packed);

    superblock.m_cpids = legacy.ExtractMagnitudes();
    superblock.m_projects = legacy.ExtractProjects();

    return superblock;
}

std::string Superblock::PackLegacy() const
{
    std::stringstream out;

    out << "<ZERO>" << m_cpids.Zeros() << "</ZERO>"
        << "<BINARY>";

    for (const auto& cpid_pair : m_cpids) {
        uint16_t mag = htobe16(cpid_pair.second);

        out.write(reinterpret_cast<const char*>(cpid_pair.first.Raw().data()), 16);
        out.write(reinterpret_cast<const char*>(&mag), sizeof(uint16_t));
    }

    out << "</BINARY>"
        << "<AVERAGES>";

    for (const auto& project_pair : m_projects) {
        out << project_pair.first << ","
            << project_pair.second.m_average_rac << ","
            << project_pair.second.m_rac << ";";
    }

    out << "</AVERAGES>"
        << "<QUOTES></QUOTES>";

    return out.str();
}

bool Superblock::WellFormed() const
{
    return m_version > 0 && m_version <= Superblock::CURRENT_VERSION
        && !m_cpids.empty()
        && !m_projects.empty();
}

bool Superblock::ConvergedByProject() const
{
    return m_projects.m_converged_by_project;
}

int64_t Superblock::Age() const
{
    return GetAdjustedTime() - m_timestamp;
}

QuorumHash Superblock::GetHash(const bool regenerate) const
{
    if (!m_hash_cache.Valid() || regenerate) {
        m_hash_cache = QuorumHash::Hash(*this);
    }

    return m_hash_cache;
}

// -----------------------------------------------------------------------------
// Class: Superblock::CpidIndex
// -----------------------------------------------------------------------------

Superblock::CpidIndex::CpidIndex()
    : m_zero_magnitude_count(0)
    , m_total_magnitude(0)
    , m_legacy(false)
{
}

Superblock::CpidIndex::CpidIndex(uint32_t zero_magnitude_count)
    : m_zero_magnitude_count(zero_magnitude_count)
    , m_total_magnitude(0)
    , m_legacy(true)
{
}

Superblock::CpidIndex::const_iterator Superblock::CpidIndex::begin() const
{
    return m_magnitudes.begin();
}

Superblock::CpidIndex::const_iterator Superblock::CpidIndex::end() const
{
    return m_magnitudes.end();
}

Superblock::CpidIndex::size_type Superblock::CpidIndex::size() const
{
    return m_magnitudes.size();
}

bool Superblock::CpidIndex::empty() const
{
    return m_magnitudes.empty();
}

uint32_t Superblock::CpidIndex::Zeros() const
{
    return m_zero_magnitude_count;
}

size_t Superblock::CpidIndex::TotalCount() const
{
    return m_magnitudes.size() + m_zero_magnitude_count;
}

uint64_t Superblock::CpidIndex::TotalMagnitude() const
{
    return m_total_magnitude;
}

double Superblock::CpidIndex::AverageMagnitude() const
{
    if (m_magnitudes.empty()) {
        return 0;
    }

    return static_cast<double>(m_total_magnitude) / m_magnitudes.size();
}

uint16_t Superblock::CpidIndex::MagnitudeOf(const Cpid& cpid) const
{
    const auto iter = m_magnitudes.find(cpid);

    if (iter == m_magnitudes.end()) {
        return 0;
    }

    return iter->second;
}

size_t Superblock::CpidIndex::OffsetOf(const Cpid& cpid) const
{
    const auto iter = m_magnitudes.find(cpid);

    if (iter == m_magnitudes.end()) {
        return m_magnitudes.size();
    }

    // Not very efficient--we can optimize this if needed:
    return std::distance(m_magnitudes.begin(), iter);
}

Superblock::CpidIndex::const_iterator
Superblock::CpidIndex::At(const size_t offset) const
{
    // Not very efficient--we can optimize this if needed:
    return std::next(m_magnitudes.begin(), offset);
}

void Superblock::CpidIndex::Add(const Cpid cpid, const uint16_t magnitude)
{
    if (magnitude > 0 || m_legacy) {
        // Only increment the total magnitude if the CPID does not already
        // exist in the index:
        if (m_magnitudes.emplace(cpid, magnitude).second == true) {
            m_total_magnitude += magnitude;
        }
    } else {
        m_zero_magnitude_count++;
    }
}

void Superblock::CpidIndex::Add(const MiningId id, const uint16_t magnitude)
{
    if (const CpidOption cpid = id.TryCpid()) {
        Add(*cpid, magnitude);
    } else if (!m_legacy) {
        m_zero_magnitude_count++;
    }
}

void Superblock::CpidIndex::RoundAndAdd(const MiningId id, const double magnitude)
{
    // The ScraperGetNeuralContract() function that these classes replace
    // rounded magnitude values using a half-away-from-zero rounding mode
    // to determine whether floating-point magnitudes round-down to zero,
    // but it added the magnitude values to the superblock with half-even
    // rounding. This caused legacy superblock contracts to contain CPIDs
    // with zero magnitude when the rounding results differed.
    //
    // To create legacy superblocks from scraper statistics with matching
    // hashes, we filter magnitudes using the same rounding rules:
    //
    if (!m_legacy || std::round(magnitude) > 0) {
        Add(id, std::nearbyint(magnitude));
    } else {
        m_zero_magnitude_count++;
    }
}

// -----------------------------------------------------------------------------
// Class: Superblock::ProjectStats
// -----------------------------------------------------------------------------

Superblock::ProjectStats::ProjectStats()
    : m_total_credit(0)
    , m_average_rac(0)
    , m_rac(0)
    , m_convergence_hint(0)
{
}

Superblock::ProjectStats::ProjectStats(
    uint64_t total_credit,
    uint64_t average_rac,
    uint64_t rac)
    : m_total_credit(total_credit)
    , m_average_rac(average_rac)
    , m_rac(rac)
    , m_convergence_hint(0)
{
}

Superblock::ProjectStats::ProjectStats(uint64_t average_rac, uint64_t rac)
    : m_total_credit(0)
    , m_average_rac(average_rac)
    , m_rac(rac)
    , m_convergence_hint(0)
{
}

// -----------------------------------------------------------------------------
// Class: Superblock::ProjectIndex
// -----------------------------------------------------------------------------

Superblock::ProjectIndex::ProjectIndex()
    : m_converged_by_project(false)
    , m_total_rac(0)
{
}

Superblock::ProjectIndex::const_iterator Superblock::ProjectIndex::begin() const
{
    return m_projects.begin();
}

Superblock::ProjectIndex::const_iterator Superblock::ProjectIndex::end() const
{
    return m_projects.end();
}

Superblock::ProjectIndex::size_type Superblock::ProjectIndex::size() const
{
    return m_projects.size();
}

bool Superblock::ProjectIndex::empty() const
{
    return m_projects.empty();
}

uint64_t Superblock::ProjectIndex::TotalRac() const
{
    return m_total_rac;
}

double Superblock::ProjectIndex::AverageRac() const
{
    if (m_projects.empty()) {
        return 0;
    }

    return static_cast<double>(m_total_rac) / m_projects.size();
}

Superblock::ProjectStatsOption
Superblock::ProjectIndex::Try(const std::string& name) const
{
    const auto iter = m_projects.find(name);

    if (iter == m_projects.end()) {
        return boost::none;
    }

    return iter->second;
}

void Superblock::ProjectIndex::Add(std::string name, const ProjectStats& stats)
{
    if (name.empty()) {
        return;
    }

    // Only increment the total RAC if the project does not already exist in
    // the index:
    if (m_projects.emplace(std::move(name), stats).second == true) {
        m_total_rac += stats.m_rac;
    }
}

void Superblock::ProjectIndex::SetHint(
    const std::string& name,
    const CSerializeData& part_data)
{
    auto iter = m_projects.find(name);

    if (iter == m_projects.end()) {
        return;
    }

    const uint256 part_hash = Hash(part_data.begin(), part_data.end());
    iter->second.m_convergence_hint = part_hash.GetUint64() >> 32;

    m_converged_by_project = true;
}

// -----------------------------------------------------------------------------
// Class: QuorumHash
// -----------------------------------------------------------------------------

static_assert(sizeof(uint256) == 32, "Unexpected uint256 size.");
static_assert(sizeof(QuorumHash::Md5Sum) == 16, "Unexpected MD5 size.");

QuorumHash::QuorumHash() : m_hash(Invalid())
{
}

QuorumHash::QuorumHash(uint256 hash) : m_hash(hash)
{
}

QuorumHash::QuorumHash(Md5Sum legacy_hash) : m_hash(legacy_hash)
{
}

QuorumHash::QuorumHash(const std::vector<unsigned char>& bytes) : QuorumHash()
{
    if (bytes.size() == sizeof(uint256)) {
        m_hash = uint256(bytes);
    } else if (bytes.size() == sizeof(Md5Sum)) {
        m_hash = Md5Sum();
        std::copy(bytes.begin(), bytes.end(), boost::get<Md5Sum>(m_hash).begin());
    }
}

QuorumHash QuorumHash::Hash(const Superblock& superblock)
{
    if (superblock.m_version > 1) {
        return QuorumHash(SerializeHash(superblock));
    }

    std::string input;
    input.reserve(superblock.m_cpids.size() * (32 + 1 + 5 + 5));

    for (const auto& cpid_pair : superblock.m_cpids) {
        double dMagLength = RoundToString(cpid_pair.second, 0).length();
        double dExponent = pow(dMagLength, 5);

        input += cpid_pair.first.ToString();
        input += RoundToString(cpid_pair.second / (dExponent + .01), 0);
        input += RoundToString(dMagLength * dExponent, 0);
        input += "<COL>";
    }

    Md5Sum output;
    MD5((const unsigned char*)input.data(), input.size(), output.data());

    return QuorumHash(output);
}

QuorumHash QuorumHash::Hash(const ScraperStats& stats)
{
    ScraperStatsQuorumHasher hasher(stats);

    return hasher.GetHash();
}

QuorumHash QuorumHash::Parse(const std::string& hex)
{
    if (hex.size() == sizeof(uint256) * 2) {
        // A uint256 object stores bytes in the reverse order of its string
        // representation. We could parse the string through the uint256S()
        // function, but this doesn't provide a mechanism to detect invalid
        // strings.
        //
        std::vector<unsigned char> bytes = ParseHex(hex);
        std::reverse(bytes.begin(), bytes.end());

        return QuorumHash(bytes);
    }

    if (hex.size() == sizeof(Md5Sum) * 2) {
        // This is the hash of an empty legacy superblock contract. A bug in
        // previous versions caused nodes to vote for empty superblocks when
        // staking a block. We can ignore any quorum hashes with this value:
        //
        if (hex != "d41d8cd98f00b204e9800998ecf8427e") {
            return QuorumHash(ParseHex(hex));
        }
    }

    return QuorumHash();
}

bool QuorumHash::operator==(const QuorumHash& other) const
{
    if (m_hash.which() != other.m_hash.which()) {
        return false;
    }

    switch (Which()) {
        case Kind::INVALID:
            return true;

        case Kind::SHA256:
            return boost::get<uint256>(m_hash)
                == boost::get<uint256>(other.m_hash);

        case Kind::MD5:
            return boost::get<Md5Sum>(m_hash)
                == boost::get<Md5Sum>(other.m_hash);
    }

    return false;
}

bool QuorumHash::operator!=(const QuorumHash& other) const
{
    return !(*this == other);
}

bool QuorumHash::operator==(const uint256& other) const
{
    return Which() == Kind::SHA256
        && boost::get<uint256>(m_hash) == other;
}

bool QuorumHash::operator!=(const uint256& other) const
{
    return !(*this == other);
}

bool QuorumHash::operator==(const std::string& other) const
{
    switch (Which()) {
        case Kind::INVALID:
            return other.empty();

        case Kind::SHA256:
            return other.size() == sizeof(uint256) * 2
                && boost::get<uint256>(m_hash) == uint256S(other);

        case Kind::MD5:
            return other.size() == sizeof(Md5Sum) * 2
                && std::equal(
                    boost::get<Md5Sum>(m_hash).begin(),
                    boost::get<Md5Sum>(m_hash).end(),
                    ParseHex(other).begin());
    }

    return false;
}

bool QuorumHash::operator!=(const std::string&other) const
{
    return !(*this == other);
}

QuorumHash::Kind QuorumHash::Which() const
{
    return static_cast<Kind>(m_hash.which());
}

bool QuorumHash::Valid() const
{
    return Which() != Kind::INVALID;
}

const unsigned char* QuorumHash::Raw() const
{
    switch (Which()) {
        case Kind::INVALID:
            return nullptr;
        case Kind::SHA256:
            return boost::get<uint256>(m_hash).begin();
        case Kind::MD5:
            return boost::get<Md5Sum>(m_hash).data();
    }

    return nullptr;
}

std::string QuorumHash::ToString() const
{
    return boost::apply_visitor(QuorumHashToStringVisitor(), m_hash);
}
