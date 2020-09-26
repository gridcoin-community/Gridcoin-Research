// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialize.h"

#include <array>
#include <boost/optional.hpp>
#include <boost/variant/get.hpp>
#include <boost/variant/variant.hpp>
#include <string>
#include <vector>

namespace GRC {
//!
//! \brief Represents a BOINC researcher's external, public CPID.
//!
//! An external CPID is the MD5 digest of the concatenation of a BOINC account's
//! internal CPID and email address.
//!
class Cpid
{
public:
    //!
    //! \brief Initialize a zero-value CPID object.
    //!
    Cpid() : m_bytes({ })
    {
    }

    //!
    //! \brief Initialize a CPID object from the bytes in an MD5 hash.
    //!
    //! \param bytes CPID output bytes from an MD5 hashing function to copy.
    //!
    Cpid(const std::vector<unsigned char>& bytes);

    //!
    //! \brief Create a CPID object from its MD5 string representation.
    //!
    //! Malformed CPIDs parse to the zero value. Because a zero-value CPID is
    //! technically a valid value, prefer to use MiningId::Parse() to parse a
    //! string that may not contain a well-formed CPID.
    //!
    //! \param hex Hex-encoded bytes of the MD5 hash.
    //!
    //! \return A new CPID object that contains the bytes in the provided hash
    //! or a zero-value CPID object if the input string does not represent a
    //! valid MD5 hash.
    //!
    static Cpid Parse(const std::string& hex);

    //!
    //! \brief Initialize a CPID object by hashing an internal, private CPID
    //! and email address to produce the external, public CPID.
    //!
    //! \param internal Private (internal) CPID as a hex-encoded string.
    //! \param email    Email address of the BOINC account for the CPID.
    //!
    //! \return The public CPID that represents the provided internal CPID and
    //! email address pair.
    //!
    static Cpid Hash(const std::string& internal, const std::string& email);

    //!
    //! \brief Compare a supplied CPID value for equality.
    //!
    //! \param other A CPID value to check equality for.
    //!
    //! \return \c true if the supplied CPID's bytes match.
    //!
    bool operator==(const Cpid& other) const
    {
        return m_bytes == other.m_bytes;
    }

    //!
    //! \brief Compare a supplied CPID value for inequality.
    //!
    //! \param other A CPID value to check inequality for.
    //!
    //! \return \c true if the supplied CPID's bytes do not match.
    //!
    bool operator!=(const Cpid& other) const
    {
        return m_bytes != other.m_bytes;
    }

    //!
    //! \brief Compare a supplied CPID value for less than other.
    //!
    //! The comparison checks from left to right with the most significant byte
    //! on the left (byte order equivalent to the hex string representation).
    //!
    //! CONSENSUS: Changing the behavior of this comparison operator will affect
    //! the sort order of protocol-sensitive maps (for example, superblocks).
    //!
    //! \param other A CPID value to check whether current is less than.
    //!
    //! \return \c true if the supplied CPID's bytes represent a greater number.
    //!
    bool operator<(const Cpid& other) const
    {
        return m_bytes < other.m_bytes;
    }

    //!
    //! \brief Compare a supplied CPID value for greater than other.
    //!
    //! The comparison checks from left to right with the most significant byte
    //! on the left (byte order equivalent to the hex string representation).
    //!
    //! CONSENSUS: Changing the behavior of this comparison operator will affect
    //! the sort order of protocol-sensitive maps (for example, superblocks).
    //!
    //! \param other A CPID value to check whether current is greater than.
    //!
    //! \return \c true if the supplied CPID's bytes represent a lesser number.
    //!
    bool operator>(const Cpid& other) const
    {
        return m_bytes > other.m_bytes;
    }

    //!
    //! \brief Determine whether the CPID contains only zeros.
    //!
    //! \return \c true if every byte in the CPID equals \c 0x00.
    //!
    bool IsZero() const;

