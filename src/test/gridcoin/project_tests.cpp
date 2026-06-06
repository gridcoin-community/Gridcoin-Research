// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "gridcoin/quorum.h"
#include "util/string.h"
#include "wallet/generated_type.h"

#include <boost/test/unit_test.hpp>

// Tests are single-threaded and drive the Whitelist contract handler
// directly (not via ApplyContracts). The handler is
// EXCLUSIVE_LOCKS_REQUIRED(cs_main) under the thread-safety annotation
// rollout; suppress the analyzer for this file rather than take a lock
// the tests do not need.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

namespace {
void AddProjectEntry(const uint32_t& payload_version, const std::string& name, const std::string& url,
                     const bool& gdpr_status, const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::Whitelist& registry = GRC::GetWhitelist();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CMutableTransaction dummy_tx;
    CBlockIndex dummy_index = CBlockIndex {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

    if (payload_version == 1) {
        contract = GRC::MakeContract<GRC::Project>(
                    uint32_t {2}, // Contract version (pre v13)
                    GRC::ContractAction::ADD,
                    name,
                    url);

    } else if (payload_version == 2) {
        contract = GRC::MakeContract<GRC::Project>(
                    uint32_t {2}, // Contract version (pre v13)
                    GRC::ContractAction::ADD,
                    payload_version,
                    name,
                    url,
                    gdpr_status);
    } else if (payload_version == 3){
        contract = GRC::MakeContract<GRC::Project>(
                    uint32_t {3}, // Contract version (post v13)
                    GRC::ContractAction::ADD,
                    payload_version,
                    name,
                    url,
                    gdpr_status);
    }

    dummy_tx.vContracts.push_back(contract);

    CTransaction ctx_tx(dummy_tx);
    registry.Add({contract, ctx_tx, &dummy_index});
}

void DeleteProjectEntry(const uint32_t& payload_version, const std::string& name,
                        const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::Whitelist& registry = GRC::GetWhitelist();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CMutableTransaction dummy_tx;
    CBlockIndex dummy_index = CBlockIndex {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

    if (payload_version == 1) {
        contract = GRC::MakeContract<GRC::Project>(
                    uint32_t {2}, // Contract version (pre v13)
                    GRC::ContractAction::REMOVE,
                    name,
                    std::string{});
    } else if (payload_version == 2) {
        contract = GRC::MakeContract<GRC::Project>(
                    uint32_t {2}, // Contract version (pre v13)
                    GRC::ContractAction::REMOVE,
                    payload_version,
                    name,
                    std::string{});
    } else if (payload_version == 3){
        contract = GRC::MakeContract<GRC::Project>(
                    uint32_t {3}, // Contract version (post v13)
                    GRC::ContractAction::REMOVE,
                    payload_version,
                    name,
                    std::string{});
    }

    dummy_tx.vContracts.push_back(contract);

    CTransaction ctx_tx(dummy_tx);
    registry.Add({contract, ctx_tx, &dummy_index});
}

//!
//! \brief Applies a v4 project ADD contract carrying an EXPLICIT status through the registry contract handler,
//! persisting it to LevelDB (a real CBlockIndex height is supplied so RegistryDB::Store runs and
//! GetProjectsFromDisk can read it back).
//!
//! This is a reusable building block for tests that need the registry/contract/LevelDB layer actually populated --
//! e.g. verifying the raw contract status read by getrawprojectstatus, and (when the autogreylist "bad project" run
//! is extended through this same path) asserting that the in-memory Snapshot overlay can diverge from the persisted
//! contract status. Pass status = ProjectEntryStatus::UNKNOWN for a plain ACTIVE add (the handler maps
//! ADD + UNKNOWN -> ACTIVE, exactly as the addkey client does).
//!
void AddProjectEntryWithStatus(const std::string& name, const std::string& url,
                               const GRC::ProjectEntryStatus& status, const int& height, const uint64_t time,
                               const bool& gdpr_status = false, const bool& requires_ext_adapter = false)
{
    GRC::Whitelist& registry = GRC::GetWhitelist();

    CMutableTransaction dummy_tx;
    CBlockIndex dummy_index = CBlockIndex {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract = GRC::MakeContract<GRC::Project>(
                uint32_t {3},   // Contract version (post v13)
                GRC::ContractAction::ADD,
                uint32_t {4},   // Payload version (v4)
                name,
                url,
                gdpr_status,
                requires_ext_adapter,
                status);

    dummy_tx.vContracts.push_back(contract);

    CTransaction ctx_tx(dummy_tx);
    registry.Add({contract, ctx_tx, &dummy_index});
}

struct AutoGreylistEntryState
{
    AutoGreylistEntryState(uint8_t zcd_20_SB_count,
                           uint64_t TC_7_SB_sum,
                           uint64_t TC_40_SB_sum,
                           Fraction was,
                           bool meets_greylisting_crit)
        : m_zcd_20_SB_count(zcd_20_SB_count)
        , m_TC_7_SB_sum(TC_7_SB_sum)
        , m_TC_40_SB_sum(TC_40_SB_sum)
        , m_was(was)
        ,m_meets_greylisting_crit(meets_greylisting_crit)
    {}

    uint8_t m_zcd_20_SB_count;
    uint64_t m_TC_7_SB_sum;
    uint64_t m_TC_40_SB_sum;
    Fraction m_was;
    bool m_meets_greylisting_crit;

    bool operator==(const AutoGreylistEntryState& rhs)
    {
        bool equal = (
            (m_zcd_20_SB_count == rhs.m_zcd_20_SB_count)
            && (m_TC_7_SB_sum == rhs.m_TC_7_SB_sum)
            && (m_TC_40_SB_sum == rhs.m_TC_40_SB_sum)
            && (m_was == rhs.m_was)
            && (m_meets_greylisting_crit == rhs.m_meets_greylisting_crit)
            );

        return equal;
    }

    bool operator!=(const AutoGreylistEntryState& rhs)
    {
        return !(*this == rhs);
    }
};

//!
//! \brief Dummy transaction for contract handler API.
//!
CTransaction g_tx;
} // anonymous namespace

// -----------------------------------------------------------------------------
// Project
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Project)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_project)
{
    const GRC::Project project;

    BOOST_CHECK_EQUAL(project.m_version, GRC::Project::CURRENT_VERSION);
    BOOST_CHECK(project.m_name.empty() == true);
    BOOST_CHECK(project.m_url.empty() == true);
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_a_new_project_contract)
{
    const GRC::Project project("Enigma", "http://enigma.test/@");

    BOOST_CHECK_EQUAL(project.m_version, 1);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_a_new_project_contract_current_version)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 0);

    BOOST_CHECK_EQUAL(project.m_version, GRC::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_project_contract_data)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.m_version, GRC::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 1234567);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_project_contract_data_and_gdpr_controls)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567, GRC::Project::CURRENT_VERSION, true);

    BOOST_CHECK_EQUAL(project.m_version, GRC::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 1234567);
    BOOST_CHECK_EQUAL(project.m_gdpr_controls, true);
}

BOOST_AUTO_TEST_CASE(it_formats_the_user_friendly_display_name)
{
    const GRC::Project project("Enigma_at_Home", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.DisplayName(), "Enigma at Home");
}

BOOST_AUTO_TEST_CASE(it_formats_the_base_project_url)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.BaseUrl(), "http://enigma.test/");
}

BOOST_AUTO_TEST_CASE(it_formats_the_project_display_url)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.DisplayUrl(), "http://enigma.test/");
}

BOOST_AUTO_TEST_CASE(it_formats_the_project_stats_url)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.StatsUrl(), "http://enigma.test/stats/");
}

BOOST_AUTO_TEST_CASE(it_formats_a_project_stats_archive_url)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.StatsUrl("user"), "http://enigma.test/stats/user.gz");
    BOOST_CHECK_EQUAL(project.StatsUrl("team"), "http://enigma.test/stats/team.gz");
}

