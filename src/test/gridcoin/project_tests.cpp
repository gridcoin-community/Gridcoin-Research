// Copyright (c) 2014-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "gridcoin/autogreylist.h"
#include "gridcoin/autogreylist_v2.h"
#include "gridcoin/quorum.h"
#include "util/string.h"
#include "wallet/generated_type.h"
#include "chain.h"

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

    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).size() == 0);
    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Enigma") == false);

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, false);

    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).size() == 1);
    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Enigma") == true);

    AddProjectEntry(2, "Foo", "http://foo.test", false, height++, time++, false);

    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).size() == 2);
    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Enigma") == true);
    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Foo") == true);
}

BOOST_AUTO_TEST_CASE(it_removes_whitelisted_projects_from_contract_data)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);

    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).size() == 1);
    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Enigma") == true);

    DeleteProjectEntry(1, "Enigma", height++, time++, false);

    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).size() == 0);
    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Enigma") == false);
}

BOOST_AUTO_TEST_CASE(it_does_not_mutate_existing_snapshots)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);
    AddProjectEntry(2, "Foo", "http://foo.test", true, height++, time++, false);

    auto snapshot = whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE);

    DeleteProjectEntry(1, "Enigma", height, time, false);

    BOOST_CHECK(snapshot.Contains("Enigma") == true);

    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Enigma") == false);
    BOOST_CHECK(whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE).Contains("Foo") == true);
}

BOOST_AUTO_TEST_CASE(it_overwrites_projects_with_the_same_name)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);
    AddProjectEntry(2, "Enigma", "http://new.enigma.test", true, height++, time++, false);

    auto snapshot = whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ACTIVE);
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

    std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

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
//! \brief Consumer-side proof of the scraper "no_records" total-credit fix (v15,
//! IsAutoGreylistTotalCreditFixEnabled).
//!
//! A whitelisted project with a long, healthy, steadily-increasing total-credit history is given a single
//! head/baseline superblock in two shapes that the scraper produces for a project whose user-statistics
//! export returned no usable records that cycle:
//!
//!   * PRESENT-WITH-ZERO (pre-fix emit): the project appears in the superblock's ProjectsAllCpidTotalCredits
//!     map with total credit 0. The auto-greylist baseline bookmark becomes 0, which is never greater than any
//!     historical total credit, so every WAS delta is skipped and WAS collapses to 0 -> the project spuriously
//!     meets the greylist criteria.
//!
//!   * ABSENT (post-fix emit): the project is omitted from the map entirely. The baseline records nullopt
//!     (benefit-of-doubt) and WAS is computed from the real history -> the project does NOT meet the criteria.
//!
//! The only difference between the two runs is present-with-0 vs absent for the head superblock -- exactly the
//! distinction the scraper gate controls. A legitimately scraped zero (no_records == false) is unaffected by
//! the fix and is out of scope here; this test isolates the no-records pathology and its remedy.
//!
BOOST_AUTO_TEST_CASE(no_records_zero_baseline_does_not_collapse_was)
{
    // Drive the auto-greylist over a total-credit sequence (oldest first; the last entry is the baseline/head).
    // A nullopt entry models a project omitted from ProjectsAllCpidTotalCredits; a value models a present entry.
    // Returns {WAS as double, meets_greylist_criteria} for the head/baseline.
    auto run = [](const std::vector<std::optional<uint64_t>>& tc_sequence) {
        GRC::Whitelist& whitelist = GRC::GetWhitelist();
        std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

        whitelist.Reset();

        int height = 0;
        int64_t time = 0;

        auto unit_test_blocks = std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

        AddProjectEntry(3, "autogreylist_test", "http://autogreylist.test", false, height, time, true);

        CBlockIndex* whitelist_index_entry = new CBlockIndex;
        ++height;
        ++time;

        CBlockIndex* index_ptr = whitelist_index_entry;
        CBlockIndex* index_ptr_prev = nullptr;

        for (const auto& tc : tc_sequence) {
            auto_greylist->Reset();

            index_ptr_prev = index_ptr;
            index_ptr = new CBlockIndex;
            index_ptr->nHeight = height;
            index_ptr->nTime = time;
            index_ptr->MarkAsSuperblock();
            index_ptr->pprev = index_ptr_prev;

            GRC::Superblock superblock = GRC::Superblock();

            // nullopt -> omitted from the map (post-fix); a value (including 0) -> present (pre-fix when 0).
            if (tc) {
                superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                    .insert(std::make_pair("autogreylist_test", *tc));
            }

            GRC::SuperblockPtr superblock_ptr = GRC::SuperblockPtr();
            superblock_ptr.Replace(superblock);
            superblock_ptr.Rebind(index_ptr);

            unit_test_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));
            auto_greylist->RefreshWithSuperblock(superblock_ptr, unit_test_blocks);

            ++height;
            ++time;
        }

        auto candidate = auto_greylist->begin()->second;
        std::pair<double, bool> result(candidate.GetWAS().ToDouble(), candidate.m_meets_greylisting_crit);

        for (auto& it : *unit_test_blocks) delete it.second.first;
        unit_test_blocks->clear();
        delete whitelist_index_entry;

        return result;
    };

    // A healthy, steadily-increasing total-credit history: no zero-credit days, strong WAS, well past the
    // 7-superblock grace period.
    std::vector<std::optional<uint64_t>> healthy;
    for (uint64_t i = 1; i <= 12; ++i) healthy.push_back(i * 1000);

    // Variant A -- PRESENT-WITH-ZERO head (pre-fix): WAS collapses to 0 and the project spuriously meets the
    // greylist criteria.
    auto with_zero_head = healthy;
    with_zero_head.push_back(std::optional<uint64_t>(0));
    const auto zero_head = run(with_zero_head);

    BOOST_CHECK(zero_head.first == 0.0);
    BOOST_CHECK(zero_head.second == true);

    // Variant B -- ABSENT head (post-fix): WAS is computed from the real history and the project does NOT meet
    // the greylist criteria.
    auto with_absent_head = healthy;
    with_absent_head.push_back(std::optional<uint64_t>());
    const auto absent_head = run(with_absent_head);

    BOOST_CHECK(absent_head.first > 0.0);
    BOOST_CHECK(absent_head.second == false);
}

//!
//! \brief Below the redesign gate, the facade's state selectors must be degenerate.
//!
//! Stage 1 of the AutoGreylist V2 redesign converts every consumer to the required
//! GreylistState selector while all producer writes still go through the frozen V1 class.
//! The contract this pins: below AutoGreylistRedesignHeight, AUTHORITATIVE and PENDING
//! resolve to the same (V1) cache -- identical answers from Contains(), IsDeepCopyActive(),
//! Get() and the Snapshot() overlay -- and NONE applies no overlay at all. This is what makes
//! the call-site conversion carry no behavioral risk pre-gate.
//!
BOOST_AUTO_TEST_CASE(facade_state_selectors_are_degenerate_below_the_gate)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();
    std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

    whitelist.Reset();

    int height = 0;
    int64_t time = 0;

    auto unit_test_blocks = std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

    // Two projects: one with a flat total-credit history (meets the greylist criteria: WAS 0
    // and 20 zero-credit days) and one healthy riser (does not).
    AddProjectEntry(3, "flatproj", "http://flat.test", false, height, time, true);
    AddProjectEntry(3, "growproj", "http://grow.test", false, height, time, false);

    CBlockIndex* whitelist_index_entry = new CBlockIndex;
    ++height;
    ++time;

    CBlockIndex* index_ptr = whitelist_index_entry;
    CBlockIndex* index_ptr_prev = nullptr;

    for (uint64_t i = 1; i <= 12; ++i) {
        auto_greylist->Reset();

        index_ptr_prev = index_ptr;
        index_ptr = new CBlockIndex;
        index_ptr->nHeight = height;
        index_ptr->nTime = time;
        index_ptr->MarkAsSuperblock();
        index_ptr->pprev = index_ptr_prev;

        GRC::Superblock superblock = GRC::Superblock();

        superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
            .insert(std::make_pair("flatproj", 500000));
        superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
            .insert(std::make_pair("growproj", i * 1000));

        GRC::SuperblockPtr superblock_ptr = GRC::SuperblockPtr();
        superblock_ptr.Replace(superblock);
        superblock_ptr.Rebind(index_ptr);

        unit_test_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));
        auto_greylist->RefreshWithSuperblock(superblock_ptr, unit_test_blocks);

        ++height;
        ++time;
    }

    // Precondition on the fixture itself: the V1 walker greylists flatproj and not growproj.
    bool flat_meets = false;
    bool grow_meets = false;
    for (auto iter = auto_greylist->begin(); iter != auto_greylist->end(); ++iter) {
        if (iter->first == "flatproj") flat_meets = iter->second.m_meets_greylisting_crit;
        if (iter->first == "growproj") grow_meets = iter->second.m_meets_greylisting_crit;
    }
    BOOST_REQUIRE(flat_meets == true);
    BOOST_REQUIRE(grow_meets == false);

    // ---- Contains: the two overlay selectors agree with each other and with V1 truth; ----
    // ---- NONE always answers false. ----
    BOOST_CHECK(auto_greylist->Contains(GRC::GreylistState::PENDING, "flatproj") == true);
    BOOST_CHECK(auto_greylist->Contains(GRC::GreylistState::AUTHORITATIVE, "flatproj") == true);
    BOOST_CHECK(auto_greylist->Contains(GRC::GreylistState::PENDING, "growproj") == false);
    BOOST_CHECK(auto_greylist->Contains(GRC::GreylistState::AUTHORITATIVE, "growproj") == false);
    BOOST_CHECK(auto_greylist->Contains(GRC::GreylistState::NONE, "flatproj") == false);

    // only_auto_greylisted == false matches any candidate entry (V1 semantics), identically
    // through either selector.
    BOOST_CHECK(auto_greylist->Contains(GRC::GreylistState::PENDING, "growproj", false) == true);
    BOOST_CHECK(auto_greylist->Contains(GRC::GreylistState::AUTHORITATIVE, "growproj", false) == true);

    // ---- IsDeepCopyActive: identical across selectors (heights 1..N are below the ----
    // ---- deep-copy gate on MAIN, so both report the V1 answer: false). ----
    BOOST_CHECK(auto_greylist->IsDeepCopyActive(GRC::GreylistState::PENDING)
                == auto_greylist->IsDeepCopyActive(GRC::GreylistState::AUTHORITATIVE));
    BOOST_CHECK(auto_greylist->IsDeepCopyActive(GRC::GreylistState::PENDING) == false);

    // ---- Get: both selectors yield an engaged V1-tagged computation with no key, equal ----
    // ---- membership, and membership matching the V1 criteria flags. ----
    const auto pending = auto_greylist->Get(GRC::GreylistState::PENDING);
    const auto authoritative = auto_greylist->Get(GRC::GreylistState::AUTHORITATIVE);

    BOOST_REQUIRE(pending.has_value());
    BOOST_REQUIRE(authoritative.has_value());
    BOOST_CHECK(pending->m_version == GRC::GreylistVersion::V1);
    BOOST_CHECK(authoritative->m_version == GRC::GreylistVersion::V1);
    BOOST_CHECK(pending->m_key.IsNull());
    BOOST_CHECK(pending->m_from_record == false);
    BOOST_CHECK(pending->m_auto_greylisted == authoritative->m_auto_greylisted);
    BOOST_CHECK(pending->m_auto_greylisted == std::set<std::string>{"flatproj"});

    BOOST_CHECK(auto_greylist->Get(GRC::GreylistState::NONE).has_value() == false);

    // ---- Snapshot: NONE applies no overlay (read FIRST -- below the gate the legacy ----
    // ---- shallow-copy overlay mutates the registry in place, so overlay snapshots ----
    // ---- must come after); the two overlay selectors then produce identical views. ----
    for (const auto& entry : whitelist.Snapshot(GRC::GreylistState::NONE,
                                                GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED)) {
        BOOST_CHECK(entry.m_status == GRC::ProjectEntryStatus::ACTIVE);
    }

    std::map<std::string, GRC::ProjectEntryStatus> pending_status;
    for (const auto& entry : whitelist.Snapshot(GRC::GreylistState::PENDING,
                                                GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED)) {
        pending_status[entry.m_name] = entry.m_status.Value();
    }

    std::map<std::string, GRC::ProjectEntryStatus> authoritative_status;
    for (const auto& entry : whitelist.Snapshot(GRC::GreylistState::AUTHORITATIVE,
                                                GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED)) {
        authoritative_status[entry.m_name] = entry.m_status.Value();
    }

    BOOST_CHECK(pending_status == authoritative_status);
    BOOST_CHECK(pending_status["flatproj"] == GRC::ProjectEntryStatus::AUTO_GREYLISTED);
    BOOST_CHECK(pending_status["growproj"] == GRC::ProjectEntryStatus::ACTIVE);

    // Clean up the shared singletons and the synthetic chain.
    for (auto& it : *unit_test_blocks) delete it.second.first;
    unit_test_blocks->clear();
    delete whitelist_index_entry;

    auto_greylist->Reset();
    whitelist.Reset();
}

