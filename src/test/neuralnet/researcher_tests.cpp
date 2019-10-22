#include "appcache.h"
#include "neuralnet/researcher.h"
#include "util.h"

#include <boost/filesystem.hpp>
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
boost::filesystem::path ResolveStubDir()
{
    const boost::filesystem::path cwd = boost::filesystem::current_path();
    const boost::filesystem::path data_dir("src/test/data");
    const std::string stub = "client_state.xml";

    // Test harness run from subdirectory of repo root (src/, build/, etc.):
    if (boost::filesystem::exists(cwd / ".." / data_dir / stub)) {
        return boost::filesystem::canonical(cwd / ".." / data_dir);
    }

    // Test harness run from platform-specific build sub-directory
    // (ex: build/gridcoin-x86_64-unknown-linux-gnu/)
    if (boost::filesystem::exists(cwd / ".." / ".." / data_dir / stub)) {
        return boost::filesystem::canonical(cwd / ".." / ".." / data_dir);
    }

    // Test harness run from sub-directory in a platform-specific build sub-
    // directory (Travis; ex: build/gridcoin-x86_64-unknown-linux-gnu/src)
    if (boost::filesystem::exists(cwd / ".." / ".." / ".." / data_dir / stub)) {
        return boost::filesystem::canonical(cwd / ".." / ".." / ".." / data_dir);
    }

    // Test harness run from test directory (src/test/):
    if (boost::filesystem::exists(cwd / "data" / stub)) {
        return cwd / "data";
    }

    // Test harness run from repo root:
    return cwd / data_dir;
}