BOOST_AUTO_TEST_CASE(it_behaves_like_a_contract_payload)
{
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.ContractType() == GRC::ContractType::PROJECT);
    BOOST_CHECK(project.WellFormed(GRC::ContractAction::ADD) == true);
    BOOST_CHECK(project.LegacyKeyString() == "Enigma");
    BOOST_CHECK(project.LegacyValueString() == "http://enigma.test/@");
    BOOST_CHECK(project.RequiredBurnAmount() > 0);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed_for_add)
{
    const GRC::Project valid("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(valid.WellFormed(GRC::ContractAction::ADD) == true);

    const GRC::Project no_name("", "http://enigma.test/@", 1234567);

    BOOST_CHECK(no_name.WellFormed(GRC::ContractAction::ADD) == false);

    const GRC::Project no_url("Enigma", "", 1234567);
    BOOST_CHECK(no_url.WellFormed(GRC::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed_for_delete)
{
    const GRC::Project valid("Enigma", "", 1234567);

    BOOST_CHECK(valid.WellFormed(GRC::ContractAction::REMOVE) == true);

    const GRC::Project no_name("", "http://enigma.test/@", 1234567);

    BOOST_CHECK(no_name.WellFormed(GRC::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_add)
{
    const GRC::Project projectv1("Enigma", "http://enigma.test/@", 1234567, 1);

    const CDataStream expectedv1 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
            << uint32_t{1}
            << std::string("Enigma")
            << std::string("http://enigma.test/@");

    CDataStream streamv1(SER_NETWORK, PROTOCOL_VERSION);
    projectv1.Serialize(streamv1, GRC::ContractAction::ADD);

    BOOST_CHECK(std::equal(
        streamv1.begin(),
        streamv1.end(),
        expectedv1.begin(),
        expectedv1.end()));

    const GRC::Project projectv2("Enigma", "http://enigma.test/@", 1234567, 2, true);

    const CDataStream expectedv2 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
            << uint32_t{2}
            << std::string("Enigma")
            << std::string("http://enigma.test/@")
            << true
            << CPubKey{};

    CDataStream streamv2(SER_NETWORK, PROTOCOL_VERSION);
    projectv2.Serialize(streamv2, GRC::ContractAction::ADD);

    BOOST_CHECK(std::equal(
        streamv2.begin(),
        streamv2.end(),
        expectedv2.begin(),
        expectedv2.end()));
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_add)
{
    CDataStream streamv1 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << uint32_t{1}
        << std::string("Enigma")
        << std::string("http://enigma.test/@");

    GRC::Project projectv1;
    projectv1.Unserialize(streamv1, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(projectv1.m_version, uint32_t{1});
    BOOST_CHECK_EQUAL(projectv1.m_name, "Enigma");
    BOOST_CHECK_EQUAL(projectv1.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(projectv1.m_timestamp, 0);
    BOOST_CHECK_EQUAL(projectv1.m_gdpr_controls, false);
    BOOST_CHECK(projectv1.m_public_key == CPubKey{});

    BOOST_CHECK(projectv1.WellFormed(GRC::ContractAction::ADD) == true);

    CPubKey public_key = CPubKey(ParseHex(
        "111111111111111111111111111111111111111111111111111111111111111111"));

    CDataStream streamv2 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
            << uint32_t{2}
        << std::string("Enigma")
        << std::string("http://enigma.test/@")
        << true
        << public_key;

    GRC::Project projectv2;
    projectv2.Unserialize(streamv2, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(projectv2.m_version, uint32_t{2});
    BOOST_CHECK_EQUAL(projectv2.m_name, "Enigma");
    BOOST_CHECK_EQUAL(projectv2.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(projectv2.m_timestamp, 0);
    BOOST_CHECK_EQUAL(projectv2.m_gdpr_controls, true);
    BOOST_CHECK(projectv2.m_public_key == public_key);

    BOOST_CHECK(projectv2.WellFormed(GRC::ContractAction::ADD) == true);

}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_delete)
{
    const GRC::Project projectv1("Enigma", "", 1234567, 1);

    const CDataStream expectedv1 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
            << uint32_t{1}
            << std::string("Enigma");

    CDataStream streamv1(SER_NETWORK, PROTOCOL_VERSION);
    projectv1.Serialize(streamv1, GRC::ContractAction::REMOVE);

    BOOST_CHECK(std::equal(
        streamv1.begin(),
        streamv1.end(),
        expectedv1.begin(),
        expectedv1.end()));

    const GRC::Project projectv2("Enigma", "", 1234567, uint32_t{2}, true);

    const CDataStream expectedv2 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << uint32_t{2}
        << std::string("Enigma");

    CDataStream streamv2(SER_NETWORK, PROTOCOL_VERSION);
    projectv2.Serialize(streamv2, GRC::ContractAction::REMOVE);

    BOOST_CHECK(std::equal(
        streamv2.begin(),
        streamv2.end(),
        expectedv2.begin(),
        expectedv2.end()));

}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_delete)
{
    CDataStream streamv1 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << uint32_t{1}
        << std::string("Enigma");

    GRC::Project projectv1;
    projectv1.Unserialize(streamv1, GRC::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL(projectv1.m_version, uint32_t{1});
    BOOST_CHECK_EQUAL(projectv1.m_name, "Enigma");
    BOOST_CHECK_EQUAL(projectv1.m_url, "");
    BOOST_CHECK_EQUAL(projectv1.m_timestamp, 0);
    BOOST_CHECK_EQUAL(projectv1.m_gdpr_controls, false);
    BOOST_CHECK(projectv1.m_public_key == CPubKey{});

    BOOST_CHECK(projectv1.WellFormed(GRC::ContractAction::REMOVE) == true);

    CDataStream streamv2 = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
            << uint32_t{2}
            << std::string("Enigma");

    GRC::Project projectv2;
    projectv2.Unserialize(streamv2, GRC::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL(projectv2.m_version, uint32_t {2});
    BOOST_CHECK_EQUAL(projectv2.m_name, "Enigma");
    BOOST_CHECK_EQUAL(projectv2.m_url, "");
    BOOST_CHECK_EQUAL(projectv2.m_timestamp, 0);
    BOOST_CHECK_EQUAL(projectv2.m_gdpr_controls, false);
    BOOST_CHECK(projectv2.m_public_key == CPubKey{});

    BOOST_CHECK(projectv2.WellFormed(GRC::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// WhitelistSnapshot
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(WhitelistSnapshot)

BOOST_AUTO_TEST_CASE(it_is_iterable)
{
    GRC::WhitelistSnapshot s(std::make_shared<GRC::ProjectList>(GRC::ProjectList {
        GRC::Project("Enigma", "http://enigma.test/@", 1234567),
        GRC::Project("Einstein@home", "http://einsteinathome.org/@", 1234567),
    }));

    auto counter = 0;

    for (auto const& project : s) {
        BOOST_CHECK(project.m_timestamp == 1234567);
        counter++;
    }

    BOOST_CHECK(counter == 2);
}

BOOST_AUTO_TEST_CASE(it_counts_the_number_of_projects)
{
    GRC::WhitelistSnapshot s1(std::make_shared<GRC::ProjectList>());

    BOOST_CHECK(s1.size() == 0);

    GRC::WhitelistSnapshot s2(std::make_shared<GRC::ProjectList>(GRC::ProjectList {
        GRC::Project("Enigma", "http://enigma.test/@", 1234567),
        GRC::Project("Einstein@home", "http://einsteinathome.org/@", 1234567),
    }));

    BOOST_CHECK(s2.size() == 2);
}

BOOST_AUTO_TEST_CASE(it_indicates_whether_it_contains_any_projects)
{
    GRC::WhitelistSnapshot s1(std::make_shared<GRC::ProjectList>());

    BOOST_CHECK(s1.Populated() == false);

    GRC::WhitelistSnapshot s2(std::make_shared<GRC::ProjectList>(GRC::ProjectList {
        GRC::Project("Enigma", "http://enigma.test/@", 1234567),
        GRC::Project("Einstein@home", "http://einsteinathome.org/@", 1234567),
    }));

    BOOST_CHECK(s2.Populated() == true);
}

BOOST_AUTO_TEST_CASE(it_sorts_a_copy_of_the_projects_by_name)
{
    // WhitelistSnapshot performs a case-insensitive sort, so we add an upper-
    // case project name to verify:
    GRC::WhitelistSnapshot snapshot = GRC::WhitelistSnapshot(
        std::make_shared<GRC::ProjectList>(GRC::ProjectList {
            GRC::Project("c", "http://c.example.com/@", 1234567),
            GRC::Project("a", "http://a.example.com/@", 1234567),
            GRC::Project("B", "http://b.example.com/@", 1234567),
        }))
        .Sorted();

    auto counter = 0;

    // Project doesn't implement comparison or stream operators, so we cannot
    // use the BOOST_CHECK_EQUAL_COLLECTIONS assertion:
    for (auto const& project : snapshot) {
        switch (counter) {
            case 0: BOOST_CHECK(project.m_name == "a"); break;
            case 1: BOOST_CHECK(project.m_name == "B"); break;
            case 2: BOOST_CHECK(project.m_name == "c"); break;
        }

        counter++;
    }
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Whitelist
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Whitelist)

BOOST_AUTO_TEST_CASE(it_adds_whitelisted_projects_from_contract_data)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    whitelist.Reset();

    int height = 0;
    int64_t time = 0;

    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).size() == 0);
    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Enigma") == false);

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, false);

    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).size() == 1);
    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Enigma") == true);

    AddProjectEntry(2, "Foo", "http://foo.test", false, height++, time++, false);

    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).size() == 2);
    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Enigma") == true);
    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Foo") == true);
}

BOOST_AUTO_TEST_CASE(it_removes_whitelisted_projects_from_contract_data)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);

    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).size() == 1);
    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Enigma") == true);

    DeleteProjectEntry(1, "Enigma", height++, time++, false);

    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).size() == 0);
    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Enigma") == false);
}

