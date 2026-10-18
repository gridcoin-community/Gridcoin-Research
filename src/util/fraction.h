// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_FRACTION_H
#define GRIDCOIN_UTIL_FRACTION_H

#include <serialize.h>
#include <tinyformat.h>
#include <util/strencodings.h>

#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

// Declared in util.h, which includes this header.
std::vector<std::string> split(const std::string& s, const std::string& delim);

//!
//! \brief Class to represent fractions and common fraction operations with built in simplification. This supports integer operations
//! for consensus critical code where floating point would cause problems across different architectures and/or compiler
//! implementations.
//!
//! In particular this class is used for sidestake allocations, both the allocation "percentage", and the CAmount allocations
//! resulting from multiplying the allocation (fraction) times the CAmount rewards.
//!
class Fraction {
public:
    //!
    //! \brief Trivial zero fraction constructor
    //!
    Fraction()
        : m_numerator(0)
        , m_denominator(1)
        , m_simplified(true)
    {}

    //!
    //! \brief Copy constructor
    //!
    //! \param Fraction f
    //!
    Fraction(const Fraction& f)
        : Fraction(f.GetNumerator(), f.GetDenominator())
    {}

    //!
    //! \brief Constructor with simplification boolean directive
    //!
    //! \param Fraction f
    //! \param boolean simplify
    //!
    Fraction(const Fraction& f, const bool& simplify)
        : Fraction(f.GetNumerator(), f.GetDenominator(), simplify)
    {}

    //!
    //! \brief Constructor from numerator and denominator
    //!
    //! \param in64t_t numerator
    //! \param int64_t denominator
    //!
    Fraction(const int64_t& numerator,
             const int64_t& denominator)
        : m_numerator(numerator)
        , m_denominator(denominator)
        , m_simplified(false)
    {
        if (m_denominator == 0) {
            throw std::out_of_range("denominator specified is zero");
        }

        // std::gcd and std::abs produce undefined behavior for INT64_MIN
        // because |INT64_MIN| is not representable as int64_t.
        if (m_numerator == std::numeric_limits<int64_t>::min()
            || m_denominator == std::numeric_limits<int64_t>::min()) {
            throw std::out_of_range("INT64_MIN is not supported (std::abs overflow)");
        }

        if (std::gcd(m_numerator, m_denominator) == 1 && m_denominator > 0) {
            m_simplified = true;
        }
    }

    //!
    //! \brief Constructor from numerator and denominator with simplification boolean directive
    //!
    //! \param int64_t numerator
    //! \param int64_t denominator
    //! \param boolean simplify
    //!
    Fraction(const int64_t& numerator,
             const int64_t& denominator,
             const bool& simplify)
        : Fraction(numerator, denominator)
    {
        if (!m_simplified && simplify) {
            Simplify();
        }
    }

    ~Fraction()
    {}

    //!
    //! \brief Constructor from input int64_t integer (i.e. denominator = 1).
    //!
    //! \param numerator
    //!
    Fraction(const int64_t& numerator)
        : Fraction(numerator, 1)
    {}

    bool IsZero() const
    {
        // The denominator cannot be zero by construction rules.
        return m_numerator == 0;
    }

    bool IsNonZero() const
    {
        return !IsZero();
    }

    bool IsPositive() const
    {
        return (m_denominator > 0 && m_numerator > 0) || (m_denominator < 0 && m_numerator < 0);
    }

    bool IsNonNegative() const
    {
        return IsPositive() || IsZero();
    }

    bool IsNegative() const
    {
        return !IsNonNegative();
    }

    constexpr int64_t GetNumerator() const
    {
        return m_numerator;
    }

    constexpr int64_t GetDenominator() const
    {
        return m_denominator;
    }

    bool IsSimplified() const
    {
        return m_simplified;
    }

    void Simplify()
    {
        // Check whether already simplified, if so, nothing to do.
        if (m_simplified) {
            return;
        }

        // Nice that we are at C++17! :)
        int64_t gcd = std::gcd(m_numerator, m_denominator);

        // If both numerator and denominator are negative,
        // change the sign of gcd to flip both to positive.
        if (m_numerator < 0 && m_denominator < 0) {
            gcd = -gcd;
        }

        m_numerator = m_numerator / gcd;
        m_denominator = m_denominator / gcd;

        // Since the case where both are less than zero has already been changed to +/+,
        // If we have m_denominator < 0, we must have m_numerator >= 0. So move the negative
        // sign to the numerator and make the denominator positive. This simplifies the equality
        // comparison.
        if (m_denominator < 0) {
            m_denominator = -m_denominator;
            m_numerator = -m_numerator;
        }

        m_simplified = true;
    }

