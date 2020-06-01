#include "neuralnet/contract/contract.h"
#include "neuralnet/project.h"

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
NN::Contract contract(std::string key, std::string value)
{
    return NN::MakeContract<NN::Project>(
        // Add or delete checked before passing to handler, so we don't need
        // to give a specific value here:
        NN::ContractAction::UNKNOWN,
        std::move(key),
        std::move(value),
        1234567); // timestamp
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Project
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Project)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_project)
{
    const NN::Project project;

    BOOST_CHECK_EQUAL(project.m_version, NN::Project::CURRENT_VERSION);
    BOOST_CHECK(project.m_name.empty() == true);
    BOOST_CHECK(project.m_url.empty() == true);
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_a_new_project_contract)
{
    const NN::Project project("Enigma", "http://enigma.test/@");

    BOOST_CHECK_EQUAL(project.m_version, NN::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_project_contract_data)
{
    const NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.m_version, NN::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 1234567);
}

BOOST_AUTO_TEST_CASE(it_formats_the_user_friendly_display_name)
{
    const NN::Project project("Enigma_at_Home", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.DisplayName(), "Enigma at Home");
}

BOOST_AUTO_TEST_CASE(it_formats_the_base_project_url)
{
    const NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.BaseUrl(), "http://enigma.test/");
}

BOOST_AUTO_TEST_CASE(it_formats_the_project_display_url)
{
    const NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.DisplayUrl(), "http://enigma.test/");
}

BOOST_AUTO_TEST_CASE(it_formats_the_project_stats_url)
{
    const NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.StatsUrl(), "http://enigma.test/stats/");
}

BOOST_AUTO_TEST_CASE(it_formats_a_project_stats_archive_url)
{
    const NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK_EQUAL(project.StatsUrl("user"), "http://enigma.test/stats/user.gz");
    BOOST_CHECK_EQUAL(project.StatsUrl("team"), "http://enigma.test/stats/team.gz");
}