BOOST_AUTO_TEST_CASE(it_does_not_mutate_existing_snapshots)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);
    AddProjectEntry(2, "Foo", "http://foo.test", true, height++, time++, false);

    auto snapshot = whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false);

    DeleteProjectEntry(1, "Enigma", height, time, false);

    BOOST_CHECK(snapshot.Contains("Enigma") == true);

    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Enigma") == false);
    BOOST_CHECK(whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false).Contains("Foo") == true);
}

BOOST_AUTO_TEST_CASE(it_overwrites_projects_with_the_same_name)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);
    AddProjectEntry(2, "Enigma", "http://new.enigma.test", true, height++, time++, false);

    auto snapshot = whitelist.Snapshot(GRC::ProjectEntry::ProjectFilterFlag::ACTIVE, false);
    BOOST_CHECK(snapshot.size() == 1);

    for (const auto& project : snapshot) {
        BOOST_CHECK(project.m_url == "http://new.enigma.test");
    }
}

BOOST_AUTO_TEST_CASE(it_reads_raw_contract_status_from_leveldb)
{
    using Status = GRC::ProjectEntryStatus;

    GRC::Whitelist& registry = GRC::GetWhitelist();

    // Reset clears the in-memory maps AND the (in-memory test) LevelDB storage, so the disk reads below see only the
    // entries this test applies.
    registry.Reset();

    const std::string name = "RawStatusTest";
    const std::string url = "https://rawstatustest.example/@";

    // Plain ACTIVE add (handler maps ADD + UNKNOWN -> ACTIVE), persisted to LevelDB.
    AddProjectEntryWithStatus(name, url, Status::UNKNOWN, 100, 100);
    {
        const auto raw = registry.GetProjectsFromDisk();
        BOOST_REQUIRE(raw.count(name) == 1);
        BOOST_CHECK(raw.at(name)->m_status == Status::ACTIVE);
    }

    // A later contract for the same key becomes the HEAD that GetProjectsFromDisk reports.
    AddProjectEntryWithStatus(name, url, Status::MAN_GREYLISTED, 101, 101);
    {
        const auto raw = registry.GetProjectsFromDisk();
        BOOST_REQUIRE(raw.count(name) == 1);
        BOOST_CHECK(raw.at(name)->m_status == Status::MAN_GREYLISTED);
    }

    AddProjectEntryWithStatus(name, url, Status::AUTO_GREYLIST_OVERRIDE, 102, 102);
    {
        const auto raw = registry.GetProjectsFromDisk();
        BOOST_REQUIRE(raw.count(name) == 1);
        BOOST_CHECK(raw.at(name)->m_status == Status::AUTO_GREYLIST_OVERRIDE);
    }

    // A delete persists a DELETED HEAD, which the raw read reports as-is.
    DeleteProjectEntry(3, name, 103, 103);
    {
        const auto raw = registry.GetProjectsFromDisk();
        BOOST_REQUIRE(raw.count(name) == 1);
        BOOST_CHECK(raw.at(name)->m_status == Status::DELETED);
    }

    // This locks in that getrawprojectstatus (via GetProjectsFromDisk) reports the raw, contract-applied status read
    // from LevelDB, and that HEAD selection follows the latest record per key. The complementary assertion -- that the
    // AutoGreylist Snapshot overlay can rewrite the in-memory status to AUTO_GREYLISTED while the persisted status read
    // here stays ACTIVE/MAN/OVERRIDE -- belongs with the bad-project autogreylist run once it is extended through this
    // same contract-application facility to the registry/LevelDB layer.

    registry.Reset();
}

