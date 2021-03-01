// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "gridcoin/cpid.h"
#include "gridcoin/magnitude.h"
#include "gridcoin/scraper/fwd.h"
#include "serialize.h"
#include "uint256.h"

#include <boost/optional.hpp>
#include <boost/variant/variant.hpp>
#include <iterator>
#include <memory>
#include <string>

extern int64_t SCRAPER_CMANIFEST_RETENTION_TIME;

extern std::vector<uint160> GetVerifiedBeaconIDs(const ConvergedManifest& StructConvergedManifest);
extern std::vector<uint160> GetVerifiedBeaconIDs(const ScraperPendingBeaconMap& VerifiedBeaconMap);
extern ScraperStatsAndVerifiedBeacons GetScraperStatsByConvergedManifest(const ConvergedManifest& StructConvergedManifest);

class CBlockIndex;
class ConvergedScraperStats; // Forward for Superblock

namespace GRC {
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
    static QuorumHash Hash(const ScraperStatsAndVerifiedBeacons& stats);

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
    //! \brief A single mapping of a CPID to a compact magnitude value.
    //!
    typedef std::pair<Cpid, uint16_t> CpidPair;


    //!
    //! \brief The underlying collection type that contains CPID to magnitude
    //! mappings.
    //!
    typedef std::vector<CpidPair> MagnitudeStorageType;

    //!
    //! \brief Determine whether a CPID in a magnitude mapping represents a
    //! lexicographically lesser value than specified CPID.
    //!
    //! \param a The first element in the pair is the CPID to compare.
    //! \param b The CPID to compare it to.
    //!
    //! \return \c true if the bytes of the CPID in the mapping compare less
    //! than the provided CPID.
    //!
    static bool CompareCpidOfPairLessThan(const CpidPair& a, const Cpid& b)
    {
        return a.first < b;
    }

    //!
    //! \brief A collection that maps CPIDs to magnitudes for a particular
    //! magnitude precision category.
    //!
    //! \tparam MagnitudeSize Unsigned integer type that stores the magnitudes.
    //! \tparam Scale         Scale factor of the magnitude size category.
    //!
    template <typename MagnitudeSize, size_t Scale>
    class MagnitudeMap
    {
        static_assert(
            std::is_unsigned<MagnitudeSize>::value,
            "Declared magnitude storage type must be an unsigned integer.");

        static_assert(
            Scale == Magnitude::SCALE_FACTOR
                || Scale == Magnitude::MEDIUM_SCALE_FACTOR
                || Scale == Magnitude::SMALL_SCALE_FACTOR,
            "Declared scale is not a valid magnitude size scale factor.");

    public:
        typedef typename MagnitudeStorageType::size_type size_type;
        typedef typename MagnitudeStorageType::iterator iterator;
        typedef typename MagnitudeStorageType::const_iterator const_iterator;

        //!
        //! \brief The factor to scale the compact magnitudes stored in this
        //! segment by.
        //!
        static constexpr size_t SCALE_FACTOR = Scale;

        //!
        //! \brief Returns an iterator to the beginning.
        //!
        const_iterator begin() const
        {
            return m_magnitudes.begin();
        }

        //!
        //! \brief Returns an iterator to the end.
        //!
        const_iterator end() const
        {
            return m_magnitudes.end();
        }

        //!
        //! \brief Get the number of unique CPIDs contained in the magnitude
        //! map that have a magnitude greater than zero.
        //!
        size_type size() const
        {
            return m_magnitudes.size();
        }

        //!
        //! \brief Get the magnitude for the specified CPID if it exists in
        //! the map.
        //!
        //! \param cpid CPID to fetch the magnitude for.
        //!
        //! \return The CPID's magnitude at normal scale.
        //!
        boost::optional<Magnitude> MagnitudeOf(const Cpid& cpid) const
        {
            const auto iter = std::lower_bound(
                m_magnitudes.begin(),
                m_magnitudes.end(),
                cpid,
                CompareCpidOfPairLessThan);

            if (iter == m_magnitudes.end() || iter->first != cpid) {
                return boost::none;
            }

            return Magnitude::FromScaled(iter->second * Scale);
        }

