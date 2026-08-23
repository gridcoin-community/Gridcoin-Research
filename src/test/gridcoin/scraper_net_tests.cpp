// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/scraper/scraper_net.h"

#include "hash.h"
#include "key.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "version.h"

#include <boost/test/unit_test.hpp>

#include <ios>
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

//!
//! The part-hash vector is the first field on the wire, so its length prefix is
//! reachable with no signature and no valid manifest behind it. A count above
//! the ceiling must be refused, and must NOT penalise the peer: the ceiling sits
//! far above any legitimate manifest, so if it is ever wrong it must not ban.
//!
BOOST_AUTO_TEST_CASE(unserializecheck_refuses_an_oversized_part_hash_vector)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    WriteCompactSize(ss, 1025);   // one over the cap, nothing behind it

    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());

    // Sentinel: the guard must actively zero this, not merely leave it alone.
    unsigned int banscore = 12345;

    LOCK2(CScraperManifest::cs_mapManifest, manifest->cs_manifest);

    BOOST_CHECK(manifest->UnserializeCheck(ss, banscore) == false);
    BOOST_CHECK_EQUAL(banscore, 0u);
}

//!
//! The boundary, from the other side. A count exactly at the ceiling is admitted
//! by the cap and then fails on the truncated stream instead -- a different
//! failure mode, which is what pins the ceiling at 1024 rather than 1023.
//!
BOOST_AUTO_TEST_CASE(unserializecheck_admits_a_part_hash_vector_at_the_cap)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    WriteCompactSize(ss, 1024);   // exactly at the cap, still nothing behind it

    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());
    unsigned int banscore = 0;

    LOCK2(CScraperManifest::cs_mapManifest, manifest->cs_manifest);

    BOOST_CHECK_THROW((void)manifest->UnserializeCheck(ss, banscore), std::ios_base::failure);
}

namespace {
//! Build a manifest stream as far as the structural checks read it. The part
//! list, the beacon-list reference and the project dentries are all that those
//! checks consult, and they run before signature verification, so no key and no
//! valid signature are needed. nTime is current so IsManifestCurrent() passes,
//! and a chainless test process is out of sync by age, which skips
//! authorization.
CDataStream ManifestPrefix(size_t part_count, int beacon_list,
                           const std::vector<int>& project_part1s)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);

    std::vector<uint256> vph(part_count);
    for (size_t i = 0; i < part_count; ++i) {
        vph[i] = Hash(std::vector<unsigned char>{static_cast<unsigned char>(i + 1)});
    }

    std::vector<CScraperManifest::dentry> projects;
    for (const int p : project_part1s) {
        CScraperManifest::dentry d;
        d.project = "project";
        d.part1 = p;
        d.partc = 0;
        projects.push_back(d);
    }

    ss << vph;
    ss << CPubKey();
    ss << std::string("test-manifest");
    ss << static_cast<int64_t>(GetAdjustedTime());
    ss << uint256();
    ss << beacon_list << static_cast<unsigned int>(0);
    ss << projects;

    return ss;
}
} // anonymous namespace

//!
//! A part declared but referenced by nothing is refused. Without this a manifest
//! can declare parts it never uses; each is registered in the process-global
//! mapParts and held until the manifest ages out, and reclamation is
//! refcount-driven and so only happens afterwards.
//!
BOOST_AUTO_TEST_CASE(unserializecheck_refuses_an_unreferenced_part)
{
    // Three parts; the beacon list covers 0 and one project covers 1. Part 2 is
    // declared and referenced by nothing.
    CDataStream ss = ManifestPrefix(3, 0, {1});

    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());
    unsigned int banscore = 0;

    LOCK2(CScraperManifest::cs_mapManifest, manifest->cs_manifest);

    BOOST_CHECK(manifest->UnserializeCheck(ss, banscore) == false);
}

//!
//! The converse: a fully covered part list gets past the coverage rule and fails
//! later on the truncated stream instead, which is what shows the rule admits
//! the shape our own publisher emits (beacon list at 0, one part per dentry).
//!
BOOST_AUTO_TEST_CASE(unserializecheck_admits_a_fully_referenced_part_list)
{
    CDataStream ss = ManifestPrefix(3, 0, {1, 2});

    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());
    unsigned int banscore = 0;

    LOCK2(CScraperManifest::cs_mapManifest, manifest->cs_manifest);

    BOOST_CHECK_THROW((void)manifest->UnserializeCheck(ss, banscore), std::ios_base::failure);
}

//!
//! A reference ending exactly at vph.size() names one part past the end. The
//! ranges are inclusive, so this must be refused -- it passed the previous
//! comparison, which rejected only strictly-greater, and scraper.cpp indexes
//! vParts[part1] without a bounds guard.
//!
BOOST_AUTO_TEST_CASE(unserializecheck_refuses_a_reference_one_past_the_end)
{
    // Two parts, so the only valid indices are 0 and 1. part1 == 2 is one past.
    CDataStream ss = ManifestPrefix(2, 0, {2});

    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());
    unsigned int banscore = 0;

    LOCK2(CScraperManifest::cs_mapManifest, manifest->cs_manifest);

    BOOST_CHECK(manifest->UnserializeCheck(ss, banscore) == false);
}

BOOST_AUTO_TEST_SUITE_END()
