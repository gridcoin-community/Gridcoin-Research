// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "fs.h"
#include "test/file_size_limit_guard.h"
#include "test/test_gridcoin.h"
#include "test/wine.h"
#include "uint256.h"
#include "util/system.h"
#include "util/transfer_file.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "test/win_fs_util.h"

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include "util/dir_permissions.h"
#include "util/win_acl.h"
#endif

namespace {

const std::string kBytes = "transfer file test payload 0123456789 abcdefghijklmnopqrstuvwxyz";

uint256 Sha256Of(const std::string& s)
{
    uint256 out;
    CSHA256().Write(reinterpret_cast<const unsigned char*>(s.data()), s.size()).Finalize(out.begin());
    return out;
}

// Opened through fsbridge, which takes the path itself. On Windows a std::fstream given
// p.string(), which is UTF-8, reads that name in the system code page, so a name with
// non-ASCII characters opens another file, or none.
std::string ReadAll(const fs::path& p)
{
    fsbridge::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteAll(const fs::path& p, const std::string& s)
{
    fsbridge::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << s;
}

size_t CountPartFiles(const fs::path& dir)
{
    size_t n = 0;

    for (fs::directory_iterator it(dir); it != fs::directory_iterator(); ++it) {
        if (it->path().extension() == ".part") {
            ++n;
        }
    }

    return n;
}

#ifndef WIN32
fs::path PartFileIn(const fs::path& dir)
{
    for (fs::directory_iterator it(dir); it != fs::directory_iterator(); ++it) {
        if (it->path().extension() == ".part") {
            return it->path();
        }
    }

    return fs::path();
}

unsigned ModeOf(const fs::path& p)
{
    struct stat st;

    BOOST_REQUIRE_EQUAL(::stat(p.c_str(), &st), 0);

    return static_cast<unsigned>(st.st_mode & 07777);
}

#endif // !WIN32

struct DirFixture
{
    fs::path dir;

    explicit DirFixture(const char* name = "transfer_file_tests") : dir(GetDataDir() / name)
    {
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    ~DirFixture()
    {
        boost::system::error_code ec;
        fs::remove_all(dir, ec);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(transfer_file_tests, DirFixture)

BOOST_AUTO_TEST_CASE(commit_renames_when_trailer_matches)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    std::string err;
    auto t = util::TransferTempFile::Create(dir / "out.bin", err);
    BOOST_REQUIRE(t);

    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    BOOST_CHECK_EQUAL(t->Size(), kBytes.size());
    BOOST_CHECK(t->Sha256() == Sha256Of(kBytes));
    BOOST_REQUIRE(t->Commit(kBytes.size(), Sha256Of(kBytes), err));

    BOOST_CHECK_EQUAL(ReadAll(dir / "out.bin"), kBytes);
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
#ifndef WIN32
    BOOST_CHECK_EQUAL(ModeOf(dir / "out.bin"), 0600u);
#else
    // The owner-only list the temporary file was created with, kept through the rename.
    if (!grc_test::RunningUnderWine()) {
        const std::optional<std::string> reason = util::winacl::CheckMatchesSpec(
            util::winacl::ReadAcl(dir / "out.bin"), util::OwnerOnlyAclSpec(/*inheritable=*/false));
        BOOST_CHECK_MESSAGE(!reason, reason.value_or(""));
    }
#endif
}

// A transfer that replaces a destination that exists leaves a file with the permissions that
// destination had; one to a name that is free is owner-only. Production change that would fail
// this: Create() not carrying the destination's permissions onto the temporary file before
// Commit() renames it over the destination.
BOOST_AUTO_TEST_CASE(commit_over_an_existing_destination_keeps_its_permissions)
{
    std::string err;
    const fs::path dest = dir / "existing.bin";
#ifndef WIN32
    WriteAll(dest, "old");
    BOOST_REQUIRE_EQUAL(::chmod(dest.c_str(), 0640), 0);
#else
    if (grc_test::RunningUnderWine()) return;
    util::winacl::AclSpec spec = util::OwnerOnlyAclSpec(/*inheritable=*/false);
    spec.aces.push_back({util::winacl::wellknown::Users(), FILE_GENERIC_READ, util::winacl::Inherit::None});
    grc_test::CreateFileWithAcl(dest, spec, "old");
#endif

    auto t = util::TransferTempFile::Create(dest, err);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    BOOST_REQUIRE_MESSAGE(t->Commit(kBytes.size(), Sha256Of(kBytes), err), err);

    BOOST_CHECK_EQUAL(ReadAll(dest), kBytes);
#ifndef WIN32
    BOOST_CHECK_EQUAL(ModeOf(dest), 0640u);
#else
    const util::winacl::ObservedAcl acl = util::winacl::ReadAcl(dest);
    BOOST_CHECK(std::any_of(acl.aces.begin(), acl.aces.end(), [](const util::winacl::ObservedAce& ace) {
        return ace.trustee == util::winacl::wellknown::Users();
    }));
#endif
}

// The permissions carried at the commit are those of the file that was at the destination when the
// transfer began. Another file put at the name meanwhile lends nothing and is not replaced.
// Production change that would fail this: a commit that reads the permissions of whatever is at
// the name by then.
BOOST_AUTO_TEST_CASE(commit_refuses_a_destination_that_is_another_file_by_then)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    std::string err;
    const fs::path dest = dir / "chosen.bin";
    const fs::path other = dir / "other.bin";
    grc_test::WriteOwnFile(dest, "old");
    grc_test::WriteOwnFile(other, "another file");
#ifndef WIN32
    BOOST_REQUIRE_EQUAL(::chmod(other.c_str(), 0644), 0);
#endif

    auto t = util::TransferTempFile::Create(dest, err);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    fs::rename(other, dest);

    BOOST_CHECK(!t->Commit(kBytes.size(), Sha256Of(kBytes), err));
    BOOST_CHECK_MESSAGE(err.find("is not the file that was there") != std::string::npos, err);
    BOOST_CHECK_EQUAL(ReadAll(dest), "another file");
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);

    // So is a destination that is gone by then.
    t = util::TransferTempFile::Create(dest, err);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    fs::remove(dest);
    BOOST_CHECK(!t->Commit(kBytes.size(), Sha256Of(kBytes), err));
    BOOST_CHECK(!fs::exists(dest));
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
}

// A caller that replaces nothing is refused a destination that is there when the transfer begins,
// and one that appears while the data arrives, as any transfer to a free name is. Production
// change that would fail this: a transfer that takes a file at the name as the caller's choice
// whatever the caller said.
BOOST_AUTO_TEST_CASE(a_transfer_that_never_replaces_refuses_a_taken_name)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    std::string err;
    const fs::path dest = dir / "taken.bin";
    grc_test::WriteOwnFile(dest, "there");

    BOOST_CHECK(!util::TransferTempFile::Create(dest, err, util::TransferTempFile::Replace::Never));
    BOOST_CHECK_MESSAGE(err.find("is already there") != std::string::npos, err);
    BOOST_CHECK_EQUAL(ReadAll(dest), "there");
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);