        //!
        //! \brief Add a magnitude to the map for the specified CPID.
        //!
        //! \param cpid      The CPID to add.
        //! \param magnitude Total magnitude to associate with the CPID.
        //!
        void Add(const Cpid& cpid, const Magnitude magnitude)
        {
            const uint16_t compact = magnitude.Scaled() / Scale;

            m_magnitudes.emplace_back(cpid, compact);
        }

        //!
        //! \brief Serialize a 1-byte magnitude to the provided stream.
        //!
        //! \param stream    The output stream.
        //! \param magnitude A magnitude value that fits in one byte.
        //!
        template <typename Stream>
        static void WriteMagnitude(Stream& stream, const uint8_t magnitude)
        {
            ::Serialize(stream, magnitude);
        }

        //!
        //! \brief Compress and serialize a multibyte magnitude to the provided
        //! stream.
        //!
        //! Magnitude values smaller than 253 serialize to one byte. Values of
        //! 253 or greater serialize as three bytes.
        //!
        //! \param stream    The output stream.
        //! \param magnitude A magnitude value that fits in one or three bytes.
        //!
        template <typename Stream>
        static void WriteMagnitude(Stream& stream, const uint16_t magnitude)
        {
            // Compact size encoding provides better compression for the
            // magnitude values than VARINT because most CPIDs have mags
            // less than 253.
            //
            // Note: This encoding imposes an upper limit of MAX_SIZE on
            // the encoded value. Magnitudes fall well within the limit.
            //
            WriteCompactSize(stream, magnitude);
        }

        //!
        //! \brief Deserialize a 1-byte magnitude from the provided stream.
        //!
        //! \param stream    The input stream.
        //! \param magnitude Set to the deserialized magnitude value.
        //!
        template <typename Stream>
        static void ReadMagnitude(Stream& stream, uint8_t& magnitude)
        {
            ::Unserialize(stream, magnitude);
        }

        //!
        //! \brief Deserialize a multibyte magnitude from the provided stream.
        //!
        //! \param stream    The input stream.
        //! \param magnitude Set to the deserialized magnitude value.
        //!
        template <typename Stream>
        static void ReadMagnitude(Stream& stream, uint16_t& magnitude)
        {
            magnitude = ReadCompactSize(stream);
        }

