// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <tinyformat.h>

#include <cmath>
#include <stdint.h>
#include <string>

namespace GRC {
//!
//! \brief Represents a CPID's computing power at a point in time relative to
//! the other CPIDs in the network.
//!
//! Gridcoin uses units of magnitude to quantify a BOINC participant's computing
//! power. Magnitude values are one of the primary variables in the formula that
//! calculates research rewards. The protocol derives these values using BOINC's
//! recent average credit (RAC) metric by calculating the ratios of a CPID's RAC
//! at each BOINC project to the sum of the RAC of all the participants at these
//! projects.
//!
//! With block version 11 and later, the network tracks magnitudes with decaying
//! precision as the magnitude values increase. These manifest in three tiers:
//!
//!   - Less than 1: two fractional places
//!   - Between 1 and 10: one fractional place
//!   - 10 and greater: whole numbers only
//!
//! This improves representation of participants with less computing power while
//! retaining the superblock compactness of the earlier, integer-only format. To
//! present these values to the rest of the application uniformly, the Magnitude
//! type stores normalized magnitudes as integers scaled by a factor of 100.
//!
class Magnitude
{
public:
    //!
    //! \brief The largest magnitude value that a CPID may obtain.
    //!
    static constexpr uint16_t MAX = 32767;

    //!
    //! \brief The smallest magnitude value that the network can represent as
    //! a non-zero magnitude is just greater than this value.
    //!
    //! Note: magnitudes less than 0.01 are rounded-up to 0.01.
    //!
    static constexpr double ZERO_THRESHOLD = 0.005;

    //!
    //! \brief The largest possible magnitude stored with two fractional places
    //! is just less than this value.
    //!
    static constexpr double MIN_MEDIUM = 0.995;

    //!
    //! \brief The largest possible magnitude stored with one fractional place
    //! is just less than this value.
    //!
    static constexpr double MIN_LARGE = 9.95;

    //!
    //! \brief Factor by which a canonical magnitude is scaled for storage in
    //! this type.
    //!
    static constexpr size_t SCALE_FACTOR = 100;

    //!
    //! \brief Factor by which a magnitude with a value less than 10 is scaled
    //! from compact storage.
    //!
    static constexpr size_t MEDIUM_SCALE_FACTOR = 10;

    //!
    //! \brief Factor by which a magnitude with a value less than 1 is scaled
    //! from compact storage.
    //!
    static constexpr size_t SMALL_SCALE_FACTOR = 1;

    //!
    //! \brief Describes the size of the magnitude value for the purpose of
    //! assigning the number of fractional places.
    //!
    enum class Kind
    {
        ZERO,   //!< A zero magnitude.
        SMALL,  //!< Less than 1 with two fractional places.
        MEDIUM, //!< 1 or greater and less than 10 with one fractional place.
        LARGE,  //!< 10 or greater with no fractional places.
    };

    //!
    //! \brief Initialize a magnitude with a value of zero.
    //!
    //! \return A magnitude with a value of zero.
    //!
    static Magnitude Zero()
    {
        return Magnitude(0);
    }

    //!
    //! \brief Initialize a magnitude directly from its scaled representation.
    //!
    //! \param scaled A magnitude value scaled by a factor of 100.
    //!
    //! \return The wrapped magnitude value.
    //!
    static Magnitude FromScaled(const uint32_t scaled)
    {
        return Magnitude(scaled);
    }

    //!
    //! \brief Initialize a magnitude from a floating-point number.
    //!
    //! \param value An unscaled magnitude.
    //!
    //! \return The wrapped magnitude value appropriately rounded and scaled.
    //!
    static Magnitude RoundFrom(const double value)
    {
        if (value <= ZERO_THRESHOLD || value > MAX) {
            return Zero();
        }

        if (value < MIN_MEDIUM) {
            return Magnitude(RoundAndScale(value, SMALL_SCALE_FACTOR));
        }

        if (value < MIN_LARGE) {
            return Magnitude(RoundAndScale(value, MEDIUM_SCALE_FACTOR));
        }

        return Magnitude(RoundAndScale(value, SCALE_FACTOR));
    }

