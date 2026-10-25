// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/cpid.h"
#include "gridcoin/pool.h"
#include "gridcoin/protocol.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "hash.h"
#include "key.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "test/state_guard.h"
#include "uint256.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::string BEACON_INVARIANT = "GetBeaconRegistry().Beacons()";
const std::string POOL_INVARIANT = "GetPoolRegistry().Entries()";
const std::string PROTOCOL_INVARIANT = "GetProtocolRegistry().ProtocolEntries()";
const std::string SCRAPER_INVARIANT = "GetScraperRegistry().Scrapers()";

//! The diffs DiffState reported for one invariant, matched as a prefix so the
//! older ".size()" names are read too.
std::vector<grc_test::StateDiff> DiffsFor(const std::vector<grc_test::StateDiff>& diffs, const std::string& invariant)
{
    std::vector<grc_test::StateDiff> out;
    for (const auto& diff : diffs) {
        // sign-compare checked: rfind() returns size_t; 0U is a non-negative literal, so both sides are unsigned.
        if (diff.invariant.rfind(invariant, 0) == 0U) out.push_back(diff);
    }
    return out;
}

bool EndsWith(const std::string& text, const std::string& suffix)
{
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

//! Add (or, for an existing key, replace) an ACTIVE protocol entry, the way
//! gridcoin_cbr_tests does.
void AddProtocolEntry(const std::string& key, const std::string& value, const CBlockIndex& index)
{
    CMutableTransaction mtx;
    mtx.nTime = index.nTime;

    const GRC::Contract contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
        uint32_t {3}, // Contract version (post v13)
        GRC::ContractAction::ADD,
        uint32_t {2}, // Protocol payload version (post v13)
        key,
        value,
        GRC::ProtocolEntryStatus::ACTIVE);
    mtx.vContracts.push_back(contract);

    const CTransaction tx(mtx);

    LOCK(cs_main);
    GRC::GetProtocolRegistry().Add({contract, tx, &index});
}

} // anonymous namespace

// The four registries the cases mutate are reset on entry and exit, so a case
// that fails part-way does not hand its edit to the next suite.
BOOST_AUTO_TEST_SUITE(state_guard_tests,
                      *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::BEACON>>()
                      *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::POOL_REGISTER>>()
                      *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::PROTOCOL>>()
                      *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::SCRAPER>>())

//! The #3368 shape: a builtin pool edited in place keeps the count at five, so
//! only a content digest can see it. Also pins that Reset() reproduces the
//! boot digest exactly, which the detector's baseline rule relies on.
BOOST_AUTO_TEST_CASE(a_builtin_pool_edited_in_place_is_a_leak)
{
    const grc_test::StateSnapshot before = grc_test::CaptureState(grc_test::CaptureScope::SUITE);

    const GRC::Cpid cpid = GRC::Cpid::Parse(GRC::PoolRegistry::BuiltinPoolSeeds().front().cpid_hex);
    {
        LOCK(cs_main);
        GRC::PoolRegistry& registry = GRC::GetPoolRegistry();
        GRC::Pool_ptr seed = registry.Try(cpid);
        BOOST_REQUIRE(seed);

        GRC::Pool entry = *seed;
        entry.m_authorization_height = 1;
        registry.SeedForTests(entry);
    }

    BOOST_REQUIRE_EQUAL(GRC::GetPoolRegistry().Entries().size(), GRC::PoolRegistry::BuiltinPoolSeeds().size());

    const grc_test::StateSnapshot edited = grc_test::CaptureState(grc_test::CaptureScope::SUITE);
    const auto leak = DiffsFor(grc_test::DiffState(before, edited), POOL_INVARIANT);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(leak.size(), 1U);
    BOOST_CHECK(leak[0].IsLeak());
    BOOST_CHECK_MESSAGE(EndsWith(leak[0].after, " [edited " + cpid.ToString() + "]"), "after: " + leak[0].after);

    {
        LOCK(cs_main);
        GRC::GetPoolRegistry().Reset();
    }

    std::vector<uint256> boot_hashes, live_hashes;
    {
        const GRC::PoolRegistry fresh; // the boot state: the constructor only seeds the builtins
        for (const GRC::Pool& pool : fresh.Entries()) boot_hashes.push_back(SerializeHash(pool));
    }
    for (const GRC::Pool& pool : GRC::GetPoolRegistry().Entries()) live_hashes.push_back(SerializeHash(pool));
    std::sort(boot_hashes.begin(), boot_hashes.end());
    std::sort(live_hashes.begin(), live_hashes.end());
    BOOST_CHECK(boot_hashes == live_hashes);

    const grc_test::StateSnapshot reset = grc_test::CaptureState(grc_test::CaptureScope::SUITE);
    BOOST_CHECK(DiffsFor(grc_test::DiffState(before, reset), POOL_INVARIANT).empty());
}

