#include "net.h" // TODO: needed for scraper_net.h
#include <univalue.h> // TODO: needed for scraper_net.h

#include "compat/endian.h"
#include "neuralnet/superblock.h"
#include "scraper_net.h"
#include "util.h"

#include <boost/variant/apply_visitor.hpp>
#include <openssl/md5.h>

using namespace NN;

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);
std::string ExtractValue(std::string data, std::string delimiter, int pos);

// TODO: use a header
ScraperStats GetScraperStatsByConvergedManifest(ConvergedManifest& StructConvergedManifest);
ScraperStats GetScraperStatsFromSingleManifest(CScraperManifest &manifest);
unsigned int NumScrapersForSupermajority(unsigned int nScraperCount);
mmCSManifestsBinnedByScraper ScraperDeleteCScraperManifests();
Superblock ScraperGetSuperblockContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false);

extern CCriticalSection cs_ConvergedScraperStatsCache;
extern ConvergedScraperStats ConvergedScraperStatsCache;

namespace {
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
    SuperblockValidator(const Superblock& superblock)
        : m_superblock(superblock)
        , m_quorum_hash(superblock.GetHash())
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
        std::set<uint256> m_candidate_hashes;

        //!
        //! \brief The set of scrapers that produced the matching manifest part
        //! for the project.
        //!
        //! This set must hold at least the mininum number of scraper IDs for a
        //! supermajority for each project or the superblock validation fails.
        //!
        std::set<ScraperID> m_scrapers;

        //!
        //! \brief The manifest part hashes found in a manifest published by a
        //! scraper used to retrieve the part for the project to construct the
        //! convergence for comparison to the superblock.
        //!
        //! Keyed by timestamp.
        //!
        //! After successfully matching each of the convergence hints in the
        //! superblock to a manifest project, the \c ProjectResolver selects
        //! the most recent part of each \c ResolvedProject to construct the
        //! final convergence.
        //!
        std::map<int64_t, uint256> m_resolved_parts;

        //!
        //! \brief Initialize a new project context object.
        //!
        ResolvedProject()
        {
        }