        //!
        //! \brief Record the serialized size of the magnitude map.
        //!
        //! This overload optimizes the size calculation for the small and
        //! medium magnitude maps in the superblock. These maps only store
        //! magnitude values fixed to one byte in size so we avoid looping
        //! over the collections to compute the total. The large magnitude
        //! map serializes values as either one or two bytes so we need to
        //! iterate over each item, but a superblock contains far fewer of
        //! these records.
        //!
        //! \param s The size computer instance to record the size with.
        //!
        template <typename M = MagnitudeSize>
        typename std::enable_if<std::is_same<M, uint8_t>::value>::type
        Serialize(CSizeComputer& s) const
        {
            WriteCompactSize(s, m_magnitudes.size());
            s.seek((sizeof(Cpid) + sizeof(MagnitudeSize)) * m_magnitudes.size());
        }

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
                WriteMagnitude(stream, (MagnitudeSize)cpid_pair.second);
            }
        }

        //!
        //! \brief Deserialize the object from the provided stream.
        //!
        //! \param stream          The input stream.
        //! \param total_magnitude Increased by the value of each magnitude.
        //!
        template<typename Stream>
        void Unserialize(Stream& stream, uint64_t& total_magnitude)
        {
            m_magnitudes.clear();

            const uint64_t size = ReadCompactSize(stream);
            m_magnitudes.reserve(size);

            for (size_t i = 0; i < size; i++) {
                Cpid cpid;
                cpid.Unserialize(stream);

                MagnitudeSize magnitude;
                ReadMagnitude(stream, magnitude);

                m_magnitudes.emplace_back(cpid, magnitude);
                total_magnitude += magnitude * Scale;
            }
        }

    private:
        MagnitudeStorageType m_magnitudes; //!< Maps CPIDs to magnitude values.
    }; // MagnitudeMap

    //!
    //! \brief Contains the CPID statistics aggregated for all projects.
    //!
    //! Because version 2+ superblocks support magnitudes with greater precision
    //! for small values, this type partitions the storage for CPID to magnitude
    //! mappings into three segments--one for each magnitude size category. This
    //! conserves space for magnitude values less than 253 which serialize using
    //! one byte and avoids transformation overhead for deserialization compared
    //! to strategies that manipulate the encoding of magnitude values for space
    //! savings.
    //!
    //! This class' API provides an abstraction for the partitioning details and
    //! for the integer-only magnitudes in legacy superblocks so client code can
    //! interact with these objects as a coherent collection.
    //!
    struct CpidIndex
    {
        typedef MagnitudeStorageType::size_type size_type;

        //!
        //! \brief A traversable sequence of the CPID to magnitude mappings in a
        //! superblock that behaves similarly to a \c const iterator.
        //!
        //! Superblocks store the CPIDs to magnitude mappings in segmented data
        //! structures to improve serialization performance and to reduce space
        //! overhead. This iterable type provides access to a sequence of these
        //! mappings without exposing the internal layout.
        //!
        class Sequence
        {
        public:
            typedef MagnitudeStorageType::const_iterator BaseIterator;
            typedef BaseIterator::difference_type difference_type;
            typedef std::forward_iterator_tag iterator_category;

            typedef Sequence value_type;
            typedef const Sequence* pointer;
            typedef const Sequence& reference;

            //!
            //! \brief Initialize a segment at the beginning or inner range.
            //!
            //! \param scale Magnitude scale factor of the segment.
            //! \param begin An iterator to the beginning of the segment.
            //! \param end   An iterator to the end of the segment.
            //! \param next  Iterator for the next segment in the range.
            //!
            Sequence(
                size_t scale,
                BaseIterator begin,
                BaseIterator end,
                Sequence next)
                : m_scale(scale)
                , m_iter(begin)
                , m_end(end)
                , m_next(std::make_shared<Sequence>(std::move(next)))
            {
                AdvanceSegment();
            }

            //!
            //! \brief Initialize a segment at the end of the range.
            //!
            //! \param scale Magnitude scale factor of the segment.
            //! \param begin An iterator to the beginning of the segment.
            //! \param end   An iterator to the end of the segment.
            //!
            Sequence(size_t scale, BaseIterator begin, BaseIterator end)
                : m_scale(scale)
                , m_iter(begin)
                , m_end(end)
            {
            }

            //!
            //! \brief Initialize a segment positioned at the end of the range.
            //!
            //! \param end An iterator to the end of the last segment.
            //!
            Sequence(BaseIterator end) : Sequence(0, end, end)
            {
            }

            //!
            //! \brief Get the CPID at the current position.
            //!
            const GRC::Cpid& Cpid() const
            {
                return m_iter->first;
            }

            //!
            //! \brief Get the magnitude for the CPID at the current position.
            //!
            GRC::Magnitude Magnitude() const
            {
                return GRC::Magnitude::FromScaled(m_iter->second * m_scale);
            }

            //!
            //! \brief Get a reference to the current position.
            //!
            //! \return A reference to itself. This hides the details of the
            //! segment. Use Cpid() or Magnitude() to fetch the values.
            //!
            reference operator*() const
            {
                return *this;
            }

            //!
            //! \brief Get a pointer to the current position.
            //!
            //! \return A pointer to itself. This hides the details of the
            //! segment. Use Cpid() or Magnitude() to fetch the values.
            //!
            pointer operator->() const
            {
                return &(*this);
            }

            //!
            //! \brief Advance the current position.
            //!
            Sequence& operator++()
            {
                ++m_iter;
                AdvanceSegment();

                return *this;
            }

            //!
            //! \brief Advance the current position.
            //!
            Sequence operator++(int)
            {
                Sequence copy(*this);
                ++(*this);

                return copy;
            }

            //!
            //! \brief Determine whether the item at the current position is
            //! equal to the specified position.
            //!
            bool operator==(reference other) const
            {
                return m_iter == other.m_iter;
            }

            //!
            //! \brief Determine whether the item at the current position is
            //! not equal to the specified position.
            //!
            bool operator!=(reference other) const
            {
                return m_iter != other.m_iter;
            }

        private:
            size_t       m_scale; //!< Scale factor of the current segment.
            BaseIterator m_iter;  //!< Position in the current segment.
            BaseIterator m_end;   //!< End of the current segment.

            //!
            //! \brief The next segment in the magnitude range to traverse
            //! after reaching the end of the current segment.
            //!
            //! A null pointer for the last segment.
            //!
            std::shared_ptr<Sequence> m_next;

            //!
            //! \brief Advance to the next segment in the range if positioned
            //! at the end of the current segment.
            //!
            void AdvanceSegment()
            {
                while (m_iter == m_end && m_next) {
                    *this = std::move(*m_next);
                }
            }
        }; // Sequence

        //!
        //! \brief The default iterator walks through each magnitude segment
        //! in order of the size category.
        //!
        typedef Sequence const_iterator;

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
        //! \brief Get a legacy representation of CPID to magnitude mappings.
        //!
        //! Legacy superblocks stored magnitudes as unscaled 16-bit integers.
        //! This provides access to the legacy collection without normalizing
        //! the magnitudes for greater precision.
        //!
        //! \return A reference to the legacy magnitude mappings or to an empty
        //! collection if the object isn't a legacy (version 1) superblock.
        //!
        const MagnitudeStorageType& Legacy() const;

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
        Magnitude MagnitudeOf(const Cpid& cpid) const;

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
        //! \param cpid      The CPID to add.
        //! \param magnitude Total magnitude to associate with the CPID.
        //!
        void Add(const Cpid cpid, const Magnitude magnitude);

        //!
        //! \brief Add the supplied magnitude for a legacy superblock to the
        //! index.
        //!
        //! \param cpid      The CPID to add.
        //! \param magnitude Total magnitude to associate with the CPID.
        //!
        void AddLegacy(const Cpid cpid, const uint16_t magnitude);

        //!
        //! \brief Get a hash of the magnitude segments.
        //!
        //! \return SHA256 hash of the CPIDs and magnitudes.
        //!
        uint256 HashSegments() const;

        //!
        //! \brief Serialize the object to the provided stream.
        //!
        //! \param stream The output stream.
        //!
        template<typename Stream>
        void Serialize(Stream& stream) const
        {
            if (!(stream.GetType() & SER_GETHASH)) {
                m_small_magnitudes.Serialize(stream);
                m_medium_magnitudes.Serialize(stream);
                m_large_magnitudes.Serialize(stream);
            } else {
                // To allow for direct hashing of scraper stats data without
                // allocating a superblock, we generate an intermediate hash
                // of the segments of CPID-to-magnitude mappings:
                //
                HashSegments().Serialize(stream);
            }

            VARINT(m_zero_magnitude_count).Serialize(stream);
        }

        //!
        //! \brief Deserialize the object from the provided stream.
        //!
        //! \param stream The input stream.
        //!
        template<typename Stream>
        void Unserialize(Stream& stream)
        {
            m_total_magnitude = 0;

            m_small_magnitudes.Unserialize(stream, m_total_magnitude);
            m_medium_magnitudes.Unserialize(stream, m_total_magnitude);
            m_large_magnitudes.Unserialize(stream, m_total_magnitude);

            VARINT(m_zero_magnitude_count).Unserialize(stream);
        }

    private:
        //!
        //! \brief Maps external CPIDs to magnitudes for magnitudes smaller
        //! than 1. These serialize as one byte.
        //!
        //! Magnitude storage is partitioned for compact serialization while
        //! retaining precision for smaller values.
        //!
        MagnitudeMap<uint8_t, Magnitude::SMALL_SCALE_FACTOR> m_small_magnitudes;

        //!
        //! \brief Maps external CPIDs to magnitudes for magnitudes in the
        //! range of [1,10). These serialize as one byte.
        //!
        //! Magnitude storage is partitioned for compact serialization while
        //! retaining precision for smaller values.
        //!
        MagnitudeMap<uint8_t, Magnitude::MEDIUM_SCALE_FACTOR> m_medium_magnitudes;

        //!
        //! \brief Maps external CPIDs to magnitudes for magnitudes greater
        //! than or equal to 10. These serialize as one or two bytes.
        //!
        //! Magnitude storage is partitioned for compact serialization while
        //! retaining precision for smaller values.
        //!
        MagnitudeMap<uint16_t, Magnitude::SCALE_FACTOR> m_large_magnitudes;

        //!
        //! \brief Maps external CPIDs to magnitudes for legacy superblocks
        //! (block version 10 and below).
        //!
        MagnitudeStorageType m_legacy_magnitudes;

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
        //! Not serialized--memory only.
        //!
        uint64_t m_total_magnitude;

        //!
        //! \brief Flag that indicates whether to enable legacy behavior.
        //!
        //! Not serialized--memory only.
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

        //!
        //! \brief A truncated hash of the converged manifest part that forms
        //! the project statistics.
        //!
        //! For fallback-to-project-level convergence scenarios, \c ProjectStats
        //! objects include the hash of the manifest part to aid receiving nodes
        //! with superblock validation. The hash is truncated to conserve space.
        //!
        uint32_t m_convergence_hint;

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(VARINT(m_total_credit));
            READWRITE(VARINT(m_average_rac));
            READWRITE(VARINT(m_rac));

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
        //!
        //! \brief A single mapping of a project name to project statistcs.
        //!
        typedef std::pair<std::string, ProjectStats> ProjectPair;

        //!
        //! \brief The underlying collection type that contains project name
        //! to project statistics mappings.
        //!
        typedef std::vector<ProjectPair> ProjectStorageType;

    public:
        typedef ProjectStorageType::size_type size_type;
        typedef ProjectStorageType::iterator iterator;
        typedef ProjectStorageType::const_iterator const_iterator;

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
        //! \param name  As it exists in the current whitelist.
        //! \param stats Contains project RAC data.
        //!
        void Add(std::string name, const ProjectStats& stats);

        //!
        //! \brief Set the convergence part hint for the specified project.
        //!
        //! \param part_data The convergence part to create the hint from.
        //!
        void SetHint(const std::string& name, const CSplitBlob::CPart *part_data_ptr);

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

            const uint64_t project_count = ReadCompactSize(stream);
            m_projects.reserve(project_count);

            for (uint64_t i = 0; i < project_count; i++) {
                ProjectPair project_pair;
                stream >> project_pair;

                if (m_converged_by_project) {
                    stream >> project_pair.second.m_convergence_hint;
                }

                m_total_rac += project_pair.second.m_rac;
                m_projects.emplace_back(std::move(project_pair));
            }
        }

    private:
        //!
        //! \brief Maps project names to their aggregated statistics.
        //!
        //! The map is keyed by project names as they exist in administrative
        //! project contracts present at the time that the superblock forms.
        //!
        ProjectStorageType m_projects;

        //!
        //! \brief Tally of the sum of the recent average credit of all the
        //! projects present in the superblock.
        //!
        //! Not serialized--memory only.
        //!
        uint64_t m_total_rac;
    }; // ProjectIndex

    struct VerifiedBeacons
    {
        //!
        //! \brief Contains the beacon IDs verified by scraper convergence.
        //!
        //! This contains a collection of the RIPEMD-160 hashes of the beacon public
        //! keys verified by the scrapers. Nodes shall activate these beacons during
        //! superblock processing.
        //!
        std::vector<uint160> m_verified;

        VerifiedBeacons() {};

        void Reset(const ScraperPendingBeaconMap& verified_beacon_id_map);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(m_verified);
        }
    };

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
    VerifiedBeacons m_verified_beacons; //!< Wrapped verified beacons vector

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
        READWRITE(m_verified_beacons);
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
    //! \brief Initialize a superblock by deserializing it from the provided
    //! stream.
    //!
    //! \param s The input stream.
    //!
    template <typename Stream>
    Superblock(deserialize_type, Stream& s)
    {
        Unserialize(s);
    }

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
    static Superblock FromConvergence(const ConvergedScraperStats &stats,
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
        const ScraperStatsAndVerifiedBeacons& stats_and_verified_beacons,
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
    //! We retain this method for unit tests only.
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
    //! \brief Get a hash of the significant data in the superblock.
    //!
    //! \param regenerate If \c true, skip selection of any cached hash value
    //! and recompute the hash.
    //!
    //! \return A quorum hash object that contains a SHA256 hash for version
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
//! \brief A smart pointer that wraps a superblock object for shared ownership
//! with context of its containing block.
//!
//! In general, this class represents a superblock published and received in a
//! block.
//!
class SuperblockPtr
{
public:
    int64_t m_height;    //!< Height of the block that contains the contract.
    int64_t m_timestamp; //!< Timestamp of the block that contains the contract.

    //!
    //! \brief Initialize an empty superblock smart pointer.
    //!
    SuperblockPtr() : SuperblockPtr(std::make_shared<const Superblock>(), 0, 0)
    {
    }

    //!
    //! \brief Wrap the provided superblock and store context of its containing
    //! block.
    //!
    //! \param superblock The superblock object to wrap.
    //! \param pindex     Index of the block that contains the superblock.
    //!
    //! \return A smart pointer that wraps the provided superblock.
    //!
    static SuperblockPtr BindShared(
        Superblock&& superblock,
        const CBlockIndex* const pindex)
    {
        return SuperblockPtr(
            std::make_shared<const Superblock>(std::move(superblock)),
            pindex);
    }

    //!
    //! \brief Create a representation of an empty, invalid superblock.
    //!
    //! \return A smart pointer to an empty superblock.
    //!
    static SuperblockPtr Empty()
    {
        return SuperblockPtr();
    }

    //!
    //! \brief Load a superblock from disk.
    //!
    //! \param pindex Index of the block that contains the superblock.
    //!
    //! \return Wrapped superblock from the specified block or an empty object
    //! if the block contains no valid superblock.
    //!
    static SuperblockPtr ReadFromDisk(const CBlockIndex* const pindex);

    const Superblock& operator*() const noexcept { return *m_superblock; }
    const Superblock* operator->() const noexcept { return m_superblock.get(); }

    //!
    //! \brief Replace the wrapped superblock object.
    //!
    //! \param superblock The superblock object to wrap.
    //!
    void Replace(Superblock superblock)
    {
        m_superblock = std::make_shared<const Superblock>(std::move(superblock));
    }

    //!
    //! \brief Reassociate the superblock with the containing block context.
    //!
    //! \param pindex Provides context about the containing block.
    //!
    void Rebind(const CBlockIndex* const pindex);

    //!
    //! \brief Get the current age of the superblock.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return Superblock age in seconds.
    //!
    int64_t Age(const int64_t now) const
    {
        return now - m_timestamp;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_superblock);
    }

private:
    std::shared_ptr<const Superblock> m_superblock; //!< The wrapped superblock.

    //!
    //! \brief Initialize a new superblock smart pointer wrapper.
    //!
    //! \param superblock Smart pointer around a superblock object.
    //! \param height     Height of the block that contains the superblock.
    //! \param timestamp  Time of the block that contains the superblock.
    //!
    SuperblockPtr(
        std::shared_ptr<const Superblock> superblock,
        const int64_t height,
        const int64_t timestamp)
        : m_height(height)
        , m_timestamp(timestamp)
        , m_superblock(std::move(superblock))
    {
    }

    //!
    //! \brief Initialize a new superblock smart pointer.
    //!
    //! \param superblock Smart pointer around a superblock object.
    //! \param pindex     Provides context about the containing block.
    //!
    SuperblockPtr(
        std::shared_ptr<const Superblock> superblock,
        const CBlockIndex* const pindex);
}; // SuperblockPtr
} // namespace GRC