//! The same shape in a registry the detector already watched: replacing the
//! value under an existing key keeps the count at one.
BOOST_AUTO_TEST_CASE(a_protocol_entry_replaced_at_the_same_count_is_a_leak)
{
    CBlockIndex index_1;
    index_1.nVersion = 13;
    index_1.nTime = 123456;
    index_1.nHeight = Params().GetConsensus().BlockV13Height;

    CBlockIndex index_2;
    index_2.nVersion = 13;
    index_2.nTime = 123457;
    index_2.nHeight = Params().GetConsensus().BlockV13Height + 1;

    AddProtocolEntry("state_guard_tests_key", "1", index_1);
    const grc_test::StateSnapshot before = grc_test::CaptureState(grc_test::CaptureScope::SUITE);

    AddProtocolEntry("state_guard_tests_key", "2", index_2);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(GRC::GetProtocolRegistry().ProtocolEntries().size(), 1U);

    const grc_test::StateSnapshot after = grc_test::CaptureState(grc_test::CaptureScope::SUITE);
    const auto diffs = DiffsFor(grc_test::DiffState(before, after), PROTOCOL_INVARIANT);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(diffs.size(), 1U);
    BOOST_CHECK(diffs[0].IsLeak());
    BOOST_CHECK_MESSAGE(EndsWith(diffs[0].after, " [edited state_guard_tests_key]"), "after: " + diffs[0].after);

    LOCK(cs_main);
    GRC::GetProtocolRegistry().Reset();
}

//! A renewal replaces the active beacon under the same CPID and keeps its
//! public key, so the count stays at one. Beacon's own serialization writes
//! only the public key; this pins that the capture hashes the StorageBeacon
//! form, which also carries the timestamp, hash, previous hash and status.
BOOST_AUTO_TEST_CASE(an_active_beacon_renewed_at_the_same_count_is_a_leak)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("00112233445566778899aabbccddeeff");
    CKey key;
    key.MakeNewKey(true);

    const GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::ADD,
        cpid,
        key.GetPubKey());

    // Advertise and activate, as mrc_tests does. A null block index gives the
    // registry's test height of -1.
    CMutableTransaction advertisement;
    advertisement.nTime = 1700000000;
    const CTransaction advertisement_tx(advertisement);
    {
        LOCK(cs_main);
        GRC::GetBeaconRegistry().Add({contract, advertisement_tx, nullptr});
        GRC::GetBeaconRegistry().ActivatePending({key.GetPubKey().GetID()}, advertisement_tx.nTime, uint256(), 0);
        // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
        BOOST_REQUIRE_EQUAL(GRC::GetBeaconRegistry().Beacons().size(), 1U);
    }

    const grc_test::StateSnapshot before = grc_test::CaptureState(grc_test::CaptureScope::SUITE);

    // The same CPID and public key one day later. The active beacon has not
    // expired, so TryRenewal replaces it under the same key.
    CMutableTransaction renewal;
    renewal.nTime = advertisement.nTime + 24 * 60 * 60;
    const CTransaction renewal_tx(renewal);
    {
        LOCK(cs_main);
        GRC::GetBeaconRegistry().Add({contract, renewal_tx, nullptr});
        // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
        BOOST_REQUIRE_EQUAL(GRC::GetBeaconRegistry().Beacons().size(), 1U);
    }

    const grc_test::StateSnapshot after = grc_test::CaptureState(grc_test::CaptureScope::SUITE);
    const auto diffs = DiffsFor(grc_test::DiffState(before, after), BEACON_INVARIANT);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(diffs.size(), 1U);
    BOOST_CHECK(diffs[0].IsLeak());
    BOOST_CHECK_MESSAGE(EndsWith(diffs[0].after, " [edited " + cpid.ToString() + "]"), "after: " + diffs[0].after);

    LOCK(cs_main);
    GRC::GetBeaconRegistry().Reset();
}