//!
//! \brief Differential harness, equality half: where no V2 correction applies, V2 == V1.
//!
//! Drives identical fixtures through the frozen V1 walker (with the audit height pinned to 0,
//! matching V2's hard-coded benefit-of-the-doubt behavior -- V2 only runs above the redesign
//! gate, which is never below the audit gate) and through AutoGreylistV2::Compute, and asserts
//! per-project equality of the greylist criteria flag, ZCD, both WAS endpoint sums, the WAS
//! value and the update-history length. The fixtures here are exactly those untouched by every
//! V2 correction: no recorded zeros with an older non-zero (latch), no missing WINDOW ENDPOINT
//! (divisor contraction -- missing data in the middle telescopes away identically in both),
//! no WAS truncation residue (sums divisible by their divisors), and no convergence-hint match
//! behind the head (phantom skip). The enumerated-delta half of the harness is the sibling
//! case v2_corrections_produce_the_enumerated_deltas.
//!
BOOST_AUTO_TEST_CASE(v2_walker_matches_v1_where_no_correction_applies)
{
    // Pin the audit gate ON for the V1 side so both walkers run the same benefit-of-the-doubt
    // arm. Uses the consensus-params idiom (NOT gArgs.ForceSetArg -- clearing that with an
    // empty string silently ACTIVATES a gate from genesis for the rest of the process).
    struct AuditHeightGuard {
        const int m_saved = Params().GetConsensus().AutoGreylistAuditHeight;
        ~AuditHeightGuard()
        {
            const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = m_saved;
        }
    } audit_height_guard;

    const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = 0;

    // Run one fixture through both walkers and assert equality. Each project maps to a
    // total-credit sequence (oldest first; last entry is the head); all sequences must have
    // equal length. first_active_time optionally delays a project's whitelisting into the
    // window to exercise the admissibility prefix.
    auto run_and_compare = [](const std::map<std::string, std::vector<std::optional<uint64_t>>>& fixture,
                              const std::map<std::string, uint64_t>& first_active_time,
                              const std::string& label) {
        GRC::Whitelist& whitelist = GRC::GetWhitelist();
        std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

        whitelist.Reset();

        int height = 0;
        int64_t time = 0;

        auto unit_test_blocks = std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

        bool first = true;
        size_t sequence_length = 0;
        for (const auto& project : fixture) {
            const auto fa = first_active_time.find(project.first);
            const uint64_t add_time = (fa != first_active_time.end()) ? fa->second : 0;

            AddProjectEntry(3, project.first, "http://" + project.first + ".test", false,
                            height, add_time, first);
            first = false;

            if (sequence_length == 0) sequence_length = project.second.size();
            BOOST_REQUIRE(project.second.size() == sequence_length);
        }

        CBlockIndex* whitelist_index_entry = new CBlockIndex;
        ++height;
        ++time;

        CBlockIndex* index_ptr = whitelist_index_entry;
        CBlockIndex* index_ptr_prev = nullptr;

        GRC::SuperblockPtr head_ptr;

        for (size_t i = 0; i < sequence_length; ++i) {
            index_ptr_prev = index_ptr;
            index_ptr = new CBlockIndex;
            index_ptr->nHeight = height;
            index_ptr->nTime = time;
            index_ptr->MarkAsSuperblock();
            index_ptr->pprev = index_ptr_prev;

            GRC::Superblock superblock = GRC::Superblock();

            for (const auto& project : fixture) {
                if (project.second[i]) {
                    superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                        .insert(std::make_pair(project.first, *project.second[i]));
                }
            }

            GRC::SuperblockPtr superblock_ptr = GRC::SuperblockPtr();
            superblock_ptr.Replace(superblock);
            superblock_ptr.Rebind(index_ptr);

            unit_test_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));
            head_ptr = superblock_ptr;

            ++height;
            ++time;
        }

        // V1: one full refresh against the head (RefreshWithSuperblock rebuilds from scratch).
        auto_greylist->Reset();
        auto_greylist->RefreshWithSuperblock(head_ptr, unit_test_blocks);

        // V2: the pure walker over the same inputs.
        const GRC::AutoGreylistV2::Result v2 = GRC::AutoGreylistV2::Compute(
            head_ptr,
            whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED),
            whitelist.GetProjectsFirstActive(),
            unit_test_blocks);

        // Compare, both directions (same key set, then per-key equality).
        size_t v1_count = 0;

        for (auto iter = auto_greylist->begin(); iter != auto_greylist->end(); ++iter) {
            ++v1_count;

            const auto v2_iter = v2.m_candidates.find(iter->first);
            BOOST_REQUIRE_MESSAGE(v2_iter != v2.m_candidates.end(),
                                  label + ": V2 missing candidate " + iter->first);

            auto v1_entry = iter->second; // copy: V1 accessors are non-const
            const GRC::GreylistCandidateV2& v2_entry = v2_iter->second;

            BOOST_CHECK_MESSAGE(v1_entry.m_meets_greylisting_crit == v2_entry.m_meets_greylisting_crit,
                                label + "/" + iter->first + ": criteria mismatch");
            BOOST_CHECK_MESSAGE(v1_entry.GetZCD() == v2_entry.GetZCD(),
                                label + "/" + iter->first + ": ZCD mismatch V1="
                                    + ToString((int) v1_entry.GetZCD()) + " V2=" + ToString((int) v2_entry.GetZCD()));
            BOOST_CHECK_MESSAGE(v1_entry.m_TC_7_SB_sum == v2_entry.m_TC_7_SB_sum,
                                label + "/" + iter->first + ": TC_7 sum mismatch");
            BOOST_CHECK_MESSAGE(v1_entry.m_TC_40_SB_sum == v2_entry.m_TC_40_SB_sum,
                                label + "/" + iter->first + ": TC_40 sum mismatch");
            BOOST_CHECK_MESSAGE(v1_entry.GetWAS().ToDouble() == v2_entry.GetWAS().ToDouble(),
                                label + "/" + iter->first + ": WAS mismatch V1="
                                    + ToString(v1_entry.GetWAS().ToDouble())
                                    + " V2=" + ToString(v2_entry.GetWAS().ToDouble()));
            BOOST_CHECK_MESSAGE(v1_entry.GetUpdateHistory().size() == v2_entry.GetUpdateHistory().size(),
                                label + "/" + iter->first + ": history length mismatch");

            BOOST_CHECK_MESSAGE(
                (v2.m_auto_greylisted.count(iter->first) > 0) == v1_entry.m_meets_greylisting_crit,
                label + "/" + iter->first + ": membership set disagrees with criteria");
        }

        BOOST_CHECK_MESSAGE(v1_count == v2.m_candidates.size(),
                            label + ": candidate count mismatch V1=" + ToString(v1_count)
                                + " V2=" + ToString(v2.m_candidates.size()));

        for (auto& it : *unit_test_blocks) delete it.second.first;
        unit_test_blocks->clear();
        delete whitelist_index_entry;

        auto_greylist->Reset();
        whitelist.Reset();
    };

    typedef std::vector<std::optional<uint64_t>> Seq;

    // A healthy 45-entry riser as the shared base shape.
    auto riser = [](uint64_t base, uint64_t step, size_t len) {
        Seq v;
        for (size_t i = 0; i < len; ++i) v.push_back(base + step * i);
        return v;
    };
    const size_t LEN = 45;
    // Index of the entry sitting j superblocks back from the head.
    auto at_j = [&](size_t j) { return LEN - 1 - j; };

    { // 1: healthy riser + flat project (flat greylists: WAS 0, 20 ZCD).
        std::map<std::string, Seq> fx;
        fx["healthy"] = riser(500000000000ULL, 60000000ULL, LEN);
        fx["flat"] = Seq(LEN, std::optional<uint64_t>(777777));
        run_and_compare(fx, {}, "healthy+flat");
    }
    { // 7: absent at j == 1.
        std::map<std::string, Seq> fx;
        fx["absentj1"] = riser(1000, 1000, LEN);
        fx["absentj1"][at_j(1)] = std::optional<uint64_t>();
        run_and_compare(fx, {}, "absent j=1");
    }
    { // 7b: absent at BOTH the head and j == 1 (a 2-superblock scraper gap). Pins the
      //     inherited first-data-after-gap ZCD edge: with the bookmark still disengaged at
      //     j == 2, the first real data point counts as a ZCD in BOTH walkers identically
      //     (the benefit of the doubt excuses only position 1). The semantic question
      //     belongs to the deferred walker-correctness pass; equality here proves V2 did
      //     not silently diverge on it.
        // Step 140 keeps the sums divisor-divisible (sum7 = 5*140 = 700 over 7; sum40 =
        // 38*140 = 5320 over 40), so the fixture carries no exact-fraction residue and the
        // comparison is legitimately exact.
        std::map<std::string, Seq> fx;
        fx["absentgap"] = riser(1400, 140, LEN);
        fx["absentgap"][at_j(0)] = std::optional<uint64_t>();
        fx["absentgap"][at_j(1)] = std::optional<uint64_t>();
        run_and_compare(fx, {}, "absent head+j=1 gap");
    }
    { // 8: absent stretch j == 3..5 (numerator skips, divisor advances -- the F8 shape).
        std::map<std::string, Seq> fx;
        fx["absentrun"] = riser(1000, 1000, LEN);
        for (size_t j = 3; j <= 5; ++j) fx["absentrun"][at_j(j)] = std::optional<uint64_t>();
        run_and_compare(fx, {}, "absent j=3..5");
    }
    { // 10: short history (5 superblocks -- inside the 7-SB grace period).
        std::map<std::string, Seq> fx;
        fx["short"] = riser(1000, 1000, 5);
        run_and_compare(fx, {}, "short history");
    }
    { // 11: rollback to a non-zero value at j == 10.
        std::map<std::string, Seq> fx;
        fx["rollback"] = riser(10000, 1000, LEN);
        fx["rollback"][at_j(10)] = std::optional<uint64_t>(*fx["rollback"][at_j(10)] - 5000);
        run_and_compare(fx, {}, "rollback j=10");
    }
    { // 12: genuinely all-zero project (must greylist identically on both walkers).
        std::map<std::string, Seq> fx;
        fx["allzero"] = Seq(LEN, std::optional<uint64_t>(0));
        fx["healthy"] = riser(1000, 1000, LEN);
        run_and_compare(fx, {}, "all-zero");
    }
    { // 13: a project whitelisted mid-window (admissibility prefix; fixture times ascend
      //     1,2,3,... so first-active time 30 truncates its walk to the newer positions).
        std::map<std::string, Seq> fx;
        fx["old"] = riser(1000, 1000, LEN);
        fx["late"] = riser(2000, 2000, LEN);
        std::map<std::string, uint64_t> fa;
        fa["late"] = 30;
        run_and_compare(fx, fa, "late whitelisting");
    }
}

