// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/scraper/scraper_net.h"

#include "hash.h"
#include "streams.h"
#include "version.h"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <vector>

BOOST_AUTO_TEST_SUITE(scraper_net_tests)

//!
//! An empty part is a peer or filesystem condition, not a programming error.
//! It must be discarded and reported, never asserted on: asserts are live in
//! release builds here (CMakeLists.txt applies -UNDEBUG globally), so aborting
//! would turn a malformed input into a process termination.
//!
BOOST_AUTO_TEST_CASE(recvpart_discards_an_empty_part)
{
    CDataStream empty(SER_NETWORK, PROTOCOL_VERSION);
    BOOST_REQUIRE(empty.empty());

    // nullptr peer: this is the self-publishing path, and must not dereference.
    BOOST_CHECK(CSplitBlob::RecvPart(nullptr, empty) == false);
}

//!
//! Our own publishing path. A zero-byte part file -- a truncated write, a full
//! disk, an interrupted gzip -- must not be registered, and must not abort.
//!
BOOST_AUTO_TEST_CASE(addpartdata_refuses_empty_data_and_registers_nothing)
{
    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());

    CDataStream empty(SER_NETWORK, PROTOCOL_VERSION);

    BOOST_CHECK_EQUAL(manifest->addPartData(std::move(empty)), -1);

    LOCK(manifest->cs_manifest);
    BOOST_CHECK_EQUAL(manifest->vParts.size(), 0u);
}

//!
//! Control: a non-empty part is still accepted and indexed from zero, so the
//! guard above rejects only what it is meant to.
//!
BOOST_AUTO_TEST_CASE(addpartdata_still_accepts_a_non_empty_part)
{
    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());

    CDataStream data(SER_NETWORK, PROTOCOL_VERSION);
    data << std::string("a non-empty part");

    BOOST_CHECK_EQUAL(manifest->addPartData(std::move(data)), 0);

    LOCK(manifest->cs_manifest);
    BOOST_CHECK_EQUAL(manifest->vParts.size(), 1u);
}

//!
//! The constant UnserializeCheck() screens manifest part lists against. Pinned
//! here so a change to the hash function cannot silently move it: a manifest
//! declaring this hash can never be satisfied, because RecvPart discards the
//! only content that would produce it.
//!
BOOST_AUTO_TEST_CASE(the_empty_content_hash_is_the_documented_constant)
{
    const uint256 empty_hash = Hash(std::vector<unsigned char>{});

    BOOST_CHECK_EQUAL(
        empty_hash.ToString(),
        "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d");
}

BOOST_AUTO_TEST_SUITE_END()
