#include "neuralnet/project.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(project_tests)

BOOST_AUTO_TEST_CASE(project_AddProjectsToWhitelistShouldBePossible)
{
    NN::Whitelist whitelist;
    whitelist.Add("Enigma", "http://enigma.test", 1234567);

    auto snapshot = whitelist.Snapshot();
    BOOST_CHECK(snapshot.Contains("Enigma") == true);
}

BOOST_AUTO_TEST_CASE(project_EmptySnapshotsShouldNotBePopulated)
{
    NN::Whitelist whitelist;
    BOOST_CHECK(whitelist.Snapshot().Populated() == false);
}

BOOST_AUTO_TEST_CASE(project_PopulatedSnapshotsShouldBePopulated)
{
    NN::Whitelist whitelist;
    whitelist.Add("Enigma", "http://enigma.test", 1234567);
    BOOST_CHECK(whitelist.Snapshot().Populated() == true);
}

BOOST_AUTO_TEST_CASE(project_DeletingProjectsShouldBePossible)
{
    NN::Whitelist whitelist;
    whitelist.Add("Enigma", "http://enigma.test", 1234567);
    whitelist.Delete("Enigma");

    auto snapshot = whitelist.Snapshot();
    BOOST_CHECK(snapshot.Contains("Enigma") == false);
}

BOOST_AUTO_TEST_CASE(project_SnapshotsShouldContainDeletedProjects)
{
    NN::Whitelist whitelist;
    whitelist.Add("Enigma", "http://enigma.test", 1234567);

    auto snapshot = whitelist.Snapshot();
    whitelist.Delete("Enigma");
    BOOST_CHECK(snapshot.Contains("Enigma") == true);
}

BOOST_AUTO_TEST_CASE(project_ReAddingAProjectShouldOverwriteIt)
{
    NN::Whitelist whitelist;
    whitelist.Add("Enigma", "http://enigma.test", 1234567);
    whitelist.Add("Enigma", "http://new.enigma.test", 1234567);

    auto snapshot = whitelist.Snapshot();
    BOOST_CHECK(snapshot.size() == 1);
}

BOOST_AUTO_TEST_SUITE_END()
