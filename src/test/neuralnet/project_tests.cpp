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
    return NN::Contract(
        NN::Contract::CURRENT_VERSION,
        NN::ContractType::PROJECT,
        // Add or delete checked before passing to handler, so we don't need
        // to give a specific value here:
        NN::ContractAction::UNKNOWN,
        key,
        value,
        // Signature checked before passing to handler, so we don't need to
        // give specific signature or public key here:
        NN::Contract::Signature(),
        NN::Contract::PublicKey(),
        1234567);
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Project
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Project)

BOOST_AUTO_TEST_CASE(it_provides_access_to_project_contract_data)
{
    NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.m_name == "Enigma");
    BOOST_CHECK(project.m_url == "http://enigma.test/@");
    BOOST_CHECK(project.m_timestamp == 1234567);
}

BOOST_AUTO_TEST_CASE(it_initializes_from_a_contract)
{
    NN::Project project(contract("Enigma", "http://enigma.test/@"));

    BOOST_CHECK(project.m_name == "Enigma");
    BOOST_CHECK(project.m_url == "http://enigma.test/@");
    BOOST_CHECK(project.m_timestamp == 1234567);
}

BOOST_AUTO_TEST_CASE(it_formats_the_user_friendly_display_name)
{
    NN::Project project("Enigma_at_Home", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.DisplayName() == "Enigma at Home");
}

BOOST_AUTO_TEST_CASE(it_formats_the_base_project_url)
{
    NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.BaseUrl() == "http://enigma.test/");
}

BOOST_AUTO_TEST_CASE(it_formats_the_project_display_url)
{
    NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.DisplayUrl() == "http://enigma.test/");
}

BOOST_AUTO_TEST_CASE(it_formats_the_project_stats_url)
{
    NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.StatsUrl() == "http://enigma.test/stats/");
}

BOOST_AUTO_TEST_CASE(it_formats_a_project_stats_archive_url)
{
    NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    BOOST_CHECK(project.StatsUrl("user") == "http://enigma.test/stats/user.gz");
    BOOST_CHECK(project.StatsUrl("team") == "http://enigma.test/stats/team.gz");
}

BOOST_AUTO_TEST_CASE(it_converts_itself_into_a_contract)
{
    NN::Project project("Enigma", "http://enigma.test/@", 1234567);

    NN::Contract contract = project.IntoContract();

    BOOST_CHECK(contract.m_type == NN::ContractType::PROJECT);
    BOOST_CHECK(contract.m_action == NN::ContractAction::ADD);
    BOOST_CHECK(contract.m_key == "Enigma");
    BOOST_CHECK(contract.m_value == "http://enigma.test/@");
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
