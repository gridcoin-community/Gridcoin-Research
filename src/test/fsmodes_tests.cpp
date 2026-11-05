// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/fsmodes.h"

#ifndef WIN32

#include "test/state_guard.h"
#include <fs.h>
#include <util/string.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <set>

namespace {

//! A scratch directory under the test data directory, removed when the case ends.
struct ScratchDir
{
    fs::path m_path;

    ScratchDir()
    {
        std::string templ = (GetDataDir() / "fsmodes_XXXXXX").string();
        BOOST_REQUIRE(::mkdtemp(templ.data()) != nullptr);
        m_path = templ;
    }

    ~ScratchDir()
    {
        boost::system::error_code ec;
        fs::remove_all(m_path, ec);
    }
};

void MakeFile(const fs::path& path, mode_t mode)
{
    {
        fsbridge::ofstream stream(path);
        stream << "fsmodes test\n";
    }
    BOOST_REQUIRE_EQUAL(::chmod(path.string().c_str(), mode), 0);
}

void MakeDir(const fs::path& path, mode_t mode)
{
    fs::create_directories(path);
    BOOST_REQUIRE_EQUAL(::chmod(path.string().c_str(), mode), 0);
}

unsigned ModeOf(const fs::path& path)
{
    struct stat st;
    BOOST_REQUIRE_EQUAL(::lstat(path.string().c_str(), &st), 0);

    // st_mode is mode_t; the permission bits fit an unsigned.
    return static_cast<unsigned>(st.st_mode & 07777);
}

bool HasFinding(const std::vector<fsmodes::ModeFinding>& findings, const fs::path& path)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&path](const fsmodes::ModeFinding& f) { return f.path == path; });
}

const fsmodes::ModeFinding& FindingFor(const std::vector<fsmodes::ModeFinding>& findings, const fs::path& path)
{
    const auto it = std::find_if(findings.begin(), findings.end(),
                                 [&path](const fsmodes::ModeFinding& f) { return f.path == path; });
    BOOST_REQUIRE(it != findings.end());
    return *it;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(fsmodes_tests)

BOOST_AUTO_TEST_CASE(warns_on_wide_existing_files_and_changes_nothing)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;

    MakeDir(datadir, 0755);
    MakeFile(datadir / "wallet.dat", 0644);
    MakeDir(datadir / "database", 0755);
    MakeFile(datadir / "database" / "log.0000000001", 0644);
    MakeDir(datadir / "walletbackups", 0755);
    MakeFile(datadir / "walletbackups" / "wallet.dat-x", 0644);
    MakeFile(datadir / "gridcoinsettings.json", 0664);
    MakeFile(datadir / "debug.log", 0644);

    // A group-readable file and an owner-only file pass silently.
    MakeFile(datadir / "gridcoinresearch.conf", 0640);
    MakeFile(datadir / "walletbackups" / "wallet.dat-ok", 0600);

    const std::set<fs::path> all = {
        datadir,
        datadir / "wallet.dat",
        datadir / "database",
        datadir / "database" / "log.0000000001",
        datadir / "walletbackups",
        datadir / "walletbackups" / "wallet.dat-x",
        datadir / "walletbackups" / "wallet.dat-ok",
        datadir / "gridcoinsettings.json",
        datadir / "debug.log",
        datadir / "gridcoinresearch.conf",
    };

    std::vector<std::pair<fs::path, unsigned>> before;
    for (const auto& path : all) before.emplace_back(path, ModeOf(path));

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, datadir / "gridcoinresearch.conf",
                                                       datadir / "gridcoinsettings.json", datadir / "wallet.dat",
                                                       datadir / "walletbackups", datadir / "debug.log");

    // Nothing changed.
    for (const auto& [path, mode] : before) {
        BOOST_CHECK_MESSAGE(ModeOf(path) == mode, path.string() << " changed mode");
    }

    const std::set<fs::path> dirs = {datadir, datadir / "database", datadir / "walletbackups"};
    const std::set<fs::path> wide = {
        datadir,
        datadir / "wallet.dat",
        datadir / "database",
        datadir / "database" / "log.0000000001",
        datadir / "walletbackups",
        datadir / "walletbackups" / "wallet.dat-x",
        datadir / "gridcoinsettings.json",
        datadir / "debug.log",
    };

    BOOST_CHECK_EQUAL(findings.size(), wide.size());

    for (const auto& path : all) {
        BOOST_CHECK_EQUAL(HasFinding(findings, path), wide.count(path) == 1);
    }

    for (const auto& path : wide) {
        const auto& finding = FindingFor(findings, path);
        BOOST_CHECK(finding.kind == fsmodes::ModeFinding::Kind::WideMode);

        const std::string fix = std::string("chmod ") + (dirs.count(path) ? "700 " : "600 ") + path.string();
        BOOST_CHECK_MESSAGE(finding.message.find(fix) != std::string::npos,
                            "'" << finding.message << "' does not name '" << fix << "'");
    }
}

