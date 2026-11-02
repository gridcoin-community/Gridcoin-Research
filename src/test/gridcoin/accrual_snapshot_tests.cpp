// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparamsbase.h"
#include "crypto/common.h"
#include "fs.h"
#include "gridcoin/account.h"
#include "gridcoin/accrual/snapshot.h"
#include "gridcoin/cpid.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"
#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdio>

namespace {

//!
//! \brief Point the data directory at a private temporary root for one test
//! case, so the accrual snapshot directory is the case's own.
//!
struct SnapshotDirSetup
{
    //! Removes the root last, after the data directory argument is restored.
    struct RootRemover
    {
        fs::path m_root;

        ~RootRemover()
        {
            // Never throw from a destructor during unwind.
            boost::system::error_code ec;
            fs::remove_all(m_root, ec);
        }
    } m_remover{fs::temp_directory_path() / fs::unique_path("accrual_snapshot_tests_%%%%-%%%%")};

    //! Clears the path cache once the argument below is restored.
    struct PathCacheClearer
    {
        ~PathCacheClearer() { gArgs.ClearPathCache(); }
    } m_cache_clearer;

    grc_test::ForcedArgGuard m_datadir;

    SnapshotDirSetup()
        : m_datadir("datadir", PrepareRoot(m_remover.m_root))
    {
        gArgs.ClearPathCache();
        BOOST_REQUIRE_MESSAGE(!GetDataDir().empty(), "datadir resolves empty");
    }

    static std::string PrepareRoot(const fs::path& root)
    {
        // Create the network subdirectory too: GetDataDir() would otherwise
        // create it with an owner-only DACL, which fails under Wine.
        fs::create_directories(root / BaseParams().DataDir());

        return root.string();
    }
};

fs::path RegistryPath()
{
    return GRC::SnapshotDirectory() / "registry.dat";
}

GRC::ResearchAccountMap OneAccount()
{
    GRC::ResearchAccountMap accounts;
    accounts.emplace(GRC::Cpid(InsecureRandBytes(16)), GRC::ResearchAccount(12345));

    return accounts;
}

//! Store a baseline snapshot at 100 and snapshots at 200 and 300.
void StoreThree(GRC::AccrualSnapshotRepository& repo, const GRC::ResearchAccountMap& accounts)
{
    BOOST_REQUIRE(repo.Initialize());
    BOOST_REQUIRE(repo.StoreBaseline(100, accounts));
    BOOST_REQUIRE(repo.Store(200, accounts));
    BOOST_REQUIRE(repo.Store(300, accounts));
}

void StoreThreeAndClose()
{
    GRC::AccrualSnapshotRepository repo;
    StoreThree(repo, OneAccount());
    BOOST_REQUIRE(repo.CloseRegistryFile());
}

//! The registry version is the first four bytes of the file, little-endian.
uint32_t RegistryVersionOnDisk()
{
    FILE* file = fsbridge::fopen(RegistryPath(), "rb");
    BOOST_REQUIRE(file != nullptr);

    std::array<unsigned char, 4> bytes{};
    const size_t read = fread(bytes.data(), 1, bytes.size(), file);
    fclose(file);
    BOOST_REQUIRE_EQUAL(read, bytes.size());

    return ReadLE32(bytes.data());
}

} // Anonymous namespace

BOOST_FIXTURE_TEST_SUITE(accrual_snapshot_tests, SnapshotDirSetup)

BOOST_AUTO_TEST_CASE(it_keeps_entries_appended_after_a_rewrite)
{
    const GRC::ResearchAccountMap accounts = OneAccount();

    {
        GRC::AccrualSnapshotRepository repo;
        StoreThree(repo, accounts);
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }

    {
        // Initialize() compacts the registry, then reopens it for appends.
        GRC::AccrualSnapshotRepository repo;
        BOOST_REQUIRE(repo.Initialize());
        BOOST_CHECK_EQUAL(repo.LatestHeight(), 300u);
        BOOST_REQUIRE(repo.Store(400, accounts));
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }

    GRC::AccrualSnapshotRepository repo;
    BOOST_REQUIRE(repo.Initialize());
    BOOST_CHECK(repo.HasBaseline());
    BOOST_CHECK_EQUAL(repo.LatestHeight(), 400u);
    BOOST_CHECK(repo.CloseRegistryFile());
}

BOOST_AUTO_TEST_CASE(it_replaces_a_leftover_temporary_registry_file)
{
    StoreThreeAndClose();

    // What an interrupted rewrite leaves behind: a partial temporary file next
    // to the intact registry.
    const fs::path temp_path = GRC::SnapshotDirectory() / "registry.dat.tmp";

    {
        FILE* file = fsbridge::fopen(temp_path, "wb");
        BOOST_REQUIRE(file != nullptr);
        const unsigned char partial[] = {0x03, 0x00};
        const size_t written = fwrite(partial, 1, sizeof(partial), file);
        fclose(file);
        BOOST_REQUIRE_EQUAL(written, sizeof(partial));
    }

    GRC::AccrualSnapshotRepository repo;
    BOOST_REQUIRE(repo.Initialize());
    BOOST_CHECK(repo.HasBaseline());
    BOOST_CHECK_EQUAL(repo.LatestHeight(), 300u);
    BOOST_CHECK(!fs::exists(temp_path));
    BOOST_CHECK(repo.CloseRegistryFile());
}

BOOST_AUTO_TEST_CASE(a_failed_compaction_is_reported_and_keeps_the_registry)
{
    StoreThreeAndClose();

    // A directory in place of the temporary registry file stops the
    // compaction from creating it, as a disk that cannot be written would.
    const fs::path temp_path = GRC::SnapshotDirectory() / "registry.dat.tmp";
    BOOST_REQUIRE(fs::create_directory(temp_path));

    {
        GRC::AccrualSnapshotRepository repo;
        BOOST_CHECK(!repo.Initialize());
        BOOST_CHECK(repo.CloseRegistryFile());
    }

    BOOST_CHECK_EQUAL(RegistryVersionOnDisk(), GRC::AccrualSnapshotRegistry::CURRENT_VERSION);
    BOOST_REQUIRE(fs::remove(temp_path));

    GRC::AccrualSnapshotRepository repo;
    BOOST_REQUIRE(repo.Initialize());
    BOOST_CHECK(repo.HasBaseline());
    BOOST_CHECK_EQUAL(repo.LatestHeight(), 300u);
    BOOST_CHECK(repo.CloseRegistryFile());
}

BOOST_AUTO_TEST_CASE(drop_above_height_removes_the_later_snapshots_for_good)
{
    {
        GRC::AccrualSnapshotRepository repo;
        StoreThree(repo, OneAccount());
        BOOST_CHECK_EQUAL(repo.DropAboveHeight(199), 2);
        BOOST_CHECK_EQUAL(repo.LatestHeight(), 100u);
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }

    BOOST_CHECK(fs::exists(GRC::SnapshotPath(100)));
    BOOST_CHECK(!fs::exists(GRC::SnapshotPath(200)));
    BOOST_CHECK(!fs::exists(GRC::SnapshotPath(300)));

    GRC::AccrualSnapshotRepository repo;
    BOOST_REQUIRE(repo.Initialize());
    BOOST_CHECK_EQUAL(repo.LatestHeight(), 100u);
    BOOST_CHECK(repo.CloseRegistryFile());
}

BOOST_AUTO_TEST_SUITE_END()
