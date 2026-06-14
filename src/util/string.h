// Copyright (c) 2019-2020 The Bitcoin Core developers
// Copyright (c) 2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_STRING_H
#define BITCOIN_UTIL_STRING_H

#include <attributes.h>
#include <span.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

void ParseString(const std::string& str, char c, std::vector<std::string>& v);

/** Split a string on any char found in separators, returning a vector.
 *
 * If sep does not occur in sp, a singleton with the entirety of sp is returned.
 *
 * @param[in] include_sep Whether to include the separator at the end of the left side of the splits.
 *
 * Note that this function does not care about braces, so splitting
 * "foo(bar(1),2),3) on ',' will return {"foo(bar(1)", "2)", "3)"}.
 *
 * If include_sep == true, splitting "foo(bar(1),2),3) on ','
 * will return:
 *  - foo(bar(1),
 *  - 2),
 *  - 3)
 */
template <typename T = Span<const char>>
std::vector<T> Split(const Span<const char>& sp, std::string_view separators, bool include_sep = false)
{
    std::vector<T> ret;
    auto it = sp.begin();
    auto start = it;
    while (it != sp.end()) {
        if (separators.find(*it) != std::string::npos) {
            if (include_sep) {
                ret.emplace_back(start, it + 1);
            } else {
                ret.emplace_back(start, it);
            }
            start = it + 1;
        }
        ++it;
    }
    ret.emplace_back(start, it);
    return ret;
}

/** Split a string on every instance of sep, returning a vector.
 *
 * If sep does not occur in sp, a singleton with the entirety of sp is returned.
 *
 * Note that this function does not care about braces, so splitting
 * "foo(bar(1),2),3) on ',' will return {"foo(bar(1)", "2)", "3)"}.
 */
template <typename T = Span<const char>>
std::vector<T> Split(const Span<const char>& sp, char sep, bool include_sep = false)
{
    return Split<T>(sp, std::string_view{&sep, 1}, include_sep);
}

[[nodiscard]] inline std::vector<std::string> SplitString(std::string_view str, char sep)
{
    return Split<std::string>(str, sep);
}

[[nodiscard]] inline std::vector<std::string> SplitString(std::string_view str, std::string_view separators)
{
    return Split<std::string>(str, separators);
}

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
 * Replace all (non-overlapping) occurrences of @a search in @a in_out with
 * @a substitute, in place. Equivalent to boost::replace_all.
 */
inline void ReplaceAll(std::string& in_out, std::string_view search, std::string_view substitute)
{
    if (search.empty()) return;
    std::string::size_type pos = 0;
    while ((pos = in_out.find(search, pos)) != std::string::npos) {
        in_out.replace(pos, search.length(), substitute);
        pos += substitute.length();
    }
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
 * the ability to specify the precision.
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
