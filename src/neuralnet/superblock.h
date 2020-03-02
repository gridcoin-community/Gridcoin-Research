#pragma once

#include "neuralnet/cpid.h"
#include "serialize.h"
#include "scraper/fwd.h"
#include "uint256.h"

#include <boost/optional.hpp>
#include <boost/variant/variant.hpp>
#include <string>

extern int64_t SCRAPER_CMANIFEST_RETENTION_TIME;

class ConvergedScraperStats; // Forward for Superblock

namespace NN {
class Superblock; // Forward for QuorumHash

//!
//! \brief Hashes and stores the digest of a superblock.
//!
class QuorumHash
{
public:
    //!
    //! \brief Internal representation of the result of a legacy MD5-based
    //! superblock hash.
    //!
    typedef std::array<unsigned char, 16> Md5Sum;

    //!
    //! \brief Describes the kind of hash contained in a \c QuorumHash object.
    //!
    enum class Kind
    {
        INVALID, //!< An empty or invalid quorum hash.
        SHA256,  //!< Hash created for superblocks version 2 and greater.
        MD5,     //!< Legacy hash created for superblocks before version 2.
    };

    //!
    //! \brief A tag type that describes an empty or invalid quorum hash.
    //!
    struct Invalid { };

    //!
    //! \brief Initialize an invalid quorum hash object.
    //!
    QuorumHash();

    //!
    //! \brief Initialize a SHA256 quorum hash object variant.
    //!
    //! \param hash Contains the bytes of the superblock digest produced by
    //! applying a SHA256 hashing algorithm to the significant data.
    //!
    QuorumHash(uint256 hash);

    //!
    //! \brief Initialize an MD5 quorum hash object variant.
    //!
    //! \param hash Contains the bytes of the superblock digest produced by the
    //! legacy MD5-based superblock hashing algorithm ("neural hash").
    //!
    QuorumHash(Md5Sum legacy_hash);

    //!
    //! \brief Initialize the appropriate quorum hash variant from the supplied
    //! bytes.
    //!
    //! Initializes to an invalid hash variant when the bytes do not represent
    //! a valid quorum hash.
    //!
    //! \param bytes 32 bytes for a SHA256 hash or 16 bytes for a legacy MD5
    //! hash.
    //!
    QuorumHash(const std::vector<unsigned char>& bytes);

    //!
    //! \brief Hash the provided superblock.
    //!
    //! \param superblock Superblock object containing the data to hash.
    //!
    //! \return The appropriate quorum hash variant digest depending on the
    //! version number of the superblock.
    //!
    static QuorumHash Hash(const Superblock& superblock);

    //!
    //! \brief Hash the provided scraper statistics to produce the same quorum
    //! hash that would be generated for a superblock created from the stats.
    //!
    //! CONSENSUS: This method will only produce a SHA256 quorum hash matching
    //! version 2+ superblocks. Do not use it to produce hashes of the scraper
    //! statistics for legacy superblocks.
    //!
    //! \param stats Scraper statistics from a convergence to hash.
    //!
    //! \return A SHA256 quorum hash of the scraper statistics.
    //!
    static QuorumHash Hash(const ScraperStats& stats);

    //!
    //! \brief Initialize a quorum hash object by parsing the supplied string
    //! representation of a hash.
    //!
    //! \param hex A 64-character hex-encoded string for a SHA256 hash, or a
    //! 32-character hex-encoded string for a legacy MD5 hash.
    //!
    //! \return A quorum hash object that contains the bytes of the hash value
    //! represented by the string or an invalid quorum hash if the string does
    //! not contain a well-formed MD5 or SHA256 hash.
    //!
    static QuorumHash Parse(const std::string& hex);

    bool operator==(const QuorumHash& other) const;
    bool operator!=(const QuorumHash& other) const;
    bool operator==(const uint256& other) const;
    bool operator!=(const uint256& other) const;
    bool operator==(const std::string& other) const;
    bool operator!=(const std::string& other) const;

    //!
    //! \brief Describe the type of hash contained.
    //!
    //! \return A value enumerated on \c QuorumHash::Kind .
    //!
    Kind Which() const;

    //!
    //! \brief Determine whether the object contains a valid superblock hash.
    //!
    //! \return \c true if the object contains a SHA256 or legacy MD5 hash.
    //!
    bool Valid() const;

