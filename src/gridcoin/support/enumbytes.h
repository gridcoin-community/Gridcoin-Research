// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialize.h"

namespace GRC {
//!
//! \brief A wrapper around an enum type that provides for serialization of the
//! enum values as unsigned integers.
//!
//! This type provides boilerplate useful for preserving the semantics of scoped
//! enumerations (enum class) while facilitating serialization of the values.
//!
//! \tparam E Enum type wrapped by this class.
//! \tparam U Underlying unsigned integer type to serialize the enum value as.
//! \tparam B Enum value that describes the upper bound of the enum range.
//!
template <typename E, typename U, E B>
class EnumBytes
{
public:
    //!
    //! \brief The greatest valid value in the wrapped enum.
    //!
    static constexpr size_t MAX = static_cast<size_t>(B) - 1;

    static_assert(std::is_enum<E>::value, "EnumBytes<E,U,B>: E is not an enum");
    static_assert(std::is_unsigned<U>::value, "EnumBytes<E,U,B>: U is signed");
    static_assert(MAX <= std::numeric_limits<U>::max(),
        "EnumBytes<E,U,B>: upper bound B for enum E exceeds range of type U");

    //!
    //! \brief Serializes and deserializes enum values from a stream.
    //!
    //! This type can be used to serialize enums without storing the value as
    //! an EnumBytes<E, U, B> object.
    //!
    class Formatter
    {
    public:
        //!
        //! \brief Serialize an enum value as a byte to the provided stream.
        //!
        //! \param stream The output stream.
        //!
        template <typename Stream>
        void Ser(Stream& s, E e)
        {
            ::Serialize(s, EnumBytes<E, U, B>(e).Raw());
        }

        //!
        //! \brief Deserialize an enum value from the provided stream.
        //!
        //! \param stream The input stream.
        //!
        template <typename Stream>
        void Unser(Stream& s, E& e)
        {
            U raw;
            ::Unserialize(s, raw);

            if (raw > EnumBytes<E, U, B>::MAX) {
                throw std::ios_base::failure("EnumBytes out of range");
            }

            e = static_cast<E>(raw);
        }
    };

    //!
    //! \brief Wrap the provided enum value.
    //!
    //! \param value The enum value to wrap.
    //!
    constexpr EnumBytes(E value) noexcept : m_value(value)
    {
    }

    constexpr bool operator==(const E& other) const noexcept { return m_value == other; }
    constexpr bool operator==(const EnumBytes<E, U, B>& other) const noexcept { return *this == other.m_value; }
    constexpr bool operator!=(const E& other) const noexcept { return !(*this == other); }
    constexpr bool operator!=(const EnumBytes<E, U, B>& other) const noexcept { return !(*this == other); }
    constexpr bool operator<(const E& other) const noexcept { return m_value < other; }
    constexpr bool operator<(const EnumBytes<E, U, B>& other) const noexcept { return *this < other.m_value; }
    constexpr bool operator<=(const E& other) const noexcept { return m_value <= other; }
    constexpr bool operator<=(const EnumBytes<E, U, B>& other) const noexcept { return *this <= other.m_value; }
    constexpr bool operator>(const E& other) const noexcept { return m_value > other; }
    constexpr bool operator>(const EnumBytes<E, U, B>& other) const noexcept { return *this > other.m_value; }
    constexpr bool operator>=(const E& other) const noexcept { return m_value >= other; }
    constexpr bool operator>=(const EnumBytes<E, U, B>& other) const noexcept { return *this >= other.m_value; }

    //!
    //! \brief Get the wrapped enum value.
    //!
    //! \return A value enumerated on enum \c E.
    //!
    constexpr E Value() const noexcept
    {
        return m_value;
    }

    //!
    //! \brief Get the wrapped enum value as a value of the underlying type.
    //!
    //! \return For example, an unsigned char for an enum that represents
    //! an underlying byte value.
    //!
    constexpr uint8_t Raw() const noexcept
    {
        return static_cast<uint8_t>(m_value);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(Using<Formatter>(m_value));
    }

protected:
    E m_value; //!< The wrapped enum value.
}; // EnumBytes<E, U, B>

//!
//! \brief A wrapper around an enum type that provides for serialization of the
//! enum values as single bytes.
//!
//! This type expects a wrapped enum to contain an OUT_OF_BOUND identifier for
//! the value greater than the upper bound of the enumerated range. A value of
//! zero should map to a meaningful identifier in the enum.
//!
//! \tparam E Enum type wrapped by this class.
//!
template <typename E>
class EnumByte : public EnumBytes<E, uint8_t, E::OUT_OF_BOUND>
{
public:
    //!
    //! \brief Initialize an enum wrapper to zero.
    //!
    constexpr EnumByte() noexcept
        : EnumByte(static_cast<E>(0))
    {
    }

    //!
    //! \brief Wrap the provided enum value.
    //!
    //! \param value The enum value to wrap.
    //!
    constexpr EnumByte(E value) noexcept
        : EnumBytes<E, uint8_t, E::OUT_OF_BOUND>(value)
    {
    }
};
} // namespace GRC