namespace {
//!
//! \brief Shared driver for the V2-correction tests: builds a synthetic superblock chain from
//! per-project total-credit sequences (oldest first; last entry is the head), runs the frozen
//! V1 walker (via the facade) and AutoGreylistV2::Compute over it, and returns both results.
//! Convergence hints may be assigned per sequence index to exercise the phantom-head skip.
//!
struct WalkerRunResults {
    std::map<std::string, GRC::AutoGreylist::GreylistCandidateEntry> m_v1;
    GRC::AutoGreylistV2::Result m_v2;
};

WalkerRunResults RunBothGreylistWalkers(const std::map<std::string, std::vector<std::optional<uint64_t>>>& fixture,
                                        const std::map<size_t, uint32_t>& convergence_hints = {},
                                        const std::map<std::string, uint64_t>& first_active_time = {})
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();
    std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

    whitelist.Reset();

    int height = 0;
    int64_t time = 0;

    auto unit_test_blocks = std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

    bool first = true;
    size_t sequence_length = 0;
    for (const auto& project : fixture) {
        const auto fa = first_active_time.find(project.first);
        const uint64_t add_time = (fa != first_active_time.end()) ? fa->second : 0;

        AddProjectEntry(3, project.first, "http://" + project.first + ".test", false, height, add_time, first);
        first = false;

        if (sequence_length == 0) sequence_length = project.second.size();
        BOOST_REQUIRE(project.second.size() == sequence_length);
    }

    CBlockIndex* whitelist_index_entry = new CBlockIndex;
    ++height;
    ++time;

    CBlockIndex* index_ptr = whitelist_index_entry;
    CBlockIndex* index_ptr_prev = nullptr;

    GRC::SuperblockPtr head_ptr;

    for (size_t i = 0; i < sequence_length; ++i) {
        index_ptr_prev = index_ptr;
        index_ptr = new CBlockIndex;
        index_ptr->nHeight = height;
        index_ptr->nTime = time;
        index_ptr->MarkAsSuperblock();
        index_ptr->pprev = index_ptr_prev;

        GRC::Superblock superblock = GRC::Superblock();

        for (const auto& project : fixture) {
            if (project.second[i]) {
                superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                    .insert(std::make_pair(project.first, *project.second[i]));
            }
        }

        const auto hint = convergence_hints.find(i);
        if (hint != convergence_hints.end()) {
            superblock.m_convergence_hint = hint->second;
        }

        GRC::SuperblockPtr superblock_ptr = GRC::SuperblockPtr();
        superblock_ptr.Replace(superblock);
        superblock_ptr.Rebind(index_ptr);

        unit_test_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));
        head_ptr = superblock_ptr;

        ++height;
        ++time;
    }

    WalkerRunResults results;

    auto_greylist->Reset();
    auto_greylist->RefreshWithSuperblock(head_ptr, unit_test_blocks);

    for (auto iter = auto_greylist->begin(); iter != auto_greylist->end(); ++iter) {
        results.m_v1.insert(std::make_pair(iter->first, iter->second));
    }

    results.m_v2 = GRC::AutoGreylistV2::Compute(
        head_ptr,
        whitelist.Snapshot(GRC::GreylistState::NONE, GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED),
        whitelist.GetProjectsFirstActive(),
        unit_test_blocks);

    for (auto& it : *unit_test_blocks) delete it.second.first;
    unit_test_blocks->clear();
    delete whitelist_index_entry;

    auto_greylist->Reset();
    whitelist.Reset();

    return results;
}
} // anonymous namespace

