// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "fs.h"
#include "test/file_size_limit_guard.h"
#include "test/test_gridcoin.h"
#include "uint256.h"
#include "util/system.h"
#include "util/transfer_file.h"

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

const std::string kBytes = "transfer file test payload 0123456789 abcdefghijklmnopqrstuvwxyz";

uint256 Sha256Of(const std::string& s)
{
    uint256 out;
    CSHA256().Write(reinterpret_cast<const unsigned char*>(s.data()), s.size()).Finalize(out.begin());
    return out;
}

std::string ReadAll(const fs::path& p)
{
    std::ifstream in(p.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteAll(const fs::path& p, const std::string& s)
{
    std::ofstream out(p.string(), std::ios::binary | std::ios::trunc);
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
#endif
}

BOOST_AUTO_TEST_CASE(commit_refuses_size_or_hash_mismatch)
{
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