    double ToDouble() const
    {
        return (double) m_numerator / (double) m_denominator;
    }

    Fraction& operator=(const Fraction& rhs)
    {
        m_numerator = rhs.GetNumerator();
        m_denominator = rhs.GetDenominator();
        m_simplified = rhs.IsSimplified();

        return *this;
    }

    std::string ToString() const
    {
        return strprintf("%" PRId64 "/" "%" PRId64, m_numerator, m_denominator);
    }

    static Fraction FromString(const std::string& string)
    {
        std::vector<std::string> string_fraction = split(string, "/");

        if (string_fraction.size() > 2 || string_fraction.size() < 1) {
            throw std::out_of_range("fraction input string cannot be parsed to fraction");
        }

        int64_t numerator;
        int64_t denominator;

        if (string_fraction.size() == 1) {
            if (!ParseInt64(string_fraction[0], &numerator)) {
                throw std::out_of_range("fraction input string cannot be parsed to fraction");
            }

            denominator = 1;
        } else {
            // There must be two elements to the string fraction if we are here.
            if (!ParseInt64(string_fraction[0], &numerator)
                || !ParseInt64(string_fraction[1], &denominator)) {
                throw std::out_of_range("fraction input string cannot be parsed to fraction");
            }
        }

        return Fraction(numerator, denominator, true);
    }

    bool operator!() const
    {
        return IsZero();
    }

    Fraction operator+(const Fraction& rhs) const
    {
        Fraction slhs(*this, true);
        Fraction srhs(rhs, true);

        // If the same denominator (and remember these are already reduced to simplest form) just add the numerators and put
        // over the common denominator...
        if (slhs.GetDenominator() == srhs.GetDenominator()) {
            return Fraction(overflow_add(slhs.GetNumerator(), srhs.GetNumerator()), slhs.GetDenominator(), true);
        }

        // Now the more complex case. In general, fraction addition follows this pattern:
        //
        // a   c                    a * (d/g) + c * (b/g)
        // - + - , g = gcd(b, d) => --------------------- where {(b/g), (d/g)} will be elements of the counting numbers.
        // b   d                      g * (b/g) * (d/g)
        //
        // (b/g) and (d/g) are divisible with no remainders precisely because of the definition of gcd.
        //
        // We have already covered the trivial common denominator case above before bothering to compute the gcd of the
        // denominator.
        int64_t denom_gcd = std::gcd(slhs.GetDenominator(), srhs.GetDenominator());

        // We have two special cases. One is where g = b (i.e. d is actually a multiple of b). In this case,
        // the expression simplifies to
        //
        // a * (d/b) + c
        // -------------
        //       d
        if (denom_gcd == slhs.GetDenominator()) {
            return Fraction(overflow_add(overflow_mult(slhs.GetNumerator(), srhs.GetDenominator() / slhs.GetDenominator()),
                                         srhs.GetNumerator()),
                            srhs.GetDenominator(),
                            true);
        }

        // The other is where g = d (i.e. b is actually a multiple of d). In this case,
        // the expression simplifies to
        //
        // a + c * (b/d)
        // -------------
        //       b
        if (denom_gcd == srhs.GetDenominator()) {
            return Fraction(overflow_add(overflow_mult(srhs.GetNumerator(), slhs.GetDenominator() / srhs.GetDenominator()),
                                         slhs.GetNumerator()),
                            slhs.GetDenominator(),
                            true);
        }

        // Otherwise do the full pattern of getting a common denominator (pulling out the gcd of the denominators),
        // and adding, then simplify...
        //
        // This approach is more complex than
        //
        // a * d + c * b
        // -------------
        //     b * d
        //
        // but has the advantage of being more resistant to overflows, especially when the two denominators are related by a large
        // gcd. In particular in Gridcoin's application with Allocations, the largest denominator of the allocations is 10000, so
        // every allocation denominator in reduced form must be divisible evenly into 10000. This means the majority of fraction
        // additions will be the two simpler cases above.
        return Fraction(overflow_add(overflow_mult(slhs.GetNumerator(), srhs.GetDenominator() / denom_gcd),
                                     overflow_mult(srhs.GetNumerator(), slhs.GetDenominator() / denom_gcd)),
                        overflow_mult(denom_gcd, overflow_mult(slhs.GetDenominator() / denom_gcd, srhs.GetDenominator() / denom_gcd)),
                        true);
    }