//!
//! \brief Differential harness, delta half: every V2 correction produces exactly the intended
//! divergence from V1 -- and nothing else.
//!
//! The corrections under test (all gated on AutoGreylistRedesignHeight, activating together):
//!
//!   1. Chain-resident zeros as missing data, with the initial-state latch: a recorded zero
//!      with a non-zero at an OLDER position is corruption, and V2 must treat it EXACTLY as
//!      it treats an absent entry -- pinned by twin fixtures (zero vs absent) at the head,
//!      j=7 and j=40, equal in every computed field, while the history still records the raw
//!      zero (the corruption stays diagnosable).
//!   2. The ZCD arm consumes the effective value too (zeros and NAs both count as ZCDs): a
//!      corrupt zero mid-window is one ZCD in V2 where V1's raw comparison counted none.
//!   3. WAS divisor contraction (F8): a missing window ENDPOINT contracts the divisor to the
//!      deepest position with data, so a uniform riser scores exactly 1.0 where V1's fixed
//!      divisor understated it; missing data in the MIDDLE still leaves the divisor alone
//!      (covered by the equality half).
//!   4. Exact-fraction WAS: (sum7 * d40) / (sum40 * d7), no integer truncation.
//!   5. The initial-state latch direction: 9 producing superblocks over 31 genuine initial
//!      zeros score WAS = 13/3 (4.3333...) -- the value only the correct (oldest-non-zero)
//!      latch with exact fractions produces. A naive newest-non-zero latch would damn the
//!      initial zeros as corruption and score 1.0.
//!
BOOST_AUTO_TEST_CASE(v2_corrections_produce_the_enumerated_deltas)
{
    // Pin the audit gate ON for the V1 comparisons (matching V2's hard-coded behavior).
    struct AuditHeightGuard {
        const int m_saved = Params().GetConsensus().AutoGreylistAuditHeight;
        ~AuditHeightGuard()
        {
            const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = m_saved;
        }
    } audit_height_guard;

    const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = 0;

    typedef std::vector<std::optional<uint64_t>> Seq;

    auto riser = [](uint64_t base, uint64_t step, size_t len) {
        Seq v;
        for (size_t i = 0; i < len; ++i) v.push_back(base + step * i);
        return v;
    };
    const size_t LEN = 45;
    auto at_j = [&](size_t j) { return LEN - 1 - j; };

    // Compare two V2 candidates field-by-field (everything except the history contents).
    auto check_same_computation = [](const GRC::GreylistCandidateV2& a, const GRC::GreylistCandidateV2& b,
                                     const std::string& label) {
        BOOST_CHECK_MESSAGE(a.m_meets_greylisting_crit == b.m_meets_greylisting_crit, label + ": criteria");
        BOOST_CHECK_MESSAGE(a.GetZCD() == b.GetZCD(), label + ": ZCD");
        BOOST_CHECK_MESSAGE(a.m_TC_7_SB_sum == b.m_TC_7_SB_sum, label + ": TC_7 sum");
        BOOST_CHECK_MESSAGE(a.m_TC_40_SB_sum == b.m_TC_40_SB_sum, label + ": TC_40 sum");
        BOOST_CHECK_MESSAGE(a.GetWAS() == b.GetWAS(), label + ": WAS");
    };

    // ---- 1. Corrupt-zero == absent, at each transit position. ----
    for (const size_t j : {(size_t) 0, (size_t) 7, (size_t) 40}) {
        std::map<std::string, Seq> zero_fx, absent_fx;
        zero_fx["p"] = riser(500000000000ULL, 60000000ULL, LEN);
        zero_fx["p"][at_j(j)] = std::optional<uint64_t>(0);
        absent_fx["p"] = riser(500000000000ULL, 60000000ULL, LEN);
        absent_fx["p"][at_j(j)] = std::optional<uint64_t>();

        const auto zero_run = RunBothGreylistWalkers(zero_fx);
        const auto absent_run = RunBothGreylistWalkers(absent_fx);

        const std::string label = "twin j=" + ToString(j);
        BOOST_REQUIRE(zero_run.m_v2.m_candidates.count("p") == 1);
        BOOST_REQUIRE(absent_run.m_v2.m_candidates.count("p") == 1);

        check_same_computation(zero_run.m_v2.m_candidates.at("p"),
                               absent_run.m_v2.m_candidates.at("p"), label);

        // Neither twin spuriously greylists: the WAS is computed from the real history.
        BOOST_CHECK_MESSAGE(zero_run.m_v2.m_auto_greylisted.empty(), label + ": no spurious greylist");

        // The history records the RAW values: the zero twin reports 0 at the transit
        // position, the absent twin reports NA -- the corruption stays diagnosable.
        const auto& zero_history = zero_run.m_v2.m_candidates.at("p").GetUpdateHistory();
        bool found_recorded_zero = false;
        for (const auto& entry : zero_history) {
            if (entry.m_sb_from_baseline_processed == j) {
                BOOST_CHECK_MESSAGE(entry.m_total_credit.has_value() && *entry.m_total_credit == 0,
                                    label + ": history must record the raw zero");
                found_recorded_zero = true;
            }
        }
        BOOST_CHECK_MESSAGE(found_recorded_zero, label + ": history entry present");

        // And the V1 comparison confirms these fixtures genuinely diverge (the delta is
        // real): V1 spuriously greylists on the head and j=40 zeros, and inflates WAS at
        // j=7. (Head/j40: WAS collapses; j7: WAS explodes. Either way != V2's clean value.)
        auto v1_zero_entry = zero_run.m_v1.at("p"); // copy: V1 accessors are non-const
        BOOST_CHECK_MESSAGE(v1_zero_entry.GetWAS().ToDouble()
                                != zero_run.m_v2.m_candidates.at("p").GetWAS().ToDouble(),
                            label + ": delta vs V1 must exist");
    }

    // ---- 2. Corrupt zero mid-window: exactly one extra ZCD, same WAS. ----
    {
        std::map<std::string, Seq> fx;
        fx["p"] = riser(1000, 1000, LEN);
        fx["p"][at_j(20)] = std::optional<uint64_t>(0);

        const auto run = RunBothGreylistWalkers(fx);

        BOOST_REQUIRE(run.m_v2.m_candidates.count("p") == 1);
        auto v1_entry = run.m_v1.at("p"); // copy: V1 accessors are non-const

        BOOST_CHECK_EQUAL((int) run.m_v2.m_candidates.at("p").GetZCD(), (int) v1_entry.GetZCD() + 1);
        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS().ToDouble() == v1_entry.GetWAS().ToDouble());
    }

    // ---- 3. Divisor contraction at missing endpoints (approved vectors). ----
    {
        // Data at j=0..5, NA at 6 and 7: sum7 = bookmark - TC[5] over divisor 5 -> exactly 1.
        std::map<std::string, Seq> fx;
        fx["p"] = riser(1000, 1000, LEN);
        fx["p"][at_j(6)] = std::optional<uint64_t>();
        fx["p"][at_j(7)] = std::optional<uint64_t>();

        const auto run = RunBothGreylistWalkers(fx);

        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(1));
        // V1 divides the 5-position numerator by 7: understated -- the F8 defect this fixes.
        auto v1_entry = run.m_v1.at("p");
        BOOST_CHECK(v1_entry.GetWAS().ToDouble() < 1.0);
    }
    {
        // NA at both window endpoints (j=7 and j=40): both divisors contract -> exactly 1.
        std::map<std::string, Seq> fx;
        fx["p"] = riser(1000, 1000, LEN);
        fx["p"][at_j(7)] = std::optional<uint64_t>();
        fx["p"][at_j(40)] = std::optional<uint64_t>();

        const auto run = RunBothGreylistWalkers(fx);

        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(1));
        auto v1_entry = run.m_v1.at("p");
        BOOST_CHECK(v1_entry.GetWAS().ToDouble() < 1.0);
    }

    // ---- 4. Exact-fraction WAS (no integer truncation): absent head. ----
    {
        // Head absent: initial bookmark repairs to TC[1]; sum7 = TC[1]-TC[7] = 6000 over
        // divisor 7 (data present at 7); sum40 = 39000 over divisor 40.
        std::map<std::string, Seq> fx;
        fx["p"] = riser(1000, 1000, LEN);
        fx["p"][at_j(0)] = std::optional<uint64_t>();

        const auto run = RunBothGreylistWalkers(fx);

        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(6000 * 40, 39000 * 7));
    }

    // ---- 5. The initial-state latch, correct direction: WAS = 13/3 exactly. ----
    {
        // 31 genuine initial zeros, then 9 producing superblocks rising by 100 (head 900).
        // latch_j = 8 (the oldest non-zero), so every zero (j=9..39) is OLDER than the latch
        // and stays a genuine value: sum7 = 900-200 = 700 over divisor 7; sum40 = 900-0 = 900
        // over divisor 39. WAS = (700*39)/(900*7) = 13/3. A naive newest-non-zero latch would
        // treat the initial zeros as corruption and score (700*8)/(800*7) = 1.
        std::map<std::string, Seq> fx;
        Seq seq(31, std::optional<uint64_t>(0));
        for (uint64_t i = 1; i <= 9; ++i) seq.push_back(i * 100);
        fx["p"] = seq;

        const auto run = RunBothGreylistWalkers(fx);

        BOOST_REQUIRE(run.m_v2.m_candidates.count("p") == 1);
        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(13, 3));

        // The project still greylists -- via ZCD (11 zero-credit days in the 20-SB window),
        // exactly as a project that produced for only 9 of 40 days should. The latch protects
        // the WAS from misreading genuine initial zeros; it does not excuse inactivity.
        BOOST_CHECK((int) run.m_v2.m_candidates.at("p").GetZCD() > 7);
        BOOST_CHECK(run.m_v2.m_auto_greylisted.count("p") == 1);
    }
}

//!
//! \brief The phantom-head skip: a committed superblock built from the SAME convergence as
//! the candidate head (identified by a matching non-zero convergence hint at the FIRST
//! committed superblock behind the head) is a re-derivation of identical data. Walking it
//! double-counts the head: TC[1] == TC[0] fires a false ZCD for every project (DESIGN.md
//! section 3). V2 skips it -- position 1 becomes the superblock before it -- so the false ZCD
//! disappears and the window covers 40 REAL superblocks. Deterministic on every node: the
//! hint is serialized in the superblock both sides compare.
//!
BOOST_AUTO_TEST_CASE(v2_skips_the_phantom_head_superblock)
{
    struct AuditHeightGuard {
        const int m_saved = Params().GetConsensus().AutoGreylistAuditHeight;
        ~AuditHeightGuard()
        {
            const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = m_saved;
        }
    } audit_height_guard;

    const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistAuditHeight = 0;

    typedef std::vector<std::optional<uint64_t>> Seq;

    // A rising history whose two newest entries are IDENTICAL (built from one convergence),
    // marked with the same convergence hint. 12 entries: 1000..10000, then 11000 twice.
    Seq seq;
    for (uint64_t i = 1; i <= 10; ++i) seq.push_back(i * 1000);
    seq.push_back(11000);
    seq.push_back(11000);

    std::map<std::string, Seq> fx;
    fx["p"] = seq;

    std::map<size_t, uint32_t> hints;
    hints[seq.size() - 1] = 0xABCD1234; // the head
    hints[seq.size() - 2] = 0xABCD1234; // the just-staked superblock from the same convergence
    hints[seq.size() - 3] = 0x00000001; // older superblocks: distinct hints

    const auto run = RunBothGreylistWalkers(fx, hints);

    BOOST_REQUIRE(run.m_v2.m_candidates.count("p") == 1);

    // V1 counts the phantom: TC[1] == TC[0] -> one false ZCD. V2 skips it: zero.
    auto v1_entry = run.m_v1.at("p");
    BOOST_CHECK_EQUAL((int) v1_entry.GetZCD(), 1);
    BOOST_CHECK_EQUAL((int) run.m_v2.m_candidates.at("p").GetZCD(), 0);

    // With the phantom skipped the window is the uniform riser: WAS is exactly 1.
    BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(1));

    // Control: the SAME data without matching hints must not skip -- V2 then counts the
    // duplicate exactly as V1 does (the skip keys on the hint, not on equal values).
    const auto no_hint_run = RunBothGreylistWalkers(fx);
    auto no_hint_v1 = no_hint_run.m_v1.at("p");
    BOOST_CHECK_EQUAL((int) no_hint_run.m_v2.m_candidates.at("p").GetZCD(), (int) no_hint_v1.GetZCD());
}