BOOST_AUTO_TEST_CASE(it_auto_greylists_correctly)
{
    std::vector<std::tuple<std::optional<uint64_t>,
                           AutoGreylistEntryState>> input_expected_result;

    /**
     * This is the input data for a HORRIBLE whitelisted project. This series of total credit
     * is designed to break every rule in the book. It starts out with no data collection at the
     * first superblock after whitelisting, and then proceeds to have lots of dropouts and zero
     * credit deltas to test the auto greylisting algorithm. Along the way the total credit deltas
     * are reduced to a level low enough to verify the triggering of the WAS rule. We go
     * enough superblocks to ensure the lookback is properly scoped once we are past 40 SB's from
     * baseline.
     *
     * No project is expected to be this bad, but we need to test out the algorithm here.
     **/

    // superblock 0 - baseline after whitelisting, but no stats due to problem with project.
    input_expected_result.push_back(
        std::make_tuple(
            std::optional<uint64_t>(),
            AutoGreylistEntryState(
                0, // zcd_20_SB_count
                0, // TC_7_SB_sum
                0, // TC_40_SB_sum
                0, // was
                false // meets_greylist_crit
                )
            )
        );

    // superblock 1 - first stats collection from project, TC = 1000
    input_expected_result.push_back(
        std::make_tuple(
            1000,
            AutoGreylistEntryState(
                1,
                0,
                0,
                0,
                false
                )
            )
        );

    // superblock 2 - no stats again due to problem with project
    input_expected_result.push_back(
        std::make_tuple(
            std::optional<uint64_t>(),
            AutoGreylistEntryState(
                2,
                0,
                0,
                0,
                false
                )
            )
        );

    // superblock 3 - no status again due to continuing problem with project
    input_expected_result.push_back(
        std::make_tuple(
            std::optional<uint64_t>(),
            AutoGreylistEntryState(
                3,
                0,
                0,
                0,
                false
                )
            )
        );

    // superblock 4 - successful stats collection, but TC = 1000 still
    input_expected_result.push_back(
        std::make_tuple(
            1000,
            AutoGreylistEntryState(
                4,
                0,
                0,
                0,
                false
                )
            )
        );

    // superblock 5 - successful stats collection, TC = 2000
    input_expected_result.push_back(
        std::make_tuple(
            2000,
            AutoGreylistEntryState(
                4,
                1000,
                1000,
                Fraction(2000/5,2000/5),
                false
                )
            )
        );

    // superblock 6 - successful stats collection, TC = 3000
    input_expected_result.push_back(
        std::make_tuple(
            3000,
            AutoGreylistEntryState(
                4,
                2000,
                2000,
                Fraction(2000/6,2000/6),
                false
                )
            )
        );

    // superblock 7 - problem with stats collection
    input_expected_result.push_back(
        std::make_tuple(
            std::optional<uint64_t>(),
            AutoGreylistEntryState(
                5,
                2000,
                2000,
                Fraction(2000/7,2000/7),
                false
                )
            )
        );

    // superblock 8 - successful stats collection, TC = 3000 still
    input_expected_result.push_back(
        std::make_tuple(
            3000,
            AutoGreylistEntryState(
                6,
                2000,
                2000,
                Fraction(2000/7,2000/8),
                false
                )
            )
        );

    // superblock 9 - successful stats collection, TC = 4000
    input_expected_result.push_back(
        std::make_tuple(
            4000,
            AutoGreylistEntryState(
                6,
                3000,
                3000,
                Fraction(3000/7,3000/9),
                false
                )
            )
        );

    // superblock 10 - successful stats collection, TC = 5000
    input_expected_result.push_back(
        std::make_tuple(
            5000,
            AutoGreylistEntryState(
                6,
                4000,
                4000,
                Fraction(4000/7,4000/10),
                false
                )
            )
        );

    // superblock 11 - successful stats collection, TC = 5000 still
    input_expected_result.push_back(
        std::make_tuple(
            5000,
            AutoGreylistEntryState(
                7,
                4000,
                4000,
                Fraction(4000/7,4000/11),
                false
                )
            )
        );

    // superblock 12 - problem with stats collection
    input_expected_result.push_back(
        std::make_tuple(
            std::optional<uint64_t>(),
            AutoGreylistEntryState(
                8,
                3000,
                4000,
                Fraction(3000/7,4000/12),
                true
                )
            )
        );

    // superblock 13 - successful stats collection, reduced output TC = 5001
    input_expected_result.push_back(
        std::make_tuple(
            5001,
            AutoGreylistEntryState(
                8,
                2001,
                4001,
                Fraction(2001/7,4001/13),
                true
                )
            )
        );

    // superblock 14 - successful stats collection, reduced output TC = 5002
    input_expected_result.push_back(
        std::make_tuple(
            5002,
            AutoGreylistEntryState(
                8,
                2002,
                4002,
                Fraction(2002/7,4002/14),
                true
                )
            )
        );

    // superblock 15 - successful stats collection, reduced output TC = 5003
    input_expected_result.push_back(
        std::make_tuple(
            5003,
            AutoGreylistEntryState(
                8,
                2003,
                4003,
                Fraction(2003/7,4003/15),
                true
                )
            )
        );

    // superblock 16 - successful stats collection, reduced output TC = 5004
    input_expected_result.push_back(
        std::make_tuple(
            5004,
            AutoGreylistEntryState(
                8,
                1004,
                4004,
                Fraction(1004/7,4004/16),
                true
                )
            )
        );

    // superblock 17 - successful stats collection, slightly improved output TC = 5200
    input_expected_result.push_back(
        std::make_tuple(
            5200,
            AutoGreylistEntryState(
                8,
                200,
                4200,
                Fraction(200/7,4200/17),
                true
                )
            )
        );

    // superblock 18 - successful stats collection, degraded output TC = 5225
    input_expected_result.push_back(
        std::make_tuple(
            5225,
            AutoGreylistEntryState(
                8,
                225,
                4225,
                Fraction(225/7,4225/18),
                true
                )
            )
        );

    // superblock 19 - successful stats collection, degraded output TC = 5230
    input_expected_result.push_back(
        std::make_tuple(
            5230,
            AutoGreylistEntryState(
                8,
                229,
                4230,
                Fraction(230/7,4230/19),
                true
                )
            )
        );

    // superblock 20 - successful stats collection, further degraded output TC = 5231
    input_expected_result.push_back(
        std::make_tuple(
            5231,
            AutoGreylistEntryState(
                8,
                230,
                4231,
                Fraction(230/7,4231/20),
                true
                )
            )
        );

    // superblock 21 - successful stats collection, further degraded output TC = 5232
    input_expected_result.push_back(
        std::make_tuple(
            5232,
            AutoGreylistEntryState(
                7,
                230,
                4232,
                Fraction(230/7,4232/21),
                false
                )
            )
        );

    // superblock 22 - successful stats collection, further degraded output TC = 5233
    input_expected_result.push_back(
        std::make_tuple(
            5233,
            AutoGreylistEntryState(
                6,
                230,
                4233,
                Fraction(230/7,4233/22),
                false
                )
            )
        );

    // superblock 23 - successful stats collection, further degraded output TC = 5234
    input_expected_result.push_back(
        std::make_tuple(
            5234,
            AutoGreylistEntryState(
                5,
                230,
                4234,
                Fraction(230/7,4234/23),
                false
                )
            )
        );

    // superblock 24 - successful stats collection, further degraded output TC = 5235
    input_expected_result.push_back(
        std::make_tuple(
            5235,
            AutoGreylistEntryState(
                4,
                35,
                4235,
                Fraction(35/7,4235/24),
                true
                )
            )
        );

    // superblock 25 - successful stats collection, degraded output TC = 5250
    input_expected_result.push_back(
        std::make_tuple(
            5250,
            AutoGreylistEntryState(
                4,
                25,
                4250,
                Fraction(25/7,4250/25),
                true
                )
            )
        );

    // superblock 26 - successful stats collection, resume "normal" output TC = 6250
    input_expected_result.push_back(
        std::make_tuple(
            6250,
            AutoGreylistEntryState(
                4,
                1020,
                5250,
                Fraction(1020/7,5250/26),
                false
                )
            )
        );

    // superblock 27 - successful stats collection, TC = 7000
    input_expected_result.push_back(
        std::make_tuple(
            7000,
            AutoGreylistEntryState(
                3,
                1769,
                6000,
                Fraction(1769/7,6000/27),
                false
                )
            )
        );

    // superblock 28 - successful stats collection, TC = 8000
    input_expected_result.push_back(
        std::make_tuple(
            8000,
            AutoGreylistEntryState(
                2,
                2768,
                7000,
                Fraction(2768/7,7000/28),
                false
                )
            )
        );

    // superblock 29 - successful stats collection, TC = 9000
    input_expected_result.push_back(
        std::make_tuple(
            9000,
            AutoGreylistEntryState(
                2,
                3767,
                8000,
                Fraction(3767/7,8000/29),
                false
                )
            )
        );

    // superblock 30 - successful stats collection, TC = 10000
    input_expected_result.push_back(
        std::make_tuple(
            10000,
            AutoGreylistEntryState(
                2,
                4766,
                9000,
                Fraction(4766/7,9000/30),
                false
                )
            )
        );

    // superblock 31 - successful stats collection, TC = 11000
    input_expected_result.push_back(
        std::make_tuple(
            11000,
            AutoGreylistEntryState(
                1,
                5765,
                10000,
                Fraction(5765/7,10000/31),
                false
                )
            )
        );

    // superblock 32 - successful stats collection, TC = 12000
    input_expected_result.push_back(
        std::make_tuple(
            12000,
            AutoGreylistEntryState(
                1,
                6750,
                11000,
                Fraction(6750/7,11000/32),
                false
                )
            )
        );

    // superblock 33 - successful stats collection, TC = 13000
    input_expected_result.push_back(
        std::make_tuple(
            13000,
            AutoGreylistEntryState(
                0,
                6750,
                12000,
                Fraction(6750/7,12000/33),
                false
                )
            )
        );

    // superblock 34 - successful stats collection, TC = 14000
    input_expected_result.push_back(
        std::make_tuple(
            14000,
            AutoGreylistEntryState(
                0,
                7000,
                13000,
                Fraction(7000/7,13000/34),
                false
                )
            )
        );

    // superblock 35 - successful stats collection, TC = 15000
    input_expected_result.push_back(
        std::make_tuple(
            15000,
            AutoGreylistEntryState(
                0,
                7000,
                14000,
                Fraction(7000/7,14000/35),
                false
                )
            )
        );

     // superblock 36 - successful stats collection, TC = 16000
    input_expected_result.push_back(
        std::make_tuple(
            16000,
            AutoGreylistEntryState(
                0,
                7000,
                15000,
                Fraction(7000/7,15000/36),
                false
                )
            )
        );

     // superblock 37 - successful stats collection, TC = 17000
    input_expected_result.push_back(
        std::make_tuple(
            17000,
            AutoGreylistEntryState(
                0,
                7000,
                16000,
                Fraction(7000/7,16000/37),
                false
                )
            )
        );

     // superblock 38 - successful stats collection, TC = 18000
    input_expected_result.push_back(
        std::make_tuple(
            18000,
            AutoGreylistEntryState(
                0,
                7000,
                17000,
                Fraction(7000/7,17000/38),
                false
                )
            )
        );

     // superblock 39 - successful stats collection, TC = 19000
    input_expected_result.push_back(
        std::make_tuple(
            19000,
            AutoGreylistEntryState(
                0,
                7000,
                18000,
                Fraction(7000/7,18000/39),
                false
                )
            )
        );

     // superblock 40 - successful stats collection, TC = 20000
    input_expected_result.push_back(
        std::make_tuple(
            20000,
            AutoGreylistEntryState(
                0,
                7000,
                19000,
                Fraction(7000/7,19000/40),
                false
                )
            )
        );

    // superblock 41 - successful stats collection, TC = 21000
    input_expected_result.push_back(
        std::make_tuple(
            21000,
            AutoGreylistEntryState(
                0,
                7000,
                20000,
                Fraction(7000/7, 20000/40),
                false
                )
            )
        );

    // superblock 42 - successful stats collection, TC = 22000
    input_expected_result.push_back(
        std::make_tuple(
            22000,
            AutoGreylistEntryState(
                0,
                7000,
                21000,
                Fraction(7000/7, 21000/40),
                false
                )
            )
        );

    // superblock 43 - successful stats collection, TC = 23000
    input_expected_result.push_back(
        std::make_tuple(
            23000,
            AutoGreylistEntryState(
                0,
                7000,
                22000,
                Fraction(7000/7, 22000/40),
                false
                )
            )
        );

    // superblock 44 - successful stats collection, TC = 24000
    input_expected_result.push_back(
        std::make_tuple(
            24000,
            AutoGreylistEntryState(
                0,
                7000,
                23000,
                Fraction(7000/7, 23000/40),
                false
                )
            )
        );

    // superblock 45 - successful stats collection, TC = 25000
    input_expected_result.push_back(
        std::make_tuple(
            25000,
            AutoGreylistEntryState(
                0,
                7000,
                23000,
                Fraction(7000/7, 23000/40),
                false
                )
            )
        );

    // superblock 46 - successful stats collection, TC = 26000
    input_expected_result.push_back(
        std::make_tuple(
            26000,
            AutoGreylistEntryState(
                0,
                7000,
                23000,
                Fraction(7000/7, 23000/40),
                false
                )
            )
        );

    /**
     * The point of this unit test is to exercise the rules in the AutoGreylist class to ensure they work correctly.
     * A set of "unit_test_blocks" is made which are fixed up superblocks using the above test data, which is then
     * injected into the RefreshWithSuperblock via the special unit test parameter.
     *
     * The results are compared with a lhs vs rhs comparison, using the == operator overload in the anonymous namespace
     * AutoGreylistEntryState put here specifically for this purpose.
     */

    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    std::shared_ptr<GRC::AutoGreylist> auto_greylist = GRC::GetAutoGreylistCache();

    whitelist.Reset();

    int height = 0;
    int64_t time = 0;

    auto unit_test_blocks = std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

    // Add a project for testing.
    AddProjectEntry(3, "autogreylist_test", "http://autogreylist.test", false, height, time, true);

    // Create dummy CBlockIndex for the whitelist entry.
    CBlockIndex* whitelist_index_entry = new CBlockIndex;

    ++height;
    ++time;

//    CBlockIndex* index_ptr = new CBlockIndex;
    CBlockIndex* index_ptr = whitelist_index_entry;
    CBlockIndex* index_ptr_prev = nullptr;

    for (auto iter : input_expected_result) {
        // Reset the auto greylist
        auto_greylist->Reset();

        // Build a fake superblock ptr for the test.
        index_ptr_prev = index_ptr;

        index_ptr = new CBlockIndex;

        index_ptr->nHeight = height;
        index_ptr->nTime = time;
        index_ptr->MarkAsSuperblock();
        index_ptr->pprev = index_ptr_prev;

        GRC::Superblock superblock = GRC::Superblock();

        // If the optional is nullopt, then don't insert into the superblock.
        if (std::get<0>(iter)) {
            superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                .insert(std::make_pair("autogreylist_test", *std::get<0>(iter)));
        }

        GRC::SuperblockPtr superblock_ptr = GRC::SuperblockPtr();
        superblock_ptr.Replace(superblock);
        superblock_ptr.Rebind(index_ptr);

        unit_test_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));

        auto_greylist->RefreshWithSuperblock(superblock_ptr, unit_test_blocks);

        // Only one project in the test.
        auto greylist_candidate = auto_greylist->begin()->second;
        auto last_history_entry = greylist_candidate.GetUpdateHistory().back();

        AutoGreylistEntryState entry_state_lhs(greylist_candidate.m_zcd_20_SB_count,
                                               greylist_candidate.m_TC_7_SB_sum,
                                               greylist_candidate.m_TC_40_SB_sum,
                                               greylist_candidate.GetWAS(),
                                               greylist_candidate.m_meets_greylisting_crit);

        AutoGreylistEntryState entry_state_rhs = std::get<1>(iter);

        LogPrintf("info: %s, height %i, sb %i", "it_auto_greylists_correctly", height, height - 1);

        if (entry_state_lhs != entry_state_rhs) {
            error("%s:\n"
                  "lhs: zcd_20_SB_count = %u, TC_7_SB_sum = %" PRId64 ", TC_40_SB_sum = %" PRId64
                  ", was = %s, meets_greylisting_crit = %u \n"
                  "rhs: zcd_20_SB_count = %u, TC_7_SB_sum = %" PRId64 ", TC_40_SB_sum = %" PRId64
                  ", was = %s, meets_greylisting_crit = %u",
                  "it_auto_greylists_correctly",
                  entry_state_lhs.m_zcd_20_SB_count,
                  entry_state_lhs.m_TC_7_SB_sum,
                  entry_state_lhs.m_TC_40_SB_sum,
                  entry_state_lhs.m_was.ToString(),
                  entry_state_lhs.m_meets_greylisting_crit,
                  entry_state_rhs.m_zcd_20_SB_count,
                  entry_state_rhs.m_TC_7_SB_sum,
                  entry_state_rhs.m_TC_40_SB_sum,
                  entry_state_rhs.m_was.ToString(),
                  entry_state_rhs.m_meets_greylisting_crit
                  );
        }

        BOOST_CHECK(entry_state_lhs == entry_state_rhs);

        ++height;
    }

    // delete CBlockIndex pointers and clear map to prevent leaking memory.
    for (auto& iter : *unit_test_blocks) {
        delete iter.second.first;
    }

    unit_test_blocks->clear();

    delete whitelist_index_entry;
}

