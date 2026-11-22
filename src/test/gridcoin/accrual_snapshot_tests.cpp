// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparamsbase.h"
#include "crypto/common.h"
#include "fs.h"
#include "gridcoin/account.h"
#include "gridcoin/accrual/snapshot.h"
#include "gridcoin/cpid.h"
#include "streams.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"
#include "uint256.h"
#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdio>
#include <utility>
#include <vector>

#ifndef WIN32
#include <sys/stat.h>
#endif

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

void SetRegistryVersionOnDisk(const uint32_t version)
{
    FILE* file = fsbridge::fopen(RegistryPath(), "r+b");
    BOOST_REQUIRE(file != nullptr);

    std::array<unsigned char, 4> bytes{};
    WriteLE32(bytes.data(), version);
    const size_t written = fwrite(bytes.data(), 1, bytes.size(), file);
    fclose(file);
    BOOST_REQUIRE_EQUAL(written, bytes.size());
}

//! The registry actions as they are written to disk.
constexpr uint8_t BASELINE = 0;
constexpr uint8_t REGISTER = 1;
constexpr uint8_t DEREGISTER = 2;

//! Write a registry file of the current version that records the given
//! actions and heights, in the disk format of a registry entry.
void WriteRegistryOnDisk(const std::vector<std::pair<uint8_t, uint64_t>>& entries)
{
    fs::create_directories(GRC::SnapshotDirectory());

    CAutoFile file(fsbridge::fopen(RegistryPath(), "wb"), SER_DISK, GRC::AccrualSnapshotRegistry::CURRENT_VERSION);
    BOOST_REQUIRE(!file.IsNull());

    file << GRC::AccrualSnapshotRegistry::CURRENT_VERSION;

    for (const auto& [action, height] : entries) {
        file << action << height << uint256();
    }
}
} // Anonymous namespace

BOOST_FIXTURE_TEST_SUITE(accrual_snapshot_tests, SnapshotDirSetup)

BOOST_AUTO_TEST_CASE(it_writes_the_current_registry_version)
{
    StoreThreeAndClose();

    BOOST_CHECK_EQUAL(RegistryVersionOnDisk(), GRC::AccrualSnapshotRegistry::CURRENT_VERSION);
}

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

BOOST_AUTO_TEST_CASE(a_previous_version_registry_loads_as_a_pending_upgrade)
{
    StoreThreeAndClose();
    SetRegistryVersionOnDisk(GRC::AccrualSnapshotRegistry::UPGRADABLE_VERSION);

    // Every start compacts the registry. Until the upgrade completes, the
    // previous version stays on disk, so an interrupted upgrade runs again.
    for (int start = 0; start < 2; ++start) {
        GRC::AccrualSnapshotRepository repo;
        BOOST_REQUIRE(repo.Initialize());
        BOOST_CHECK(repo.UpgradePending());
        BOOST_CHECK(repo.HasBaseline());
        BOOST_CHECK_EQUAL(repo.LatestHeight(), 300u);
        BOOST_REQUIRE(repo.CloseRegistryFile());
        BOOST_CHECK_EQUAL(RegistryVersionOnDisk(), GRC::AccrualSnapshotRegistry::UPGRADABLE_VERSION);
    }

    {
        // Completing the upgrade rewrites the registry; a snapshot stored
        // afterwards goes to the reopened registry file.
        GRC::AccrualSnapshotRepository repo;
        BOOST_REQUIRE(repo.Initialize());
        BOOST_REQUIRE(repo.CompleteUpgrade());
        BOOST_CHECK(!repo.UpgradePending());
        BOOST_REQUIRE(repo.Store(400, OneAccount()));
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }

    BOOST_CHECK_EQUAL(RegistryVersionOnDisk(), GRC::AccrualSnapshotRegistry::CURRENT_VERSION);

    GRC::AccrualSnapshotRepository repo;
    BOOST_REQUIRE(repo.Initialize());
    BOOST_CHECK(!repo.UpgradePending());
    BOOST_CHECK(repo.HasBaseline());
    BOOST_CHECK_EQUAL(repo.LatestHeight(), 400u);
    BOOST_CHECK(repo.CloseRegistryFile());
}