BOOST_AUTO_TEST_CASE(an_owner_only_layout_has_no_findings)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;

    MakeDir(datadir, 0700);
    MakeFile(datadir / "wallet.dat", 0600);
    MakeDir(datadir / "database", 0700);
    MakeFile(datadir / "database" / "log.0000000001", 0600);

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, datadir / "gridcoinresearch.conf",
                                                       datadir / "gridcoinsettings.json", datadir / "wallet.dat",
                                                       datadir / "walletbackups", datadir / "debug.log");

    BOOST_CHECK(findings.empty());
}

BOOST_AUTO_TEST_CASE(root_owned_conf_is_check_only)
{
    if (::geteuid() == 0) return;

    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;
    MakeDir(datadir, 0700);

    const fs::path passwd = "/etc/passwd";
    BOOST_REQUIRE(!fsmodes::OwnedByUs(passwd));

    const unsigned before = ModeOf(passwd);
    BOOST_REQUIRE(!fsmodes::NotOwnedModeAcceptable(before));

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, passwd, {}, {}, {}, {});

    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == passwd);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::WideModeNotOwned);
    BOOST_CHECK_EQUAL(ModeOf(passwd), before);

    // The pure rule: a root:group 0640 file passes silently.
    BOOST_CHECK(fsmodes::NotOwnedModeAcceptable(0640));
    BOOST_CHECK(fsmodes::NotOwnedModeAcceptable(0600));
    BOOST_CHECK(!fsmodes::NotOwnedModeAcceptable(0644));
    BOOST_CHECK(!fsmodes::NotOwnedModeAcceptable(0660));
    BOOST_CHECK(!fsmodes::NotOwnedModeAcceptable(0604));
}

BOOST_AUTO_TEST_CASE(symlink_not_followed)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path;
    MakeDir(datadir, 0700);

    const fs::path target = datadir / "real_wallet";
    MakeFile(target, 0644);

    const fs::path wallet = datadir / "wallet.dat";
    fs::create_symlink(target, wallet);

    BOOST_CHECK(!fsmodes::OwnedByUs(wallet));

    const auto findings = fsmodes::CheckOwnerOnlyModes(datadir, {}, {}, wallet, {}, {});

    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == wallet);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::Symlink);
    BOOST_CHECK_EQUAL(ModeOf(target), 0644U);
}

