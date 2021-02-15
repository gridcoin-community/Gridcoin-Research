// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/appcache.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/project.h"
#include "gridcoin/researcher.h"
#include "util.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <vector>

namespace {
//!
//! \brief Attempt to locate the test data directory that contains a BOINC
//! client_state.xml stub file.
//!
//! \return Path to the test data directory containing the stub.
//!
fs::path ResolveStubDir()
{
    const fs::path cwd = fs::current_path();
    const fs::path data_dir("src/test/data");
    const std::string stub = "client_state.xml";

    // Test harness run from subdirectory of repo root (src/, build/, etc.):
    if (fs::exists(cwd / ".." / data_dir / stub)) {
        return fs::canonical(cwd / ".." / data_dir);
    }

    // Test harness run from platform-specific build sub-directory
    // (ex: build/gridcoin-x86_64-unknown-linux-gnu/)
    if (fs::exists(cwd / ".." / ".." / data_dir / stub)) {
        return fs::canonical(cwd / ".." / ".." / data_dir);
    }

    // Test harness run from sub-directory in a platform-specific build sub-
    // directory (Travis; ex: build/gridcoin-x86_64-unknown-linux-gnu/src)
    if (fs::exists(cwd / ".." / ".." / ".." / data_dir / stub)) {
        return fs::canonical(cwd / ".." / ".." / ".." / data_dir);
    }

    // Test harness run from test directory (src/test/):
    if (fs::exists(cwd / "data" / stub)) {
        return cwd / "data";
    }

    // Test harness run from repo root:
    return cwd / data_dir;
}

// Why do we count the beacons?
// Because otherwise the hashes of all beacon entries collide since calls to GetAdjustedTime
// are equal. Since the previously deleted beacon entries are retained, subsequent beacon additions
// fail as the hash for the new beacon is equal to the previously deleted ones.
int64_t nBeaconCount = 0;

//!
//! \brief Register an active beacon for testing.
//!
//! \param cpid External CPID used in the test.
//!
void AddTestBeacon(const GRC::Cpid cpid)
{
    // TODO: mock the beacon registry
    CPubKey public_key = CPubKey(ParseHex(
        "111111111111111111111111111111111111111111111111111111111111111111"));

    const CKeyID key_id = public_key.GetID();
    const int64_t now = GetAdjustedTime() + nBeaconCount;
    nBeaconCount++;
    

    // Dummy transaction for the contract handler API:
    CTransaction tx;
    tx.nTime = now;

    GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::ADD,
        cpid,
        std::move(public_key));

    GRC::GetBeaconRegistry().Add({ contract, tx, nullptr });
    GRC::GetBeaconRegistry().ActivatePending({ key_id }, now, uint256(), -1);
}

//!
//! \brief Register an expired beacon for testing.
//!
//! \param cpid External CPID used in the test.
//!
void AddExpiredTestBeacon(const GRC::Cpid cpid)
{
    // TODO: mock the beacon registry
    CPubKey public_key = CPubKey(ParseHex(
        "111111111111111111111111111111111111111111111111111111111111111111"));

    const CKeyID key_id = public_key.GetID();

    // Dummy transaction for the contract handler API:
    CTransaction tx;
    tx.nTime = nBeaconCount;
    nBeaconCount++;

    GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::ADD,
        cpid,
        std::move(public_key));

    GRC::GetBeaconRegistry().Add({ contract, tx, nullptr });
    GRC::GetBeaconRegistry().ActivatePending({ key_id }, 0, uint256(), -1);
}

//!
//! \brief Remove a beacon added for testing.
//!
//! \param cpid External CPID used in the test.
//!
void RemoveTestBeacon(const GRC::Cpid cpid)
{
    // TODO: mock the beacon registry
    CPubKey public_key = CPubKey(ParseHex(
        "111111111111111111111111111111111111111111111111111111111111111111"));

    // Dummy transaction for the contract handler API:
    CTransaction tx;
    tx.nTime = 0;

    uint256 mock_superblock_hash = uint256();

    GRC::Contract contract = GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::ADD,
        cpid,
        std::move(public_key));

    GRC::GetBeaconRegistry().Deactivate(mock_superblock_hash);
    GRC::GetBeaconRegistry().Delete({ contract, tx, nullptr });
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// MiningProject
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(MiningProject)