    //!
    //! \brief Determine whether the external CPID matches the hash created from
    //! the supplied internal CPID and email address.
    //!
    //! \param internal Private CPID as a 32-character hex-encoded string.
    //! \param email    Email address of the BOINC account for the CPID.
    //!
    //! \return \c true if the hashed concatenation of the internal CPID and
    //! email address match the external CPID represented by this object.
    //!
    bool Matches(const std::string& internal, const std::string& email) const;

    //!
    //! \brief Get the bytes that make up the CPID.
    //!
    //! \return An immutable reference to the underlying byte array.
    //!
    const std::array<unsigned char, 16>& Raw() const
    {
        return m_bytes;
    }

    //!
    //! \brief Get the bytes that make up the CPID.
    //!
    //! \return A mutable reference to the underlying byte array.
    //!
    std::array<unsigned char, 16>& Raw()
    {
        return m_bytes;
    }

    //!
    //! \brief Get the MD5 string representation of the CPID.
    //!
    //! \return Hex-encoded bytes of the MD5 hash.
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
        stream.write(CharCast(m_bytes.data()), m_bytes.size());
    }

    //!
    //! \brief Deserialize the object from the provided stream.
    //!
    //! \param stream The input stream.
    //!
    template<typename Stream>
    void Unserialize(Stream& stream)
    {
        stream.read(CharCast(m_bytes.data()), m_bytes.size());
    }

private:
    //!
    //! \brief Bytes that make up the public CPID (an MD5 hash).
    //!
    //! Store these as an array instead of a \c uint128 object because CPIDs
    //! have little in common with that numeric type. The \c uint128 numbers
    //! represent themselves as big-endian whereas CPIDs display canonically
    //! as little-endian. Use of \c uint128 here would complicate validation
    //! and add unnecessary overhead.
    //!
    std::array<unsigned char, 16> m_bytes;
}; // Cpid

//!
//! \brief An optional type that either contains a reference to some external
//! CPID value or does not.
//!
typedef boost::optional<const Cpid> CpidOption;

//!
//! \brief A variant type that identifies an entity that may receive rewards.
//!
class MiningId
{
public:
    //!
    //! \brief Describes the kind of miner represented by a \c MiningId object.
    //!
    enum class Kind : unsigned char
    {
        INVALID  = 0x00, //!< An empty or invalid CPID.
        INVESTOR = 0x01, //!< A CPID that represents a non-researcher.
        CPID     = 0x02, //!< A valid exernal CPID.
    };

    //!
    //! \brief A tag type that describes an empty or invalid CPID.
    //!
    struct Invalid
    {
        //!
        //! \brief Get the string representation of an invalid mining ID.
        //!
        //! \return An empty string.
        //!
        std::string ToString() const;
    };

    //!
    //! \brief A tag type that describes a CPID that represents a non-researcher
    //! (an investor without a BOINC CPID).
    //!
    struct Investor
    {
        //!
        //! \brief Get the string representation of an investor.
        //!
        //! \return The string literal "INVESTOR".
        //!
        std::string ToString() const;
    };

    //!
    //! \brief Initialize an empty, invalid mining ID object.
    //!
    MiningId() : m_variant(Invalid())
    {
    }

    //!
    //! \brief Initialize a mining ID for the provided CPID value.
    //!
    //! \param cpid An external BOINC CPID.
    //!
    MiningId(Cpid cpid) : m_variant(std::move(cpid))
    {
    }

    //!
    //! \brief Initialize a mining ID that represents a non-researcher.
    //!
    static MiningId ForInvestor()
    {
        MiningId miningId;
        miningId.m_variant = Investor();

        return miningId;
    }

    //!
    //! \brief Create a mining ID object from its string representation.
    //!
    //! \param input Hex-encoded bytes of a CPID, or the string "INVESTOR".
    //!
    //! \return A mining ID object parsed from the input.
    //!
    static MiningId Parse(const std::string& input);

    //!
    //! \brief Compare a supplied mining ID value for equality.
    //!
    //! \param other A mining ID value to check equality for.
    //!
    //! \return \c true if the supplied mining ID's bytes match.
    //!
    bool operator==(const MiningId& other) const
    {
        if (m_variant.which() != other.m_variant.which()) {
            return false;
        }

        if (Which() == Kind::CPID) {
            return *this == boost::get<Cpid>(other.m_variant);
        }

        return true;
    }