//!
//! \brief Snapshot's auto-greylist overlay must NOT mutate the underlying registry entries.
//!
//! Reproduces the consensus bug in Whitelist::Snapshot(): the override working copy at
//! project.cpp:665 is a SHALLOW copy of ProjectEntryMap (std::map<std::string,
//! std::shared_ptr<ProjectEntry>>), so setting m_status = AUTO_GREYLISTED on the copy
//! (project.cpp:680) mutates the SHARED registry ProjectEntry in place. There is no revert
//! path, so a recovered project stays stuck AUTO_GREYLISTED in memory while a restarted node
//! reloads ACTIVE from LevelDB -- the in-memory-vs-persisted divergence that splits the network.
//!
//! Drives a project into the auto-greylist (reusing the it_auto_greylists_correctly "horrible
//! project" series through the point it meets criteria), takes a Snapshot with the overlay,
//! then asserts (1) the in-memory registry entry is still ACTIVE and (2) it agrees with the
//! persisted (LevelDB) contract status. Both FAIL on the buggy shallow-copy Snapshot and pass
//! once Snapshot deep-copies the entries before overlaying.
//!
BOOST_AUTO_TEST_CASE(snapshot_overlay_must_not_mutate_registry_entries)
{
    using Status = GRC::ProjectEntryStatus;
    using Filter = GRC::ProjectEntry::ProjectFilterFlag;

    GRC::Whitelist& whitelist = GRC::GetWhitelist();
    std::shared_ptr<GRC::AutoGreylist> auto_greylist = GRC::GetAutoGreylistCache();

    const std::string name = "snapshot_mutation_test";
    const std::string url = "http://snapshot.mutation.test";

    int height = 0;
    int64_t time = 0;

    whitelist.Reset();

    // RAII guard so a mid-test BOOST_REQUIRE abort doesn't leak -autogreylistdeepcopyheight=0
    // into subsequent test cases in the same binary.
    struct DeepCopyHeightGuard {
        ~DeepCopyHeightGuard()
        {
            gArgs.ForceSetArg("-autogreylistdeepcopyheight",
                              ToString(Params().GetConsensus().AutoGreylistDeepCopyHeight));
        }
    } deep_copy_height_guard;

    // Activate the deep-copy gate for this test (low override) so Snapshot exercises the post-gate
    // registry-safe overlay. Pre-fix code has no gate and mutates regardless -> the test fails (red);
    // post-fix code deep-copies the entries -> the registry is untouched (green).
    gArgs.ForceSetArg("-autogreylistdeepcopyheight", "0");

    // Add the project as a real v4 ACTIVE contract through the registry/contract/LevelDB path
    // (ADD + UNKNOWN -> ACTIVE; a real height is supplied so it persists and GetProjectsFromDisk
    // can read it back).
    AddProjectEntryWithStatus(name, url, Status::UNKNOWN, height, time);

    // Sanity: the contract status is ACTIVE on disk to start.
    {
        const auto disk = whitelist.GetProjectsFromDisk();
        BOOST_REQUIRE(disk.count(name) == 1);
        BOOST_REQUIRE(disk.at(name)->m_status == Status::ACTIVE);
    }

    // Drive the AutoGreylist with the "horrible project" total-credit series through the point
    // where it meets greylisting criteria (subset of it_auto_greylists_correctly's data; the
    // project meets criteria from superblock 12 onward).
    const std::vector<std::optional<uint64_t>> tc_series = {
        std::nullopt, 1000, std::nullopt, std::nullopt, 1000, 2000, 3000, std::nullopt,
        3000, 4000, 5000, 5000, std::nullopt, 5001, 5002,
    };

    auto unit_test_blocks =
        std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

    CBlockIndex* whitelist_index_entry = new CBlockIndex;
    ++height;
    ++time;

    CBlockIndex* index_ptr = whitelist_index_entry;
    CBlockIndex* index_ptr_prev = nullptr;
    GRC::SuperblockPtr superblock_ptr;

    for (const auto& tc : tc_series) {
        index_ptr_prev = index_ptr;
        index_ptr = new CBlockIndex;
        index_ptr->nHeight = height;
        index_ptr->nTime = time;
        index_ptr->MarkAsSuperblock();
        index_ptr->pprev = index_ptr_prev;

        GRC::Superblock superblock = GRC::Superblock();
        if (tc) {
            superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                .insert(std::make_pair(name, *tc));
        }

        superblock_ptr = GRC::SuperblockPtr();
        superblock_ptr.Replace(superblock);
        superblock_ptr.Rebind(index_ptr);

        unit_test_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));

        ++height;
        ++time;
    }

    auto_greylist->Reset();
    auto_greylist->RefreshWithSuperblock(superblock_ptr, unit_test_blocks);

    // Precondition: the project now meets greylisting criteria (is in the auto-greylist).
    BOOST_REQUIRE(auto_greylist->Contains(name));

    // Take a Snapshot WITH the overlay applied. On the buggy shallow-copy Snapshot this mutates
    // the shared registry ProjectEntry in place. The test drives RefreshWithSuperblock above so the
    // greylist reflects the synthetic superblock series; the chain-handler-driven Refresh path is
    // bypassed in this unit-test setup.
    whitelist.Snapshot(Filter::ALL_BUT_DELETED, /*include_override=*/true);

    // Read back the in-memory registry status WITHOUT re-applying the overlay
    // (include_override=false) -- this reflects whatever the previous Snapshot left behind in
    // m_project_entries.
    Status in_memory_status = Status::UNKNOWN;
    bool found = false;
    for (const auto& entry : whitelist.Snapshot(Filter::ALL_BUT_DELETED, false)) {
        if (entry.m_name == name) {
            in_memory_status = entry.m_status.Value();
            found = true;
        }
    }
    BOOST_REQUIRE(found);

    // The persisted (LevelDB) contract status is never overlaid.
    Status disk_status = Status::UNKNOWN;
    {
        const auto disk = whitelist.GetProjectsFromDisk();
        BOOST_REQUIRE(disk.count(name) == 1);
        disk_status = disk.at(name)->m_status.Value();
    }

    LogPrintf("snapshot_overlay_must_not_mutate_registry_entries: in_memory=%d disk=%d "
              "(ACTIVE=%d AUTO_GREYLISTED=%d)",
              static_cast<int>(in_memory_status), static_cast<int>(disk_status),
              static_cast<int>(Status::ACTIVE), static_cast<int>(Status::AUTO_GREYLISTED));

    // (1) The overlay must not have mutated the underlying registry entry.
    BOOST_CHECK(in_memory_status == Status::ACTIVE);

    // (2) In-memory and persisted status must agree -- divergence here is the fork vector.
    BOOST_CHECK(in_memory_status == disk_status);

    // Clean up the heap-allocated test block indices.
    for (auto& iter : *unit_test_blocks) {
        delete iter.second.first;
    }
    unit_test_blocks->clear();
    delete whitelist_index_entry;

    // gArgs restore handled by DeepCopyHeightGuard's dtor on scope exit.

    whitelist.Reset();
}