//! The same shape in the scraper registry: deauthorizing a scraper replaces
//! its entry under the same key, so the count stays at one.
BOOST_AUTO_TEST_CASE(a_scraper_entry_replaced_at_the_same_count_is_a_leak)
{
    CKey key;
    key.MakeNewKey(true);
    const CKeyID key_id = key.GetPubKey().GetID();

    // Authorize, then deauthorize, as scraper_registry_tests does. A null
    // block index gives the registry's test height of -1.
    CMutableTransaction authorization;
    authorization.nTime = 1700000000;
    const GRC::Contract authorize = GRC::MakeContract<GRC::ScraperEntryPayload>(
        uint32_t {3}, // Contract version (post v13)
        GRC::ContractAction::ADD,
        uint32_t {2}, // Scraper payload version (post v13)
        key_id,
        GRC::ScraperEntryStatus::AUTHORIZED);
    authorization.vContracts.push_back(authorize);
    const CTransaction authorization_tx(authorization);
    {
        LOCK(cs_main);
        GRC::GetScraperRegistry().Add({authorize, authorization_tx, nullptr});
        // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
        BOOST_REQUIRE_EQUAL(GRC::GetScraperRegistry().Scrapers().size(), 1U);
    }

    const grc_test::StateSnapshot before = grc_test::CaptureState(grc_test::CaptureScope::SUITE);

    CMutableTransaction deauthorization;
    deauthorization.nTime = authorization.nTime + 1;
    const GRC::Contract deauthorize = GRC::MakeContract<GRC::ScraperEntryPayload>(
        uint32_t {3}, // Contract version (post v13)
        GRC::ContractAction::ADD,
        uint32_t {2}, // Scraper payload version (post v13)
        key_id,
        GRC::ScraperEntryStatus::NOT_AUTHORIZED);
    deauthorization.vContracts.push_back(deauthorize);
    const CTransaction deauthorization_tx(deauthorization);
    {
        LOCK(cs_main);
        GRC::GetScraperRegistry().Add({deauthorize, deauthorization_tx, nullptr});
        // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
        BOOST_REQUIRE_EQUAL(GRC::GetScraperRegistry().Scrapers().size(), 1U);
    }

    const grc_test::StateSnapshot after = grc_test::CaptureState(grc_test::CaptureScope::SUITE);
    const auto diffs = DiffsFor(grc_test::DiffState(before, after), SCRAPER_INVARIANT);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(diffs.size(), 1U);
    BOOST_CHECK(diffs[0].IsLeak());
    BOOST_CHECK_MESSAGE(EndsWith(diffs[0].after, " [edited " + key_id.ToString() + "]"), "after: " + diffs[0].after);

    LOCK(cs_main);
    GRC::GetScraperRegistry().Reset();
}

// SYNTHETIC SNAPSHOTS FROM HERE TO THE END OF THE SUITE. The cases below build
// pool-only snapshots by hand to pin the classification rule key by key.
namespace {

//! A snapshot whose only non-empty registry is the pool, from key -> hash
//! pairs. The hashes are small stand-in values, not real entry hashes.
grc_test::StateSnapshot PoolsOnly(std::initializer_list<std::pair<const char*, const char*>> entries)
{
    grc_test::StateSnapshot s;
    s.has_registries = true;
    for (const auto& [key, hash] : entries) s.pools.entries[key] = uint256S(hash);
    return s;
}

// "a" and "b" stand for builtins (the baseline); "x" and "y" for residue an
// earlier suite left; hash "7" on "a" for an earlier in-place edit.
const grc_test::StateSnapshot clean = PoolsOnly({{"a", "1"}, {"b", "2"}});
const grc_test::StateSnapshot dirty = PoolsOnly({{"a", "1"}, {"b", "2"}, {"x", "9"}});
const grc_test::StateSnapshot dirtier = PoolsOnly({{"a", "1"}, {"b", "2"}, {"x", "9"}, {"y", "8"}});
const grc_test::StateSnapshot edited = PoolsOnly({{"a", "7"}, {"b", "2"}});
const grc_test::StateSnapshot edited_dirty = PoolsOnly({{"a", "7"}, {"b", "2"}, {"x", "9"}});
const grc_test::StateSnapshot only_a = PoolsOnly({{"a", "1"}});
const grc_test::StateSnapshot a_and_x = PoolsOnly({{"a", "1"}, {"x", "9"}});
const grc_test::StateSnapshot replaced = PoolsOnly({{"a", "1"}, {"b", "2"}, {"y", "8"}});

//! Whether DiffState classes the pool change from \p before to \p after as a
//! leak. Exactly one pool diff is required: every change must be reported.
bool ClassedAsLeak(const grc_test::StateSnapshot& before, const grc_test::StateSnapshot& after,
                   const grc_test::StateSnapshot* baseline)
{
    const auto diffs = DiffsFor(grc_test::DiffState(before, after, baseline), POOL_INVARIANT);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(diffs.size(), 1U);
    return diffs[0].IsLeak();
}

} // anonymous namespace