    //!
    //! \brief Compare a supplied mining ID value for inequality.
    //!
    //! \param other A mining ID value to check inequality for.
    //!
    //! \return \c true if the supplied mining ID's bytes do not match.
    //!
    bool operator!=(const MiningId& other) const
    {
        return !(*this == other);
    }

    //!
    //! \brief Compare a supplied CPID value for equality.
    //!
    //! \param other A CPID value to check equality for.
    //!
    //! \return \c true if this object contains a CPID variant and the supplied
    //! CPID's bytes match.
    //!
    bool operator==(const Cpid& other) const
    {
        return Which() == Kind::CPID
            && boost::get<Cpid>(m_variant) == other;
    }

    //!
    //! \brief Compare a supplied CPID value for inequality.
    //!
    //! \param other A CPID value to check inequality for.
    //!
    //! \return \c true if this object does not contain a CPID variant or the
    //! supplied CPID's bytes do not match.
    //!
    bool operator!=(const Cpid& other) const
    {
        return !(*this == other);
    }

    //!
    //! \brief Describe the type of entity represented by the mining ID.
    //!
    //! \return A value enumerated on \c MiningId::Kind .
    //!
    Kind Which() const
    {
        return static_cast<Kind>(m_variant.which());
    }

    //!
    //! \brief Determine whether the mining ID is valid.
    //!
    //! \return \c true if the object represents a valid CPID or an investor.
    //!
    bool Valid() const
    {
        return Which() != Kind::INVALID;
    }

    //!
    //! \brief Get the CPID value if it exists.
    //!
    //! \return An object that contains a reference to a \c Cpid object if the
    //! variant holds a CPID value.
    //!
    CpidOption TryCpid() const
    {
        if (Which() != Kind::CPID) {
            return boost::none;
        }

        return boost::get<Cpid>(m_variant);
    }

    //!
    //! \brief Get the string representation of the mining ID.
    //!
    //! \return Hex-encoded bytes of the MD5 hash for a CPID, "INVESTOR" for
    //! an investor, or an empty string for invalid mining IDs.
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
        unsigned char kind = m_variant.which();

        ::Serialize(stream, kind);

        if (static_cast<Kind>(kind) == Kind::CPID) {
            boost::get<Cpid>(m_variant).Serialize(stream);
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
        unsigned char kind;

        ::Unserialize(stream, kind);

        switch (static_cast<Kind>(kind)) {
            case Kind::INVESTOR:
                m_variant = Investor();
                break;
            case Kind::CPID:
                {
                    Cpid cpid;
                    cpid.Unserialize(stream);

                    m_variant = std::move(cpid);
                }
                break;
            default:
                m_variant = Invalid();
                break;
        }
    }

private:
    //!
    //! \brief Stores the various states that a mining ID may exist in.
    //!
    boost::variant<Invalid, Investor, Cpid> m_variant;
}; // MiningId
} // namespace GRC

namespace std {
//!
//! \brief Specializes std::hash<T> for GRC::Cpid.
//!
//! This enables the use of GRC::Cpid as a key in a std::unordered_map object.
//!
//! CONSENSUS: Don't use the hash produced by this routine (or by any std::hash
//! specialization) in protocol-specific implementations. It ignores endianness
//! and outputs a value with a chance of collision probably too great for usage
//! besides the intended local look-up functionality.
//!
template<>
struct hash<GRC::Cpid>
{
    //!
    //! \brief Create a hash of the supplied CPID object.
    //!
    //! \param cpid Contains the bytes to hash.
    //!
    //! \return A hash as the sum of the two halves of the bytes in the CPID.
    //!
    size_t operator()(const GRC::Cpid& cpid) const
    {
        // Just convert the CPID into a value that we can store in a size_t
        // object. CPIDs are already unique identifiers.
        //
        return *reinterpret_cast<const uint64_t*>(cpid.Raw().data())
            + *reinterpret_cast<const uint64_t*>(cpid.Raw().data() + 8);
    }
};
}
