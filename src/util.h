// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include "arith_uint256.h"
#include "uint256.h"
#include "fwd.h"
#include "hash.h"

#include <memory>
#include <numeric>
#include <utility>
#include <map>
#include <vector>
#include <string>
#include <locale>
#include <strings.h>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

// After merging some more of Bitcoin's utilities, we can split them out
// of this file to reduce the header load:
#include <util/strencodings.h>
#include "util/system.h"
#include "util/string.h"

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

// to obtain PRId64 on some old systems
#define __STDC_FORMAT_MACROS 1

#include <stdint.h>
#include <inttypes.h>

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define UBEGIN(a)           ((unsigned char*)&(a))
#define UEND(a)             ((unsigned char*)&((&(a))[1]))

#define UVOIDBEGIN(a)        ((void*)&(a))
#define CVOIDBEGIN(a)        ((const void*)&(a))
#define UINTBEGIN(a)        ((uint32_t*)&(a))
#define CUINTBEGIN(a)        ((const uint32_t*)&(a))

/* Format characters for (s)size_t and ptrdiff_t (C99 standard) */
#define PRIszx    "zx"
#define PRIszu    "zu"
#define PRIszd    "zd"
#define PRIpdx    "tx"
#define PRIpdu    "tu"
#define PRIpdd    "td"


#ifdef WIN32
#define MSG_NOSIGNAL        0
#define MSG_DONTWAIT        0

#ifndef S_IRUSR
#define S_IRUSR             0400
#define S_IWUSR             0200
#endif
#else
#define MAX_PATH            1024
#endif

//void MilliSleep(int64_t n);

extern int GetDayOfYear(int64_t timestamp);

extern bool fPrintToConsole;
extern bool fRequestShutdown;
extern std::atomic<bool> fShutdown;
extern bool fCommandLine;
extern bool fTestNet;
extern bool fNoListen;
extern bool fLogTimestamps;
extern bool fReopenDebugLog;
extern bool fDevbuildCripple;

void LogException(std::exception* pex, const char* pszThread);
void PrintException(std::exception* pex, const char* pszThread);
void PrintExceptionContinue(std::exception* pex, const char* pszThread);
std::string FormatMoney(int64_t n, bool fPlus=false);
bool ParseMoney(const std::string& str, int64_t& nRet);
bool ParseMoney(const char* pszIn, int64_t& nRet);
bool WildcardMatch(const char* psz, const char* mask);
bool WildcardMatch(const std::string& str, const std::string& mask);
bool TryCreateDirectories(const fs::path& p);

std::string TimestampToHRDate(double dtm);

bool DirIsWritable(const fs::path& directory);
bool LockDirectory(const fs::path& directory, const std::string lockfile_name, bool probe_only=false);
bool TryCreateDirectories(const fs::path& p);

//!
//! \brief Read the contents of the specified file into memory.
//!
//! \param filepath The path to the file. Provide absolute paths when possible.
//!
//! \return The file contents as a string.
//!
std::string GetFileContents(const fs::path filepath);

int64_t GetTimeOffset();
int64_t GetAdjustedTime();
void AddTimeData(const CNetAddr& ip, int64_t nOffsetSample);
#if HAVE_SYSTEM
void runCommand(std::string strCommand);
#endif

//!
//! \brief Round double value to N decimal places.
//! \param d Value to round.
//! \param place Number of decimal places.
//!
double Round(double d, int place);

//!
//! \brief Round a double value and convert it to a string.
//! \param d Value to round.
//! \param place Number of decimal places.
//! \note This always produces an output with dot as decimal separator.
//!
std::string RoundToString(double d, int place);

//!
//! \brief Round a double value contained in a string.
//!
//! Does \c atof on \p s and rounds the result.
//!
//! \returns \p s represented as a double rounded to \p place decimals.
//!
double RoundFromString(const std::string& s, int place);

bool Contains(const std::string& data, const std::string& instring);
std::vector<std::string> split(const std::string& s, const std::string& delim);

