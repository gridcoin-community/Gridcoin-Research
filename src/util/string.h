// Copyright (c) 2019-2020 The Bitcoin Core developers
// Copyright (c) 2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_STRING_H
#define BITCOIN_UTIL_STRING_H

#include <attributes.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

void ParseString(const std::string& str, char c, std::vector<std::string>& v);

[[nodiscard]] inline std::string TrimString(const std::string& str, const std::string& pattern = " \f\n\r\t\v")
{
    std::string::size_type front = str.find_first_not_of(pattern);
    if (front == std::string::npos) {
        return std::string();
    }
    std::string::size_type end = str.find_last_not_of(pattern);
    return str.substr(front, end - front + 1);
}

[[nodiscard]] inline std::string RemovePrefix(const std::string& str, const std::string& prefix)
{
    if (str.substr(0, prefix.size()) == prefix) {
        return str.substr(prefix.size());
    }
    return str;
}

/**
 * Join a list of items
 *
 * @param list       The list to join
 * @param separator  The separator
 * @param unary_op   Apply this operator to each item in the list
 */
template <typename T, typename BaseType, typename UnaryOp>
auto Join(const std::vector<T>& list, const BaseType& separator, UnaryOp unary_op)
    -> decltype(unary_op(list.at(0)))
{
    decltype(unary_op(list.at(0))) ret;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) ret += separator;
        ret += unary_op(list.at(i));
    }
    return ret;
}

template <typename T>
T Join(const std::vector<T>& list, const T& separator)
{
    return Join(list, separator, [](const T& i) { return i; });
}

// Explicit overload needed for c_str arguments, which would otherwise cause a substitution failure in the template above.
inline std::string Join(const std::vector<std::string>& list, const std::string& separator)
{
    return Join<std::string>(list, separator);
}

/**
 * Check if a string does not contain any embedded NUL (\0) characters
 */
[[nodiscard]] inline bool ValidAsCString(const std::string& str) noexcept
{
    return str.size() == strlen(str.c_str());
}

/**
 * Locale-independent version of std::to_string
 */
template <typename T>
std::string ToString(const T& t)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << t;
    return oss.str();
}

/**
 * @brief Locale-independent version of ToString specifically for doubles with
 * the ability to specificy the precision.
 *
 * @param t Input double floating point
 * @param precision
 * @return string
 */
std::string FromDoubleToString(const double& t, const int& precision);

/**
 * Check whether a container begins with the given prefix.
 */
template <typename T1, size_t PREFIX_LEN>
[[nodiscard]] inline bool HasPrefix(const T1& obj,
                                const std::array<uint8_t, PREFIX_LEN>& prefix)
{
    return obj.size() >= PREFIX_LEN &&
           std::equal(std::begin(prefix), std::end(prefix), std::begin(obj));
}

#endif // BITCOIN_UTIL_STRENCODINGS_H