    //!
    //! \brief Get a pointer to the bytes in the hash.
    //!
    //! \return A pointer to the beginning of the bytes in the hash, or a
    //! \c nullptr value if the object contains an invalid hash.
    //!
    const unsigned char* Raw() const;

    //!
    //! \brief Get the string representation of the hash.
    //!
    //! \return A 64-character hex-encoded string for a SHA256 hash, or a
    //! 32-character hex-encoded string for a legacy MD5 hash. Returns an
    //! empty string for an invalid hash.
    //!
    std::string ToString() const;

    //!
    //! \brief Serialize the object to the provided stream.
    //!
    //! \param stream The output stream.
    //!
    template<typename Stream>
    void Serialize(Stream& stream) const
    {
        unsigned char kind = m_hash.which();

        ::Serialize(stream, kind);

        switch (static_cast<Kind>(kind)) {
            case Kind::INVALID:
                break; // Suppress warning.

            case Kind::SHA256:
                boost::get<uint256>(m_hash).Serialize(stream);
                break;

            case Kind::MD5: {
                const Md5Sum& hash = boost::get<Md5Sum>(m_hash);

                stream.write(CharCast(hash.data()), hash.size());
                break;
            }
        }
    }

    //!
    //! \brief Deserialize the object from the provided stream.
    //!
    //! \param stream   The input stream.
    //!
    template<typename Stream>
    void Unserialize(Stream& stream)
    {
        unsigned char kind;

        ::Unserialize(stream, kind);

        switch (static_cast<Kind>(kind)) {
            case Kind::SHA256: {
                uint256 hash;
                hash.Unserialize(stream);

                m_hash = hash;
                break;
            }

            case Kind::MD5: {
                Md5Sum hash;
                stream.read(CharCast(hash.data()), hash.size());

                m_hash = hash;
                break;
            }

            default:
                m_hash = Invalid();
                break;
        }
    }

private:
    //!
    //! \brief Contains the bytes of a SHA256 or MD5 digest.
    //!
    //! CONSENSUS: Do not remove or reorder the types in this variant. This
    //! class relies on the type ordinality to tag serialized values.
    //!
    boost::variant<Invalid, uint256, Md5Sum> m_hash;
}; // QuorumHash

//!
//! \brief Stores the number recent of zero credit days for a BOINC project.
//! Used for automated project greylisting.
//!
class ZeroCreditTally
{
public:
    //!
    //! \brief The number of days of history to consider for the zero-credit
    //! days greylisting rule.
    //!
    static constexpr size_t ZCD_DAYS = 20;

    //!
    //! \brief The number of recent days that a project produces zero credit
    //! after which it is greylisted by the zero-credit days rule.
    //!
    static constexpr size_t ZCD_DAYS_LIMIT = 7;

    //!
    //! \brief Initialize an empty tally.
    //!
    ZeroCreditTally();

    //!
    //! \brief Initialize a tally from the packed representation of the zero
    //! credit days.
    //!
    //! \param zcd_packed 1-bits represent zero credit days.
    //!
    ZeroCreditTally(uint32_t zcd_packed);

    //!
    //! \brief Count the number of zero-credit days present in the tally.
    //!
    //! \return The number of recent days that a project produced no credit.
    //!
    size_t Count() const;

    //!
    //! \brief Determine whether a project triggered the zero-credit days rule
    //! for automated greylisting.
    //!
    bool Greylisted() const;

    //!
    //! \brief Shift the tally by one day and append the status of the latest
    //! day. The earliest day is dropped from the history.
    //!
    //! \brief is_zero_credit If true, set the latest day to a zero-credit day.
    //!
    //! \return A copy of the tally adjusted for the latest day.
    //!
    ZeroCreditTally Advance(const bool is_zero_credit) const;

    //!
    //! \brief Shift the tally by one day and append the status of the latest
    //! day by calculating the difference in project credit. The earliest day
    //! is dropped from the history.
    //!
    //! \param previous_credit Project credit preceeding the latest day.
    //! \param current_credit  Project credit on the latest day.
    //!
    //! \return A copy of the tally adjusted for the latest day.
    //!
    ZeroCreditTally AdvanceByDelta(
        const uint64_t previous_credit,
        const uint64_t current_credit) const;

