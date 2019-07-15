#pragma once

#include "neuralnet/cpid.h"
#include "serialize.h"
#include "scraper/fwd.h"
#include "uint256.h"

#include <boost/optional.hpp>
#include <boost/variant/variant.hpp>
#include <string>

std::string UnpackBinarySuperblock(std::string block);
std::string PackBinarySuperblock(std::string sBlock);

namespace NN {
class QuorumHash; // Forward for Superblock

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
        //! \brief Get the size of the data to serialize.
        //!
        //! \param nType    Target protocol type (network, disk, etc.).
        //! \param nVersion Protocol version.
        //!
        //! \return Size of the data in bytes.
        //!
        unsigned int GetSerializeSize(int nType, int nVersion) const
        {
            unsigned int size =  GetSizeOfCompactSize(m_magnitudes.size())
                + m_magnitudes.size() * sizeof(Cpid)
                + VARINT(m_zero_magnitude_count).GetSerializeSize();

            for (const auto& cpid_pair : m_magnitudes) {
                // Compact size encoding provides better compression for the
                // magnitude values than VARINT because most CPIDs have mags
                // less than 253:
                //
                // Note: This encoding imposes an upper limit of MAX_SIZE on
                // the encoded value. Magnitudes fall well within the limit.
                //
                size += GetSizeOfCompactSize(cpid_pair.second);
            }

            return size;
        }

        //!
        //! \brief Serialize the object to the provided stream.
        //!
        //! \param stream   The output stream.
        //! \param nType    Target protocol type (network, disk, etc.).
        //! \param nVersion Protocol version.
        //!
        template<typename Stream>
        void Serialize(Stream& stream, int nType, int nVersion) const
        {
            WriteCompactSize(stream, m_magnitudes.size());

            for (const auto& cpid_pair : m_magnitudes) {
                cpid_pair.first.Serialize(stream, nType, nVersion);

                // Write magnitude using compact-size encoding:
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
        void Unserialize(Stream& stream, int nType, int nVersion)
        {
            m_magnitudes.clear();
            m_total_magnitude = 0;

            unsigned int size = ReadCompactSize(stream);

            for (size_t i = 0; i < size; i++) {
                Cpid cpid;
                cpid.Unserialize(stream, nType, nVersion);

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

        IMPLEMENT_SERIALIZE
        (
            READWRITE(VARINT(m_total_credit));
            READWRITE(VARINT(m_average_rac));
            READWRITE(VARINT(m_rac));
        )
    };

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

        IMPLEMENT_SERIALIZE
        (
            READWRITE(m_projects);

            // Tally up the recent average credit after deserializing.
            //
            if (fRead) {
                REF(m_total_rac) = 0;

                for (const auto& project_pair : m_projects) {
                    REF(m_total_rac) += project_pair.second.m_rac;
                }
            }
        )

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

    CpidIndex m_cpids;       //!< Maps superblock CPIDs to magntudes.
    ProjectIndex m_projects; //!< Whitelisted projects statistics.
    //std::vector<BeaconAcknowledgement> m_verified_beacons;

    int64_t m_height;    //!< Height of the block that contains the contract.
    int64_t m_timestamp; //!< Timestamp of the block that contains the contract.

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH)) {
            READWRITE(m_version);
        }

        nVersion = m_version;

        READWRITE(m_cpids);
        READWRITE(m_projects);
        //READWRITE(m_verified_beacons);
    )

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
    //! \brief Initialize a superblock from the provided scraper statistics.
    //!
    //! \param stats Converged statistics containing CPID and project credit
    //! data.
    //!
    //! \return A new superblock instance that contains the imported scraper
    //! statistics.
    //!
    static Superblock FromStats(const ScraperStats& stats);

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
    //! \brief Get the current age of the superblock.
    //!
    //! \return Superblock age in seconds.
    //!
    int64_t Age() const;
}; // Superblock

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
    //! \brief Hash the provided superblock.
    //!
    //! \param superblock Superblock object containing the data to hash.
    //!
    //! \return The appropriate quorum hash variant digest depending on the
    //! version number of the superblock.
    //!
    static QuorumHash Hash(const Superblock& superblock);

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
    //! \brief Get the string representation of the hash.
    //!
    //! \return A 64-character hex-encoded string for a SHA256 hash, or a
    //! 32-character hex-encoded string for a legacy MD5 hash. Returns an
    //! empty string for an invalid hash.
    //!
    std::string ToString() const;

private:
    //!
    //! \brief Contains the bytes of a SHA256 or MD5 digest.
    //!
    boost::variant<Invalid, uint256, Md5Sum> m_hash;
}; // QuorumHash
}
