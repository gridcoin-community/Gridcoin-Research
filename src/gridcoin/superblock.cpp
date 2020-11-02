// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compat/endian.h"
#include "hash.h"
#include "main.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/xml.h"
#include "sync.h"
#include "util.h"
#include "util/reverse_iterator.h"

#include <boost/variant/apply_visitor.hpp>
#include <openssl/md5.h>

using namespace GRC;

extern ScraperStatsAndVerifiedBeacons GetScraperStatsAndVerifiedBeacons(const ConvergedScraperStats& stats);

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
    void BuildFromStats(const ScraperStatsAndVerifiedBeacons& stats_and_verified_beacons)
    {
        for (const auto& entry : stats_and_verified_beacons.mScraperStats) {
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
                    m_superblock.m_cpids.Add(
                        Cpid::Parse(object_id),
                        Magnitude::RoundFrom(entry.second.statsvalue.dMag));

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
                    goto end_build_from_stats_loop;
            }
        }

        // ScraperStatsQuorumHasher expects the verified beacons data after the
        // CPID and project data, so we use a goto statement to break the above
        // loop instead or reordering the logic to use a return statement:
        //
        end_build_from_stats_loop:
        m_superblock.m_verified_beacons.Reset(stats_and_verified_beacons.mVerifiedMap);
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
    ScraperStatsQuorumHasher(const ScraperStatsAndVerifiedBeacons& stats) : m_stats(stats)
    {
    }

    //!
    //! \brief Generate a quorum hash of the provided scraper statistics.
    //!
    //! \param stats The scraper statistics to generate a hash from.
    //!
    //! \return A hash that matches the hash of a corresponding superblock.
    //!
    static QuorumHash Hash(const ScraperStatsAndVerifiedBeacons& stats)
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
    //! \brief Provides a compatible interface for calls to GRC::Superblock that
    //! directly hashes the data passed.
    //!
    struct SuperblockMock
    {
        //!
        //! \brief Provides a compatible interface for calls to GRC::Superblock
        //! containers that directly hashes the data passed.
        //!
        struct HasherProxy
        {
            CHashWriter m_hasher;            //!< Hashes the supplied data.
            CHashWriter m_small_hasher;      //!< Hashes small mag segment.
            CHashWriter m_medium_hasher;     //!< Hashes medium mag segment.
            CHashWriter m_large_hasher;      //!< Hashes large mag segment.
            uint32_t m_zero_magnitude_count; //!< Tracks zero-magnitude CPIDs.
            bool m_zero_magnitude_hashed;    //!< Tracks when to hash the zeros.

            //!
            //! \brief Initialize a proxy object that hashes supplied superblock
            //! data to produce a quorum hash.
            //!
            HasherProxy()
                : m_hasher(CHashWriter(SER_GETHASH, PROTOCOL_VERSION))
                , m_small_hasher(CHashWriter(SER_GETHASH, PROTOCOL_VERSION))
                , m_medium_hasher(CHashWriter(SER_GETHASH, PROTOCOL_VERSION))
                , m_large_hasher(CHashWriter(SER_GETHASH, PROTOCOL_VERSION))
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
            void Add(const Cpid cpid, const Magnitude magnitude)
            {
                switch (magnitude.Which()) {
                    case Magnitude::Kind::ZERO:
                        m_zero_magnitude_count++;
                        break;

                    case Magnitude::Kind::SMALL:
                        m_small_hasher
                            << cpid
                            << static_cast<uint8_t>(magnitude.Compact());
                        break;

                    case Magnitude::Kind::MEDIUM:
                        m_medium_hasher
                            << cpid
                            << static_cast<uint8_t>(magnitude.Compact());
                        break;

                    case Magnitude::Kind::LARGE:
                        m_large_hasher << cpid;
                        WriteCompactSize(m_large_hasher, magnitude.Compact());
                        break;
                }
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
                    m_hasher
                        << (CHashWriter(SER_GETHASH, PROTOCOL_VERSION)
                            << m_small_hasher.GetHash()
                            << m_medium_hasher.GetHash()
                            << m_large_hasher.GetHash())
                            .GetHash()
                        << VARINT(m_zero_magnitude_count);

                    m_zero_magnitude_hashed = true;
                }

                m_hasher << name << stats;
            }

            //!
            //! \brief Hash the verified beacons vector as it would exist in the
            //! superblock.
            //!
            //! \param verified_beacon_id_map Contains beacon IDs verified by
            //! scraper convergence. Keyed by the RIPEMD-160 hashes of beacon
            //! public keys.
            //!
            void Reset(const ScraperPendingBeaconMap& verified_beacon_id_map)
            {
                std::vector<uint160> key_ids;
                key_ids.reserve(verified_beacon_id_map.size());

                for (const auto& entry_pair : verified_beacon_id_map) {
                    key_ids.emplace_back(entry_pair.second.key_id);
                }

                // Base58 encoding on the ScraperPendingBeaconMap key results
                // in different ordering of entries from that of the key IDs:
                //
                std::sort(key_ids.begin(), key_ids.end());

                m_hasher << key_ids;
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

        HasherProxy m_proxy;             //!< Hashes data passed passed to its methods.
        HasherProxy& m_cpids;            //!< Proxies calls for Superblock::CpidIndex.
        HasherProxy& m_projects;         //!< Proxies calls for Superblock::ProjectIndex.
        HasherProxy& m_verified_beacons; //!< Proxies calls for Superblock.m_verified_beacons.

        //!
        //! \brief Initialize a mock superblock object.
        //!
        SuperblockMock() : m_cpids(m_proxy), m_projects(m_proxy), m_verified_beacons(m_proxy) { }
    };

    const ScraperStatsAndVerifiedBeacons& m_stats; //!< The stats to hash like a Superblock.
};

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
            LogPrint(BCLog::LogFlags::SB,
                "LegacySuperblock: Failed to parse zero mag CPIDs.\n");
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
                LogPrint(BCLog::LogFlags::SB,
                    "LegacySuperblock: Failed to parse project RAC.\n");
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
            magnitudes.AddLegacy(
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

            if (const CpidOption cpid = MiningId::Parse(parts[0]).TryCpid()) {
                try {
                    magnitudes.AddLegacy(*cpid, std::stoi(parts[1]));
                } catch(...) {
                    LogPrint(BCLog::LogFlags::SB,
                        "LegacySuperblock: Failed to parse magnitude.\n");
                }
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
// Class: Superblock
// -----------------------------------------------------------------------------

Superblock::Superblock()
    : m_version(Superblock::CURRENT_VERSION)
    , m_convergence_hint(0)
    , m_manifest_content_hint(0)
{
}

Superblock::Superblock(uint32_t version)
    : m_version(version)
    , m_convergence_hint(0)
    , m_manifest_content_hint(0)
{
}

Superblock Superblock::FromConvergence(
    const ConvergedScraperStats& stats,
    const uint32_t version)
{
    Superblock superblock = Superblock::FromStats(GetScraperStatsAndVerifiedBeacons(stats), version);

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
    for (const auto& part_pair : stats.Convergence.ConvergedManifestPartPtrsMap) {
        const std::string& project_name = part_pair.first;
        const CSplitBlob::CPart* part_data_ptr = part_pair.second;

        projects.SetHint(project_name, part_data_ptr);
    }

    return superblock;
}

Superblock Superblock::FromStats(const ScraperStatsAndVerifiedBeacons& stats_and_verified_beacons, const uint32_t version)
{
    Superblock superblock(version);
    ScraperStatsSuperblockBuilder<Superblock> builder(superblock);

    builder.BuildFromStats(stats_and_verified_beacons);

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

    for (const auto& cpid_pair : m_cpids.Legacy()) {
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
    if (m_legacy) {
        return const_iterator(
            Magnitude::SCALE_FACTOR,
            m_legacy_magnitudes.begin(),
            m_legacy_magnitudes.end());
    }

    return const_iterator(
        decltype(m_small_magnitudes)::SCALE_FACTOR,
        m_small_magnitudes.begin(),
        m_small_magnitudes.end(),
        const_iterator(
            decltype(m_medium_magnitudes)::SCALE_FACTOR,
            m_medium_magnitudes.begin(),
            m_medium_magnitudes.end(),
            const_iterator(
                decltype(m_large_magnitudes)::SCALE_FACTOR,
                m_large_magnitudes.begin(),
                m_large_magnitudes.end())));
}

Superblock::CpidIndex::const_iterator Superblock::CpidIndex::end() const
{
    if (m_legacy) {
        return const_iterator(m_legacy_magnitudes.end());
    }

    return const_iterator(m_large_magnitudes.end());
}

Superblock::CpidIndex::size_type Superblock::CpidIndex::size() const
{
    if (m_legacy) {
        return m_legacy_magnitudes.size();
    }

    return m_small_magnitudes.size()
        + m_medium_magnitudes.size()
        + m_large_magnitudes.size();
}

bool Superblock::CpidIndex::empty() const
{
    return size() == 0;
}

const Superblock::MagnitudeStorageType& Superblock::CpidIndex::Legacy() const
{
    return m_legacy_magnitudes;
}

uint32_t Superblock::CpidIndex::Zeros() const
{
    return m_zero_magnitude_count;
}

size_t Superblock::CpidIndex::TotalCount() const
{
    return size() + m_zero_magnitude_count;
}

uint64_t Superblock::CpidIndex::TotalMagnitude() const
{
    return m_total_magnitude / Magnitude::SCALE_FACTOR;
}

double Superblock::CpidIndex::AverageMagnitude() const
{
    if (empty()) {
        return 0;
    }

    return static_cast<double>(TotalMagnitude()) / size();
}

Magnitude Superblock::CpidIndex::MagnitudeOf(const Cpid& cpid) const
{
    if (m_legacy) {
        const auto iter = std::lower_bound(
            m_legacy_magnitudes.begin(),
            m_legacy_magnitudes.end(),
            cpid,
            Superblock::CompareCpidOfPairLessThan);

        if (iter == m_legacy_magnitudes.end() || iter->first != cpid) {
            return Magnitude::Zero();
        }

        return Magnitude::FromScaled(iter->second * Magnitude::SCALE_FACTOR);
    }

    if (const auto mag_option = m_small_magnitudes.MagnitudeOf(cpid)) {
        return *mag_option;
    }

    if (const auto mag_option = m_medium_magnitudes.MagnitudeOf(cpid)) {
        return *mag_option;
    }

    if (const auto mag_option = m_large_magnitudes.MagnitudeOf(cpid)) {
        return *mag_option;
    }

    return Magnitude::Zero();
}

Superblock::CpidIndex::const_iterator
Superblock::CpidIndex::At(const size_t offset) const
{
    // Not very efficient--we can optimize this if needed:
    return std::next(begin(), offset);
}

void Superblock::CpidIndex::Add(const Cpid cpid, const Magnitude magnitude)
{
    // Only increment the total magnitude if the CPID does not already
    // exist in the index:
    switch (magnitude.Which()) {
        case Magnitude::Kind::ZERO:
            m_zero_magnitude_count++;
            return;

        case Magnitude::Kind::SMALL:
            m_small_magnitudes.Add(cpid, magnitude);
            break;

        case Magnitude::Kind::MEDIUM:
            m_medium_magnitudes.Add(cpid, magnitude);
            break;

        case Magnitude::Kind::LARGE:
            m_large_magnitudes.Add(cpid, magnitude);
            break;
    }

    m_total_magnitude += magnitude.Scaled();
}

void Superblock::CpidIndex::AddLegacy(const Cpid cpid, const uint16_t magnitude)
{
    m_legacy_magnitudes.emplace_back(cpid, magnitude);

    m_total_magnitude += magnitude * Magnitude::SCALE_FACTOR;
}

uint256 Superblock::CpidIndex::HashSegments() const
{
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);

    hasher << SerializeHash(m_small_magnitudes);
    hasher << SerializeHash(m_medium_magnitudes);
    hasher << SerializeHash(m_large_magnitudes);

    return hasher.GetHash();
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
    const auto iter = std::lower_bound(
        m_projects.begin(),
        m_projects.end(),
        name,
        [](const ProjectPair& a, const std::string& b) { return a.first < b; });

    if (iter == m_projects.end() || iter->first != name) {
        return boost::none;
    }

    return iter->second;
}

void Superblock::ProjectIndex::Add(std::string name, const ProjectStats& stats)
{
    if (name.empty()) {
        return;
    }

    m_projects.emplace_back(std::move(name), stats);
    m_total_rac += stats.m_rac;
}

void Superblock::ProjectIndex::SetHint(
    const std::string& name,
    const CSplitBlob::CPart* part_data_ptr)
{
    auto iter = std::lower_bound(
        m_projects.begin(),
        m_projects.end(),
        name,
        [](const ProjectPair& a, const std::string& b) { return a.first < b; });

    if (iter == m_projects.end() || iter->first != name) {
        return;
    }

    const uint256 part_hash = Hash(part_data_ptr->data.begin(), part_data_ptr->data.end());
    iter->second.m_convergence_hint = part_hash.GetUint64() >> 32;

    m_converged_by_project = true;
}

void Superblock::VerifiedBeacons::Reset(
    const ScraperPendingBeaconMap& verified_beacon_id_map)
{
    m_verified.clear();
    m_verified.reserve(verified_beacon_id_map.size());

    for (const auto& entry_pair : verified_beacon_id_map) {
        m_verified.emplace_back(entry_pair.second.key_id);
    }

    // Base58 encoding on the ScraperPendingBeaconMap key results in different
    // ordering of entries from that of the actual key IDs:
    //
    std::sort(m_verified.begin(), m_verified.end());
}

// -----------------------------------------------------------------------------
// Class: SuperblockPtr
// -----------------------------------------------------------------------------

SuperblockPtr::SuperblockPtr(
    std::shared_ptr<const Superblock> superblock,
    const CBlockIndex* const pindex)
    : SuperblockPtr(std::move(superblock), pindex->nHeight, pindex->nTime)
{
}

SuperblockPtr SuperblockPtr::ReadFromDisk(const CBlockIndex* const pindex)
{
    if (!pindex) {
        error("%s: invalid superblock index", __func__);
        return Empty();
    }

    if (!pindex->IsSuperblock()) {
        error("%s: %" PRId64 " is not a superblock", __func__, pindex->nHeight);
        return Empty();
    }

    CBlock block;

    if (!block.ReadFromDisk(pindex)) {
        error("%s: failed to read superblock from disk", __func__);
        return Empty();
    }

    return block.GetSuperblock(pindex);
}

void SuperblockPtr::Rebind(const CBlockIndex* const pindex)
{
    m_height = pindex->nHeight;
    m_timestamp = pindex->nTime;
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

    for (const auto& cpid_pair : superblock.m_cpids.Legacy()) {
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

QuorumHash QuorumHash::Hash(const ScraperStatsAndVerifiedBeacons& stats)
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