    //!
    //! \brief Get the string representation of the tally.
    //!
    //! \return 32 characters where 'Y' represents a zero-credit day, and 'N'
    //! represents a non-zero-credit day. Ordered from earliest to latest.
    //!
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        // Most projects rarely produce zero-credit days, so we serialize
        // the field with VARINT encoding. This produces in a single byte
        // of overhead for each project most of the time.
        //
        READWRITE(VARINT(m_packed));
    }

private:
    //!
    //! \brief Zero-credit day bits represented as an integer.
    //!
    //! Each bit in this integer represents a day for a total of 32 days. The
    //! bits set to 1 represent zero-credit days.
    //!
    uint32_t m_packed;
}; // ZeroCreditTally

//!
//! \brief A snapshot of BOINC statistics used to calculate and verify research
//! rewards.
//!
//! About once per day, the Gridcoin network produces a superblock that contains
//! the magnitudes of active CPIDs. The superblocks embed into the block chain a
//! subset of data contained in the converged statistics emitted via the scraper
//! nodes. Regular nodes then use the statistics to calculate and verify rewards
//! claimed for BOINC computing work in generated blocks following a superblock.
//!
class Superblock
{
public:
    //!
    //! \brief Version number of the current format for a serialized superblock.
    //!
    //! CONSENSUS: Increment this value when introducing a breaking change and
    //! ensure that the serialization/deserialization routines also handle all
    //! of the previous versions.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

    //!
    //! \brief The maximum allowed size of a serialized superblock in bytes.
    //!
    //! The bulk of the superblock data is comprised of pairs of CPIDs to
    //! magnitude values. A value of 4 MB provides space for roughly 200k
    //! CPIDs that accumulated a non-zero magnitude.
    //!
    static constexpr size_t MAX_SIZE = 4 * 1000 * 1000;

    //!
    //! \brief Contains the CPID statistics aggregated for all projects.
    //!
    //! To conserve space in a serialized superblock, other sections refer to
    //! CPIDs by numeric offsets of the items stored in this index instead of
    //! storing the CPID values themselves.
    //!
    struct CpidIndex
    {
        typedef std::map<Cpid, uint16_t>::size_type size_type;
        typedef std::map<Cpid, uint16_t>::iterator iterator;
        typedef std::map<Cpid, uint16_t>::const_iterator const_iterator;

        //!
        //! \brief Initialize an empty CPID index.
        //!
        CpidIndex();

        //!
        //! \brief Initialize an empty CPID index for a legacy superblock with
        //! the number of zero-magnitude CPIDs.
        //!
        //! This initializes the object in legacy mode for superblock contract
        //! strings that contain the number of zero-magnitude CPIDs. The index
        //! will not imilicitly count and filter inserted zero-magnitude CPIDs
        //! so that a legacy quorum hash matches that of the string contract.
        //!
        //! \param zero_magnitude_count Number of zero-magnitude CPIDs omitted
        //! from the superblock.
        //!
        CpidIndex(uint32_t zero_magnitude_count);

        //!
        //! \brief Returns an iterator to the beginning.
        //!
        const_iterator begin() const;

        //!
        //! \brief Returns an iterator to the end.
        //!
        const_iterator end() const;

        //!
        //! \brief Get the number of unique CPIDs contained in the superblock
        //! that have a magnitude greater than zero.
        //!
        size_type size() const;

        //!
        //! \brief Determine whether the index contains any active CPIDs with a
        //! magnitude greater than zero.
        //!
        bool empty() const;

        //!
        //! \brief Get the number of zero-magnitude CPIDs.
        //!
        //! \return Number of CPIDs with beacons not included in the superblock
        //! because they exhibit zero magnitude.
        //!
        uint32_t Zeros() const;

        //!
        //! \brief Get the number of valid CPIDs present at the time that the
        //! superblock formed.
        //!
        //! \return The number of CPIDs, including zero-magnitude CPIDs.
        //!
        size_t TotalCount() const;

        //!
        //! \brief Get the sum of the magnitudes of all the CPIDs in the index.
        //!
        //! \return Total magnitude at the time of the superblock.
        //!
        uint64_t TotalMagnitude() const;

        //!
        //! \brief Get the average magnitude of all the CPIDs in the index.
        //!
        //! \return Average magnitude at the time of the superblock.
        //!
        double AverageMagnitude() const;

