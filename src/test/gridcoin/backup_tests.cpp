// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/backup.h"
#include "init.h"
#include "test/state_guard.h"
#include "test/wallet_file_placeholder.h"
#include "wallet/db.h"
#include "wallet/wallet.h"
#include <fs.h>
#include <util/system.h>

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <boost/test/unit_test.hpp>

namespace {

using grc_test::RequireWalletFileNotInUse;
using grc_test::WalletFilePlaceholder;

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

// A backup that cannot be made is reported by returning false, never by an
// exception: the scheduled backup runs on the scheduler thread, where an
// exception ends the process.

BOOST_AUTO_TEST_CASE(a_failed_wallet_copy_fails_the_backup)
{
    const BackupDir dir;

    // The fixture's wallet lives in a mock database, so there is no file to
    // copy.
    BOOST_REQUIRE(!fs::exists(GetDataDir() / pwalletMain->strWalletFile));
    RequireWalletFileNotInUse();

    BOOST_CHECK(!GRC::BackupWallet(*pwalletMain, (dir.m_path / "wallet.dat").string()));
}

BOOST_AUTO_TEST_CASE(a_failed_config_copy_fails_the_backup)
{
    const BackupDir dir;

    // The fixture writes no configuration file.
    BOOST_REQUIRE(!fs::exists(GetConfigFile()));

    BOOST_CHECK(!GRC::BackupConfigFile((dir.m_path / "gridcoinresearch.conf").string()));
}

BOOST_AUTO_TEST_CASE(a_backup_directory_that_cannot_be_created_fails_the_backup)
{
    const BackupDir dir;

    // A regular file stands where a directory above the backup has to go.
    const fs::path file = dir.m_path / "not_a_directory";
    {
        fsbridge::ofstream stream(file);
        stream << "not a directory";
    }

    const fs::path target = file / "walletbackups" / "backup";

    RequireWalletFileNotInUse();

    BOOST_CHECK(!GRC::BackupWallet(*pwalletMain, target.string()));
    BOOST_CHECK(!GRC::BackupConfigFile(target.string()));
}

BOOST_AUTO_TEST_CASE(a_config_file_under_a_non_ascii_path_is_backed_up)
{
    const BackupDir dir;

    // A directory whose name has a non-ASCII character (U+00E9, as UTF-8).
    const fs::path conf_dir = dir.m_path / std::string("conf_\xc3\xa9");
    fs::create_directories(conf_dir);

    const fs::path conf = conf_dir / "gridcoinresearch.conf";
    {
        fsbridge::ofstream stream(conf);
        stream << "# backup_tests\n";
    }

    grc_test::ForcedArgGuard conf_arg("conf", conf.string());
    BOOST_REQUIRE(GetConfigFile() == conf);

    const fs::path target = conf_dir / "gridcoinresearch.conf.backup";

    BOOST_CHECK(GRC::BackupConfigFile(target.string()));
    BOOST_CHECK(fs::exists(target));
}

BOOST_AUTO_TEST_CASE(a_wallet_backup_into_a_directory_is_named_after_the_wallet)
{
    const BackupDir dir;
    const WalletFilePlaceholder wallet_file;

    RequireWalletFileNotInUse();

    BOOST_CHECK(GRC::BackupWallet(*pwalletMain, dir.m_path.string()));
    BOOST_CHECK(fs::exists(dir.m_path / pwalletMain->strWalletFile));
}

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

#ifndef WIN32
namespace {

//! Sets the process umask for one case and puts the old one back at the end.
struct UmaskGuard
{
    const mode_t m_saved;

    explicit UmaskGuard(mode_t mask) : m_saved(::umask(mask)) {}
    ~UmaskGuard() { ::umask(m_saved); }
};

unsigned ModeOf(const fs::path& path)
{
    struct stat st;
    BOOST_REQUIRE_EQUAL(::stat(path.string().c_str(), &st), 0);

    // st_mode is mode_t; the permission bits fit an unsigned.
    return static_cast<unsigned>(st.st_mode & 07777);
}

fs::path MakeConf(const fs::path& dir)
{
    const fs::path conf = dir / "gridcoinresearch.conf";
    {
        fsbridge::ofstream stream(conf);
        stream << "# backup_tests\n";
    }
    return conf;
}

} // anonymous namespace

BOOST_AUTO_TEST_CASE(new_backup_copy_is_0600)
{
    const BackupDir dir;
    const UmaskGuard permissive(0);

    const fs::path conf = MakeConf(dir.m_path);
    grc_test::ForcedArgGuard conf_arg("conf", conf.string());

    const fs::path target = dir.m_path / "fresh-backup-name";

    BOOST_CHECK(GRC::BackupConfigFile(target.string()));
    BOOST_CHECK_EQUAL(ModeOf(target), 0600U);
}

BOOST_AUTO_TEST_CASE(a_new_backup_directory_is_0700)
{
    const BackupDir dir;
    const UmaskGuard permissive(0);

    const fs::path conf = MakeConf(dir.m_path);
    grc_test::ForcedArgGuard conf_arg("conf", conf.string());

    const fs::path target = dir.m_path / "newdir" / "backup";

    BOOST_CHECK(GRC::BackupConfigFile(target.string()));
    BOOST_CHECK_EQUAL(ModeOf(target.parent_path()), 0700U);
    BOOST_CHECK_EQUAL(ModeOf(target), 0600U);
}

BOOST_AUTO_TEST_CASE(an_existing_backup_target_keeps_its_mode)
{
    const BackupDir dir;
    const UmaskGuard permissive(0);

    const fs::path conf = MakeConf(dir.m_path);
    grc_test::ForcedArgGuard conf_arg("conf", conf.string());

    const fs::path target = dir.m_path / "existing-backup";
    {
        fsbridge::ofstream stream(target);
        stream << "old contents that are longer than the new ones, to show the truncation\n";
    }
    BOOST_REQUIRE_EQUAL(::chmod(target.string().c_str(), 0644), 0);

    BOOST_CHECK(GRC::BackupConfigFile(target.string()));
    BOOST_CHECK_EQUAL(ModeOf(target), 0644U);
    BOOST_CHECK_EQUAL(fs::file_size(target), fs::file_size(conf));
}

BOOST_AUTO_TEST_CASE(a_new_wallet_backup_is_0600)
{
    const BackupDir dir;
    const WalletFilePlaceholder wallet_file;
    const UmaskGuard permissive(0);

    RequireWalletFileNotInUse();

    const fs::path target = dir.m_path / "wallet-backups" / "wallet.dat-fresh";

    BOOST_CHECK(GRC::BackupWallet(*pwalletMain, target.string()));
    BOOST_CHECK_EQUAL(ModeOf(target), 0600U);
    BOOST_CHECK_EQUAL(ModeOf(target.parent_path()), 0700U);
}
#endif // !WIN32

BOOST_AUTO_TEST_SUITE_END()