BOOST_AUTO_TEST_CASE(it_applies_benefit_of_doubt_correctly)
{
    /**
     * This test exercises the "Benefit of the Doubt" logic in the AutoGreylist system. When a staking node's scraper
     * fails to reach a project, the head superblock is missing the project's total credit entry. Without the
     * benefit-of-doubt fix, std::optional comparison semantics cause the valid historical SB at sb_from_baseline == 1
     * to be counted as a false ZCD (because any engaged optional >= nullopt in C++17). The fix suppresses this false
     * ZCD at the head position.
     *
     * The test also verifies the "deferred penalty": once a good SB becomes the new head and the bad SB ages to
     * sb_from_baseline == 1, the missing data is correctly counted as a ZCD.
     *
     * Scenario A (benefit-of-doubt ON, head missing):
     *   Head (nullopt) -> SB-1 (TC=1000) -> SB-2 (TC=500)
     *   Expected: ZCD = 0 (false ZCD suppressed at sb==1)
     *
     * Scenario A' (benefit-of-doubt OFF, same data):
     *   Expected: ZCD = 1 (false ZCD counted at sb==1)
     *
     * Scenario B (deferred penalty, good head after bad SB):
     *   Head (TC=2000) -> SB-1 (nullopt) -> SB-2 (TC=1000) -> SB-3 (TC=500)
     *   Expected: ZCD = 1 (nullopt at sb==1 correctly counted, benefit-of-doubt does not apply
     *   because head has data)
     */

    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    std::shared_ptr<GRC::AutoGreylist> auto_greylist = GRC::GetAutoGreylistCache();

    whitelist.Reset();

    int height = 0;
    int64_t time = 0;

    // Add a project for testing.
    AddProjectEntry(3, "bod_test", "http://bod.test", false, height, time, true);

    // Create dummy CBlockIndex for the whitelist entry.
    CBlockIndex* whitelist_index_entry = new CBlockIndex;

    ++height;
    ++time;

    // ---- Build the superblock chain: SB1(TC=500), SB2(TC=1000), SB3(nullopt), SB4(TC=2000) ----

    auto unit_test_blocks = std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

    CBlockIndex* index_ptr = whitelist_index_entry;
    CBlockIndex* index_ptr_prev = nullptr;

    // Helper to build a superblock at the next height.
    auto build_sb = [&](std::optional<uint64_t> tc) {
        index_ptr_prev = index_ptr;
        index_ptr = new CBlockIndex;
        index_ptr->nHeight = height;
        index_ptr->nTime = time;
        index_ptr->MarkAsSuperblock();
        index_ptr->pprev = index_ptr_prev;

        GRC::Superblock superblock = GRC::Superblock();

        if (tc) {
            superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                .insert(std::make_pair("bod_test", *tc));
        }

        GRC::SuperblockPtr superblock_ptr = GRC::SuperblockPtr();
        superblock_ptr.Replace(superblock);
        superblock_ptr.Rebind(index_ptr);

        unit_test_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));

        ++height;
        ++time;
    };

    build_sb(500);    // SB at height 1: TC = 500
    build_sb(1000);   // SB at height 2: TC = 1000
    build_sb({});     // SB at height 3: TC = nullopt (scraper failure)
    build_sb(2000);   // SB at height 4: TC = 2000

    // ---- Scenario A: Benefit-of-doubt ON, head is the bad SB (nullopt at height 3) ----
    // Head: nullopt -> sb_from_baseline==1: TC=1000 -> sb_from_baseline==2: TC=500
    // Expected: ZCD = 0 (benefit-of-doubt suppresses false ZCD at sb==1)

    const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = 0;

    auto_greylist->Reset();

    // Use SB at height 3 (nullopt) as the head.
    auto head_iter = unit_test_blocks->find(3);
    auto_greylist->RefreshWithSuperblock(head_iter->second.second, unit_test_blocks);

    {
        auto greylist_candidate = auto_greylist->begin()->second;

        LogPrintf("info: %s: Scenario A (BoD ON, head missing) - ZCD = %u (expected 0)",
                  "it_applies_benefit_of_doubt_correctly", greylist_candidate.m_zcd_20_SB_count);

        BOOST_CHECK_EQUAL(greylist_candidate.m_zcd_20_SB_count, 0);
    }

    // ---- Scenario A': Same data, benefit-of-doubt OFF ----
    // Expected: ZCD = 1 (TC=1000 >= nullopt bookmark at sb==1 -> false ZCD counted)

    const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = std::numeric_limits<int>::max();

    auto_greylist->Reset();

    auto_greylist->RefreshWithSuperblock(head_iter->second.second, unit_test_blocks);

    {
        auto greylist_candidate = auto_greylist->begin()->second;

        LogPrintf("info: %s: Scenario A' (BoD OFF, head missing) - ZCD = %u (expected 1)",
                  "it_applies_benefit_of_doubt_correctly", greylist_candidate.m_zcd_20_SB_count);

        BOOST_CHECK_EQUAL(greylist_candidate.m_zcd_20_SB_count, 1);
    }

    // ---- Scenario B: Deferred penalty — bad SB ages past head, benefit-of-doubt ON ----
    // Head: TC=2000 -> sb_from_baseline==1: nullopt -> sb_from_baseline==2: TC=1000 -> sb_from_baseline==3: TC=500
    // Expected: ZCD = 1 (nullopt at sb==1 correctly counted; benefit-of-doubt does NOT apply because head has data)

    const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = 0;

    auto_greylist->Reset();

    // Use SB at height 4 (TC=2000) as the head.
    head_iter = unit_test_blocks->find(4);
    auto_greylist->RefreshWithSuperblock(head_iter->second.second, unit_test_blocks);

    {
        auto greylist_candidate = auto_greylist->begin()->second;

        LogPrintf("info: %s: Scenario B (BoD ON, deferred penalty) - ZCD = %u (expected 1)",
                  "it_applies_benefit_of_doubt_correctly", greylist_candidate.m_zcd_20_SB_count);

        BOOST_CHECK_EQUAL(greylist_candidate.m_zcd_20_SB_count, 1);
    }

    // ---- Cleanup ----

    // Restore default (disabled) state.
    const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = std::numeric_limits<int>::max();

    for (auto& iter : *unit_test_blocks) {
        delete iter.second.first;
    }

    unit_test_blocks->clear();

    delete whitelist_index_entry;
}