//!
//! \brief The latch evidence scan: boundary behavior of the capped beyond-window extension.
//!
//! A zero run touching the window edge cannot be classified from inside the window (the
//! WCG 2026-08-06 shape: the corrupt zero at position 40 had no older in-window evidence).
//! The walker therefore scans up to 40 additional superblocks past the window for the first
//! older admissible non-zero. Pinned here:
//!
//!   * evidence one superblock past the edge -> the edge zero is corrupt (the mainnet case);
//!   * the chain simply ending -> the edge zeros are GENUINE initial state and stay values;
//!   * evidence past the +40 cap -> not consulted; the zeros stay genuine. The cap trades a
//!     bounded walk (at most 2x) for a deterministic rule every node evaluates identically.
//!
BOOST_AUTO_TEST_CASE(v2_latch_evidence_scan_boundaries)
{
    typedef std::vector<std::optional<uint64_t>> Seq;

    auto riser = [](uint64_t base, uint64_t step, size_t len) {
        Seq v;
        for (size_t i = 0; i < len; ++i) v.push_back(base + step * i);
        return v;
    };

    // ---- Evidence just past the edge: corrupt (already exercised by the twin fixtures; ----
    // ---- pinned here at the exact +1 shape with a minimal chain: LEN 42, zero at j=40, ----
    // ---- non-zero evidence at j=41). ----
    {
        Seq seq = riser(1000, 1000, 42);
        const size_t LEN = 42;
        seq[LEN - 1 - 40] = std::optional<uint64_t>(0);

        std::map<std::string, Seq> fx;
        fx["p"] = seq;

        const auto run = RunBothGreylistWalkers(fx);

        // Corrupt zero at the endpoint -> treated as missing -> the 40-interval contracts to
        // the deepest data position (39): sum40 = TC[0]-TC[39] = 39000 over divisor 39;
        // sum7 = 7000 over 7 -> WAS exactly 1.
        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(1));
        BOOST_CHECK(run.m_v2.m_auto_greylisted.empty());
    }

    // ---- Chain ends at the zeros: genuine initial state. LEN 41: the three OLDEST ----
    // ---- entries are zeros (the project's true beginning), then a riser. ----
    {
        const size_t LEN = 41;
        Seq seq;
        seq.push_back(std::optional<uint64_t>(0));
        seq.push_back(std::optional<uint64_t>(0));
        seq.push_back(std::optional<uint64_t>(0));
        for (uint64_t i = 1; i <= LEN - 3; ++i) seq.push_back(i * 1000);

        std::map<std::string, Seq> fx;
        fx["p"] = seq;

        const auto run = RunBothGreylistWalkers(fx);

        // Zeros at j=38..40 are genuine values: the 40-sum is assigned at j=40 with tc=0
        // (initial 38000 - 0) over divisor 40; sum7 = 7000 over 7.
        // WAS = (7000*40)/(38000*7) = 20/19.
        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(20, 19));
    }

    // ---- The +40 cap: evidence at extension position 41 (past the cap) is not consulted. ----
    // ---- LEN 86: non-zero at j=81..85, zeros j=40..80, riser j=0..39. The scan covers ----
    // ---- extension positions 41..80 (40 superblocks), finds only zeros, and stops: the ----
    // ---- edge zero run stays genuine. ----
    {
        const size_t LEN = 86;
        Seq seq;
        for (uint64_t i = 1; i <= 5; ++i) seq.push_back(100000 + i);       // j=85..81 (beyond cap)
        for (size_t i = 0; i < 41; ++i) seq.push_back(std::optional<uint64_t>(0)); // j=80..40
        for (uint64_t i = 1; i <= 40; ++i) seq.push_back(200000 + i * 1000);       // j=39..0
        BOOST_REQUIRE(seq.size() == LEN);

        std::map<std::string, Seq> fx;
        fx["p"] = seq;

        const auto run = RunBothGreylistWalkers(fx);

        // The zero at j=40 stays a genuine value: sum40 = TC[0] - 0 = 240000 over divisor 40;
        // sum7 = 7000 over 7. WAS = (7000*40)/(240000*7) = 1/6 -- depressed by the genuine
        // (as far as any node can tell within the cap) inactivity, exactly as intended.
        BOOST_CHECK(run.m_v2.m_candidates.at("p").GetWAS() == Fraction(1, 6));

        // Variant: move the evidence INSIDE the cap (non-zero at extension position 41,
        // i.e. j=41) -> the whole zero run becomes corrupt -> missing -> the 40-interval
        // contracts to 39 and the WAS is the clean riser's exactly.
        Seq seq_in_cap = seq;
        seq_in_cap[LEN - 1 - 41] = std::optional<uint64_t>(150000);

        std::map<std::string, Seq> fx2;
        fx2["p"] = seq_in_cap;

        const auto run2 = RunBothGreylistWalkers(fx2);

        // sum40 = TC[0]-TC[39] = 39000 over 39; sum7 = 7000 over 7 -> exactly 1.
        BOOST_CHECK(run2.m_v2.m_candidates.at("p").GetWAS() == Fraction(1));
    }
}

//!
//! \brief Newly joined projects: NA-then-zero-then-production histories (forward terms).
//!
//! Two properties of the latch make a newcomer safe by construction, pinned here because
//! they are exactly the shapes the old V1 table test exercised:
//!
//!   * NAs never participate in the latch -- only ENGAGED values set evidence -- so a
//!     convergence-failure prefix (scrapers could not converge on the new project) is inert;
//!   * corruption requires a non-zero at a strictly OLDER position, so a genuine first-record
//!     zero (older than all production) can never be normalized away, while a later
//!     chain-resident zero in the SAME history (newer than production) is -- both classified
//!     correctly by the one latch.
//!
//! The first fixture also pins the first-activation boundary of the latch evidence scan
//! (a newcomer's trailing zeros resolve GENUINE the moment the scan crosses its
//! first-activation timestamp -- the erase path, distinct from the chain-end and cap
//! terminations pinned above).
//!
BOOST_AUTO_TEST_CASE(v2_newly_joined_project_na_then_zero_history)
{
    typedef std::vector<std::optional<uint64_t>> Seq;

    // ---- A newcomer against a long-running chain: first-active mid-window, then (forward)
    // ---- NA, NA, genuine zero, production 100..1400. ----
    {
        const size_t LEN = 45;

        std::map<std::string, Seq> fx;

        // A long-running healthy project so the chain spans the whole window.
        Seq old_project;
        for (uint64_t i = 1; i <= LEN; ++i) old_project.push_back(i * 1000);
        fx["old"] = old_project;

        // The newcomer: helper superblock times run 1..LEN; first-active at time 30 makes
        // positions j=0..15 admissible. Forward from its beginning: NA, NA (convergence
        // failures), a genuine zero (its true initial state), then production rising 100/SB
        // with a final jump to 2000 (asymmetric, so the expected WAS is a distinctive value
        // rather than an aliased 1).
        Seq newcomer(29, std::optional<uint64_t>());   // pre-first-active: no records
        newcomer.push_back(std::optional<uint64_t>()); // i=29 (j=15): NA
        newcomer.push_back(std::optional<uint64_t>()); // i=30 (j=14): NA
        newcomer.push_back(std::optional<uint64_t>(0)); // i=31 (j=13): genuine zero
        for (uint64_t i = 1; i <= 12; ++i) newcomer.push_back(i * 100); // j=12..1: 100..1200
        newcomer.push_back(std::optional<uint64_t>(2000));              // j=0 (head): 2000
        BOOST_REQUIRE(newcomer.size() == LEN);
        fx["newcomer"] = newcomer;

        std::map<std::string, uint64_t> fa;
        fa["newcomer"] = 30;

        const auto run = RunBothGreylistWalkers(fx, {}, fa);

        BOOST_REQUIRE(run.m_v2.m_candidates.count("newcomer") == 1);
        const auto& candidate = run.m_v2.m_candidates.at("newcomer");

        // The genuine zero at j=13 stays a value: sum40 is assigned there (2000 - 0) over
        // divisor 13 (the deepest effective data); sum7 = 2000 - TC[7] = 2000 - 600 = 1400
        // over divisor 7. WAS = (1400*13)/(2000*7) = 13/10. ZCD = 2 (the two NAs; the
        // genuine zero at j=13 is NOT a ZCD -- 0 >= bookmark(100) is false).
        BOOST_CHECK(candidate.GetWAS() == Fraction(13, 10));
        BOOST_CHECK_EQUAL((int) candidate.GetZCD(), 2);
        BOOST_CHECK(!candidate.m_meets_greylisting_crit);
        BOOST_CHECK(run.m_v2.m_auto_greylisted.count("newcomer") == 0);
    }

    // ---- One history holding BOTH a genuine initial zero and a later corrupt zero. ----
    // ---- Forward: NA, NA, 0(genuine), 100..500, 0(corrupt), 600..1200. ----
    {
        Seq seq;
        seq.push_back(std::optional<uint64_t>());  // j=15: NA
        seq.push_back(std::optional<uint64_t>());  // j=14: NA
        seq.push_back(std::optional<uint64_t>(0)); // j=13: genuine initial zero
        for (uint64_t i = 1; i <= 5; ++i) seq.push_back(i * 100);  // j=12..8: 100..500
        seq.push_back(std::optional<uint64_t>(0)); // j=7: CORRUPT (production exists older)
        for (uint64_t i = 6; i <= 12; ++i) seq.push_back(i * 100); // j=6..0: 600..1200

        std::map<std::string, Seq> fx;
        fx["p"] = seq;

        const auto run = RunBothGreylistWalkers(fx);

        BOOST_REQUIRE(run.m_v2.m_candidates.count("p") == 1);
        const auto& candidate = run.m_v2.m_candidates.at("p");

        // The corrupt zero at j=7 is missing for the WAS: the 7-interval contracts to 6
        // (sum7 = 1200-600 = 600); the genuine zero at j=13 anchors the 40-side
        // (sum40 = 1200-0 over divisor 13). WAS = (600*13)/(1200*6) = 13/12.
        // ZCD = 3: the corrupt zero (as NA) plus the two genuine NAs; the genuine zero is
        // not a ZCD (0 >= bookmark(100) is false), and the resumption at j=8 is not either
        // (500 >= bookmark(600) is false -- the bookmark held through the corrupt NA).
        BOOST_CHECK(candidate.GetWAS() == Fraction(13, 12));
        BOOST_CHECK_EQUAL((int) candidate.GetZCD(), 3);
        BOOST_CHECK(!candidate.m_meets_greylisting_crit);

        // Both zeros remain visible in the history exactly as recorded.
        int raw_zeros_in_history = 0;
        for (const auto& entry : candidate.GetUpdateHistory()) {
            if (entry.m_total_credit && *entry.m_total_credit == 0) ++raw_zeros_in_history;
        }
        BOOST_CHECK_EQUAL(raw_zeros_in_history, 2);
    }
}

namespace {
//!
//! \brief Guard pinning the redesign (and deep-copy, per the enforced ordering) gate heights
//! for the state-separation tests, restoring the network defaults on scope exit. Uses the
//! consensus-params idiom, NOT gArgs.ForceSetArg (an empty-string "clear" there silently
//! activates a gate from genesis for the rest of the process).
//!
struct RedesignHeightGuard {
    const int m_saved_redesign = Params().GetConsensus().AutoGreylistRedesignHeight;
    const int m_saved_deep_copy = Params().GetConsensus().AutoGreylistDeepCopyHeight;

