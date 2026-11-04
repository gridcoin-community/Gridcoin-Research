// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "crypto/sha256.h"
#include "fs.h"
#include "interfaces/file_transfer.h"
#include "test/test_gridcoin.h"
#include "uint256.h"
#include "util/system.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<unsigned char> Bytes(size_t n)
{
    std::vector<unsigned char> v(n);

    for (size_t i = 0; i < n; ++i) {
        v[i] = static_cast<unsigned char>((i * 131 + (i >> 8)) & 0xff);
    }

    return v;
}

uint256 Sha256Of(const std::vector<unsigned char>& v)
{
    uint256 out;
    CSHA256().Write(v.data(), v.size()).Finalize(out.begin());
    return out;
}

std::string ReadAll(const fs::path& p)
{
    std::ifstream in(p.string(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

size_t CountEntries(const fs::path& dir)
{
    return static_cast<size_t>(std::distance(fs::directory_iterator(dir), fs::directory_iterator()));
}

struct FakeSource : public interfaces::FileSource
{
    explicit FakeSource(std::vector<unsigned char> d) : data(std::move(d)) {}

    std::vector<unsigned char> data;
    int64_t reported_size{-1};      // -2 means the data size
    uint64_t size_hint{0};
    int throw_on_read{0};           // 1-based read number that throws
    int error_on_read{0};           // 1-based read number that returns an error chunk
    bool wrong_sha{false};
    size_t pos{0};
    int reads{0};
    uint32_t max_requested{0};
    bool cancelled{false};

    std::vector<interfaces::FileEntryInfo> entries() override
    {
        interfaces::FileEntryInfo e;
        e.name = "fake";
        e.size = reported_size == -2 ? static_cast<int64_t>(data.size()) : reported_size;
        e.size_hint = size_hint;
        return {e};
    }

    interfaces::FileChunk read(uint32_t, uint32_t max_bytes) override
    {
        ++reads;
        max_requested = std::max(max_requested, max_bytes);

        if (reads == throw_on_read) {
            throw std::runtime_error("connection lost");
        }

        interfaces::FileChunk c;

        if (reads == error_on_read) {
            c.error = "source failed";
            return c;
        }

        const size_t n = std::min<size_t>(max_bytes, data.size() - pos);
        c.data.assign(data.begin() + pos, data.begin() + pos + n);
        pos += n;
        c.eof = pos == data.size();
        return c;
    }

    interfaces::FileTrailer trailer(uint32_t) override
    {
        interfaces::FileTrailer t;
        t.ok = true;
        t.size = data.size();
        t.sha256 = wrong_sha ? Sha256Of(Bytes(7)) : Sha256Of(data);
        return t;
    }

    void cancel() override { cancelled = true; }
};

struct FakeSink : public interfaces::FileSink
{
    std::vector<unsigned char> received;
    size_t max_chunk{0};
    bool cancelled{false};
    bool finished{false};
    uint64_t finish_size{0};
    uint256 finish_sha;
    int fail_on_write{0};     // 1-based write number that returns false
    int throw_on_write{0};    // 1-based write number that throws
    int writes{0};

    bool write(const std::vector<unsigned char>& d) override
    {
        ++writes;

        if (writes == throw_on_write) {
            throw std::runtime_error("connection lost");
        }

        if (writes == fail_on_write) {
            return false;
        }

        max_chunk = std::max(max_chunk, d.size());
        received.insert(received.end(), d.begin(), d.end());
        return true;
    }

    interfaces::FileSinkResult finish(uint64_t size, const uint256& sha) override
    {
        finished = true;
        finish_size = size;
        finish_sha = sha;
        interfaces::FileSinkResult r;
        r.ok = true;
        return r;
    }

    void cancel() override { cancelled = true; }
};

struct DirFixture
{
    fs::path dir;

    DirFixture() : dir(GetDataDir() / "file_transfer_tests")
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

BOOST_FIXTURE_TEST_SUITE(file_transfer_tests, DirFixture)

BOOST_AUTO_TEST_CASE(download_chunks_and_verifies)
{
    FakeSource src(Bytes(1'000'003));
    src.reported_size = -2;
    std::atomic<bool> cancel{false};
    std::vector<uint64_t> progress;

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin",
                                         [&](uint64_t d, int64_t) { progress.push_back(d); }, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::Ok);
    BOOST_CHECK_EQUAL(out.bytes, 1'000'003u);
    BOOST_CHECK(out.sha256 == Sha256Of(src.data));
    BOOST_CHECK(src.max_requested <= 4096u);
    BOOST_CHECK(std::is_sorted(progress.begin(), progress.end()));
    BOOST_CHECK(!progress.empty() && progress.back() == 1'000'003u);
    BOOST_CHECK(ReadAll(dir / "d.bin") == std::string(src.data.begin(), src.data.end()));
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);
}

BOOST_AUTO_TEST_CASE(download_trailer_mismatch_is_verify_failed)
{
    FakeSource src(Bytes(20'000));
    src.wrong_sha = true;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::VerifyFailed);
    BOOST_CHECK(!fs::exists(dir / "d.bin"));
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

// Review Focus 2
BOOST_AUTO_TEST_CASE(download_source_error_mid_stream_removes_part)
{
    {
        std::ofstream(( dir / "d.bin").string(), std::ios::binary) << "old";
    }

    FakeSource src(Bytes(100'000));
    src.throw_on_read = 3;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(out.message.find("connection lost") != std::string::npos);
    BOOST_CHECK_EQUAL(ReadAll(dir / "d.bin"), "old");
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);

    // an error chunk ends the transfer the same way
    FakeSource src2(Bytes(100'000));
    src2.error_on_read = 2;

    out = interfaces::DownloadEntry(src2, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::SourceError);
    BOOST_CHECK(out.message.find("source failed") != std::string::npos);
    BOOST_CHECK_EQUAL(ReadAll(dir / "d.bin"), "old");
    BOOST_CHECK_EQUAL(CountEntries(dir), 1u);
}

BOOST_AUTO_TEST_CASE(download_cancel_calls_source_cancel)
{
    FakeSource src(Bytes(100'000));
    std::atomic<bool> cancel{false};
    int chunks = 0;

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin",
                                         [&](uint64_t, int64_t) { if (++chunks == 2) cancel = true; }, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::Cancelled);
    BOOST_CHECK(src.cancelled);
    BOOST_CHECK(!fs::exists(dir / "d.bin"));
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_no_space_refused_before_create)
{
    FakeSource src(Bytes(10));
    src.reported_size = std::numeric_limits<int64_t>::max() / 2;
    std::atomic<bool> cancel{false};

    auto out = interfaces::DownloadEntry(src, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::NoSpace);
    BOOST_CHECK_EQUAL(src.reads, 0);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);

    // a generated entry is checked against its size hint
    FakeSource src2(Bytes(10));
    src2.reported_size = -1;
    src2.size_hint = std::numeric_limits<uint64_t>::max() / 2;

    out = interfaces::DownloadEntry(src2, 0, dir / "d.bin", nullptr, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::NoSpace);
    BOOST_CHECK_EQUAL(CountEntries(dir), 0u);
}

BOOST_AUTO_TEST_CASE(download_generated_entry_has_unknown_total)
{
    FakeSource src(Bytes(10'000));
    std::atomic<bool> cancel{false};
    int64_t last_total = 0;

    auto out = interfaces::DownloadEntry(src, 0, dir / "g.bin",
                                         [&](uint64_t, int64_t total) { last_total = total; }, cancel, 4096);

    BOOST_CHECK(out.status == interfaces::TransferStatus::Ok);
    BOOST_CHECK_EQUAL(last_total, -1);
}

BOOST_AUTO_TEST_CASE(upload_sends_size_and_hash)
{
    const std::vector<unsigned char> content = Bytes(100'003);
    {
        std::ofstream out((dir / "up.bin").string(), std::ios::binary);
        out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    FakeSink sink;
    std::atomic<bool> cancel{false};
    uint64_t last = 0;

    auto r = interfaces::UploadFile(sink, dir / "up.bin", [&](uint64_t d, int64_t) { last = d; }, cancel, 4096);

    BOOST_CHECK(r.ok);
    BOOST_CHECK(sink.finished);
    BOOST_CHECK_EQUAL(sink.finish_size, content.size());
    BOOST_CHECK(sink.finish_sha == Sha256Of(content));
    BOOST_CHECK(sink.max_chunk <= 4096u);
    BOOST_CHECK(sink.received == content);
    BOOST_CHECK_EQUAL(last, content.size());
}

BOOST_AUTO_TEST_CASE(upload_cancel_and_failures)
{
    {
        std::ofstream out((dir / "up.bin").string(), std::ios::binary);
        const std::vector<unsigned char> content = Bytes(100'000);
        out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    std::atomic<bool> cancel{false};

    FakeSink cancelled_sink;
    int chunks = 0;
    auto r = interfaces::UploadFile(cancelled_sink, dir / "up.bin",
                                    [&](uint64_t, int64_t) { if (++chunks == 2) cancel = true; }, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(cancelled_sink.cancelled);
    BOOST_CHECK(!cancelled_sink.finished);

    cancel = false;
    FakeSink refusing;
    refusing.fail_on_write = 3;
    r = interfaces::UploadFile(refusing, dir / "up.bin", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
    BOOST_CHECK(!refusing.finished);

    FakeSink throwing;
    throwing.throw_on_write = 2;
    r = interfaces::UploadFile(throwing, dir / "up.bin", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(r.error.find("connection lost") != std::string::npos);
    BOOST_CHECK(!throwing.finished);

    FakeSink missing;
    r = interfaces::UploadFile(missing, dir / "does-not-exist", nullptr, cancel, 4096);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!missing.finished);
}

BOOST_AUTO_TEST_CASE(console_file_commands_table)
{
    const auto& t = interfaces::ConsoleFileCommands();

    BOOST_REQUIRE_EQUAL(t.size(), 4u);
    BOOST_CHECK_EQUAL(std::string(t[0].method), "dumpwallet");
    BOOST_CHECK_EQUAL(std::string(t[1].method), "importwallet");
    BOOST_CHECK_EQUAL(std::string(t[2].method), "dumpcontracts");
    BOOST_CHECK_EQUAL(std::string(t[3].method), "parseaccrualsnapshotfile");

    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("dumpwallet")->path_index, 0u);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("dumpwallet")->direction == interfaces::TransferDirection::Download);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("dumpcontracts")->path_index, 1u);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("dumpcontracts")->direction == interfaces::TransferDirection::Download);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("importwallet")->direction == interfaces::TransferDirection::Upload);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("importwallet")->max_upload_bytes, 64u * 1024 * 1024);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("parseaccrualsnapshotfile")->path_index, 0u);
    BOOST_CHECK(interfaces::FindConsoleFileCommand("parseaccrualsnapshotfile")->direction == interfaces::TransferDirection::Upload);
    BOOST_CHECK_EQUAL(interfaces::FindConsoleFileCommand("parseaccrualsnapshotfile")->max_upload_bytes, 256u * 1024 * 1024);
    BOOST_CHECK(!interfaces::FindConsoleFileCommand("backupwallet"));
}

BOOST_AUTO_TEST_SUITE_END()