    bool operator==(const Magnitude other) const
    {
        return m_scaled == other.m_scaled;
    }

    bool operator!=(const Magnitude other) const
    {
        return m_scaled != other.m_scaled;
    }

    bool operator==(const int64_t other) const
    {
        return static_cast<int64_t>(m_scaled) == other * SCALE_FACTOR;
    }

    bool operator!=(const int64_t other) const
    {
        return !(*this == other);
    }

    bool operator==(const int32_t other) const
    {
        return *this == static_cast<int64_t>(other);
    }

    bool operator!=(const int32_t other) const
    {
        return !(*this == other);
    }

    bool operator==(const double other) const
    {
        return *this == RoundFrom(other);
    }

    bool operator!=(const double other) const
    {
        return !(*this == other);
    }

    //!
    //! \brief Describe the size of the magnitude value for the purpose of
    //! categorizing it for serialization.
    //!
    //! \return A value that represents the size of the magnitude.
    //!
    Kind Which() const
    {
        if (m_scaled == 0) {
            return Kind::ZERO;
        }

        if (m_scaled < (MIN_MEDIUM * SCALE_FACTOR)) {
            return Kind::SMALL;
        }

        if (m_scaled < (MIN_LARGE * SCALE_FACTOR)) {
            return Kind::MEDIUM;
        }

        return Kind::LARGE;
    }

    //!
    //! \brief Get the scaled representation of the magnitude.
    //!
    //! Use the scaled value instead of the floating-point representation when
    //! appropriate to avoid floating-point errors.
    //!
    //! \return Magnitude scaled by a factor of 100.
    //!
    uint32_t Scaled() const
    {
        return m_scaled;
    }

    //!
    //! \brief Get the compact representation of the magnitude.
    //!
    //! This produces an integer value that matches the format of the magnitude
    //! as a superblock stores it.
    //!
    //! \return Magnitude scaled-down as needed for its size category.
    //!
    uint16_t Compact() const
    {
        switch (Which()) {
            case Kind::ZERO:   return 0;
            case Kind::SMALL:  return m_scaled / SMALL_SCALE_FACTOR;
            case Kind::MEDIUM: return m_scaled / MEDIUM_SCALE_FACTOR;
            case Kind::LARGE:  return m_scaled / SCALE_FACTOR;
        }

        return 0;
    }

    //!
    //! \brief Get the floating-point representation of the magnitude.
    //!
    //! Use the scaled value instead of the floating-point representation when
    //! appropriate to avoid floating-point errors.
    //!
    //! \return Floating-point magnitude at canonical scale.
    //!
    double Floating() const
    {
        return static_cast<double>(m_scaled) / SCALE_FACTOR;
    }

    //!
    //! \brief Get the string representation of the magnitude.
    //!
    //! \return The magnitude's floating-point representation as a string.
    //!
    std::string ToString() const
    {
        return strprintf("%g", Floating());
    }

private:
    uint32_t m_scaled; //!< Magnitude scaled by a factor of 100.

    //!
    //! \brief Initialize a magnitude with a value of zero.
    //!
    Magnitude() : Magnitude(0)
    {
    }

    //!
    //! \brief Initialize a magnitude directly from its scaled representation.
    //!
    //! \param scaled A magnitude value scaled by a factor of 100.
    //!
    explicit Magnitude(const uint32_t scaled) : m_scaled(scaled)
    {
    }

    //!
    //! \brief Scale a floating-point magnitude by the specified factor and
    //! round the result.
    //!
    //! \param magnitude An unscaled magnitude.
    //! \param scale     Scale factor of the magnitude's size category.
    //!
    //! \return The scaled integer representation of the rounded magnitude.
    //!
    static uint32_t RoundAndScale(const double magnitude, const double scale)
    {
        return std::nearbyint(magnitude * (SCALE_FACTOR / scale)) * scale;
    }
}; // Magnitude
}