BOOST_AUTO_TEST_CASE(it_initializes_with_project_data)
{
    GRC::Cpid expected(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    GRC::MiningProject project("project name", expected, "team name", "url",
                               0.0);

    BOOST_CHECK(project.m_name == "project name");
    BOOST_CHECK(project.m_cpid == expected);
    BOOST_CHECK(project.m_team == "team name");
    BOOST_CHECK(project.m_url == "url");
    BOOST_CHECK(project.m_rac == 0.0);
    BOOST_CHECK(project.m_error == GRC::MiningProject::Error::NONE);
}

BOOST_AUTO_TEST_CASE(it_parses_a_project_xml_string)
{
    SetArgument("email", "researcher@example.com");

    // The XML string contains a subset of data found within a <project> element
    // from BOINC's client_state.xml file:
    //
    GRC::MiningProject project = GRC::MiningProject::Parse(
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name</project_name>
          <team_name>Team Name</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
          <user_expavg_credit>123.45</user_expavg_credit>
        </project>
        )XML");

    GRC::Cpid cpid = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    BOOST_CHECK(project.m_name == "project name");
    BOOST_CHECK(project.m_cpid == cpid);
    BOOST_CHECK(project.m_team == "team name");
    BOOST_CHECK(project.m_url == "https://example.com/");
    BOOST_CHECK(project.m_rac == 123.45);
    BOOST_CHECK(project.m_error == GRC::MiningProject::Error::NONE);

    // Clean up:
    SetArgument("email", "");
}

BOOST_AUTO_TEST_CASE(it_falls_back_to_compute_a_missing_external_cpid)
{
    SetArgument("email", "researcher@example.com");

    // A bug in BOINC sometimes results in the empty empty external CPID field
    // as shown below. This will recompute the CPID from the internal CPID and
    // the email address defined above if the email hash field matches the MD5
    // digest of the email address:
    //
    GRC::MiningProject project = GRC::MiningProject::Parse(
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name</project_name>
          <team_name>Team Name</team_name>
          <email_hash>b8b58cce3c90c7dc9f3601674202b21c</email_hash>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid></external_cpid>
          <user_expavg_credit>123.45</user_expavg_credit>
        </project>
        )XML");

    GRC::Cpid cpid = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    BOOST_CHECK(project.m_name == "project name");
    BOOST_CHECK(project.m_cpid == cpid);
    BOOST_CHECK(project.m_team == "team name");
    BOOST_CHECK(project.m_url == "https://example.com/");
    BOOST_CHECK(project.m_rac == 123.45);
    BOOST_CHECK(project.m_error == GRC::MiningProject::Error::NONE);

    // Clean up:
    SetArgument("email", "");
}

BOOST_AUTO_TEST_CASE(it_normalizes_project_names)
{
    GRC::MiningProject project("Project_NAME", GRC::Cpid(), "team name", "url",
                               0.0);

    BOOST_CHECK(project.m_name == "project name");
}

BOOST_AUTO_TEST_CASE(it_converts_team_names_to_lowercase)
{
    GRC::MiningProject project("project name", GRC::Cpid(), "TEAM NAME", "url",
                               0.0);

    BOOST_CHECK(project.m_team == "team name");
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_project_is_eligible)
{
    // Eligibility is determined by the absence of an error set while loading
    // the project from BOINC's client_state.xml file.

    GRC::MiningProject project("project name", GRC::Cpid(), "team name", "url",
                               0.0);

    BOOST_CHECK(project.Eligible() == true);

    project.m_error = GRC::MiningProject::Error::INVALID_TEAM;

    BOOST_CHECK(project.Eligible() == false);
}

BOOST_AUTO_TEST_CASE(it_detects_projects_with_pool_cpids)
{
    // The XML string contains a subset of data found within a <project> element
    // from BOINC's client_state.xml file:
    //
    GRC::MiningProject project = GRC::MiningProject::Parse(
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name</project_name>
          <team_name>Team Name</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>7d0d73fe026d66fd4ab8d5d8da32a611</external_cpid>
        </project>
        )XML");

    BOOST_CHECK(project.m_error == GRC::MiningProject::Error::POOL);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_project_is_whitelisted)
{
    GRC::MiningProject project("project name", GRC::Cpid(), "team name", "url",
                               0.0);

    GRC::WhitelistSnapshot s(std::make_shared<GRC::ProjectList>(GRC::ProjectList {
        GRC::Project("Enigma", "http://enigma.test/@", 1234567),
        GRC::Project("Einstein@home", "http://einsteinathome.org/@", 1234567),
    }));

    BOOST_CHECK(project.Whitelisted(s) == false);

    // By name (exact):
    project.m_name = "Enigma";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // Invalidate the name so we can test the URL matching:
    project.m_name = "project name";

    // By URL (exact):
    BOOST_CHECK(project.Whitelisted(s) == false);
    project.m_url = "http://enigma.test/@";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // By URL (different scheme):
    project.m_url = "https://enigma.test/";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // By URL (no scheme):
    project.m_url = "enigma.test/";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // By URL (just domain):
    project.m_url = "enigma.test";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // By URL (different path):
    project.m_url = "enigma.test/boincitty-boinc-boinc";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // By URL (query parameter):
    project.m_url = "enigma.test?boincitty-boinc-boinc";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // By URL (mix):
    project.m_url = "https://enigma.test/test?boincitty-boinc-boinc";
    BOOST_CHECK(project.Whitelisted(s) == true);

    // By URL (port numbers count):
    project.m_url = "https://enigma.test:8000/";
    BOOST_CHECK(project.Whitelisted(s) == false);
    project.m_url = "enigma.test:8000";
    BOOST_CHECK(project.Whitelisted(s) == false);
}

BOOST_AUTO_TEST_CASE(it_formats_error_messages_for_display)
{
    GRC::MiningProject project("project name", GRC::Cpid(), "team name", "url",
                               0.0);

    BOOST_CHECK(project.ErrorMessage().empty() == true);

    project.m_error = GRC::MiningProject::Error::INVALID_TEAM;

    BOOST_CHECK(project.ErrorMessage().empty() == false);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// MiningProjectMap
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(MiningProjectMap)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_collection)
{
    GRC::MiningProjectMap projects;

    BOOST_CHECK(projects.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_is_iterable)
{
    GRC::MiningProjectMap projects;

    projects.Set(GRC::MiningProject("project name 1", GRC::Cpid(), "team name",
                                    "url", 0.0));
    projects.Set(GRC::MiningProject("project name 2", GRC::Cpid(), "team name",
                                    "url", 0.0));

    auto counter = 0;

    for (auto const& project : projects) {
        BOOST_CHECK(boost::starts_with(project.first, "project name") == true);
        counter++;
    }

    BOOST_CHECK(counter == 2);
}

BOOST_AUTO_TEST_CASE(it_parses_a_set_of_project_xml_sections)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    GRC::MiningProjectMap projects = GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/1</master_url>
          <project_name>Project Name 1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>123.45</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <master_url>https://example.com/2</master_url>
          <project_name>Project Name 2</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY</cross_project_id>
          <user_expavg_credit>567.89</user_expavg_credit>
          <external_cpid>8edc235ddcecf9c416a5f9417d56f4fd</external_cpid>
        </project>
        )XML",
    });

    GRC::Cpid cpid_1 = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");
    GRC::Cpid cpid_2 = GRC::Cpid::Parse("8edc235ddcecf9c416a5f9417d56f4fd");

    BOOST_CHECK(projects.size() == 2);

    if (const GRC::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid_1);
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_url == "https://example.com/1");
        BOOST_CHECK(project1->m_rac == 123.45);
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid_2);
        BOOST_CHECK(project2->m_team == "gridcoin");
        BOOST_CHECK(project2->m_url == "https://example.com/2");
        BOOST_CHECK(project2->m_rac == 567.89);
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_skips_loading_project_xml_with_empty_project_names)
{
    GRC::MiningProjectMap projects = GRC::MiningProjectMap::Parse({
        // Empty <project_name> element:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name></project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>123.45</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // Missing <project_name> element:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>123.45</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    });

    // No valid projects loaded; mining ID should remain INVESTOR:
    BOOST_CHECK(projects.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_counts_the_number_of_projects)
{
    GRC::MiningProjectMap projects;

    BOOST_CHECK(projects.size() == 0);

    projects.Set(GRC::MiningProject("project name 1", GRC::Cpid(), "team name",
                                    "url", 0.0));
    projects.Set(GRC::MiningProject("project name 2", GRC::Cpid(), "team name",
                                    "url", 0.0));

    BOOST_CHECK(projects.size() == 2);
}

BOOST_AUTO_TEST_CASE(it_indicates_whether_it_contains_any_projects)
{
    GRC::MiningProjectMap projects;

    BOOST_CHECK(projects.empty() == true);

    projects.Set(GRC::MiningProject("project name", GRC::Cpid(), "team name",
                                    "url", 0.0));

    BOOST_CHECK(projects.empty() == false);
}

BOOST_AUTO_TEST_CASE(it_indicates_whether_it_contains_any_pool_projects)
{
    GRC::MiningProjectMap projects;
    GRC::MiningProject project("project name", GRC::Cpid(), "team name",
                               "url", 0.0);

    project.m_error = GRC::MiningProject::Error::POOL;
    projects.Set(std::move(project));

    BOOST_CHECK(projects.ContainsPool() == true);
}

BOOST_AUTO_TEST_CASE(it_fetches_a_project_by_name)
{
    GRC::MiningProjectMap projects;

    projects.Set(GRC::MiningProject("project name", GRC::Cpid(), "team name",
                                    "url", 0.0));

    BOOST_CHECK(projects.Try("project name")->m_name == "project name");
    BOOST_CHECK(projects.Try("nonexistent") == nullptr);
}

BOOST_AUTO_TEST_CASE(it_does_not_overwrite_projects_with_the_same_name)
{
    GRC::MiningProjectMap projects;

    projects.Set(GRC::MiningProject("project name", GRC::Cpid(), "team name 1",
                                    "url", 0.0));
    projects.Set(GRC::MiningProject("project name", GRC::Cpid(), "team name 2",
                                    "url", 0.0));

    BOOST_CHECK(projects.Try("project name")->m_team == "team name 1");
}

BOOST_AUTO_TEST_CASE(it_applies_a_provided_team_whitelist)
{
    GRC::MiningProjectMap projects;

    projects.Set(GRC::MiningProject("project 1", GRC::Cpid(), "gridcoin",
                                    "url", 1.0));
    projects.Set(GRC::MiningProject("project 2", GRC::Cpid(), "team 1",
                                    "url", 0.0));
    projects.Set(GRC::MiningProject("project 3", GRC::Cpid(), "team 2",
                                    "url", 0.0));

    // Before applying a whitelist, all projects are eligible:

    if (const GRC::ProjectOption project1 = projects.Try("project 1")) {
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("project 2")) {
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project3 = projects.Try("project 3")) {
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project3->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Applying this whitelist disables eligibility for project 1:
    //
    projects.ApplyTeamWhitelist({ "team 1", "team 2" });

    if (const GRC::ProjectOption project1 = projects.Try("project 1")) {
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project1->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("project 2")) {
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project3 = projects.Try("project 3")) {
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project3->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Applying an empty whitelist removes the team requirement:
    //
    projects.ApplyTeamWhitelist({ });

    if (const GRC::ProjectOption project1 = projects.Try("project 1")) {
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("project 2")) {
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project3 = projects.Try("project 3")) {
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project3->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Researcher
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Researcher)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_investor)
{
    GRC::Researcher researcher;

    BOOST_CHECK(researcher.Id().Which() == GRC::MiningId::Kind::INVESTOR);
    BOOST_CHECK(researcher.Projects().empty() == true);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_researcher_context_data)
{
    GRC::MiningProjectMap expected;

    expected.Set(GRC::MiningProject("project name", GRC::Cpid(), "team name",
                                    "url", 0.0));

    GRC::Researcher researcher(GRC::Cpid(), expected);

    BOOST_CHECK(researcher.Id().Which() == GRC::MiningId::Kind::CPID);
    BOOST_CHECK(researcher.Projects().size() == 1);
}

BOOST_AUTO_TEST_CASE(it_converts_a_configured_email_address_to_lowercase)
{
    SetArgument("email", "RESEARCHER@EXAMPLE.COM");

    BOOST_CHECK(GRC::Researcher::Email() == "researcher@example.com");

    // Clean up:
    SetArgument("email", "");
}

BOOST_AUTO_TEST_CASE(it_provides_access_to_a_global_researcher_singleton)
{
    GRC::ResearcherPtr researcher(GRC::Researcher::Get());

    BOOST_CHECK(researcher->Id() == GRC::MiningId::ForInvestor());
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_wallet_is_eligible_for_rewards)
{
    GRC::Researcher researcher;

    BOOST_CHECK(researcher.Eligible() == false);
    BOOST_CHECK(researcher.IsInvestor() == true);

    // A zero-value CPID is technically a valid CPID:
    researcher = GRC::Researcher(GRC::Cpid(), GRC::MiningProjectMap());

    AddTestBeacon(GRC::Cpid());

    BOOST_CHECK(researcher.Eligible() == true);
    BOOST_CHECK(researcher.IsInvestor() == false);

    // Clean up:
    RemoveTestBeacon(GRC::Cpid());
}

BOOST_AUTO_TEST_CASE(it_reports_ineligible_when_beacon_missing_or_expired)
{
    GRC::Researcher researcher{GRC::Cpid(), GRC::MiningProjectMap()};

    BOOST_CHECK(researcher.Eligible() == false);

    AddExpiredTestBeacon(GRC::Cpid());

    BOOST_CHECK(researcher.Eligible() == false);

    RemoveTestBeacon(GRC::Cpid());
    AddTestBeacon(GRC::Cpid());

    BOOST_CHECK(researcher.Eligible() == true);

    // Clean up:
    RemoveTestBeacon(GRC::Cpid());
}

BOOST_AUTO_TEST_CASE(it_provides_an_overall_status_of_the_researcher_context)
{
    GRC::Researcher researcher;

    BOOST_CHECK(researcher.Status() == GRC::ResearcherStatus::INVESTOR);

    GRC::MiningProjectMap projects;
    projects.Set(GRC::MiningProject("ineligible", GRC::Cpid(), "team name",
                                    "url", 0.0));

    researcher = GRC::Researcher(GRC::MiningId::ForInvestor(), projects);

    // Has projects but none eligible (investor):
    BOOST_CHECK(researcher.Status() == GRC::ResearcherStatus::NO_PROJECTS);

    researcher = GRC::Researcher(GRC::Cpid(), projects);

    // Has eligible projects but no beacon:
    BOOST_CHECK(researcher.Status() == GRC::ResearcherStatus::NO_BEACON);

    AddTestBeacon(GRC::Cpid());

    BOOST_CHECK(researcher.Status() == GRC::ResearcherStatus::ACTIVE);

    // Clean up:
    RemoveTestBeacon(GRC::Cpid());
}

BOOST_AUTO_TEST_CASE(it_parses_project_xml_to_a_global_researcher_singleton)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/1</master_url>
          <project_name>Project Name 1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>1.1</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <master_url>https://example.com/2</master_url>
          <project_name>Project Name 2</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY</cross_project_id>
          <user_expavg_credit>2.2</user_expavg_credit>
          <external_cpid>8edc235ddcecf9c416a5f9417d56f4fd</external_cpid>
        </project>
        )XML",
    }));

    GRC::Cpid cpid_1 = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");
    GRC::Cpid cpid_2 = GRC::Cpid::Parse("8edc235ddcecf9c416a5f9417d56f4fd");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(GRC::Researcher::Get()->Id() == cpid_2);

    const GRC::MiningProjectMap& projects = GRC::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 2);

    if (const GRC::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid_1);
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_url == "https://example.com/1");
        BOOST_CHECK(project1->m_rac == 1.1);
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid_2);
        BOOST_CHECK(project2->m_team == "gridcoin");
        BOOST_CHECK(project2->m_url == "https://example.com/2");
        BOOST_CHECK(project2->m_rac == 2.2);
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_looks_up_loaded_boinc_projects_by_name)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Name</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>1.1</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    GRC::Cpid cpid = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    const GRC::ProjectOption project = GRC::Researcher::Get()->Project("name");

    if (project) {
        BOOST_CHECK(project->m_name == "name");
        BOOST_CHECK(project->m_cpid == cpid);
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_url == "https://example.com/");
        BOOST_CHECK(project->m_rac == 1.1);
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_resets_to_investor_mode_when_parsing_no_projects)
{
    GRC::Researcher::Reload(GRC::MiningProjectMap());

    BOOST_CHECK(GRC::Researcher::Get()->Id() == GRC::MiningId::ForInvestor());
    BOOST_CHECK(GRC::Researcher::Get()->Projects().empty() == true);
}