    fs::remove(dest);
    auto t = util::TransferTempFile::Create(dest, err, util::TransferTempFile::Replace::Never);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    grc_test::WriteOwnFile(dest, "appeared");
    BOOST_CHECK(!t->Commit(kBytes.size(), Sha256Of(kBytes), err));
    BOOST_CHECK_EQUAL(ReadAll(dest), "appeared");
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
}

#ifndef WIN32
// A destination that cannot be examined cannot be told from another file at the commit, so the
// transfer does not begin. Production change that would fail this: CreateAt() going on without the
// destination's identity. (On Windows a name too long to examine counts as not there.)
BOOST_AUTO_TEST_CASE(a_destination_that_cannot_be_examined_is_not_replaced)
{
    std::string err;
    const fs::path dest = dir / std::string(300, 'n');
    BOOST_CHECK(!util::TransferTempFile::CreateAt(dest, dir / "t.part", err));
    BOOST_CHECK_MESSAGE(err.find("cannot be examined") != std::string::npos, err);
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
}
#endif

// While it is written the temporary file is owner-only, also when the destination it will replace
// is wider: the destination's permissions are put on it only at the commit. Production change that
// would fail this: CreateAt() carrying the destination's permissions onto the temporary file.
BOOST_AUTO_TEST_CASE(the_part_file_stays_owner_only_until_the_commit)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    std::string err;
    const fs::path dest = dir / "existing.bin";
    const fs::path temp = dir / "existing.bin.0011223344556677.part";
#ifndef WIN32
    WriteAll(dest, "old");
    BOOST_REQUIRE_EQUAL(::chmod(dest.c_str(), 0640), 0);
#else
    util::winacl::AclSpec spec = util::OwnerOnlyAclSpec(/*inheritable=*/false);
    spec.aces.push_back({util::winacl::wellknown::Users(), FILE_GENERIC_READ, util::winacl::Inherit::None});
    grc_test::CreateFileWithAcl(dest, spec, "old");
#endif

    auto t = util::TransferTempFile::CreateAt(dest, temp, err);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
#ifndef WIN32
    BOOST_CHECK_EQUAL(ModeOf(temp), 0600u);
#else
    const std::optional<std::string> reason = util::winacl::CheckMatchesSpec(
        util::winacl::ReadAcl(temp), util::OwnerOnlyAclSpec(/*inheritable=*/false));
    BOOST_CHECK_MESSAGE(!reason, reason.value_or(""));