//!
//! Verifies the deep-copy gate-crossing rebuild fires at the right chain-handler point.
//!
//! Sequence:
//!  1. Set -autogreylistdeepcopyheight=100 so SBs with m_height >= 100 are post-gate.
//!  2. Add a project as ACTIVE through the real contract + LevelDB path.
//!  3. Drive AutoGreylist to put the project into the greylist (replays the "horrible project" series).
//!  4. Snapshot WITH overlay while m_deep_copy_active is false (synthetic SBs are pre-gate by height)
//!     -- this exercises the LEGACY shallow-copy path and mutates m_project_entries->m_status in place
//!     to AUTO_GREYLISTED. Verify in-memory diverges from LevelDB (corruption simulated).
//!  5. Push a pre-gate SuperblockPtr (height 50) through Quorum::PushSuperblock. Gate crossing does
//!     not fire (prev empty, current pre-gate); in-memory corruption persists.
//!  6. Push a post-gate SuperblockPtr (height 100) through Quorum::PushSuperblock. Gate crossing
//!     fires (prev pre-gate, current post-gate); ReinitFromDisk rebuilds m_project_entries from
//!     LevelDB; in-memory status reverts to ACTIVE.
//!
//! Locks in the chain-handler wiring so a future refactor cannot silently delete the heal again
//! (the prior incarnation lived in Quorum::CommitSuperblock, which is structurally unreachable for
//! v2+ SBs in v11+ steady state -- the bug that motivated this test).
//!
BOOST_AUTO_TEST_CASE(push_superblock_heals_corruption_at_gate_crossing)
{
    using Status = GRC::ProjectEntryStatus;
    using Filter = GRC::ProjectEntry::ProjectFilterFlag;

    GRC::Whitelist& whitelist = GRC::GetWhitelist();
    std::shared_ptr<GRC::AutoGreylist> auto_greylist = GRC::GetAutoGreylistCache();

    const std::string name = "push_heal_test_project";
    const std::string url = "http://push.heal.test";

    whitelist.Reset();

    // RAII guard so a mid-test BOOST_REQUIRE failure doesn't leak -autogreylistdeepcopyheight=100
    // into subsequent test cases in the same binary. Also save/restore pindexBest: earlier tests in
    // the suite may leave pindexBest pointing at a stack-allocated CBlockIndex that has since gone
    // out of scope, and Quorum::PushSuperblock's Refresh path runs BlockFinder::FindByHeight which
    // dereferences pindexBest -- triggers ASan stack-use-after-return on CI. Set pindexBest=nullptr
    // for the duration of the test so FindByHeight returns immediately at its null-check.
    struct TestStateGuard {
        CBlockIndex* saved_pindexBest;
        TestStateGuard() : saved_pindexBest(pindexBest) { pindexBest = nullptr; }
        ~TestStateGuard()
        {
            pindexBest = saved_pindexBest;
            gArgs.ForceSetArg("-autogreylistdeepcopyheight",
                              ToString(Params().GetConsensus().AutoGreylistDeepCopyHeight));
        }
    } test_state_guard;

    // Activate the deep-copy gate with a test height. SBs with m_height < 100 are pre-gate; m_height
    // >= 100 are post-gate. The "horrible project" synthetic series uses heights 1..N so the
    // AutoGreylist's RefreshWithSuperblock leaves m_deep_copy_active=false at the end -- exactly the
    // state needed to trigger the legacy in-place overlay corruption below.
    gArgs.ForceSetArg("-autogreylistdeepcopyheight", "100");

    // ---- Step 1: add project as ACTIVE through real contract + LevelDB path ----
    // Non-zero height so LevelDB's height_stored persists past ReinitFromDisk's clear_in_memory_only.
    // Initialize bails when LoadDBHeight returns 0 (the registry_db's "uninitialized" sentinel).
    // Time=0 so RefreshWithSuperblock's project_first_active timestamp check (synth SB ts >= project ts)
    // succeeds for the synthetic SBs at time 1..15 below.
    AddProjectEntryWithStatus(name, url, Status::UNKNOWN, /*height=*/20, /*time=*/0);

    {
        const auto disk = whitelist.GetProjectsFromDisk();
        BOOST_REQUIRE(disk.count(name) == 1);
        BOOST_REQUIRE(disk.at(name)->m_status == Status::ACTIVE);
    }

    // ---- Step 2: drive AutoGreylist so the project meets greylisting criteria ----
    // Reused "horrible project" TC series from it_auto_greylists_correctly -- the project meets
    // criteria from sb_from_baseline >= 12.
    const std::vector<std::optional<uint64_t>> tc_series = {
        std::nullopt, 1000, std::nullopt, std::nullopt, 1000, 2000, 3000, std::nullopt,
        3000, 4000, 5000, 5000, std::nullopt, 5001, 5002,
    };

    auto unit_test_blocks =
        std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

    CBlockIndex* whitelist_index_entry = new CBlockIndex;
    int height = 1;
    int64_t time = 1;

    CBlockIndex* synth_index = whitelist_index_entry;
    CBlockIndex* synth_index_prev = nullptr;
    GRC::SuperblockPtr synth_head_ptr;

    for (const auto& tc : tc_series) {
        synth_index_prev = synth_index;
        synth_index = new CBlockIndex;
        synth_index->nHeight = height;
        synth_index->nTime = time;
        synth_index->MarkAsSuperblock();
        synth_index->pprev = synth_index_prev;

        GRC::Superblock sb;
        if (tc) {
            sb.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                .insert(std::make_pair(name, *tc));
        }

        synth_head_ptr = GRC::SuperblockPtr();
        synth_head_ptr.Replace(sb);
        synth_head_ptr.Rebind(synth_index);

        unit_test_blocks->insert(std::make_pair(height, std::make_pair(synth_index, synth_head_ptr)));

        ++height;
        ++time;
    }

    auto_greylist->Reset();
    auto_greylist->RefreshWithSuperblock(synth_head_ptr, unit_test_blocks);
    BOOST_REQUIRE(auto_greylist->Contains(name));

    // Synthetic SBs are at heights 1..N (all pre-gate at 100); m_deep_copy_active should be false.
    BOOST_REQUIRE(!auto_greylist->IsDeepCopyActive());

    // ---- Step 3: trigger the legacy in-place overlay mutation (corruption simulated) ----
    whitelist.Snapshot(Filter::ALL_BUT_DELETED, /*include_override=*/true);

    {
        Status in_memory_status = Status::UNKNOWN;
        bool found = false;
        for (const auto& entry : whitelist.Snapshot(Filter::ALL_BUT_DELETED, /*include_override=*/false)) {
            if (entry.m_name == name) {
                in_memory_status = entry.m_status.Value();
                found = true;
            }
        }
        BOOST_REQUIRE(found);
        BOOST_REQUIRE_EQUAL(static_cast<int>(in_memory_status), static_cast<int>(Status::AUTO_GREYLISTED));

        // Disk is untouched -- still ACTIVE -- because the legacy overlay mutates m_status in the
        // shared shared_ptr backing m_project_entries, not the registry DB.
        const auto disk = whitelist.GetProjectsFromDisk();
        BOOST_REQUIRE_EQUAL(static_cast<int>(disk.at(name)->m_status.Value()),
                            static_cast<int>(Status::ACTIVE));
    }

    // ---- Step 4: push a pre-gate SuperblockPtr through Quorum::PushSuperblock ----
    CBlockIndex* pre_gate_index = new CBlockIndex;
    pre_gate_index->nHeight = 50;   // pre-gate
    pre_gate_index->nTime = 50000;
    pre_gate_index->MarkAsSuperblock();

    GRC::Superblock pre_gate_sb;
    GRC::SuperblockPtr pre_gate_ptr;
    pre_gate_ptr.Replace(pre_gate_sb);
    pre_gate_ptr.Rebind(pre_gate_index);

    GRC::Quorum::PushSuperblock(pre_gate_ptr);

    // Gate did NOT cross (pre_gate_ptr at height 50 < gate 100); m_deep_copy_active stays false.
    BOOST_CHECK(!auto_greylist->IsDeepCopyActive());

    // Corruption persists -- pre-gate push does not cross the gate.
    {
        Status in_memory_status = Status::UNKNOWN;
        bool found = false;
        for (const auto& entry : whitelist.Snapshot(Filter::ALL_BUT_DELETED, /*include_override=*/false)) {
            if (entry.m_name == name) {
                in_memory_status = entry.m_status.Value();
                found = true;
            }
        }
        BOOST_REQUIRE(found);
        BOOST_CHECK_EQUAL(static_cast<int>(in_memory_status), static_cast<int>(Status::AUTO_GREYLISTED));
    }

    // ---- Step 5: push a post-gate SuperblockPtr through Quorum::PushSuperblock ----
    CBlockIndex* post_gate_index = new CBlockIndex;
    post_gate_index->nHeight = 100;   // post-gate (== AutoGreylistDeepCopyHeight)
    post_gate_index->nTime = 100000;
    post_gate_index->MarkAsSuperblock();
    post_gate_index->pprev = pre_gate_index;

    GRC::Superblock post_gate_sb;
    GRC::SuperblockPtr post_gate_ptr;
    post_gate_ptr.Replace(post_gate_sb);
    post_gate_ptr.Rebind(post_gate_index);

    GRC::Quorum::PushSuperblock(post_gate_ptr);

    // Gate crossed forward; m_deep_copy_active is now true. Asserting this separately from the
    // healed-status check below localizes future regressions: if this fails the gate detector itself
    // is broken; if this passes but the status check fails the ReinitFromDisk side-effect is broken.
    BOOST_CHECK(auto_greylist->IsDeepCopyActive());

    // Gate crossing fired -> ReinitFromDisk rebuilt m_project_entries from LevelDB ->
    // in-memory status is back to ACTIVE.
    {
        Status in_memory_status = Status::UNKNOWN;
        bool found = false;
        for (const auto& entry : whitelist.Snapshot(Filter::ALL_BUT_DELETED, /*include_override=*/false)) {
            if (entry.m_name == name) {
                in_memory_status = entry.m_status.Value();
                found = true;
            }
        }
        BOOST_REQUIRE(found);
        BOOST_CHECK_EQUAL(static_cast<int>(in_memory_status), static_cast<int>(Status::ACTIVE));
    }

    // ---- Cleanup ----
    // Pop the two Quorum-pushed SBs before deleting their CBlockIndex backing storage. Pop's Refresh
    // bails harmlessly when the cache becomes empty.
    GRC::Quorum::PopSuperblock(post_gate_index);
    GRC::Quorum::PopSuperblock(pre_gate_index);
    delete pre_gate_index;
    delete post_gate_index;

    for (auto& iter : *unit_test_blocks) {
        delete iter.second.first;
    }
    unit_test_blocks->clear();
    delete whitelist_index_entry;

    // gArgs restore handled by DeepCopyHeightGuard's dtor on scope exit.

    whitelist.Reset();
}

BOOST_AUTO_TEST_SUITE_END()

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