        //!
        //! \brief Initialize a new project context object.
        //!
        //! \param candidate hashes The manifest part hashes procured from the
        //! convergence hints in the superblock.
        //!
        ResolvedProject(std::set<uint256> candidate_hashes)
            : m_candidate_hashes(std::move(candidate_hashes))
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
            return m_candidate_hashes.count(part_hash);
        }

        //!
        //! \brief Get the most recent part resolved from a manifest for this
        //! project.
        //!
        //! \return The hash of the part for this project used to construct a
        //! convergence for the validated superblock.
        //!
        uint256 MostRecentPartHash() const
        {
            return m_resolved_parts.rbegin()->second;
        }

        //!
        //! \brief Commit the part hash to this project and record the timestamp
        //! of the manifest.
        //!
        //! \param part_hash Hash of the candidate part to commit.
        //! \param time      Timestamp of the manifest that contains the part.
        //!
        void LinkPart(const uint256& part_hash, const int64_t time)
        {
            m_resolved_parts.emplace(time, part_hash);
        }
    };

    //!
    //! \brief Reconstructs by-project convergence from local manifest data
    //! based on project convergence hints from the superblock to produce a
    //! new superblock used for comparison.
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
            std::map<std::string, std::set<uint256>> candidate_parts,
            mmCSManifestsBinnedByScraper manifests_by_scraper)
            : m_manifests_by_scraper(std::move(manifests_by_scraper))
            , m_supermajority(NumScrapersForSupermajority(m_manifests_by_scraper.size()))
            , m_latest_manifest_timestamp(0)
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
        bool ResolveProjectParts()
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

                    return false;
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

                    return false;
                }

                if (project_pair.second.m_scrapers.size() < m_supermajority) {
                    LogPrintf(
                        "ValidateSuperblock(): fallback by-project resolution "
                        "failed. No supermajority exists for project %s.",
                        project_pair.first);

                    return false;
                }
            }

            return true;
        }

        //!
        //! \brief Construct a superblock from the local manifest data that
        //! matches the most recent set of resolved project parts.
        //!
        //! \return A new superblock instance to compare to the superblock
        //! under validation.
        //!
        Superblock BuildSuperblock() const
        {
            ConvergedManifest convergence;

            {
                LOCK(CScraperManifest::cs_mapManifest);

                const auto iter = CScraperManifest::mapManifest.find(m_latest_manifest_hash);

                // If the manifest for the beacon list disappeared, we cannot
                // proceed, but the most recent manifest should always exist:
                if (iter == CScraperManifest::mapManifest.end()) {
                    LogPrintf("ValidateSuperblock(): beacon list manifest disappeared.");
                    return Superblock();
                }

                convergence.ConvergedManifestPartsMap.emplace(
                    "BeaconList",
                    iter->second->vParts[0]->data);
            }

            {
                LOCK(CSplitBlob::cs_mapParts);

                for (const auto& project_pair : m_resolved_projects) {
                    const auto iter = CSplitBlob::mapParts.find(
                        project_pair.second.MostRecentPartHash());

                    // If the resolved part disappeared, we cannot proceed, but
                    // the most recent project part should always exist:
                    if (iter == CSplitBlob::mapParts.end()) {
                        LogPrintf("ValidateSuperblock(): project part disappeared.");
                        return Superblock();
                    }

                    convergence.ConvergedManifestPartsMap.emplace(
                        project_pair.first, // project name
                        iter->second.data); // serialized part data
                }
            }

            return Superblock::FromStats(GetScraperStatsByConvergedManifest(convergence));
        }

    private:
        const mmCSManifestsBinnedByScraper m_manifests_by_scraper;
        const size_t m_supermajority;
        std::map<std::string, ResolvedProject> m_resolved_projects;
        std::map<std::string, std::set<ScraperID>> m_other_projects;
        int64_t m_latest_manifest_timestamp;
        uint256 m_latest_manifest_hash;

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
                ResolvedProject& resolved = m_resolved_projects.at(project);

                resolved.m_scrapers.emplace(scraper_id);

                return resolved;
            }

            m_other_projects[project].emplace(scraper_id);

            return boost::none;
        }

        //!
        //! \brief Store the hash and timestamp of the specified manifest if
        //! the timestamp is more recent than the last seen manifest.
        //!
        //! After resolving each project part, the most recent matching manifest
        //! will provide the manifest part for the convergence beacon list.
        //!
        //! \param hash The manifest hash to store.
        //! \param time Timestamp of the
        //!
        void RecordLatestManifest(const uint256& hash, const int64_t time)
        {
            if (time > m_latest_manifest_timestamp) {
                m_latest_manifest_timestamp = time;
                m_latest_manifest_hash = hash;
            }
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
                if (!project_option) {
                    continue;
                }

                for (const auto& part : manifest.vParts) {
                    if (project_option->Expects(part->hash)) {
                        project_option->LinkPart(part->hash, entry.LastModified);
                        RecordLatestManifest(manifest_hash, entry.LastModified);
                    }
                }
            }
        }
    }; // ProjectResolver

