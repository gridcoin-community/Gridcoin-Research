// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/backup.h"
#include "init.h"
#include "wallet/db.h"
#include "wallet/wallet.h"
#include <fs.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

namespace {

//! BackupWallet copies the wallet only when the wallet is file-backed and
//! nothing has its file open, and returns false without trying otherwise.
//! Checked first so that a case cannot pass without reaching the copy.
void RequireWalletFileNotInUse()
{
    BOOST_REQUIRE(pwalletMain->fFileBacked);

    LOCK(bitdb.cs_db);

    const auto use_count = bitdb.mapFileUseCount.find(pwalletMain->strWalletFile);

    BOOST_REQUIRE(use_count == bitdb.mapFileUseCount.end() || use_count->second == 0);
}

//! A directory for one case's backups, removed when the case ends.
struct BackupDir
{
    const fs::path m_path = GetDataDir() / "backup_tests";

    BackupDir() { fs::create_directories(m_path); }

    ~BackupDir()
    {
        boost::system::error_code ec;
        fs::remove_all(m_path, ec);
    }
};

//! Makes a directory the process's working directory for one case, and puts
//! the old one back when the case ends.
struct WorkingDirectory
{
    const fs::path m_saved = fs::current_path();

    explicit WorkingDirectory(const fs::path& dir) { fs::current_path(dir); }

    ~WorkingDirectory()
    {
        boost::system::error_code ec;
        fs::current_path(m_saved, ec);
    }
};

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(backup_tests)

BOOST_AUTO_TEST_CASE(a_backup_leaves_a_wallet_file_in_the_working_directory_alone)
{
    const BackupDir dir;

    // A Berkeley DB file with the wallet's name, in the working directory and
    // stamped with an old time. The fixture's mock environment has no home
    // directory, so a file name passed to it resolves against the working
    // directory.
    const fs::path cwd = dir.m_path / "cwd";
    fs::create_directories(cwd);

    const fs::path other_wallet = cwd / pwalletMain->strWalletFile;
    {
        Db db(nullptr, DB_CXX_NO_EXCEPTIONS);
        BOOST_REQUIRE_EQUAL(db.open(nullptr, other_wallet.string().c_str(), "main", DB_BTREE, DB_CREATE, 0), 0);
        BOOST_REQUIRE_EQUAL(db.close(0), 0);
    }

    const std::time_t stamp = 946684800; // 2000-01-01T00:00:00Z
    fs::last_write_time(other_wallet, stamp);

    const WorkingDirectory in_cwd(cwd);

    // The copy itself fails, as the mock wallet has no file; what matters is
    // the work the backup does on the wallet before it copies.
    RequireWalletFileNotInUse();
    GRC::BackupWallet(*pwalletMain, (dir.m_path / "backup" / "wallet.dat").string());

    BOOST_CHECK_EQUAL(fs::last_write_time(other_wallet), stamp);
}

BOOST_AUTO_TEST_SUITE_END()
