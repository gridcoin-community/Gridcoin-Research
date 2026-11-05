// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/backup.h"
#include "init.h"
#include "node/shutdown.h"
#include "rpc/server.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "test/wallet_file_placeholder.h"
#include "wallet/db.h"
#include "wallet/wallet.h"
#include <fs.h>
#include <logging.h>
#include <univalue.h>
#include <util/system.h>
#include <util/time.h>

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <algorithm>
#include <functional>
#include <list>
#include <string>
#include <vector>

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
//! Collects the lines the logger prints while in scope.
struct CaptureLog
{
    std::vector<std::string> lines;
    std::list<std::function<void(const std::string&)>>::iterator callback;

    CaptureLog()
        : callback(LogInstance().PushBackCallback([this](const std::string& s) { lines.push_back(s); }))
    {
    }
    ~CaptureLog() { LogInstance().DeleteCallback(callback); }
    CaptureLog(const CaptureLog&) = delete;
    CaptureLog& operator=(const CaptureLog&) = delete;

    size_t Count(const std::string& needle) const
    {
        return static_cast<size_t>(std::count_if(lines.begin(), lines.end(), [&](const std::string& l) {
            return l.find(needle) != std::string::npos;
        }));
    }
};

// The line about backups that cannot be owner-only is logged once per process.
BOOST_AUTO_TEST_CASE(an_unrestricted_backup_is_logged_once)
{
    GRC::ResetUnrestrictedBackupLogForTesting();
    CaptureLog log;

    BOOST_CHECK(GRC::LogUnrestrictedBackupOnce("/media/stick/backups", "no POSIX modes"));
    BOOST_CHECK(!GRC::LogUnrestrictedBackupOnce("/media/stick/backups", "no POSIX modes"));
    BOOST_CHECK(!GRC::LogUnrestrictedBackupOnce("/media/other", "no POSIX modes"));

    BOOST_CHECK_EQUAL(log.Count("cannot be made owner-only"), 1U);
    BOOST_CHECK_EQUAL(log.Count("/media/stick/backups"), 1U);
    BOOST_CHECK_EQUAL(log.Count("no POSIX modes"), 1U);
    GRC::ResetUnrestrictedBackupLogForTesting();
}

// A new backup directory that cannot be made owner-only still receives the backup, and
// the backup is logged as not owner-only. Where the filesystem has POSIX modes it is
// owner-only and nothing is logged. The other branch runs where a path named
// "unrestrictable" behaves like FAT.
BOOST_AUTO_TEST_CASE(an_unrestrictable_new_backup_directory_still_receives_the_backup)
{
    const BackupDir dir;
    const UmaskGuard permissive(0);

    const fs::path conf = MakeConf(dir.m_path);
    grc_test::ForcedArgGuard conf_arg("conf", conf.string());

    GRC::ResetUnrestrictedBackupLogForTesting();
    CaptureLog log;

    const fs::path target = dir.m_path / "unrestrictable" / "backups" / "gridcoinresearch.conf-backup";
    BOOST_CHECK(GRC::BackupConfigFile(target.string()));
    BOOST_REQUIRE(fs::exists(target));
    BOOST_CHECK_EQUAL(fs::file_size(target), fs::file_size(conf));

    const bool owner_only = ModeOf(target.parent_path()) == 0700U && ModeOf(target) == 0600U;
    BOOST_CHECK_EQUAL(log.Count("cannot be made owner-only"), owner_only ? 0U : 1U);
    BOOST_TEST_MESSAGE("owner-only: " << owner_only);
    GRC::ResetUnrestrictedBackupLogForTesting();
}
#endif // !WIN32

namespace {

//! 2026-06-01T00:00:00Z.
constexpr int64_t JUNE_2026{1780272000};

//! 2026-01-01T00:00:00Z.
constexpr int64_t JANUARY_2026{1767225600};

//! Creates a regular file at \p path.
void Touch(const fs::path& path)
{
    fsbridge::ofstream stream(path);
    stream << "backup_tests\n";
}

//! Puts the mock time back at the end of a case.
struct MockTimeGuard
{
    const int64_t m_saved = GetMockTime().count();

    ~MockTimeGuard() { SetMockTime(m_saved); }
};

//! Clears a shutdown request at the end of a case, so the rest of the binary runs without one.
struct AbortShutdownOnExit
{
    ~AbortShutdownOnExit() { AbortShutdown(); }
};

} // anonymous namespace