    Fraction operator+(const int64_t& rhs) const
    {
        // gcd(denominator, 1) = 1 takes the fraction sum's fast path to the same
        // (a + rhs b) / b; routed the same way as the other integer operands.
        return *this + Fraction(rhs);
    }

    Fraction operator-(const Fraction& rhs) const
    {
        return (*this + Fraction(-rhs.GetNumerator(), rhs.GetDenominator()));
    }

    Fraction operator-(const int64_t& rhs) const
    {
        // Fraction(rhs) refuses INT64_MIN; negating it here was signed overflow.
        return *this - Fraction(rhs);
    }

    Fraction operator*(const Fraction& rhs) const
    {
        Fraction slhs(*this, true);
        Fraction srhs(rhs, true);

        // Gcd's can be used in multiplication for better overflow resistance as well.
        //
        // Consider
        // a   c
        // - * -, where a/b and c/d are already simplified (i.e. gcd(a, b) = gcd(c, d) = 1.
        // b   d
        //
        // We can have g = gcd(a, d) and h = gcd(c, b), which is with the numerators reversed, since multiplication is
        // commutative. This means we have
        //
        // (c / h)   (a / g)
        // ------- * ------- .
        // (b / h)   (d / g)
        //
        // If we form Fraction(c, b, true) and Fraction(a, d, true), the simplification will determine and divide the numerator and
        // denominator by h and g respectively.
        //
        // A specific example is instructive.
        //
        // 1998   1000    999   1000   1000   999   1   1
        // ---- * ---- = ---- * ---- = ---- * --- = - * -
        // 2000    999   1000    999   1000   999   1   1
        //
        // This is a formal form of what grade school teachers called factor cancellation. :).

        Fraction sxlhs(srhs.GetNumerator(), slhs.GetDenominator(), true);
        Fraction sxrhs(slhs.GetNumerator(), srhs.GetDenominator(), true);

        return Fraction(overflow_mult(sxlhs.GetNumerator(), sxrhs.GetNumerator()),
                        overflow_mult(sxlhs.GetDenominator(), sxrhs.GetDenominator()),
                        true);
    }

    Fraction operator*(const int64_t& rhs) const
    {
        Fraction slhs(*this, true);
        Fraction srhs(rhs);

        // The fraction multiply with rhs over 1, written out so that no unit factor goes through the overflow guard:
        // gcd(a, 1) = 1 leaves h = gcd(rhs, b) as the only cancellation, so an allocation times a reward, or a magnitude
        // unit times a constant, only forms the product its reduced result needs. Fraction(rhs) refuses INT64_MIN.
        int64_t h = std::gcd(srhs.GetNumerator(), slhs.GetDenominator());

        return Fraction(overflow_mult(slhs.GetNumerator(), srhs.GetNumerator() / h), slhs.GetDenominator() / h, true);
    }

    Fraction operator/(const Fraction& rhs) const
    {
        return (*this * Fraction(rhs.GetDenominator(), rhs.GetNumerator()));
    }

    Fraction operator/(const int64_t& rhs) const
    {
        Fraction slhs(*this, true);
        Fraction srhs(rhs);

        if (srhs.IsZero()) {
            throw std::out_of_range("denominator specified is zero");
        }

        // a/b times 1/rhs, written out the same way: g = gcd(a, rhs) is the only cancellation.
        int64_t g = std::gcd(slhs.GetNumerator(), srhs.GetNumerator());

        return Fraction(slhs.GetNumerator() / g, overflow_mult(slhs.GetDenominator(), srhs.GetNumerator() / g), true);
    }

    Fraction& operator+=(const Fraction& rhs)
    {
        Simplify();

        *this = *this + rhs;

        return *this;
    }

    Fraction& operator+=(const int64_t& rhs)
    {
        Simplify();

        *this = *this + rhs;

        return *this;
    }

    Fraction& operator-=(const Fraction& rhs)
    {
        Simplify();

        *this = *this - rhs;

        return *this;
    }

    Fraction& operator-=(const int64_t& rhs)
    {
        Simplify();

        *this = *this - rhs;

        return *this;
    }

    Fraction& operator*=(const Fraction& rhs)
    {
        Simplify();

        *this = *this * rhs;

        return *this;
    }

    Fraction& operator*=(const int64_t& rhs)
    {
        Simplify();

        *this = *this * rhs;

        return *this;
    }

    Fraction& operator/=(const Fraction& rhs)
    {
        Simplify();

        *this = *this / rhs;

        return *this;
    }