#endif

    BOOST_REQUIRE_MESSAGE(t->Commit(kBytes.size(), Sha256Of(kBytes), err), err);
    BOOST_CHECK_EQUAL(ReadAll(dest), kBytes);
#ifndef WIN32
    BOOST_CHECK_EQUAL(ModeOf(dest), 0640u);
#endif
}

#ifndef WIN32
// A destination that another account owns lends the file that replaces it nothing: what that
// account chose says nothing of what this one wants, so the new file is owner-only. Only root can
// make such a file here; the CI containers run as root. Production change that would fail this: a
// Commit() that carries the destination's mode without own_only.
BOOST_AUTO_TEST_CASE(another_accounts_destination_lends_no_permissions)
{
    if (::geteuid() != 0) {
        BOOST_TEST_MESSAGE("only root can give the destination another owner; skipping");
        return;
    }
    std::string err;
    const fs::path dest = dir / "theirs.bin";
    WriteAll(dest, "old");
    BOOST_REQUIRE_EQUAL(::chmod(dest.c_str(), 0644), 0);
    // Any uid but root's will do; 65534 is nobody on Linux.
    BOOST_REQUIRE_EQUAL(::chown(dest.c_str(), 65534, static_cast<gid_t>(-1)), 0);

    auto t = util::TransferTempFile::Create(dest, err);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    BOOST_REQUIRE_MESSAGE(t->Commit(kBytes.size(), Sha256Of(kBytes), err), err);

    BOOST_CHECK_EQUAL(ReadAll(dest), kBytes);
    BOOST_CHECK_EQUAL(ModeOf(dest), 0600u);
}
#endif

// A destination that did not exist when the transfer began is never replaced: a file that appears
// at its name while the data arrives is left as it is, and the transfer fails. Production change
// that would fail this: a Commit() that renames over whatever is at the name.
BOOST_AUTO_TEST_CASE(a_destination_that_appears_during_the_transfer_is_not_replaced)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    std::string err;
    const fs::path dest = dir / "new.bin";

    auto t = util::TransferTempFile::Create(dest, err);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    WriteAll(dest, "theirs");

    BOOST_CHECK(!t->Commit(kBytes.size(), Sha256Of(kBytes), err));
    BOOST_CHECK_MESSAGE(err.find("appeared") != std::string::npos, err);
    BOOST_CHECK_EQUAL(ReadAll(dest), "theirs");
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
}

BOOST_AUTO_TEST_CASE(commit_refuses_size_or_hash_mismatch)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    const unsigned char* p = reinterpret_cast<const unsigned char*>(kBytes.data());
    std::string err;

    // wrong size
    WriteAll(dir / "dest", "old");
    {
        auto t = util::TransferTempFile::Create(dir / "dest", err);
        BOOST_REQUIRE(t);
        BOOST_REQUIRE(t->Write(p, kBytes.size(), err));
        BOOST_CHECK(!t->Commit(kBytes.size() + 1, Sha256Of(kBytes), err));
        BOOST_CHECK(!err.empty());
    }
    BOOST_CHECK_EQUAL(ReadAll(dir / "dest"), "old");
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);

    // wrong hash
    {
        auto t = util::TransferTempFile::Create(dir / "dest", err);
        BOOST_REQUIRE(t);
        BOOST_REQUIRE(t->Write(p, kBytes.size(), err));
        BOOST_CHECK(!t->Commit(kBytes.size(), Sha256Of("something else"), err));
    }
    BOOST_CHECK_EQUAL(ReadAll(dir / "dest"), "old");
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
}

BOOST_AUTO_TEST_CASE(abort_and_destruction_leave_nothing)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    const unsigned char* p = reinterpret_cast<const unsigned char*>(kBytes.data());
    std::string err;

    {
        auto t = util::TransferTempFile::Create(dir / "gone", err);
        BOOST_REQUIRE(t);
        BOOST_REQUIRE(t->Write(p, kBytes.size(), err));
        BOOST_CHECK_EQUAL(CountPartFiles(dir), 1u);
        t->Abort();
        BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
        t->Abort();
    }

    {
        auto t = util::TransferTempFile::Create(dir / "gone", err);
        BOOST_REQUIRE(t);
        BOOST_REQUIRE(t->Write(p, kBytes.size(), err));
        BOOST_CHECK_EQUAL(CountPartFiles(dir), 1u);
    }

    BOOST_CHECK(!fs::exists(dir / "gone"));
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);

    // a moved-from object does not remove the temp file the new owner holds
    {
        auto t = util::TransferTempFile::Create(dir / "moved", err);
        BOOST_REQUIRE(t);
        {
            util::TransferTempFile u(std::move(*t));
            BOOST_CHECK_EQUAL(CountPartFiles(dir), 1u);
            BOOST_REQUIRE(u.Write(p, kBytes.size(), err));
            BOOST_REQUIRE(u.Commit(kBytes.size(), Sha256Of(kBytes), err));
        }
        t.reset();
    }

    BOOST_CHECK_EQUAL(ReadAll(dir / "moved"), kBytes);
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
}