//!
//! \brief Precondition that skips tests that read a client_state.xml stub from
//! disk if it does not exist.
//!
boost::test_tools::assertion_result ClientStateStubExists(boost::unit_test::test_unit_id)
{
    if (boost::filesystem::exists(ResolveStubDir() / "client_state.xml")) {
        return true;
    }

    boost::test_tools::assertion_result result(false);
    result.message() << "client_state.xml test stub not found";

    return result;
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// MiningProject
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(MiningProject)

BOOST_AUTO_TEST_CASE(it_initializes_with_project_data)
{
    NN::Cpid expected(std::vector<unsigned char> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    NN::MiningProject project("project name", expected, "team name");

    BOOST_CHECK(project.m_name == "project name");
    BOOST_CHECK(project.m_cpid == expected);
    BOOST_CHECK(project.m_team == "team name");
    BOOST_CHECK(project.m_error == NN::MiningProject::Error::NONE);
}

BOOST_AUTO_TEST_CASE(it_parses_a_project_xml_string)
{
    SetArgument("email", "researcher@example.com");

    // The XML string contains a subset of data found within a <project> element
    // from BOINC's client_state.xml file:
    //
    NN::MiningProject project = NN::MiningProject::Parse(
        R"XML(
        <project>
          <project_name>Project Name</project_name>
          <team_name>Team Name</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML");

    NN::Cpid cpid = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    BOOST_CHECK(project.m_name == "project name");
    BOOST_CHECK(project.m_cpid == cpid);
    BOOST_CHECK(project.m_team == "team name");
    BOOST_CHECK(project.m_error == NN::MiningProject::Error::NONE);

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
    NN::MiningProject project = NN::MiningProject::Parse(
        R"XML(
        <project>
          <project_name>Project Name</project_name>
          <team_name>Team Name</team_name>
          <email_hash>b8b58cce3c90c7dc9f3601674202b21c</email_hash>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid></external_cpid>
        </project>
        )XML");

    NN::Cpid cpid = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    BOOST_CHECK(project.m_name == "project name");
    BOOST_CHECK(project.m_cpid == cpid);
    BOOST_CHECK(project.m_team == "team name");
    BOOST_CHECK(project.m_error == NN::MiningProject::Error::NONE);

    // Clean up:
    SetArgument("email", "");
}

BOOST_AUTO_TEST_CASE(it_normalizes_project_names)
{
    NN::MiningProject project("Project_NAME", NN::Cpid(), "team name");

    BOOST_CHECK(project.m_name == "project name");
}

BOOST_AUTO_TEST_CASE(it_converts_team_names_to_lowercase)
{
    NN::MiningProject project("project name", NN::Cpid(), "TEAM NAME");

    BOOST_CHECK(project.m_team == "team name");
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_project_is_eligible)
{
    // Eligibility is determined by the absense of an error set while loading
    // the project from BOINC's client_state.xml file.

    NN::MiningProject project("project name", NN::Cpid(), "team name");

    BOOST_CHECK(project.Eligible() == true);

    project.m_error = NN::MiningProject::Error::INVALID_TEAM;

    BOOST_CHECK(project.Eligible() == false);
}

BOOST_AUTO_TEST_CASE(it_formats_error_messages_for_display)
{
    NN::MiningProject project("project name", NN::Cpid(), "team name");

    BOOST_CHECK(project.ErrorMessage().empty() == true);

    project.m_error = NN::MiningProject::Error::INVALID_TEAM;

    BOOST_CHECK(project.ErrorMessage().empty() == false);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// MiningProjectMap
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(MiningProjectMap)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_collection)
{
    NN::MiningProjectMap projects;

    BOOST_CHECK(projects.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_is_iterable)
{
    NN::MiningProjectMap projects;

    projects.Set(NN::MiningProject("project name 1", NN::Cpid(), "team name"));
    projects.Set(NN::MiningProject("project name 2", NN::Cpid(), "team name"));

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

    NN::MiningProjectMap projects = NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>Project Name 1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <project_name>Project Name 2</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY</cross_project_id>
          <external_cpid>8edc235ddcecf9c416a5f9417d56f4fd</external_cpid>
        </project>
        )XML",
    });

    NN::Cpid cpid_1 = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");
    NN::Cpid cpid_2 = NN::Cpid::Parse("8edc235ddcecf9c416a5f9417d56f4fd");

    BOOST_CHECK(projects.size() == 2);

    if (const NN::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid_1);
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid_2);
        BOOST_CHECK(project2->m_team == "gridcoin");
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_skips_loading_project_xml_with_empty_project_names)
{
    NN::MiningProjectMap projects = NN::MiningProjectMap::Parse({
        // Empty <project_name> element:
        R"XML(
        <project>
          <project_name></project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // Missing <project_name> element:
        R"XML(
        <project>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    });

    // No valid projects loaded; mining ID should remain INVESTOR:
    BOOST_CHECK(projects.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_counts_the_number_of_projects)
{
    NN::MiningProjectMap projects;

    BOOST_CHECK(projects.size() == 0);

    projects.Set(NN::MiningProject("project name 1", NN::Cpid(), "team name"));
    projects.Set(NN::MiningProject("project name 2", NN::Cpid(), "team name"));

    BOOST_CHECK(projects.size() == 2);
}

BOOST_AUTO_TEST_CASE(it_indicates_whether_it_contains_any_projects)
{
    NN::MiningProjectMap projects;

    BOOST_CHECK(projects.empty() == true);

    projects.Set(NN::MiningProject("project name", NN::Cpid(), "team name"));

    BOOST_CHECK(projects.empty() == false);
}

BOOST_AUTO_TEST_CASE(it_fetches_a_project_by_name)
{
    NN::MiningProjectMap projects;

    projects.Set(NN::MiningProject("project name", NN::Cpid(), "team name"));

    BOOST_CHECK(projects.Try("project name").value().m_name == "project name");
    BOOST_CHECK(projects.Try("nonexistent") == boost::none);
}

BOOST_AUTO_TEST_CASE(it_does_not_overwrite_projects_with_the_same_name)
{
    NN::MiningProjectMap projects;

    projects.Set(NN::MiningProject("project name", NN::Cpid(), "team name 1"));
    projects.Set(NN::MiningProject("project name", NN::Cpid(), "team name 2"));

    BOOST_CHECK(projects.Try("project name").value().m_team == "team name 1");
}

BOOST_AUTO_TEST_CASE(it_applies_a_provided_team_whitelist)
{
    NN::MiningProjectMap projects;

    projects.Set(NN::MiningProject("project 1", NN::Cpid(), "gridcoin"));
    projects.Set(NN::MiningProject("project 2", NN::Cpid(), "team 1"));
    projects.Set(NN::MiningProject("project 3", NN::Cpid(), "team 2"));

    // Before applying a whitelist, all projects are eligible:

    if (const NN::ProjectOption project1 = projects.Try("project 1")) {
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("project 2")) {
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project3 = projects.Try("project 3")) {
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project3->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Applying this whitelist disables eligibility for project 1:
    //
    projects.ApplyTeamWhitelist({ "team 1", "team 2" });

    if (const NN::ProjectOption project1 = projects.Try("project 1")) {
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project1->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("project 2")) {
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project3 = projects.Try("project 3")) {
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project3->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Applying an empty whitelist removes the team requirement:
    //
    projects.ApplyTeamWhitelist({ });

    if (const NN::ProjectOption project1 = projects.Try("project 1")) {
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("project 2")) {
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project3 = projects.Try("project 3")) {
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_error == NN::MiningProject::Error::NONE);
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
    NN::Researcher researcher;

    BOOST_CHECK(researcher.Id().Which() == NN::MiningId::Kind::INVESTOR);
    BOOST_CHECK(researcher.Projects().empty() == true);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_researcher_context_data)
{
    NN::MiningProjectMap expected;

    expected.Set(NN::MiningProject("project name", NN::Cpid(), "team name"));

    NN::Researcher researcher(NN::Cpid(), expected);

    BOOST_CHECK(researcher.Id().Which() == NN::MiningId::Kind::CPID);
    BOOST_CHECK(researcher.Projects().size() == 1);
}

BOOST_AUTO_TEST_CASE(it_converts_a_configured_email_address_to_lowercase)
{
    SetArgument("email", "RESEARCHER@EXAMPLE.COM");

    BOOST_CHECK(NN::Researcher::Email() == "researcher@example.com");

    // Clean up:
    SetArgument("email", "");
}

BOOST_AUTO_TEST_CASE(it_provides_access_to_a_global_researcher_singleton)
{
    NN::ResearcherPtr researcher(NN::Researcher::Get());

    BOOST_CHECK(researcher->Id() == NN::MiningId::ForInvestor());
}

BOOST_AUTO_TEST_CASE(it_determines_whether_a_wallet_is_eligible_for_rewards)
{
    NN::Researcher researcher;

    BOOST_CHECK(researcher.Eligible() == false);
    BOOST_CHECK(researcher.IsInvestor() == true);

    // A zero-value CPID is technically a valid CPID:
    researcher = NN::Researcher(NN::Cpid(), NN::MiningProjectMap());

    BOOST_CHECK(researcher.Eligible() == true);
    BOOST_CHECK(researcher.IsInvestor() == false);
}

BOOST_AUTO_TEST_CASE(it_provides_an_overall_status_of_the_reseracher_context)
{
    NN::Researcher researcher;

    BOOST_CHECK(researcher.Status() == NN::ResearcherStatus::INVESTOR);

    NN::MiningProjectMap projects;
    projects.Set(NN::MiningProject("ineligible", NN::Cpid(), "team name"));

    researcher = NN::Researcher(NN::MiningId::ForInvestor(), projects);

    // Has projects but none eligible (investor):
    BOOST_CHECK(researcher.Status() == NN::ResearcherStatus::NO_PROJECTS);

    researcher = NN::Researcher(NN::Cpid(), std::move(projects));

    BOOST_CHECK(researcher.Status() == NN::ResearcherStatus::ACTIVE);
}

BOOST_AUTO_TEST_CASE(it_parses_project_xml_to_a_global_researcher_singleton)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>Project Name 1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <project_name>Project Name 2</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY</cross_project_id>
          <external_cpid>8edc235ddcecf9c416a5f9417d56f4fd</external_cpid>
        </project>
        )XML",
    }));

    NN::Cpid cpid_1 = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");
    NN::Cpid cpid_2 = NN::Cpid::Parse("8edc235ddcecf9c416a5f9417d56f4fd");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(NN::Researcher::Get()->Id() == cpid_2);

    const NN::MiningProjectMap& projects = NN::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 2);

    if (const NN::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid_1);
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid_2);
        BOOST_CHECK(project2->m_team == "gridcoin");
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_looks_up_loaded_boinc_projects_by_name)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>Name</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    NN::Cpid cpid = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    const NN::ProjectOption project = NN::Researcher::Get()->Project("name");

    if (project) {
        BOOST_CHECK(project->m_name == "name");
        BOOST_CHECK(project->m_cpid == cpid);
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_resets_to_investor_mode_when_parsing_no_projects)
{
    NN::Researcher::Reload(NN::MiningProjectMap());

    BOOST_CHECK(NN::Researcher::Get()->Id() == NN::MiningId::ForInvestor());
    BOOST_CHECK(NN::Researcher::Get()->Projects().empty() == true);
}