        //!
        //! \brief Get the network-wide magnitude of the provided CPID.
        //!
        //! \param cpid The CPID to look-up the magnitude for.
        //!
        //! \return A magnitude in the range of 0 to 65535. Returns zero if the
        //! CPID doesn't exist in the index.
        //!
        uint16_t MagnitudeOf(const Cpid& cpid) const;

        //!
        //! \brief Get the offset of the provided CPID in the index.
        //!
        //! \param cpid The CPID to look-up the offset for.
        //!
        //! \return The CPID's zero-based offset if it exists in the index, or
        //! the offset after the last element in the index (the index size) if
        //! it doesn't.
        //!
        size_t OffsetOf(const Cpid& cpid) const;

        //!
        //! \brief Get the CPID indexed at the specified offset.
        //!
        //! \param offset Zero-based offset of the CPID to fetch.
        //!
        //! \return Iterator to the specified CPID if it exists or an iterator
        //! to the end of the index if it does not. The iterator points to the
        //! CPID/magnitude pair.
        //!
        const_iterator At(const size_t offset) const;

        //!
        //! \brief Add the supplied CPID to the index.
        //!
        //! This method ignores an attempt to add a duplicate entry if a CPID
        //! already exists.
        //!
        //! \param cpid      The CPID to add.
        //! \param magnitude Total magnitude to associate with the CPID.
        //!
        void Add(const Cpid cpid, const uint16_t magnitude);

        //!
        //! \brief Add the supplied mining ID to the index if it represents a
        //! valid CPID.
        //!
        //! This method ignores an attempt to add a duplicate entry if a CPID
        //! already exists.
        //!
        //! \param id        May contain a CPID.
        //! \param magnitude Total magnitude to associate with the CPID.
        //!
        void Add(const MiningId id, const uint16_t magnitude);

        //!
        //! \brief Add the supplied mining ID to the index if it represents a
        //! valid CPID after rounding the magnitude to an integer.
        //!
        //! This method ignores an attempt to add a duplicate entry if a CPID
        //! already exists.
        //!
        //! \param id        May contain a CPID.
        //! \param magnitude Total magnitude to associate with the CPID.
        //!
        void RoundAndAdd(const MiningId id, const double magnitude);

        //!
        //! \brief Serialize the object to the provided stream.
        //!
        //! \param stream The output stream.
        //!
        template<typename Stream>
        void Serialize(Stream& stream) const
        {
            if (!(stream.GetType() & SER_GETHASH)) {
                WriteCompactSize(stream, m_magnitudes.size());
            }

            for (const auto& cpid_pair : m_magnitudes) {
                cpid_pair.first.Serialize(stream);

                // Compact size encoding provides better compression for the
                // magnitude values than VARINT because most CPIDs have mags
                // less than 253:
                //
                // Note: This encoding imposes an upper limit of MAX_SIZE on
                // the encoded value. Magnitudes fall well within the limit.
                //
                WriteCompactSize(stream, cpid_pair.second);
            }

            VARINT(m_zero_magnitude_count).Serialize(stream);
        }

        //!
        //! \brief Deserialize the object from the provided stream.
        //!
        //! \param stream   The input stream.
        //! \param nType    Target protocol type (network, disk, etc.).
        //! \param nVersion Protocol version.
        //!
        template<typename Stream>
        void Unserialize(Stream& stream)
        {
            m_magnitudes.clear();
            m_total_magnitude = 0;

            unsigned int size = ReadCompactSize(stream);

            for (size_t i = 0; i < size; i++) {
                Cpid cpid;
                cpid.Unserialize(stream);

                // Read magnitude using compact-size encoding:
                uint16_t magnitude = ReadCompactSize(stream);

                m_magnitudes.emplace(cpid, magnitude);
                m_total_magnitude += magnitude;
            }

            VARINT(m_zero_magnitude_count).Unserialize(stream);
        }

    private:
        //!
        //! \brief Maps external CPIDs to their aggregated statistics.
        //!
        std::map<Cpid, uint16_t> m_magnitudes;

        //!
        //! \brief Contains the number of CPIDs with beacons not included in
        //! the superblock because they exhibit zero magnitude.
        //!
        //! Omitting zero-magnitude CPIDs from superblocks conserves space.
        //!
        uint32_t m_zero_magnitude_count;

        //!
        //! \brief Tally of the sum of the magnitudes of all the CPIDs present
        //! in the superblock.
        //!
        uint64_t m_total_magnitude;

