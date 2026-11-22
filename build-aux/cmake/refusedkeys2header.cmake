cmake_minimum_required(VERSION 3.19)

if(CMAKE_ARGC LESS 5)
    message(FATAL_ERROR "Usage: cmake -P refusedkeys2header.cmake in_file out_file")
endif()

set(IN_FILE ${CMAKE_ARGV3})
set(OUT_FILE ${CMAKE_ARGV4})

# Read the file whole and split on newlines. Semicolons and brackets are not in
# the grammar; a line carrying one fails the grammar check below, so the list
# splitting cannot hide it.
file(READ "${IN_FILE}" text)
string(REPLACE "\r" "" text "${text}")
string(REPLACE "[" "<" text "${text}")
string(REPLACE "]" ">" text "${text}")
string(REPLACE ";" "," text "${text}")
string(REPLACE "\n" ";" lines "${text}")

set(patterns "")
set(entries "")
set(count 0)
set(line_number 0)
foreach(line IN LISTS lines)
    math(EXPR line_number "${line_number} + 1")
    if(line STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()
    if(NOT line MATCHES "^[a-z0-9_*-]+$")
        message(FATAL_ERROR "${IN_FILE}:${line_number}: line is outside the pattern grammar ^[a-z0-9_*-]+$: '${line}'")
    endif()
    if(line IN_LIST patterns)
        message(FATAL_ERROR "${IN_FILE}:${line_number}: duplicate pattern '${line}'")
    endif()
    list(APPEND patterns "${line}")
    string(APPEND entries "    \"${line}\",\n")
    math(EXPR count "${count} + 1")
endforeach()

if(count EQUAL 0)
    message(FATAL_ERROR "${IN_FILE}: no patterns")
endif()

file(WRITE "${OUT_FILE}" "// Generated from share/gridcoin/refused-setting-keys.txt. Do not edit.
#ifndef GRIDCOIN_UTIL_REFUSED_SETTING_KEYS_H
#define GRIDCOIN_UTIL_REFUSED_SETTING_KEYS_H

#include <array>
#include <string_view>

namespace util {
inline constexpr std::array<std::string_view, ${count}> REFUSED_SETTING_KEY_PATTERNS{{
${entries}}};
} // namespace util

#endif // GRIDCOIN_UTIL_REFUSED_SETTING_KEYS_H
")