BOOST_AUTO_TEST_CASE(it_behaves_like_a_contract_payload)
{
    const NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.ContractType() == NN::ContractType::PROJECT);
    BOOST_CHECK(project.WellFormed(NN::ContractAction::ADD) == true);
    BOOST_CHECK(project.LegacyKeyString() == "Enigma");
    BOOST_CHECK(project.LegacyValueString() == "http://enigma.test/@");
    BOOST_CHECK(project.RequiredBurnAmount() > 0);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed_for_add)
{
    const NN::Project valid("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(valid.WellFormed(NN::ContractAction::ADD) == true);

    const NN::Project no_name("", "http://enigma.test/@", 1234567);

    BOOST_CHECK(no_name.WellFormed(NN::ContractAction::ADD) == false);

    const NN::Project no_url("Enigma", "", 1234567);
    BOOST_CHECK(no_url.WellFormed(NN::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_the_payload_is_well_formed_for_delete)
{
    const NN::Project valid("Enigma", "", 1234567);

    BOOST_CHECK(valid.WellFormed(NN::ContractAction::REMOVE) == true);

    const NN::Project no_name("", "http://enigma.test/@", 1234567);

    BOOST_CHECK(no_name.WellFormed(NN::ContractAction::ADD) == false);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_add)
{
    const NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::Project::CURRENT_VERSION
        << std::string("Enigma")
        << std::string("http://enigma.test/@");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    project.Serialize(stream, NN::ContractAction::ADD);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_add)
{
    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::Project::CURRENT_VERSION
        << std::string("Enigma")
        << std::string("http://enigma.test/@");

    NN::Project project;
    project.Unserialize(stream, NN::ContractAction::ADD);

    BOOST_CHECK_EQUAL(project.m_version, NN::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "http://enigma.test/@");
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);

    BOOST_CHECK(project.WellFormed(NN::ContractAction::ADD) == true);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_delete)
{
    const NN::Project project("Enigma", "", 1234567);

    const CDataStream expected = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::Project::CURRENT_VERSION
        << std::string("Enigma");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    project.Serialize(stream, NN::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_delete)
{
    CDataStream stream = CDataStream(SER_NETWORK, PROTOCOL_VERSION)
        << NN::Project::CURRENT_VERSION
        << std::string("Enigma");

    NN::Project project;
    project.Unserialize(stream, NN::ContractAction::REMOVE);

    BOOST_CHECK_EQUAL(project.m_version, NN::Project::CURRENT_VERSION);
    BOOST_CHECK_EQUAL(project.m_name, "Enigma");
    BOOST_CHECK_EQUAL(project.m_url, "");
    BOOST_CHECK_EQUAL(project.m_timestamp, 0);

    BOOST_CHECK(project.WellFormed(NN::ContractAction::REMOVE) == true);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// WhitelistSnapshot
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(WhitelistSnapshot)

BOOST_AUTO_TEST_CASE(it_is_iterable)
{
    NN::WhitelistSnapshot s(std::make_shared<NN::ProjectList>(NN::ProjectList {
        NN::Project("Enigma", "http://enigma.test/@", 1234567),
        NN::Project("Einstein@home", "http://einsteinathome.org/@", 1234567),
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
    NN::WhitelistSnapshot s1(std::make_shared<NN::ProjectList>());

    BOOST_CHECK(s1.size() == 0);

    NN::WhitelistSnapshot s2(std::make_shared<NN::ProjectList>(NN::ProjectList {
        NN::Project("Enigma", "http://enigma.test/@", 1234567),
        NN::Project("Einstein@home", "http://einsteinathome.org/@", 1234567),
    }));

    BOOST_CHECK(s2.size() == 2);
}

BOOST_AUTO_TEST_CASE(it_indicates_whether_it_contains_any_projects)
{
    NN::WhitelistSnapshot s1(std::make_shared<NN::ProjectList>());

    BOOST_CHECK(s1.Populated() == false);

    NN::WhitelistSnapshot s2(std::make_shared<NN::ProjectList>(NN::ProjectList {
        NN::Project("Enigma", "http://enigma.test/@", 1234567),
        NN::Project("Einstein@home", "http://einsteinathome.org/@", 1234567),
    }));

    BOOST_CHECK(s2.Populated() == true);
}

BOOST_AUTO_TEST_CASE(it_sorts_a_copy_of_the_projects_by_name)
{
    // WhitelistSnapshot performs a case-insensitive sort, so we add an upper-
    // case project name to verify:
    NN::WhitelistSnapshot snapshot = NN::WhitelistSnapshot(
        std::make_shared<NN::ProjectList>(NN::ProjectList {
            NN::Project("c", "http://c.example.com/@", 1234567),
            NN::Project("a", "http://a.example.com/@", 1234567),
            NN::Project("B", "http://b.example.com/@", 1234567),
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
    NN::Whitelist whitelist;

    BOOST_CHECK(whitelist.Snapshot().size() == 0);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == false);

    whitelist.Add(contract("Enigma", "http://enigma.test"));

    BOOST_CHECK(whitelist.Snapshot().size() == 1);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == true);
}

BOOST_AUTO_TEST_CASE(it_removes_whitelisted_projects_from_contract_data)
{
    NN::Whitelist whitelist;
    whitelist.Add(contract("Enigma", "http://enigma.test"));

    BOOST_CHECK(whitelist.Snapshot().size() == 1);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == true);

    whitelist.Delete(contract("Enigma", "http://enigma.test"));

    BOOST_CHECK(whitelist.Snapshot().size() == 0);
    BOOST_CHECK(whitelist.Snapshot().Contains("Enigma") == false);
}

BOOST_AUTO_TEST_CASE(it_does_not_mutate_existing_snapshots)
{
    NN::Whitelist whitelist;
    whitelist.Add(contract("Enigma", "http://enigma.test"));

    auto snapshot = whitelist.Snapshot();
    whitelist.Delete(contract("Enigma", "http://enigma.test"));

    BOOST_CHECK(snapshot.Contains("Enigma") == true);
}

BOOST_AUTO_TEST_CASE(it_overwrites_projects_with_the_same_name)
{
    NN::Whitelist whitelist;
    whitelist.Add(contract("Enigma", "http://enigma.test"));
    whitelist.Add(contract("Enigma", "http://new.enigma.test"));

    auto snapshot = whitelist.Snapshot();
    BOOST_CHECK(snapshot.size() == 1);

    for (const auto& project : snapshot) {
        BOOST_CHECK(project.m_url == "http://new.enigma.test");
    }
}

BOOST_AUTO_TEST_SUITE_END()