        //!
        //! \brief Flag that indicates whether to enable legacy behavior.
        //!
        //! This flag initializes to \c true when constructing a superblock
        //! with a preset zero-magnitude CPID count.
        //!
        //! Some legacy superblocks contain CPIDs with zero magnitudes. The
        //! quorum hash includes these CPIDs, so we load them into the CPID
        //! collection instead of incrementing the zero-magnitude counter.
        //!
        bool m_legacy;
    }; // CpidIndex

    //!
    //! \brief Contains the statistics associated with a BOINC project.
    //!
    struct ProjectStats
    {
        //!
        //! \brief Initialize an empty (zero) statistics object.
        //!
        ProjectStats();

        //!
        //! \brief Initialize an object with the provided statistics.
        //!
        //! \param total_credit All-time credit produced by the project.
        //! \param average_rac  Average recent average credit of the project.
        //! \param rac          Sum of the RAC of all the project CPIDs.
        //!
        ProjectStats(uint64_t total_credit, uint64_t average_rac, uint64_t rac);

        //!
        //! \brief Initialize an object with the provided statistics available
        //! in a legacy superblock.
        //!
        //! \param average_rac Average recent average credit of the project.
        //! \param rac         Sum of the RAC of all the project CPIDs.
        //!
        ProjectStats(uint64_t average_rac, uint64_t rac);

        uint64_t m_total_credit; //!< All-time credit produced by the project.
        uint64_t m_average_rac;  //!< Average project recent average credit.
        uint64_t m_rac;          //!< Sum of the RAC of all the project CPIDs.
        ZeroCreditTally m_zcd;   //!< Tracks zero-credit days for greylistiing.

        //!
        //! \brief A truncated hash of the converged manifest part that forms
        //! the project statistics.
        //!
        //! For fallback-to-project-level convergence scenarios, \c ProjectStats
        //! objects include the hash of the manifest part to aid receiving nodes
        //! with superblock validation. The hash is truncated to conserve space.
        //!
        uint32_t m_convergence_hint;

        //!
        //! \brief Determine whether the project triggered automated greylist
        //! rules.
        //!
        //! \return \c true if the zero-credit days exceeded the limit.
        //!
        bool Greylisted() const;

        //!
        //! \brief Produce a new zero-credit day tally for the subsequent
        //! superblock by calculating the difference in credit.
        //!
        //! \param next_credit The projects credit for the upcoming superblock.
        //! If no greater than the current credit, set the latest day to a zero
        //! credit day.
        //!
        //! \return A zero-credit day tally object for the project in the next
        //! superblock.
        //!
        ZeroCreditTally AdvanceZcd(const uint64_t next_credit) const;

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(VARINT(m_total_credit));
            READWRITE(VARINT(m_average_rac));
            READWRITE(VARINT(m_rac));
            READWRITE(m_zcd);