// The automatic wallet backups made before a time are the regular files named as
// GetBackupFilename("wallet.dat") names them, with an earlier time in the name. A file
// with another name, a backup named after the time and a symbolic link with a backup's
// name are not listed, and nothing is removed.
BOOST_AUTO_TEST_CASE(list_names_only_automatic_backups_before_time)
{
    const BackupDir dir;

    const fs::path before = dir.m_path / "wallet.dat-2026-01-01T00-00-00";
    const fs::path after = dir.m_path / "wallet.dat-2026-12-01T00-00-00";
    const fs::path other = dir.m_path / "mywallet.bak";
    Touch(before);
    Touch(after);
    Touch(other);

#ifndef WIN32
    const fs::path link = dir.m_path / "wallet.dat-2026-01-02T00-00-00";
    fs::create_symlink(other, link);
#endif

    const std::vector<fs::path> found = GRC::ListAutomaticWalletBackupsBefore(dir.m_path, JUNE_2026);

    BOOST_REQUIRE_EQUAL(found.size(), 1U);
    BOOST_CHECK_EQUAL(found[0].string(), before.string());

    BOOST_CHECK(fs::exists(before));
    BOOST_CHECK(fs::exists(after));
    BOOST_CHECK(fs::exists(other));
#ifndef WIN32
    BOOST_CHECK(fs::is_symlink(link));
#endif
}

// The names the core writes have a Z after the time (FormatISO8601DateTimeDashSep), and
// some have a suffix after it; the names of older releases have neither. Each is listed
// when its time is before the one given, in name order. Names that only look like one,
// and a directory with a backup's name, are not.
BOOST_AUTO_TEST_CASE(list_takes_the_names_the_core_writes)
{
    const BackupDir dir;
    grc_test::ForcedArgGuard backupdir_arg("backupdir", dir.m_path.string());
    const MockTimeGuard mock_time;

    // 2026-01-01T01:01:01Z.
    SetMockTime(JANUARY_2026 + 3661);

    const fs::path written = GRC::GetBackupFilename("wallet.dat");
    const fs::path suffixed = GRC::GetBackupFilename("wallet.dat", "beacon");
    BOOST_REQUIRE_EQUAL(written.filename().string(), "wallet.dat-2026-01-01T01-01-01Z");
    BOOST_REQUIRE_EQUAL(suffixed.filename().string(), "wallet.dat-2026-01-01T01-01-01Z-beacon");
    Touch(written);
    Touch(suffixed);

    const fs::path older_release = dir.m_path / "wallet.dat-2020-05-05T05-05-05-beacon";
    Touch(older_release);

    for (const char* name : {"wallet.dat-2026-01-01T01-01-01x", "wallet.dat-2026-01-01T01-01-01Zbeacon",
                             "wallet.dat-2026-01-01T01-01", "wallet.dat-2026-1-01T01-01-01Z",
                             "wallet.dat-2026-13-01T01-01-01Z", "wallet.dat-2026-01-01 01-01-01Z",
                             "xwallet.dat-2026-01-01T01-01-01Z", "gridcoinresearch.conf-2026-01-01T01-01-01Z",
                             "wallet.dat"}) {
        Touch(dir.m_path / name);
    }
    fs::create_directories(dir.m_path / "wallet.dat-2026-01-02T00-00-00Z");

    const std::vector<fs::path> found = GRC::ListAutomaticWalletBackupsBefore(dir.m_path, JUNE_2026);

    BOOST_REQUIRE_EQUAL(found.size(), 3U);
    BOOST_CHECK_EQUAL(found[0].string(), older_release.string());
    BOOST_CHECK_EQUAL(found[1].string(), written.string());
    BOOST_CHECK_EQUAL(found[2].string(), suffixed.string());

    // A backup named in the second given is not before it.
    BOOST_CHECK_EQUAL(GRC::ListAutomaticWalletBackupsBefore(dir.m_path, JANUARY_2026 + 3661).size(), 1U);
}

// A backup directory that does not exist has no backups in it.
BOOST_AUTO_TEST_CASE(list_of_a_missing_directory_is_empty)
{
    const BackupDir dir;

    BOOST_CHECK(GRC::ListAutomaticWalletBackupsBefore(dir.m_path / "missing", JUNE_2026).empty());
}

#ifndef WIN32
// A name with a control character (a byte below 0x20, or 0x7f) is not listed, whatever else
// it looks like. Whoever can write the backup directory could otherwise put a terminal escape
// or a line break into the encryptwallet result and the GUI's list.
BOOST_AUTO_TEST_CASE(list_skips_names_with_control_characters)
{
    const BackupDir dir;

    const fs::path plain = dir.m_path / "wallet.dat-2026-01-01T00-00-00Z";
    Touch(plain);

    for (const std::string& name : {std::string("wallet.dat-2026-01-02T00-00-00Z-\x1b[2J"),
                                    std::string("wallet.dat-2026-01-03T00-00-00Z-a\nb"),
                                    std::string("wallet.dat-2026-01-04T00-00-00Z-\x7f"),
                                    std::string("wallet.dat-2026-01-05T00-00-00Z-\t")}) {
        Touch(dir.m_path / name);
        BOOST_REQUIRE(fs::is_regular_file(dir.m_path / name));
    }

    const std::vector<fs::path> found = GRC::ListAutomaticWalletBackupsBefore(dir.m_path, JUNE_2026);

    BOOST_REQUIRE_EQUAL(found.size(), 1U);
    BOOST_CHECK_EQUAL(found[0].string(), plain.string());
}