BOOST_AUTO_TEST_CASE(it_tags_invalid_projects_with_errors_when_parsing_xml)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        // Required team mismatch:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 1</project_name>
          <team_name>Not Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>1.1</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // <team_name> element missing:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 2</project_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>2.2</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // Malformed external CPID:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 3</project_name>
          <team_name>Gridcoin</team_name>
          <user_expavg_credit>3.3</user_expavg_credit>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>invalid</external_cpid>
        </project>
        )XML",
        // <external_cpid> element missing:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 4</project_name>
          <team_name>Gridcoin</team_name>
          <user_expavg_credit>4.4</user_expavg_credit>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
        </project>
        )XML",
        // Mismatched external CPID to internal CPID/email address:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 5</project_name>
          <team_name>Gridcoin</team_name>
          <user_expavg_credit>5.5</user_expavg_credit>
          <cross_project_id>invalid</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // <cross_project_id> element missing:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 6</project_name>
          <team_name>Gridcoin</team_name>
          <user_expavg_credit>6.6</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // Pool CPID:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 7</project_name>
          <team_name>Gridcoin</team_name>
          <user_expavg_credit>7.7</user_expavg_credit>
          <external_cpid>7d0d73fe026d66fd4ab8d5d8da32a611</external_cpid>
        </project>
        )XML",
        // Malformed RAC:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 8</project_name>
          <team_name>Not Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>FOO</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    GRC::Cpid cpid = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    // No valid projects loaded; mining ID should remain INVESTOR:
    BOOST_CHECK(GRC::Researcher::Get()->Id() == GRC::MiningId::ForInvestor());

    const GRC::MiningProjectMap& projects = GRC::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 8);

    if (const GRC::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid);
        BOOST_CHECK(project1->m_team == "not gridcoin");
        BOOST_CHECK(project1->m_rac == 1.1);
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project1->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid);
        BOOST_CHECK(project2->m_team.empty() == true);
        BOOST_CHECK(project2->m_rac == 2.2);
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project2->Eligible() == false);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project3 = projects.Try("project name 3")) {
        BOOST_CHECK(project3->m_name == "project name 3");
        BOOST_CHECK(project3->m_cpid == GRC::Cpid());
        BOOST_CHECK(project3->m_team == "gridcoin");
        BOOST_CHECK(project3->m_rac == 3.3);
        BOOST_CHECK(project3->m_error == GRC::MiningProject::Error::MALFORMED_CPID);
        BOOST_CHECK(project3->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project4 = projects.Try("project name 4")) {
        BOOST_CHECK(project4->m_name == "project name 4");
        BOOST_CHECK(project4->m_cpid == GRC::Cpid());
        BOOST_CHECK(project4->m_team == "gridcoin");
        BOOST_CHECK(project4->m_rac == 4.4);
        BOOST_CHECK(project4->m_error == GRC::MiningProject::Error::MALFORMED_CPID);
        BOOST_CHECK(project4->Eligible() == false);
    } else {
        BOOST_FAIL("Project 4 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project5 = projects.Try("project name 5")) {
        BOOST_CHECK(project5->m_name == "project name 5");
        BOOST_CHECK(project5->m_cpid == cpid);
        BOOST_CHECK(project5->m_team == "gridcoin");
        BOOST_CHECK(project5->m_rac == 5.5);
        BOOST_CHECK(project5->m_error == GRC::MiningProject::Error::MISMATCHED_CPID);
        BOOST_CHECK(project5->Eligible() == false);
    } else {
        BOOST_FAIL("Project 5 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project6 = projects.Try("project name 6")) {
        BOOST_CHECK(project6->m_name == "project name 6");
        BOOST_CHECK(project6->m_cpid == cpid);
        BOOST_CHECK(project6->m_team == "gridcoin");
        BOOST_CHECK(project6->m_rac == 6.6);
        BOOST_CHECK(project6->m_error == GRC::MiningProject::Error::MISMATCHED_CPID);
        BOOST_CHECK(project6->Eligible() == false);
    } else {
        BOOST_FAIL("Project 6 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project7 = projects.Try("project name 7")) {
        BOOST_CHECK(project7->m_name == "project name 7");
        BOOST_CHECK(project7->m_rac == 7.7);
        BOOST_CHECK(project7->m_error == GRC::MiningProject::Error::POOL);
        BOOST_CHECK(project7->Eligible() == false);
    } else {
        BOOST_FAIL("Project 7 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project8 = projects.Try("project name 8")) {
        BOOST_CHECK(project8->m_name == "project name 8");
        BOOST_CHECK(project8->m_cpid == cpid);
        BOOST_CHECK(project8->m_team == "not gridcoin");
        BOOST_CHECK(project8->m_rac == 0.0);
        BOOST_CHECK(project8->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project8->Eligible() == false);
    } else {
        BOOST_FAIL("Project 8 does not exist in the mining project map.");
    }

    // HasRAC should be false.
    BOOST_CHECK(!GRC::Researcher::Get()->HasRAC());

    // Clean up:
    SetArgument("email", "");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_skips_loading_project_xml_with_empty_project_names)
{
    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        // Empty <project_name> element:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name></project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // Missing <project_name> element:
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    // No valid projects loaded; mining ID should remain INVESTOR:
    BOOST_CHECK(GRC::Researcher::Get()->Id() == GRC::MiningId::ForInvestor());
    BOOST_CHECK(GRC::Researcher::Get()->Projects().empty() == true);

    // Clean up:
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_skips_the_team_requirement_when_set_by_protocol)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Simulate a protocol control directive that disables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "false", 1);

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 1</project_name>
          <team_name>! Not Gridcoin !</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <user_expavg_credit>1.1</user_expavg_credit>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    GRC::Cpid cpid = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(GRC::Researcher::Get()->Id() == cpid);

    const GRC::MiningProjectMap& projects = GRC::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 1);

    if (const GRC::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid);
        BOOST_CHECK(project1->m_team == "! not gridcoin !");
        BOOST_CHECK(project1->m_rac == 1.1);
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    // HasRAC should be true.
    BOOST_CHECK(GRC::Researcher::Get()->HasRAC());

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_applies_the_team_whitelist_when_set_by_the_protocol)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Simulate a protocol control directive with whitelisted teams:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "team 1|Team 2", 1);

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 1</project_name>
          <team_name>! Not Gridcoin !</team_name>
          <user_expavg_credit>1.1</user_expavg_credit>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 2</project_name>
          <team_name>Team 1</team_name>
          <user_expavg_credit>0</user_expavg_credit>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Project Name 3</project_name>
          <team_name>Team 2</team_name>
          <user_expavg_credit>0</user_expavg_credit>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    GRC::Cpid cpid = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(GRC::Researcher::Get()->Id() == cpid);

    const GRC::MiningProjectMap& projects = GRC::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 3);

    if (const GRC::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid);
        BOOST_CHECK(project1->m_team == "! not gridcoin !");
        BOOST_CHECK(project1->m_rac == 1.1);
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project1->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid);
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_rac == 0);
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project3 = projects.Try("project name 3")) {
        BOOST_CHECK(project3->m_name == "project name 3");
        BOOST_CHECK(project3->m_cpid == cpid);
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_rac == 0);
        BOOST_CHECK(project3->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project3->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // HasRAC should be false, because the only eligible projects have zero RAC.
    BOOST_CHECK(!GRC::Researcher::Get()->HasRAC());

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "TEAM_WHITELIST");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_applies_the_team_requirement_dynamically)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>name</project_name>
          <team_name>! Not Gridcoin !</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    BOOST_CHECK(GRC::Researcher::Get()->IsInvestor() == true);

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("name")) {
        BOOST_CHECK(project->m_team == "! not gridcoin !");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that disables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "false", 1);

    // Rescan in-memory projects for previously-ineligible teams:
    GRC::Researcher::MarkDirty();
    GRC::Researcher::Refresh();

    BOOST_CHECK(GRC::Researcher::Get()->IsInvestor() == false);

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("name")) {
        BOOST_CHECK(project->m_team == "! not gridcoin !");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "true", 1);

    // Rescan in-memory projects for previously-eligible teams:
    GRC::Researcher::MarkDirty();
    GRC::Researcher::Refresh();

    BOOST_CHECK(GRC::Researcher::Get()->IsInvestor() == true);

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("name")) {
        BOOST_CHECK(project->m_team == "! not gridcoin !");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_applies_the_team_whitelist_dynamically)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>p1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>p2</project_name>
          <team_name>Team 1</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>p3</project_name>
          <team_name>Team 2</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p2")) {
        BOOST_CHECK(project->m_team == "team 1");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p3")) {
        BOOST_CHECK(project->m_team == "team 2");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team whitelist:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "Team 1|Team 2", 1);

    // Rescan in-memory projects for previously-ineligible teams:
    GRC::Researcher::MarkDirty();
    GRC::Researcher::Refresh();

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p2")) {
        BOOST_CHECK(project->m_team == "team 1");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p3")) {
        BOOST_CHECK(project->m_team == "team 2");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that disables the team whitelist:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "", 1);

    // Rescan in-memory projects for previously-eligible teams:
    GRC::Researcher::MarkDirty();
    GRC::Researcher::Refresh();

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p2")) {
        BOOST_CHECK(project->m_team == "team 1");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p3")) {
        BOOST_CHECK(project->m_team == "team 2");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "TEAM_WHITELIST");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_ignores_the_team_whitelist_without_the_team_requirement)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Simulate a protocol control directive that disables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "false", 1);

    AddTestBeacon(GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294"));

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>p1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    BOOST_CHECK(GRC::Researcher::Get()->Eligible() == true);

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team whitelist:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "Team 1|Team 2", 1);

    // Rescan in-memory projects for previously-eligible teams:
    GRC::Researcher::MarkDirty();
    GRC::Researcher::Refresh();

    BOOST_CHECK(GRC::Researcher::Get()->Eligible() == true);

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team requirement
    // (and thus, the whitelist):
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "true", 1);

    // Rescan in-memory projects for previously-eligible teams:
    GRC::Researcher::MarkDirty();
    GRC::Researcher::Refresh();

    BOOST_CHECK(GRC::Researcher::Get()->Eligible() == false);

    if (const GRC::ProjectOption project = GRC::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == GRC::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP");
    DeleteCache(Section::PROTOCOL, "TEAM_WHITELIST");
    RemoveTestBeacon(GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294"));
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

// Note: the precondition skips this test case when the test harness cannot
// resolve the client_state.xml stub.
void it_parses_project_xml_from_a_client_state_xml_file()
{
    // This test case reads a client_state.xml stub file in test/data/ and loads
    // the projects and CPIDs into the global researcher context.

    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Set the directory from which the wallet will open the client_state.xml.
    // We point it at our test stub:
    SetArgument("boincdatadir", ResolveStubDir().string() + "/");

    // Read the stub and load projects and CPIDs into the researcher context:
    GRC::Researcher::Reload();

    GRC::Cpid cpid_1 = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");
    GRC::Cpid cpid_2 = GRC::Cpid::Parse("8edc235ddcecf9c416a5f9417d56f4fd");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(GRC::Researcher::Get()->Id() == cpid_2);

    const GRC::MiningProjectMap& projects = GRC::Researcher::Get()->Projects();

    // The stub contains 4 projects. One invalid project omits the project name
    // so the wallet skips it:
    BOOST_CHECK(projects.size() == 3);

    if (const GRC::ProjectOption project1 = projects.Try("valid project 1")) {
        BOOST_CHECK(project1->m_name == "valid project 1");
        BOOST_CHECK(project1->m_cpid == cpid_1);
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_rac == 1.1);
        BOOST_CHECK(project1->m_url == "https://project1.example.com/boinc/");
        BOOST_CHECK(project1->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const GRC::ProjectOption project2 = projects.Try("valid project 2")) {
        BOOST_CHECK(project2->m_name == "valid project 2");
        BOOST_CHECK(project2->m_cpid == cpid_2);
        BOOST_CHECK(project2->m_team == "gridcoin");
        BOOST_CHECK(project2->m_rac == 2.2);
        BOOST_CHECK(project2->m_url == "https://project2.example.com/boinc/");
        BOOST_CHECK(project2->m_error == GRC::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    // The client_state.xml stub contains an invalid project for this test:
    if (const GRC::ProjectOption project3 = projects.Try("invalid project 3")) {
        BOOST_CHECK(project3->m_name == "invalid project 3");
        BOOST_CHECK(project3->m_cpid == cpid_2);
        BOOST_CHECK(project3->m_team == "gridcoin");
        BOOST_CHECK(project3->m_rac == 3.3);
        BOOST_CHECK(project3->m_url == "https://project3.example.com/boinc/");
        BOOST_CHECK(project3->m_error == GRC::MiningProject::Error::MISMATCHED_CPID);
        BOOST_CHECK(project3->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // HasRAC should be true.
    BOOST_CHECK(GRC::Researcher::Get()->HasRAC());

    // Clean up:
    SetArgument("email", "");
    SetArgument("boincdatadir", "");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

// Note: the precondition skips this test case when the test harness cannot
// resolve the client_state.xml stub.
void it_resets_to_investor_mode_when_explicitly_configured()
{
    SetArgument("investor", "1");

    // For a valid test, set the email address because it will also pass falsely
    // if this is absent:
    SetArgument("email", "researcher@example.com");

    // For a valid test, ensure that we can access the client_state.xml stub
    // because it will also pass falsely if this is unset:
    SetArgument("boincdatadir", ResolveStubDir().string() + "/");

    GRC::Researcher::Reload();

    BOOST_CHECK(GRC::Researcher::Get()->Id() == GRC::MiningId::ForInvestor());
    BOOST_CHECK(GRC::Researcher::Get()->Projects().empty() == true);

    // Clean up:
    SetArgument("investor", "0");
    SetArgument("email", "");
    SetArgument("boincdatadir", "");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

//!
//! \brief Precondition that skips tests that read a client_state.xml stub from
//! disk if it does not exist.
//!
BOOST_AUTO_TEST_CASE(client_state_stub_exists)
{
    if (fs::exists(ResolveStubDir() / "client_state.xml")) {
        boost::unit_test::framework::master_test_suite().add(BOOST_TEST_CASE(&it_parses_project_xml_from_a_client_state_xml_file));
        boost::unit_test::framework::master_test_suite().add(BOOST_TEST_CASE(&it_resets_to_investor_mode_when_explicitly_configured));
    } else {
        BOOST_TEST_MESSAGE("client_state.xml test stub not found");
    }
}

BOOST_AUTO_TEST_CASE(it_resets_to_investor_when_it_only_finds_pool_projects)
{
    const GRC::Cpid cpid = GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");
    SetArgument("email", "researcher@example.com");
    AddTestBeacon(cpid);

    // External CPID is a pool CPID:
    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Pool Project</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>7d0d73fe026d66fd4ab8d5d8da32a611</external_cpid>
        </project>
        )XML",
    }));

    BOOST_CHECK(GRC::Researcher::Get()->Id() == GRC::MiningId::ForInvestor());
    BOOST_CHECK(GRC::Researcher::Get()->Eligible() == false);
    BOOST_CHECK(GRC::Researcher::Get()->Status() == GRC::ResearcherStatus::POOL);

    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>My Project</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>7d0d73fe026d66fd4ab8d5d8da32a611</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Pool Project</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    BOOST_CHECK(GRC::Researcher::Get()->Id() == cpid);
    BOOST_CHECK(GRC::Researcher::Get()->Eligible() == true);
    BOOST_CHECK(GRC::Researcher::Get()->Status() != GRC::ResearcherStatus::POOL);

    // Clean up:
    SetArgument("email", "");
    RemoveTestBeacon(cpid);
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_allows_pool_operators_to_load_pool_cpids)
{
    SetArgument("pooloperator", "1");

    // External CPID is a pool CPID:
    GRC::Researcher::Reload(GRC::MiningProjectMap::Parse({
        R"XML(
        <project>
          <master_url>https://example.com/</master_url>
          <project_name>Name</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>7d0d73fe026d66fd4ab8d5d8da32a611</external_cpid>
        </project>
        )XML",
    }));

    // We can't completely test that a pool CPID loads, but we can check that
    // the it didn't fail because of the pool CPID:
    //
    BOOST_CHECK(GRC::Researcher::Get()->Status() != GRC::ResearcherStatus::POOL);

    // Clean up:
    SetArgument("pooloperator", "0");
    GRC::Researcher::Reload(GRC::MiningProjectMap());
}

BOOST_AUTO_TEST_SUITE_END()
