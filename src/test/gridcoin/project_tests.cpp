// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"

#include <boost/test/unit_test.hpp>

namespace {
//!
//! \brief Generate a mock project contract.
//!
//! \param key   A fake project name as it might appear in a contract.
//! \param value A fake project URL as it might appear in a contract.
//!
//! \return A mock project contract.
//!
GRC::Contract contract(std::string key, std::string value)
{
    return GRC::MakeContract<GRC::Project>(
        // Add or delete checked before passing to handler, so we don't need
        // to give a specific value here:
        GRC::ContractAction::UNKNOWN,
        std::move(key),
        std::move(value),
        1234567); // timestamp
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
    const GRC::Project project("Enigma", "http://enigma.test/@", 1234567);

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::Project::CURRENT_VERSION
        << std::string("Enigma")
        << std::string("http://enigma.test/@");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    project.Serialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_add)
{
    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::Project::CURRENT_VERSION
        << std::string("Enigma")
        << std::string("http://enigma.test/@");

    GRC::Project project;
    project.Unserialize(stream, GRC::ContractAction::ADD);

    BOOST_CHECK_EQUAL(project.m_version, GRC::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);

    BOOST_CHECK(project.WellFormed(GRC::ContractAction::ADD) == true);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_delete)
{
    const GRC::Project project("Enigma", "", 1234567);

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::Project::CURRENT_VERSION
        << std::string("Enigma");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    project.Serialize(stream, GRC::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_delete)
{
    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << GRC::Project::CURRENT_VERSION
        << std::string("Enigma");

    GRC::Project project;
    project.Unserialize(stream, GRC::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL(project.m_version, GRC::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "");
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);

    BOOST_CHECK(project.WellFormed(GRC::ContractAction::REMOVE) == true);
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
    GRC::Whitelist whitelist;

    BOOST_CHECK(whitelist.Snapshot().size() == 0);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == false);

    const GRC::Contract project = contract("Enigma", "http://enigma.test");
    whitelist.Add({ project, g_tx, nullptr });

    BOOST_CHECK(whitelist.Snapshot().size() == 1);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == true);
}

BOOST_AUTO_TEST_CASE(it_removes_whitelisted_projects_from_contract_data)
{
    GRC::Whitelist whitelist;
    const GRC::Contract project = contract("Enigma", "http://enigma.test");

    whitelist.Add({ project, g_tx, nullptr });

    BOOST_CHECK(whitelist.Snapshot().size() == 1);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == true);

    whitelist.Delete({ project, g_tx, nullptr });

    BOOST_CHECK(whitelist.Snapshot().size() == 0);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == false);
}

BOOST_AUTO_TEST_CASE(it_does_not_mutate_existing_snapshots)
{
    GRC::Whitelist whitelist;
    const GRC::Contract project = contract("Enigma", "http://enigma.test");

    whitelist.Add({ project, g_tx, nullptr });

    auto snapshot = whitelist.Snapshot();
    whitelist.Delete({ project, g_tx, nullptr });

    BOOST_CHECK(snapshot.Contains("Enigma") == true);
}

BOOST_AUTO_TEST_CASE(it_overwrites_projects_with_the_same_name)
{
    GRC::Whitelist whitelist;
    const GRC::Contract project1 = contract("Enigma", "http://enigma.test");
    const GRC::Contract project2 = contract("Enigma", "http://new.enigma.test");

    whitelist.Add({ project1, g_tx, nullptr });
    whitelist.Add({ project2, g_tx, nullptr });

    auto snapshot = whitelist.Snapshot();
    BOOST_CHECK(snapshot.size() == 1);

    for (const auto& project : snapshot) {
        BOOST_CHECK(project.m_url == "http://new.enigma.test");
    }
}

BOOST_AUTO_TEST_SUITE_END()