inline int roundint(double d)
{
    return (int)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t abs64(int64_t n)
{
    return (n >= 0 ? n : -n);
}

//!
//! \brief Class to represent fractions and common fraction operations with built in simplification. This supports integer operations
//! for consensus critical code where floating point would cause problems across different architectures and/or compiler
//! implementations.
//!
//! In particular this class is used for sidestake allocations, both the allocation "percentage", and the CAmount allocations
//! resulting from muliplying the allocation (fraction) times the CAmount rewards.
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

    Fraction operator=(const Fraction& rhs)
    {
        m_numerator = rhs.GetNumerator();
        m_denominator = rhs.GetDenominator();

        return *this;
    }

    bool operator!()
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

        // Otherwise do the full pattern of getting a common denominator and adding, then simplify...
        return Fraction(overflow_add(overflow_mult(slhs.GetNumerator(), srhs.GetDenominator()),
                                     overflow_mult(slhs.GetDenominator(), srhs.GetNumerator())),
                        overflow_mult(slhs.GetDenominator(), srhs.GetDenominator()),
                        true);
    }

    Fraction operator+(const int64_t& rhs) const
    {
        Fraction slhs(*this, true);

        return Fraction(overflow_add(slhs.GetNumerator(), overflow_mult(slhs.GetDenominator(), rhs)), slhs.GetDenominator(), true);
    }

    Fraction operator-(const Fraction& rhs) const
    {
        return (*this + Fraction(-rhs.GetNumerator(), rhs.GetDenominator()));
    }

    Fraction operator-(const int64_t& rhs) const
    {
        return (*this + -rhs);
    }

    Fraction operator*(const Fraction& rhs) const
    {
        Fraction slhs(*this, true);
        Fraction srhs(rhs, true);

        return Fraction(overflow_mult(slhs.GetNumerator(), srhs.GetNumerator()),
                        overflow_mult(slhs.GetDenominator(), srhs.GetDenominator()),
                        true);
    }

    Fraction operator*(const int64_t& rhs) const
    {
        Fraction slhs(*this, true);

        return Fraction(overflow_mult(slhs.GetNumerator(), rhs), slhs.GetDenominator(), true);
    }

    Fraction operator/(const Fraction& rhs) const
    {
        return (*this * Fraction(rhs.GetDenominator(), rhs.GetNumerator()));
    }

    Fraction operator/(const int64_t& rhs) const
    {
        Fraction slhs(*this, true);

        return Fraction(slhs.GetNumerator(), overflow_mult(slhs.GetDenominator(), rhs), true);
    }

    Fraction operator+=(const Fraction& rhs)
    {
        Simplify();

        *this = *this + rhs;

        return *this;
    }

    Fraction operator+=(const int64_t& rhs)
    {
        Simplify();

        *this = *this + rhs;

        return *this;
    }

    Fraction operator-=(const Fraction& rhs)
    {
        Simplify();

        *this = *this - rhs;

        return *this;
    }

    Fraction operator-=(const int64_t& rhs)
    {
        Simplify();

        *this = *this - rhs;

        return *this;
    }

    Fraction operator*=(const Fraction& rhs)
    {
        Simplify();

        *this = *this * rhs;

        return *this;
    }

    Fraction operator*=(const int64_t& rhs)
    {
        Simplify();

        *this = *this * rhs;

        return *this;
    }

    Fraction operator/=(const Fraction& rhs)
    {
        Simplify();

        *this = *this / rhs;

        return *this;
    }

    Fraction operator/=(const int64_t& rhs)
    {
        Simplify();

        *this = *this / rhs;

        return *this;
    }

    bool operator==(const Fraction& rhs) const
    {
        Fraction slhs(*this, true);
        Fraction srhs(rhs, true);

        return (slhs.GetNumerator() == srhs.GetNumerator() && slhs.GetDenominator() == slhs.GetDenominator());
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
    }

private:
    int msb(const int64_t& n) const
    {
        // Log2 is O(1) both time and space-wise and so is the best choice here.
        return (static_cast<int>(floor(log2(std::abs(n)))));
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
            if (a >= std::numeric_limits<int64_t>::min() - b) {
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

inline std::string leftTrim(std::string src, char chr)
{
    std::string::size_type pos = src.find_first_not_of(chr, 0);

    if(pos > 0)
        src.erase(0, pos);

    return src;
}

/** Median filter over a stream of values.
 * Returns the median of the last N numbers
 */
template <typename T> class CMedianFilter
{
private:
    std::vector<T> vValues;
    std::vector<T> vSorted;
    unsigned int nSize;
public:
    CMedianFilter(unsigned int size, T initial_value):
        nSize(size)
    {
        vValues.reserve(size);
        vValues.push_back(initial_value);
        vSorted = vValues;
    }

    void input(T value)
    {
        if(vValues.size() == nSize)
        {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const
    {
        int size = vSorted.size();
        assert(size>0);
        if(size & 1) // Odd number of elements
        {
            return vSorted[size/2];
        }
        else // Even number of elements
        {
            return (vSorted[size/2-1] + vSorted[size/2]) / 2;
        }
    }

    int size() const
    {
        return vValues.size();
    }

    std::vector<T> sorted () const
    {
        return vSorted;
    }
};

bool NewThread(void(*pfn)(void*), void* parg);
void RenameThread(const char* name);

class ThreadHandler
{
public:
    ThreadHandler(){};
    bool createThread(void(*pfn)(ThreadHandlerPtr), ThreadHandlerPtr parg, const std::string tname);
    bool createThread(void(*pfn)(void*), void* parg, const std::string tname);
    int numThreads();
    bool threadExists(const std::string tname);
    void interruptAll();
    void removeAll();
    void removeByName(const std::string tname);
private:
    boost::thread_group threadGroup;
    std::map<std::string, boost::thread*> threadMap;
};


/**
 * .. A wrapper that just calls func once
 */
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    RenameThread(name);
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("%s thread interrupt\n", name);
        throw;
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...) {
        PrintExceptionContinue(nullptr, name);
        throw;
    }
}

#endif
