// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"

#include <boost/test/unit_test.hpp>

namespace {
void AddProjectEntry(const uint32_t& payload_version, const std::string& name, const std::string& url,
                     const bool& gdpr_status, const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::Whitelist& registry = GRC::GetWhitelist();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
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

    registry.Add({contract, dummy_tx, &dummy_index});
}

void DeleteProjectEntry(const uint32_t& payload_version, const std::string& name,
                        const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::Whitelist& registry = GRC::GetWhitelist();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
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

    registry.Add({contract, dummy_tx, &dummy_index});
}

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

    BOOST_CHECK(whitelist.Snapshot().size() == 0);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == false);

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);

    BOOST_CHECK(whitelist.Snapshot().size() == 1);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == true);

    AddProjectEntry(2, "Foo", "http://foo.test", false, height++, time++, false);

    BOOST_CHECK(whitelist.Snapshot().size() == 2);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == true);
    BOOST_CHECK(whitelist.Snapshot().Contains("Foo") == true);
}

BOOST_AUTO_TEST_CASE(it_removes_whitelisted_projects_from_contract_data)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);

    BOOST_CHECK(whitelist.Snapshot().size() == 1);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == true);

    DeleteProjectEntry(1, "Enigma", height++, time++, false);

    BOOST_CHECK(whitelist.Snapshot().size() == 0);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == false);
}

BOOST_AUTO_TEST_CASE(it_does_not_mutate_existing_snapshots)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);
    AddProjectEntry(2, "Foo", "http://foo.test", true, height++, time++, false);

    auto snapshot = whitelist.Snapshot();

    DeleteProjectEntry(1, "Enigma", height, time, false);

    BOOST_CHECK(snapshot.Contains("Enigma") == true);

    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == false);
    BOOST_CHECK(whitelist.Snapshot().Contains("Foo") == true);
}

BOOST_AUTO_TEST_CASE(it_overwrites_projects_with_the_same_name)
{
    GRC::Whitelist& whitelist = GRC::GetWhitelist();

    int height = 0;
    int64_t time = 0;

    AddProjectEntry(1, "Enigma", "http://enigma.test", false, height, time, true);
    AddProjectEntry(2, "Enigma", "http://new.enigma.test", true, height++, time++, false);

    auto snapshot = whitelist.Snapshot();
    BOOST_CHECK(snapshot.size() == 1);

    for (const auto& project : snapshot) {
        BOOST_CHECK(project.m_url == "http://new.enigma.test");
    }
}

BOOST_AUTO_TEST_SUITE_END()