BOOST_AUTO_TEST_CASE(symlinked_datadir_root_is_followed)
{
    const ScratchDir scratch;

    // A data directory moved to another disk through a link is a legitimate setup. The link
    // itself is not reported, and the normal rule applies to its target.
    const fs::path narrow = scratch.m_path / "narrow";
    MakeDir(narrow, 0700);
    const fs::path narrow_link = scratch.m_path / "narrow_link";
    fs::create_symlink(narrow, narrow_link);

    BOOST_CHECK(fsmodes::CheckOwnerOnlyModes(narrow_link, {}, {}, {}, {}, {}).empty());

    const fs::path wide = scratch.m_path / "wide";
    MakeDir(wide, 0755);
    const fs::path wide_link = scratch.m_path / "wide_link";
    fs::create_symlink(wide, wide_link);

    const auto findings = fsmodes::CheckOwnerOnlyModes(wide_link, {}, {}, {}, {}, {});

    BOOST_REQUIRE_EQUAL(findings.size(), 1U);
    BOOST_CHECK(findings[0].path == wide_link);
    BOOST_CHECK(findings[0].kind == fsmodes::ModeFinding::Kind::WideMode);
    BOOST_CHECK_EQUAL(findings[0].mode, 0755U);
    BOOST_CHECK(findings[0].message.find("chmod 700 " + wide_link.string()) != std::string::npos);

    // Inside a linked data directory a symlinked wallet file is still reported and not followed.
    const fs::path real_wallet = narrow / "real_wallet";
    MakeFile(real_wallet, 0644);
    const fs::path wallet = narrow / "wallet.dat";
    fs::create_symlink(real_wallet, wallet);

    const auto inner = fsmodes::CheckOwnerOnlyModes(narrow_link, {}, {}, wallet, {}, {});

    BOOST_REQUIRE_EQUAL(inner.size(), 1U);
    BOOST_CHECK(inner[0].path == wallet);
    BOOST_CHECK(inner[0].kind == fsmodes::ModeFinding::Kind::Symlink);
}

// The backup directory, the configuration file and debug.log may be links to another
// disk, as the data directory may: the link is not reported, and the rule applies to
// its target. The files inside a linked backup directory keep the rule that a link is
// reported, and so does the settings file.
BOOST_AUTO_TEST_CASE(symlinked_backupdir_conf_and_debug_log_are_followed)
{
    const ScratchDir scratch;
    const fs::path datadir = scratch.m_path / "data";
    MakeDir(datadir, 0700);

    const auto link = [](const fs::path& target, const fs::path& path) {
        fs::create_symlink(target, path);
        return path;
    };

    MakeDir(scratch.m_path / "backups_narrow", 0700);
    MakeFile(scratch.m_path / "conf_narrow", 0600);
    MakeFile(scratch.m_path / "log_narrow", 0600);
    const fs::path backups_narrow = link(scratch.m_path / "backups_narrow", scratch.m_path / "backups_narrow_link");
    const fs::path conf_narrow = link(scratch.m_path / "conf_narrow", scratch.m_path / "conf_narrow_link");
    const fs::path log_narrow = link(scratch.m_path / "log_narrow", scratch.m_path / "log_narrow_link");

    BOOST_CHECK(fsmodes::CheckOwnerOnlyModes(datadir, conf_narrow, {}, {}, backups_narrow, log_narrow).empty());

    MakeDir(scratch.m_path / "backups_wide", 0755);
    MakeFile(scratch.m_path / "conf_wide", 0644);
    MakeFile(scratch.m_path / "log_wide", 0604);
    const fs::path backups_wide = link(scratch.m_path / "backups_wide", scratch.m_path / "backups_wide_link");
    const fs::path conf_wide = link(scratch.m_path / "conf_wide", scratch.m_path / "conf_wide_link");
    const fs::path log_wide = link(scratch.m_path / "log_wide", scratch.m_path / "log_wide_link");

    const auto wide = fsmodes::CheckOwnerOnlyModes(datadir, conf_wide, {}, {}, backups_wide, log_wide);
    BOOST_CHECK_EQUAL(wide.size(), 3U);
    for (const auto& [path, mode] : std::vector<std::pair<fs::path, unsigned>>{
             {backups_wide, 0755U}, {conf_wide, 0644U}, {log_wide, 0604U}}) {
        const fsmodes::ModeFinding& finding = FindingFor(wide, path);
        BOOST_CHECK(finding.kind == fsmodes::ModeFinding::Kind::WideMode);
        BOOST_CHECK_EQUAL(finding.mode, mode);
    }

    // Inside the linked backup directory.
    MakeFile(scratch.m_path / "backups_narrow" / "wallet.dat-wide", 0644);
    MakeFile(scratch.m_path / "elsewhere", 0600);
    fs::create_symlink(scratch.m_path / "elsewhere", scratch.m_path / "backups_narrow" / "wallet.dat-link");
    const fs::path settings = link(scratch.m_path / "elsewhere", datadir / "gridcoinsettings.json");

    const auto inner = fsmodes::CheckOwnerOnlyModes(datadir, {}, settings, {}, backups_narrow, {});
    BOOST_CHECK_EQUAL(inner.size(), 3U);
    BOOST_CHECK(FindingFor(inner, backups_narrow / "wallet.dat-wide").kind == fsmodes::ModeFinding::Kind::WideMode);
    BOOST_CHECK(FindingFor(inner, backups_narrow / "wallet.dat-link").kind == fsmodes::ModeFinding::Kind::Symlink);
    BOOST_CHECK(FindingFor(inner, settings).kind == fsmodes::ModeFinding::Kind::Symlink);
}