namespace std {
//!
//! \brief Specializes std::hash<T> for GRC::QuorumHash.
//!
//! This enables the use of GRC::QuorumHash as a key in a std::unordered_map
//! object.
//!
//! CONSENSUS: Don't use the hash produced by this routine (or by any std::hash
//! specialization) in protocol-specific implementations. It ignores endianness
//! and outputs a value with a chance of collision probably too great for usage
//! besides the intended local look-up functionality.
//!
template<>
struct hash<GRC::QuorumHash>
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
    size_t operator()(const GRC::QuorumHash& quorum_hash) const
    {
        // Just convert the quorum hash into a value that we can store in a
        // size_t object. The hashes are already unique identifiers.
        //
        size_t out = 0;
        const unsigned char* const bytes = quorum_hash.Raw();

        switch (quorum_hash.Which()) {
            case GRC::QuorumHash::Kind::INVALID:
                break; // 0 represents invalid
            case GRC::QuorumHash::Kind::SHA256:
                out = *reinterpret_cast<const uint64_t*>(bytes + 16)
                    + *reinterpret_cast<const uint64_t*>(bytes + 24);
                // Pass-through case.
            case GRC::QuorumHash::Kind::MD5:
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
    ConvergedScraperStats() : Convergence(), NewFormatSuperblock()
    {
        bClean = false;
        bMinHousekeepingComplete = false;

        nTime = 0;
        mScraperConvergedStats = {};
        PastConvergences = {};
    }

    ConvergedScraperStats(const int64_t nTime_in, const ConvergedManifest& Convergence) : Convergence(Convergence)
    {
        bClean = false;
        bMinHousekeepingComplete = false;

        nTime = nTime_in;

        mScraperConvergedStats = GetScraperStatsByConvergedManifest(Convergence).mScraperStats;
    }

    // Flag to indicate cache is clean or dirty (i.e. state change of underlying statistics has occurred.
    // This flag is marked true in ScraperGetSuperblockContract() and false on receipt or deletion of
    // statistics objects.
    bool bClean;

    // This flag tracks the completion of at least one iteration of the housekeeping loop. The purpose of this flag
    // is to ensure enough time has gone by after a (re)start of the wallet that a complete set of manifests/parts
    // have been collected. Trying to form a contract too early may result in a local convergence that may not
    // match an incoming superblock that comes in very close to the wallet start, and if enough manifests/parts are
    // missing the backup validation checks will fail, resulting in a forked client due to failure to validate
    // the superblock. This should help the difficult corner case of a wallet restarted literally a minute or two
    // before the superblock is received. This has the effect of allowing a grace period of nScraperSleep after the
    // wallet start where an incoming superblock will allowed with Result::UNKNOWN, rather than rejected with
    // Result::INVALID.
    bool bMinHousekeepingComplete;

    int64_t nTime;
    ScraperStats mScraperConvergedStats;
    ConvergedManifest Convergence;

    // There is a small chance of collision on the key, but given this is really a hint map,
    // It is okay.
    // reduced nContentHash ------ SB Hash ---- Converged Manifest object
    std::map<uint32_t, std::pair<GRC::QuorumHash, ConvergedManifest>> PastConvergences;

    // New superblock object and hash.
    GRC::Superblock NewFormatSuperblock;

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

        std::map<uint32_t, std::pair<GRC::QuorumHash, ConvergedManifest>>::iterator iter;
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