    explicit RedesignHeightGuard(int gate_height)
    {
        const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistRedesignHeight = gate_height;
        const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistDeepCopyHeight = gate_height;
    }

    ~RedesignHeightGuard()
    {
        const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistRedesignHeight = m_saved_redesign;
        const_cast<Consensus::Params&>(Params().GetConsensus()).AutoGreylistDeepCopyHeight = m_saved_deep_copy;
    }
};

//!
//! \brief A synthetic superblock chain for driving the facade producers. Builds SBs at
//! heights 1..N from per-project total-credit sequences and keeps ownership of the block
//! index entries. The head SB is at height N; a tip index at height N+1 is provided for
//! binding candidates (the pending anchor).
//!
struct FacadeChainFixture {
    std::shared_ptr<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>> m_blocks;
    std::vector<CBlockIndex*> m_owned;
    GRC::SuperblockPtr m_head;
    CBlockIndex* m_tip = nullptr; //!< Height N+1, pprev = the head SB's index.

    explicit FacadeChainFixture(const std::map<std::string, std::vector<std::optional<uint64_t>>>& fixture)
    {
        GRC::Whitelist& whitelist = GRC::GetWhitelist();

        whitelist.Reset();

        m_blocks = std::make_shared<std::map<int, std::pair<CBlockIndex*, GRC::SuperblockPtr>>>();

        int height = 0;
        int64_t time = 0;

        bool first = true;
        size_t sequence_length = 0;
        for (const auto& project : fixture) {
            AddProjectEntry(3, project.first, "http://" + project.first + ".test", false, height, 0, first);
            first = false;

            if (sequence_length == 0) sequence_length = project.second.size();
            BOOST_REQUIRE(project.second.size() == sequence_length);
        }

        CBlockIndex* base = new CBlockIndex;
        m_owned.push_back(base);
        ++height;
        ++time;

        CBlockIndex* index_ptr = base;

        for (size_t i = 0; i < sequence_length; ++i) {
            CBlockIndex* prev = index_ptr;
            index_ptr = new CBlockIndex;
            m_owned.push_back(index_ptr);
            index_ptr->nHeight = height;
            index_ptr->nTime = time;
            index_ptr->MarkAsSuperblock();
            index_ptr->pprev = prev;

            GRC::Superblock superblock = GRC::Superblock();

            for (const auto& project : fixture) {
                if (project.second[i]) {
                    superblock.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
                        .insert(std::make_pair(project.first, *project.second[i]));
                }
            }

            GRC::SuperblockPtr superblock_ptr = GRC::SuperblockPtr();
            superblock_ptr.Replace(superblock);
            superblock_ptr.Rebind(index_ptr);

            m_blocks->insert(std::make_pair(height, std::make_pair(index_ptr, superblock_ptr)));
            m_head = superblock_ptr;

            ++height;
            ++time;
        }

        m_tip = new CBlockIndex;
        m_owned.push_back(m_tip);
        m_tip->nHeight = height;
        m_tip->nTime = time;
        m_tip->pprev = index_ptr;
    }

    ~FacadeChainFixture()
    {
        GRC::GetAutoGreylistCache()->Reset();
        GRC::GetWhitelist().Reset();

        for (CBlockIndex* index : m_owned) delete index;
    }
};
} // anonymous namespace

//!
//! \brief Above the gate, AUTHORITATIVE is READ from the superblock's m_project_status
//! record -- never recomputed -- and the read carries its identity.
//!
//! The record-is-truth pin doubles as the vacuity guard for every future authoritative-path
//! test: this fixture's total-credit history WOULD greylist the flat project if the walker
//! ran, but the record is empty, and the authoritative read must report exactly what the
//! record says (nothing greylisted). A synthetic fixture that forgets to populate
//! m_project_status therefore CANNOT vacuously pass an assertion that expects walker-derived
//! membership -- the mismatch this case pins is precisely what it would produce.
//!
BOOST_AUTO_TEST_CASE(facade_authoritative_is_read_from_the_record_above_the_gate)
{
    RedesignHeightGuard gate_guard(/*gate_height=*/0);

    std::map<std::string, std::vector<std::optional<uint64_t>>> fx;
    fx["flatproj"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(500000));
    std::vector<std::optional<uint64_t>> rising;
    for (uint64_t i = 1; i <= 12; ++i) rising.push_back(i * 1000);
    fx["growproj"] = rising;

    std::shared_ptr<GRC::AutoGreylistService> service = GRC::GetAutoGreylistCache();

    // --- Empty record: the walker would greylist flatproj, the record says nothing is. ---
    {
        FacadeChainFixture chain(fx);

        service->RefreshWithSuperblock(chain.m_head, chain.m_blocks);

        const auto authoritative = service->Get(GRC::GreylistState::AUTHORITATIVE);
        BOOST_REQUIRE(authoritative.has_value());
        BOOST_CHECK(authoritative->m_version == GRC::GreylistVersion::V2);
        BOOST_CHECK(authoritative->m_from_record == true);
        BOOST_CHECK(authoritative->m_auto_greylisted.empty());
        BOOST_CHECK(!service->Contains(GRC::GreylistState::AUTHORITATIVE, "flatproj"));

        // The walker disagrees -- proving the read served the RECORD, not a recompute.
        const auto walked = GRC::AutoGreylistV2::Compute(
            chain.m_head,
            GRC::GetWhitelist().Snapshot(GRC::GreylistState::NONE,
                                         GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED),
            GRC::GetWhitelist().GetProjectsFirstActive(),
            chain.m_blocks);
        BOOST_CHECK(walked.m_auto_greylisted.count("flatproj") == 1);
    }

    // --- Populated record: served verbatim, MAN_GREYLISTED entries excluded from the ---
    // --- AUTO membership, and the deep-copy answer is hard-coded true for V2 reads. ---
    {
        FacadeChainFixture chain(fx);

        GRC::Superblock recorded = *chain.m_head;
        recorded.m_project_status.m_project_status.insert(
            std::make_pair("flatproj", GRC::ProjectEntryStatus::AUTO_GREYLISTED));
        recorded.m_project_status.m_project_status.insert(
            std::make_pair("somemanual", GRC::ProjectEntryStatus::MAN_GREYLISTED));

        GRC::SuperblockPtr recorded_ptr = chain.m_head;
        recorded_ptr.Replace(recorded);

        service->RefreshWithSuperblock(recorded_ptr, chain.m_blocks);

        BOOST_CHECK(service->Contains(GRC::GreylistState::AUTHORITATIVE, "flatproj"));
        BOOST_CHECK(!service->Contains(GRC::GreylistState::AUTHORITATIVE, "somemanual"));
        BOOST_CHECK(!service->Contains(GRC::GreylistState::AUTHORITATIVE, "growproj"));
        BOOST_CHECK(service->IsDeepCopyActive(GRC::GreylistState::AUTHORITATIVE));

        // The Snapshot overlay above the gate promotes from the record.
        std::map<std::string, GRC::ProjectEntryStatus> status;
        for (const auto& entry : GRC::GetWhitelist().Snapshot(
                 GRC::GreylistState::AUTHORITATIVE,
                 GRC::ProjectEntry::ProjectFilterFlag::ALL_BUT_DELETED)) {
            status[entry.m_name] = entry.m_status.Value();
        }
        BOOST_CHECK(status.at("flatproj") == GRC::ProjectEntryStatus::AUTO_GREYLISTED);
        BOOST_CHECK(status.at("growproj") == GRC::ProjectEntryStatus::ACTIVE);
    }
}

//!
//! \brief The pending state: keyed by convergence identity with reuse, stamped through the
//! one record rule, and TOTAL (absent a convergence, pending == authoritative).
//!
BOOST_AUTO_TEST_CASE(facade_pending_lifecycle_above_the_gate)
{
    RedesignHeightGuard gate_guard(/*gate_height=*/0);

    struct TestStateGuard {
        CBlockIndex* saved_pindexBest = pindexBest;
        ~TestStateGuard() { pindexBest = saved_pindexBest; }
    } test_state_guard;

    std::map<std::string, std::vector<std::optional<uint64_t>>> fx;
    fx["flatproj"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(500000));
    std::vector<std::optional<uint64_t>> rising;
    for (uint64_t i = 1; i <= 12; ++i) rising.push_back(i * 1000);
    fx["growproj"] = rising;

    FacadeChainFixture chain(fx);
    std::shared_ptr<GRC::AutoGreylistService> service = GRC::GetAutoGreylistCache();

    pindexBest = chain.m_tip;

    // --- Base case first: authoritative primed, no pending -> pending == authoritative. ---
    service->RefreshWithSuperblock(chain.m_head, chain.m_blocks);

    const auto base_pending = service->Get(GRC::GreylistState::PENDING);
    const auto base_authoritative = service->Get(GRC::GreylistState::AUTHORITATIVE);
    BOOST_REQUIRE(base_pending.has_value() && base_authoritative.has_value());
    BOOST_CHECK(base_pending->m_key == base_authoritative->m_key);
    BOOST_CHECK(base_pending->m_from_record == base_authoritative->m_from_record);
    BOOST_CHECK(base_pending->m_auto_greylisted == base_authoritative->m_auto_greylisted);

    // --- Build a candidate from "convergence X": flatproj meets criteria via the walk. ---
    GRC::Superblock candidate = GRC::Superblock();
    candidate.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("flatproj", 500000));
    candidate.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("growproj", 13000));

    const uint256 convergence_x = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    const uint256 convergence_y = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");

    service->RefreshWithAndUpdateSuperblock(candidate, convergence_x, /*update_pending_cache=*/true,
                                            chain.m_blocks);

    // The candidate's record was stamped through the record rule.
    BOOST_REQUIRE(candidate.m_project_status.m_project_status.count("flatproj") == 1);
    BOOST_CHECK(candidate.m_project_status.m_project_status.at("flatproj").Value()
                == GRC::ProjectEntryStatus::AUTO_GREYLISTED);
    BOOST_CHECK(candidate.m_project_status.m_project_status.count("growproj") == 0);

    const auto pending_x = service->Get(GRC::GreylistState::PENDING);
    BOOST_REQUIRE(pending_x.has_value());
    BOOST_CHECK(pending_x->m_key == convergence_x);
    BOOST_CHECK(pending_x->m_from_record == false);
    BOOST_CHECK(pending_x->m_auto_greylisted.count("flatproj") == 1);
    BOOST_CHECK(service->Contains(GRC::GreylistState::PENDING, "flatproj"));

    // The authoritative slot is untouched by the pending write (still the empty record).
    BOOST_CHECK(!service->Contains(GRC::GreylistState::AUTHORITATIVE, "flatproj"));

    // --- Key reuse: same convergence id with DIFFERENT candidate data must NOT rewalk. ---
    GRC::Superblock altered = GRC::Superblock();
    altered.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("flatproj", 999999)); // would change the walk if it ran

    service->RefreshWithAndUpdateSuperblock(altered, convergence_x, /*update_pending_cache=*/true,
                                            chain.m_blocks);

    const auto pending_x_again = service->Get(GRC::GreylistState::PENDING);
    BOOST_REQUIRE(pending_x_again.has_value());
    BOOST_CHECK(pending_x_again->m_auto_greylisted == pending_x->m_auto_greylisted);
    // And the reuse still stamped the new candidate from the cached membership.
    BOOST_CHECK(altered.m_project_status.m_project_status.count("flatproj") == 1);

    // --- A NEW convergence id recomputes. ---
    GRC::Superblock candidate_y = GRC::Superblock();
    candidate_y.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("flatproj", 500000));
    candidate_y.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("growproj", 13000));

    service->RefreshWithAndUpdateSuperblock(candidate_y, convergence_y, /*update_pending_cache=*/true,
                                            chain.m_blocks);

    const auto pending_y = service->Get(GRC::GreylistState::PENDING);
    BOOST_REQUIRE(pending_y.has_value());
    BOOST_CHECK(pending_y->m_key == convergence_y);

    // --- update_pending_cache == false (a PAST convergence): stamps but does not clobber. ---
    GRC::Superblock past_candidate = GRC::Superblock();
    past_candidate.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("flatproj", 500000));

    const uint256 convergence_past = uint256S("0x3333333333333333333333333333333333333333333333333333333333333333");
    service->RefreshWithAndUpdateSuperblock(past_candidate, convergence_past,
                                            /*update_pending_cache=*/false, chain.m_blocks);

    BOOST_CHECK(past_candidate.m_project_status.m_project_status.count("flatproj") == 1);
    const auto pending_after_past = service->Get(GRC::GreylistState::PENDING);
    BOOST_REQUIRE(pending_after_past.has_value());
    BOOST_CHECK(pending_after_past->m_key == convergence_y); // live state not clobbered
}