            // ProjectIndex handles serialization of m_convergence_hint
            // when creating superblocks from fallback-to-project-level
            // convergence scenarios.
        }
    }; // ProjectStats

    //!
    //! \brief An optional type that either contains some project statistics or
    //! does not.
    //!
    typedef boost::optional<ProjectStats> ProjectStatsOption;

    //!
    //! \brief Contains aggregated project statistics.
    //!
    class ProjectIndex
    {
    public:
        typedef std::map<std::string, ProjectStats>::size_type size_type;
        typedef std::map<std::string, ProjectStats>::iterator iterator;
        typedef std::map<std::string, ProjectStats>::const_iterator const_iterator;

        //!
        //! \brief A serialization flag used to pass fallback-to-project-level
        //! convergence context to \c ProjectStats serialization routines.
        //!
        //! The selected serialization modifier value will not conflict with
        //! those enumerated in serialize.h.
        //!
        static constexpr int SER_CONVERGED_BY_PROJECT = 24;

        //!
        //! \brief Indicates that the superblock was generated from a fallback-
        //! to-project-level convergence.
        //!
        bool m_converged_by_project;

        //!
        //! \brief Initialize an empty project index.
        //!
        ProjectIndex();

        //!
        //! \brief Returns an iterator to the beginning.
        //!
        const_iterator begin() const;

        //!
        //! \brief Returns an iterator to the end.
        //!
        const_iterator end() const;

        //!
        //! \brief Get the number of projects contained in the superblock.
        //!
        size_type size() const;

        //!
        //! \brief Determine whether the index contains any projects.
        //!
        bool empty() const;

        //!
        //! \brief Get the sum of the recent average credit of all the projects
        //! on the whitelist.
        //!
        //! \return Total project RAC at the time of the superblock.
        //!
        uint64_t TotalRac() const;

        //!
        //! \brief Get the average recent average credit of all the projects on
        //! the whitelist.
        //!
        //! \return Average project RAC at the time of the superblock.
        //!
        double AverageRac() const;

        //!
        //! \brief Try to get the project statistics for a project with the
        //! specified name.
        //!
        //! \param name As it exists in the current whitelist.
        //!
        //! \return An object that contains the matching project statistics if
        //! it exists.
        //!
        ProjectStatsOption Try(const std::string& name) const;

        //!
        //! \brief Add the supplied project statistics to the index.
        //!
        //! This method ignores an attempt to add a duplicate entry if a project
        //! already exists with the same name.
        //!
        //! \param name  As it exists in the current whitelist.
        //! \param stats Contains project RAC data.
        //!
        void Add(std::string name, const ProjectStats& stats);

        //!
        //! \brief Set the convergence part hint for the specified project.
        //!
        //! \param part_data The convergence part to create the hint from.
        //!
        void SetHint(const std::string& name, const CSerializeData& part_data);

        //!
        //! \brief Serialize the object to the provided stream.
        //!
        //! \param stream The output stream.
        //!
        template<typename Stream>
        void Serialize(Stream& stream) const
        {
            if (!(stream.GetType() & SER_GETHASH)) {
                stream << m_converged_by_project;
                WriteCompactSize(stream, m_projects.size());
            }

            for (const auto& project_pair : m_projects) {
                stream << project_pair;

                // Trigger serialization of ProjectStats convergence hints for
                // superblocks generated by a fallback-to-project convergence:
                //
                if (!(stream.GetType() & SER_GETHASH) && m_converged_by_project) {
                    stream << project_pair.second.m_convergence_hint;
                }
            }
        }

        //!
        //! \brief Deserialize the object from the provided stream.
        //!
        //! \param stream The input stream.
        //!
        template<typename Stream>
        void Unserialize(Stream& stream)
        {
            m_projects.clear();
            m_total_rac = 0;

            stream >> m_converged_by_project;

            const unsigned int project_count = ReadCompactSize(stream);
            auto iter = m_projects.begin();

            for (unsigned int i = 0; i < project_count; i++) {
                std::pair<std::string, ProjectStats> project_pair;
                stream >> project_pair;

                if (m_converged_by_project) {
                    stream >> project_pair.second.m_convergence_hint;
                }

                m_total_rac += project_pair.second.m_rac;
                iter = m_projects.insert(iter, project_pair);
            }
        }

    private:
        //!
        //! \brief Maps project names to their aggregated statistics.
        //!
        //! The map is keyed by project names as they exist in administrative
        //! project contracts present at the time that the superblock forms.
        //!
        std::map<std::string, ProjectStats> m_projects;

        //!
        //! \brief Tally of the sum of the recent average credit of all the
        //! projects present in the superblock.
        //!
        uint64_t m_total_rac;
    }; // ProjectIndex

    //!
    //! \brief Version number of the serialized superblock format.
    //!
    //! Defaults to the most recent version for a new superblock instance.
    //!
    //! Version 1: Legacy packed superblock contract stored as a record in the
    //! transaction's hashBoinc field. Contains only top-level CPID magnitudes
    //! and the credit averages for each project.
    //!
    //! Version 2: Superblock data serializable using the built-in serialize.h
    //! facilities. Stored in the superblock field of a block rather than in a
    //! transaction to provide for a greater size. It includes total credit of
    //! each project to facilitate automated greylisting.
    //!
    uint32_t m_version = CURRENT_VERSION;

    //!
    //! \brief The truncated scraper convergence content hash and underlying
    //! manifest content hash (they are computed differently).
    //!
    //! These values aid receiving nodes with validation for superblocks created
    //! from past convergence data.
    //!
    uint32_t m_convergence_hint;
    uint32_t m_manifest_content_hint;

    CpidIndex m_cpids;       //!< Maps superblock CPIDs to magntudes.
    ProjectIndex m_projects; //!< Whitelisted projects statistics.
    //std::vector<BeaconAcknowledgement> m_verified_beacons;

    int64_t m_height;    //!< Height of the block that contains the contract.
    int64_t m_timestamp; //!< Timestamp of the block that contains the contract.

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(m_version);
            READWRITE(m_convergence_hint);
            READWRITE(m_manifest_content_hint);
        }

        READWRITE(m_cpids);
        READWRITE(m_projects);
        //READWRITE(m_verified_beacons);
    }

    //!
    //! \brief Initialize an empty superblock object.
    //!
    Superblock();

    //!
    //! \brief Initialize an empty superblock object of the specified version.
    //!
    //! \param version Version number of the serialized superblock format.
    //!
    Superblock(uint32_t version);

    //!
    //! \brief Initialize a superblock from the provided converged scraper
    //! statistics.
    //!
    //! \param stats Converged statistics containing CPID and project credit
    //! data.
    //!
    //! \return A new superblock instance that contains the imported scraper
    //! statistics.
    //!
    static Superblock FromConvergence(
        const ConvergedScraperStats& stats,
        const uint32_t version = Superblock::CURRENT_VERSION);

    //!
    //! \brief Initialize a superblock from the provided scraper statistics.
    //!
    //! \param stats Converged statistics containing CPID and project credit
    //! data.
    //!
    //! \return A new superblock instance that contains the imported scraper
    //! statistics.
    //!
    static Superblock FromStats(
        const ScraperStats& stats,
        const uint32_t version = Superblock::CURRENT_VERSION);

    //!
    //! \brief Initialize a superblock from a legacy superblock contract.
    //!
    //! \param packed Legacy superblock contract as a string of XML-like text
    //! data and binary-packed CPID/magnitude data.
    //!
    //! \return A new superblock instance that contains the imported contract
    //! statistics.
    //!
    static Superblock UnpackLegacy(const std::string& packed);

    //!
    //! \brief Pack the superblock data into a legacy superblock contract.
    //!
    //! CONSENSUS: Although this method produces a legacy contract compatible
    //! with older protocols, it does not guarantee that the contract matches
    //! exactly to legacy input contract versions imported by UnpackLegacy().
    //! Use this method to produce new contracts from a superblock object. Do
    //! not reproduce existing superblock contracts with this routine if they
    //! will be retransmitted to other nodes.
    //!
    //! \return Legacy superblock contract as a string of XML-like text data
    //! and binary-packed CPID/magnitude data.
    //!
    std::string PackLegacy() const;

    //!
    //! \brief Determine whether the instance represents a complete superblock.
    //!
    //! \return \c true if the superblock contains all of the required elements.
    //!
    bool WellFormed() const;

    //!
    //! \brief Determine whether the superblock was generated from a fallback-
    //! to-project-level scraper convergence.
    //!
    //! \return \c true if the ProjectIndex fallback convergence flag is set.
    //!
    bool ConvergedByProject() const;

    //!
    //! \brief Get the current age of the superblock.
    //!
    //! \return Superblock age in seconds.
    //!
    int64_t Age() const;

    //!
    //! \brief Get a hash of the significant data in the superblock.
    //!
    //! \param regenerate If \c true, skip selection of any cached hash value
    //! and recompute the hash.
    //!
    //! \return A quorum hash object that contiains a SHA256 hash for version
    //! 2+ superblocks or an MD5 hash for legacy version 1 superblocks.
    //!
    QuorumHash GetHash(const bool regenerate = false) const;