#ifndef WIN32
// The rewritten registry replaces the old one and has its mode: a registry the operator made
// group-readable stays so. Production change that would fail this: Rewrite() writing its
// temporary file with the default mode and renaming it over the registry without carrying the
// old one.
BOOST_AUTO_TEST_CASE(a_registry_rewrite_keeps_the_mode_of_the_registry)
{
    StoreThreeAndClose();
    const std::string path = RegistryPath().string();
    BOOST_REQUIRE_EQUAL(::chmod(path.c_str(), 0640), 0);

    {
        GRC::AccrualSnapshotRepository repo;
        BOOST_REQUIRE(repo.Initialize());
        BOOST_REQUIRE(repo.CompleteUpgrade());
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }

    struct stat st;
    BOOST_REQUIRE_EQUAL(::stat(path.c_str(), &st), 0);
    // mode_t is 16 bits on macOS, where the masked mode is an int; compare it unsigned.
    BOOST_CHECK_EQUAL(static_cast<unsigned>(st.st_mode & 07777), 0640U);

    // With no registry, the rewrite creates it owner-only.
    BOOST_REQUIRE(fs::remove(RegistryPath()));
    {
        GRC::AccrualSnapshotRepository repo;
        BOOST_REQUIRE(repo.Initialize());
        BOOST_REQUIRE(repo.CompleteUpgrade());
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }
    BOOST_REQUIRE_EQUAL(::stat(path.c_str(), &st), 0);
    // mode_t is 16 bits on macOS, where the masked mode is an int; compare it unsigned.
    BOOST_CHECK_EQUAL(static_cast<unsigned>(st.st_mode & 07777), 0600U);
}
#endif

BOOST_AUTO_TEST_CASE(erasing_the_snapshots_writes_the_current_version_at_once)
{
    StoreThreeAndClose();
    SetRegistryVersionOnDisk(GRC::AccrualSnapshotRegistry::UPGRADABLE_VERSION);

    GRC::AccrualSnapshotRepository repo;
    BOOST_REQUIRE(repo.Initialize());
    BOOST_REQUIRE(repo.UpgradePending());

    // The one-time rebuild erases the snapshots first. The new, empty registry
    // has the current version from the start: an interrupted rebuild is
    // finished by the startup repair from the last intact snapshot, not by the
    // version marker.
    BOOST_REQUIRE(repo.EraseAll());
    BOOST_CHECK(!repo.UpgradePending());
    BOOST_CHECK(!repo.HasBaseline());
    BOOST_REQUIRE(repo.CloseRegistryFile());

    BOOST_CHECK_EQUAL(RegistryVersionOnDisk(), GRC::AccrualSnapshotRegistry::CURRENT_VERSION);
    BOOST_CHECK(!fs::exists(GRC::SnapshotPath(100)));
}

BOOST_AUTO_TEST_CASE(other_registry_versions_still_force_a_rebuild)
{
    StoreThreeAndClose();

    for (const uint32_t version : {uint32_t{1}, GRC::AccrualSnapshotRegistry::CURRENT_VERSION + 1}) {
        SetRegistryVersionOnDisk(version);

        GRC::AccrualSnapshotRepository repo;
        BOOST_CHECK_THROW(repo.Initialize(), GRC::SnapshotRegistryVersionMismatchError);
        BOOST_CHECK(repo.CloseRegistryFile());
    }
}

BOOST_AUTO_TEST_CASE(it_refuses_a_registry_that_adds_a_height_out_of_order)
{
    // The heights in ascending order load:
    WriteRegistryOnDisk({{BASELINE, 100}, {REGISTER, 100}, {REGISTER, 200}, {REGISTER, 300}});

    {
        GRC::AccrualSnapshotRepository repo;
        BOOST_REQUIRE(repo.Initialize());
        BOOST_CHECK_EQUAL(repo.LatestHeight(), 300u);
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }

    // An equal height and a lower height make the tally rebuild the
    // snapshots:
    for (const uint64_t height : {uint64_t{300}, uint64_t{200}}) {
        WriteRegistryOnDisk({{BASELINE, 100}, {REGISTER, 100}, {REGISTER, 300}, {REGISTER, height}});

        GRC::AccrualSnapshotRepository repo;
        BOOST_CHECK_THROW(repo.Initialize(), GRC::SnapshotStateError);
        BOOST_CHECK(repo.CloseRegistryFile());
    }
}

BOOST_AUTO_TEST_CASE(it_refuses_a_registry_that_removes_a_snapshot_it_does_not_hold)
{
    // Removing the latest snapshot loads:
    WriteRegistryOnDisk({{BASELINE, 100}, {REGISTER, 100}, {REGISTER, 200}, {DEREGISTER, 200}});

    {
        GRC::AccrualSnapshotRepository repo;
        BOOST_REQUIRE(repo.Initialize());
        BOOST_CHECK_EQUAL(repo.LatestHeight(), 100u);
        BOOST_REQUIRE(repo.CloseRegistryFile());
    }

    // Removing an earlier snapshot, or any snapshot from an empty registry,
    // makes the tally rebuild the snapshots:
    WriteRegistryOnDisk({{BASELINE, 100}, {REGISTER, 100}, {REGISTER, 200}, {DEREGISTER, 100}});

    {
        GRC::AccrualSnapshotRepository repo;
        BOOST_CHECK_THROW(repo.Initialize(), GRC::SnapshotStateError);
        BOOST_CHECK(repo.CloseRegistryFile());
    }

    WriteRegistryOnDisk({{BASELINE, 100}, {DEREGISTER, 100}});

    GRC::AccrualSnapshotRepository repo;
    BOOST_CHECK_THROW(repo.Initialize(), GRC::SnapshotStateError);
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