private: // SuperblockValidator fields

    const Superblock& m_superblock; //!< Points to the superblock to validate.
    const QuorumHash m_quorum_hash; //!< Hash of the superblock to validate.

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
        const mmCSManifestsBinnedByScraper manifests_by_scraper = ScraperDeleteCScraperManifests();
        const size_t supermajority = NumScrapersForSupermajority(manifests_by_scraper.size());

        std::map<uint256, size_t> content_hash_tally;

        for (const auto& by_scraper : manifests_by_scraper) {
            const mCSManifest& hashes_by_time = by_scraper.second;

            for (const auto& hashes : hashes_by_time) {
                const uint256& manifest_hash = hashes.second.first;
                const uint256& content_hash = hashes.second.second;

                if (content_hash.Get64() >> 32 == m_superblock.m_manifest_content_hint) {
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

        return Superblock::FromStats(stats).GetHash() == m_quorum_hash;
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
        const std::map<uint32_t, std::string> hints = CollectPartHints();
        std::map<std::string, std::set<uint256>> candidates = CollectCandidateParts(hints);

        if (candidates.size() != hints.size()) {
            LogPrintf(
                "ValidateSuperblock(): fallback by-project resolution failed. "
                "Could not match every project convergence hint to a part.");

            return false;
        }

        ProjectResolver resolver(std::move(candidates), ScraperDeleteCScraperManifests());

        if (!resolver.ResolveProjectParts()) {
            return false;
        }

        return resolver.BuildSuperblock().GetHash() == m_quorum_hash;
    }

    //!
    //! \brief Build a collection of project-level convergence hints from the
    //! superblock.
    //!
    //! \return A map of manifest project part hints to project names.
    //!
    std::map<uint32_t, std::string> CollectPartHints() const
    {
        std::map<uint32_t, std::string> hints;

        for (const auto& project_pair : m_superblock.m_projects) {
            hints.emplace(
                project_pair.second.m_convergence_hint,
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
    std::map<std::string, std::set<uint256>>
    CollectCandidateParts(const std::map<uint32_t, std::string>& hints) const
    {
        struct Candidate
        {
            std::string m_project_name;
            std::set<uint256> m_part_hashes;
        };

        std::map<uint32_t, Candidate> candidates;

        {
            LOCK(CSplitBlob::cs_mapParts);

            for (const auto& part_pair : CSplitBlob::mapParts) {
                uint32_t hint = part_pair.second.hash.Get64() >> 32;

                const auto hint_iter = hints.find(hint);

                if (hint_iter != hints.end()) {
                    auto iter_pair = candidates.emplace(hint, Candidate());
                    Candidate& candidate = iter_pair.first->second;

                    // Set the project name if we just inserted a new candidate:
                    if (iter_pair.second) {
                        candidate.m_project_name = hint_iter->second;
                    }

                    candidate.m_part_hashes.emplace(part_pair.second.hash);
                }
            }
        }

        // Pivot the candidate part hashes that match the hints into a map keyed
        // by project names:
        //
        std::map<std::string, std::set<uint256>> candidates_by_project;

        for (auto&& candidate_pair : candidates) {
            candidates_by_project.emplace(
                std::move(candidate_pair.second.m_project_name),
                std::move(candidate_pair.second.m_part_hashes));
        }

        return candidates_by_project;
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

struct BinaryResearcher
{
    std::array<unsigned char, 16> cpid;
    int16_t magnitude;
};

// Ensure that the compiler does not add padding between the cpid and the
// magnitude. If it does it does it to align the data, at which point the
// pointer cast in UnpackBinarySuperblock will be illegal. In such a
// case we will have to resort to a slower unpack.
static_assert(offsetof(struct BinaryResearcher, magnitude) ==
              sizeof(struct BinaryResearcher) - sizeof(int16_t),
              "Unexpected padding in BinaryResearcher");
} // anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

bool NN::ValidateSuperblock(const Superblock& superblock, const bool use_cache)
{
    using Result = SuperblockValidator::Result;

    const Result result = SuperblockValidator(superblock).Validate(use_cache);
    std::string message;

    switch (result) {
        case Result::UNKNOWN:
            message = "UNKNOWN - Waiting for manifest data";
            break;
        case Result::INVALID:
            message = "INVALID - Validation failed";
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
// Legacy Functions
// -----------------------------------------------------------------------------

std::string UnpackBinarySuperblock(std::string sBlock)
{
    // 12-21-2015: R HALFORD: If the block is not binary, return the legacy format for backward compatibility
    std::string sBinary = ExtractXML(sBlock,"<BINARY>","</BINARY>");
    if (sBinary.empty()) return sBlock;

    std::ostringstream stream;
    stream << "<AVERAGES>" << ExtractXML(sBlock,"<AVERAGES>","</AVERAGES>") << "</AVERAGES>"
           << "<QUOTES>" << ExtractXML(sBlock,"<QUOTES>","</QUOTES>") << "</QUOTES>"
           << "<MAGNITUDES>";

    // Binary data support structure:
    // Each CPID consumes 16 bytes and 2 bytes for magnitude: (Except CPIDs with zero magnitude - the count of those is stored in XML node <ZERO> to save space)
    // 1234567890123456MM
    // MM = Magnitude stored as 2 bytes
    // No delimiter between CPIDs, Step Rate = 18.
    // CPID and magnitude are stored in big endian.
    for (unsigned int x = 0; x < sBinary.length(); x += 18)
    {
        if(sBinary.length() - x < 18)
            break;

        const BinaryResearcher* researcher = reinterpret_cast<const BinaryResearcher*>(sBinary.data() + x);
        stream << HexStr(researcher->cpid.begin(), researcher->cpid.end()) << ","
               << be16toh(researcher->magnitude) << ";";
    }

    // Append zero magnitude researchers so the beacon count matches
    int num_zero_mag = atoi(ExtractXML(sBlock,"<ZERO>","</ZERO>"));
    const std::string zero_entry("0,15;");
    for(int i=0; i<num_zero_mag; ++i)
        stream << zero_entry;

    stream << "</MAGNITUDES>";
    return stream.str();
}

std::string PackBinarySuperblock(std::string sBlock)
{
    std::string sMagnitudes = ExtractXML(sBlock,"<MAGNITUDES>","</MAGNITUDES>");

    // For each CPID in the superblock, convert data to binary
    std::stringstream stream;
    int64_t num_zero_mag = 0;
    for (auto& entry : split(sMagnitudes.c_str(), ";"))
    {
        if (entry.length() < 1)
            continue;

        const std::vector<unsigned char>& binary_cpid = ParseHex(entry);
        if(binary_cpid.size() < 16)
        {
            ++num_zero_mag;
            continue;
        }

        BinaryResearcher researcher;
        std::copy_n(binary_cpid.begin(), researcher.cpid.size(), researcher.cpid.begin());

        // Ensure we do not blow out the binary space (technically we can handle 0-65535)
        double magnitude_d = strtod(ExtractValue(entry, ",", 1).c_str(), NULL);
        // Changed to 65535 for the new NN. This will still be able to be successfully unpacked by any node.
        magnitude_d = std::max(0.0, std::min(magnitude_d, 65535.0));
        researcher.magnitude = htobe16(roundint(magnitude_d));

        stream.write((const char*) &researcher, sizeof(BinaryResearcher));
    }

    std::stringstream block_stream;
    block_stream << "<ZERO>" << num_zero_mag << "</ZERO>"
                    "<BINARY>" << stream.rdbuf() << "</BINARY>"
                    "<AVERAGES>" << ExtractXML(sBlock,"<AVERAGES>","</AVERAGES>") << "</AVERAGES>"
                    "<QUOTES>" << ExtractXML(sBlock,"<QUOTES>","</QUOTES>") << "</QUOTES>";
    return block_stream.str();
}

// -----------------------------------------------------------------------------
// Class: Superblock
// -----------------------------------------------------------------------------

Superblock::Superblock()
    : m_version(Superblock::CURRENT_VERSION)
    , m_convergence_hint(0)
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

Superblock Superblock::FromConvergence(const ConvergedScraperStats& stats)
{
    Superblock superblock = Superblock::FromStats(stats.mScraperConvergedStats);

    superblock.m_convergence_hint = stats.Convergence.nContentHash.Get64() >> 32;

    if (!stats.Convergence.bByParts) {
        superblock.m_manifest_content_hint = stats.Convergence.nUnderlyingManifestContentHash.Get64() >> 32;
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

Superblock Superblock::FromStats(const ScraperStats& stats)
{
    // The loop below depends on the relative value of these enum types:
    static_assert(statsobjecttype::NetworkWide < statsobjecttype::byCPID,
        "Unexpected enumeration order of scraper stats map object types.");
    static_assert(statsobjecttype::byCPID < statsobjecttype::byCPIDbyProject,
        "Unexpected enumeration order of scraper stats map object types.");
    static_assert(statsobjecttype::byProject < statsobjecttype::byCPIDbyProject,
        "Unexpected enumeration order of scraper stats map object types.");

    Superblock superblock;

    for (const auto& entry : stats) {
        // A CPID, project name, or project name/CPID pair that identifies the
        // current statistics record:
        const std::string& object_id = entry.first.objectID;

        switch (entry.first.objecttype) {
            case statsobjecttype::NetworkWide:
                // This map starts with a single, network-wide statistics entry.
                // Skip it because superblock objects will recalculate the stats
                // as needed after deserialization.
                //
                continue;

            case statsobjecttype::byCPID:
                superblock.m_cpids.Add(
                    Cpid::Parse(object_id),
                    std::round(entry.second.statsvalue.dMag));

                break;

            case statsobjecttype::byProject:
                superblock.m_projects.Add(object_id, ProjectStats(
                    std::round(entry.second.statsvalue.dTC),
                    std::round(entry.second.statsvalue.dAvgRAC),
                    std::round(entry.second.statsvalue.dRAC)));

                break;

            default:
                // The scraper statistics map orders the entries by "objecttype"
                // starting with "byCPID" and "byProject". After importing these
                // sections into the superblock, we can exit this loop.
                //
                return superblock;
        }
    }

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
    iter->second.m_convergence_hint = part_hash.Get64() >> 32;

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

QuorumHash QuorumHash::Parse(const std::string& hex)
{
    if (hex.size() != sizeof(uint256) * 2 && hex.size() != sizeof(Md5Sum) * 2) {
        return QuorumHash();
    }

    return QuorumHash(ParseHex(hex));
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
                && boost::get<uint256>(m_hash) == uint256(other);

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

unsigned int QuorumHash::GetSerializeSize(int nType, int nVersion) const
{
    switch (Which()) {
        case Kind::SHA256: return 1 + sizeof(uint256);
        case Kind::MD5:    return 1 + sizeof(Md5Sum);

        // For variants without any associated data, we serialize the variant
        // tag only as a single byte:
        default:           return 1;
    }
}
