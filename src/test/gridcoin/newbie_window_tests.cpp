// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/common.h"
#include "crypto/sha256.h"
#include "gridcoin/accrual/newbie_window.h"
#include "span.h"
#include "util/strencodings.h"

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstring>
#include <string>

namespace {
//!
//! \brief Check a newbie window table against the values pinned for it.
//!
//! The values pin the tables that contrib/devtools/gen-newbie-window-table.py
//! generated. A regenerated table must match.
//!
void CheckTable(
    const Span<const GRC::NewbieWindowEntry> table,
    const int64_t first_height,
    const int64_t last_height,
    const std::string& digest_hex)
{
    BOOST_REQUIRE(!table.empty());
    BOOST_CHECK_EQUAL(table.front().m_height, first_height);
    BOOST_CHECK_EQUAL(table.back().m_height, last_height);

    CSHA256 hasher;
    const GRC::NewbieWindowEntry* previous = nullptr;

    for (const auto& entry : table) {
        BOOST_CHECK(entry.m_correction != 0);
        BOOST_CHECK_EQUAL(std::strlen(entry.m_cpid), size_t{32});

        // Ordered by height and CPID, without duplicates:
        if (previous != nullptr) {
            BOOST_CHECK(previous->m_height < entry.m_height
                || (previous->m_height == entry.m_height && std::strcmp(previous->m_cpid, entry.m_cpid) < 0));
        }

        // The digest covers each entry as height (int64 LE), the CPID's hex
        // characters and the correction (int64 LE):
        std::array<unsigned char, 8> number;
        WriteLE64(number.data(), static_cast<uint64_t>(entry.m_height));
        hasher.Write(number.data(), number.size());
        hasher.Write(reinterpret_cast<const unsigned char*>(entry.m_cpid), std::strlen(entry.m_cpid));
        WriteLE64(number.data(), static_cast<uint64_t>(entry.m_correction));
        hasher.Write(number.data(), number.size());

        previous = &entry;
    }

    std::array<unsigned char, CSHA256::OUTPUT_SIZE> digest;
    hasher.Finalize(digest.data());

    BOOST_CHECK_EQUAL(HexStr(digest), digest_hex);
}
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(newbie_window_tests)

BOOST_AUTO_TEST_CASE(it_holds_the_newbie_corrections_that_mainnet_applied)
{
    CheckTable(
        GRC::MAINNET_NEWBIE_WINDOW,
        2197273,
        2274769,
        "81a607f63750955e6503a4f45f5b72ba692ee7c8fb2fd0ea14d6aae8a4897ba2");
}

BOOST_AUTO_TEST_CASE(it_holds_the_newbie_corrections_that_testnet_applied)
{
    CheckTable(
        GRC::TESTNET_NEWBIE_WINDOW,
        1480429,
        1480429,
        "3a6221a52579bde23df123496aa049cef6a65af4b8839bb7dc7c3794ae90b832");
}

BOOST_AUTO_TEST_SUITE_END()
