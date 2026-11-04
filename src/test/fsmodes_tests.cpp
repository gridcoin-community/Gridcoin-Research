// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/fsmodes.h"

#ifndef WIN32

#include "test/state_guard.h"
#include <fs.h>
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

BOOST_AUTO_TEST_SUITE_END()

#endif // !WIN32