    Fraction& operator/=(const int64_t& rhs)
    {
        Simplify();

        *this = *this / rhs;

        return *this;
    }

    bool operator==(const Fraction& rhs) const
    {
        Fraction slhs(*this, true);
        Fraction srhs(rhs, true);

        return (slhs.GetNumerator() == srhs.GetNumerator() && slhs.GetDenominator() == srhs.GetDenominator());
    }

    bool operator!=(const Fraction& rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<=(const Fraction& rhs) const
    {
        return (rhs - *this).IsNonNegative();
    }

    bool operator>=(const Fraction& rhs) const
    {
        return (*this - rhs).IsNonNegative();
    }

    bool operator<(const Fraction& rhs) const
    {
        return (rhs - *this).IsPositive();
    }

    bool operator>(const Fraction& rhs) const
    {
        return (*this - rhs).IsPositive();
    }

    bool operator==(const int64_t& rhs) const
    {
        return (*this == Fraction(rhs));
    }

    bool operator!=(const int64_t& rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<=(const int64_t& rhs) const
    {
        return *this <= Fraction(rhs);
    }

    bool operator>=(const int64_t& rhs) const
    {
        return *this >= Fraction(rhs);
    }

    bool operator<(const int64_t& rhs) const
    {
        return *this < Fraction(rhs);
    }

    bool operator>(const int64_t& rhs) const
    {
        return *this > Fraction(rhs);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_numerator);
        READWRITE(m_denominator);
        READWRITE(m_simplified);

        if (ser_action.ForRead()) {
            if (m_denominator == 0) {
                throw std::ios_base::failure("deserialized Fraction has zero denominator");
            }
            if (m_numerator == std::numeric_limits<int64_t>::min()
                || m_denominator == std::numeric_limits<int64_t>::min()) {
                throw std::ios_base::failure("deserialized Fraction has INT64_MIN value");
            }
        }
    }

private:
    int msb(const int64_t& n) const
    {
        // std::abs(INT64_MIN) is undefined behavior (two's complement
        // asymmetry). |INT64_MIN| = 2^63, so msb = 64.
        if (n == std::numeric_limits<int64_t>::min()) {
            return 64;
        }

        int64_t abs_n = std::abs(n);

        int index = 0;

        for (; index <= 63; ++index) {
            if (abs_n >> index == 0) {
                break;
            }
        }

        return index;
    }

    int64_t overflow_mult(const int64_t& a, const int64_t& b) const
    {
        if (a == 0 || b == 0) {
            return 0;
        }

        // A 64-bit integer with the lower 32 bits filled has value 2^32 - 1. Multiplying two of these, a * b, together
        // is (2^32 - 1) * (2^32 - 1) = 2^64 - 2^33 + 1 > 2^63. Log2(2^63) = msb(a) + msb(b) - 1. So a quick overflow limit...

        if (msb(a) + msb(b) > 63) {
            throw std::overflow_error("fraction multiplication results in an overflow");
        }

        return a * b;
    }

    int64_t overflow_add(const int64_t& a, const int64_t& b) const
    {
        if (a == 0) {
            return b;
        }

        if (b == 0) {
            return a;
        }

        if (a > 0 && b > 0) {
            if (a <= std::numeric_limits<int64_t>::max() - b) {
                return a + b;
            } else {
                throw std::overflow_error("fraction addition of a + b where a > 0 and b > 0 results in an overflow");
            }
        }

        if (a < 0 && b < 0) {
            // Remember b is negative here, so the difference below is GREATER than std::numeric_limits<int64_t>::min().
            //
            // The reason for the + 1 below is that the first case handled in the addition overflow operator method above,
            // the fraction addition with a common denominator, also specifies simplification of the resultant fraction.
            // This will call std::gcd on the numerator and denominator, which then calls std::abs. If std::abs is called on
            // std::numeric_limits<int64_t>::min(), and -D_GLIBCXX_ASSERTIONS is set, then the program will abort on a glibc
            // assertion, because the abs will overflow. To prevent this, we increase (make less negative) by 1, which will
            // ensure the call to std::abs will succeed at the extreme case.
            if (a >= std::numeric_limits<int64_t>::min() + 1 - b) {
                return a + b;
            } else {
                throw std::overflow_error("fraction addition of a + b where a < 0 and b < 0 results in an overflow");
            }
        }

        // The only thing left is that a and b are opposite in sign, so addition cannot overflow.
        return a + b;
    }

    int64_t m_numerator;
    int64_t m_denominator;
    bool m_simplified;
};

#endif // GRIDCOIN_UTIL_FRACTION_H
