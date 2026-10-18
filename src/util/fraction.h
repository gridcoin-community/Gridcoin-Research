// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_FRACTION_H
#define GRIDCOIN_UTIL_FRACTION_H

#include <serialize.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/string.h>

#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

//!
//! \brief Exact rational arithmetic on int64_t.
//!
//! Consensus code multiplies rewards by sidestake allocations, the magnitude
//! unit and fee fractions. Floating point would make those results depend on
//! the compiler, the architecture and the math library, so this class keeps
//! every value as an exact ratio of two int64_t and confines floating point to
//! construction from a double (GRC::Allocation) and to display (ToDouble()).
//!
//! Contract. Every operation either returns the exact rational result or
//! throws. It never returns a wrong value.
//!
//!  - std::overflow_error when a numerator or denominator the operation has to
//!    form does not fit int64_t. The multiply guard is conservative, see
//!    overflow_mult(); the addition check is exact.
//!  - std::out_of_range for a zero denominator, for INT64_MIN as a component or
//!    as an integer operand, and for a string that does not parse.
//!
//! INT64_MIN is refused at every entry because its magnitude is not
//! representable, which makes std::gcd and std::abs undefined on it.
//!
//! Representation. A fraction may be held unreduced, so that the pair a caller
//! supplied, or a contract carried, is reported back as given. The constructors
//! and Simplify() set m_simplified from the pair: true exactly when it is in
//! canonical form, lowest terms with a positive denominator and the sign on the
//! numerator. Every operator rebuilds its operands in that form from the pair
//! before computing and returns its result in it, so the flag never decides a
//! value. On the wire the flag travels with the pair and comes back out as it
//! arrived; a byte that disagrees with its pair changes nothing but
//! IsSimplified().
//!
class Fraction
{
public:
    //! Zero, as 0/1.
    Fraction() noexcept
        : m_numerator(0)
        , m_denominator(1)
        , m_simplified(true)
    {}

    Fraction(const Fraction&) = default;
    Fraction& operator=(const Fraction&) = default;

    //! A copy of \p f built from its pair, so the flag is recomputed, and
    //! reduced to canonical form when \p simplify is set.
    Fraction(const Fraction& f, bool simplify)
        : Fraction(f.m_numerator, f.m_denominator, simplify)
    {}

    //! \p numerator / \p denominator, held as given.
    //!
    //! \throws std::out_of_range for a zero denominator or an INT64_MIN component.
    Fraction(int64_t numerator, int64_t denominator)
        : m_numerator(numerator)
        , m_denominator(denominator)
        , m_simplified(false)
    {
        if (m_denominator == 0) {
            throw std::out_of_range("denominator specified is zero");
        }

        if (m_numerator == std::numeric_limits<int64_t>::min()
            || m_denominator == std::numeric_limits<int64_t>::min()) {
            throw std::out_of_range("INT64_MIN is not supported (std::abs overflow)");
        }

        m_simplified = IsCanonical(m_numerator, m_denominator);
    }

    //! \p numerator / \p denominator, reduced to canonical form when \p simplify is set.
    Fraction(int64_t numerator, int64_t denominator, bool simplify)
        : Fraction(numerator, denominator)
    {
        if (simplify) {
            Simplify();
        }
    }

    //! The integer \p numerator, as numerator/1. Implicit on purpose: integers
    //! and fractions mix freely in the reward arithmetic.
    Fraction(int64_t numerator)
        : Fraction(numerator, 1)
    {}

    bool IsZero() const noexcept
    {
        // The denominator cannot be zero by construction.
        return m_numerator == 0;
    }

    bool IsNonZero() const noexcept
    {
        return !IsZero();
    }

    bool IsPositive() const noexcept
    {
        return (m_denominator > 0 && m_numerator > 0) || (m_denominator < 0 && m_numerator < 0);
    }

    bool IsNonNegative() const noexcept
    {
        return IsPositive() || IsZero();
    }

    bool IsNegative() const noexcept
    {
        return !IsNonNegative();
    }

    constexpr int64_t GetNumerator() const noexcept
    {
        return m_numerator;
    }