// A FIFO with a backup's name is not a backup, and is not listed.
BOOST_AUTO_TEST_CASE(list_skips_a_fifo_with_a_backup_name)
{
    const BackupDir dir;

    const fs::path plain = dir.m_path / "wallet.dat-2026-01-01T00-00-00Z";
    Touch(plain);

    const fs::path fifo = dir.m_path / "wallet.dat-2026-01-02T00-00-00Z";
    BOOST_REQUIRE_EQUAL(::mkfifo(fifo.string().c_str(), 0600), 0);

    const std::vector<fs::path> found = GRC::ListAutomaticWalletBackupsBefore(dir.m_path, JUNE_2026);

    BOOST_REQUIRE_EQUAL(found.size(), 1U);
    BOOST_CHECK_EQUAL(found[0].string(), plain.string());
}
#endif // !WIN32

// The warning names the backup directory and each backup found, says that a backup made
// by hand before encryption holds the unencrypted keys too, gives the order in which to
// get rid of them, and points to the docs. With none found it still names the directory
// and the backups made by hand.
BOOST_AUTO_TEST_CASE(warning_names_dir_and_manual_backups)
{
    const fs::path dir = fs::path("srv") / "gridcoin" / "walletbackups";
    const std::vector<fs::path> found{dir / "wallet.dat-2026-01-01T00-00-00Z", dir / "wallet.dat-2026-02-01T00-00-00Z"};

    const std::string warning = GRC::PreEncryptionBackupWarning(dir, found);
    BOOST_TEST_MESSAGE(warning);

    BOOST_CHECK(warning.find(dir.string()) != std::string::npos);
    BOOST_CHECK(warning.find("wallet.dat-2026-01-01T00-00-00Z") != std::string::npos);
    BOOST_CHECK(warning.find("wallet.dat-2026-02-01T00-00-00Z") != std::string::npos);
    BOOST_CHECK(warning.find("made by hand before encryption") != std::string::npos);
    BOOST_CHECK(warning.find("unencrypted keys") != std::string::npos);
    BOOST_CHECK(warning.find("doc/running-unattended.md#purging-pre-encryption-backups") != std::string::npos);

    const size_t check = warning.find("check that the new passphrase works");
    const size_t fresh = warning.find("make a fresh backup of the encrypted wallet");
    const size_t remove = warning.find("delete the old backups by hand");
    BOOST_REQUIRE(check != std::string::npos);
    BOOST_REQUIRE(fresh != std::string::npos);
    BOOST_REQUIRE(remove != std::string::npos);
    BOOST_CHECK(check < fresh);
    BOOST_CHECK(fresh < remove);

    const std::string none = GRC::PreEncryptionBackupWarning(dir, {});
    BOOST_TEST_MESSAGE(none);

    BOOST_CHECK(none.find(dir.string()) != std::string::npos);
    BOOST_CHECK(none.find("made by hand before encryption") != std::string::npos);
    BOOST_CHECK(none.find("delete the old backups by hand") != std::string::npos);
    BOOST_CHECK(none.find("doc/running-unattended.md#purging-pre-encryption-backups") != std::string::npos);
}

// encryptwallet deletes no backup. Its result lists the automatic backup made before the
// encryption and not the one named after it, and both files are still there.
BOOST_AUTO_TEST_CASE(encryptwallet_deletes_nothing)
{
    const BackupDir dir;
    grc_test::ForcedArgGuard backupdir_arg("backupdir", dir.m_path.string());
    const MockTimeGuard mock_time;
    const AbortShutdownOnExit abort_shutdown;

    SetMockTime(JUNE_2026);

    const fs::path before = dir.m_path / "wallet.dat-2026-01-01T00-00-00Z";
    const fs::path after = dir.m_path / "wallet.dat-2026-12-01T00-00-00Z";
    Touch(before);
    Touch(after);

    // In memory: EncryptWallet on a file-backed wallet rewrites the wallet file, which the
    // mock database of this binary refuses.
    CWallet wallet;
    grc_test::SwapMainWallet swap{&wallet};
    {
        CKey key;
        key.MakeNewKey(true);
        LOCK(wallet.cs_wallet);
        BOOST_REQUIRE(wallet.AddKey(key));
    }

    UniValue params(UniValue::VARR);
    params.push_back("backup_tests passphrase");

    const UniValue result = tableRPC.execute("encryptwallet", params);
    BOOST_REQUIRE(wallet.IsCrypted());

    const std::string text = result.get_str();
    BOOST_TEST_MESSAGE(text);

    BOOST_CHECK(text.find("wallet encrypted;") == 0);
    BOOST_CHECK(text.find(before.filename().string()) != std::string::npos);
    BOOST_CHECK(text.find(after.filename().string()) == std::string::npos);
    BOOST_CHECK(text.find("made by hand before encryption") != std::string::npos);

    BOOST_CHECK(fs::exists(before));
    BOOST_CHECK(fs::exists(after));
}

BOOST_AUTO_TEST_SUITE_END()