private:
    //!
    //! \brief The most recently-regenerated quorum hash of the superblock.
    //!
    //! Because of their size, superblocks are expensive to hash. A superblock
    //! caches its quorum hash when calling the GetHash() method to speed up a
    //! subsequent hash request. The block acceptance pipeline may need a hash
    //! value in several places when processing a superblock.
    //!
    //! The cached value is NOT invalidated when modifying a superblock. Call
    //! the GetHash() method with the argument set to \c true to regenerate a
    //! cached quorum hash. A received superblock's significant data is never
    //! modified, so the need to regenerate a cached hash will rarely occur.
    //!
    mutable QuorumHash m_hash_cache;
}; // Superblock


//!
//! \brief Validate the supplied superblock by comparing it to local manifest
//! data.
//!
//! \param superblock The superblock to validate.
//! \param use_cache  If \c false, skip validation with the scraper cache.
//! \param hint_bits  For testing by-project fallback validation.
//!
//! \return \c True if the local manifest data produces a matching superblock.
//!
bool ValidateSuperblock(
    const Superblock& superblock,
    const bool use_cache = true,
    const size_t hint_bits = 32);
} // namespace NN

namespace std {
//!
//! \brief Specializes std::hash<T> for NN::QuorumHash.
//!
//! This enables the use of NN::QuorumHash as a key in a std::unordered_map
//! object.
//!
//! CONSENSUS: Don't use the hash produced by this routine (or by any std::hash
//! specialization) in protocol-specific implementations. It ignores endianness
//! and outputs a value with a chance of collision probably too great for usage
//! besides the intended local look-up functionality.
//!
template<>
struct hash<NN::QuorumHash>
{
    //!
    //! \brief Create a hash of the supplied quorum hash object.
    //!
    //! \param quorum_hash Contains the bytes to hash.
    //!
    //! \return A hash as the sum of the two halves of the bytes in a legacy
    //! MD5 hash, or the sum of the quarters of a SHA256 hash. Returns 0 for
    //! an invalid hash.
    //!
    size_t operator()(const NN::QuorumHash& quorum_hash) const
    {
        // Just convert the quorum hash into a value that we can store in a
        // size_t object. The hashes are already unique identifiers.
        //
        size_t out = 0;
        const unsigned char* const bytes = quorum_hash.Raw();

        switch (quorum_hash.Which()) {
            case NN::QuorumHash::Kind::INVALID:
                break; // 0 represents invalid
            case NN::QuorumHash::Kind::SHA256:
                out = *reinterpret_cast<const uint64_t*>(bytes + 16)
                    + *reinterpret_cast<const uint64_t*>(bytes + 24);
                // Pass-through case.
            case NN::QuorumHash::Kind::MD5:
                out += *reinterpret_cast<const uint64_t*>(bytes)
                    + *reinterpret_cast<const uint64_t*>(bytes + 8);
                break;
        }

        return out;
    }
};
} // namespace std