    constexpr int64_t GetDenominator() const noexcept
    {
        return m_denominator;
    }

    //! The flag as set by the constructors and Simplify(), or as it arrived on
    //! the wire. See the class comment.
    bool IsSimplified() const noexcept
    {
        return m_simplified;
    }

    //! Reduce to canonical form: lowest terms, denominator positive, sign on
    //! the numerator.
    void Simplify()
    {
        if (m_simplified) {
            return;
        }

        int64_t gcd = std::gcd(m_numerator, m_denominator);

        // Both negative: a negative gcd flips both positive in one division.
        if (m_numerator < 0 && m_denominator < 0) {
            gcd = -gcd;
        }

        m_numerator /= gcd;
        m_denominator /= gcd;

        // Only numerator >= 0 with denominator < 0 can remain. Move the sign.
        if (m_denominator < 0) {
            m_denominator = -m_denominator;
            m_numerator = -m_numerator;
        }

        m_simplified = true;
    }

    //! For display only. Not for any value that feeds consensus.
    double ToDouble() const noexcept
    {
        return static_cast<double>(m_numerator) / static_cast<double>(m_denominator);
    }

    std::string ToString() const
    {
        return strprintf("%" PRId64 "/" "%" PRId64, m_numerator, m_denominator);
    }

    //! Parse "n" or "n/d", with n and d as ParseInt64() reads them, into canonical form.
    //!
    //! \throws std::out_of_range when the string does not parse, when d is
    //! zero, or when a component is INT64_MIN.
    static Fraction FromString(const std::string& string)
    {
        const std::vector<std::string> parts = SplitString(string, '/');

        if (parts.size() > 2 || parts.empty()) {
            throw std::out_of_range("fraction input string cannot be parsed to fraction");
        }

        int64_t numerator;
        int64_t denominator = 1;

        if (!ParseInt64(parts[0], &numerator)
            || (parts.size() == 2 && !ParseInt64(parts[1], &denominator))) {
            throw std::out_of_range("fraction input string cannot be parsed to fraction");
        }

        return Fraction(numerator, denominator, true);
    }

    bool operator!() const noexcept
    {
        return IsZero();
    }

    Fraction operator+(const Fraction& rhs) const
    {
        const Fraction a = Canonical();
        const Fraction b = rhs.Canonical();

        // Same denominator: add the numerators.
        if (a.m_denominator == b.m_denominator) {
            return Fraction(overflow_add(a.m_numerator, b.m_numerator), a.m_denominator, true);
        }

        // Otherwise, with g = gcd(b, d),
        //
        //   a   c   a (d/g) + c (b/g)
        //   - + - = -----------------
        //   b   d      g (b/g) (d/g)
        //
        // which is the smallest common denominator the two allow, and so the
        // least exposed to overflow. Allocation denominators all divide 10000,
        // so one usually divides the other and the sum lands on the larger:
        const int64_t g = std::gcd(a.m_denominator, b.m_denominator);

        if (g == a.m_denominator) {
            // d is a multiple of b: (a (d/b) + c) / d
            return Fraction(overflow_add(overflow_mult(a.m_numerator, b.m_denominator / a.m_denominator), b.m_numerator),
                            b.m_denominator,
                            true);
        }

        if (g == b.m_denominator) {
            // b is a multiple of d: (a + c (b/d)) / b
            return Fraction(overflow_add(overflow_mult(b.m_numerator, a.m_denominator / b.m_denominator), a.m_numerator),
                            a.m_denominator,
                            true);
        }

        return Fraction(overflow_add(overflow_mult(a.m_numerator, b.m_denominator / g),
                                     overflow_mult(b.m_numerator, a.m_denominator / g)),
                        overflow_mult(g, overflow_mult(a.m_denominator / g, b.m_denominator / g)),
                        true);
    }

    Fraction operator-(const Fraction& rhs) const
    {
        // The numerator is never INT64_MIN (the constructors and the reader
        // refuse it), so the negation cannot overflow.
        return *this + Fraction(-rhs.m_numerator, rhs.m_denominator);
    }