BOOST_AUTO_TEST_CASE(it_tags_invalid_projects_with_errors_when_parsing_xml)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        // Required team mismatch:
        R"XML(
        <project>
          <project_name>Project Name 1</project_name>
          <team_name>Not Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // <team_name> element missing:
        R"XML(
        <project>
          <project_name>Project Name 2</project_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // Malformed external CPID:
        R"XML(
        <project>
          <project_name>Project Name 3</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>invalid</external_cpid>
        </project>
        )XML",
        // <external_cpid> element missing:
        R"XML(
        <project>
          <project_name>Project Name 4</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
        </project>
        )XML",
        // Mismatched external CPID to internal CPID/email address:
        R"XML(
        <project>
          <project_name>Project Name 5</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>invalid</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // <cross_project_id> element missing:
        R"XML(
        <project>
          <project_name>Project Name 6</project_name>
          <team_name>Gridcoin</team_name>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    NN::Cpid cpid = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    // No valid projects loaded; mining ID should remain INVESTOR:
    BOOST_CHECK(NN::Researcher::Get()->Id() == NN::MiningId::ForInvestor());

    const NN::MiningProjectMap& projects = NN::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 6);

    if (const NN::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid);
        BOOST_CHECK(project1->m_team == "not gridcoin");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project1->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid);
        BOOST_CHECK(project2->m_team.empty() == true);
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project2->Eligible() == false);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project3 = projects.Try("project name 3")) {
        BOOST_CHECK(project3->m_name == "project name 3");
        BOOST_CHECK(project3->m_cpid == NN::Cpid());
        BOOST_CHECK(project3->m_team == "gridcoin");
        BOOST_CHECK(project3->m_error == NN::MiningProject::Error::MALFORMED_CPID);
        BOOST_CHECK(project3->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project4 = projects.Try("project name 4")) {
        BOOST_CHECK(project4->m_name == "project name 4");
        BOOST_CHECK(project4->m_cpid == NN::Cpid());
        BOOST_CHECK(project4->m_team == "gridcoin");
        BOOST_CHECK(project4->m_error == NN::MiningProject::Error::MALFORMED_CPID);
        BOOST_CHECK(project4->Eligible() == false);
    } else {
        BOOST_FAIL("Project 4 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project5 = projects.Try("project name 5")) {
        BOOST_CHECK(project5->m_name == "project name 5");
        BOOST_CHECK(project5->m_cpid == cpid);
        BOOST_CHECK(project5->m_team == "gridcoin");
        BOOST_CHECK(project5->m_error == NN::MiningProject::Error::MISMATCHED_CPID);
        BOOST_CHECK(project5->Eligible() == false);
    } else {
        BOOST_FAIL("Project 5 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project6 = projects.Try("project name 6")) {
        BOOST_CHECK(project6->m_name == "project name 6");
        BOOST_CHECK(project6->m_cpid == cpid);
        BOOST_CHECK(project6->m_team == "gridcoin");
        BOOST_CHECK(project6->m_error == NN::MiningProject::Error::MISMATCHED_CPID);
        BOOST_CHECK(project6->Eligible() == false);
    } else {
        BOOST_FAIL("Project 6 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_skips_loading_project_xml_with_empty_project_names)
{
    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        // Empty <project_name> element:
        R"XML(
        <project>
          <project_name></project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        // Missing <project_name> element:
        R"XML(
        <project>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    // No valid projects loaded; mining ID should remain INVESTOR:
    BOOST_CHECK(NN::Researcher::Get()->Id() == NN::MiningId::ForInvestor());
    BOOST_CHECK(NN::Researcher::Get()->Projects().empty() == true);

    // Clean up:
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_skips_the_team_requirement_when_set_by_protocol)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Simulate a protocol control directive that disables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "false", 1);

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>Project Name 1</project_name>
          <team_name>! Not Gridcoin !</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    NN::Cpid cpid = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(NN::Researcher::Get()->Id() == cpid);

    const NN::MiningProjectMap& projects = NN::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 1);

    if (const NN::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid);
        BOOST_CHECK(project1->m_team == "! not gridcoin !");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_applies_the_team_whitelist_when_set_by_the_protocol)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Simulate a protocol control directive with whitelisted teams:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "team 1|Team 2", 1);

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>Project Name 1</project_name>
          <team_name>! Not Gridcoin !</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <project_name>Project Name 2</project_name>
          <team_name>Team 1</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <project_name>Project Name 3</project_name>
          <team_name>Team 2</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    NN::Cpid cpid = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(NN::Researcher::Get()->Id() == cpid);

    const NN::MiningProjectMap& projects = NN::Researcher::Get()->Projects();
    BOOST_CHECK(projects.size() == 3);

    if (const NN::ProjectOption project1 = projects.Try("project name 1")) {
        BOOST_CHECK(project1->m_name == "project name 1");
        BOOST_CHECK(project1->m_cpid == cpid);
        BOOST_CHECK(project1->m_team == "! not gridcoin !");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project1->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("project name 2")) {
        BOOST_CHECK(project2->m_name == "project name 2");
        BOOST_CHECK(project2->m_cpid == cpid);
        BOOST_CHECK(project2->m_team == "team 1");
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project3 = projects.Try("project name 3")) {
        BOOST_CHECK(project3->m_name == "project name 3");
        BOOST_CHECK(project3->m_cpid == cpid);
        BOOST_CHECK(project3->m_team == "team 2");
        BOOST_CHECK(project3->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project3->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "TEAM_WHITELIST");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_applies_the_team_requirement_dynamically)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>name</project_name>
          <team_name>! Not Gridcoin !</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    BOOST_CHECK(NN::Researcher::Get()->IsInvestor() == true);

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("name")) {
        BOOST_CHECK(project->m_team == "! not gridcoin !");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that disables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "false", 1);

    // Rescan in-memory projects for previously-ineligible teams:
    NN::Researcher::Refresh();

    BOOST_CHECK(NN::Researcher::Get()->IsInvestor() == false);

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("name")) {
        BOOST_CHECK(project->m_team == "! not gridcoin !");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "true", 1);

    // Rescan in-memory projects for previously-eligible teams:
    NN::Researcher::Refresh();

    BOOST_CHECK(NN::Researcher::Get()->IsInvestor() == true);

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("name")) {
        BOOST_CHECK(project->m_team == "! not gridcoin !");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_applies_the_team_whitelist_dynamically)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>p1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <project_name>p2</project_name>
          <team_name>Team 1</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
        R"XML(
        <project>
          <project_name>p3</project_name>
          <team_name>Team 2</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p2")) {
        BOOST_CHECK(project->m_team == "team 1");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p3")) {
        BOOST_CHECK(project->m_team == "team 2");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team whitelist:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "Team 1|Team 2", 1);

    // Rescan in-memory projects for previously-ineligible teams:
    NN::Researcher::Refresh();

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p2")) {
        BOOST_CHECK(project->m_team == "team 1");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p3")) {
        BOOST_CHECK(project->m_team == "team 2");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that disables the team whitelist:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "", 1);

    // Rescan in-memory projects for previously-eligible teams:
    NN::Researcher::Refresh();

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p2")) {
        BOOST_CHECK(project->m_team == "team 1");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p3")) {
        BOOST_CHECK(project->m_team == "team 2");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "TEAM_WHITELIST");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_CASE(it_ignores_the_team_whitelist_without_the_team_requirement)
{
    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Simulate a protocol control directive that disables the team requirement:
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "false", 1);

    NN::Researcher::Reload(NN::MiningProjectMap::Parse({
        R"XML(
        <project>
          <project_name>p1</project_name>
          <team_name>Gridcoin</team_name>
          <cross_project_id>XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX</cross_project_id>
          <external_cpid>f5d8234352e5a5ae3915debba7258294</external_cpid>
        </project>
        )XML",
    }));

    BOOST_CHECK(NN::Researcher::Get()->Eligible() == true);

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team whitelist:
    WriteCache(Section::PROTOCOL, "TEAM_WHITELIST", "Team 1|Team 2", 1);

    // Rescan in-memory projects for previously-eligible teams:
    NN::Researcher::Refresh();

    BOOST_CHECK(NN::Researcher::Get()->Eligible() == true);

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project->Eligible() == true);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Simulate a protocol control directive that enables the team requirement
    // (and thus, the whitelist):
    WriteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP", "true", 1);

    // Rescan in-memory projects for previously-eligible teams:
    NN::Researcher::Refresh();

    BOOST_CHECK(NN::Researcher::Get()->Eligible() == false);

    if (const NN::ProjectOption project = NN::Researcher::Get()->Project("p1")) {
        BOOST_CHECK(project->m_team == "gridcoin");
        BOOST_CHECK(project->m_error == NN::MiningProject::Error::INVALID_TEAM);
        BOOST_CHECK(project->Eligible() == false);
    } else {
        BOOST_FAIL("Project does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    DeleteCache(Section::PROTOCOL, "REQUIRE_TEAM_WHITELIST_MEMBERSHIP");
    DeleteCache(Section::PROTOCOL, "TEAM_WHITELIST");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

// Note: the precondition skips this test case when the test harness cannot
// resolve the client_state.xml stub. Autotools' "make check" target counts
// this skip as a failure, so the test harness must be run from a supported
// path within the source tree.
//
BOOST_AUTO_TEST_CASE(it_parses_project_xml_from_a_client_state_xml_file,
    *boost::unit_test::precondition(ClientStateStubExists))
{
    // This test case reads a client_state.xml stub file in test/data/ and loads
    // the projects and CPIDs into the global researcher context.

    // External CPIDs generated with this email address:
    SetArgument("email", "researcher@example.com");

    // Set the directory from which the wallet will open the client_state.xml.
    // We point it at our test stub:
    SetArgument("boincdatadir", ResolveStubDir().string() + "/");

    // Read the stub and load projects and CPIDs into the researcher context:
    NN::Researcher::Reload();

    NN::Cpid cpid_1 = NN::Cpid::Parse("f5d8234352e5a5ae3915debba7258294");
    NN::Cpid cpid_2 = NN::Cpid::Parse("8edc235ddcecf9c416a5f9417d56f4fd");

    // Primary CPID is selected from the last valid CPID loaded:
    BOOST_CHECK(NN::Researcher::Get()->Id() == cpid_2);

    const NN::MiningProjectMap& projects = NN::Researcher::Get()->Projects();

    // The stub contains 4 projects. One invalid project omits the project name
    // so the wallet skips it:
    BOOST_CHECK(projects.size() == 3);

    if (const NN::ProjectOption project1 = projects.Try("valid project 1")) {
        BOOST_CHECK(project1->m_name == "valid project 1");
        BOOST_CHECK(project1->m_cpid == cpid_1);
        BOOST_CHECK(project1->m_team == "gridcoin");
        BOOST_CHECK(project1->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project1->Eligible() == true);
    } else {
        BOOST_FAIL("Project 1 does not exist in the mining project map.");
    }

    if (const NN::ProjectOption project2 = projects.Try("valid project 2")) {
        BOOST_CHECK(project2->m_name == "valid project 2");
        BOOST_CHECK(project2->m_cpid == cpid_2);
        BOOST_CHECK(project2->m_team == "gridcoin");
        BOOST_CHECK(project2->m_error == NN::MiningProject::Error::NONE);
        BOOST_CHECK(project2->Eligible() == true);
    } else {
        BOOST_FAIL("Project 2 does not exist in the mining project map.");
    }

    // The client_state.xml stub contains an invalid project for this test:
    if (const NN::ProjectOption project3 = projects.Try("invalid project 3")) {
        BOOST_CHECK(project3->m_name == "invalid project 3");
        BOOST_CHECK(project3->m_cpid == cpid_2);
        BOOST_CHECK(project3->m_team == "gridcoin");
        BOOST_CHECK(project3->m_error == NN::MiningProject::Error::MISMATCHED_CPID);
        BOOST_CHECK(project3->Eligible() == false);
    } else {
        BOOST_FAIL("Project 3 does not exist in the mining project map.");
    }

    // Clean up:
    SetArgument("email", "");
    SetArgument("boincdatadir", "");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

// Note: the precondition skips this test case when the test harness cannot
// resolve the client_state.xml stub. Autotools' "make check" target counts
// this skip as a failure, so the test harness must be run from a supported
// path within the source tree.
//
BOOST_AUTO_TEST_CASE(it_resets_to_investor_mode_when_missing_email_address,
    *boost::unit_test::precondition(ClientStateStubExists))
{
    // We don't SetArgument("email", "...") for this test.

    // For a valid test, ensure that we can access the client_state.xml stub
    // because it will also pass falsely if this is unset:
    SetArgument("boincdatadir", ResolveStubDir().string() + "/");

    NN::Researcher::Reload();

    BOOST_CHECK(NN::Researcher::Get()->Id() == NN::MiningId::ForInvestor());
    BOOST_CHECK(NN::Researcher::Get()->Projects().empty() == true);

    // Clean up:
    SetArgument("boincdatadir", "");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

// Note: the precondition skips this test case when the test harness cannot
// resolve the client_state.xml stub. Autotools' "make check" target counts
// this skip as a failure, so the test harness must be run from a supported
// path within the source tree.
//
BOOST_AUTO_TEST_CASE(it_resets_to_investor_mode_when_explicitly_configured,
    *boost::unit_test::precondition(ClientStateStubExists))
{
    SetArgument("investor", "1");

    // For a valid test, set the email address because it will also pass falsely
    // if this is absent:
    SetArgument("email", "researcher@example.com");

    // For a valid test, ensure that we can access the client_state.xml stub
    // because it will also pass falsely if this is unset:
    SetArgument("boincdatadir", ResolveStubDir().string() + "/");

    NN::Researcher::Reload();

    BOOST_CHECK(NN::Researcher::Get()->Id() == NN::MiningId::ForInvestor());
    BOOST_CHECK(NN::Researcher::Get()->Projects().empty() == true);

    // Clean up:
    SetArgument("investor", "0");
    SetArgument("email", "");
    SetArgument("boincdatadir", "");
    NN::Researcher::Reload(NN::MiningProjectMap());
}

BOOST_AUTO_TEST_SUITE_END()