// This is part of the scraper but is put here, because it needs the complete NN:Superblock class.
struct ConvergedScraperStats
{
    // Flag to indicate cache is clean or dirty (i.e. state change of underlying statistics has occurred.
    // This flag is marked true in ScraperGetSuperblockContract() and false on receipt or deletion of
    // statistics objects.
    bool bClean = false;

    int64_t nTime;
    ScraperStats mScraperConvergedStats;
    ConvergedManifest Convergence;

    // There is a small chance of collision on the key, but given this is really a hint map,
    // It is okay.
    // reduced nContentHash ------ SB Hash ---- Converged Manifest object
    std::map<uint32_t, std::pair<NN::QuorumHash, ConvergedManifest>> PastConvergences;

    // New superblock object and hash.
    NN::Superblock NewFormatSuperblock;

    void AddConvergenceToPastConvergencesMap()
    {
        uint32_t nReducedContentHash = Convergence.nContentHash.GetUint64() >> 32;

        if (Convergence.nContentHash != uint256() && PastConvergences.find(nReducedContentHash) == PastConvergences.end())
        {
            // This is specifically this form of insert to insure that if there is a hint "collision" the referenced
            // SB Hash and Convergence stored will be the LATER one.
            PastConvergences[nReducedContentHash] = std::make_pair(NewFormatSuperblock.GetHash(), Convergence);
        }
    }

    unsigned int DeleteOldConvergenceFromPastConvergencesMap()
    {
        unsigned int nDeleted = 0;

        std::map<uint32_t, std::pair<NN::QuorumHash, ConvergedManifest>>::iterator iter;
        for (iter = PastConvergences.begin(); iter != PastConvergences.end(); )
        {
            // If the convergence entry is older than CManifest retention time, then delete the past convergence
            // entry, because the underlying CManifest will be deleted by the housekeeping loop using the same
            // aging. The erase advances the iterator in C++11.
            if (iter->second.second.timestamp < GetAdjustedTime() - SCRAPER_CMANIFEST_RETENTION_TIME)
            {
                iter = PastConvergences.erase(iter);

                ++nDeleted;
            }
            else
            {
                ++iter;
            }
        }

        return nDeleted;
    }

};