//!
//! \brief The version-dispatch invariant across the gate: a V1-producer write clears both V2
//! slots (a reorg back across the gate migrates no state in either direction).
//!
BOOST_AUTO_TEST_CASE(facade_v1_write_clears_v2_slots_across_the_gate)
{
    // Gate at height 100: the synthetic chain (heights 1..N) is BELOW it; a head pinned at
    // height 150 is above.
    RedesignHeightGuard gate_guard(/*gate_height=*/100);

    std::map<std::string, std::vector<std::optional<uint64_t>>> fx;
    fx["flatproj"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(500000));

    FacadeChainFixture chain(fx);
    std::shared_ptr<GRC::AutoGreylistService> service = GRC::GetAutoGreylistCache();

    // Above-gate write: rebind the head to a synthetic index at height 150.
    CBlockIndex above_gate_index;
    above_gate_index.nHeight = 150;
    above_gate_index.nTime = chain.m_head.m_timestamp;
    above_gate_index.MarkAsSuperblock();

    GRC::SuperblockPtr above_head = chain.m_head;
    above_head.Rebind(&above_gate_index);

    service->RefreshWithSuperblock(above_head, chain.m_blocks);

    const auto v2_read = service->Get(GRC::GreylistState::AUTHORITATIVE);
    BOOST_REQUIRE(v2_read.has_value());
    BOOST_CHECK(v2_read->m_version == GRC::GreylistVersion::V2);

    // Reorg back across the gate: a V1-producer write (head below the gate) clears the V2
    // slots, and reads fall through to V1.
    service->RefreshWithSuperblock(chain.m_head, chain.m_blocks);

    const auto v1_read = service->Get(GRC::GreylistState::AUTHORITATIVE);
    BOOST_REQUIRE(v1_read.has_value());
    BOOST_CHECK(v1_read->m_version == GRC::GreylistVersion::V1);
}

//!
//! \brief The stamp derives through the one record rule: registry manual and override
//! statuses interact with computed membership exactly as the historical write site did.
//!
BOOST_AUTO_TEST_CASE(facade_stamp_respects_manual_and_override_status)
{
    RedesignHeightGuard gate_guard(/*gate_height=*/0);

    struct TestStateGuard {
        CBlockIndex* saved_pindexBest = pindexBest;
        ~TestStateGuard() { pindexBest = saved_pindexBest; }
    } test_state_guard;

    // Five projects distinguished by registry status and history. The record rule is V1's
    // overlay verbatim: ACTIVE or MAN_GREYLISTED plus criteria-met promotes to
    // AUTO_GREYLISTED (yes, a manual entry that ALSO meets auto criteria records as AUTO --
    // the historical write-site behavior); MAN_GREYLISTED without auto criteria records as
    // manual; AUTO_GREYLIST_OVERRIDE is never promoted and never recorded; healthy ACTIVE
    // is omitted.
    std::vector<std::optional<uint64_t>> rising;
    for (uint64_t i = 1; i <= 12; ++i) rising.push_back(i * 1000);

    std::map<std::string, std::vector<std::optional<uint64_t>>> fx;
    fx["activeproj"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(500000));
    fx["manualproj"] = rising; // healthy: stays MAN_GREYLISTED in the record
    fx["manualworst"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(800000)); // manual AND meets criteria
    fx["overrideproj"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(700000));
    fx["healthyproj"] = rising;

    FacadeChainFixture chain(fx);

    // Overwrite the registry statuses for the manual and override projects (v4 payloads
    // through the real contract handler; heights advance past the fixture's adds).
    AddProjectEntryWithStatus("manualproj", "http://manualproj.test",
                              GRC::ProjectEntryStatus::MAN_GREYLISTED, 500, 500);
    AddProjectEntryWithStatus("manualworst", "http://manualworst.test",
                              GRC::ProjectEntryStatus::MAN_GREYLISTED, 501, 501);
    AddProjectEntryWithStatus("overrideproj", "http://overrideproj.test",
                              GRC::ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE, 502, 502);

    std::shared_ptr<GRC::AutoGreylistService> service = GRC::GetAutoGreylistCache();

    pindexBest = chain.m_tip;

    GRC::Superblock candidate = GRC::Superblock();
    for (const auto& project : fx) {
        candidate.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
            .insert(std::make_pair(project.first, *project.second.back()));
    }

    const uint256 convergence_id = uint256S("0x4444444444444444444444444444444444444444444444444444444444444444");
    service->RefreshWithAndUpdateSuperblock(candidate, convergence_id, /*update_pending_cache=*/true,
                                            chain.m_blocks);

    const auto& record = candidate.m_project_status.m_project_status;

    BOOST_REQUIRE(record.count("activeproj") == 1);
    BOOST_CHECK(record.at("activeproj").Value() == GRC::ProjectEntryStatus::AUTO_GREYLISTED);

    BOOST_REQUIRE(record.count("manualproj") == 1);
    BOOST_CHECK(record.at("manualproj").Value() == GRC::ProjectEntryStatus::MAN_GREYLISTED);

    BOOST_REQUIRE(record.count("manualworst") == 1); // manual AND criteria-met: promotion wins
    BOOST_CHECK(record.at("manualworst").Value() == GRC::ProjectEntryStatus::AUTO_GREYLISTED);

    BOOST_CHECK(record.count("overrideproj") == 0); // never promoted, never recorded
    BOOST_CHECK(record.count("healthyproj") == 0);  // ACTIVE, omitted
}

//!
//! \brief The record produce/validate pair: a bind-time stamp round-trips through
//! acceptance-time validation, and every tamper shape is rejected.
//!
//! The m_project_status field is serialized but excluded from the quorum hash, so this
//! validation is the ONLY thing standing between a staker and network-wide adoption of an
//! arbitrary greylist once the record is read back as authoritative. The producer stamp and
//! the validator recomputation share one walker and one record-derivation rule, so agreement
//! is structural; these cases pin it and the rejection of each divergence.
//!
BOOST_AUTO_TEST_CASE(facade_record_stamp_validates_and_tampering_is_rejected)
{
    RedesignHeightGuard gate_guard(/*gate_height=*/0);

    std::map<std::string, std::vector<std::optional<uint64_t>>> fx;
    fx["flatproj"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(500000));
    std::vector<std::optional<uint64_t>> rising;
    for (uint64_t i = 1; i <= 12; ++i) rising.push_back(i * 1000);
    fx["growproj"] = rising;

    FacadeChainFixture chain(fx);
    std::shared_ptr<GRC::AutoGreylistService> service = GRC::GetAutoGreylistCache();

    // The candidate as a staker would bind it: anchored at tip height + 1.
    GRC::Superblock candidate = GRC::Superblock();
    candidate.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("flatproj", 500000));
    candidate.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("growproj", 13000));

    // Pre-populate the record with garbage: the bind-time stamp must OVERWRITE, not append
    // (the cached-contract case, where a stale convergence-time record must not survive).
    candidate.m_project_status.m_project_status.insert(
        std::make_pair("garbageproject", GRC::ProjectEntryStatus::AUTO_GREYLISTED));

    const int anchor_height = chain.m_tip->nHeight;
    const int64_t anchor_time = chain.m_tip->nTime;

    service->StampProjectStatus(candidate, anchor_height, anchor_time, /*walk_start=*/nullptr,
                                chain.m_blocks);

    BOOST_REQUIRE(candidate.m_project_status.m_project_status.count("flatproj") == 1);
    BOOST_CHECK(candidate.m_project_status.m_project_status.count("garbageproject") == 0);

    // The validator's view: the received superblock bound to its containing block.
    auto bind_received = [&](const GRC::Superblock& received) {
        GRC::SuperblockPtr ptr;
        ptr.Replace(received);
        ptr.m_height = anchor_height;
        ptr.m_timestamp = anchor_time;
        return ptr;
    };

    // --- Round trip: the honest record validates. ---
    BOOST_CHECK(service->ValidateProjectStatus(bind_received(candidate), nullptr, chain.m_blocks));

    // --- Tamper: a bogus AUTO_GREYLISTED entry added. ---
    {
        GRC::Superblock tampered = candidate;
        tampered.m_project_status.m_project_status.insert(
            std::make_pair("growproj", GRC::ProjectEntryStatus::AUTO_GREYLISTED));
        BOOST_CHECK(!service->ValidateProjectStatus(bind_received(tampered), nullptr, chain.m_blocks));
    }

    // --- Tamper: the legitimate entry removed (an empty record where non-empty expected). ---
    {
        GRC::Superblock tampered = candidate;
        tampered.m_project_status.m_project_status.clear();
        BOOST_CHECK(!service->ValidateProjectStatus(bind_received(tampered), nullptr, chain.m_blocks));
    }

    // --- Tamper: the status flipped AUTO -> MAN. ---
    {
        GRC::Superblock tampered = candidate;
        tampered.m_project_status.m_project_status.erase("flatproj");
        tampered.m_project_status.m_project_status.insert(
            std::make_pair("flatproj", GRC::ProjectEntryStatus::MAN_GREYLISTED));
        BOOST_CHECK(!service->ValidateProjectStatus(bind_received(tampered), nullptr, chain.m_blocks));
    }

    // --- Registry dependence: validation runs against the registry as it stands, which the
    // --- ConnectBlock ordering (TryLoadSuperblock BEFORE ApplyContracts) guarantees is the
    // --- same parent-block state the producer stamped against. Demonstrated by mutating the
    // --- registry after the stamp: the honest record now fails, because the expected record
    // --- moved. Same-block project contracts therefore CANNOT affect acceptance -- they are
    // --- applied only after this check has passed. ---
    {
        AddProjectEntryWithStatus("flatproj", "http://flatproj.test",
                                  GRC::ProjectEntryStatus::AUTO_GREYLIST_OVERRIDE, 600, 600);

        BOOST_CHECK(!service->ValidateProjectStatus(bind_received(candidate), nullptr, chain.m_blocks));
    }
}