BOOST_AUTO_TEST_CASE(backupdir_not_writable_reported)
{
    const ScratchDir scratch;

    // A directory that does not exist yet is created, and is writable.
    const fs::path fresh = scratch.m_path / "fresh" / "backups";
    BOOST_CHECK(!fsmodes::CheckBackupDirWritable(fresh));
    BOOST_CHECK(fs::is_directory(fresh));
    BOOST_CHECK_EQUAL(ModeOf(fresh), 0700U);
    BOOST_CHECK(fs::is_empty(fresh));

    if (::geteuid() == 0) return;

    const fs::path locked = scratch.m_path / "locked";
    MakeDir(locked, 0500);

    const auto reason = fsmodes::CheckBackupDirWritable(locked);
    BOOST_CHECK(reason.has_value() && !reason->empty());

    // Let the scratch directory be removed.
    BOOST_REQUIRE_EQUAL(::chmod(locked.string().c_str(), 0700), 0);
}

// A probe left behind by an earlier start that was killed between creating and
// removing it does not make the directory look unwritable. Under PrivatePIDs= every
// start has the same pid, so a probe named by pid and count would collide.
BOOST_AUTO_TEST_CASE(a_leftover_write_probe_does_not_fail_the_check)
{
    const ScratchDir scratch;
    const fs::path backups = scratch.m_path / "backups";
    MakeDir(backups, 0700);

    for (int count = 0; count < 200; ++count) {
        MakeFile(backups / (".gridcoin-write-test-" + ToString(::getpid()) + "-" + ToString(count)), 0600);
    }

    BOOST_CHECK(!fsmodes::CheckBackupDirWritable(backups));
    BOOST_CHECK(!fsmodes::CheckBackupDirWritable(backups));
}

// A probe name that is already taken is removed and the probe tried once more. A
// name that cannot be freed is reported.
BOOST_AUTO_TEST_CASE(a_taken_probe_name_is_freed_and_tried_once_more)
{
    const ScratchDir scratch;
    const fs::path backups = scratch.m_path / "backups";
    MakeDir(backups, 0700);

    const auto fixed = [] { return std::string{"probe"}; };

    MakeFile(backups / "probe", 0600);
    BOOST_CHECK(!fsmodes::CheckBackupDirWritable(backups, fixed));
    BOOST_CHECK(!fs::exists(backups / "probe"));

    // A directory of that name is not removed by unlink, so the second try fails too.
    MakeDir(backups / "probe", 0700);
    const auto reason = fsmodes::CheckBackupDirWritable(backups, fixed);
    BOOST_CHECK(reason.has_value() && !reason->empty());
    BOOST_CHECK(fs::is_directory(backups / "probe"));
}

BOOST_AUTO_TEST_SUITE_END()

#endif // !WIN32