    Fraction operator*(const Fraction& rhs) const
    {
        const Fraction a = Canonical();
        const Fraction b = rhs.Canonical();

        // Cancel across before multiplying. With a/b and c/d in lowest terms,
        // g = gcd(a, d) and h = gcd(c, b),
        //
        //   a   c   (a/g) (c/h)
        //   - * - = -----------
        //   b   d   (b/h) (d/g)
        //
        // and the right-hand side is already in lowest terms, so a factor
        // product that leaves int64_t here is a result that could not be stored
        // anyway. 1998/2000 * 1000/999 cancels to 1/1 without forming 1998 * 1000.
        const int64_t g = std::gcd(a.m_numerator, b.m_denominator);
        const int64_t h = std::gcd(b.m_numerator, a.m_denominator);

        return Fraction(overflow_mult(a.m_numerator / g, b.m_numerator / h),
                        overflow_mult(a.m_denominator / h, b.m_denominator / g),
                        true);
    }

    Fraction operator/(const Fraction& rhs) const
    {
        return *this * Fraction(rhs.m_denominator, rhs.m_numerator);
    }

    // The integer overloads are the fraction overloads with the integer over 1,
    // and Fraction(rhs) refuses INT64_MIN before any arithmetic. The sum and the
    // difference go through the fraction operators as they are: with b > 1 the
    // sum takes the "b is a multiple of d" branch and forms (a + rhs b) / b, with
    // b = 1 the same-denominator branch. The product and the quotient are written
    // out, because the fraction forms would also push a unit factor through the
    // guard (b/h times 1), which refuses |b/h| >= 2^62 for nothing.

    Fraction operator+(int64_t rhs) const
    {
        return *this + Fraction(rhs);
    }

    Fraction operator-(int64_t rhs) const
    {
        return *this - Fraction(rhs);
    }

    Fraction operator*(int64_t rhs) const
    {
        const Fraction a = Canonical();
        const Fraction r(rhs);

        // gcd(a, 1) = 1 leaves h = gcd(rhs, b) as the only cancellation, so an
        // allocation times a reward, or a magnitude unit times a constant, only
        // forms the product its reduced result needs.
        const int64_t h = std::gcd(r.m_numerator, a.m_denominator);

        return Fraction(overflow_mult(a.m_numerator, r.m_numerator / h), a.m_denominator / h, true);
    }

    Fraction operator/(int64_t rhs) const
    {
        const Fraction a = Canonical();
        const Fraction r(rhs);

        if (r.IsZero()) {
            throw std::out_of_range("denominator specified is zero");
        }

        // a/b times 1/rhs: g = gcd(a, rhs) is the only cancellation.
        const int64_t g = std::gcd(a.m_numerator, r.m_numerator);

        return Fraction(a.m_numerator / g, overflow_mult(a.m_denominator, r.m_numerator / g), true);
    }

    Fraction& operator+=(const Fraction& rhs)
    {
        return *this = *this + rhs;
    }

    Fraction& operator+=(int64_t rhs)
    {
        return *this = *this + rhs;
    }

    Fraction& operator-=(const Fraction& rhs)
    {
        return *this = *this - rhs;
    }

    Fraction& operator-=(int64_t rhs)
    {
        return *this = *this - rhs;
    }

    Fraction& operator*=(const Fraction& rhs)
    {
        return *this = *this * rhs;
    }

    Fraction& operator*=(int64_t rhs)
    {
        return *this = *this * rhs;
    }

    Fraction& operator/=(const Fraction& rhs)
    {
        return *this = *this / rhs;
    }

    Fraction& operator/=(int64_t rhs)
    {
        return *this = *this / rhs;
    }

    bool operator==(const Fraction& rhs) const
    {
        const Fraction a = Canonical();
        const Fraction b = rhs.Canonical();

        return a.m_numerator == b.m_numerator && a.m_denominator == b.m_denominator;
    }

    bool operator!=(const Fraction& rhs) const
    {
        return !(*this == rhs);
    }

    // Ordering goes through subtraction, which cancels before it multiplies.
    // A cross multiplication a d <=> c b would be exposed to overflow where
    // the subtraction is not.

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

    bool operator==(int64_t rhs) const
    {
        return *this == Fraction(rhs);
    }