//!
//! \brief Below the gate, the record is advisory exactly as it is today: the stamp is a
//! no-op and validation accepts anything -- including garbage record bytes.
//!
BOOST_AUTO_TEST_CASE(facade_record_is_advisory_below_the_gate)
{
    RedesignHeightGuard gate_guard(/*gate_height=*/std::numeric_limits<int>::max());

    std::map<std::string, std::vector<std::optional<uint64_t>>> fx;
    fx["flatproj"] = std::vector<std::optional<uint64_t>>(12, std::optional<uint64_t>(500000));

    FacadeChainFixture chain(fx);
    std::shared_ptr<GRC::AutoGreylistService> service = GRC::GetAutoGreylistCache();

    GRC::Superblock candidate = GRC::Superblock();
    candidate.m_projects_all_cpids_total_credits.m_projects_all_cpid_total_credits
        .insert(std::make_pair("flatproj", 500000));
    candidate.m_project_status.m_project_status.insert(
        std::make_pair("garbageproject", GRC::ProjectEntryStatus::AUTO_GREYLISTED));

    // The stamp is a no-op: the (garbage) record is untouched.
    service->StampProjectStatus(candidate, chain.m_tip->nHeight, chain.m_tip->nTime,
                                /*walk_start=*/nullptr, chain.m_blocks);
    BOOST_CHECK(candidate.m_project_status.m_project_status.count("garbageproject") == 1);

    // Validation does not apply: garbage is accepted, as it is on today's network.
    GRC::SuperblockPtr received;
    received.Replace(candidate);
    received.m_height = chain.m_tip->nHeight;
    received.m_timestamp = chain.m_tip->nTime;

    BOOST_CHECK(service->ValidateProjectStatus(received, nullptr, chain.m_blocks));
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
    std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

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
    BOOST_REQUIRE(auto_greylist->Contains(GRC::GreylistState::PENDING, name));

    // Take a Snapshot WITH the overlay applied. On the buggy shallow-copy Snapshot this mutates
    // the shared registry ProjectEntry in place. The test drives RefreshWithSuperblock above so the
    // greylist reflects the synthetic superblock series; the chain-handler-driven Refresh path is
    // bypassed in this unit-test setup.
    whitelist.Snapshot(GRC::GreylistState::PENDING, Filter::ALL_BUT_DELETED);

    // Read back the in-memory registry status WITHOUT re-applying the overlay
    // (include_override=false) -- this reflects whatever the previous Snapshot left behind in
    // m_project_entries.
    Status in_memory_status = Status::UNKNOWN;
    bool found = false;
    for (const auto& entry : whitelist.Snapshot(GRC::GreylistState::NONE, Filter::ALL_BUT_DELETED)) {
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

    std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

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
    std::shared_ptr<GRC::AutoGreylistService> auto_greylist = GRC::GetAutoGreylistCache();

    const std::string name = "push_heal_test_project";
    const std::string url = "http://push.heal.test";

    whitelist.Reset();

    // RAII guard so a mid-test BOOST_REQUIRE failure doesn't leak -autogreylistdeepcopyheight=100
    // into subsequent test cases in the same binary. Also save/restore the chain-tip globals:
    // earlier tests in the suite may leave pindexBest OR pindexGenesisBlock pointing at a
    // stack-allocated CBlockIndex that has since gone out of scope, and Quorum::PushSuperblock's
    // Refresh path runs BlockFinder::FindByHeight, which dereferences one of them. FindByHeight
    // picks its start as `height < nBestHeight/2 ? pindexGenesisBlock : pindexBest`, so nulling
    // pindexBest ALONE is not enough: when the low-height branch is taken (nBestHeight is itself a
    // stale global here), it walks a dangling pindexGenesisBlock instead -- benign on Linux (freed
    // memory reads as a plausible/terminating pointer) but a wild-pointer fault under the Windows
    // cross-compile's Wine run, which scribbles freed memory. Null BOTH so FindByHeight returns at
    // its null-check regardless of the branch.
    struct TestStateGuard {
        CBlockIndex* saved_pindexBest;
        CBlockIndex* saved_pindexGenesisBlock;
        TestStateGuard()
            : saved_pindexBest(pindexBest)
            , saved_pindexGenesisBlock(pindexGenesisBlock)
        {
            pindexBest = nullptr;
            pindexGenesisBlock = nullptr;
        }
        ~TestStateGuard()
        {
            pindexBest = saved_pindexBest;
            pindexGenesisBlock = saved_pindexGenesisBlock;
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
    BOOST_REQUIRE(auto_greylist->Contains(GRC::GreylistState::PENDING, name));

    // Synthetic SBs are at heights 1..N (all pre-gate at 100); m_deep_copy_active should be false.
    BOOST_REQUIRE(!auto_greylist->IsDeepCopyActive(GRC::GreylistState::PENDING));

    // ---- Step 3: trigger the legacy in-place overlay mutation (corruption simulated) ----
    whitelist.Snapshot(GRC::GreylistState::PENDING, Filter::ALL_BUT_DELETED);

    {
        Status in_memory_status = Status::UNKNOWN;
        bool found = false;
        for (const auto& entry : whitelist.Snapshot(GRC::GreylistState::NONE, Filter::ALL_BUT_DELETED)) {
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
    BOOST_CHECK(!auto_greylist->IsDeepCopyActive(GRC::GreylistState::PENDING));

    // Corruption persists -- pre-gate push does not cross the gate.
    {
        Status in_memory_status = Status::UNKNOWN;
        bool found = false;
        for (const auto& entry : whitelist.Snapshot(GRC::GreylistState::NONE, Filter::ALL_BUT_DELETED)) {
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
    BOOST_CHECK(auto_greylist->IsDeepCopyActive(GRC::GreylistState::PENDING));

    // Gate crossing fired -> ReinitFromDisk rebuilt m_project_entries from LevelDB ->
    // in-memory status is back to ACTIVE.
    {
        Status in_memory_status = Status::UNKNOWN;
        bool found = false;
        for (const auto& entry : whitelist.Snapshot(GRC::GreylistState::NONE, Filter::ALL_BUT_DELETED)) {
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

BOOST_AUTO_TEST_CASE(it_does_not_throw_when_the_40SB_average_truncates_to_zero)
{
    // Regression test for the GetWAS() divide-by-zero that crashed mainnet nodes in
    // ThreadScraperSubscriber (St12out_of_range "denominator specified is zero").
    //
    // GetWAS() returns Fraction(TC_7_SB_avg, TC_40_SB_avg), where both averages are
    // integer divisions (sum / min(processed, 7|40)). A low-activity project whose
    // 40-SB total-credit delta is small enough that m_TC_40_SB_sum / 40 truncates to 0,
    // while m_TC_7_SB_sum / 7 is still non-zero, used to fall through the old
    // "TC_7_SB_avg == 0 && TC_40_SB_avg == 0" guard to Fraction(non-zero, 0), which
    // throws. The fix guards on the denominator (TC_40_SB_avg) alone and returns WAS = 0.
    //
    // Drive the candidate through the public UpdateGreylistCandidateEntry path (which
    // itself calls GetWAS() internally) so this reproduces the exact production code path.
    GRC::AutoGreylist::GreylistCandidateEntry candidate("TestProject", std::optional<uint64_t>(1000));

    // sb_from_baseline = 7, total credit 990: 7-SB delta = 1000 - 990 = 10 -> m_TC_7_SB_sum = 10.
    candidate.UpdateGreylistCandidateEntry(990, 7, false);

    // sb_from_baseline = 40, total credit 970: 40-SB delta = 1000 - 970 = 30 -> m_TC_40_SB_sum = 30,
    // m_sb_from_baseline_processed = 40. GetWAS() (called inside UpdateGreylistCandidateEntry) then
    // computes TC_7_SB_avg = 10 / min(40,7) = 10/7 = 1 (non-zero) and
    // TC_40_SB_avg = 30 / min(40,40) = 30/40 = 0 (integer truncation) -- the crash trigger.
    BOOST_REQUIRE_NO_THROW(candidate.UpdateGreylistCandidateEntry(970, 40, false));

    // A zero 40-SB average means negligible long-term work availability, so WAS = 0.0.
    BOOST_CHECK(candidate.GetWAS() == Fraction(0));
}

BOOST_AUTO_TEST_SUITE_END()

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