//! Ending exactly at the baseline is a cleaner, whatever the count did.
BOOST_AUTO_TEST_CASE(a_registry_that_ends_at_the_baseline_is_a_cleaner)
{
    BOOST_CHECK(!ClassedAsLeak(dirty, clean, &clean));        // residue removed
    BOOST_CHECK(!ClassedAsLeak(edited, clean, &clean));       // an earlier edit undone, same count
    BOOST_CHECK(!ClassedAsLeak(edited_dirty, clean, &clean)); // both at once
}

//! Removing only keys the baseline lacks, and changing nothing else, is a
//! cleaner even when the registry does not reach the baseline.
BOOST_AUTO_TEST_CASE(shrink_of_non_baseline_key_after_dirty_entry_is_a_cleaner)
{
    BOOST_CHECK(!ClassedAsLeak(dirtier, dirty, &clean));       // "y" removed, "x" left behind
    BOOST_CHECK(!ClassedAsLeak(edited_dirty, edited, &clean)); // "x" removed; the edit of "a" is not this suite's
}

//! Removing a key the first suite found is a leak, from clean or dirty.
BOOST_AUTO_TEST_CASE(a_removed_baseline_key_is_a_leak)
{
    BOOST_CHECK(ClassedAsLeak(clean, only_a, &clean));  // "b" removed from a clean registry
    BOOST_CHECK(ClassedAsLeak(dirty, a_and_x, &clean)); // "b" removed, residue left
    BOOST_CHECK(ClassedAsLeak(dirty, only_a, &clean));  // "b" and the residue removed: below the baseline
}

//! A removal does not excuse an edit made in the same suite.
BOOST_AUTO_TEST_CASE(an_edited_survivor_plus_a_removal_is_a_leak)
{
    BOOST_CHECK(ClassedAsLeak(dirty, edited, &clean));         // "x" removed and "a" edited
    BOOST_CHECK(ClassedAsLeak(dirtier, edited_dirty, &clean)); // "y" removed and "a" edited
}

//! The #3368 shape at the unit level: the count does not move.
BOOST_AUTO_TEST_CASE(a_same_count_edit_is_a_leak)
{
    BOOST_CHECK(ClassedAsLeak(clean, edited, &clean));        // from clean
    BOOST_CHECK(ClassedAsLeak(dirty, edited_dirty, &clean));  // from dirty
    BOOST_CHECK(ClassedAsLeak(dirty, replaced, &clean));      // "x" replaced by "y"
}

//! Growth is a leak with or without a baseline.
BOOST_AUTO_TEST_CASE(an_added_key_is_a_leak)
{
    BOOST_CHECK(ClassedAsLeak(clean, dirty, &clean));
    BOOST_CHECK(ClassedAsLeak(clean, dirty, nullptr));
}

//! With no baseline there are no baseline keys to protect: a pure removal is a
//! cleaner and anything else is a leak.
BOOST_AUTO_TEST_CASE(without_a_baseline_only_a_pure_removal_is_a_cleaner)
{
    BOOST_CHECK(!ClassedAsLeak(dirty, clean, nullptr));
    BOOST_CHECK(!ClassedAsLeak(dirty, only_a, nullptr));
    BOOST_CHECK(ClassedAsLeak(dirty, edited, nullptr)); // a removal plus an edit
}

//! The after text names the changed keys, at most five of them.
BOOST_AUTO_TEST_CASE(the_leak_message_names_the_changed_keys)
{
    const auto diffs = DiffsFor(grc_test::DiffState(dirty, edited, &clean), POOL_INVARIANT);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(diffs.size(), 1U);
    BOOST_CHECK_MESSAGE(EndsWith(diffs[0].after, " [edited a, removed x]"), "after: " + diffs[0].after);
    BOOST_CHECK_MESSAGE(diffs[0].before.find('[') == std::string::npos, "before: " + diffs[0].before);

    grc_test::StateSnapshot many = clean;
    for (const char* key : {"k1", "k2", "k3", "k4", "k5", "k6", "k7"}) many.pools.entries[key] = uint256S("5");

    const auto grown = DiffsFor(grc_test::DiffState(clean, many, &clean), POOL_INVARIANT);
    // sign-compare checked: size() is size_t; 1U is a non-negative literal, so both sides are unsigned.
    BOOST_REQUIRE_EQUAL(grown.size(), 1U);
    BOOST_CHECK_MESSAGE(EndsWith(grown[0].after, " [added k1, added k2, added k3, added k4, added k5 ... and 2 more]"),
                        "after: " + grown[0].after);
}

BOOST_AUTO_TEST_SUITE_END()