BOOST_AUTO_TEST_CASE(create_refuses_preexisting_temp_name)
{
    std::string err;

    WriteAll(dir / "target", "keep");

#ifndef WIN32
    // a symlink at the temp name, pointing at a file that must stay unchanged
    BOOST_REQUIRE_EQUAL(::symlink((dir / "target").c_str(), (dir / "t.part").c_str()), 0);
    BOOST_CHECK(!util::TransferTempFile::CreateAt(dir / "out", dir / "t.part", err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(ReadAll(dir / "target"), "keep");
    fs::remove(dir / "t.part");
#endif

    // a regular file at the temp name
    WriteAll(dir / "t.part", "preexisting");
    err.clear();
    BOOST_CHECK(!util::TransferTempFile::CreateAt(dir / "out", dir / "t.part", err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(ReadAll(dir / "t.part"), "preexisting");
    BOOST_CHECK_EQUAL(ReadAll(dir / "target"), "keep");
    BOOST_CHECK(!fs::exists(dir / "out"));
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(temp_is_0600_under_umask_000)
{
    std::string err;
    const mode_t old = ::umask(0);
    auto t = util::TransferTempFile::Create(dir / "x", err);
    ::umask(old);

    BOOST_REQUIRE(t);
    BOOST_CHECK_EQUAL(ModeOf(PartFileIn(dir)), 0600u);
}

// A failed write leaves the destination untouched and no temporary file behind
BOOST_AUTO_TEST_CASE(write_failure_leaves_destination_untouched)
{
    std::string err;

    WriteAll(dir / "dest", "old");

    auto t = util::TransferTempFile::Create(dir / "dest", err);
    BOOST_REQUIRE(t);

    const std::vector<unsigned char> big(64 * 1024, 0x5a);
    bool ok = true;

    {
        grc_test::FileSizeLimitGuard guard(4096);
        ok = t->Write(big.data(), big.size(), err);
        guard.Restore();
    }

    BOOST_CHECK(!ok);
    BOOST_CHECK(!err.empty());

    t->Abort();

    BOOST_CHECK_EQUAL(ReadAll(dir / "dest"), "old");
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);
}
#endif // !WIN32

// A destination with non-ASCII characters and a space round-trips
BOOST_AUTO_TEST_CASE(non_ascii_destination_round_trips)
{
    if (grc_test::RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected access list, which the transfer directory and files need; skipping");
        return;
    }
    // UTF-8 for "Sicherung äöü 测试.dat", with a space
    const std::string name = "Sicherung \xc3\xa4\xc3\xb6\xc3\xbc \xe6\xb5\x8b\xe8\xaf\x95.dat";
    const fs::path dest = dir / fs::path(name);
    std::string err;

    auto t = util::TransferTempFile::Create(dest, err);
    BOOST_REQUIRE_MESSAGE(t, err);
    BOOST_REQUIRE(t->Write(reinterpret_cast<const unsigned char*>(kBytes.data()), kBytes.size(), err));
    BOOST_REQUIRE_MESSAGE(t->Commit(kBytes.size(), Sha256Of(kBytes), err), err);

    BOOST_CHECK(fs::exists(dest));
    BOOST_CHECK_EQUAL(ReadAll(dest), kBytes);
    BOOST_CHECK_EQUAL(CountPartFiles(dir), 0u);

#ifdef WIN32
    // The name on disk is the one intended: the UTF-8 above read as UTF-8, not in the system
    // code page.
    const std::wstring intended = L"Sicherung \u00e4\u00f6\u00fc \u6d4b\u8bd5.dat";
    BOOST_CHECK(dest.filename().wstring() == intended);
    bool found = false;
    for (fs::directory_iterator it(dir); it != fs::directory_iterator(); ++it) {
        found = found || it->path().filename().wstring() == intended;
    }
    BOOST_CHECK_MESSAGE(found, "no entry of " << dir.string() << " has the intended name");
#endif
}

BOOST_AUTO_TEST_CASE(free_space_check)
{
    std::string err;

    BOOST_CHECK(util::HasFreeSpace(dir, 0, err));

    err.clear();
    BOOST_CHECK(!util::HasFreeSpace(dir, std::numeric_limits<uint64_t>::max() / 2, err));
    BOOST_CHECK(err.find(dir.string()) != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