    bool operator!=(int64_t rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<=(int64_t rhs) const
    {
        return *this <= Fraction(rhs);
    }

    bool operator>=(int64_t rhs) const
    {
        return *this >= Fraction(rhs);
    }

    bool operator<(int64_t rhs) const
    {
        return *this < Fraction(rhs);
    }

    bool operator>(int64_t rhs) const
    {
        return *this > Fraction(rhs);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_numerator);
        READWRITE(m_denominator);
        // Carried as received and written back as held. Transaction hashes are
        // computed from re-serialized members, so a byte that arrived must go
        // back out unchanged; the arithmetic never consults it.
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

    //! The number of bits in |n|: 0 for 0, 63 for INT64_MAX, 64 for INT64_MIN.
    static int BitWidth(int64_t n) noexcept
    {
        // std::abs is undefined on INT64_MIN; |INT64_MIN| = 2^63 takes 64 bits.
        if (n == std::numeric_limits<int64_t>::min()) {
            return 64;
        }

        const uint64_t magnitude = static_cast<uint64_t>(std::abs(n));

        if (magnitude == 0) {
            return 0;
        }

#if defined(__GNUC__)
        static_assert(sizeof(unsigned long long) == sizeof(uint64_t), "clzll must cover 64 bits");
        return 64 - __builtin_clzll(magnitude);
#else
        int width = 0;

        for (uint64_t m = magnitude; m != 0; m >>= 1) {
            ++width;
        }

        return width;
#endif
    }

private:
    //! A copy in canonical form, rebuilt from the pair. The flag is not
    //! consulted, so a wire byte that disagrees with its pair cannot steer a
    //! result.
    Fraction Canonical() const
    {
        return Fraction(m_numerator, m_denominator, true);
    }

    static bool IsCanonical(int64_t numerator, int64_t denominator)
    {
        return denominator > 0 && std::gcd(numerator, denominator) == 1;
    }

    //! \returns a * b.
    //!
    //! \throws std::overflow_error when the bit widths of the factors sum past
    //! 63. |a| < 2^w(a) and |b| < 2^w(b) give |a b| < 2^(w(a) + w(b)), so a sum
    //! of at most 63 proves the product fits. The converse does not hold:
    //! 2^31 * 2^31 = 2^62 fits and is refused. The guard is kept conservative
    //! on purpose. It is a few lines of int64 arithmetic that behave the same
    //! on every compiler and target, which for consensus code is worth more
    //! than the last bit of range, as long as it cannot fire on an input the
    //! chain can carry. The consensus multiplicands stay well inside it: see the
    //! overflow analysis at SnapshotAccrualComputer::MaxReward() and the
    //! Allocation domain in gridcoin/sidestake.h. Changing which products throw
    //! is a consensus change.
    static int64_t overflow_mult(int64_t a, int64_t b)
    {
        if (a == 0 || b == 0) {
            return 0;
        }

        if (BitWidth(a) + BitWidth(b) > 63) {
            throw std::overflow_error("fraction multiplication results in an overflow");
        }

        return a * b;
    }

    //! \returns a + b.
    //!
    //! \throws std::overflow_error when the sum leaves int64_t, or would land
    //! on INT64_MIN.
    static int64_t overflow_add(int64_t a, int64_t b)
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
            }

            throw std::overflow_error("fraction addition of a + b where a > 0 and b > 0 results in an overflow");
        }

        if (a < 0 && b < 0) {
            // The + 1 keeps INT64_MIN itself out of the result. The caller
            // simplifies the sum, which calls std::gcd and so std::abs on it,
            // and std::abs(INT64_MIN) aborts under -D_GLIBCXX_ASSERTIONS and is
            // undefined without them.
            if (a >= std::numeric_limits<int64_t>::min() + 1 - b) {
                return a + b;
            }

            throw std::overflow_error("fraction addition of a + b where a < 0 and b < 0 results in an overflow");
        }

        // Opposite signs cannot overflow.
        return a + b;
    }

    int64_t m_numerator;
    int64_t m_denominator;
    bool m_simplified;
};

#endif // GRIDCOIN_UTIL_FRACTION_H
